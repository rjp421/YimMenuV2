#include "core/commands/BoolCommand.hpp"
#include "core/commands/LoopedCommand.hpp"
#include "core/commands/Commands.hpp"
#include "core/util/Joaat.hpp"
#include "game/backend/Detections.hpp"
#include "game/backend/NetRecorder.hpp"
#include "game/backend/Players.hpp"
#include "game/backend/Self.hpp"
#include "game/gta/Natives.hpp"
#include "game/hooks/Hooks.hpp"
#include "game/pointers/Pointers.hpp"
#include "types/network/CNetGamePlayer.hpp"
#include "types/network/netGameEvent.hpp"
#include "types/entity/fwEntity.hpp"
#include "types/network/netObject.hpp"
#include "types/rage/datBitBuffer.hpp"
#include "types/script/globals/GlobalPlayerBD.hpp"
#include <set>

namespace YimMenu::Hooks
{
	// legacy's "Block Traffic Manipulation" (same default)
	static BoolCommand _BlockTrafficManipulation{"blocktrafficmanipulation", "Block Traffic Manipulation", "Blocks others from changing the traffic and pedestrians around you", true};

	static void SendEventAck(rage::netEventMgr* event_mgr, CNetGamePlayer* source_player, CNetGamePlayer* target_player, uint32_t event_index, uint32_t event_handled_bits)
	{
		if (source_player->m_PlayerIndex != 255 && !Pointers.EventAck(0x778LL * source_player->m_PlayerIndex + (intptr_t)event_mgr + 0x2C408, target_player, event_index, event_handled_bits))
		{
			Pointers.SendEventAck(event_mgr, source_player);
			Pointers.EventAck((intptr_t)event_mgr + 0x2C408 + 0x778LL * source_player->m_PlayerIndex, target_player, event_index, event_handled_bits);
		}

		*reinterpret_cast<std::uint32_t*>(0x118LL * event_index + (uint64_t)event_mgr + 4 * source_player->m_PlayerIndex + 0x458) = event_handled_bits;
	}

	// Returns true if bad event
	static bool ScanExplosionEvent(CExplosionEvent& event, Player player)
	{
		if (*(int16_t*)&event.m_InteriorIndex < -1)
		{
			LOG(WARNING) << "blocked invalid interior crash from " << player.GetName();
			return true;
		}

		if (event.m_IsInvisible)
		{
			float dist = event.m_Position.GetDistance(Self::GetPed().GetPosition());
			if (event.m_CameraShake > 0.0f && dist <= 30.0f)
			{
				// Camera shake
				LOGF(WARNING, "Blocked EXPLOSION_EVENT from {} since m_CameraShake > 0.0f", player.GetName());
				return true;
			}

			if (event.m_DamageScale > 0.0f && dist <= 10.0f)
			{
				// Ragdoll
				LOGF(WARNING, "Blocked EXPLOSION_EVENT from {} since m_DamageScale > 0.0f", player.GetName());
				return true;
			}
		}

		// more?

		return false;
	}

