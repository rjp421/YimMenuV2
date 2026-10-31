#include "game/backend/BailMonitor.hpp"
#include "game/backend/Detections.hpp"
#include "game/backend/NetRecorder.hpp"
#include "game/backend/Self.hpp"
#include "game/gta/Natives.hpp"
#include "game/gta/Scripts.hpp"
#include "game/gta/data/LauncherScripts.hpp"
#include "game/hooks/Hooks.hpp"
#include "game/pointers/Pointers.hpp"
#include "types/network/netGameEvent.hpp"
#include "types/script/globals/GPBD_FM_3.hpp"
#include "types/script/globals/GlobalPlayerBD.hpp"
#include "types/script/ScriptEvent.hpp"
#include "core/scripting/LuaManager.hpp"
#include "core/scripting/LuaUtils.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/util/Joaat.hpp"

namespace YimMenu::Hooks
{
	// Bounty is legacy's toggle (same default). Legacy doesn't block text label SMS at all, V2 always did, so this is on by default
	static BoolCommand _BlockBounty{"blockbounty", "Bounty", "Blocks bounties that other players place on you", true};
	static BoolCommand _BlockSendSMS{"blocksendsms", "Send SMS", "Blocks text label messages from other players (used for spam)", true};

	// ported from legacy's script event protections, same defaults
	static BoolCommand _BlockCeoMoney{"blockceomoney", "CEO Money", "Blocks CEO payments from players who are not your boss", true};
	static BoolCommand _BlockClearWantedLevel{"blockclearwantedlevel", "Clear Wanted Level", "Blocks other players clearing your wanted level (unless they are driving your vehicle)", true};
	static BoolCommand _BlockForceMission{"blockforcemission", "Force Mission", "Blocks other players forcing you into a mission", true};
	static BoolCommand _BlockForceTeleport{"blockforceteleport", "Force Teleport", "Blocks other players teleporting you to a property (unless they are driving your vehicle)", true};
	static BoolCommand _BlockGtaBanner{"blockgtabanner", "GTA Banner", "Blocks other players showing a GTA banner on your screen", false};
	static BoolCommand _BlockMCTeleport{"blockmcteleport", "MC Teleport", "Blocks MC teleports from players who are not your boss", true};
	static BoolCommand _BlockPersonalVehicleDestroyed{"blockpersonalvehicledestroyed", "Personal Vehicle Destroyed", "Blocks fake \"personal vehicle destroyed\" messages", true};
	static BoolCommand _BlockRemoteOffRadar{"blockremoteoffradar", "Remote Off Radar", "Blocks other players giving you off radar (unless they are your boss or driving your vehicle)", true};
	static BoolCommand _BlockRotateCam{"blockrotatecam", "Rotate Cam", "Blocks other players rotating your camera (outside of jobs)", true};
	static BoolCommand _BlockSendToCutscene{"blocksendtocutscene", "Send To Cutscene", "Blocks cutscenes started by players who are not your boss", true};
	static BoolCommand _BlockSendToLocation{"blocksendtolocation", "Send To Location", "Blocks other players sending you to a location, unless they are your boss", true};
	static BoolCommand _BlockSoundSpam{"blocksoundspam", "Sound Spam", "Blocks invites and sounds that a player sends to you too often", true};
	static BoolCommand _BlockSpectate{"blockspectate", "Spectate", "Blocks the \"is spectating you\" message", true};
	static BoolCommand _BlockGiveCollectible{"blockgivecollectible", "Give Collectible", "Blocks other players giving you collectibles", true};
	static BoolCommand _BlockVehicleKick{"blockvehiclekick", "Vehicle Kick", "Blocks other players kicking you out of your vehicle", true};
	static BoolCommand _BlockTeleportToWarehouse{"blockteleporttowarehouse", "Teleport To Warehouse", "Blocks other players teleporting you to a warehouse (unless they are driving your vehicle)", true};
	static BoolCommand _BlockStartActivity{"blockstartactivity", "Start Activity", "Blocks activities started by players who are not your goons", true};
	static BoolCommand _BlockStartScript{"blockstartscript", "Start Script", "Blocks other players starting minigames, arcade games and jobs for you", true};

