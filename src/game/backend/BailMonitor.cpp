#include "BailMonitor.hpp"
#include "core/backend/ScriptMgr.hpp"
#include "core/commands/BoolCommand.hpp"
#include "game/backend/Detections.hpp"
#include "game/backend/NativeHooks.hpp"
#include "game/backend/NetRecorder.hpp"
#include "game/backend/Self.hpp"
#include "game/gta/Natives.hpp"
#include "game/gta/Scripts.hpp"
#include "types/network/CNetGamePlayer.hpp"
#include "types/script/CGameScriptHandlerNetComponent.hpp"
#include "types/script/GtaThread.hpp"
#include "game/gta/invoker/Invoker.hpp"
#include "game/pointers/Pointers.hpp"
#include "types/rage/atArray.hpp"
#include "types/rage/tlsContext.hpp"
#include "types/script/scrThread.hpp"
#include "types/script/globals/GlobalPlayerBD.hpp"

namespace YimMenu
{
	static BoolCommand _LogNetworkEvents{"lognetworkevents", "Log Network Events", "Logs the first network event of each type and every session event, not only the ones that end a session. The netlogs always have all of them", false};

	static const char* CurrentScriptName()
	{
		auto thread = rage::tlsContext::Get()->m_CurrentScriptThread;
		return thread ? thread->m_ScriptName : "unknown";
	}

	static void LogBail(const char* native, rage::scrNativeCallContext* ctx)
	{
		auto thread = rage::tlsContext::Get()->m_CurrentScriptThread;
		LOGF(WARNING, "Script {} called {}({}, {}, {}) (thread pc 0x{:X})", CurrentScriptName(), native, ctx->GetArg<int>(0), ctx->GetArg<int>(1), ctx->GetArg<int>(2), thread ? thread->m_Context.m_ProgramCounter : 0);
		NetRecorder::Dump("bail");
	}

	static void NetworkBail(rage::scrNativeCallContext* ctx)
	{
		LogBail("NETWORK_BAIL", ctx);
		NativeInvoker::GetNativeHandler(NativeIndex::NETWORK_BAIL)(ctx);
	}

	static void NetworkBailTransition(rage::scrNativeCallContext* ctx)
	{
		LogBail("NETWORK_BAIL_TRANSITION", ctx);
		NativeInvoker::GetNativeHandler(NativeIndex::NETWORK_BAIL_TRANSITION)(ctx);
	}

	// warning screens are drawn every frame while they are shown, so log each text once per 10 seconds
	static void LogWarning(const char* native, const char* header, const char* line1, const char* line2, int error_code)
	{
		static std::mutex mutex;
		static std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_logs;

		auto text = std::format("{} / {} / {}", header ? header : "", line1 ? line1 : "", line2 ? line2 : "");
		auto now = std::chrono::steady_clock::now();
		{
			std::lock_guard lock(mutex);
			auto& last = last_logs[text];
			if (last != std::chrono::steady_clock::time_point{} && now - last < std::chrono::seconds(10))
				return;
			last = now;
		}

		LOGF(WARNING, "Script {} shows a warning screen ({}): {} (error code {})", CurrentScriptName(), native, text, error_code);
	}

	static void SetWarningMessage(rage::scrNativeCallContext* ctx)
	{
		LogWarning("SET_WARNING_MESSAGE", ctx->GetArg<const char*>(0), ctx->GetArg<const char*>(2), nullptr, ctx->GetArg<int>(8));
		NativeInvoker::GetNativeHandler(NativeIndex::SET_WARNING_MESSAGE)(ctx);
	}

	static void SetWarningMessageWithHeader(rage::scrNativeCallContext* ctx)
	{
		LogWarning("SET_WARNING_MESSAGE_WITH_HEADER", ctx->GetArg<const char*>(0), ctx->GetArg<const char*>(1), ctx->GetArg<const char*>(3), 0);
		NativeInvoker::GetNativeHandler(NativeIndex::SET_WARNING_MESSAGE_WITH_HEADER)(ctx);
	}

