#pragma once

namespace YimMenu
{
	class ExceptionHandler final
	{
	public:
		ExceptionHandler();
		virtual ~ExceptionHandler();

	private:
		void* m_Handler;
		uint32_t m_OldErrorMode;
	};

	extern LONG VectoredExceptionHandler(EXCEPTION_POINTERS* exception_info);

	// optional, called on every exception before it is logged (lets game code save extra state)
	inline void (*g_OnException)() = nullptr;
}