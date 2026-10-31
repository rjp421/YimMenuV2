#include "core/hooking/DetourHook.hpp"
#include "game/backend/Detections.hpp"
#include "game/backend/NetRecorder.hpp"
#include "game/backend/Players.hpp"
#include "game/backend/Self.hpp"
#include "game/backend/SyncProtections.hpp"
#include "game/hooks/Hooks.hpp"
#include "game/pointers/Pointers.hpp"
#include "types/network/netObject.hpp"

namespace YimMenu::Hooks
{
	using BlockReason = NetRecorder::BlockReason;

	static constexpr std::uint16_t MAX_OBJECT_TYPE = static_cast<std::uint16_t>(rage::netObject::Type::MAX);

	static const char* GetSenderName(Player player)
	{
		return player ? player.GetName() : "unknown";
	}

	// nobody else can legitimately create, sync or delete our own ped, or a vehicle that we control
	static bool TargetsOurEntity(std::uint16_t object_id, BlockReason& reason)
	{
		if (Self::GetPed().GetPointer<void*>() && Self::GetPed().GetNetworkObjectId() == object_id)
		{
			reason = BlockReason::OwnPed;
			return true;
		}

		if (Self::GetVehicle() && Self::GetVehicle().HasControl() && Self::GetVehicle().GetNetworkObjectId() == object_id)
		{
			reason = BlockReason::OwnVehicle;
			return true;
		}

		return false;
	}

	// the player object of a player can only be synced or removed by that player
	static bool IsForeignPlayerObject(Player sender, std::uint16_t object_id)
	{
		auto object = Pointers.GetNetObjectById(object_id);
		return object && object->m_ObjectType == rage::netObject::Type::PLAYER && sender && object->m_OwnerId != sender.GetId();
	}

	// players never delete their own player object while in the session (the game cleans it up when they leave);
	// attackers do, to re-create it with crafted ped data
	static bool IsPlayerObject(std::uint16_t object_id)
	{
		auto object = Pointers.GetNetObjectById(object_id);
		return object && object->m_ObjectType == rage::netObject::Type::PLAYER;
	}

	static void AddDetection(std::string_view what, Player sender, BlockReason reason)
	{
		switch (reason)
		{
		case BlockReason::BadObjectType: Detections::Add(sender, DetectionLevel::Certain, "invalid object type"); break;
		case BlockReason::PlayerObjectRemoval: Detections::Add(sender, DetectionLevel::Certain, "removal of a player object"); break;
		case BlockReason::OwnPed:
			// same reason text as the deletion check in ReceiveNetMessage, so one attack counts once
			if (what == "remove")
				Detections::Add(sender, DetectionLevel::Certain, "deleting our player");
			else
				Detections::Add(sender, DetectionLevel::Strong, "taking over our player");
			break;
		case BlockReason::PlayerObjectNotOwnedBySender: Detections::Add(sender, DetectionLevel::Strong, "syncing another player's player object"); break;
		// can happen in normal play when an object id is reused or a vehicle migrates while the message is on the way
		case BlockReason::ObjectTypeMismatch: Detections::Add(sender, DetectionLevel::Weak, "object type mismatch"); break;
		case BlockReason::OwnVehicle: Detections::Add(sender, DetectionLevel::Weak, "taking over our vehicle"); break;
		default: break;
		}
	}

	static void LogBlocked(const char* what, Player sender, std::uint16_t object_id, BlockReason reason)
	{
		NetRecorder::Record(sender, NetRecorder::Kind::Blocked, object_id, static_cast<std::uint32_t>(reason));
		LOGF(WARNING, "Blocked clone {} for object {} from {} (reason {})", what, object_id, GetSenderName(sender), static_cast<std::uint32_t>(reason));
		// the weak ones (see AddDetection)
		bool serious = reason != BlockReason::ObjectTypeMismatch && reason != BlockReason::OwnVehicle;
		NetRecorder::DumpBlocked("blocked clone message", sender, std::format("clone {} reason {}", what, static_cast<std::uint32_t>(reason)), serious);
		AddDetection(what, sender, reason);
	}

	void Network::HandleCloneCreate(CNetworkObjectMgr* mgr, CNetGamePlayer* source, CNetGamePlayer* target, std::uint16_t object_type, std::uint16_t object_id, std::uint16_t object_flags, rage::datBitBuffer* buffer, std::uint32_t timestamp)
	{
		Player sender = source;
		NetRecorder::Record(sender, NetRecorder::Kind::CloneCreate, object_id, object_type);

		if (object_type > MAX_OBJECT_TYPE)
		{
			LogBlocked("create", sender, object_id, BlockReason::BadObjectType);
			return;
		}

		if (BlockReason reason; TargetsOurEntity(object_id, reason))
		{
			LogBlocked("create", sender, object_id, reason);
			return;
		}

		SyncProtections::SenderScope scope(sender);
		BaseHook::Get<Network::HandleCloneCreate, DetourHook<decltype(&Network::HandleCloneCreate)>>()->Original()(mgr, source, target, object_type, object_id, object_flags, buffer, timestamp);
	}

	int Network::HandleCloneSync(CNetworkObjectMgr* mgr, CNetGamePlayer* source, CNetGamePlayer* target, std::uint16_t object_type, std::uint16_t object_id, rage::datBitBuffer* buffer, std::uint16_t sequence, std::uint32_t timestamp)
	{
		constexpr int ACK_FAIL = 1; // also makes the game skip the rest of this packet

		Player sender = source;

		if (object_type > MAX_OBJECT_TYPE)
		{
			LogBlocked("sync", sender, object_id, BlockReason::BadObjectType);
			return ACK_FAIL;
		}

		if (auto object = Pointers.GetNetObjectById(object_id); object && static_cast<std::uint16_t>(object->m_ObjectType) != object_type)
		{
			LogBlocked("sync", sender, object_id, BlockReason::ObjectTypeMismatch);
			return ACK_FAIL;
		}

		if (BlockReason reason; TargetsOurEntity(object_id, reason))
		{
			LogBlocked("sync", sender, object_id, reason);
			return ACK_FAIL;
		}

		if (IsForeignPlayerObject(sender, object_id))
		{
			LogBlocked("sync", sender, object_id, BlockReason::PlayerObjectNotOwnedBySender);
			return ACK_FAIL;
		}

		SyncProtections::SenderScope scope(sender);
		return BaseHook::Get<Network::HandleCloneSync, DetourHook<decltype(&Network::HandleCloneSync)>>()->Original()(mgr, source, target, object_type, object_id, buffer, sequence, timestamp);
	}

	void Network::HandleCloneRemove(CNetworkObjectMgr* mgr, CNetGamePlayer* source, CNetGamePlayer* target, std::uint16_t object_id, int ownership_token)
	{
		Player sender = source;

		if (BlockReason reason; TargetsOurEntity(object_id, reason))
		{
			LogBlocked("remove", sender, object_id, reason);
			return;
		}

		if (IsPlayerObject(object_id))
		{
			LogBlocked("remove", sender, object_id, BlockReason::PlayerObjectRemoval);
			return;
		}

		BaseHook::Get<Network::HandleCloneRemove, DetourHook<decltype(&Network::HandleCloneRemove)>>()->Original()(mgr, source, target, object_id, ownership_token);
	}
}