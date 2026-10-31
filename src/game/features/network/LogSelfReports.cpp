#include "core/commands/LoopedCommand.hpp"
#include "core/hooking/DetourHook.hpp"
#include "game/gta/invoker/Invoker.hpp"
#include "game/hooks/Hooks.hpp"
#include "game/pointers/Pointers.hpp"

#include <intrin.h>

namespace YimMenu::Features
{
	class LogSelfReports : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;

		std::uint32_t m_LastNativeSets = 0;
		bool m_WasSet = false;
		int m_OutsideLogs = 0;

		virtual void OnEnable() override
		{
			NativeInvoker::m_CheckScriptThread = true;
		}

		virtual void OnDisable() override
		{
			NativeInvoker::m_CheckScriptThread = false;
		}

		// the game clears bit 0 every frame (except while loading). Set here without one of our natives setting it means
		// the game itself or other code tripped the guard
		virtual void OnTick() override
		{
			auto flags = Pointers.ScriptGuardFlags;
			bool set = flags && (*flags & 1);
			auto native_sets = NativeInvoker::m_GuardFlagSetCount.load();

			if (set && !m_WasSet && native_sets == m_LastNativeSets && m_OutsideLogs < 50)
			{
				m_OutsideLogs++;
				LOG(WARNING) << "The game's script guard flag was set, but not by a native we called (the game itself or other code)";
			}

			m_WasSet = set;
			m_LastNativeSets = native_sets;
		}
	};

	static LogSelfReports _LogSelfReports{"logselfreports", "Log Self Reports", "Logs every REPORT_MYSELF_EVENT our game sends to the other players (the game's own anticheat reporting us), with its type and the game code that sent it. Also logs any native the menu calls that sets the game's script guard flag (the cause of a type 5 report), with its call stack"};

	// type 5: a guarded game function ran without a script thread; type 6: repeated request failures (harmless); 7/8: two counters of one game object
	static const char* GetReportDescription(uint32_t type)
	{
		switch (type)
		{
		case 5: return "game function called outside a script thread";
		case 6: return "repeated request failures";
		case 7: return "counter (GSCW)";
		case 8: return "counter (GSCB)";
		case 9: return "GSINV";
		case 10: return "GSINT";
		default: return "unknown";
		}
	}
}

namespace YimMenu::Hooks
{
	void Network::TriggerReportMyself(uint32_t type, uint32_t value)
	{
		if (YimMenu::Features::_LogSelfReports.GetState())
		{
			// the caller tells which of the game's checks fired
			auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress()) - reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
			LOGF(WARNING, "Our game sent REPORT_MYSELF_EVENT type {} ({}), value {}, from GTA5_Enhanced.exe+0x{:X}", type, YimMenu::Features::GetReportDescription(type), value, caller);
		}

		BaseHook::Get<Network::TriggerReportMyself, DetourHook<decltype(&Network::TriggerReportMyself)>>()->Original()(type, value);
	}
}
