#include "core/hooking/DetourHook.hpp"
#include "core/hooking/Hooking.hpp"
#include "core/scripting/LuaManager.hpp"
#include "game/backend/AnticheatBypass.hpp"
#include "game/backend/BailMonitor.hpp"
#include "game/backend/Detections.hpp"
#include "game/backend/NetRecorder.hpp"
#include "game/backend/Players.hpp"
#include "game/frontend/ChatDisplay.hpp"
#include "game/hooks/Hooks.hpp"
#include "game/gta/Packet.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/backend/Self.hpp"
#include "types/network/CNetworkSession.hpp"
#include "types/network/netEvent.hpp"
#include "types/network/netMessage.hpp"
#include "types/rage/datBitBuffer.hpp"
#include "types/script/CGameScriptId.hpp"
#include "core/frontend/widgets/imgui_colors.h"

namespace YimMenu::Hooks
{
	static std::optional<rage::netMessage::Type> ReadType(rage::datBitBuffer& buffer)
	{
		if (buffer.Read<int>(14) != 0x3246)
			return std::nullopt;

		auto extended = buffer.Read<bool>(1);
		return buffer.Read<rage::netMessage::Type>(extended ? 16 : 8);
	}

	// high-volume messages that would push the interesting ones out of the net recorder
	static bool IsRoutineMessage(rage::netMessage::Type type, int length)
	{
		switch (type)
		{
		case rage::netMessage::Type::NetArrayMgrUpdateAck:
		case rage::netMessage::Type::PackedCloneSyncACKs:
		case rage::netMessage::Type::NetTimeSync:
		case rage::netMessage::Type::BattlEyeCmd: return true;
		case rage::netMessage::Type::CloneSync: return length < 128;
		default: return false;
		}
	}

	// like legacy's rate limiter: true when more than Allowed messages came within PeriodMs, separately per sender
	// (players that are still joining share one state); logs once when a burst starts being blocked
	template<int Allowed, int PeriodMs>
	static bool IsFlooding(Player player, const char* what)
	{
		struct State
		{
			std::chrono::steady_clock::time_point m_Start{};
			int m_Count = 0;
		};
		static std::mutex mutex;
		static std::array<State, 256> states;

		std::lock_guard lock(mutex);
		auto& state = states[player ? static_cast<std::uint8_t>(player.GetId()) : 255];
		auto now = std::chrono::steady_clock::now();
		if (now - state.m_Start >= std::chrono::milliseconds(PeriodMs))
		{
			state.m_Start = now;
			state.m_Count = 0;
		}

		if (++state.m_Count <= Allowed)
			return false;
		if (state.m_Count == Allowed + 1)
			LOGF(WARNING, "Blocked {} flood from {}", what, player ? player.GetName() : "a joining player");
		return true;
	}

	struct SessionMember
	{
		std::string m_Name;
		bool m_IsSessionHost;
	};

	// the session member with this connection peer id: finds senders that have no player slot yet (mostly while we join a
	// session, when the players already there still have none). Copies what it needs, the game changes these lists itself
	static std::optional<SessionMember> FindSessionMember(std::uint32_t peer_id)
	{
		if (!Pointers.NetworkSession || !*Pointers.NetworkSession)
			return std::nullopt;

		auto network = *Pointers.NetworkSession;
		for (auto session : {network->m_GameSession, network->m_TransitionSession})
		{
			if (!session)
				continue;

			auto count = std::clamp(session->m_PlayerCount, 0, 32);
			for (int i = 0; i < count; i++)
			{
				auto member = session->m_Players[i];
				if (!member || member->m_PeerId != peer_id)
					continue;

				// the name is in the member's peer (same Rockstar id)
				auto rid = member->m_GamerInfo.m_GamerHandle.m_RockstarId;
				std::string name = std::format("rid {}", rid);
				auto peers = std::clamp(session->m_PeerCount, 0, 32);
				for (int j = 0; j < peers; j++)
				{
					if (auto peer = session->m_Peers[j]; peer && peer->m_GamerInfo.m_GamerHandle.m_RockstarId == rid)
					{
						auto& info = peer->m_GamerInfo;
						if (auto length = strnlen(info.m_name, sizeof(info.m_name)))
							name.assign(info.m_name, length);
						break;
					}
				}

				return SessionMember{name, member->m_HostToken == session->m_HostToken};
			}
		}

		return std::nullopt;
	}

