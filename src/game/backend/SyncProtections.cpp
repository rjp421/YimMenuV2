#include "SyncProtections.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/util/Joaat.hpp"
#include "game/backend/Detections.hpp"
#include "game/backend/NetRecorder.hpp"
#include "game/backend/Self.hpp"
#include "game/gta/Natives.hpp"
#include "game/gta/Nodes.hpp"
#include "types/network/netObject.hpp"
#include "types/network/sync/CProjectBaseSyncDataNode.hpp"
#include "types/network/sync/netSyncTree.hpp"
#include "types/network/sync/nodes/door/CDoorCreationDataNode.hpp"
#include "types/network/sync/nodes/dynamic_entity/CDynamicEntityGameStateDataNode.hpp"
#include "types/network/sync/nodes/entity/CEntityScriptInfoDataNode.hpp"
#include "types/network/sync/nodes/object/CObjectCreationDataNode.hpp"
#include "types/network/sync/nodes/ped/CPedAttachDataNode.hpp"
#include "types/network/sync/nodes/ped/CPedCreationDataNode.hpp"
#include "types/network/sync/nodes/ped/CPedGameStateDataNode.hpp"
#include "types/network/sync/nodes/ped/CPedMovementGroupDataNode.hpp"
#include "types/network/sync/nodes/ped/CPedTaskTreeDataNode.hpp"
#include "types/network/sync/nodes/physical/CPhysicalAttachDataNode.hpp"
#include "types/network/sync/nodes/pickup/CPickupCreationDataNode.hpp"
#include "types/network/sync/nodes/player/CPlayerAppearanceDataNode.hpp"
#include "types/network/sync/nodes/player/CPlayerCameraDataNode.hpp"
#include "types/network/sync/nodes/player/CPlayerCreationDataNode.hpp"
#include "types/network/sync/nodes/player/CPlayerGameStateDataNode.hpp"
#include "types/network/sync/nodes/player/CPlayerGamerDataNode.hpp"
#include <set>
#include "types/network/sync/nodes/player/CPlayerSectorPosNode.hpp"
#include "types/network/sync/nodes/proximity_migrateable/CSectorDataNode.hpp"
#include "types/network/sync/nodes/train/CTrainGameStateDataNode.hpp"
#include "types/network/sync/nodes/vehicle/CVehicleControlDataNode.hpp"
#include "types/network/sync/nodes/vehicle/CVehicleCreationDataNode.hpp"
#include "types/network/sync/nodes/vehicle/CVehicleGadgetDataNode.hpp"
#include "types/network/sync/nodes/vehicle/CVehicleProximityMigrationDataNode.hpp"
#include "types/network/sync/nodes/vehicle/CVehicleTaskDataNode.hpp"

namespace YimMenu
{
	static BoolCommand _BlockBadSyncData{"blockbadsyncdata", "Block Bad Sync Data", "Rejects incoming entity data that can crash the game. When off, it is only logged, except for known crashes", true};

	static thread_local Player t_Sender{};

	SyncProtections::SenderScope::SenderScope(Player sender) :
	    m_Previous(t_Sender)
	{
		t_Sender = sender;
	}

	SyncProtections::SenderScope::~SenderScope()
	{
		t_Sender = m_Previous;
	}

	Player SyncProtections::GetCurrentSender()
	{
		return t_Sender;
	}