	// legacy's script launch list (util/protection.cpp): scripts another player may not start for us, by g_LauncherScripts index
	static bool IsAllowedScriptLaunch(int index)
	{
		if (index < 0 || index >= static_cast<int>(g_LauncherScripts.size()))
			return false;

		switch (g_LauncherScripts[index])
		{
		case "AM_Darts"_J:
		case "AM_PI_MENU"_J:
		case "fm_intro"_J:
		case "Pilot_School_MP"_J:
		case "am_darts_apartment"_J:
		case "grid_arcade_cabinet"_J:
		case "scroll_arcade_cabinet"_J:
		case "example_arcade"_J:
		case "road_arcade"_J:
		case "gunslinger_arcade"_J:
		case "wizard_arcade"_J:
		case "ggsm_arcade"_J:
		case "puzzle"_J:
		case "camhedz_arcade"_J:
		case "fm_content_ufo_abduction"_J:
		case "SCTV"_J: return false;
		// only in jobs
		case "golf_mp"_J:
		case "tennis_network_mp"_J:
		case "FM_Impromptu_DM_Controler"_J:
		case "fm_deathmatch_controler"_J:
		case "FM_Race_Controler"_J:
		case "FM_Horde_Controler"_J: return NETWORK::NETWORK_IS_ACTIVITY_SESSION();
		default: return true;
		}
	}

	static bool CheckLuaScripts(Player player, CScriptedGameEvent& event)
	{
		return LuaManager::DispatchEvent(MenuEvent::ScriptedGameEventReceived, [player, &event](lua_State* state)
		{
			Lua::CreateObject<YimMenu::Player>(state, player);

			lua_newtable(state);
			auto length = event.m_ArgsSize / 8;
			for (int i = 0; i < length; i++)
			{
				lua_pushinteger(state, i == 0 ? (ptrdiff_t)(int)event.m_Args[i] : event.m_Args[i]);
				lua_rawseti(state, -2, i + 1);
			}

			return 2;
		}, true);
	}

	// most events have 3-7 arguments, but normal players send these regularly, e.g. 0xC3959757 with 41 arguments
	// when joining and when leaving an interior
	static constexpr std::uint32_t LARGE_EVENT_ARGS = 12;

	static bool IsKnownLargeEvent(std::uint32_t index, std::uint32_t arg_count)
	{
		switch (index)
		{
		case 0xC3959757: return arg_count == 41;
		case 0xD9B11BFD: return arg_count == 15;
		case 0xCCF2F1D1: return arg_count == 21;
		case 0x1239820C: return arg_count == 21;
		case 0x5A73FFBB: return arg_count == 23;
		case 0x7D127259: return arg_count == 23; // BountySurvived, sent by freemode
		case 0xA94AF7A5:
			return arg_count == 16; // job invite: args[9..11] = the job's content id as text
		// each sent by many different normal players
		case 0x5BB7E31E: return arg_count == 24;
		case 0xF0913C4A: return arg_count == 27;
		case 0xD6F24305: return arg_count == 20;
		case 0x6C8FC52E: return arg_count == 20;
		case 0x3F2DC73A: return arg_count == 26;
		case 0x27531209: return arg_count == 20;
		// these four carry a 22 character job content id as text, like the job invite above
		case 0x48688E47: return arg_count == 18;
		case 0x9E955D3F: return arg_count == 24;
		case 0xD217F8D0: return arg_count == 24;
		case 0xF5382545: return arg_count == 24;
		// each sent by many different players, none of them known attackers
		case 0x9E561FD0: return arg_count == 16; // InteriorControl (still checked below)
		case 0x41C063AD: return arg_count == 38; // MCTeleport (still checked below)
		case 0x1899E37A: return arg_count == 24; // handled by fmmc_launcher and freemode, sent by normal players
		case 0xE57929E7: return arg_count == 41; // not in the 42 dumped scripts
		case 0x919AEEA1:
			return arg_count == 28; // not in the 42 dumped scripts
		// arcade machines (am_mp_arc_cab_manager): args[3..] are text such as CLAW_START_PLAY or LOVETEST_START_PLAY (7 players)
		case 0x4D8369CB: return arg_count == 40;
		default: return false;
		}
	}

