#pragma once

#include "core/frontend/manager/Category.hpp"
#include "game/frontend/items/Items.hpp"

namespace YimMenu
{
	std::shared_ptr<Category> DrawGUISettingsMenu();
	void InitializeColorCommands();
	void SyncColorCommandsToStyle();
	void LoadSettings();
	void SaveSettings();
	void ApplyThemeToImGui();
}