	// models from legacy YimMenu (util/protection.cpp) that crash when they are created over the network
	// clang-format off
	static const std::unordered_set<std::uint32_t> g_CrashObjects = {
	    "proc_drygrasses01"_J, "proc_drygrasses01b"_J, "proc_brittlebush_01"_J, "proc_forest_ivy_01"_J, "proc_grassdandelion01"_J,
	    "proc_grasses01b"_J, "proc_grassfronds01"_J, "proc_grassplantmix_01"_J, "proc_indian_pbrush_01"_J, "proc_leafybush_01"_J,
	    "proc_lizardtail_01"_J, "proc_lupins_01"_J, "proc_meadowmix_01"_J, "proc_meadowpoppy_01"_J, "proc_desert_sage_01"_J,
	    "prop_saplin_001_b"_J, "prop_dummy_01"_J, "prop_dummy_car"_J, "prop_dummy_light"_J, "prop_dummy_plane"_J,
	    "prop_distantcar_night"_J, "prop_distantcar_day"_J, "hei_bh1_08_details4_em_night"_J, "dt1_18_sq_night_slod"_J, "ss1_12_night_slod"_J,
	    "hash_b334b5e2_qyquzxq_collision"_J, "h4_prop_bush_bgnvla_med_01"_J, "h4_prop_bush_bgnvla_lrg_01"_J, "h4_prop_bush_buddleia_low_01"_J, "h4_prop_bush_ear_aa"_J,
	    "h4_prop_bush_ear_ab"_J, "h4_prop_bush_fern_low_01"_J, "h4_prop_bush_fern_tall_cc"_J, "h4_prop_bush_mang_ad"_J, "h4_prop_bush_mang_low_aa"_J,
	    "h4_prop_bush_mang_low_ab"_J, "h4_prop_bush_seagrape_low_01"_J, "prop_h4_ground_cover"_J, "h4_prop_weed_groundcover_01"_J, "h4_prop_grass_med_01"_J,
	    "h4_prop_grass_tropical_lush_01"_J, "h4_prop_grass_wiregrass_01"_J, "h4_prop_weed_01_plant"_J, "h4_prop_weed_01_row"_J, "urbanweeds02_l1"_J,
	    "proc_forest_grass01"_J, "prop_small_bushyba"_J, "v_res_d_dildo_a"_J, "v_res_d_dildo_b"_J, "v_res_d_dildo_c"_J,
	    "v_res_d_dildo_d"_J, "v_res_d_dildo_e"_J, "v_res_d_dildo_f"_J, "v_res_skateboard"_J, "prop_battery_01"_J,
	    // not prop_bbq_3: missions pass it between normal players
	    "prop_barbell_01"_J, "prop_barbell_02"_J, "prop_bandsaw_01"_J, "v_med_curtainsnewcloth2"_J,
	    "bh1_07_flagpoles"_J, "hash_058a7eb5_deihiws_collision"_J, "proc_dry_plants_01"_J, "proc_leafyplant_01"_J, "proc_grassplantmix_02"_J,
	    "proc_dryplantsgrass_01"_J, "proc_dryplantsgrass_02"_J, "proc_grasses01"_J, "prop_dryweed_002_a"_J, "prop_fernba"_J,
	    "prop_weed_001_aa"_J, "urbangrnfrnds_01"_J, "urbanweeds01"_J, "prop_dandy_b"_J, "v_proc2_temp"_J,
	    "prop_fernbb"_J, "proc_drygrassfronds01"_J, "prop_log_ae"_J, "prop_grass_da"_J, "prop_fragtest_cnst_04"_J,
	    "prop_thindesertfiller_aa"_J,
	};
	// clang-format on
	static const std::unordered_set<std::uint32_t> g_CrashPeds = {"slod_human"_J, "slod_small_quadped"_J, "slod_large_quadped"_J};

	static BoolCommand _BlockCages{"blockcages", "Block Cages", "Rejects the cage, container and tube objects that menus spawn around players to trap them", true};

	// objects that menus spawn around a player to trap them; in the recorded traffic only attackers created them in freemode.
	// Stunt races build their tracks from the tubes, so jobs are left alone
	static const std::unordered_set<std::uint32_t> g_CageObjects = {
	    "prop_gold_cont_01"_J,
	    "prop_container_05a"_J,
	    "stt_prop_stunt_tube_s"_J,
	    "stt_prop_stunt_tube_xxs"_J,
	    // names unknown, created together with the ones above
	    0x2DB04373,
	    0xA2023E64,
	    0xAF9E03CD,
	};

	// the model natives only read the model info table, so they are safe to call from the network thread
	static bool IsBadPedModel(std::uint32_t model)
	{
		// a player or ped with a model that is not a ped crashes the game when its clothes are applied
		return g_CrashPeds.contains(model) || !STREAMING::IS_MODEL_A_PED(model);
	}

	static bool IsBadVehicleModel(std::uint32_t model)
	{
		return !STREAMING::IS_MODEL_A_VEHICLE(model);
	}

	static bool IsBadObjectModel(std::uint32_t model)
	{
		return g_CrashObjects.contains(model) || STREAMING::IS_MODEL_A_PED(model) || STREAMING::IS_MODEL_A_VEHICLE(model);
	}

	// positions that are outside of the world's sector grid crash the game
	static bool IsInvalidOverridePos(float x, float y)
	{
		// same as legacy (which converted to an unsigned sector index), but without converting negative floats to unsigned
		float x_pos = ((x + 149) + 8192) / 75;
		float y_pos = ((y + 149) + 8192) / 75;
		return !(x_pos >= 0.0f && x_pos < UCHAR_MAX && y_pos >= 0.0f && y_pos < UCHAR_MAX);
	}

	// the game only applies (and checks) nodes that are present in this update
	static bool HasData(rage::netSyncNodeBase* node)
	{
		return *reinterpret_cast<std::uint8_t*>(reinterpret_cast<std::uintptr_t>(node) + 0x40) != 0;
	}

	static rage::netSyncNodeBase* FindNodeWithData(rage::netSyncTree* tree, joaat_t id)
	{
		for (std::uint32_t i = 0; i < std::min<std::uint32_t>(tree->m_ChildNodeCount, std::size(tree->m_ChildNodes)); i++)
		{
			auto node = tree->m_ChildNodes[i];
			if (node && HasData(node))
				if (auto node_id = Nodes::TryFind(reinterpret_cast<std::uintptr_t>(node)); node_id && node_id->id == id)
					return node;
		}

		return nullptr;
	}

