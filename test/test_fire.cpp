/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fire's graphic (ECS/Fire/FireGraphic): the charring grey, checked against the original's integer code for every
// charring byte.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <gtest/gtest.h>

#include "ECS/Components/MagicFireBall.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireGraphic.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/WorldSystems.h"

using namespace openblack::ecs;

namespace
{
/// The blue byte as the original computes it: -(k x 7 x 25), shifted right by 8, minus 1
uint8_t OriginalBlue(uint32_t k)
{
	const uint32_t ecx = 0u - k * 7u * 25u;
	return static_cast<uint8_t>((ecx >> 8) - 1u);
}
/// the green byte: the same value shifted left then right by 8, minus 0x100, its second byte
uint8_t OriginalGreen(uint32_t k)
{
	const uint32_t ecx = (((0u - k * 7u * 25u) << 8) >> 8) - 0x100u;
	return static_cast<uint8_t>(ecx >> 8);
}
/// the red byte: the same value shifted left by 16 then right by 8, minus 0x10000, bits 16..23
uint8_t OriginalRed(uint32_t k)
{
	const uint32_t eax = (((0u - k * 7u * 25u) << 16) >> 8) - 0x10000u;
	return static_cast<uint8_t>(eax >> 16);
}
} // namespace

TEST(FireGraphic, CharringGreyMatchesTheExe)
{
	fire::FireEffect effect;
	for (uint32_t k = 0; k < 256; ++k)
	{
		// a charring whose (x 255) truncated is exactly k
		effect.charring = static_cast<float>(k) / 255.0f;
		ASSERT_EQ(static_cast<uint32_t>(static_cast<int>(effect.charring * 255.0f) & 0xFF), k);
		const auto grey = fire::graphic::CharringGrey(effect);
		EXPECT_EQ(grey, OriginalBlue(k)) << "k " << k;
		EXPECT_EQ(grey, OriginalGreen(k)) << "k " << k;
		EXPECT_EQ(grey, OriginalRed(k)) << "k " << k;
	}
}

TEST(FireGraphic, FireballSteamsInsteadOfFlames)
{
	// the fire graphic's initial flags: all of bits 1-4 for an ordinary object; a MagicFireBall keeps only the steam
	// (bit 3)
	openblack::Locator::entitiesRegistry::emplace<openblack::ecs::Registry>();
	openblack::test::EmplaceWorldSystems();
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto rock = registry.Create();
	const auto ball = registry.Create();
	registry.Assign<components::MagicFireBall>(ball);
	EXPECT_EQ(fire::graphic::InitialFlags(rock), 0x1E);
	EXPECT_EQ(fire::graphic::InitialFlags(ball), 0x08);
	openblack::Locator::entitiesRegistry::reset();
	openblack::test::ResetWorldSystems();
}

TEST(FireGraphic, CharringGreyEnds)
{
	fire::FireEffect effect;
	effect.charring = 0.0f;
	EXPECT_EQ(fire::graphic::CharringGrey(effect), 255);
	effect.charring = 1.0f;
	EXPECT_EQ(fire::graphic::CharringGrey(effect), 80); // 255 - ceil(175 x 255 / 256)
}

TEST(FireEffect, NoPlayerUntilOneIsGiven)
{
	// a fire starts with no player (as the old hasPlayer false); a player given keeps its value
	fire::FireEffect effect;
	EXPECT_FALSE(effect.player.has_value());
	EXPECT_EQ(effect.player.value_or(openblack::PlayerNames::NEUTRAL), openblack::PlayerNames::NEUTRAL);
	effect.player = openblack::PlayerNames::PLAYER_TWO;
	EXPECT_EQ(effect.player.value_or(openblack::PlayerNames::NEUTRAL), openblack::PlayerNames::PLAYER_TWO);
}
