/*******************************************************************************
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
/// The object is carried by a particle system (the storm's tornado, the vortex) between
/// physics::particle_carried_objects::Take and Release. It is IN_PHYSICS then with no body: out of the map cells, not hit, not
/// picked up (a tree's landing reads it).
struct CarriedByParticleSystem
{
};
} // namespace openblack::ecs::components