	// returns the reason if the node's data must not be applied, or nullptr
	// from legacy: if the entity the object gets attached to is (through its own attachments) attached to the object,
	// the attachments form a loop, which crashes the game
	static bool IsAttachmentLoop(rage::netObject* object, std::uint16_t attached_to)
	{
		auto target = Pointers.GetNetObjectById(attached_to);
		for (int i = 0; target && i < 32; i++)
		{
			if (target == object)
				return true;
			if (!target->m_GameObject)
				return false;

			auto handle = Pointers.PtrToHandle(target->m_GameObject);
			if (!handle)
				return false;
			auto next = ENTITY::GET_ENTITY_ATTACHED_TO(handle);
			if (!next)
				return false;
			target = YimMenu::Entity(static_cast<int>(next)).GetNetworkObject();
		}
		return false;
	}

	// legacy compares the vehicle type of the model with the object type (a plane model created as a car crashes). V2 can't read
	// model infos, so use the model natives and only judge models with one unambiguous class (blimps, autogyros, trailers and
	// submarines aren't covered by these natives and are skipped)
	static bool IsVehicleModelMismatch(std::uint32_t model, rage::netObject::Type type)
	{
		using Type = rage::netObject::Type;
		std::optional<Type> expected;
		auto add = [&](bool matches, Type object_type) {
			if (!matches)
				return true;
			if (expected && *expected != object_type)
				return false; // more than one class, don't judge
			expected = object_type;
			return true;
		};

		if (!add(VEHICLE::IS_THIS_MODEL_A_PLANE(model), Type::PLANE)
		    || !add(VEHICLE::IS_THIS_MODEL_A_HELI(model), Type::HELI)
		    || !add(VEHICLE::IS_THIS_MODEL_A_BOAT(model) || VEHICLE::IS_THIS_MODEL_A_JETSKI(model), Type::BOAT)
		    || !add(VEHICLE::IS_THIS_MODEL_A_TRAIN(model), Type::TRAIN)
		    || !add(VEHICLE::IS_THIS_MODEL_A_BIKE(model) || VEHICLE::IS_THIS_MODEL_A_BICYCLE(model), Type::BIKE)
		    || !add(VEHICLE::IS_THIS_MODEL_A_CAR(model) || VEHICLE::IS_THIS_MODEL_A_QUADBIKE(model) || VEHICLE::IS_THIS_MODEL_AN_AMPHIBIOUS_CAR(model) || VEHICLE::IS_THIS_MODEL_AN_AMPHIBIOUS_QUADBIKE(model), Type::AUTOMOBILE))
			return false;

		return expected && *expected != type;
	}

	static bool IsVehicleObjectType(rage::netObject::Type type)
	{
		using Type = rage::netObject::Type;
		return type == Type::AUTOMOBILE || type == Type::BIKE || type == Type::BOAT || type == Type::HELI || type == Type::PLANE
		    || type == Type::SUBMARINE || type == Type::TRAILER || type == Type::TRAIN;
	}

	// the vehicle's model: of the existing vehicle, or from the creation node in this update (the object isn't created yet)
	static std::optional<std::uint32_t> GetVehicleModel(rage::netSyncTree* tree, rage::netObject* object)
	{
		if (object->m_GameObject)
			if (auto handle = Pointers.PtrToHandle(object->m_GameObject))
				return ENTITY::GET_ENTITY_MODEL(handle);
		if (auto creation = FindNodeWithData(tree, "CVehicleCreationDataNode"_J))
			return reinterpret_cast<CVehicleCreationDataNode*>(creation)->m_ModelHash;
		return std::nullopt;
	}

	// legacy's task type numbers, used by its "invalid ped/vehicle task" crash checks. They match enhanced: players on foot
	// use 264 (CTaskMotionPed), birds 273, helicopters 456/473, planes 455
	namespace LegacyTask
	{
		// clang-format off
		enum : std::uint32_t
		{
			AimGunOnFoot = 4, AimGun = 12, Unalerted = 120, MotionPed = 264, AimAndThrowProjectile = 289, Gun = 290, AimGunVehicleDriveBy = 295, Jump = 422,
			VehicleGoToPlane = 455, VehicleGoToHelicopter = 456, VehicleGoToSubmarine = 457, VehicleGoToBoat = 458, VehicleGoToPointWithAvoidanceAutomobile = 460,
			VehicleCruiseBoat = 468, VehicleFleeAirborne = 473, VehicleFleeBoat = 474, VehicleLandPlane = 483, VehiclePoliceBehaviourHelicopter = 489,
			VehiclePoliceBehaviourBoat = 490, VehicleHeliProtect = 492, VehiclePlayerDriveBoat = 496, VehiclePlayerDriveSubmarine = 497,
			VehiclePlayerDrivePlane = 500, VehiclePlayerDriveHeli = 501, VehiclePlaneChase = 505,
		};
		// clang-format on
	}

	static bool IsAnimal(rage::netObject* object)
	{
		if (!object->m_GameObject)
			return false;
		auto handle = Pointers.PtrToHandle(object->m_GameObject);
		return handle && PED::GET_PED_TYPE(handle) == 28; // PED_TYPE_ANIMAL
	}