	// records what an explosion was for the netlogs; only an owned explosion blamed on another player is logged
	static void RecordExplosion(CExplosionEvent& event, Player player)
	{
		// an explosion attached to an entity (a car blowing up, a sticky bomb on a ped) carries its position relative to that
		// entity, so record where it is on the map
		auto position = event.m_Position;
		bool attached = false;
		if (event.m_TargetEnt2)
		{
			if (auto object = Pointers.GetNetObjectById(event.m_TargetEnt2); object && object->m_GameObject)
			{
				auto entity = reinterpret_cast<rage::fwEntity*>(object->m_GameObject)->GetPosition();
				position = {position.x + entity->x, position.y + entity->y, position.z + entity->z};
				attached = true;
			}
		}

		std::uint8_t nearest_id = 255;
		float nearest = 6553.5f;
		for (auto& [id, other] : Players::GetPlayers())
		{
			if (auto ped = other.GetPed())
			{
				auto dist = position.GetDistance(ped.GetPosition());
				if (dist < nearest)
				{
					nearest = dist;
					nearest_id = id;
				}
			}
		}
		float to_us = 429496729.5f;
		if (auto self = Self::GetPed())
			to_us = std::min(position.GetDistance(self.GetPosition()), to_us);

		std::int64_t owner_type = 0xFF, owner_player = 0xFF;
		if (auto object = Pointers.GetNetObjectById(event.m_OwnerNetId))
		{
			owner_type = static_cast<std::int64_t>(object->m_ObjectType);
			owner_player = static_cast<std::uint8_t>(object->m_OwnerId);
		}

		std::uint32_t flags = (event.m_AddOwnedExplosion ? NetRecorder::ExplosionOwned : 0) | (event.m_IsInvisible ? NetRecorder::ExplosionInvisible : 0)
		    | (event.m_IsAudible ? NetRecorder::ExplosionAudible : 0) | (event.m_HasTargetEnt2 ? NetRecorder::ExplosionHasTarget2 : 0)
		    | (attached ? NetRecorder::ExplosionAttached : 0);
		std::uint32_t b = flags | nearest_id << 8 | static_cast<std::uint32_t>(std::min(nearest * 10.0f, 65535.0f)) << 16;

		auto to_fixed = [](float value, float scale) {
			return static_cast<std::int64_t>(std::llround(value * scale));
		};
		std::array<std::int64_t, 8> values = {
		    event.m_OwnerNetId,
		    owner_type | owner_player << 8,
		    event.m_TargetEntity | static_cast<std::int64_t>(event.m_TargetEnt2) << 16,
		    to_fixed(position.x, 100.0f),
		    to_fixed(position.y, 100.0f),
		    to_fixed(position.z, 100.0f),
		    (to_fixed(std::clamp(event.m_DamageScale, 0.0f, 65.0f), 1000.0f) & 0xFFFF) | (to_fixed(std::clamp(event.m_CameraShake, -32.0f, 32.0f), 1000.0f) & 0xFFFF) << 16
		        | to_fixed(to_us, 10.0f) << 32,
		    event.m_AddOwnedExplosion ? static_cast<std::int64_t>(event.m_ScriptHash) << 32 | event.m_ProgramCounter : 0,
		};
		NetRecorder::RecordExplosion(player, static_cast<std::uint32_t>(event.m_ExplosionType), b, values);

		// an owned explosion names the ped it is blamed on (legacy's "blame explosion"); log only, at most every 10 seconds per sender
		if (event.m_AddOwnedExplosion && owner_type == static_cast<std::int64_t>(rage::netObject::Type::PLAYER) && owner_player != player.GetId())
		{
			static std::array<std::chrono::steady_clock::time_point, 256> last_logs{};
			auto& last = last_logs[static_cast<std::uint8_t>(player.GetId())];
			auto now = std::chrono::steady_clock::now();
			if (now - last > std::chrono::seconds(10))
			{
				last = now;
				auto& players = Players::GetPlayers();
				auto blamed = players.find(static_cast<std::uint8_t>(owner_player));
				LOGF(WARNING, "Explosion from {} is blamed on {} (owned explosion with another player's ped as owner, type {}; not blocked)", player.GetName(), blamed != players.end() ? blamed->second.GetName() : std::format("player {}", owner_player), static_cast<int>(event.m_ExplosionType));
			}
		}
	}

