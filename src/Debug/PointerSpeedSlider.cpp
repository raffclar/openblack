/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PointerSpeedSlider.h"

#include <cmath>

#include <imgui.h>

#include "Input/GameActionMapInterface.h"
#include "Input/PointerSpeed.h"
#include "Locator.h"

void openblack::debug::gui::DrawPointerSpeedSlider()
{
	if (!Locator::gameActionSystem::has_value())
	{
		return;
	}
	auto& actions = Locator::gameActionSystem::value();
	float multiplier = actions.GetPointerSpeedMultiplier();
	if (ImGui::SliderFloat("Pointer speed", &multiplier, input::pointer_speed::k_MinMultiplier,
	                       input::pointer_speed::k_MaxMultiplier, "%.2f"))
	{
		// In steps of a twentieth, from the next movement of the mouse on
		actions.SetPointerSpeedMultiplier(std::round(multiplier * 20.0f) / 20.0f);
	}
	ImGui::SetItemTooltip("How fast the pointer moves against the speed matched to the screen (0.5 by default)");
}
