/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Registry.h"

namespace openblack::ecs
{

/// The registry of the things that only the frames make, move and remove, and that only the draw, the picks and the
/// tools read: the copy of a creature in a film, the marks over the disciples and the stones on a plinth. These things take no
/// entity numbers from the land's registry, so the synced world is the same on each computer at any frame rate. A thing of this
/// registry can refer to a thing of the land by its entity. A field of a thing of the land that keeps the entity of a thing of
/// this registry is not a part of the synced world.
class DrawnRegistry final: public Registry
{
};

} // namespace openblack::ecs
