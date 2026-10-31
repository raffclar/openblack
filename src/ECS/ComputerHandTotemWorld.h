/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

namespace openblack::computer_hand
{

/// What the hand reads and changes in the game when the god raises or lowers a totem. The totem steps add their calls
/// here.
class TotemWorld
{
public:
	virtual ~TotemWorld() = default;
	/// The hand of the god takes the totem, and the town of the totem takes the share of its people at worship. The creature
	/// of the town's player takes it that the player wants to impress, by half.
	virtual void RaiseTotem(entt::entity totem, float share) = 0;
};

} // namespace openblack::computer_hand