	static void SetWarningMessageWithHeaderAndSubstringFlags(rage::scrNativeCallContext* ctx)
	{
		LogWarning("SET_WARNING_MESSAGE_WITH_HEADER_AND_SUBSTRING_FLAGS", ctx->GetArg<const char*>(0), ctx->GetArg<const char*>(1), ctx->GetArg<const char*>(3), ctx->GetArg<int>(10));
		NativeInvoker::GetNativeHandler(NativeIndex::SET_WARNING_MESSAGE_WITH_HEADER_AND_SUBSTRING_FLAGS)(ctx);
	}

	static void SetWarningMessageWithHeaderExtended(rage::scrNativeCallContext* ctx)
	{
		LogWarning("SET_WARNING_MESSAGE_WITH_HEADER_EXTENDED", ctx->GetArg<const char*>(0), ctx->GetArg<const char*>(1), ctx->GetArg<const char*>(3), 0);
		NativeInvoker::GetNativeHandler(NativeIndex::SET_WARNING_MESSAGE_WITH_HEADER_EXTENDED)(ctx);
	}

	static void SetWarningMessageWithHeaderAndSubstringFlagsExtended(rage::scrNativeCallContext* ctx)
	{
		LogWarning("SET_WARNING_MESSAGE_WITH_HEADER_AND_SUBSTRING_FLAGS_EXTENDED", ctx->GetArg<const char*>(0), ctx->GetArg<const char*>(1), ctx->GetArg<const char*>(4), ctx->GetArg<int>(11));
		NativeInvoker::GetNativeHandler(NativeIndex::SET_WARNING_MESSAGE_WITH_HEADER_AND_SUBSTRING_FLAGS_EXTENDED)(ctx);
	}

	// a killed thread keeps its error message until the slot is reused, so check every frame
	static void CheckThreads()
	{
		static std::unordered_set<std::uint32_t> reported;
		if (!Pointers.ScriptThreads)
			return;

		for (auto thread : *Pointers.ScriptThreads)
		{
			if (!thread || thread->m_Context.m_ThreadId == 0 || thread->m_Context.m_State != rage::scrThread::State::KILLED)
				continue;
			// scripts that end normally leave one of these
			std::string_view message = thread->m_ErrorMessage;
			if (message.empty() || message == "Not aborted yet?" || message == "via scrThread::Kill on myself")
				continue;
			if (!reported.insert(thread->m_Context.m_ThreadId).second)
				continue;

			LOGF(WARNING, "Script {} (thread {}) was killed by an error: {} (pc 0x{:X})", thread->m_ScriptName, thread->m_Context.m_ThreadId, thread->m_ErrorMessage, thread->m_Context.m_ProgramCounter);
			NetRecorder::Dump("script error");
		}
	}

	static std::string PlayerName(int id)
	{
		if (id < 0 || id >= 32)
			return std::to_string(id);
		Player player(static_cast<std::uint8_t>(id));
		return player.IsValid() ? std::format("{} ({})", player.GetName(), id) : std::to_string(id);
	}

	// session/host/transition events, numbers from legacy's list (StorePlayerLeft .. ConnectionTimeout, Bail .. JoinSessionResponse,
	// AttemptHostMigration, SessionEvent .. TransitionGamerInstruction)
	static bool IsSessionEvent(int type)
	{
		return (type >= 155 && type <= 162) || (type >= 179 && type <= 184) || type == 204 || (type >= 206 && type <= 214);
	}