	static void RecordScriptedGameEvent(Player player, CScriptedGameEvent& event)
	{
		auto arg_count = std::min<std::uint32_t>(event.m_ArgsSize / 8, std::size(event.m_Args));
		if (arg_count == 0)
			return;

		auto index = static_cast<std::uint32_t>(event.m_Args[0]);
		NetRecorder::RecordScriptEvent(player, event.m_Args, arg_count);

		if (arg_count > LARGE_EVENT_ARGS && !IsKnownLargeEvent(index, arg_count))
		{
			// a player can repeat one for minutes, so log the first one per sender and index, then at most one line per minute
			// with the count (the net recorder still has every one)
			static std::mutex mutex;
			static std::map<std::pair<std::uint64_t, std::uint32_t>, std::pair<std::chrono::steady_clock::time_point, int>> last_logs;
			auto key = std::make_pair(player ? player.GetRID() : 0, index);
			auto now = std::chrono::steady_clock::now();
			int repeats;
			{
				std::lock_guard lock(mutex);
				auto& [last, count] = last_logs[key];
				if (last != std::chrono::steady_clock::time_point{} && now - last < std::chrono::minutes(1))
				{
					count++;
					return;
				}
				repeats = count;
				last = now;
				count = 0;
			}

			std::string args;
			for (std::uint32_t i = 0; i < std::min<std::uint32_t>(arg_count, 16); i++)
				args += std::format("{:X} ", event.m_Args[i]);

			LOGF(WARNING, "Unusual large scripted game event from {}: index 0x{:X} with {} args{}, first ones: {}", player ? player.GetName() : "unknown", index, arg_count, repeats ? std::format(" (and {} more in the last minute)", repeats) : "", args);
		}
	}

	// script ints are 32 bit (the upper half of an argument can be garbage), so compare the lower half like the game does
	static int Arg(CScriptedGameEvent& event, std::uint32_t index)
	{
		if (index >= std::min<std::uint32_t>(event.m_ArgsSize / 8, std::size(event.m_Args)))
			return 0;
		return static_cast<int>(event.m_Args[index]);
	}

	static bool IsNaN(int value)
	{
		return std::isnan(std::bit_cast<float>(value));
	}

	// our player index, or -1 when it's not a valid index into the 32 entry player arrays of the script globals
	static int SelfIndex()
	{
		auto id = Self::GetPlayer().GetId();
		return id >= 0 && id < 32 ? id : -1;
	}

	static bool IsOurBoss(Player player)
	{
		auto fm3 = GPBD_FM_3::Get();
		return fm3 && SelfIndex() != -1 && player.GetId() == fm3->Entries[SelfIndex()].BossGoon.Boss;
	}

	static bool IsOurGoon(Player player)
	{
		auto fm3 = GPBD_FM_3::Get();
		if (!fm3 || SelfIndex() == -1)
			return false;

		auto& boss_goon = fm3->Entries[SelfIndex()].BossGoon;
		if (boss_goon.Boss != Self::GetPlayer().GetId())
			return false;

		for (int i = 0; i < std::min(boss_goon.Goons.Size, 8); i++)
			if (static_cast<int>(boss_goon.Goons[i]) == player.GetId())
				return true;
		return false;
	}

	static bool IsDriverOfOurVehicle(Player player)
	{
		if (!Self::GetPed())
			return false;

		auto vehicle = Self::GetPed().GetVehicle();
		auto ped = player.GetPed();
		if (!vehicle || !ped)
			return false;

		return VEHICLE::GET_PED_IN_VEHICLE_SEAT(vehicle.GetHandle(), -1, false) == ped.GetHandle();
	}