	static bool ScanPlaySoundEvent(CPlaySoundEvent& event, Player player)
	{
		static constexpr std::array blocked_ref_hashes = {"Arena_Vehicle_Mod_Shop_Sounds"_J, "CELEBRATION_SOUNDSET"_J, "DLC_AW_Arena_Office_Planning_Wall_Sounds"_J, "DLC_AW_Arena_Spin_Wheel_Game_Frontend_Sounds"_J, "DLC_Biker_SYG_Sounds"_J, "DLC_BTL_SECURITY_VANS_RADAR_PING_SOUNDS"_J, "DLC_BTL_Target_Pursuit_Sounds"_J, "DLC_GR_Bunker_Door_Sounds"_J, "DLC_GR_CS2_Sounds"_J, "DLC_IO_Warehouse_Mod_Garage_Sounds"_J, "DLC_MPSUM2_HSW_Up_Sounds"_J, "DLC_sum20_Business_Battle_AC_Sounds"_J, "DLC_TG_Running_Back_Sounds"_J, "dlc_vw_table_games_frontend_sounds"_J, "dlc_xm_facility_entry_exit_sounds"_J, "Frontend"_J, "GTAO_Boss_Goons_FM_Soundset"_J, "GTAO_Exec_SecuroServ_Computer_Sounds"_J, "GTAO_Exec_SecuroServ_Warehouse_PC_Sounds"_J, "GTAO_Script_Doors_Faded_Screen_Sounds"_J, "GTAO_SMG_Hangar_Computer_Sounds"_J, "HUD_AMMO_SHOP_SOUNDSET"_J, "HUD_FRONTEND_CUSTOM_SOUNDSET"_J, "HUD_FRONTEND_DEFAULT_SOUNDSET"_J, "HUD_FRONTEND_MP_SOUNDSET"_J, "HUD_FRONTEND_MP_COLLECTABLE_SOUNDS"_J, "HUD_FRONTEND_TATTOO_SHOP_SOUNDSET"_J, "HUD_FRONTEND_CLOTHESSHOP_SOUNDSET"_J, "HUD_FRONTEND_STANDARD_PICKUPS_NPC_SOUNDSET"_J, "HUD_FRONTEND_VEHICLE_PICKUPS_NPC_SOUNDSET"_J, "HUD_FRONTEND_WEAPONS_PICKUPS_NPC_SOUNDSET"_J, "HUD_FREEMODE_SOUNDSET"_J, "HUD_MINI_GAME_SOUNDSET"_J, "HUD_AWARDS"_J, "JA16_Super_Mod_Garage_Sounds"_J, "Low2_Super_Mod_Garage_Sounds"_J, "MissionFailedSounds"_J, "MP_CCTV_SOUNDSET"_J, "MP_LOBBY_SOUNDS"_J, "MP_MISSION_COUNTDOWN_SOUNDSET"_J, "Phone_SoundSet_Default"_J, "Phone_SoundSet_Glasses_Cam"_J, "Phone_SoundSet_Prologue"_J, "Phone_SoundSet_Franklin"_J, "Phone_SoundSet_Michael"_J, "Phone_SoundSet_Trevor"_J, "PLAYER_SWITCH_CUSTOM_SOUNDSET"_J, "RESPAWN_ONLINE_SOUNDSET"_J, "TATTOOIST_SOUNDS"_J, "WastedSounds"_J, "WEB_NAVIGATION_SOUNDS_PHONE"_J};
		static constexpr std::array blocked_sound_hashes = {"Remote_Ring"_J, "COP_HELI_CAM_ZOOM"_J, "Object_Dropped_Remote"_J};
		static constexpr std::array blocked_script_hashes = {"main_persistent"_J, "shop_controller"_J};

		if (std::ranges::contains(blocked_ref_hashes, event.m_RefHash)
		    || std::ranges::contains(blocked_sound_hashes, event.m_SoundHash)
		    || std::ranges::contains(blocked_script_hashes, event.m_ScriptId.m_Hash))
			return true;

		switch (event.m_SoundHash)
		{
		case "DLC_XM_Explosions_Orbital_Cannon"_J:
		{
			if (event.m_IsEntity)
				return true;

			if (event.m_ScriptId.m_Hash != "am_mp_defunct_base"_J && event.m_ScriptId.m_Hash != "am_mp_orbital_cannon"_J
			    && event.m_ScriptId.m_Hash != "fm_mission_controller_2020"_J && event.m_ScriptId.m_Hash != "fm_mission_controller"_J)
				return true;

			if (!GlobalPlayerBD::Get()->Entries[player.GetId()].OrbitalBitset.IsSet(eOrbitalBitset::kOrbitalCannonActive))
				return true;

			break;
		}
		}

		switch (event.m_RefHash)
		{
		case "GTAO_Biker_Modes_Soundset"_J:
		case "DLC_Biker_Sell_Postman_Sounds"_J:
		{
			if (event.m_IsEntity)
				return true;

			if (event.m_ScriptId.m_Hash != "gb_biker_contraband_sell"_J)
				return true;

			break;
		}
		case "DLC_AW_General_Sounds"_J:
		{
			if (event.m_SoundHash != "Airhorn_Blast_Long"_J)
				return true;

			if (event.m_ScriptId.m_Hash != "gb_casino_heist"_J)
				return true;

			break;
		}
		case "GTAO_FM_Events_Soundset"_J:
		{
			if (!event.m_IsEntity)
				return true;

			if (event.m_SoundHash != "Explosion_Countdown"_J)
				return true;

			break;
		}
		}

		return false;
	}

