#include "core/frontend/Notifications.hpp"
#include "game/commands/PlayerCommand.hpp"
#include "game/gta/Natives.hpp"

#include <cmath>

namespace YimMenu::Features
{
	// the vehicle the player is in, or nullptr after telling the user why not
	static Vehicle GetTargetVehicle(Player player)
	{
		auto ped = player.GetPed();
		if (!ped)
			return nullptr;

		auto vehicle = ped.GetVehicle();
		if (!vehicle)
		{
			// the game only creates a far away vehicle on our side once we are close to it
			Notifications::Show("Troll",
			    ped.GetVehicleObjectId() ? std::format("{}'s vehicle is too far away", player.GetName()) : std::format("{} is not in a vehicle", player.GetName()),
			    NotificationType::Warning);
		}

		return vehicle;
	}

	class VehicleCommand : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		virtual void OnCall(Player player) override
		{
			auto vehicle = GetTargetVehicle(player);
			if (!vehicle)
				return;

			if (!vehicle.RequestControl())
			{
				Notifications::Show("Troll", std::format("Could not take control of {}'s vehicle", player.GetName()), NotificationType::Warning);
				return;
			}

			OnVehicle(vehicle);
		}

	protected:
		virtual void OnVehicle(Vehicle vehicle) = 0;
	};

	// taking the vehicle from its driver throws them out, so this one must not wait for them to hand it over
	class KickFromVehicle : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		virtual void OnCall(Player player) override
		{
			if (auto vehicle = GetTargetVehicle(player))
				vehicle.ForceControl();
		}
	};

	class KillEngine : public VehicleCommand
	{
		using VehicleCommand::VehicleCommand;

		virtual void OnVehicle(Vehicle vehicle) override
		{
			VEHICLE::SET_VEHICLE_ENGINE_HEALTH(vehicle.GetHandle(), -4000.0f);
		}
	};

	class BurstTires : public VehicleCommand
	{
		using VehicleCommand::VehicleCommand;

		virtual void OnVehicle(Vehicle vehicle) override
		{
			VEHICLE::SET_VEHICLE_TYRES_CAN_BURST(vehicle.GetHandle(), true);
			for (int tire = 0; tire < 8; tire++)
				VEHICLE::SET_VEHICLE_TYRE_BURST(vehicle.GetHandle(), tire, true, 1000.0f);
		}
	};

	class SmashWindows : public VehicleCommand
	{
		using VehicleCommand::VehicleCommand;

		virtual void OnVehicle(Vehicle vehicle) override
		{
			for (int window = 0; window < 8; window++)
				VEHICLE::SMASH_VEHICLE_WINDOW(vehicle.GetHandle(), window);
		}
	};

	class StopVehicle : public VehicleCommand
	{
		using VehicleCommand::VehicleCommand;

		virtual void OnVehicle(Vehicle vehicle) override
		{
			vehicle.BringToHalt(0.0f, 1);
		}
	};

	class BoostVehicle : public VehicleCommand
	{
		using VehicleCommand::VehicleCommand;

		virtual void OnVehicle(Vehicle vehicle) override
		{
			VEHICLE::SET_VEHICLE_FORWARD_SPEED(vehicle.GetHandle(), VEHICLE::GET_VEHICLE_ESTIMATED_MAX_SPEED(vehicle.GetHandle()));
		}
	};

	class LaunchVehicle : public VehicleCommand
	{
		using VehicleCommand::VehicleCommand;

		virtual void OnVehicle(Vehicle vehicle) override
		{
			ENTITY::APPLY_FORCE_TO_ENTITY(vehicle.GetHandle(), 1, 0.0f, 0.0f, 50000.0f, 0.0f, 0.0f, 0.0f, 0, false, true, true, false, true);
		}
	};

	class FlipVehicle : public VehicleCommand
	{
		using VehicleCommand::VehicleCommand;

		virtual void OnVehicle(Vehicle vehicle) override
		{
			// setting the heading stops the vehicle, so keep its speed
			const float speed = vehicle.GetSpeed();
			vehicle.SetHeading(std::fmod(vehicle.GetHeading() + 180.0f, 360.0f));
			VEHICLE::SET_VEHICLE_FORWARD_SPEED(vehicle.GetHandle(), speed);
		}
	};

	class OpenDoors : public VehicleCommand
	{
		using VehicleCommand::VehicleCommand;

		virtual void OnVehicle(Vehicle vehicle) override
		{
			for (int door = 0; door < VEHICLE::GET_NUMBER_OF_VEHICLE_DOORS(vehicle.GetHandle()); door++)
				VEHICLE::SET_VEHICLE_DOOR_OPEN(vehicle.GetHandle(), door, true, false);
		}
	};

	class CloseDoors : public VehicleCommand
	{
		using VehicleCommand::VehicleCommand;

		virtual void OnVehicle(Vehicle vehicle) override
		{
			VEHICLE::SET_VEHICLE_DOORS_SHUT(vehicle.GetHandle(), false);
		}
	};

	static constexpr int VEHICLE_LOCK_UNLOCKED = 1;
	static constexpr int VEHICLE_LOCK_LOCKOUT_PLAYER_ONLY = 3;

	class LockVehicle : public VehicleCommand
	{
		using VehicleCommand::VehicleCommand;

		virtual void OnVehicle(Vehicle vehicle) override
		{
			VEHICLE::SET_VEHICLE_DOORS_LOCKED(vehicle.GetHandle(), VEHICLE_LOCK_LOCKOUT_PLAYER_ONLY);
		}
	};

	class UnlockVehicle : public VehicleCommand
	{
		using VehicleCommand::VehicleCommand;

		virtual void OnVehicle(Vehicle vehicle) override
		{
			VEHICLE::SET_VEHICLE_DOORS_LOCKED(vehicle.GetHandle(), VEHICLE_LOCK_UNLOCKED);
		}
	};

	static KickFromVehicle _KickFromVehicle{"vehkick", "Kick From Vehicle", "Throws the player out of their vehicle"};
	static KillEngine _KillEngine{"killengine", "Kill Engine", "Destroys their vehicle's engine"};
	static BurstTires _BurstTires{"bursttires", "Burst Tires", "Bursts all of their vehicle's tires"};
	static SmashWindows _SmashWindows{"smashwindows", "Smash Windows", "Smashes all of their vehicle's windows"};
	static StopVehicle _StopVehicle{"stopveh", "Stop Vehicle", "Stops their vehicle dead"};
	static BoostVehicle _BoostVehicle{"boostveh", "Boost Vehicle", "Pushes their vehicle forward at its top speed"};
	static LaunchVehicle _LaunchVehicle{"launchveh", "Launch Vehicle", "Launches their vehicle into the air"};
	static FlipVehicle _FlipVehicle{"flip180", "Flip 180", "Turns their vehicle around, keeping its speed"};
	static OpenDoors _OpenDoors{"opendoors", "Open Doors", "Opens all of their vehicle's doors"};
	static CloseDoors _CloseDoors{"closedoors", "Close Doors", "Closes all of their vehicle's doors"};
	static LockVehicle _LockVehicle{"lockveh", "Lock Vehicle", "Locks their vehicle's doors against players"};
	static UnlockVehicle _UnlockVehicle{"unlockveh", "Unlock Vehicle", "Unlocks their vehicle's doors"};
}
