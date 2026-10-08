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

/// The object never catches fire: set by the script's SET_SET_ON_FIRE false, cleared by true and when a land is loaded
struct CannotBeSetOnFire
{
};

/// Burning does the object no harm: set by the script's SET_HURT_BY_FIRE false, cleared by true and when a land is
/// loaded
struct NotHurtByFire
{
};

} // namespace openblack::ecs::components