	// logs a blocked sound (at most once a second per player) and returns true when they come in bursts
	static bool IsBlockedSoundSpam(Player player, const CPlaySoundEvent& event)
	{
		static constexpr auto WINDOW = std::chrono::seconds(5);
		static constexpr int MAX_SOUNDS_PER_WINDOW = 10;

		struct State
		{
			std::chrono::steady_clock::time_point m_WindowStart{};
			int m_Count = 0;
			std::chrono::steady_clock::time_point m_LastLog{};
		};

		static std::array<State, 256> states;
		auto& state = states[static_cast<std::uint8_t>(player.GetId())];
		auto now = std::chrono::steady_clock::now();

		if (now - state.m_WindowStart > WINDOW)
		{
			state.m_WindowStart = now;
			state.m_Count = 0;
		}
		++state.m_Count;

		if (now - state.m_LastLog > std::chrono::seconds(1))
		{
			state.m_LastLog = now;
			LOGF(WARNING, "Blocked NETWORK_PLAY_SOUND_EVENT from {} with IsEntity: {}, RefHash: {:X}, SoundHash: {:X}, SoundId: {}, ScriptHash: {:X} ({} in the last {} s)", player.GetName(), event.m_IsEntity, event.m_RefHash, event.m_SoundHash, event.m_SoundId, event.m_ScriptId.m_Hash, state.m_Count, std::chrono::duration_cast<std::chrono::seconds>(now - state.m_WindowStart).count());
		}

		return state.m_Count > MAX_SOUNDS_PER_WINDOW;
	}