	// the network event queue only holds this frame's events. Everything goes to the net recorder; the log gets the first event of
	// each type (to check the type numbers) and session events
	static void CheckNetworkEvents()
	{
		static int last_frame = -1;
		static std::unordered_map<int, int> data_sizes; // per type, in 8 byte units; 0 = none found
		static std::unordered_set<int> seen;
		static std::unordered_map<int, std::chrono::steady_clock::time_point> last_logs;
		static std::array<std::int64_t, 512> data;

		auto frame = MISC::GET_FRAME_COUNT();
		if (frame == last_frame)
			return;
		last_frame = frame;

		auto count = SCRIPT::GET_NUMBER_OF_EVENTS(1);
		for (int i = 0; i < count; i++)
		{
			auto type = SCRIPT::GET_EVENT_AT_INDEX(1, i);

			// the data size is fixed per type except for script events, and GET_EVENT_DATA only copies when the size matches
			int size = 0;
			if (type != 174)
			{
				auto it = data_sizes.find(type);
				if (it == data_sizes.end())
				{
					for (int s = 1; s <= 64; s++)
						if (SCRIPT::GET_EVENT_DATA(1, i, data.data(), s))
						{
							size = s;
							break;
						}
					data_sizes[type] = size;
				}
				else if (it->second && SCRIPT::GET_EVENT_DATA(1, i, data.data(), it->second))
					size = it->second;
			}

			auto values = std::min(size, 8);
			NetRecorder::RecordOwn(NetRecorder::Kind::NetworkEvent, type, size, data.data(), values);

			if (type == 186 && size == 13) // EntityDamage
				Detections::OnEntityDamage(data.data());

			// the netlog always gets every event; the log only the ones that explain a session end, unless Log Network Events is on
			bool first = seen.insert(type).second;
			bool session_end = type == 157 || type == 160 || type == 161 || type == 179; // EndSession, removed (stall / complaints), Bail
			if (!_LogNetworkEvents.GetState() && !session_end)
				continue;
			if (!first && !IsSessionEvent(type))
				continue;
			auto now = std::chrono::steady_clock::now();
			auto& last = last_logs[type];
			// AttemptHostMigration comes every 2 seconds for the whole session
			auto interval = type == 204 ? std::chrono::seconds(300) : std::chrono::seconds(2);
			if (!first && now - last < interval)
				continue;
			last = now;

			std::string text;
			for (int j = 0; j < values; j++)
				text += std::format(" {}", static_cast<std::int32_t>(data[j]));
			// session ends (kicks, removals, bails) as warnings so they stand out
			if (session_end)
				LOGF(WARNING, "Network event {} ({}){}, size {}{}{}", type, NetRecorder::NetworkEventName(type), first ? " seen first" : "", size, values ? ":" : "", text);
			else
				LOGF(INFO, "Network event {} ({}){}, size {}{}{}", type, NetRecorder::NetworkEventName(type), first ? " seen first" : "", size, values ? ":" : "", text);
		}
	}

	// logs changes of our session state (and records them with the connected player count)
	// steady_clock ticks of when we became freemode host, 0 = not host
	static std::atomic<std::int64_t> g_FreemodeHostSince = 0;
	static std::atomic<int> g_FreemodeHost = -1;
	static std::atomic<std::int64_t> g_FreemodeHostChangedAt = 0; // steady_clock ticks of the last change of freemode's host (anyone)

	int BailMonitor::FreemodeHost()
	{
		return g_FreemodeHost;
	}

