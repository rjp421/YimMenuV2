#include "core/frontend/Notifications.hpp"
#include "core/hooking/DetourHook.hpp"
#include "game/backend/BlockJoinReasons.hpp"
#include "game/backend/SavedPlayers.hpp"
#include "game/hooks/Hooks.hpp"
#include "game/pointers/Pointers.hpp"
#include "types/network/rlGamerInfo.hpp"
#include "types/rage/datBitBuffer.hpp"

namespace YimMenu::Hooks
{
	// writes a join response that refuses them with this status, like legacy's write_join_response_data (the game writes
	// it inline in the handler): a version hash, the status, then the remaining fields left 0 as legacy does
	static bool WriteRefusal(std::uint8_t* ctx, int reason)
	{
		if (!Pointers.GetJoinResponseVersion)
			return false;

		rage::datBitBuffer buffer(ctx + 0x34, 0x200);
		buffer.Write<int>(0x3246, 14); // message header (as Packet::WriteMessageHeader), join response = id 8
		buffer.Write<bool>(false, 1);
		buffer.Write<int>(8, 8);
		buffer.Write<std::uint32_t>(Pointers.GetJoinResponseVersion(), 32);
		buffer.Write<int>(reason, 6);
		for (int bits : {32, 1, 4, 64, 8, 8, 1, 3, 32, 32, 3, 2})
			buffer.Write<std::uint64_t>(0, bits);
		*reinterpret_cast<std::uint32_t*>(ctx + 0x234) = (buffer.m_CurBit + 7) >> 3;
		return true;
	}

	// a blocked player gets a refusing response with their reason, or an empty one if GetJoinResponseVersion wasn't found
	bool Network::HandleJoinRequest(void* network, void* session, rage::rlGamerInfo* gamer_info, std::uint8_t* ctx, BOOL is_transition_session)
	{
		if (auto reason = gamer_info ? SavedPlayers::GetBlockJoinReason(gamer_info->m_GamerHandle.m_RockstarId) : std::nullopt)
		{
			bool sent = WriteRefusal(ctx, *reason);
			if (!sent)
				*reinterpret_cast<std::uint32_t*>(ctx + 0x234) = 0;

			// they retry, so tell the user once per minute per player
			static std::mutex mutex;
			static std::unordered_map<std::uint64_t, std::chrono::steady_clock::time_point> last_logs;
			auto rid = gamer_info->m_GamerHandle.m_RockstarId;
			auto now = std::chrono::steady_clock::now();
			bool log;
			{
				std::lock_guard lock(mutex);
				auto& last = last_logs[rid];
				log = last == std::chrono::steady_clock::time_point{} || now - last > std::chrono::minutes(1);
				if (log)
					last = now;
			}
			if (log)
			{
				std::string name(gamer_info->m_name, strnlen(gamer_info->m_name, sizeof(gamer_info->m_name)));
				if (sent)
					LOGF(WARNING, "Refused join request from {} (RID {}): Block Join, reason {} ({})", name, rid, GetBlockJoinReasonName(*reason), *reason);
				else
					LOGF(WARNING, "Refused join request from {} (RID {}): Block Join (no reason sent)", name, rid);
				Notifications::Show("Block Join", std::format("Refused join request from {}", name), NotificationType::Warning);
			}

			return false;
		}

		return BaseHook::Get<Network::HandleJoinRequest, DetourHook<decltype(&Network::HandleJoinRequest)>>()->Original()(network, session, gamer_info, ctx, is_transition_session);
	}
}
