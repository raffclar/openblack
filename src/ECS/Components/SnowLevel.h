/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

namespace openblack::ecs::components
{

/// The snow that a thing shows, 0 to 255. The level moves slowly towards the snow where the thing stands, but only while
/// the land draws the thing. A thing without this component shows no snow, as a new thing does.
struct SnowLevel
{
	int32_t level {0};
	/// The snow on the broken model of a broken building. It starts at the level of the building when the building
	/// breaks. While the building is broken, its own level does not change.
	std::optional<int32_t> broken;
	/// The broken model that the snow of the broken model is for, by the count of the broken models of the building
	uint32_t brokenModel {0};
	/// A piece that broke off a building. It keeps the snow of the broken model at the time when it broke off.
	bool piece {false};
	/// A tree that the game drew out of the land one time. From then on it carries less snow.
	bool drawnOutOfLand {false};
	/// A thing that carries no snow from now on: the phantom building of a scaffold that its site showed one time. Its
	/// level stays 0.
	bool noSnow {false};
};

} // namespace openblack::ecs::components
