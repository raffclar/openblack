/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The temples' outsides: each turn every temple's look moves toward its player's alignment and share
/// of influence, and its mesh and texture are blended afresh for them as they change
class TempleExteriorSystemInterface
{
public:
	virtual ~TempleExteriorSystemInterface() = default;

	/// Each turn also lets a player's temple entrances take the cursor from the first turn a temple of theirs stands fully
	/// built
	virtual void UpdateTurn() = 0;
	/// At the start of a player's turn, each temple of the player takes the size that it grows toward: twice the share
	/// of the influence that the player has at that time
	/// @param share The player's share of the influence, nothing when nobody has any
	virtual void NoteSizeTarget(PlayerNames player, std::optional<float> share) = 0;
	/// After a load of a saved game: makes each temple's blended mesh again for the look that the save kept
	virtual void FindAfterLoad() = 0;
};

} // namespace openblack::ecs::systems
