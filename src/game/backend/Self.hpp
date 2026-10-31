#pragma once
#include "game/gta/Entity.hpp"
#include "game/gta/Ped.hpp"
#include "game/gta/Player.hpp"
#include "game/gta/Vehicle.hpp"

namespace YimMenu
{
	class Self
	{
		Ped m_SelfPed;
		Player m_SelfPlayer;
		Vehicle m_Vehicle;
		bool m_IsDriver = false;

	public:
		static Ped GetPed()
		{
			return GetInstance().m_SelfPed;
		}

		static Player GetPlayer()
		{
			return GetInstance().m_SelfPlayer;
		}

		static Vehicle GetVehicle()
		{
			return GetInstance().m_Vehicle;
		}

		// whether we sit in the driver seat of GetVehicle(); cached on the game thread so network hooks can use it
		static bool IsDriver()
		{
			return GetInstance().m_IsDriver;
		}

		static void RunScript()
		{
			GetInstance().RunScriptImpl();
		}

		static void Update()
		{
			GetInstance().UpdateImpl();
		}

	private:
		Self();
		void RunScriptImpl();
		void UpdateImpl();

		static Self& GetInstance()
		{
			static Self Instance;
			return Instance;
		}
	};
}