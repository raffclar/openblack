/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <optional>
#include <random>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleSymbolLayout.h"

using namespace openblack;
using namespace openblack::particles;
using namespace openblack::particles::draw;

namespace
{
/// Which sheets the sources know
enum class Sheets
{
	Both,
	NoGlow,
	NoSymbol,
	Neither,
	TheSame,
};

Sources MakeSources(Sheets sheets)
{
	Sources sources;
	sources.textures = [sheets](std::string_view texture) -> std::optional<std::pair<entt::id_type, entt::id_type>> {
		const bool glow = texture == "S_SpriteSheet3";
		if (sheets == Sheets::Neither || (glow && sheets == Sheets::NoGlow) || (!glow && sheets == Sheets::NoSymbol))
		{
			return std::nullopt;
		}
		if (sheets == Sheets::TheSame || glow)
		{
			return std::pair<entt::id_type, entt::id_type> {11, 12};
		}
		return std::pair<entt::id_type, entt::id_type> {21, 22};
	};
	sources.playerColour = [](int player) { return 0x102030u * static_cast<uint32_t>(player + 1); };
	sources.symbolsInARow = 8;
	return sources;
}

const Creator& PointCreator()
{
	static const Creator k_Point;
	return k_Point;
}

/// A walk of symbols at random places. Some symbols share a place, and some show a player of their own.
Effect::DrawWalk MakeWalk(const Creator& creator, size_t count, uint32_t seed)
{
	std::mt19937 random(seed);
	std::uniform_real_distribution<float> place(-50.0f, 50.0f);
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);
	Effect::DrawWalk walk;
	for (size_t i = 0; i < count; ++i)
	{
		Effect::DrawAtom atom {
		    .creator = &creator,
		    .position = {place(random), place(random), place(random)},
		    .rotation = glm::mat3(1.0f),
		    .scale = unit(random) * 3.0f,
		    .stretch = 1.0f,
		    .alpha = unit(random) * 300.0f,
		    .frame = 0.0f,
		    .rgb = {static_cast<uint8_t>(random()), static_cast<uint8_t>(random()), static_cast<uint8_t>(random())},
		    .age = unit(random) * 40.0f};
		if (i % 7 == 3 && i > 0)
		{
			atom.position = walk.atoms.back().position;
		}
		if (i % 5 == 1)
		{
			atom.player = static_cast<int>(i % 11);
		}
		// Some atoms only carry collections and draw nothing
		if (i % 4 == 2)
		{
			atom.creator = &PointCreator();
		}
		// The steps go in another order than the atoms
		walk.steps.push_back({.chain = false, .index = static_cast<uint32_t>(count - 1 - i)});
		walk.atoms.push_back(atom);
	}
	return walk;
}

