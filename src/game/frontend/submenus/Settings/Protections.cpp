#include "Protections.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/commands/Commands.hpp"

namespace YimMenu::Submenus
{
	// every toggle on this page with its default, in legacy's order
	static constexpr std::pair<joaat_t, bool> scriptEventProtections[] = {
	    {"blockbounty"_J, true},
	    {"blockceomoney"_J, true},
	    {"blockforcemission"_J, true},
	    {"blockforceteleport"_J, true},
	    {"blockgtabanner"_J, false},
	    {"blockmcteleport"_J, true},
	    {"blocksendtocutscene"_J, true},
	    {"blocksendtolocation"_J, true},
	    {"blocksoundspam"_J, true},
	    {"blockpersonalvehicledestroyed"_J, true},
	    {"blockremoteoffradar"_J, true},
	    {"blockrotatecam"_J, true},
	    {"blockteleporttowarehouse"_J, true},
	    {"blockstartactivity"_J, true},
	    {"blockstartscript"_J, true},
	    {"blockspectate"_J, true},
	    {"blockvehiclekick"_J, true},
	    {"blockclearwantedlevel"_J, true},
	    {"blockgivecollectible"_J, true},
	    {"blocksendsms"_J, true},
	};

	static constexpr std::pair<joaat_t, bool> otherProtections[] = {
	    {"blocktrafficmanipulation"_J, true},
	    {"blockbadsyncdata"_J, true},
	    {"blockcages"_J, true},
	    {"blockinjectedexplosions"_J, true},
	};

	static void SetAll(std::optional<bool> state)
	{
		using List = std::span<const std::pair<joaat_t, bool>>;
		for (auto list : {List(scriptEventProtections), List(otherProtections)})
			for (auto [hash, def] : list)
				if (auto command = Commands::GetCommand<BoolCommand>(hash))
					command->SetState(state.value_or(def));
	}

	std::shared_ptr<Category> BuildProtectionsMenu()
	{
		auto menu = std::make_shared<Category>("Protections");
		auto scriptEvents = std::make_shared<Group>("Script Events");
		auto other = std::make_shared<Group>("Other");
		auto options = std::make_shared<Group>("Options");
		auto detection = std::make_shared<Group>("Detection");
		auto reactions = std::make_shared<Group>("Reactions");

		for (auto [hash, def] : scriptEventProtections)
			scriptEvents->AddItem(std::make_shared<BoolCommandItem>(hash));
		for (auto [hash, def] : otherProtections)
			other->AddItem(std::make_shared<BoolCommandItem>(hash));

		detection->AddItem(std::make_shared<BoolCommandItem>("modderdetection"_J));
		detection->AddItem(std::make_shared<BoolCommandItem>("modderdetectiontrustfriends"_J));

		reactions->AddItem(std::make_shared<BoolCommandItem>("reactionsavemodders"_J));
		reactions->AddItem(std::make_shared<BoolCommandItem>("reactionhostkick"_J));
		reactions->AddItem(std::make_shared<BoolCommandItem>("reactionblockjoin"_J));
		reactions->AddItem(std::make_shared<BoolCommandItem>("reactionreclaimscripthost"_J));

		options->AddItem(std::make_shared<ImGuiItem>([] {
			if (ImGui::Button("Enable All"))
				SetAll(true);
			ImGui::SameLine();
			if (ImGui::Button("Disable All"))
				SetAll(false);
			ImGui::SameLine();
			if (ImGui::Button("Reset To Defaults"))
				SetAll(std::nullopt);
		}));

		menu->AddItem(scriptEvents);
		menu->AddItem(other);
		menu->AddItem(options);
		menu->AddItem(detection);
		menu->AddItem(reactions);
		return menu;
	}
}
