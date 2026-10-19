/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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
	/// After a load: the models that the game made for these things while it played are made again. A save keeps the
	/// names of these models but not the models, and the game that loads it may not have them.
	virtual void RemakeModelsAfterLoad() {}
};

} // namespace openblack::ecs::systems