/// Checks that the layout gives the commands and the sprites that the frame of the same walk gives
void ExpectSameAsFrame(const Creator& creator, const Effect::DrawWalk& walk, DrawPath path, Sheets sheets,
                       std::optional<glm::vec3> hand)
{
	const auto sources = MakeSources(sheets);
	const glm::vec3 origin {3.0f, -2.0f, 9.0f};
	const glm::vec3 camera {12.0f, 30.0f, -70.0f};
	constexpr int k_Player = -1;

	Frame frame;
	AddEffect(frame, walk, path, origin, k_Player, sources);
	std::vector<Command> frameCommands;
	std::vector<uint32_t> frameOrder;
	Order(frame, camera, hand, frameCommands, frameOrder);

	// A run before the run that is checked, whose commands are kept, moves the places of the checked run
	SymbolLayout layout;
	const auto other = MakeWalk(creator, 5, 99);
	const auto before = layout.Add(other, DrawPath::Sorted, origin, 2, MakeSources(Sheets::Both));
	const auto run = layout.Add(walk, path, origin, k_Player, sources);
	EXPECT_EQ(layout.Draws(run), !frame.Empty());
	std::vector<Command> commands;
	std::vector<uint32_t> places;
	SymbolOrderLists lists;
	layout.MakeSprites(before);
	OrderSymbols(layout, before, camera, hand, commands, places, lists);
	const auto firstCommand = commands.size();
	const auto firstPlace = places.size();
	layout.MakeSprites(run);
	OrderSymbols(layout, run, camera, hand, commands, places, lists);

	// The same box of the sprites, bit for bit
	const auto box = SpriteBox(frame);
	const auto& laidBox = layout.Run(run).spriteBox;
	ASSERT_EQ(box.has_value(), laidBox.has_value());
	if (box.has_value())
	{
		EXPECT_EQ(std::memcmp(&box->first, &laidBox->first, sizeof(glm::vec3)), 0);
		EXPECT_EQ(std::memcmp(&box->second, &laidBox->second, sizeof(glm::vec3)), 0);
	}

	ASSERT_EQ(commands.size() - firstCommand, frameCommands.size());
	ASSERT_EQ(places.size() - firstPlace, frameOrder.size());
	for (size_t i = 0; i < frameCommands.size(); ++i)
	{
		const auto& expected = frameCommands[i];
		const auto& actual = commands[firstCommand + i];
		EXPECT_EQ(actual.kind, expected.kind);
		EXPECT_EQ(actual.first - firstPlace, expected.first);
		EXPECT_EQ(actual.count, expected.count);
		EXPECT_EQ(actual.depth, expected.depth);
		ASSERT_TRUE(layout.Run(run).materials.at(actual.material).has_value());
		EXPECT_EQ(*layout.Run(run).materials.at(actual.material), frame.materials.at(expected.material));
	}
	for (size_t i = 0; i < frameOrder.size(); ++i)
	{
		const auto& expected = frame.sprites.at(frameOrder[i]);
		const auto& actual = layout.Sprite(places[firstPlace + i]);
		EXPECT_EQ(std::memcmp(&expected, &actual, sizeof(sprites::SpriteInstance)), 0) << "sprite " << i;
	}
}
} // namespace

TEST(ParticleSymbolLayout, EachPathAndEachSetOfSheetsGivesTheSameDrawsAsTheFrame)
{
	Creator creator;
	creator.kind = Creator::Kind::Symbol;
	const std::optional<glm::vec3> hand = glm::vec3 {-4.0f, 8.0f, 1.0f};
	// Few symbols sort by comparisons, and many symbols sort by their bytes
	for (const size_t count : {0U, 1U, 33U, 200U})
	{
		const auto walk = MakeWalk(creator, count, static_cast<uint32_t>(count) + 1);
		for (const auto path : {DrawPath::Sorted, DrawPath::Queued, DrawPath::Immediate, DrawPath::BeforeLand,
		                        DrawPath::AtOnceAfterLand, DrawPath::WithLearningWords})
		{
			for (const auto sheets : {Sheets::Both, Sheets::NoGlow, Sheets::NoSymbol, Sheets::Neither, Sheets::TheSame})
			{
				SCOPED_TRACE(testing::Message() << "count " << count << " path " << static_cast<int>(path) << " sheets "
				                                << static_cast<int>(sheets));
				ExpectSameAsFrame(creator, walk, path, sheets, hand);
				ExpectSameAsFrame(creator, walk, path, sheets, std::nullopt);
			}
		}
	}
}

TEST(ParticleSymbolLayout, OnlyAWalkOfPlainSymbolsGoesIntoTheLayout)
{
	Creator symbol;
	symbol.kind = Creator::Kind::Symbol;
	Creator sprite;
	sprite.kind = Creator::Kind::Sprite;
	auto walk = MakeWalk(symbol, 4, 7);
	EXPECT_TRUE(OnlySymbols(walk));
	EXPECT_TRUE(OnlySymbols(Effect::DrawWalk {}));
	// A sprite, a ribbon or a shower of sparks keeps the effect in a frame of its own
	walk.atoms[2].creator = &sprite;
	EXPECT_FALSE(OnlySymbols(walk));
	// An atom that only carries collections draws nothing
	walk.atoms[2].creator = &PointCreator();
	EXPECT_TRUE(OnlySymbols(walk));
	walk.atoms[2].creator = &symbol;
	walk.steps.push_back({.chain = true, .index = 0});
	EXPECT_FALSE(OnlySymbols(walk));
	walk.steps.pop_back();
	const ObjectShower shower {};
	walk.atoms[1].shower = &shower;
	EXPECT_FALSE(OnlySymbols(walk));
}