	// legacy's is_crash_ped_task: aiming, shooting and jumping tasks on animals, the unalerted task on a player
	static const char* LegacyCrashPedTask(std::uint32_t task, rage::netObject* object)
	{
		using namespace LegacyTask;
		switch (task)
		{
		case Unalerted: return object->m_ObjectType == rage::netObject::Type::PLAYER ? "unalerted task on a player" : nullptr;
		case Jump:
		case Gun:
		case AimGunOnFoot:
		case AimAndThrowProjectile:
		case AimGun:
		case AimGunVehicleDriveBy:
			if (!IsAnimal(object))
				return nullptr;
			// normal players can be animals (peyote plants) and still send this
			return object->m_ObjectType == rage::netObject::Type::PLAYER ? "aiming or jumping task on a player in animal form" : "aiming or jumping task on an animal";
		}
		return nullptr;
	}

	// legacy's is_crash_vehicle_task: a vehicle task for another kind of vehicle
	static const char* LegacyCrashVehicleTask(std::uint32_t task, rage::netObject::Type type)
	{
		using namespace LegacyTask;
		using Type = rage::netObject::Type;
		switch (task)
		{
		case VehicleGoToPlane:
		case VehicleLandPlane:
		case VehiclePlayerDrivePlane:
		case VehiclePlaneChase: return type != Type::PLANE ? "plane task on another vehicle" : nullptr;
		case VehicleGoToHelicopter:
		case VehiclePoliceBehaviourHelicopter:
		case VehiclePlayerDriveHeli:
		case VehicleHeliProtect: return type != Type::HELI ? "helicopter task on another vehicle" : nullptr;
		case VehicleGoToBoat:
		case VehicleCruiseBoat:
		case VehicleFleeBoat:
		case VehiclePoliceBehaviourBoat:
		case VehiclePlayerDriveBoat: return type != Type::BOAT ? "boat task on another vehicle" : nullptr;
		case VehicleGoToSubmarine:
		case VehiclePlayerDriveSubmarine: return type != Type::SUBMARINE ? "submarine task on another vehicle" : nullptr;
		case VehicleFleeAirborne: return type != Type::HELI && type != Type::PLANE ? "airborne task on another vehicle" : nullptr;
		case VehicleGoToPointWithAvoidanceAutomobile: return type != Type::AUTOMOBILE ? "car task on another vehicle" : nullptr;
		}
		return nullptr;
	}

	// checks that a normal player can hit: they still block (with Block Bad Sync Data), but add no detection points
	static bool IsUnverifiedCheck(std::string_view reason)
	{
		return reason.ends_with("on a player in animal form");
	}

	// legacy's spectate, max health and spoofed crew infractions: each once per sender and key per game run, at most 300 in all
	static bool FirstTime(Player sender, std::string key)
	{
		static std::mutex mutex;
		static std::set<std::pair<std::uint64_t, std::string>> seen;
		std::lock_guard lock(mutex);
		return seen.size() < 300 && seen.emplace(sender.GetRID(), std::move(key)).second;
	}

	static void ObservePlayerGameState(CPlayerGameStateDataNode* game_state, Player sender)
	{
		// legacy's super jump infraction: freemode never calls the super jump native, so the flag comes from a menu. Strong, not
		// certain: it isn't an attack, so it marks without the attack reactions
		if (game_state->m_SuperJump)
			Detections::Add(sender, DetectionLevel::Strong, "using super jump");

		// legacy counts this as super jump too
		if (!game_state->m_IsMaxArmorAndHealthDefault && game_state->m_MaxHealth == 0 && game_state->m_PlayerState == 1 && FirstTime(sender, "max health 0"))
			LOGF(WARNING, "Max health 0 in player state 1 from {} (legacy's super jump infraction, log only)", sender.GetName());

		// legacy's spectate reaction: a living player watching another player (dead players watch their killer). Weak, so
		// watching three different players marks them
		if (game_state->m_IsSpectating)
		{
			auto ped = sender.GetPed();
			if (!ped || ped.GetHealth() <= 0)
				return;

			auto object = Pointers.GetNetObjectById(game_state->m_SpectatingNetId);
			if (!object || object->m_ObjectType != rage::netObject::Type::PLAYER)
				return;

			// a target that is the sender's own ped isn't spectating
			Player target(static_cast<std::uint8_t>(object->m_OwnerId));
			if (!target.IsValid() || target.GetId() == sender.GetId())
				return;

			auto target_name = target.IsLocal() ? std::string("us") : std::string(target.GetName());
			if (FirstTime(sender, "spectate " + target_name))
			{
				LOGF(WARNING, "{} is spectating {} while alive (health {}; legacy's spectate reaction)", sender.GetName(), target_name, ped.GetHealth());
				Detections::Add(sender, DetectionLevel::Weak, "spectating players while alive");
			}
		}
	}

	static std::string Printable(const char* text, std::size_t size)
	{
		std::string result(text, strnlen(text, size));
		for (auto& c : result)
			if (c < 0x20 || c > 0x7E)
				c = '?';
		return result;
	}

