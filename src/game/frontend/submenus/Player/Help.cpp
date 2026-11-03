#include "Help.hpp"
#include "game/backend/PlayerData.hpp"
#include "game/backend/Players.hpp"

namespace YimMenu::Submenus
{
	std::shared_ptr<Category> BuildHelpMenu()
	{
		auto menu = std::make_shared<Category>("Help");

		auto give = std::make_shared<Group>("Give");
		auto vehicle = std::make_shared<Group>("Vehicle");
		auto loops = std::make_shared<Group>("Keep Them Going");

		give->AddItem(std::make_shared<PlayerCommandItem>("givehealth"_J));
		give->AddItem(std::make_shared<PlayerCommandItem>("givearmor"_J));
		give->AddItem(std::make_shared<PlayerCommandItem>("giveammo"_J));
		give->AddItem(std::make_shared<PlayerCommandItem>("giveweapons"_J));

		vehicle->AddItem(std::make_shared<PlayerCommandItem>("fixplayerveh"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("giftveh"_J));

		auto police = std::make_shared<Group>("Police and Radar");
		police->AddItem(std::make_shared<PlayerCommandItem>("clearplayerwanted"_J));
		police->AddItem(std::make_shared<PlayerCommandItem>("giveplayerotr"_J));

		// per player, reset when someone else takes the slot
		loops->AddItem(std::make_shared<ImGuiItem>([] {
			auto player = Players::GetSelected();
			if (!player.IsValid())
				return;

			auto& data = player.GetData();
			ImGui::Checkbox("Semi Godmode", &data.m_SemiGodmode);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Refills their health and armor when they drop. A big hit can still kill them");
			ImGui::Checkbox("Auto Fix Vehicle", &data.m_AutoFixVehicle);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Repairs their vehicle when it is damaged");
			ImGui::Checkbox("Never Wanted", &data.m_NeverWanted);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Clears their wanted level when they get one");
			ImGui::Checkbox("Off The Radar", &data.m_OffRadar);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Keeps them off the radar. Some menus flag this as griefing");
		}));

		menu->AddItem(give);
		menu->AddItem(vehicle);
		menu->AddItem(police);
		menu->AddItem(loops);

		return menu;
	}
}
