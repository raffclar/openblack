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

#include <array>
#include <functional>
#include <vector>

#include <glm/vec3.hpp>

#include "Particles/LiquidParticles.h"

/// The rules for the motes and the smoke that a creature makes because of its alignment. Each game turn, a creature
/// whose body looks good makes three light motes of pale colours, and a creature whose body looks evil makes three puffs
/// of dark smoke that rise. Each mote or puff starts near a random bone of the posed body. The further the body is from
/// neutral, the less transparent the motes and the puffs are. A body near neutral makes none.
namespace openblack::creature_alignment_motes
{
/// A body that looks more good than this makes motes, and a body that looks more evil than the negative makes smoke
constexpr float k_GoodAbove = 0.2f;
constexpr float k_EvilBelow = -0.2f;
/// The number of motes or puffs in each game turn
constexpr int32_t k_PerTurn = 3;
/// Each axis of the start point is a random bone's point plus (r - 50) times this times the shown size times
/// k_SpreadPerSize, r a random number from 0 to 100
constexpr int32_t k_SpreadRandomRange = 101;
constexpr int32_t k_SpreadRandomMiddle = 50;
constexpr float k_SpreadStep = 0.02f;
constexpr float k_SpreadPerSize = 0.75f;
/// The alpha of a mote is the alignment times this, and the alpha of a puff of smoke is the negative alignment times
/// this. The fraction is dropped.
constexpr float k_MoteAlphaPerAlignment = 32.0f;
constexpr float k_SmokeAlphaPerAlignment = 72.0f;
/// The size of a mote or a puff for each unit of the shown size
constexpr float k_MoteSizePerSize = 5.0f;
constexpr float k_SmokeSizePerSize = 2.0f;
/// The smoke rises at this speed. The motes do not move.
constexpr glm::vec3 k_SmokeVelocity {0.0f, 2.0f, 0.0f};
/// The colours (0xRRGGBB) that a random number from 0 to 5 selects
constexpr std::array<uint32_t, 6> k_MoteColours {0xFF9090u, 0x90FF90u, 0x9090FFu, 0xFFFF90u, 0x90FFFFu, 0xFF90FFu};
constexpr std::array<uint32_t, 6> k_SmokeColours {0x300000u, 0x003000u, 0x000030u, 0x303000u, 0x003030u, 0x300030u};

/// Gives a random whole number from 0 to less than a limit, from the local random numbers of the game
using LocalRandom = std::function<uint32_t(int32_t)>;
/// Gives the point of a bone of the posed body in the world, by its index
using BonePoint = std::function<glm::vec3(uint32_t)>;

/// One mote or puff of smoke, as the pool of liquid particles takes it
struct Puff
{
	glm::vec3 position {0.0f};
	glm::vec3 velocity {0.0f};
	float size {0.0f};
	/// 0xAARRGGBB
	uint32_t argb {0};
	liquid_particles::Kind kind {liquid_particles::Kind::Mote};
};

/// Whether a body of this shown alignment makes motes or smoke
[[nodiscard]] constexpr bool MakesAny(float alignment)
{
	return alignment > k_GoodAbove || alignment < k_EvilBelow;
}

/// The motes or the puffs of smoke that a creature makes in one game turn. The alignment is the alignment that the body
/// shows (-1 evil to 1 good). The bone count is the number of bones of the posed body. For each mote or puff, the random
/// numbers come in this order: the offsets on x, y and z, the bone, the colour and, for a mote only, the order of its
/// frames. A creature with no bones makes none.
[[nodiscard]] std::vector<Puff> PuffsOf(float alignment, float shownSize, uint32_t boneCount, const BonePoint& bonePoint,
                                        const LocalRandom& random);

} // namespace openblack::creature_alignment_motes
