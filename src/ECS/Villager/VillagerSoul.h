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

#include <entt/entity/entity.hpp>

namespace openblack
{
enum class MeshId : uint32_t;
}

// The soul of a dead villager, as the original draws it: made when the villager dies, on its first DEAD turn out of
// the water: a translucent white copy of the villager
// playing its "goto heaven / hell" clip (the clip moves it), alpha 105, fading out in its last 500 ms. In openblack an
// entity with a Transform, Mesh, SkeletalAnimation (the clip), Alpha (the translucent pass), ObjectColour (white) and
// components::VillagerSoul; no Villager, so nothing else treats it as one.

namespace openblack::ecs::components
{
struct VillagerSoul
{
	uint32_t elapsedMs {0};  ///< Time since the soul was made
	uint32_t durationMs {0}; ///< The clip's duration
};
} // namespace openblack::ecs::components

namespace openblack::ecs::villager_soul
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// Random(0, 100) (the CRT stream, drawn always) < 50 -> heaven, else hell; the source's current clip name
/// "M_P_DEAD1" -> the pair (247 P_DEAD2_GOTO_HEAVEN, 248 P_DEAD2_GOTO_HELL), else (244 P_DEAD1_GOTO_HEAVEN,
/// 245 P_DEAD1_GOTO_HELL) (literal: the pair looks inverted); heavenForced (a child) -> the heaven clip after the roll
[[nodiscard]] int32_t SoulClip(bool sourceIsDead1Name, bool heavenForced, float roll);
/// 105; in the last 500 ms (elapsed > duration - 500, signed) (1 - (elapsed - (duration - 500)) x 0.002) x 105,
/// truncated toward zero
[[nodiscard]] uint8_t SoulAlpha(uint32_t elapsed, uint32_t duration);
/// elapsed + 110 > duration (signed) -> the soul is freed
[[nodiscard]] bool SoulExpired(uint32_t elapsed, uint32_t duration);

// ---- the souls ---------------------------------------------------------------------------------------------------

/// The soul at the source's position and orientation, mesh = MeshPack[mesh], its clip (SoulClip); returns the clip
/// (-1 when nothing was made)
int32_t Create(entt::entity source, MeshId mesh, bool heavenForced);
/// Per soul, once per frame in the original: the time, the alpha, the deletion. (approximate) openblack calls it once
/// per game turn with the turn's 100 ms (LivingActionSystem); the clip's time runs per frame with the animations in
/// between. Exact: a frame hook in Game.cpp (pending)
void Update(uint32_t milliseconds);
} // namespace openblack::ecs::villager_soul
