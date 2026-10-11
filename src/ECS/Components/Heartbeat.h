/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Heartbeat.h"

namespace openblack::ecs::components
{

/// A player's heartbeat, on the player's entity: how fast their heart beats with the danger their people are in, and
/// where it is in its beat
struct Heartbeat
{
	heartbeat::State state;
};

} // namespace openblack::ecs::components
