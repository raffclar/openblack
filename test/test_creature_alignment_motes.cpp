/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <utility>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureAlignmentMotes.h"
#include "Physics/TurnRules.h"

using namespace openblack;
using namespace openblack::creature_alignment_motes;
using physics::turn::DustPuff;
using physics::turn::PuffKind;

namespace
{
/// Gives the queued numbers in order and records each limit it was asked for
struct ScriptedRandom
{
	std::vector<uint32_t> values;
	std::vector<int32_t> limits;
	size_t next {0};

	LocalRandom Get()
	{
		return [this](int32_t n) {
			limits.push_back(n);
			return next < values.size() ? values[next++] : 0u;
		};
	}
};

/// Bone i is at (i, 10 i, 100 i)
glm::vec3 BoneAt(uint32_t bone)
{
	const auto i = static_cast<float>(bone);
	return {i, 10.0f * i, 100.0f * i};
}
} // namespace

TEST(CreatureAlignmentMotes, ANearlyNeutralBodyMakesNone)
{
	ScriptedRandom random;
	EXPECT_TRUE(PuffsOf(0.2f, 1.0f, 30, BoneAt, random.Get()).empty());
	EXPECT_TRUE(PuffsOf(-0.2f, 1.0f, 30, BoneAt, random.Get()).empty());
	EXPECT_TRUE(PuffsOf(0.0f, 1.0f, 30, BoneAt, random.Get()).empty());
	EXPECT_TRUE(random.limits.empty());
	// A body with no bones makes none either
	EXPECT_TRUE(PuffsOf(1.0f, 1.0f, 0, BoneAt, random.Get()).empty());
}

TEST(CreatureAlignmentMotes, AGoodBodyMakesThreeStillPaleMotesNearRandomBones)
{
	ScriptedRandom random;
	// Per mote: x, y, z offsets, bone, colour, frame order
	random.values = {100, 50, 0, 3, 0, 1, 50, 50, 50, 7, 5, 0, 0, 0, 0, 0, 2, 1};
	const auto puffs = PuffsOf(0.5f, 2.0f, 30, BoneAt, random.Get());
	ASSERT_EQ(puffs.size(), 3u);
	const std::vector<int32_t> limits {101, 101, 101, 30, 6, 2, 101, 101, 101, 30, 6, 2, 101, 101, 101, 30, 6, 2};
	EXPECT_EQ(random.limits, limits);
	// Offsets of (r - 50) x 0.02 x 0.75 x size on each axis
	EXPECT_NEAR(puffs[0].position.x, 3.0f + 1.5f, 1e-5f);
	EXPECT_NEAR(puffs[0].position.y, 30.0f, 1e-5f);
	EXPECT_NEAR(puffs[0].position.z, 300.0f - 1.5f, 1e-4f);
	EXPECT_EQ(puffs[0].velocity, glm::vec3(0.0f));
	EXPECT_FLOAT_EQ(puffs[0].size, 10.0f);
	// Alpha 0.5 x 32 = 16
	EXPECT_EQ(puffs[0].argb, 0x10FF9090u);
	EXPECT_EQ(puffs[0].kind, PuffKind::Mote);
	EXPECT_EQ(puffs[1].position, BoneAt(7));
	EXPECT_EQ(puffs[1].argb, 0x10FF90FFu);
	EXPECT_EQ(puffs[1].kind, PuffKind::MoteReversed);
	EXPECT_EQ(puffs[2].argb, 0x109090FFu);
	EXPECT_EQ(puffs[2].kind, PuffKind::Mote);
}