	static void ObserveCrewData(CPlayerGamerDataNode* gamer, Player sender)
	{
		auto& clan = gamer->m_CanData;
		if (!clan.m_clan_id && !clan.m_clan_id_2)
			return;

		// legacy's spoofed crew rules: the menu default id, tags of 4+ letters or digits (3+ for Rockstar's system crews), and a
		// closed or empty system crew. Only logged
		auto tag = Printable(clan.m_clan_tag, sizeof(clan.m_clan_tag));
		const char* flag = nullptr;
		if (clan.m_clan_id == 123456 && clan.m_clan_id_2 == 123456)
			flag = "crew id 123456";
		else if (clan.m_clan_id > 0 && clan.m_clan_id_2 > 0)
		{
			auto alnum = std::all_of(tag.begin(), tag.end(), [](char c) {
				return std::isalnum(static_cast<unsigned char>(c));
			});
			if (!alnum || tag.size() <= (clan.m_is_system_clan ? 2u : 3u))
				flag = "invalid crew tag";
			else if (clan.m_is_system_clan && (!clan.m_is_clan_open || clan.m_clan_member_count == 0))
				flag = "closed or empty system crew";
		}

		if (!flag || !FirstTime(sender, std::format("crew {}", clan.m_clan_id)))
			return;

		LOGF(WARNING, "Crew data from {} looks spoofed: {} (ids {} / {}, tag '{}', name '{}', {} members, system {}, open {}, rank '{}'; legacy's rule, log only)", sender.GetName(), flag, clan.m_clan_id, clan.m_clan_id_2, tag, Printable(clan.m_clan_name, sizeof(clan.m_clan_name)), clan.m_clan_member_count, clan.m_is_system_clan, clan.m_is_clan_open, Printable(clan.m_clan_rank_name, sizeof(clan.m_clan_rank_name)));
	}

