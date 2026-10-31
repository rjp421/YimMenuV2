#pragma once
#include <chrono>
#include <unordered_set>
#include "types/rage/vector.hpp"

namespace YimMenu
{
	class PlayerData
	{
	public:
		bool m_IsModder{};
		std::string m_Name; // read on join, a dropped player has no name to read
		bool m_RunFreemodeStateKick{};
		bool m_SemiGodmode{};    // Help: refill health and armor when they drop
		bool m_AutoFixVehicle{}; // Help: repair their vehicle when it is damaged
		bool m_NeverWanted{};    // Help: clear their wanted level when they get one
		bool m_OffRadar{};       // Help: keep them off the radar
		std::chrono::steady_clock::time_point m_LastClearWanted{}, m_LastOffRadar{};
		bool m_IsGodmode{}; // updated by Players::RunScript, read by the player list

		// the selected player's ped for the Info page, updated by Players::RunScript
		bool m_HasPed{};
		int m_Health{}, m_MaxHealth{};
		rage::fvector3 m_Position{};
		float m_Distance{};
	};
}