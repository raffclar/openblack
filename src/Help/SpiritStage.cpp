/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpiritStage.h"

#include <glm/geometric.hpp>

#include "Gui/ScreenFit.h"

using namespace openblack;
using namespace openblack::help::spirits;

Stage help::spirits::StageOf(Screen screen)
{
	Stage stage {.screen = screen, .layout = screen, .left = 0, .share = 1.0f};
	const auto box = gui::FitInside(gui::k_FourByThree, screen.width, screen.height);
	// Only a screen wider than 4:3 gets an area of its own; a taller one keeps the game's whole-screen layout
	if (box.width < screen.width)
	{
		stage.layout.width = static_cast<uint16_t>(box.width);
		stage.left = box.x;
		stage.share = static_cast<float>(box.width) / static_cast<float>(screen.width);
	}
	return stage;
}

glm::vec3 help::spirits::ShrinkAcrossScreen(const glm::vec3& vector, const glm::vec3& forward, float share)
{
	if (share == 1.0f)
	{
		return vector;
	}
	return vector * share + forward * (glm::dot(vector, forward) * (1.0f - share));
}

glm::mat3 help::spirits::ShrinkAcrossScreen(const glm::mat3& axes, const glm::vec3& forward, float share)
{
	glm::mat3 shrunk = axes;
	for (glm::length_t i = 0; i < 3; ++i)
	{
		shrunk[i] = ShrinkAcrossScreen(axes[i], forward, share);
	}
	return shrunk;
}
