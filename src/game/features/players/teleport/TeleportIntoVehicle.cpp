#include "core/backend/ScriptMgr.hpp"
#include "core/frontend/Notifications.hpp"
#include "game/backend/Self.hpp"
#include "game/commands/PlayerCommand.hpp"

namespace YimMenu::Features
{
	class TeleportIntoVehicle : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		static void Notify(std::string_view message, NotificationType type = NotificationType::Warning)
		{
			Notifications::Show("Teleport Into Vehicle", std::string(message), type);
		}

		// the game only creates a far away vehicle on our side once we are close to it
		static Vehicle WaitForVehicle(Player player, Ped self)
		{
			auto position = player.GetPed().GetPosition();
			position.z += 10.0f; // above them, so a vehicle we are driving doesn't land on theirs
			self.TeleportTo(position);

			for (int i = 0; i < 30; i++)
			{
				ScriptMgr::Yield(100ms);

				if (!player.IsValid() || !player.GetPed())
					break;

				if (auto vehicle = player.GetPed().GetVehicle())
					return vehicle;
			}

			return nullptr;
		}

		virtual void OnCall(Player player) override
		{
			auto self = Self::GetPed();
			auto ped = player.GetPed();
			if (!self || !ped || player.IsLocal())
				return;

			auto vehicle = ped.GetVehicle();
			if (!vehicle)
			{
				if (!ped.GetVehicleObjectId())
				{
					Notify(std::format("{} is not in a vehicle", player.GetName()));
					return;
				}

				vehicle = WaitForVehicle(player, self);
				if (!vehicle)
				{
					Notify("Their vehicle did not appear, you were left near their last position");
					return;
				}
			}

			// -2 is any free passenger seat
			int seat = -1;
			if (!vehicle.IsSeatFree(seat))
			{
				seat = -2;
				if (!vehicle.IsSeatFree(seat))
				{
					Notify("Their vehicle has no free seats");
					return;
				}
			}

			self.SetInVehicle(vehicle, seat);
			ScriptMgr::Yield();

			auto current = self.GetVehicle();
			if (!current || current != vehicle)
				Notify("Could not get into their vehicle");
		}
	};

	// all_version off: everyone's vehicle at once means nothing for one ped
	static TeleportIntoVehicle _TeleportIntoVehicle{"tpintoveh", "Teleport Into Vehicle", "Puts you in the player's vehicle, in the driver's seat if it is free", 0, false};
}
