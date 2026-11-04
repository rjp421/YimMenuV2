#include "core/backend/ScriptMgr.hpp"
#include "core/commands/IntCommand.hpp"
#include "core/frontend/Notifications.hpp"
#include "game/commands/PlayerCommand.hpp"
#include "game/features/players/help/PlayerHelp.hpp"
#include "game/pointers/Pointers.hpp"

namespace YimMenu::Features
{
	static IntCommand _TrollWantedLevel{"trollwantedlevel", "Wanted Level", "The wanted level to set", 0, 5, 5};

	class SetWanted : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		virtual void OnCall(Player player) override
		{
			const int level = _TrollWantedLevel.GetState();

			if (player.IsLocal())
			{
				player.SetWantedLevel(level);
				return;
			}

			// the alter wanted level event can only raise it, so a lower level starts by clearing it
			if (player.GetWantedLevel() > level)
			{
				PlayerHelp::ClearWanted(1 << player.GetId());
				for (int i = 0; i < 30 && player.IsValid() && player.GetWantedLevel() > level; i++)
					ScriptMgr::Yield(100ms);
			}

			for (int i = 0; i < 10 && player.IsValid() && player.GetWantedLevel() < level; i++)
			{
				if (Pointers.SetWantedLevel)
					Pointers.SetWantedLevel(player.GetHandle(), level, 0, -1);
				ScriptMgr::Yield(200ms);
			}

			if (player.IsValid() && player.GetWantedLevel() != level)
				Notifications::Show("Set Wanted", std::format("{} did not take the wanted level", player.GetName()), NotificationType::Warning);
		}
	};

	static SetWanted _SetWanted{"playersetwanted", "Set Wanted", "Sets the player's wanted level"};
}