	static const char* CheckNode(joaat_t node_id, rage::netSyncNodeBase* node, rage::netSyncTree* tree, rage::netObject* object, Player sender, bool block)
	{
		switch (node_id)
		{
		case "CPlayerCreationDataNode"_J:
		{
			auto creation = reinterpret_cast<CPlayerCreationDataNode*>(node);
			NetRecorder::Record(sender, NetRecorder::Kind::SyncNode, node_id, creation->m_ModelHash);
			if (IsBadPedModel(creation->m_ModelHash))
				return "player model is not a ped (creation)";
			break;
		}
		case "CPlayerAppearanceDataNode"_J:
		{
			auto appearance = reinterpret_cast<CPlayerAppearanceDataNode*>(node);
			NetRecorder::Record(sender, NetRecorder::Kind::SyncNode, node_id, appearance->m_model_hash);
			if (IsBadPedModel(appearance->m_model_hash))
				return "player model is not a ped (appearance)";

			// legacy: there is a crash with the anim dict index here that is hard to detect, and phone gestures are unused
			if (block)
				appearance->m_mobile_phone_gesture_active = false;
			break;
		}
		case "CPedCreationDataNode"_J:
		{
			auto creation = reinterpret_cast<CPedCreationDataNode*>(node);
			NetRecorder::Record(sender, NetRecorder::Kind::SyncNode, node_id, creation->m_ModelHash);
			if (IsBadPedModel(creation->m_ModelHash))
				return "invalid ped model";
			if (creation->m_HasProp && IsBadObjectModel(creation->m_PropModel))
				return "invalid ped prop model";
			break;
		}
		case "CObjectCreationDataNode"_J:
		{
			auto creation = reinterpret_cast<CObjectCreationDataNode*>(node);
			NetRecorder::Record(sender, NetRecorder::Kind::SyncNode, node_id, creation->m_Model);
			if (IsBadObjectModel(creation->m_Model))
				return "invalid object model";
			if (g_CageObjects.contains(creation->m_Model) && !NETWORK::NETWORK_IS_ACTIVITY_SESSION())
				return "cage object";
			break;
		}
		case "CDoorCreationDataNode"_J:
		{
			auto creation = reinterpret_cast<CDoorCreationDataNode*>(node);
			NetRecorder::Record(sender, NetRecorder::Kind::SyncNode, node_id, creation->m_ModelHash);
			if (IsBadObjectModel(creation->m_ModelHash))
				return "invalid door model";
			break;
		}
		case "CPickupCreationDataNode"_J:
		{
			auto creation = reinterpret_cast<CPickupCreationDataNode*>(node);
			// type and amount show money drops (cash pickups around a player) in the netlogs
			NetRecorder::Record(sender, NetRecorder::Kind::Pickup, creation->m_PickupHash, creation->m_Amount);
			if (creation->m_CustomModel)
				NetRecorder::Record(sender, NetRecorder::Kind::SyncNode, node_id, creation->m_CustomModel);
			if (creation->m_CustomModel && IsBadObjectModel(creation->m_CustomModel))
				return "invalid pickup model";
			if (creation->m_NumWeaponComponents > std::size(creation->m_WeaponComponent))
				return "out of bounds pickup weapon component count";
			// legacy: a component the game has no HUD stats for doesn't exist
			for (std::uint32_t i = 0; i < creation->m_NumWeaponComponents; i++)
			{
				std::uint64_t stats[20]{};
				if (!WEAPON::GET_WEAPON_COMPONENT_HUD_STATS(creation->m_WeaponComponent[i], stats))
					return "invalid pickup weapon component";
			}
			break;
		}
		case "CVehicleCreationDataNode"_J:
		{
			auto creation = reinterpret_cast<CVehicleCreationDataNode*>(node);
			NetRecorder::Record(sender, NetRecorder::Kind::SyncNode, node_id, creation->m_ModelHash);
			if (IsBadVehicleModel(creation->m_ModelHash))
				return "invalid vehicle model";
			if (IsVehicleModelMismatch(creation->m_ModelHash, object->m_ObjectType))
				return "vehicle model doesn't match the object type";
			break;
		}
		case "CPhysicalAttachDataNode"_J:
		{
			auto attach = reinterpret_cast<CPhysicalAttachDataNode*>(node);
			if (attach->m_IsAttached && attach->m_Attachedto == static_cast<std::uint16_t>(object->m_ObjectId))
				return "entity attached to itself";
			if (attach->m_IsAttached && IsAttachmentLoop(object, attach->m_Attachedto))
				return "entity attachment loop";
			// legacy: a trailer attached to something other than a vehicle crashes
			if (attach->m_IsAttached && object->m_ObjectType == rage::netObject::Type::TRAILER)
				if (auto target = Pointers.GetNetObjectById(attach->m_Attachedto); target && !IsVehicleObjectType(target->m_ObjectType))
					return "trailer attached to something that isn't a vehicle";
			break;
		}
		case "CVehicleControlDataNode"_J:
		{
			// the submarine car flag makes the game write the rudder rotation to the vehicle's submarine part, which only submarine cars
			// have, so it crashes on any other vehicle (see ApplyVehicleControlData). An existing vehicle is asked for that part, as the
			// game does; a new one is checked by model (legacy: the Stromberg and Toreador are the submarine cars)
			auto control = reinterpret_cast<CVehicleControlDataNode*>(node);
			if (control->m_IsSubmarineCar)
			{
				if (auto vehicle = reinterpret_cast<void*>(object->m_GameObject))
				{
					using Getter = void* (*)(void* vehicle);
					if (!reinterpret_cast<Getter>((*reinterpret_cast<void***>(vehicle))[0x640 / 8])(vehicle))
						return "submarine car data on a vehicle that isn't one";
				}
				else if (auto model = GetVehicleModel(tree, object))
				{
					if (*model != "stromberg"_J && *model != "toreador"_J)
						return "submarine car data on a vehicle that isn't one";
				}
				else
					control->m_IsSubmarineCar = false; // no model to check against: drop the flag (legacy)
			}
			break;
		}
		case "CPlayerGamerDataNode"_J:
		{
			ObserveCrewData(reinterpret_cast<CPlayerGamerDataNode*>(node), sender);
			break;
		}
		case "CDynamicEntityGameStateDataNode"_J:
		{
			// legacy: the game checks the interior index (its low 16 bits) with a signed compare, so a negative one other than -1 (no
			// interior) passes and crashes; no real interior index is negative
			auto game_state = reinterpret_cast<CDynamicEntityGameStateDataNode*>(node);
			auto index = static_cast<std::int16_t>(game_state->m_InteriorIndex & 0xFFFF);
			if (index < -1)
				return "invalid interior index";
			break;
		}
		case "CPedAttachDataNode"_J:
		{
			auto attach = reinterpret_cast<CPedAttachDataNode*>(node);
			if (attach->m_IsAttached && attach->m_AttachedTo == static_cast<std::uint16_t>(object->m_ObjectId))
				return "ped attached to itself";
			if (attach->m_IsAttached && IsAttachmentLoop(object, attach->m_AttachedTo))
				return "ped attachment loop";
			break;
		}
		case "CTrainGameStateDataNode"_J:
		{
			auto train = reinterpret_cast<CTrainGameStateDataNode*>(node);
			if (train->m_TrackId != -1 && (train->m_TrackId < 0 || train->m_TrackId >= 12))
				return "out of bounds train track index";
			// from legacy; the game indexes its train config array with these without checks (CTrain +0x1570 / +0x1571)
			if (train->m_TrainConfigIndex != -1 && Pointers.TrainConfigs)
			{
				auto configs = *reinterpret_cast<std::uint8_t**>(Pointers.TrainConfigs);
				auto count = *reinterpret_cast<std::uint16_t*>(reinterpret_cast<std::uint8_t*>(Pointers.TrainConfigs) + 8);
				if (train->m_TrainConfigIndex < 0 || train->m_TrainConfigIndex >= count)
					return "out of bounds train config index";
				auto carriages = *reinterpret_cast<std::uint16_t*>(configs + train->m_TrainConfigIndex * 0x30 + 0x28);
				if (train->m_CarriageConfigIndex != -1 && (train->m_CarriageConfigIndex < 0 || train->m_CarriageConfigIndex >= carriages))
					return "out of bounds carriage config index";
			}
			break;
		}
		case "CVehicleGadgetDataNode"_J:
		{
			auto gadgets = reinterpret_cast<CVehicleGadgetDataNode*>(node);
			if (gadgets->m_GadgetCount > std::size(gadgets->m_GadgetData))
				return "out of bounds vehicle gadget count";
			for (std::uint32_t i = 0; i < gadgets->m_GadgetCount; i++)
				if (gadgets->m_GadgetData[i].m_Type > 7)
					return "out of bounds vehicle gadget type";
			break;
		}
		case "CPedGameStateDataNode"_J:
		{
			auto game_state = reinterpret_cast<CPedGameStateDataNode*>(node);
			// legacy's WEAPON_STRICKLER check is not ported: it is a normal weapon on enhanced
			if (game_state->m_OnMount)
				return "mount flag";
			if (game_state->m_NumEquippedGadgets > std::size(game_state->m_GadgetHash))
				return "out of bounds gadget count";
			for (std::uint32_t i = 0; i < game_state->m_NumEquippedGadgets; i++)
				if (game_state->m_GadgetHash[i] != "gadget_parachute"_J && game_state->m_GadgetHash[i] != "gadget_nightvision"_J)
					return "invalid gadget";
			break;
		}
		case "CEntityScriptInfoDataNode"_J:
		{
			auto script_info = reinterpret_cast<CEntityScriptInfoDataNode*>(node);
			if (script_info->m_HasScriptInfo && script_info->m_ScriptInfo.m_NetworkId == 0)
				return "invalid script info";
			break;
		}
		case "CSectorDataNode"_J:
		{
			if (object->m_ObjectType != rage::netObject::Type::PLAYER)
				break;

			float sector_pos_x = 0.0f, sector_pos_y = 0.0f;
			if (auto sector_pos = FindNodeWithData(tree, "CPlayerSectorPosNode"_J))
			{
				sector_pos_x = reinterpret_cast<CPlayerSectorPosNode*>(sector_pos)->m_SectorPosX;
				sector_pos_y = reinterpret_cast<CPlayerSectorPosNode*>(sector_pos)->m_SectorPosY;
			}

			auto sector = reinterpret_cast<CSectorDataNode*>(node);
			int pos_x = (sector->m_SectorX - 512.0f) * 54.0f;
			int pos_y = (sector->m_SectorY - 512.0f) * 54.0f;
			if (IsInvalidOverridePos(pos_x + sector_pos_x, pos_y + sector_pos_y))
				return "invalid sector position";
			break;
		}
		case "CPlayerGameStateDataNode"_J:
		{
			auto game_state = reinterpret_cast<CPlayerGameStateDataNode*>(node);
			ObservePlayerGameState(game_state, sender);
			if (game_state->m_IsOverridingPopulationControlSphere && IsInvalidOverridePos(game_state->m_PopulationControlSphere.x, game_state->m_PopulationControlSphere.y))
				return "invalid population control sphere position";
			break;
		}
		case "CPlayerCameraDataNode"_J:
		{
			auto camera = reinterpret_cast<CPlayerCameraDataNode*>(node);
			if (IsInvalidOverridePos(camera->m_FreeCamPos.x, camera->m_FreeCamPos.y))
				return "invalid camera position";
			break;
		}
		case "CPedTaskTreeDataNode"_J:
		{
			auto tasks = reinterpret_cast<CPedTaskTreeDataNode*>(node);
			for (int i = 0; i < 8; i++)
				if (tasks->m_TaskBitset & (1 << i))
					if (auto reason = LegacyCrashPedTask(static_cast<std::uint32_t>(tasks->m_Tasks[i].m_TaskType), object))
						return reason;
			break;
		}
		case "CPedMovementGroupDataNode"_J:
		{
			// legacy: the on-foot motion task in its aiming stage on an animal
			auto movement = reinterpret_cast<CPedMovementGroupDataNode*>(node);
			if (movement->m_MovementTaskInded == LegacyTask::MotionPed && movement->m_MovementTaskStage == 13 && IsAnimal(object))
				return object->m_ObjectType == rage::netObject::Type::PLAYER ? "aiming movement on a player in animal form" : "aiming movement on an animal";
			break;
		}
		case "CVehicleTaskDataNode"_J:
		{
			if (auto reason = LegacyCrashVehicleTask(reinterpret_cast<CVehicleTaskDataNode*>(node)->m_TaskType, object->m_ObjectType))
				return reason;
			break;
		}
		case "CVehicleScriptGameStateDataNode"_J:
		{
			// legacy: +0x14A "has a parachute", +0x148 the parachute's net id, whose model must be one of the three vehicle
			// parachutes. The parachute object can arrive a moment later: no object, no check
			auto data = reinterpret_cast<std::uint8_t*>(node);
			if (!data[0x14A])
				break;
			auto parachute = Pointers.GetNetObjectById(*reinterpret_cast<std::uint16_t*>(data + 0x148));
			if (!parachute || !parachute->m_GameObject)
				break;
			auto handle = Pointers.PtrToHandle(parachute->m_GameObject);
			if (!handle)
				break;
			auto model = ENTITY::GET_ENTITY_MODEL(handle);
			if (model != "imp_prop_impexp_para_s"_J && model != "sr_prop_specraces_para_s_01"_J && model != "gr_prop_gr_para_s_01"_J)
				return "invalid vehicle parachute";
			break;
		}
		case "CVehicleProximityMigrationDataNode"_J:
		{
			if (!Self::GetPed().GetPointer<void*>())
				break;

			// being put into a vehicle by someone else is a remote teleport, unless we already are in that vehicle
			auto vehicle = Self::GetVehicle();
			if (vehicle && vehicle.GetNetworkObjectId() == static_cast<std::uint16_t>(object->m_ObjectId))
				break;

			auto ped_id = static_cast<std::int16_t>(Self::GetPed().GetNetworkObjectId());
			auto migration = reinterpret_cast<CVehicleProximityMigrationDataNode*>(node);
			for (std::uint32_t i = 0; i < std::min<std::uint32_t>(migration->m_MaxOccupants, std::size(migration->m_Occupants)); i++)
				if (migration->m_HasOccupants[i] && migration->m_Occupants[i] == ped_id)
					return "remote teleport into a vehicle";
			break;
		}
		}

		return nullptr;
	}

