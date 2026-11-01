/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <limits>
#include <optional>

#include <MindFile.h>
#include <gtest/gtest.h>

#include "Creature/CreatureMindFileBody.h"
#include "Creature/CreatureTattoo.h"

using namespace openblack;
using namespace openblack::creature_mind_body;

TEST(CreatureMindFileBody, SpeciesRowsStartWithTheGiantApe)
{
	EXPECT_EQ(SpeciesFromRow(0), CreatureType::GiantApe);
	EXPECT_EQ(SpeciesFromRow(1), CreatureType::Cow);
	EXPECT_EQ(SpeciesFromRow(4), CreatureType::Wolf);
	EXPECT_EQ(SpeciesFromRow(16), CreatureType::Gorilla);
	EXPECT_FALSE(SpeciesFromRow(17).has_value());
	EXPECT_FALSE(SpeciesFromRow(1013008280).has_value());
}

TEST(CreatureMindFileBody, TakesTheBodyAFileKeeps)
{
	creaturemind::MindFileData file;
	file.speciesRow = 3;
	file.name = u"Spot";
	file.alignment = -0.75f;
	file.physique.strength = 0.6f;
	file.physique.size = 1.5f;
	file.tattooHeader = std::array<uint32_t, 8> {0x04080C21, 0x000000F0, 0, 0, 0, 0, 0, 0};
	const auto body = FromMindFile(file);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->species, CreatureType::Leopard);
	EXPECT_EQ(body->name, u"Spot");
	EXPECT_EQ(body->alignment, -0.75f);
	EXPECT_EQ(body->strength, 0.6f);
	EXPECT_EQ(body->size, 1.5f);
	ASSERT_TRUE(body->tattoos.has_value());
	const auto& first = body->tattoos->at(0);
	EXPECT_EQ(first.design, 1);
	EXPECT_EQ(first.site, 2);
	EXPECT_EQ(first.colour.r, 0x04);
	EXPECT_EQ(first.colour.g, 0x08);
	EXPECT_EQ(first.colour.b, 0x0C);
	EXPECT_TRUE(body->tattoos->at(1).Empty());
}

TEST(CreatureMindFileBody, OlderFilesLeaveTheRestToTheSpecies)
{
	creaturemind::MindFileData file;
	file.version = 10;
	file.speciesRow = 0;
	file.physique.strength = 0.4f;
	const auto body = FromMindFile(file);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->species, CreatureType::GiantApe);
	EXPECT_FALSE(body->alignment.has_value());
	EXPECT_FALSE(body->size.has_value());
	EXPECT_FALSE(body->tattoos.has_value());
}

TEST(CreatureMindFileBody, ValuesAreKeptAsTheFileHasThem)
{
	// As the game does, only the alignment is kept from -1 to 1. A value that is not a number is not taken.
	creaturemind::MindFileData file;
	file.speciesRow = 2;
	file.alignment = 3.0f;
	file.physique.strength = std::numeric_limits<float>::quiet_NaN();
	file.physique.size = -1.0f;
	file.physique.fatness = 1.5f;
	const auto body = FromMindFile(file);
	ASSERT_TRUE(body.has_value());
	EXPECT_EQ(body->alignment, 1.0f);
	EXPECT_EQ(body->strength, 0.5f);
	EXPECT_EQ(body->size, std::optional<float>(-1.0f));
	EXPECT_EQ(body->fatness, 1.5f);
}

TEST(CreatureMindFileBody, TheFileKeepsThePooTheDehydrationAndTheItchiness)
{
	// The six further values of the body: the poo first, the exhaustion second and the dehydration third
	creaturemind::MindFileData file;
	file.speciesRow = 2;
	file.physique.needs = {0.3f, 0.25f, 0.8f, 7.0f, 8.0f, 9.0f};
	file.physique.scratch = 0.6f;
	const auto body = FromMindFile(file);
	ASSERT_TRUE(body.has_value());
	ASSERT_TRUE(body->needs.has_value());
	EXPECT_EQ(body->needs->poo, 0.3f);
	EXPECT_EQ(body->needs->exhaustion, 0.25f);
	EXPECT_EQ(body->needs->dehydration, 0.8f);
	EXPECT_EQ(body->needs->itchiness, 0.6f);

	// The body writes the same values back to the same places of the file
	creaturemind::MindFileData written;
	ToMindFile(*body, written);
	EXPECT_EQ(written.physique.needs.at(0), 0.3f);
	EXPECT_EQ(written.physique.needs.at(1), 0.25f);
	EXPECT_EQ(written.physique.needs.at(2), 0.8f);
	EXPECT_EQ(written.physique.scratch, std::optional<float>(0.6f));
}

TEST(CreatureMindFileBody, AFileOlderThanTheItchinessLeavesItAtNone)
{
	creaturemind::MindFileData file;
	file.speciesRow = 2;
	file.physique.scratch = std::nullopt;
	const auto body = FromMindFile(file);
	ASSERT_TRUE(body.has_value());
	ASSERT_TRUE(body->needs.has_value());
	EXPECT_EQ(body->needs->itchiness, 0.0f);
}

TEST(CreatureMindFileBody, UnknownSpeciesIsNoCreature)
{
	creaturemind::MindFileData file;
	file.speciesRow = 99;
	EXPECT_FALSE(FromMindFile(file).has_value());
}

TEST(CreatureMindFileBody, TattooWordsRoundTrip)
{
	const creature_tattoo::Slot slot {.design = 13, .site = 5, .colour = {0xAB, 0xCD, 0xEF}};
	EXPECT_EQ(creature_tattoo::ToWord(slot), 0xABCDEF5Du);
	EXPECT_EQ(creature_tattoo::FromWord(creature_tattoo::ToWord(slot)), slot);
	EXPECT_TRUE(creature_tattoo::FromWord(0x000000F0).Empty());
}
