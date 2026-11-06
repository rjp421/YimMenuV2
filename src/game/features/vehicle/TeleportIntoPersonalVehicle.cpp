#include "core/commands/Command.hpp"
#include "core/frontend/Notifications.hpp"
#include "game/backend/PersonalVehicles.hpp"
#include "game/backend/Self.hpp"
#include "game/pointers/Pointers.hpp"

namespace YimMenu::Features
{
	class TeleportIntoPersonalVehicle : public Command
	{
		using Command::Command;

		virtual void OnCall() override
		{
			if (!*Pointers.IsSessionStarted)
			{
				Notifications::Show("Teleport Into Personal Vehicle", "Please join GTA Online.", NotificationType::Error);
				return;
			}

			auto veh = PersonalVehicles::GetCurrentHandle();
			if (!veh.IsValid() || veh.IsDead())
			{
				Notifications::Show("Teleport Into Personal Vehicle", "Your personal vehicle is not out. Request it first.", NotificationType::Error);
				return;
			}

			auto ped = Self::GetPed();
			if (ped.GetVehicle() == veh)
				return;

			// -1 is the driver seat, -2 lets the game pick any free passenger seat
			int seat;
			if (veh.IsSeatFree(-1))
				seat = -1;
			else if (veh.IsSeatFree(-2))
				seat = -2;
			else
			{
				Notifications::Show("Teleport Into Personal Vehicle", "There are no free seats in your personal vehicle.", NotificationType::Error);
				return;
			}

			ped.SetInVehicle(veh, seat);
		}
	};

	static TeleportIntoPersonalVehicle _TeleportIntoPersonalVehicle{"tpintopv", "Teleport Into Personal Vehicle", "Puts you in the driver seat of your personal vehicle, or a free passenger seat if someone is driving it."};
}
