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

#include <entt/entity/entity.hpp>

#include "Common/Zoomer.h"
#include "Enums.h"

namespace openblack::ecs::components
{

struct Pot
{
	uint16_t amount;
	uint16_t maxAmount;
	PotInfo type = PotInfo::_COUNT;
	bool poisoned = false;
	bool speedUp = false;
	entt::entity speedUpVisual = entt::null; ///< The PILEFOOD_SPEEDUP spot visual container
	/// The pile's own player: the player whose miracle or hand made a magic pile, else the neutral player. A pile of a
	/// storage pit belongs to the pit's player instead (object_resources::PlayerOfPile)
	PlayerNames owner = PlayerNames::NEUTRAL;
};

// A pile's sink offset: piles rise out of / sink into the ground instead of scaling. A change of amount moves the
// offset to its new target in 1 s (the same quartic as the Zoomer).
struct PileSink
{
	float baseY;
	float height = 1.0f;
	openblack::Zoomer offset {};
};

// Texture offset of the object (u, v; graphics::frame_anim::UvOffset), added by the draw to the primitives whose
// material lacks flag bit 0x10. The food pile's draw scrolls the grain of the storage pit and magic food piles by
// v = 0.25 * sink / height, so that the grain stays put in the world and the pile seems to shrink; the one-off spell
// seeds step the orbs' 4x4 texture (u and v in quarters); the designed waterfall scrolls the water's v; the spell seed
// graphic steps the creature spell phials' 8 x 4 texture (u and v in eighths).
struct UvScroll
{
	float v = 0.0f;
	float u = 0.0f; ///< in 1/256 steps (the renderer packs it with v, frame_anim::PackUvOffset)
};

} // namespace openblack::ecs::components
