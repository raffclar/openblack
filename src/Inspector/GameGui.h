/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "GameControls.h"

namespace openblack::inspector
{

/// The windows of the world and the game's own dialogs: its menu, the start-of-game question, the tattoo editor, the
/// boxes and the controls of its interface. The game's dialogs are clicked through the game's input, as the player
/// clicks them.
class GameGui final: public WorldGui
{
public:
	using WorldGui::WorldGui;

	[[nodiscard]] std::vector<WindowInfo> Windows() const override;
	std::string Open(std::string_view window) override;
	std::string Close(std::string_view window) override;
	std::string Press(std::string_view window, const std::vector<ButtonPathStep>& path) override;
	[[nodiscard]] OverlayInfo Overlays() const override;
};

} // namespace openblack::inspector
