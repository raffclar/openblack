/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{

/// The animals' clips (docs/bw1-notes/animation.md, animals.md): an animal's clip always comes from the species'
/// function of the state (the animal state table, no info.dat fallback); -1 keeps the clip playing.
/// There are no into / out-of clips for animals.

/// The ANM_ index for the animal's top state, -1 = keep the current clip
[[nodiscard]] int32_t AnimalAnimId(entt::entity entity);
/// Cut to that clip (from 0 with `reset`); the same clip is not restarted
void SetAnimalAnim(entt::entity entity, int32_t clip, bool reset);
/// The state's clip, on every top state change
void SetAnimalStateAnim(entt::entity entity);

/// Every frame: new animals get their clip; moving ones advance it with the ground covered
void UpdateAnimalAnimations();

} // namespace openblack::ecs
