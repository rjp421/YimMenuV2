#include "Info.hpp"

#include "core/frontend/Notifications.hpp"
#include "core/backend/FiberPool.hpp"
#include "game/backend/PlayerData.hpp"
#include "game/backend/Players.hpp"
#include "game/backend/SavedPlayers.hpp"
#include "game/backend/Self.hpp"
#include "game/gta/Natives.hpp"
#include "types/network/CNetGamePlayer.hpp"
#include "types/network/rlGamerInfo.hpp"
#include "types/script/globals/GPBD_FM.hpp"
#include "types/script/globals/GPBD_FM_3.hpp"

namespace YimMenu::Submenus
{
	static std::string BuildIPStr(int field1, int field2, int field3, int field4)
	{
		std::ostringstream oss;
		oss << field1 << '.' << field2 << '.' << field3 << '.' << field4;
		return oss.str();
	}

	static const char* GetNatTypeName(std::uint32_t type)
	{
		switch (type)
		{
		case 1: return "Open";
		case 2: return "Moderate";
		case 3: return "Strict";
		}

		return "Unknown";
	}

	// each player's game fills in their own text, so it may not end where it should
	template<int SIZE>
	static std::string ReadLabel(const SCR_TEXT_LABEL<SIZE>& label)
	{
		return std::string(label.Data, strnlen(label.Data, SIZE));
	}

	static void DrawNameLine(const char* title, const std::string& name)
	{
		ImGui::Text("%s: %s", title, name.empty() ? "None" : name.c_str());
	}

	static void DrawScriptInfo(Player player)
	{
		int id = player.GetId();

		ImGui::Text("Language: %s", player.GetLanguageName());

		if (auto fm3 = GPBD_FM_3::Get())
		{
			auto& bossGoon = fm3->Entries[id].BossGoon;
			DrawNameLine("CEO Name", ReadLabel(bossGoon.GangName));

			// legacy reads ClubhouseName, which the struct marks as possibly unused
			auto mcName = ReadLabel(bossGoon.MCName);
			DrawNameLine("MC Name", mcName.empty() ? ReadLabel(bossGoon.ClubhouseName) : mcName);
		}

		if (auto fm = GPBD_FM::Get())
		{
			auto& entry = fm->Entries[id];
			auto& stats = entry.PlayerStats;

			ImGui::Text("Wallet: %d", stats.WalletBalance);
			ImGui::Text("Bank: %d", stats.Money - stats.WalletBalance);
			ImGui::Text("Total Money: %d", stats.Money);
			ImGui::Text("Rank: %d (%d RP)", stats.Rank, stats.RP);
			ImGui::Text("K/D Ratio: %.2f", stats.KdRatio);
			ImGui::Text("Kills On Players: %d", stats.KillsOnPlayers);
			ImGui::Text("Deaths By Players: %d", stats.DeathsByPlayers);
			ImGui::Text("Prostitutes Frequented: %d", stats.ProstitutesFrequented);
			ImGui::Text("Lap Dances Bought: %d", stats.LapDancesBought);
			ImGui::Text("Missions Created: %d", stats.MissionsCreated);
			ImGui::Text("Meltdown Complete: %s", entry.MeltdownComplete ? "Yes" : "No");
		}
	}

	static void DrawNetworkInfo(Player player)
	{
		ImGui::Text("Average Latency: %.2f", player.GetAverageLatency());
		ImGui::Text("Packet Loss: %.2f", player.GetAveragePacketLoss());

		if (auto gamerInfo = player.GetGamerInfo())
		{
			ImGui::Text("NAT Type: %s", GetNatTypeName(gamerInfo->m_NatType));

			auto hostToken = std::format("{:X}", gamerInfo->m_HostToken);
			ImGui::Text("Host Token:");
			ImGui::SameLine();
			if (ImGui::SmallButton(hostToken.c_str()))
				ImGui::SetClipboardText(hostToken.c_str());
		}
	}

