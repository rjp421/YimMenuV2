#include "game/commands/PlayerCommand.hpp"
#include "game/gta/data/Weapons.hpp"

namespace YimMenu::Features
{
	class RemoveWeapons : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		virtual void OnCall(Player player) override
		{
			auto ped = player.GetPed();
			if (!ped)
				return;

			for (auto weapon : g_WeaponHashes)
				if (weapon != "WEAPON_UNARMED"_J)
					ped.RemoveWeapon(weapon);
		}
	};

	static RemoveWeapons _RemoveWeapons{"removeweapons", "Remove Weapons", "Removes all of the player's weapons"};
}
