#pragma once
#include <chrono>
#include <unordered_set>

namespace YimMenu
{
	class PlayerData
	{
	public:
		bool m_IsModder{};
		bool m_RunFreemodeStateKick{};
		bool m_SemiGodmode{};    // Help: refill health and armor when they drop
		bool m_AutoFixVehicle{}; // Help: repair their vehicle when it is damaged
		bool m_NeverWanted{};    // Help: clear their wanted level when they get one
		bool m_OffRadar{};       // Help: keep them off the radar
		std::chrono::steady_clock::time_point m_LastClearWanted{}, m_LastOffRadar{};
	};
}