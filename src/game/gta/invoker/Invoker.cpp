#include "Invoker.hpp"

#include "Crossmap.hpp"
#include "core/hooking/DetourHook.hpp"
#include "game/hooks/Hooks.hpp"

#include "types/script/scrProgram.hpp"
#include "types/rage/tlsContext.hpp"

#include <intrin.h>
#include <format>
#include <mutex>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace YimMenu
{
	void NativeInvoker::DefaultHandler(rage::scrNativeCallContext* ctx)
	{
		LOG(FATAL) << "Native handler not registered";
		ctx->SetReturnValue(0);
	}

	void NativeInvoker::CacheHandlers()
	{
		if (m_AreHandlersCached)
			return;

		memcpy(m_Handlers.data(), g_Crossmap.data(), sizeof(g_Crossmap));
		auto program = reinterpret_cast<rage::scrProgram*>(calloc(1, sizeof(rage::scrProgram)));
		program->m_NativeCount = m_Handlers.size();
		program->m_NativeEntrypoints = m_Handlers.data();
		BaseHook::Get<Hooks::Script::InitNativeTables, DetourHook<decltype(&Hooks::Script::InitNativeTables)>>()->Original()(program);
		free(program);
		m_AreHandlersCached = true;
	}

	__declspec(noinline) void NativeInvoker::CallChecked(int index, rage::scrNativeCallContext* ctx)
	{
		bool no_thread = !rage::tlsContext::Get()->m_CurrentScriptThread;
		auto flags = Pointers.ScriptGuardFlags;
		bool was_set = flags && (*flags & 1);

		m_Handlers[index](ctx);

		// natives without a script thread are only a problem when they trip the guard (most such calls run on the game
		// thread outside script context and are harmless), so only those are logged
		bool set_now = !was_set && flags && (*flags & 1);
		if (!set_now)
			return;
		m_GuardFlagSetCount++;

		// our part of the call stack (the native call is inlined into its caller), so shared helpers such as
		// Entity::IsValid show who called them
		void* frames[16];
		auto count = RtlCaptureStackBackTrace(1, static_cast<DWORD>(std::size(frames)), frames, nullptr);
		auto base = reinterpret_cast<std::uintptr_t>(&__ImageBase);
		auto size = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + __ImageBase.e_lfanew)->OptionalHeader.SizeOfImage;
		std::string stack;
		std::uint64_t key = static_cast<std::uint64_t>(index);
		for (int i = 0, ours = 0; i < count && ours < 5; i++)
		{
			auto offset = reinterpret_cast<std::uintptr_t>(frames[i]) - base;
			if (offset >= size)
				continue;
			stack += std::format("{}+0x{:X}", ours++ ? " <- " : "", offset);
			key = (key * 0x100000001B3ull) ^ offset;
		}

		// each native and call stack once
		static std::mutex mutex;
		static std::unordered_set<std::uint64_t> seen;
		{
			std::lock_guard lock(mutex);
			if (seen.size() >= 1000 || !seen.insert(key).second)
				return;
		}

		LOGF(WARNING, "Native {} set the game's script guard flag (type 5 self report cause) on thread {}{}, called from YimMenuV2.dll {}", index, GetCurrentThreadId(), no_thread ? " without a script thread" : "", stack);
	}
}