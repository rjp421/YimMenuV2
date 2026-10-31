#pragma once

namespace YimMenu
{
	// Diagnostics for "There has been an error with this session" kicks: logs (and saves a netlog for) scripts that call
	// NETWORK_BAIL / NETWORK_BAIL_TRANSITION, warning screens shown by scripts, and script threads that are killed by an error.
	// Also records every network script event and changes of our session state (host, transition, script host) into the net recorder,
	// and logs session events and state changes
	class BailMonitor
	{
	public:
		static void RunScript();

		// when the local player became freemode's script host, or nullopt when we aren't (thread safe, updated every frame)
		static std::optional<std::chrono::steady_clock::time_point> FreemodeHostSince();

		// freemode's script host (player id), -1 when there is none or we aren't in a session (thread safe, updated every frame)
		static int FreemodeHost();

		// how long freemode's script host (whoever it is) has been the host, as far as we have seen (thread safe)
		static std::chrono::steady_clock::duration FreemodeHostHeldFor();

		// asks the game thread to make everyone take us as freemode's script host again (Reclaim Script Host; thread safe)
		static void RequestFreemodeHostReclaim();

		// true for 5 s after we reclaimed (answers to it must not count as takeovers; thread safe)
		static bool ReclaimedRecently();
	};
}