TEST(CreatureAlignmentMotes, AnEvilBodyMakesThreeDarkRisingPuffs)
{
	ScriptedRandom random;
	// Per puff: x, y, z offsets, bone, colour
	random.values = {50, 50, 50, 1, 0, 50, 50, 50, 2, 3, 50, 50, 50, 4, 5};
	const auto puffs = PuffsOf(-1.0f, 1.5f, 10, BoneAt, random.Get());
	ASSERT_EQ(puffs.size(), 3u);
	const std::vector<int32_t> limits {101, 101, 101, 10, 6, 101, 101, 101, 10, 6, 101, 101, 101, 10, 6};
	EXPECT_EQ(random.limits, limits);
	EXPECT_EQ(puffs[0].position, BoneAt(1));
	EXPECT_EQ(puffs[0].velocity, glm::vec3(0.0f, 2.0f, 0.0f));
	EXPECT_FLOAT_EQ(puffs[0].size, 3.0f);
	// Alpha 1 x 72 = 0x48
	EXPECT_EQ(puffs[0].argb, 0x48300000u);
	EXPECT_EQ(puffs[0].kind, PuffKind::Smoke);
	EXPECT_EQ(puffs[1].argb, 0x48303000u);
	EXPECT_EQ(puffs[2].argb, 0x48300030u);
}

TEST(CreatureAlignmentMotes, TheAlphaDropsItsFraction)
{
	ScriptedRandom random;
	const auto good = PuffsOf(0.99f, 1.0f, 1, BoneAt, random.Get());
	ASSERT_FALSE(good.empty());
	EXPECT_EQ(good[0].argb >> 24u, 31u);
	const auto evil = PuffsOf(-0.21f, 1.0f, 1, BoneAt, random.Get());
	ASSERT_FALSE(evil.empty());
	EXPECT_EQ(evil[0].argb >> 24u, 15u);
}

TEST(CreatureAlignmentMotes, SmokeGrowsAndShrinksOverTwoSeconds)
{
	DustPuff puff {.size = 4.0f, .variant = 3, .kind = PuffKind::Smoke};
	EXPECT_FLOAT_EQ(physics::turn::DustPuffSize(puff), 0.0f);
	puff.age = 0.125f;
	EXPECT_NEAR(physics::turn::DustPuffSize(puff), 4.0f * (1.875f / 2.0f) * 0.5f, 1e-5f);
	puff.age = 1.0f;
	EXPECT_NEAR(physics::turn::DustPuffSize(puff), 2.0f, 1e-5f);
	EXPECT_EQ(physics::turn::DustPuffFrame(puff), 16 + 5);
	// It rises with its speed and lives two seconds
	puff.velocity = glm::vec3(0.0f, 2.0f, 0.0f);
	EXPECT_TRUE(physics::turn::AdvanceDustPuff(puff, 0.5f));
	EXPECT_FLOAT_EQ(puff.position.y, 1.0f);
	EXPECT_FALSE(physics::turn::AdvanceDustPuff(puff, 0.5f));
}

TEST(CreatureAlignmentMotes, AMotePulsesWhileItFadesAndPlaysItsFramesQuickly)
{
	DustPuff mote {.size = 5.0f, .variant = 14, .kind = PuffKind::Mote};
	EXPECT_FLOAT_EQ(physics::turn::DustPuffSize(mote), 0.0f);
	mote.age = 0.125f;
	const float growing = (std::sin(0.125f * glm::two_pi<float>()) + 2.0f) * 0.33f * 5.0f * (1.875f * 0.5f) * 0.5f;
	EXPECT_NEAR(physics::turn::DustPuffSize(mote), growing, 1e-5f);
	EXPECT_EQ(physics::turn::DustPuffFrame(mote), (14 + 1) & 15);
	mote.age = 1.5f;
	const float fading = (std::sin(1.5f * glm::two_pi<float>()) + 2.0f) * 0.33f * 5.0f * 0.25f;
	EXPECT_NEAR(physics::turn::DustPuffSize(mote), fading, 1e-5f);
	EXPECT_EQ(physics::turn::DustPuffFrame(mote), (14 + 12) & 15);
	// The other kind plays the same frames from the other end
	mote.kind = PuffKind::MoteReversed;
	EXPECT_EQ(physics::turn::DustPuffFrame(mote), 15 - ((14 + 12) & 15));
	EXPECT_TRUE(physics::turn::IsAdditive(mote.kind));
	EXPECT_FALSE(physics::turn::IsAdditive(PuffKind::Smoke));
	EXPECT_FLOAT_EQ(physics::turn::PuffLife(PuffKind::Mote), 2.0f);
	EXPECT_FLOAT_EQ(physics::turn::PuffLife(PuffKind::Dust), 1.0f);
}
