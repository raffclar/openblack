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

#include "Enums.h"

namespace openblack::ecs::components
{

/// The miracle dispenser (an Abode): a building that makes a one-shot orb of its magic every `period` turns while the
/// last one has been taken
struct SpellDispenser
{
	uint32_t tick {0};
	uint32_t period {0};               ///< In game turns (0 = inactive)
	entt::entity oneShot {entt::null}; ///< The orb it made
	bool active {false};
	MagicType magicType {MagicType::None};
	uint32_t psys {0}; ///< Its effect (particle type 0x90, at the ground under it)
};

} // namespace openblack::ecs::components
