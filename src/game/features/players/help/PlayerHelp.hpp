#pragma once
#include "game/gta/Player.hpp"

namespace YimMenu::Features
{
	// helps other players with pickup rewards (applied by their own game) and script events. The Give* functions
	// yield between rewards, so call them from a fiber or script. player_bits has 1 << id for each player
	class PlayerHelp
	{
	public:
		static void GiveReward(int player_bits, std::uint32_t reward);
		static void GiveHealth(int player_bits);
		static void GiveArmor(int player_bits);
		static void GiveAmmo(int player_bits);
		static void FixVehicle(int player_bits);
		static void GiveWeapons(Player player); // GIVE_WEAPON_TO_PED on their ped, like the Give Weapons command
		// script events
		static void ClearWanted(int player_bits);
		static void GiveOffRadar(int player_bits);

		static int GetBits(const std::vector<Player>& players);

		// runs the per-player toggles in PlayerData
		static void RunScript();
	};
}
