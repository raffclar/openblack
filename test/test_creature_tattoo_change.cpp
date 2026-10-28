/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/CreatureTattooChange.h"

using namespace openblack;
using namespace openblack::ecs;
using components::CreatureTattoos;

namespace
{
constexpr auto k_Creature = static_cast<entt::entity>(5);

creature_tattoo::Slots StartSlots()
{
	creature_tattoo::Slots slots {};
	slots.at(0).site = 2;
	slots.at(0).design = 3;
	return slots;
}

player_commands::CreatureTattoo Change(bool withPictures)
{
	player_commands::CreatureTattoo change {.creature = k_Creature, .slots = StartSlots()};
	change.slots.at(1).site = 4;
	change.slots.at(1).design = 6;
	if (withPictures)
	{
		change.hasPictures = true;
		change.pictures = {1, 2, 3};
	}
	return change;
}

CreatureTattoos Brought()
{
	return CreatureTattoos {.slots = StartSlots(), .pictures = {9}};
}

/// Keeps the commands that go to the turn
struct Sent
{
	std::vector<player_commands::CreatureTattoo> commands;
	creature_tattoo_change::Submit Submit()
	{
		return [this](player_commands::CreatureTattoo command) { commands.push_back(std::move(command)); };
	}
};
} // namespace

TEST(CreatureTattooChange, AloneTheChangeGoesToTheTurnAndTheSkinShowsItUntilThen)
{
	auto tattoos = Brought();
	Sent sent;
	creature_tattoo_change::Apply(tattoos, Change(true), false, sent.Submit());
	ASSERT_EQ(sent.commands.size(), 1u);
	EXPECT_EQ(sent.commands.front(), Change(true));
	// The creature keeps its tattoos until the turn
	EXPECT_EQ(tattoos.slots, StartSlots());
	EXPECT_EQ(tattoos.pictures, std::vector<uint8_t>({9}));
	EXPECT_EQ(creature_tattoo_change::ShownSlots(tattoos), Change(true).slots);
	EXPECT_EQ(creature_tattoo_change::ShownPictures(tattoos), std::vector<uint8_t>({1, 2, 3}));
}

TEST(CreatureTattooChange, InANetworkGameTheChangeStaysOnThisComputer)
{
	auto tattoos = Brought();
	Sent sent;
	creature_tattoo_change::Apply(tattoos, Change(true), true, sent.Submit());
	// No command goes to the other computers, so they keep the tattoos that the creature brought
	EXPECT_TRUE(sent.commands.empty());
	EXPECT_EQ(tattoos.slots, StartSlots());
	EXPECT_EQ(tattoos.pictures, std::vector<uint8_t>({9}));
	// This computer shows the change
	EXPECT_EQ(creature_tattoo_change::ShownSlots(tattoos), Change(true).slots);
	EXPECT_EQ(creature_tattoo_change::ShownPictures(tattoos), std::vector<uint8_t>({1, 2, 3}));
}

TEST(CreatureTattooChange, AChangeWithNoPicturesKeepsTheShownPictures)
{
	auto tattoos = Brought();
	Sent sent;
	creature_tattoo_change::Apply(tattoos, Change(true), true, sent.Submit());
	creature_tattoo_change::Apply(tattoos, Change(false), true, sent.Submit());
	EXPECT_EQ(creature_tattoo_change::ShownPictures(tattoos), std::vector<uint8_t>({1, 2, 3}));
}

TEST(CreatureTattooChange, WithNoTurnTheCreatureGetsTheChangeAtOnce)
{
	auto tattoos = Brought();
	creature_tattoo_change::Apply(tattoos, Change(true), true, {});
	EXPECT_EQ(tattoos.slots, Change(true).slots);
	EXPECT_EQ(tattoos.pictures, std::vector<uint8_t>({1, 2, 3}));
	EXPECT_FALSE(tattoos.shown.has_value());
	EXPECT_EQ(creature_tattoo_change::ShownSlots(tattoos), Change(true).slots);
}