	// like legacy's rate limiter: at most `allowed` events per `period`, per player
	template<int Allowed, int PeriodSeconds>
	static bool IsRateLimited(Player player)
	{
		struct State
		{
			std::chrono::steady_clock::time_point m_Start{};
			int m_Count = 0;
		};
		static std::array<State, 256> states;

		auto& state = states[static_cast<std::uint8_t>(player.GetId())];
		auto now = std::chrono::steady_clock::now();
		if (now - state.m_Start < std::chrono::seconds(PeriodSeconds))
			return ++state.m_Count > Allowed;

		state.m_Start = now;
		state.m_Count = 1;
		return false;
	}

	// logs (at most once per second per player, sound spam can be fast), records and saves a netlog, then drops the event
	// more than 10 vehicle kicks in 5 seconds
	static bool IsVehicleKickSpam(Player player)
	{
		static constexpr auto WINDOW = std::chrono::seconds(5);
		static constexpr int MAX_KICKS_PER_WINDOW = 10;

		static std::array<std::pair<std::chrono::steady_clock::time_point, int>, 256> states{};
		auto& [window_start, count] = states[static_cast<std::uint8_t>(player.GetId())];
		auto now = std::chrono::steady_clock::now();
		if (now - window_start > WINDOW)
		{
			window_start = now;
			count = 0;
		}
		return ++count > MAX_KICKS_PER_WINDOW;
	}

	static bool Block(Player player, CScriptedGameEvent& event, std::string_view what, DetectionLevel level)
	{
		static std::array<std::chrono::steady_clock::time_point, 256> last_log{};
		auto index = static_cast<std::uint32_t>(event.m_Args[0]);
		auto& last = last_log[static_cast<std::uint8_t>(player.GetId())];
		auto now = std::chrono::steady_clock::now();
		if (now - last > std::chrono::seconds(1))
		{
			last = now;
			LOGF(WARNING, "Blocked script event {} (0x{:X}) from {}", what, index, player ? player.GetName() : "unknown");
		}

		NetRecorder::Record(player, NetRecorder::Kind::BlockedScriptEvent, index);
		NetRecorder::DumpBlocked("blocked scripted game event", player, what, level >= DetectionLevel::Strong);
		Detections::Add(player, level, std::format("script event {}", what));
		return false;
	}

