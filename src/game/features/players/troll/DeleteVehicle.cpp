#include "core/frontend/Notifications.hpp"
#include "game/commands/PlayerCommand.hpp"

namespace YimMenu::Features
{
	class DeleteVehicle : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		virtual void OnCall(Player player) override
		{
			auto ped = player.GetPed();
			if (!ped)
				return;

			if (auto id = ped.GetVehicleObjectId())
				Entity::DeleteNetwork(id);
			else
				Notifications::Show("Delete Vehicle", std::format("{} is not in a vehicle", player.GetName()), NotificationType::Warning);
		}
	};

	static DeleteVehicle _DeleteVehicle{"deleteveh", "Delete Vehicle", "Deletes the vehicle the player is in"};
}