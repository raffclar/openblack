/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::computer_hand
{

/// What the hand reads and changes in the game when the god gives orders to its creature, plays with it or leashes it.
/// The steps of the creature add their calls here.
class CreatureWorld
{
public:
	virtual ~CreatureWorld() = default;
};

} // namespace openblack::computer_hand
