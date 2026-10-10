/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// A thing in a state that keeps it out of sight, as a villager inside a building or slaughtering out of view, or a
/// script's scroll while the scripts hide the scrolls: it isn't drawn while it is
struct HiddenByState
{
};

} // namespace openblack::ecs::components