	static std::string GetSenderName(Player player, rage::netEvent* event)
	{
		if (player)
			return player.GetName();
		if (auto member = FindSessionMember(event->m_PeerId))
			return std::format("{} (no player slot yet{})", member->m_Name, member->m_IsSessionHost ? ", session host" : "");
		return std::format("unknown (peer {})", event->m_PeerId);
	}

	// Data for protecting the session while someone else is freemode's host: every freemode script host message goes to the log, with
	// who freemode's host is at that moment (at most 60 lines a minute; the messages are also in the netlogs)
	static void LogFreemodeHostMessage(Player sender, rage::netEvent* event, std::string_view what)
	{
		static std::mutex mutex;
		static std::chrono::steady_clock::time_point window{};
		static int count = 0;
		{
			std::lock_guard lock(mutex);
			auto now = std::chrono::steady_clock::now();
			if (now - window > std::chrono::minutes(1))
			{
				window = now;
				count = 0;
			}
			if (++count > 60)
				return;
		}

		std::string host = "none";
		if (auto id = BailMonitor::FreemodeHost(); id >= 0 && id < 32)
		{
			if (id == Self::GetPlayer().GetId())
			{
				auto since = BailMonitor::FreemodeHostSince();
				host = since ? std::format("us (for {} s)", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - *since).count()) : "us";
			}
			else if (Player player(static_cast<std::uint8_t>(id)); player.IsValid())
				host = player.GetName();
			else
				host = std::format("player {}", id);
		}
		LOGF(INFO, "Freemode script host message from {}: {}; freemode host: {}", GetSenderName(sender, event), what, host);
	}

	void Network::ReceiveNetMessage(void* a1, rage::netConnectionManager* mgr, rage::netEvent* event)
	{
		if (!g_Running)
			return BaseHook::Get<Network::ReceiveNetMessage, DetourHook<decltype(&Network::ReceiveNetMessage)>>()->Original()(a1, mgr, event);

		Hooking::InstallObjectMgrHooks();

		if (event->GetEventType() != rage::netEvent::Type::FrameReceived)
			return BaseHook::Get<Network::ReceiveNetMessage, DetourHook<decltype(&Network::ReceiveNetMessage)>>()->Original()(a1, mgr, event);

		// TODO: port security ID stuff from V1
		auto fr_evt = reinterpret_cast<rage::netEventFrameReceived*>(event);
		rage::datBitBuffer buffer(fr_evt->m_Data, fr_evt->m_Length, true);

		rage::netMessage::Type type = ReadType(buffer).value_or(rage::netMessage::Type::Invalid);
		if (!IsRoutineMessage(type, fr_evt->m_Length))
			NetRecorder::Record(Players::GetByMessageId(fr_evt->m_MsgId), NetRecorder::Kind::Message, static_cast<uint32_t>(type), fr_evt->m_Length);

		switch (type)
		{
		case rage::netMessage::Type::TextMessage:
		{
			auto player = Players::GetByMessageId(fr_evt->m_MsgId);
			if (player)
			{
				char msg[256]{};
				buffer.ReadString(msg, sizeof(msg)); // we don't need the rest

				// enhanced has no text chat, so only mod menus send these. Proof of a menu, but not an attack: strong, so two
				// separate messages mark the sender as a modder and there is no host kick
				Detections::Add(player, DetectionLevel::Strong, "chat message (enhanced has no chat)");

				if (!LuaManager::DispatchEvent(MenuEvent::ChatMessageReceived, [player, msg](lua_State* state){
					lua_pushinteger(state, player.GetId());
					lua_pushstring(state, msg);
					return 2;
				}))
					return;

				ChatDisplay::Show(player.GetName(), msg, ImGui::Colors::LightBlue);
			}
			return; // the game doesn't handle that anyway
		}
		case rage::netMessage::Type::PackedEvents:
		{
			auto player = Players::GetByMessageId(fr_evt->m_MsgId);

			if (!player)
				break;

			uint32_t count = buffer.Read<uint32_t>(5);
			uint32_t buffer_size = buffer.Read<uint32_t>(15);

			if (buffer_size > 7296)
				buffer_size = 7296;

			int remaining = buffer_size;

			while (remaining >= 39)
			{
				int bits_read = buffer.m_BitsRead;

				uint16_t event_id = buffer.Read<uint16_t>(7);
				uint32_t event_index = buffer.Read<uint32_t>(9);
				uint32_t event_handled_bits = buffer.Read<uint32_t>(8);
				uint32_t event_data_size = buffer.Read<uint32_t>(15);
				NetRecorder::Record(player, NetRecorder::Kind::Event, event_id, event_data_size);

				if (buffer.Read<bool>(1))
					buffer.Read<uint32_t>(16);

				char event_data[4096 + 1];
				if (event_data_size)
				{
					buffer.ReadArray(event_data, event_data_size);
				}

				rage::datBitBuffer event_buffer(event_data, sizeof(event_data), true);
				event_buffer.m_MaxBit = event_data_size + 1;

				ReceiveNetGameEvent(player, event_id, event_index, event_handled_bits, event_buffer);

				remaining -= (int)buffer.m_BitsRead - bits_read;
			}

			break;
		}
		case rage::netMessage::Type::PackedReliables:
		{
			auto player = Players::GetByMessageId(fr_evt->m_MsgId);

			auto flags = buffer.Read<int>(4);
			NetRecorder::Record(player, NetRecorder::Kind::Reliables, flags);

			if ((flags & 1) != 0)
			{
				auto timestamp = buffer.Read<std::uint32_t>(32);
				if (auto num_msgs = buffer.Read<int>(5))
					buffer.Seek(buffer.Read<std::uint32_t>(13));
			}

			if ((flags & 2) != 0)
			{
				if (auto num_msgs = buffer.Read<int>(5))
					buffer.Seek(buffer.Read<std::uint32_t>(13));
			}

			if ((flags & 4) != 0)
			{
				if (auto num_msgs = buffer.Read<int>(5))
				{
					auto sz = buffer.Read<std::uint32_t>(13);
					auto pos_now = buffer.m_BitsRead;
					while (pos_now + sz > buffer.m_BitsRead)
					{
						auto id = buffer.Read<std::uint16_t>(13);
						auto token = buffer.Read<int>(5);
						NetRecorder::Record(player, NetRecorder::Kind::CloneRemove, id, token);
						bool reject = false;

						if (Self::GetPed().GetPointer<void*>() && Self::GetPed().GetNetworkObjectId() == id)
						{
							if (player)
								LOGF(WARNING, "Blocked player deletion crash from {}", player.GetName());
							Detections::Add(player, DetectionLevel::Certain, "deleting our player");
							reject = true;
						}

						if (Self::GetVehicle() && Self::GetVehicle().HasControl() && Self::GetVehicle().GetNetworkObjectId() == id)
						{
							if (player)
								LOGF(WARNING, "Blocked vehicle deletion from {}", player.GetName());
							Detections::Add(player, DetectionLevel::Weak, "taking over our vehicle");
							reject = true;
						}

						if (reject)
						{
							rage::datBitBuffer write_buf(fr_evt->m_Data, fr_evt->m_Length);
							write_buf.Seek(buffer.m_BitsRead - 5 - 13);
							write_buf.Write<std::uint16_t>(0xFFFF, 16);
							NetRecorder::Dump("blocked object deletion");
						}
					}
				}
			}

			break;
		}
		case rage::netMessage::Type::KickPlayer:
		{
			// the session host kicks us (reason in 5 bits, legacy's KickReason). A host whose BattlEye gets no answers from us kicks
			// us a few minutes after we join; legacy ignores voted out and BattlEye kicks/bans. The sender often has no player slot
			// yet (GetSenderName finds them in the session)
			static constexpr const char* reasons[] = {"voted out", "peer complaints", "connection error", "NAT type", "Rockstar admin", "Rockstar admin blacklist", "ROS ban", "BattlEye kick", "BattlEye ban"};
			auto reason = buffer.Read<std::uint32_t>(5);
			auto reason_name = reason < std::size(reasons) ? reasons[reason] : "unknown";
			auto sender = GetSenderName(Players::GetByMessageId(fr_evt->m_MsgId), fr_evt);
			if (!AnticheatBypass::IsFSLProvidingBattlEyeBypass())
			{
				// V2 answers BattlEye itself then (see BattlEyeCmd below) and ignores every host kick
				LOGF(WARNING, "Ignored a host kick from {}: {} ({})", sender, reason_name, reason);
				return;
			}
			// blocking the kick message does not keep us in: the host side drops us anyway
			LOGF(WARNING, "Host kick from {}: {} ({})", sender, reason_name, reason);
			break;
		}
		case rage::netMessage::Type::BattlEyeCmd:
		{
			if (!AnticheatBypass::IsFSLProvidingBattlEyeBypass() && !AnticheatBypass::IsBattlEyeRunning())
			{
				char data[1028]{};
				int size = buffer.Read<int>(11);
				bool from_client = buffer.Read<bool>(1);
				buffer.Seek(4); // normalize before we read
				buffer.ReadArrayBytes(&data, size);

				if (from_client)
					break;

				Packet reply;
				char reply_buf[1028]{};
				int reply_sz = 0;

				auto op = data[0];

				if (op == 0)
				{
					char payload[] = {0x0, 0x5};
					reply_sz = sizeof(payload);
					memcpy(reply_buf, payload, reply_sz);
				}
				else if (op == 2 || op == 4)
				{
					char payload[] = {op, data[1]};
					reply_sz = sizeof(payload);
					memcpy(reply_buf, payload, reply_sz);
				}
				else if (op == 9)
				{
					reply_sz = size;
					memcpy(reply_buf, data, reply_sz);
				}

				reply.WriteMessageHeader(rage::netMessage::Type::BattlEyeCmd);
				reply.GetBuffer().Write<int>(reply_sz, 11);
				reply.GetBuffer().Write<bool>(true, 1);
				reply.GetBuffer().Seek(4);
				reply.GetBuffer().WriteArrayBytes(reply_buf, reply_sz);
				reply.Send(fr_evt->m_MsgId);
				return;
			}
			break;
		}
		case rage::netMessage::Type::RequestKickFromHost:
		case rage::netMessage::Type::ConfigRequest:
		{
			LOGF(WARNING, "Blocked net message {:X} from {}", static_cast<int>(type), GetSenderName(Players::GetByMessageId(fr_evt->m_MsgId), fr_evt));
			return;
		}
		// ported from legacy, both are kick attempts when flooded. Normally only joining players send radio station sync
		// requests (at most 3 in 5 s)
		case rage::netMessage::Type::RadioStationSyncRequest:
		{
			auto player = Players::GetByMessageId(fr_evt->m_MsgId);

			if (player ? IsFlooding<2, 5000>(player, "RadioStationSyncRequest") : IsFlooding<5, 1000>(player, "RadioStationSyncRequest"))
			{
				Detections::Add(player, DetectionLevel::Certain, "radio station sync request flood (kick)");
				return;
			}

			// only the host answers these
			if (!Self::GetPlayer().IsHost())
				return;

			break;
		}
		case rage::netMessage::Type::ScriptNewHost:
		case rage::netMessage::Type::ScriptVerifyHost:
		case rage::netMessage::Type::ScriptHostRequest:
		{
			// assuming they start with the script id like ScriptVerifyHostAck (only logged when that reads as freemode)
			if (buffer.Read<std::uint32_t>(32) == "freemode"_J)
			{
				auto name = type == rage::netMessage::Type::ScriptNewHost ? "ScriptNewHost" : type == rage::netMessage::Type::ScriptVerifyHost ? "ScriptVerifyHost" :
				                                                                                                                                 "ScriptHostRequest";
				LogFreemodeHostMessage(Players::GetByMessageId(fr_evt->m_MsgId), fr_evt, name);
			}
			break;
		}
		case rage::netMessage::Type::ScriptMigrateHost:
		{
			auto player = Players::GetByMessageId(fr_evt->m_MsgId);
			if (rage::datBitBuffer peek = buffer; peek.Read<std::uint32_t>(32) == "freemode"_J)
				LogFreemodeHostMessage(player, fr_evt, "ScriptMigrateHost");

			if (player && IsFlooding<15, 2000>(player, "ScriptMigrateHost"))
			{
				Detections::Add(player, DetectionLevel::Certain, "script host migration flood (kick)");
				return;
			}

			break;
		}
		case rage::netMessage::Type::ScriptVerifyHostAck:
		{
			// "force script host" (legacy's scripts::force_host): the menu makes itself host locally and sends this to everyone, which
			// makes their games accept it as freemode's host; then it writes the host broadcast data (end session / script host kick).
			// Normally these only come as answers right after we become a script's host, so one for freemode while we have been its
			// host for a while is a takeover. Payload: CGameScriptId, "rejected" (1), "valid host" (1, only when rejected), host token
			// (16). The receiver makes the sender host when both bools are set ("I am the host"), with no token check, so the last
			// sender wins; otherwise it only raises its own host token. Only that form is blocked
			CGameScriptId id;
			id.Deserialize(buffer);
			bool rejected = buffer.Read<bool>(1);
			bool valid_host = rejected && buffer.Read<bool>(1);
			auto token = buffer.Read<std::uint16_t>(16);
			auto hash = static_cast<std::uint32_t>(id.m_Hash);
			if (hash == "freemode"_J)
			{
				NetRecorder::Record(Players::GetByMessageId(fr_evt->m_MsgId), NetRecorder::Kind::ScriptHostAck, hash, (rejected ? 1u : 0u) | (valid_host ? 2u : 0u) | (static_cast<std::uint32_t>(token) << 16));
				LogFreemodeHostMessage(Players::GetByMessageId(fr_evt->m_MsgId), fr_evt, std::format("ScriptVerifyHostAck, rejected {}, valid host {}, token {}{}", rejected, valid_host, token, rejected && valid_host ? " (\"I am the host\")" : ""));
			}

			auto since = BailMonitor::FreemodeHostSince();
			if (hash == "freemode"_J && rejected && valid_host && since && std::chrono::steady_clock::now() - *since > std::chrono::seconds(10))
			{
				auto player = Players::GetByMessageId(fr_evt->m_MsgId);
				// right after our own reclaim this may be a normal game's answer to it: drop it, but don't count it as an attack
				if (BailMonitor::ReclaimedRecently())
				{
					LOGF(WARNING, "Dropped a freemode script host claim from {} right after we reclaimed it (no detection)", GetSenderName(player, fr_evt));
					return;
				}
				// the sender also sent it to everyone else, whose games now take them as host: tell them we still are
				BailMonitor::RequestFreemodeHostReclaim();
				static std::chrono::steady_clock::time_point last_log{};
				if (std::chrono::steady_clock::now() - last_log > std::chrono::seconds(5))
				{
					last_log = std::chrono::steady_clock::now();
					LOGF(WARNING, "Blocked a forced freemode script host takeover (ScriptVerifyHostAck) from {}", GetSenderName(player, fr_evt));
					NetRecorder::Dump("blocked script host takeover");
				}
				if (player)
					Detections::Add(player, DetectionLevel::Certain, "forced script host takeover");
				return;
			}

			// a takeover from another player who holds freemode: the same "I am the host" form, from someone who isn't the host
			// while the host has held it for 10+ s and is still in the session (the real host taking it back sends ScriptVerifyHost
			// and ScriptNewHost instead). Not refused: everyone else's game accepts it anyway, and ours alone keeping the old host
			// could put our freemode out of step with theirs. Detected, so the reactions follow
			if (hash == "freemode"_J && rejected && valid_host)
			{
				auto player = Players::GetByMessageId(fr_evt->m_MsgId);
				auto host_id = BailMonitor::FreemodeHost();
				if (player && host_id >= 0 && host_id < 32 && host_id != player.GetId() && host_id != Self::GetPlayer().GetId()
				    && BailMonitor::FreemodeHostHeldFor() > std::chrono::seconds(10))
				{
					if (Player host(static_cast<std::uint8_t>(host_id)); host.IsValid())
					{
						static std::chrono::steady_clock::time_point last_log{};
						if (std::chrono::steady_clock::now() - last_log > std::chrono::seconds(5))
						{
							last_log = std::chrono::steady_clock::now();
							LOGF(WARNING, "Forced freemode script host takeover by {} from {} (not refused: we aren't the host)", GetSenderName(player, fr_evt), host.GetName());
							NetRecorder::Dump("script host takeover from another player");
						}
						Detections::Add(player, DetectionLevel::Certain, "forced script host takeover");
					}
				}
			}

			break;
		}
		case rage::netMessage::Type::RadioStationSync:
		{
			auto player = Players::GetByMessageId(fr_evt->m_MsgId);

			if (!player || !player.IsHost())
			{
				// the session host sends these while we join (4 per join), before they have a player slot: only senders known not to
				// be the host are dropped
				if (auto member = !player ? FindSessionMember(fr_evt->m_PeerId) : std::nullopt; member && member->m_IsSessionHost)
					break;

				LOGF(WARNING, "Blocked RadioStationSync from {} since they are not the host", GetSenderName(player, fr_evt));
				return;
			}

			break;
		}
		case rage::netMessage::Type::RoamingJoinBubbleAck:
		{
			auto status = buffer.Read<int>(2);
			auto bubble = buffer.Read<int>(4);

			if (status == 0)
			{
				LOGF(WARNING, "Blocked RoamingJoinBubbleAck from {} with bubble {}", GetSenderName(Players::GetByMessageId(fr_evt->m_MsgId), fr_evt), bubble);
				return;
			}

			break;
		}
		case rage::netMessage::Type::RoamingInitialBubble:
		{
			auto player = Players::GetByMessageId(fr_evt->m_MsgId);

			auto my_bubble = buffer.Read<int>(4);
			auto my_player_id = buffer.Read<int>(6);
			auto their_bubble = buffer.Read<int>(4);
			auto their_player_id = buffer.Read<int>(6);

			// only the host sends this, and only before we join
			if ((player && Self::GetPlayer().IsValid()) || my_bubble >= 10 || their_bubble >= 10 || my_bubble != their_bubble || my_player_id == their_player_id)
			{
				LOGF(WARNING, "Blocked RoamingInitialBubble from {}", GetSenderName(player, fr_evt));
				return;
			}

			break;
		}
		case rage::netMessage::Type::NonPhysicalData:
		{
			auto player = Players::GetByMessageId(fr_evt->m_MsgId);

			buffer.Seek(7); // size
			auto bubble = buffer.Read<int>(4);
			auto player_id = buffer.Read<int>(6);

			// players can announce themselves, but can't take another player's slot
			auto& players = Players::GetPlayers();
			auto existing = players.find(static_cast<uint8_t>(player_id));
			if (bubble >= 10 || player_id >= 32 || (existing != players.end() && !(existing->second == player)))
			{
				// every joining player sends bubble 10 / player id 32 once (one past the valid range, a placeholder), so don't log that one
				if (bubble != 10 || player_id != 32)
					LOGF(WARNING, "Blocked NonPhysicalData from {} with bubble {} and player id {}", GetSenderName(player, fr_evt), bubble, player_id);
				return;
			}

			break;
		}
		default:
			break;
		}

		BaseHook::Get<Network::ReceiveNetMessage, DetourHook<decltype(&Network::ReceiveNetMessage)>>()->Original()(a1, mgr, event);
	}
}