	std::shared_ptr<Category> BuildInfoMenu()
	{
		auto menu = std::make_shared<Category>("Info");

		auto teleportGroup = std::make_shared<Group>("Teleport");
		auto playerOptionsGroup = std::make_shared<Group>("Info");

		playerOptionsGroup->AddItem(std::make_shared<ImGuiItem>([] {
			if (Players::GetSelected().IsValid())
				ImGui::Text("%s", Players::GetSelected().GetName());
		}));
		playerOptionsGroup->AddItem(std::make_shared<BoolCommandItem>("spectate"_J));
		playerOptionsGroup->AddItem(std::make_shared<ImGuiItem>([] {
			if (Players::GetSelected().IsValid())
			{
				ImGui::Text("Rank: %d (%d RP)", Players::GetSelected().GetRank(), Players::GetSelected().GetRP());
				ImGui::Text("Money: %d", Players::GetSelected().GetMoney());

				// read on the game thread by Players::RunScript
				if (auto& data = Players::GetSelected().GetData(); data.m_HasPed)
				{
					auto health = data.m_Health;
					auto maxHealth = data.m_MaxHealth;
					std::string healthStr = std::format("HP: {}/{} ({:.2f}%)", health, maxHealth, maxHealth ? (float)health / maxHealth * 100.0f : 0.0f);
					ImGui::Text("%s", healthStr.c_str());

					auto coords = data.m_Position;
					ImGui::Text("Coords: %.2f, %.2f, %.2f", coords.x, coords.y, coords.z);

					ImGui::Text("Distance: %.2f", data.m_Distance);
				}
				else
				{
					ImGui::Text("Ped missing or deleted");
				}

				auto rid1 = Players::GetSelected().GetRID();

				std::string ridStr = std::to_string(rid1);

				ImGui::Text("RID:");
				ImGui::SameLine();
				if (ImGui::SmallButton(std::to_string(rid1).c_str()))
				{
					ImGui::SetClipboardText(std::to_string(rid1).c_str());
				}

				auto& platformAccountId = Players::GetSelected().GetHandle()->m_PlatformAccountId;
				switch (platformAccountId.m_Platform)
				{
				case PlatformAccountId::PLATFORM_XBOX:
					ImGui::Text("Xbox User ID:");
					ImGui::SameLine();
					if (ImGui::SmallButton(std::to_string(platformAccountId.m_XboxUserId).c_str()))
					{
						ImGui::SetClipboardText(std::to_string(platformAccountId.m_XboxUserId).c_str());
					}
					break;
				case PlatformAccountId::PLATFORM_STEAM:
					ImGui::Text("Steam ID:");
					ImGui::SameLine();
					if (ImGui::SmallButton(std::to_string(platformAccountId.m_SteamId).c_str()))
					{
						ImGui::SetClipboardText(std::to_string(platformAccountId.m_SteamId).c_str());
					}
					break;
				case PlatformAccountId::PLATFORM_EPIC:
					ImGui::Text("Epic Account ID:");
					ImGui::SameLine();
					if (ImGui::SmallButton(platformAccountId.m_EpicAccountId))
					{
						ImGui::SetClipboardText(platformAccountId.m_EpicAccountId);
					}
					break;
				default:
					break;
				}


				auto ip = Players::GetSelected().GetExternalAddress();

				auto addr2 = BuildIPStr(ip.m_IpAddress.m_Field1, ip.m_IpAddress.m_Field2, ip.m_IpAddress.m_Field3, ip.m_IpAddress.m_Field4);

				ImGui::Text("IP Address:");
				ImGui::SameLine();
				if (ImGui::SmallButton(addr2.c_str()))
				{
					ImGui::SetClipboardText(addr2.c_str());
				}

				if (ImGui::Button("Add to Saved"))
					FiberPool::Push([player = Players::GetSelected()] {
						SavedPlayers::GetPlayerData(player);
					});
				ImGui::SameLine();
				if (ImGui::Button("View SC Profile"))
					FiberPool::Push([] {
						uint64_t handle[13];
						NETWORK::NETWORK_HANDLE_FROM_PLAYER(Players::GetSelected().GetId(), handle, std::size(handle));
						NETWORK::NETWORK_SHOW_PROFILE_UI(handle);
					});
				ImGui::SameLine();
				if (ImGui::Button("Add Friend"))
					FiberPool::Push([] {
						uint64_t handle[13];
						NETWORK::NETWORK_HANDLE_FROM_PLAYER(Players::GetSelected().GetId(), handle, std::size(handle));
						NETWORK::NETWORK_ADD_FRIEND(handle, "");
					});

				if (ImGui::Button("More Info"))
					ImGui::OpenPopup("More Info");

				ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
				if (ImGui::BeginPopupModal("More Info", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::BeginGroup();
					DrawScriptInfo(Players::GetSelected());
					ImGui::EndGroup();

					ImGui::SameLine(0.0f, 30.0f);

					ImGui::BeginGroup();
					DrawNetworkInfo(Players::GetSelected());
					ImGui::EndGroup();

					ImGui::Spacing();

					if (ImGui::Button("Close") || ((!ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
				}
			}
			else
			{
				Players::SetSelected(Self::GetPlayer());
				ImGui::Text("No players yet!");
			}
		}));

		menu->AddItem(playerOptionsGroup);

		
		auto customPlayerTp = std::make_shared<Group>("", 1);
		customPlayerTp->AddItem(std::make_shared<Vector3CommandItem>("playertpcoord"_J, ""));
		customPlayerTp->AddItem(std::make_shared<PlayerCommandItem>("tpplayertocoord"_J, "Teleport"));
		auto tpToProperty = std::make_shared<Group>("", 1);
		tpToProperty->AddItem(std::make_shared<ListCommandItem>("sendtopropertyindex"_J, "##selproperty"));
		tpToProperty->AddItem(std::make_shared<PlayerCommandItem>("sendtoproperty"_J));
		auto tpToInterior = std::make_shared<Group>("", 1);
		tpToInterior->AddItem(std::make_shared<ListCommandItem>("sendtointeriorindex"_J, "##selinterior"));
		tpToInterior->AddItem(std::make_shared<PlayerCommandItem>("sendtointerior"_J));
		teleportGroup->AddItem(tpToProperty);
		teleportGroup->AddItem(tpToInterior);
		auto tpToPlayer = std::make_shared<Group>("", 1);
		tpToPlayer->AddItem(std::make_shared<PlayerCommandItem>("tptoplayer"_J, "Teleport To"));
		tpToPlayer->AddItem(std::make_shared<PlayerCommandItem>("tpintoveh"_J));
		teleportGroup->AddItem(tpToPlayer);
		teleportGroup->AddItem(std::make_shared<PlayerCommandItem>("bring"_J));
		teleportGroup->AddItem(customPlayerTp);

		menu->AddItem(teleportGroup);

		return menu;
	}
}