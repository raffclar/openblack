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

#include "Enums.h"

namespace openblack::ecs::components
{

/// A villager that has died: the turns it lies dead before it goes, and whether its flesh has gone yet, leaving the
/// skeleton
struct VillagerDeath
{
	uint32_t turnsLeft {0};
	bool skeleton {false};
	/// What killed it, and the player put down for it, when its death says
	std::optional<DeathReason> reason;
	std::optional<PlayerNames> killer;
	/// It died of a fall that left it on its back: it falls and lies dead in its own clip for that
	bool onBack {false};
	/// The people about it have been told of the death, once, as it began to fall; never for a death at home
	bool noticed {false};
};

} // namespace openblack::ecs::components
