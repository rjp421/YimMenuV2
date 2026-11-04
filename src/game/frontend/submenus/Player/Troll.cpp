#include "Troll.hpp"

namespace YimMenu::Submenus
{
	std::shared_ptr<Category> BuildTrollMenu()
	{
		auto menu = std::make_shared<Category>("Troll");

		auto general = std::make_shared<Group>("General");
		auto wanted = std::make_shared<Group>("Wanted", 1);
		auto bounty = std::make_shared<Group>("Bounty", 1);
		auto vehicle = std::make_shared<Group>("Vehicle");

		general->AddItem(std::make_shared<PlayerCommandItem>("sendsext"_J));
		general->AddItem(std::make_shared<PlayerCommandItem>("sendphantomcar"_J));
		general->AddItem(std::make_shared<PlayerCommandItem>("sendgooch"_J));
		general->AddItem(std::make_shared<PlayerCommandItem>("ragdoll"_J));
		general->AddItem(std::make_shared<PlayerCommandItem>("removeweapons"_J));

		wanted->AddItem(std::make_shared<IntCommandItem>("trollwantedlevel"_J, "Level"));
		wanted->AddItem(std::make_shared<PlayerCommandItem>("playersetwanted"_J));

		bounty->AddItem(std::make_shared<IntCommandItem>("bountyamount"_J, "Amount"));
		bounty->AddItem(std::make_shared<BoolCommandItem>("anonymousbounty"_J, "Anonymous"));
		bounty->AddItem(std::make_shared<PlayerCommandItem>("setbounty"_J));

		vehicle->AddItem(std::make_shared<PlayerCommandItem>("deleteveh"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("vehkick"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("killengine"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("bursttires"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("smashwindows"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("stopveh"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("boostveh"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("launchveh"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("flip180"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("opendoors"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("closedoors"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("lockveh"_J));
		vehicle->AddItem(std::make_shared<PlayerCommandItem>("unlockveh"_J));

		menu->AddItem(general);
		menu->AddItem(wanted);
		menu->AddItem(bounty);
		menu->AddItem(vehicle);

		return menu;
	}
}