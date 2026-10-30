#include "core/hooking/DetourHook.hpp"
#include "game/backend/Detections.hpp"
#include "game/backend/NetRecorder.hpp"
#include "game/backend/SyncProtections.hpp"
#include "game/hooks/Hooks.hpp"

namespace YimMenu::Hooks
{
	// the setter that applies CVehicleControlDataNode (the vehicle is *(accessor - 0x1B0)). When m_IsSubmarineCar is set, it
	// writes m_RudderRotation to the vehicle's submarine part without a null check, which crashes on a vehicle that isn't a
	// submarine car. SyncProtections rejects that before it gets here; this is the backstop for when it can't check (no model
	// yet). Only that write is skipped: the flag is cleared, the rest of the node still applies
	void Network::ApplyVehicleControlData(std::uint8_t* accessor, std::uint8_t* node)
	{
		auto vehicle = accessor ? *reinterpret_cast<void**>(accessor - 0x1B0) : nullptr;
		if (vehicle && node && node[0x119])
		{
			using Getter = void* (*)(void* vehicle);
			auto getter = reinterpret_cast<Getter>((*reinterpret_cast<void***>(vehicle))[0x640 / 8]);
			if (!getter(vehicle))
			{
				node[0x119] = 0;

				auto sender = SyncProtections::GetCurrentSender();
				static std::chrono::steady_clock::time_point last_log{};
				auto now = std::chrono::steady_clock::now();
				if (now - last_log > std::chrono::seconds(10))
				{
					last_log = now;
					LOGF(WARNING, "Blocked submarine car crash (rudder data for a vehicle that isn't a submarine car) from {}", sender ? sender.GetName() : "unknown");
					NetRecorder::Dump("bad sync data");
				}
				if (sender)
					Detections::Add(sender, DetectionLevel::Certain, "submarine car data on a vehicle that isn't one");
			}
		}

		BaseHook::Get<Network::ApplyVehicleControlData, DetourHook<decltype(&Network::ApplyVehicleControlData)>>()->Original()(accessor, node);
	}
}
