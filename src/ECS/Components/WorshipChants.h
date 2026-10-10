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

namespace openblack::ecs::components
{

/// The prayer power of a worship site, on the site's entity. Each turn the villagers dancing at the site chant prayer
/// power into it; its spell icons and the miracles cast from them draw on what the dancers chant that turn and on a
/// battery that stores what is left over.
struct WorshipChants
{
	float battery {0.0f};         ///< stored prayer power
	float available {0.0f};       ///< what can be drawn this turn: the battery and the dancers' chanting
	float used {0.0f};            ///< drawn so far this turn
	float requested {0.0f};       ///< asked for so far this turn, which may be more than was drawn
	float chantsPerDancer {0.0f}; ///< what each dancer chanted last turn, which tires them
	float danceIntensity {0.0f};  ///< how fast the dance goes, 0 to 1
	float strain {0.0f};          ///< how far demand outstrips the dancers: (requested - capacity) / capacity
	bool infinite {false};        ///< a cheat: the site never runs dry
	bool freeMaintenance {false}; ///< a cheat: maintaining miracles costs nothing
	/// The villagers dancing at the site, who chant; those hiding at its door don't
	uint32_t dancers {0};
};

} // namespace openblack::ecs::components
