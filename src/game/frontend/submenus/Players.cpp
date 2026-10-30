#include "Players.hpp"

#include "Player/Info.hpp"
#include "Player/Toxic.hpp"
#include "Player/Troll.hpp"
#include "Player/Kick.hpp"
#include "core/frontend/widgets/imgui_colors.h"
#include "game/backend/Detections.hpp"
#include "game/backend/PlayerData.hpp"
#include "game/backend/Players.hpp"
#include "game/backend/SavedPlayers.hpp"
#include "game/frontend/items/Items.hpp"

namespace YimMenu::Submenus
{
	struct Tag
	{
		std::string Name;
		ImVec4 Color;
	};

	static std::vector<Tag> GetPlayerTags(YimMenu::Player player)
	{
		std::vector<Tag> tags;

		if (player.IsHost())
			tags.push_back({"HOST", ImGui::Colors::DeepSkyBlue});

		if (player.IsModder())
			tags.push_back({"MOD", ImGui::Colors::DeepPink});

		if (player.GetPed() && player.GetPed().IsInvincible())
			tags.push_back({"GOD", ImGui::Colors::Crimson});

		if (player.GetPed() && !player.GetPed().IsVisible() && false) // doesn't really seem to work on GTA
			tags.push_back({"INVIS", ImGui::Colors::MediumPurple});

		return tags;
	}

	static void DrawPlayerList(bool external = true, float offset = 15.0f)
	{
		struct ComparePlayerNames
		{
			bool operator()(YimMenu::Player a, YimMenu::Player b) const
			{
				std::string nameA = a.GetName();
				std::string nameB = b.GetName();
				return nameA < nameB;
			}
		};

		std::map<uint8_t, Player, ComparePlayerNames> sortedPlayers(YimMenu::Players::GetPlayers().begin(),
		    YimMenu::Players::GetPlayers().end());

		if (external)
		{
			ImGui::SetNextWindowPos(
			    ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x + offset, ImGui::GetWindowPos().y));
			ImGui::SetNextWindowSize(ImVec2(215, ImGui::GetWindowSize().y));
			ImGui::Begin("Player List", nullptr, ImGuiWindowFlags_NoDecoration);

			BoolCommandItem("spectate"_J).Draw();
			for (auto& [id, player] : sortedPlayers)
			{
				std::string display_name = player.GetName();

				ImGui::PushID(id);
				if (ImGui::Selectable(display_name.c_str(), (YimMenu::Players::GetSelected() == player)))
				{
					YimMenu::Players::SetSelected(id);
				}
				ImGui::PopID();

				// modders, and anyone with detections or notes (e.g. spectating) that didn't mark them
				if (ImGui::IsItemHovered())
				{
					auto detections = Detections::Describe(player.GetRID());
					if (detections.empty() && player.IsModder())
						if (auto saved = SavedPlayers::GetPlayerData(player.GetRID()))
							detections = saved->m_Detections;

					if (player.IsModder() || !detections.empty())
					{
						ImGui::BeginTooltip();
						if (detections.empty())
							ImGui::TextUnformatted("Marked as a modder in Saved Players");
						for (auto& detection : detections)
							ImGui::BulletText("%s", detection.c_str());
						ImGui::EndTooltip();
					}
				}

				auto tags = GetPlayerTags(player);

				auto old_item_spacing = ImGui::GetStyle().ItemSpacing.x;

				for (auto& tag : tags)
				{
					ImGui::SameLine();
					ImGui::PushStyleColor(ImGuiCol_Text, tag.Color);
					ImGui::Text("%s", ("[" + tag.Name + "]").c_str());
					ImGui::PopStyleColor();
					ImGui::GetStyle().ItemSpacing.x = 1;
				}

				ImGui::GetStyle().ItemSpacing.x = old_item_spacing;
			}
			ImGui::End();
		}
		else
		{
			if (ImGui::BeginCombo("Players", YimMenu::Players::GetSelected().GetName()))
			{
				for (auto& [id, player] : sortedPlayers)
				{
					if (ImGui::Selectable(player.GetName(), (YimMenu::Players::GetSelected() == player)))
					{
						YimMenu::Players::SetSelected(id);
					}
				}
				ImGui::EndCombo();
			}
		}
	}

	Players::Players() :
		#define ICON_FA_USERS "\xef\x83\x80"
	    Submenu::Submenu("Players", ICON_FA_USERS)
	{
		AddCategory(std::move(BuildInfoMenu()));
		AddCategory(std::move(BuildTrollMenu()));
		AddCategory(std::move(BuildToxicMenu()));
		AddCategory(std::move(BuildKickMenu()));

		for (auto& category : m_Categories)
			category->PrependItem(std::make_shared<ImGuiItem>([] {
				DrawPlayerList();
			}));
	}
}