	// drops synced scene updates from players that send them in bursts, which normal players hardly ever do
	static bool IsSyncedSceneSpam(Player player)
	{
		static constexpr auto WINDOW = std::chrono::seconds(5);
		// normal players send one burst of 11-12 within 2 seconds, spammers 24-28 per 5 seconds for minutes
		static constexpr int MAX_EVENTS_PER_WINDOW = 20;

		struct State
		{
			std::chrono::steady_clock::time_point m_WindowStart{};
			int m_Count = 0;
			std::chrono::steady_clock::time_point m_LastLog{};
		};

		static std::array<State, 256> states;
		auto& state = states[static_cast<std::uint8_t>(player.GetId())];
		auto now = std::chrono::steady_clock::now();

		if (now - state.m_WindowStart > WINDOW)
		{
			state.m_WindowStart = now;
			state.m_Count = 0;
		}

		if (++state.m_Count <= MAX_EVENTS_PER_WINDOW)
			return false;

		if (now - state.m_LastLog > std::chrono::seconds(30))
		{
			state.m_LastLog = now;
			LOGF(WARNING, "Blocked NETWORK_UPDATE_SYNCED_SCENE_EVENT spam from {}", player.GetName());
		}

		return true;
	}
	bool ShouldAllowNetEvent(Player player, uint16_t event_id, rage::datBitBuffer& buffer)
	{
		switch (static_cast<rage::netGameEvent::Type>(event_id))
		{
		case rage::netGameEvent::Type::REQUEST_CONTROL_EVENT:
		{
			// the game hands over the vehicle we are driving on request, and the new owner can then move it with us inside.
			// Legacy refuses this in freemode unless the requester drives the vehicle; when we drive, nobody else does.
			// Jobs are left alone, since their scripts may move control of our vehicle around
			auto object_id = buffer.Read<std::uint16_t>(13);
			auto vehicle = Self::GetVehicle();

			if (vehicle && Self::IsDriver() && vehicle.GetNetworkObjectId() == object_id && !NETWORK::NETWORK_IS_ACTIVITY_SESSION())
			{
				LOGF(WARNING, "Blocked REQUEST_CONTROL_EVENT for our vehicle from {}", player.GetName());
				Detections::Add(player, DetectionLevel::Weak, "requesting control of our vehicle");
				return false;
			}

			break;
		}
		case rage::netGameEvent::Type::WEAPON_DAMAGE_EVENT:
		{
			CWeaponDamageEvent event;
			event.Deserialize(buffer);

			if (event.m_WeaponType == "WEAPON_TRANQUILIZER"_J)
			{
				LOGF(WARNING, "Blocked WEAPON_DAMAGE_EVENT from {} with m_WeaponType == WEAPON_TRANQUILIZER", player.GetName());
				Detections::Add(player, DetectionLevel::Strong, "tranquilizer damage (not available in online)");
				return false;
			}

			// legacy blocks a weapon hash that isn't a CWeaponInfo as a crash; IS_WEAPON_VALID asks the same weapon info manager.
			// Log only for now: once per sender and hash, at most 100 per game run
			if (!WEAPON::IS_WEAPON_VALID(event.m_WeaponType))
			{
				static std::mutex mutex;
				static std::set<std::pair<std::uint64_t, std::uint32_t>> seen;
				std::lock_guard lock(mutex);
				if (seen.size() < 100 && seen.emplace(player.GetRID(), event.m_WeaponType).second)
					LOGF(WARNING, "Would block WEAPON_DAMAGE_EVENT from {} with weapon 0x{:08X}: IS_WEAPON_VALID is false (legacy blocks it as a crash, log only)", player.GetName(), event.m_WeaponType);
			}

			break;
		}
		case rage::netGameEvent::Type::EXPLOSION_EVENT:
		{
			CExplosionEvent event;
			event.Deserialize(buffer);
			RecordExplosion(event, player);

			if (ScanExplosionEvent(event, player))
			{
				return false;
			}

			break;
		}
		case rage::netGameEvent::Type::CHANGE_RADIO_STATION_EVENT:
		{
			uint16_t veh_id = buffer.Read<uint16_t>(13);

			Vehicle self_veh = Self::GetVehicle();
			Vehicle sender_veh = player.GetPed().GetVehicle();

			if (self_veh && self_veh.GetNetworkObjectId() == veh_id && sender_veh && sender_veh.GetNetworkObjectId() != veh_id)
			{
				// Vehicle takeover
				LOGF(WARNING, "Blocked CHANGE_RADIO_STATION_EVENT on our local vehicle from {}", player.GetName());
				return false;
			}

			break;
		}
		case rage::netGameEvent::Type::DOOR_BREAK_EVENT:
		{
			// never used for legitimate reasons
			LOGF(WARNING, "Blocked DOOR_BREAK_EVENT from {}", player.GetName());
			return false;
		}
		case rage::netGameEvent::Type::SCRIPTED_GAME_EVENT:
		{
			CScriptedGameEvent event;

			if (!event.Deserialize(buffer))
				return false;

			if (!Network::HandleScriptedGameEvent(player, event))
				return false;

			break;
		}
		case rage::netGameEvent::Type::NETWORK_UPDATE_SYNCED_SCENE_EVENT:
		{
			if (IsSyncedSceneSpam(player))
				return false;

			break;
		}
		case rage::netGameEvent::Type::SCRIPT_WORLD_STATE_EVENT:
		{
			auto type = buffer.Read<CScriptWorldStateEvent::Type>(4);
			buffer.Read<bool>(1);
			CGameScriptId id;
			id.Deserialize(buffer);

			// ported from legacy
			if (type > CScriptWorldStateEvent::Type::VehiclePlayerLocking)
			{
				LOGF(WARNING, "Blocked SCRIPT_WORLD_STATE_EVENT with invalid type {} from {}", static_cast<std::uint32_t>(type), player.GetName());
				// strong, not certain (no host kick): this event is common
				Detections::Add(player, DetectionLevel::Strong, "invalid world state type crash");
				return false;
			}

			if (type == CScriptWorldStateEvent::Type::Rope)
			{
				buffer.Read<int>(9);        // network rope id
				buffer.Read<int>(19, true); // position
				buffer.Read<int>(19, true);
				buffer.Read<int>(19);
				buffer.Read<int>(19, true); // rotation
				buffer.Read<int>(19, true);
				buffer.Read<int>(19);
				float max_length = buffer.ReadSignedFloat(16, 100.0f);
				int rope_type = buffer.Read<int>(4);
				float initial_length = buffer.ReadSignedFloat(16, 100.0f);
				float min_length = buffer.ReadSignedFloat(16, 100.0f);

				if (rope_type == 0 || initial_length < min_length || max_length < min_length || max_length < 0.0f)
				{
					LOGF(WARNING, "Blocked SCRIPT_WORLD_STATE_EVENT of type Rope with invalid lengths from {}", player.GetName());
					// strong, not certain: legacy's field layout of rope data isn't verified on enhanced
					Detections::Add(player, DetectionLevel::Strong, "rope crash");
					return false;
				}
			}

			if (type == CScriptWorldStateEvent::Type::PopGroupOverride)
			{
				int pop_schedule = buffer.Read<int>(8);
				int pop_group = buffer.Read<int>(32);
				int percentage = buffer.Read<int>(7);

				if (pop_group == 0 && (percentage == 0 || percentage == 103))
				{
					// pop group override crash
					LOGF(WARNING, "Blocked SCRIPT_WORLD_STATE_EVENT of type PopGroupOverride with invalid params from {}", player.GetName());
					Detections::Add(player, DetectionLevel::Certain, "pop group override crash");
					return false;
				}
			}
			else if (type == CScriptWorldStateEvent::Type::PopMultiplierArea && _BlockTrafficManipulation.GetState() && !NETWORK::NETWORK_IS_ACTIVITY_SESSION())
			{
				// Stop traffic
				LOGF(WARNING, "Blocked SCRIPT_WORLD_STATE_EVENT of type PopMultiplierArea from {}", player.GetName());
				// no points: normal game scripts send it too
				Detections::Add(player, DetectionLevel::None, "traffic manipulation");
				return false;
			}

			break;
		}
		case rage::netGameEvent::Type::SCRIPT_ENTITY_STATE_CHANGE_EVENT:
		{
			uint16_t entity = buffer.Read<uint16_t>(13);
			auto type = buffer.Read<CScriptEntityStateChangeEvent::Type>(4);
			uint32_t unk = buffer.Read<uint32_t>(32);

			if (type == CScriptEntityStateChangeEvent::Type::SettingOfTaskVehicleTempAction)
			{
				uint16_t veh_id = buffer.Read<uint16_t>(13);
				uint32_t action = buffer.Read<uint32_t>(8);

				if (action >= 15 && action <= 18)
				{
					// vehicle temp action crash
					LOGF(WARNING, "Blocked SCRIPT_ENTITY_STATE_CHANGE_EVENT of type SettingOfTaskVehicleTempAction with invalid params from {}", player.GetName());
					Detections::Add(player, DetectionLevel::Certain, "vehicle temp action crash");
					return false;
				}

				if (Self::GetPed().GetNetworkObjectId() == entity)
				{
					// Ped takeover
					LOGF(WARNING, "Blocked SCRIPT_ENTITY_STATE_CHANGE_EVENT of type SettingOfTaskVehicleTempAction on our local ped from {}", player.GetName());
					Detections::Add(player, DetectionLevel::Strong, "taking over our player");
					return false;
				}

				Vehicle self_veh = Self::GetVehicle();
				if (self_veh && self_veh.GetNetworkObjectId() == veh_id)
				{
					// Vehicle takeover
					LOGF(WARNING, "Blocked SCRIPT_ENTITY_STATE_CHANGE_EVENT of type SettingOfTaskVehicleTempAction on our local vehicle from {}", player.GetName());
					return false;
				}
			}
			else if (type == CScriptEntityStateChangeEvent::Type::SetVehicleLockState)
			{
				Vehicle self_veh = Self::GetVehicle();
				if (self_veh && self_veh.GetNetworkObjectId() == entity)
				{
					LOGF(WARNING, "Blocked SCRIPT_ENTITY_STATE_CHANGE_EVENT of type SetVehicleLockState from {} on our local vehicle", player.GetName());
					return false;
				}
			}
			else if (type == CScriptEntityStateChangeEvent::Type::SetVehicleExclusiveDriver)
			{
				Vehicle self_veh = Self::GetVehicle();
				if (self_veh && self_veh.GetNetworkObjectId() == entity)
				{
					// Vehicle kick
					LOGF(WARNING, "Blocked SCRIPT_ENTITY_STATE_CHANGE_EVENT of type SetVehicleExclusiveDriver from {} on our local vehicle", player.GetName());
					return false;
				}
			}
			else if (type == CScriptEntityStateChangeEvent::Type::SetPedFacialIdleAnimOverride)
			{
				if (Self::GetPed().GetNetworkObjectId() == entity)
				{
					LOGF(WARNING, "Blocked SCRIPT_ENTITY_STATE_CHANGE_EVENT of type SetPedFacialIdleAnimOverride from {} on our local player", player.GetName());
					return false;
				}
			}
			else if (type > CScriptEntityStateChangeEvent::Type::SetVehicleExclusiveDriver)
			{
				// invalid script entity change type crash
				return false;
			}

			break;
		}
		case rage::netGameEvent::Type::NETWORK_PLAY_SOUND_EVENT:
		{
			CPlaySoundEvent event;
			event.Deserialize(buffer);

			if (ScanPlaySoundEvent(event, player))
			{
				if (IsBlockedSoundSpam(player, event))
					Detections::Add(player, DetectionLevel::Strong, "sound spam");
				else
					Detections::Add(player, DetectionLevel::Weak, "blocked sound");
				return false;
			}

			break;
		}
		case rage::netGameEvent::Type::REPORT_MYSELF_EVENT:
		{
			CUpdateFxnEvent event;
			event.Deserialize(buffer);

			// the game's own anticheat caught them and their game reports it to everyone (legacy's TRIGGERED_ANTICHEAT)
			if (event.m_0x30 != 6) // false positives when telemetry endpoint is unreachable
			{
				LOGF(WARNING, "{} triggered Rockstar's anticheat (REPORT_MYSELF_EVENT type {})", player.GetName(), event.m_0x30);
				Detections::Add(player, DetectionLevel::Modder, std::format("triggered the game's anticheat (type {})", event.m_0x30));
			}

			static auto cheaterPool = Commands::GetCommand<LoopedCommand>("cheaterpool"_J);

			if (cheaterPool->GetState())
			{
				LOGF(WARNING, "Blocked REPORT_MYSELF_EVENT from {} with type {} since we're in a cheater-only session", player.GetName(), event.m_0x30);
				return false;
			}

			break;
		}
		case rage::netGameEvent::Type::ACTIVATE_VEHICLE_SPECIAL_ABILITY_EVENT:
		{
			CActivateVehicleSpecialAbilityEvent event;
			event.Deserialize(buffer);

			if (Self::GetPed().GetVehicleObjectId() == event.m_VehicleId && !Self::GetVehicle().IsRemote())
			{
				// Vehicle takeover
				LOGF(WARNING, "Blocked ACTIVATE_VEHICLE_SPECIAL_ABILITY_EVENT on our local vehicle from {}", player.GetName());
				return false;
			}

			break;
		}
		case rage::netGameEvent::Type::RAGDOLL_REQUEST_EVENT:
		{
			CRagdollRequestEvent event;
			event.Deserialize(buffer);

			if (Self::GetPed().GetNetworkObjectId() == event.m_PedToRagdoll)
			{
				// is sometimes used legit, beware
				LOGF(WARNING, "Blocked RAGDOLL_REQUEST_EVENT on our local ped from {}", player.GetName());
				return false;
			}

			break;
		}
		// the cases below are ported from legacy
		case rage::netGameEvent::Type::REMOVE_WEAPON_EVENT:
		{
			auto ped_id = buffer.Read<std::uint16_t>(13);
			auto weapon = buffer.Read<std::uint32_t>(32);

			if (weapon == "WEAPON_UNARMED"_J)
			{
				LOGF(WARNING, "Blocked REMOVE_WEAPON_EVENT with WEAPON_UNARMED (crash) from {}", player.GetName());
				Detections::Add(player, DetectionLevel::Certain, "remove unarmed crash");
				return false;
			}

			if (Self::GetPed().GetNetworkObjectId() == ped_id)
			{
				LOGF(WARNING, "Blocked REMOVE_WEAPON_EVENT on our local ped from {} (weapon {:X})", player.GetName(), weapon);
				Detections::Add(player, DetectionLevel::Weak, "removing our weapons");
				return false;
			}

			break;
		}
		case rage::netGameEvent::Type::GIVE_WEAPON_EVENT:
		{
			auto ped_id = buffer.Read<std::uint16_t>(13);
			auto weapon = buffer.Read<std::uint32_t>(32);

			// not a detection: scripts might give weapons to other players in some modes
			if (Self::GetPed().GetNetworkObjectId() == ped_id)
			{
				LOGF(WARNING, "Blocked GIVE_WEAPON_EVENT on our local ped from {} (weapon {:X})", player.GetName(), weapon);
				return false;
			}

			break;
		}
		case rage::netGameEvent::Type::GIVE_CONTROL_EVENT:
		{
			// legacy blocks object types past TRAIN (a crash)
			buffer.Read<std::uint32_t>(32); // timestamp
			int count = std::min(buffer.Read<int>(2), 3);
			buffer.Read<bool>(1); // all objects migrate together

			for (int i = 0; i < count; i++)
			{
				buffer.Read<std::uint16_t>(13); // object id
				auto object_type = buffer.Read<std::uint32_t>(4);
				buffer.Read<int>(3); // migration type

				// enhanced added GLASS_PANE after TRAIN
				if (object_type > static_cast<std::uint32_t>(rage::netObject::Type::GLASS_PANE))
				{
					LOGF(WARNING, "Blocked GIVE_CONTROL_EVENT with invalid object type {} from {}", object_type, player.GetName());
					Detections::Add(player, DetectionLevel::Certain, "give control with an invalid object type");
					return false;
				}
			}

			break;
		}
		case rage::netGameEvent::Type::NETWORK_CLEAR_PED_TASKS_EVENT:
		{
			auto ped_id = buffer.Read<std::uint16_t>(13);

			if (Self::GetPed().GetNetworkObjectId() == ped_id)
			{
				LOGF(WARNING, "Blocked NETWORK_CLEAR_PED_TASKS_EVENT on our local ped from {}", player.GetName());
				Detections::Add(player, DetectionLevel::Weak, "clearing our ped tasks");
				return false;
			}

			break;
		}
		case rage::netGameEvent::Type::KICK_VOTES_EVENT:
		{
			// normal players can vote to kick, so only tell the user
			auto voted = buffer.Read<std::uint32_t>(32);
			if (auto self = Self::GetPlayer().GetId(); self < 32 && (voted & (1u << self)))
				LOGF(WARNING, "{} voted to kick you", player.GetName());

			break;
		}
		default:
			break;
		}

		return true;
	}

	void Network::ReceiveNetGameEvent(Player player, uint16_t event_id, uint32_t event_index, uint32_t event_handled_bits, rage::datBitBuffer& buffer)
	{
		if (!ShouldAllowNetEvent(player, event_id, buffer))
			SendEventAck(*Pointers.NetEventMgr, player.GetHandle(), Self::GetPlayer().GetHandle(), event_index, event_handled_bits);
	}
}