	bool Network::HandleScriptedGameEvent(Player player, CScriptedGameEvent& event)
	{
		RecordScriptedGameEvent(player, event);

		if (!CheckLuaScripts(player, event))
			return false;

		SCRIPT_EVENT* script_event = reinterpret_cast<SCRIPT_EVENT*>(event.m_Args);

		switch (static_cast<ScriptEventIndex>(script_event->GetEventIndex()))
		{
		case ScriptEventIndex::Bounty:
		{
			SCRIPT_EVENT_BOUNTY* bounty = static_cast<SCRIPT_EVENT_BOUNTY*>(script_event);

			if (event.m_ArgsSize != SCRIPT_EVENT_BOUNTY::GetSize())
			{
				//player.AddDetection();
				return false;
			}

			if (_BlockBounty.GetState() && bounty->Target == Self::GetPlayer().GetId())
			{
				return false;
			}

			break;
		}
		case ScriptEventIndex::SendTextLabelSMS:
		{
			if (!_BlockSendSMS.GetState())
				break;

			//player.AddDetection();
			return false;
		}
		case ScriptEventIndex::CeoKick:
		{
			if (!IsOurBoss(player))
			{
				return false;
			}

			break;
		}
		case ScriptEventIndex::CeoMoney:
		{
			if (_BlockCeoMoney.GetState() && !IsOurBoss(player))
				return Block(player, event, "CEO money", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::ClearWantedLevel:
		{
			if (_BlockClearWantedLevel.GetState() && !IsDriverOfOurVehicle(player))
				return Block(player, event, "clear wanted level", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::Crash:
		{
			return Block(player, event, "crash", DetectionLevel::Certain);
		}
		case ScriptEventIndex::Crash2:
		{
			if (Arg(event, 3) > 32) // the game crashes above 255
				return Block(player, event, "crash 2", DetectionLevel::Certain);
			break;
		}
		case ScriptEventIndex::Crash3:
		{
			if (IsNaN(Arg(event, 3)) || IsNaN(Arg(event, 4)) || IsNaN(Arg(event, 5)))
				return Block(player, event, "crash 3 (NaN)", DetectionLevel::Certain);
			if (Arg(event, 3) == -4640169 && Arg(event, 7) == -36565476 && Arg(event, 8) == -53105203)
				return Block(player, event, "crash 3", DetectionLevel::Certain);
			break;
		}
		case ScriptEventIndex::Notification:
		{
			switch (static_cast<ScriptEventIndex>(Arg(event, 3)))
			{
			case ScriptEventIndex::NotificationMoneyBanked:
			case ScriptEventIndex::NotificationMoneyRemoved:
			case ScriptEventIndex::NotificationMoneyStolen: return Block(player, event, "fake money notification", DetectionLevel::Strong);
			case ScriptEventIndex::NotificationCrash1: return Block(player, event, "notification crash", DetectionLevel::Certain);
			case ScriptEventIndex::NotificationCrash2:
				// meant to cause a sound crash by spamming notifications, only used by the salvage yard script
				if (!Scripts::FindScriptThread("gb_salvage"_J))
					return Block(player, event, "notification spam", DetectionLevel::Weak);
				break;
			default: break;
			}
			break;
		}
		case ScriptEventIndex::StartScriptBegin:
		{
			// freemode's "force player on mission" starts g_LauncherScripts[args[3]] for us. No detection points until it is
			// seen in real sessions
			auto index = Arg(event, 3);
			auto script = index >= 0 && index < static_cast<int>(g_LauncherScripts.size()) ? g_LauncherScripts[index] : 0;
			if (_BlockStartScript.GetState() && !IsAllowedScriptLaunch(index))
				return Block(player, event, std::format("start script {} (0x{:X})", index, script), DetectionLevel::None);
			LOGF(INFO, "Allowed StartScriptBegin from {} for launcher script {} (0x{:X})", player.GetName(), index, script);
			break;
		}
		case ScriptEventIndex::StartScriptProceed:
		{
			// legacy drops this from anyone but freemode's script host, with a "TODO: Breaks stuff": log only until we see real ones
			static std::array<std::chrono::steady_clock::time_point, 256> last_log{};
			auto host = BailMonitor::FreemodeHost();
			auto& last = last_log[static_cast<std::uint8_t>(player.GetId())];
			if (host >= 0 && host != player.GetId() && std::chrono::steady_clock::now() - last > std::chrono::seconds(10))
			{
				last = std::chrono::steady_clock::now();
				LOGF(WARNING, "Would block StartScriptProceed from {}: not the freemode script host (legacy's check, log only)", player.GetName());
			}
			break;
		}
		case ScriptEventIndex::ForceMission:
		{
			if (_BlockForceMission.GetState())
				return Block(player, event, "force mission", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::GiveCollectible:
		{
			if (_BlockGiveCollectible.GetState())
				return Block(player, event, "give collectible", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::GtaBanner:
		{
			if (_BlockGtaBanner.GetState())
				return Block(player, event, "GTA banner", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::MCTeleport:
		{
			int count = Arg(event, 4);
			if (count > 32)
				return Block(player, event, "MC teleport crash", DetectionLevel::Certain);

			if (_BlockMCTeleport.GetState() && !IsOurBoss(player))
			{
				auto our_hash = static_cast<int>(NETWORK::NETWORK_HASH_FROM_PLAYER_HANDLE(Self::GetPlayer().GetId()));
				for (std::uint32_t i = 0; i < 32; i++)
					if (Arg(event, 5 + i) == our_hash)
						return Block(player, event, "MC teleport", DetectionLevel::Weak);
			}
			break;
		}
		case ScriptEventIndex::PersonalVehicleDestroyed:
		{
			if (_BlockPersonalVehicleDestroyed.GetState())
				return Block(player, event, "personal vehicle destroyed", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::RemoteOffradar:
		{
			if (_BlockRemoteOffRadar.GetState() && !IsOurBoss(player) && !IsDriverOfOurVehicle(player))
				return Block(player, event, "remote off radar", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::TSECommand:
		{
			if (NETWORK::NETWORK_IS_ACTIVITY_SESSION())
				break;

			auto command = static_cast<ScriptEventIndex>(Arg(event, 3));
			if (_BlockRotateCam.GetState() && command == ScriptEventIndex::TSECommandRotateCam)
				return Block(player, event, "rotate cam", DetectionLevel::Weak);
			if (_BlockSoundSpam.GetState() && command == ScriptEventIndex::TSECommandSound && IsRateLimited<2, 5>(player))
				return Block(player, event, "sound spam", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::SendToCayoPerico:
		{
			if (_BlockSendToLocation.GetState() && Arg(event, 4) == 0)
				return Block(player, event, "send to Cayo Perico", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::SendToCutscene:
		{
			if (_BlockSendToCutscene.GetState() && !IsOurBoss(player))
				return Block(player, event, "send to cutscene", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::SendToLocation:
		{
			if (IsOurBoss(player))
				break;

			bool known_location = Arg(event, 3) == 0 && Arg(event, 4) == 0
			    && ((Arg(event, 5) == 4 && Arg(event, 6) == 0) || ((Arg(event, 5) == 3 || Arg(event, 5) == 4) && Arg(event, 6) == 1));

			// an unknown location freezes the game
			if (!known_location)
				return Block(player, event, "send to location freeze", DetectionLevel::Strong);
			if (_BlockSendToLocation.GetState())
				return Block(player, event, "send to location", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::SoundSpam:
		{
			if (_BlockSoundSpam.GetState() && IsRateLimited<2, 10>(player))
				return Block(player, event, "invite sound spam", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::Spectate:
		{
			if (_BlockSpectate.GetState())
				return Block(player, event, "spectate notification", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::Teleport:
		{
			if (_BlockForceTeleport.GetState() && !IsDriverOfOurVehicle(player))
				return Block(player, event, "force teleport", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::TransactionError:
		{
			return Block(player, event, "transaction error", DetectionLevel::Strong);
		}
		case ScriptEventIndex::VehicleKick:
		{
			if (_BlockVehicleKick.GetState())
			{
				if (IsVehicleKickSpam(player))
					return Block(player, event, "vehicle kick spam", DetectionLevel::Strong);
				return Block(player, event, "vehicle kick", DetectionLevel::Weak);
			}
			break;
		}
		case ScriptEventIndex::NetworkBail:
		{
			return Block(player, event, "network bail kick", DetectionLevel::Certain);
		}
		case ScriptEventIndex::TeleportToWarehouse:
		{
			if (_BlockTeleportToWarehouse.GetState() && !IsDriverOfOurVehicle(player))
				return Block(player, event, "teleport to warehouse", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::StartActivity:
		{
			auto activity = static_cast<eActivityType>(Arg(event, 3));
			if (activity == eActivityType::Tennis)
				return Block(player, event, "start activity crash (tennis)", DetectionLevel::Certain);

			if (_BlockStartActivity.GetState())
			{
				switch (activity)
				{
				case eActivityType::Survival:
				case eActivityType::Mission:
				case eActivityType::Deathmatch:
				case eActivityType::BaseJump:
				case eActivityType::Race: return Block(player, event, "start activity freeze", DetectionLevel::Strong);
				case eActivityType::Darts:
				case eActivityType::PilotSchool:
				case eActivityType::ImpromptuDeathmatch: return Block(player, event, "start activity", DetectionLevel::Weak);
				case eActivityType::DefendSpecialCargo:
				case eActivityType::GunrunningDefend:
				case eActivityType::BikerDefend: return Block(player, event, "business raid", DetectionLevel::Weak);
				default:
					if (static_cast<int>(activity) == 238)
						return Block(player, event, "business raid", DetectionLevel::Weak);
					break;
				}

				if (!IsOurGoon(player))
					return Block(player, event, "start activity", DetectionLevel::Weak);
			}
			break;
		}
		case ScriptEventIndex::InteriorControl:
		{
			SCRIPT_EVENT_SEND_TO_INTERIOR* interior_control = static_cast<SCRIPT_EVENT_SEND_TO_INTERIOR*>(script_event);

			if (interior_control->Interior < 0 || interior_control->Interior >= static_cast<int>(eSimpleInteriorIndex::SIMPLE_INTERIOR_MAX)) // the upper bound will change after an update
			{
				return Block(player, event, "null function kick", DetectionLevel::Certain);
			}

			if (!interior_control->GoonsOnly)
			{
				// send to interior
				return false;
			}

			break;
		}
		case ScriptEventIndex::DestroyPersonalVehicle:
		{
			return Block(player, event, "destroy personal vehicle", DetectionLevel::Weak);
		}
		case ScriptEventIndex::KickFromInterior:
		{
			auto bd = GlobalPlayerBD::Get();
			if (bd && SelfIndex() != -1 && bd->Entries[SelfIndex()].SimpleInteriorData.Owner != player.GetId())
				return Block(player, event, "kick from interior", DetectionLevel::Weak);
			break;
		}
		case ScriptEventIndex::TriggerCEORaid:
		{
			// legacy drops these from everyone
			return Block(player, event, "trigger CEO raid", DetectionLevel::None);
		}
		case ScriptEventIndex::PlayerBitsetUpdate:
		{
			// args[3] is a player index, args[4] indexes a script array (freemode accepts 0..25 elsewhere, -1 = none)
			if (Arg(event, 3) < 0 || Arg(event, 3) >= 32 || Arg(event, 4) < -1 || Arg(event, 4) > 25)
				return Block(player, event, "array overrun kick", DetectionLevel::Certain);
			break;
		}
		case ScriptEventIndex::UnknownKick:
		{
			// freemode only accepts a player index in args[3]; the kicker sent hash-like values, which only a menu makes
			if (Arg(event, 3) < 0 || Arg(event, 3) > 31)
				return Block(player, event, "malformed player bit event (kicker)", DetectionLevel::Strong);
			break;
		}
		case ScriptEventIndex::BountySurvived:
		{
			// a bounty is survived once, minutes apart; more than 3 a minute is a money loop. Not harmful to us, so it is a strong
			// detection (marks them, no host kick) and the extra ones are dropped so the feed isn't flooded. Not Block(): it would log
			// every second and save a netlog every 30 seconds for as long as the loop runs
			if (IsRateLimited<3, 60>(player))
			{
				static std::array<std::chrono::steady_clock::time_point, 256> last_log{}, last_dump{};
				auto slot = static_cast<std::uint8_t>(player.GetId());
				auto now = std::chrono::steady_clock::now();
				if (last_log[slot] == std::chrono::steady_clock::time_point{} || now - last_log[slot] > std::chrono::minutes(1))
				{
					last_log[slot] = now;
					LOGF(WARNING, "Blocked script event bounty survived spam (0x{:X}) from {} (money loop)", static_cast<std::uint32_t>(event.m_Args[0]), player.GetName());
				}
				NetRecorder::Record(player, NetRecorder::Kind::BlockedScriptEvent, static_cast<std::uint32_t>(event.m_Args[0]));
				if (last_dump[slot] == std::chrono::steady_clock::time_point{} || now - last_dump[slot] > std::chrono::minutes(10))
				{
					last_dump[slot] = now;
					NetRecorder::Dump("blocked scripted game event");
				}
				Detections::Add(player, DetectionLevel::Strong, "bounty survived spam (money loop)");
				return false;
			}
			break;
		}
		default:
			break;
		}

		// menus that don't set the sender argument to their own player id
		if (player && Arg(event, 1) != player.GetId())
			return Block(player, event, "with a wrong sender", DetectionLevel::Strong);

		return true;
	}
}
