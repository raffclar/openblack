/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "Gui/VillagerLabels.h"

namespace openblack::gui
{
class TextDatabase;
}

/// Which villagers show a label this frame, and what each says
namespace openblack::ecs::villager_label_source
{

struct Frame
{
	/// The screen in pixels
	glm::ivec2 screen {0};
	/// The names display's size
	float nameFade {0.0f};
	/// The game's help texts, which the details' words come from
	const gui::TextDatabase* texts {nullptr};
};

/// The labels of the villagers whose point over the head is on the screen, in the order the villagers come. None while
/// the cinema bars are in.
[[nodiscard]] std::vector<gui::villager_labels::Request> Gather(const Frame& frame);

} // namespace openblack::ecs::villager_label_source
