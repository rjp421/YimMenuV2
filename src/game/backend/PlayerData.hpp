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
		bool m_IsGodmode{}; // updated by Players::RunScript, read by the player list

		// the selected player's ped for the Info page, updated by Players::RunScript
		bool m_HasPed{};
		int m_Health{}, m_MaxHealth{};
		rage::fvector3 m_Position{};
		float m_Distance{};
	};
}