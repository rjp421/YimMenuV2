#pragma once
#include "Crossmap.hpp"
#include "game/pointers/Pointers.hpp"
#include "types/script/scrNativeHandler.hpp"

enum class NativeIndex;
namespace YimMenu
{
	class CustomCallContext : public rage::scrNativeCallContext
	{
	public:
		constexpr CustomCallContext()
		{
			m_ReturnValue = &m_ReturnStack[0];
			m_Args = &m_ArgStack[0];
		}

	private:
		uint64_t m_ReturnStack[10];
		uint64_t m_ArgStack[40];
	};

	class NativeInvoker
	{
		static void DefaultHandler(rage::scrNativeCallContext* ctx);
		static inline std::array<rage::scrNativeHandler, g_Crossmap.size()> m_Handlers;
		static inline bool m_AreHandlersCached{false};

	public:
		constexpr NativeInvoker() {};

		constexpr void BeginCall()
		{
			m_CallContext.reset();
		}

		template<int index, bool fix_vectors>
		constexpr void EndCall()
		{
			if (m_CheckScriptThread) [[unlikely]]
				CallChecked(index, &m_CallContext);
			else
				m_Handlers[index](&m_CallContext);
			if constexpr (fix_vectors)
				m_CallContext.FixVectors();
		}

		constexpr void EndCall(int index, bool fix_vectors)
		{
			if (m_CheckScriptThread) [[unlikely]]
				CallChecked(index, &m_CallContext);
			else
				m_Handlers[index](&m_CallContext);
			if (fix_vectors)
				m_CallContext.FixVectors();
		}

		template<typename T>
		constexpr void PushArg(T&& value)
		{
			m_CallContext.PushArg(std::forward<T>(value));
		}

		template<typename T>
		constexpr T& GetReturnValue()
		{
			return *m_CallContext.GetReturnValue<T>();
		}

	public:
		static void CacheHandlers();

		// Log Self Reports: logs the natives we call that set the game's script guard flag
		// (the cause of the type 5 REPORT_MYSELF_EVENT)
		static inline bool m_CheckScriptThread{false};
		static inline std::atomic<std::uint32_t> m_GuardFlagSetCount{0};
		static void CallChecked(int index, rage::scrNativeCallContext* ctx);

		template<int index, typename Ret, bool fix_vectors, typename... Args>
		static constexpr FORCEINLINE Ret Invoke(Args&&... args)
		{
			if (!m_AreHandlersCached) [[unlikely]] // we can't get rid of this unless we rewrite GetNativeHandler
				CacheHandlers();

			NativeInvoker invoker{};

			invoker.BeginCall();
			(invoker.PushArg(std::forward<Args>(args)), ...);
			invoker.EndCall<index, fix_vectors>();

			if constexpr (!std::is_same_v<Ret, void>)
			{
				return invoker.GetReturnValue<Ret>();
			}
		}

		static inline rage::scrNativeHandler GetNativeHandler(NativeIndex index)
		{
			if (!m_AreHandlersCached) [[unlikely]]
				CacheHandlers();

			return m_Handlers[(int)index];
		}

		static inline bool AreHandlersCached()
		{
			return m_AreHandlersCached;
		}

		CustomCallContext m_CallContext{};
	};
}