	static bool InitNodes()
	{
		static bool failed = false;
		if (failed)
			return false;

		if (Nodes::IsInitialized())
			return true;

		try
		{
			Nodes::Init();
		}
		catch (const std::exception& e)
		{
			failed = true;
			LOG(WARNING) << "Sync node identification failed, the sync data checks are disabled: " << e.what();
		}

		return !failed && Nodes::IsInitialized();
	}

	bool SyncProtections::ShouldReject(rage::netSyncTree* tree, rage::netObject* object)
	{
		Player sender = t_Sender;
		if (!sender || !tree || !object || !InitNodes())
			return false;

		bool block = _BlockBadSyncData.GetState();

		for (std::uint32_t i = 0; i < std::min<std::uint32_t>(tree->m_ChildNodeCount, std::size(tree->m_ChildNodes)); i++)
		{
			auto node = tree->m_ChildNodes[i];
			if (!node || !HasData(node))
				continue;

			auto node_id = Nodes::TryFind(reinterpret_cast<std::uintptr_t>(node));
			if (!node_id)
				continue;

			auto reason = CheckNode(node_id->id, node, tree, object, sender, block);
			if (!reason)
				continue;

			// crashes verified on enhanced are always blocked: a player model that isn't a ped, the mount flag (the ped's mount link
			// always reads through a null pointer), crash object models and the submarine car flag on another vehicle
			std::string_view why = reason;
			bool crash = why == "mount flag" || why == "invalid object model" || why.starts_with("player model is not a ped")
			    || why == "submarine car data on a vehicle that isn't one";
			// cages have their own toggle
			bool cage = why == "cage object";
			bool reject = cage ? _BlockCages.GetState() : block || crash;

			NetRecorder::Record(sender, NetRecorder::Kind::Blocked, static_cast<std::uint16_t>(object->m_ObjectId), static_cast<std::uint32_t>(reject ? NetRecorder::BlockReason::BadSyncData : NetRecorder::BlockReason::BadSyncDataNotBlocked));

			// a player's ped forms an attachment loop with its parachute, which normal players hit: rejected silently, like legacy
			if (why == "ped attachment loop" && object->m_ObjectType == rage::netObject::Type::PLAYER)
			{
				if (reject)
					return true;
				continue;
			}

			// an attacker can send this many times per second, so log each sender at most once per 10 seconds per reason
			static std::array<std::pair<const char*, std::chrono::steady_clock::time_point>, 256> last_logs{};
			auto& last_log = last_logs[static_cast<std::uint8_t>(sender.GetId())];
			auto now = std::chrono::steady_clock::now();
			if (last_log.first != reason || now - last_log.second > std::chrono::seconds(10))
			{
				last_log = {reason, now};
				const char* action = reject ? "Blocked" : (cage ? "Would block (Block Cages is off)" : "Would block (Block Bad Sync Data is off)");
				LOGF(WARNING, "{} {} from {} in {} (object {}, type {})", action, reason, sender.GetName(), node_id->name, object->m_ObjectId, static_cast<int>(object->m_ObjectType));
			}

			if (reject)
			{
				NetRecorder::DumpBlocked("bad sync data", sender, why, crash);

				// the other checks only add weak points, none for those a normal player can hit
				if (crash)
					Detections::Add(sender, DetectionLevel::Certain, why);
				else if (cage)
					Detections::Add(sender, DetectionLevel::Strong, "spawning cage objects");
				else if (!IsUnverifiedCheck(why))
					Detections::Add(sender, DetectionLevel::Weak, std::format("bad sync data: {}", why));
				return true;
			}
		}

		return false;
	}
}
