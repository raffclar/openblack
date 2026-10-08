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

/// A poisoned living. The heal miracle cures it (the spell's default effect, event 5). Everything else about it is in
/// ECS/Life.h (ecs::life): who sets it (the poisoned food that reaches a villager, given or taken), the life it costs on
/// every periodic hunger check and the tint the drawing needs, k_PoisonDiffuse / k_PoisonSpecular (drawn by
/// RenderingSystem DrawColoursOf).
struct Poisoned
{
};

} // namespace openblack::ecs::components