	std::chrono::steady_clock::duration BailMonitor::FreemodeHostHeldFor()
	{
		return std::chrono::steady_clock::now() - std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(g_FreemodeHostChangedAt.load()));
	}

	std::optional<std::chrono::steady_clock::time_point> BailMonitor::FreemodeHostSince()
	{
		auto ticks = g_FreemodeHostSince.load();
		if (ticks == 0)
			return std::nullopt;
		return std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(ticks));
	}

	static BoolCommand _ReclaimScriptHost{"reactionreclaimscripthost", "Reclaim Script Host", "Takes freemode's script host back when a player forces it from you (how script host kicks start)", true};
	static std::atomic<bool> g_ReclaimRequested = false;
	static std::atomic<std::int64_t> g_LastReclaim = 0; // steady_clock ticks

	void BailMonitor::RequestFreemodeHostReclaim()
	{
		g_ReclaimRequested = true;
	}

	bool BailMonitor::ReclaimedRecently()
	{
		auto ticks = g_LastReclaim.load();
		return ticks && std::chrono::steady_clock::now() - std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(ticks)) < 5s;
	}

	// a takeover we blocked still reached everyone else, and one we didn't block made us lose it: either way, send the "I am the host"
	// answer to everyone (and become host locally again first if needed), like legacy's scripts::force_host
	static void ReclaimFreemodeHost()
	{
		if (!g_ReclaimRequested.exchange(false) || !_ReclaimScriptHost.GetState())
			return;

		// a menu that forces host every frame would make this a loop: at most once per 2 s and 10 per minute
		static std::chrono::steady_clock::time_point last{}, window_start{};
		static int in_window = 0;
		auto now = std::chrono::steady_clock::now();
		if (now - last < 2s)
			return;
		if (now - window_start > 60s)
		{
			window_start = now;
			in_window = 0;
		}
		if (++in_window > 10)
		{
			if (in_window == 11)
				LOG(WARNING) << "Not reclaiming freemode script host again for now (10 times in a minute)";
			return;
		}
		last = now;

		if (!NETWORK::NETWORK_IS_IN_SESSION() || NETWORK::NETWORK_IS_IN_TRANSITION())
			return;
		auto thread = Scripts::FindScriptThread("freemode"_J);
		auto net_component = thread ? reinterpret_cast<GtaThread*>(thread)->m_NetComponent : nullptr;
		if (!net_component)
			return;

		g_LastReclaim = std::chrono::steady_clock::now().time_since_epoch().count();
		if (net_component->IsLocalPlayerHost())
		{
			Scripts::SendScriptHostAck(thread);
			LOG(WARNING) << "Told everyone that we are still freemode's script host";
		}
		else
		{
			auto host = net_component->GetHost();
			Scripts::ForceScriptHost(thread);
			LOGF(WARNING, "Took freemode script host back from {}", host ? host->GetName() : "?");
		}
	}

	static void CheckSessionState()
	{
		using State = NetRecorder::SessionState;
		auto self = Self::GetPlayer().GetId();
		auto bd = GlobalPlayerBD::Get();
		auto ped = Self::GetPed();
		static std::array<int, 8> last{-2, -2, -2, -2, -2, -2, -2, -2};
		std::array<int, 8> now{
		    NETWORK::NETWORK_IS_IN_SESSION() ? 1 : 0,
		    NETWORK::NETWORK_IS_HOST() ? 1 : 0,
		    NETWORK::NETWORK_IS_IN_TRANSITION() ? 1 : 0,
		    NETWORK::NETWORK_GET_HOST_PLAYER_INDEX(),
		    NETWORK::NETWORK_GET_HOST_OF_SCRIPT("freemode", -1, 0),
		    NETWORK::NETWORK_GET_NUM_CONNECTED_PLAYERS(),
		    bd && self >= 0 && self < 32 ? static_cast<int>(bd->Entries[self].SimpleInteriorData.Index) : -1,
		    ped ? INTERIOR::GET_INTERIOR_FROM_ENTITY(ped.GetHandle()) : 0,
		};

		if (int host = now[0] ? now[4] : -1; g_FreemodeHost.exchange(host) != host)
			g_FreemodeHostChangedAt = std::chrono::steady_clock::now().time_since_epoch().count();
		bool freemode_host = now[0] && now[4] == Self::GetPlayer().GetId();
		if (!freemode_host)
			g_FreemodeHostSince = 0;
		else if (g_FreemodeHostSince == 0)
			g_FreemodeHostSince = std::chrono::steady_clock::now().time_since_epoch().count();

		for (std::size_t i = 0; i < now.size(); i++)
		{
			if (now[i] == last[i])
				continue;
			NetRecorder::RecordOwn(NetRecorder::Kind::State, static_cast<std::uint32_t>(i), now[i]);
			if (last[i] == -2 || i == static_cast<std::size_t>(State::ConnectedPlayers))
				continue;

			// freemode's script host only moves on its own when the host leaves; menus can force it ("force script host"), which
			// gives them the script host kick. Certain: it is how the session kick starts
			if (i == static_cast<std::size_t>(State::FreemodeHost) && last[i] == self && now[i] >= 0 && now[i] < 32 && now[i] != self && now[0] && !now[2])
			{
				Player taker(static_cast<std::uint8_t>(now[i]));
				if (taker.IsValid())
				{
					LOGF(WARNING, "{} took freemode script host from us", taker.GetName());
					Detections::Add(taker, DetectionLevel::Certain, "took freemode script host from us");
					BailMonitor::RequestFreemodeHostReclaim();
				}
			}

			switch (static_cast<State>(i))
			{
			case State::InSession: LOGF(INFO, "Session state: in session {} ({} players)", now[i], now[5]); break;
			case State::Host: LOGF(INFO, "Session state: we are host {} ({} players)", now[i], now[5]); break;
			case State::InTransition: LOGF(INFO, "Session state: in transition {} ({} players)", now[i], now[5]); break;
			case State::SessionHost: LOGF(INFO, "Session state: session host {} -> {} ({} players)", PlayerName(last[i]), PlayerName(now[i]), now[5]); break;
			case State::FreemodeHost: LOGF(INFO, "Session state: freemode script host {} -> {} ({} players)", PlayerName(last[i]), PlayerName(now[i]), now[5]); break;
			// to find what moves us out of an interior: the netlog has these too, next to what other players sent
			case State::SimpleInterior: LOGF(INFO, "Session state: simple interior {} -> {}", last[i], now[i]); break;
			case State::Interior: LOGF(INFO, "Session state: interior {} -> {}", last[i], now[i]); break;
			default: break;
			}
		}
		last = now;
	}

	void BailMonitor::RunScript()
	{
		NativeHooks::AddHook(NativeHooks::ALL_SCRIPTS, NativeIndex::NETWORK_BAIL, &NetworkBail);
		NativeHooks::AddHook(NativeHooks::ALL_SCRIPTS, NativeIndex::NETWORK_BAIL_TRANSITION, &NetworkBailTransition);
		NativeHooks::AddHook(NativeHooks::ALL_SCRIPTS, NativeIndex::SET_WARNING_MESSAGE, &SetWarningMessage);
		NativeHooks::AddHook(NativeHooks::ALL_SCRIPTS, NativeIndex::SET_WARNING_MESSAGE_WITH_HEADER, &SetWarningMessageWithHeader);
		NativeHooks::AddHook(NativeHooks::ALL_SCRIPTS, NativeIndex::SET_WARNING_MESSAGE_WITH_HEADER_AND_SUBSTRING_FLAGS, &SetWarningMessageWithHeaderAndSubstringFlags);
		NativeHooks::AddHook(NativeHooks::ALL_SCRIPTS, NativeIndex::SET_WARNING_MESSAGE_WITH_HEADER_EXTENDED, &SetWarningMessageWithHeaderExtended);
		NativeHooks::AddHook(NativeHooks::ALL_SCRIPTS, NativeIndex::SET_WARNING_MESSAGE_WITH_HEADER_AND_SUBSTRING_FLAGS_EXTENDED, &SetWarningMessageWithHeaderAndSubstringFlagsExtended);

		while (g_Running)
		{
			CheckThreads();
			CheckNetworkEvents();
			CheckSessionState();
			ReclaimFreemodeHost();
			ScriptMgr::Yield();
		}
	}
}
