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

#include <MindFile.h>
#include <gtest/gtest.h>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureMindModel.h"

using namespace openblack;

namespace
{

/// A mind file with two desires: the first has sources of kinds 3 and 7, the second has a source of kind 3
creaturemind::MindFileData TwoDesireFile()
{
	creaturemind::MindFileData file;
	file.desires.resize(2);
	file.desires[0].sources = {{.value = 0.9f, .threshold = 0.1f, .type = 3}, {.value = 0.2f, .threshold = 0.3f, .type = 7}};
	file.desires[1].sources = {{.value = 0.4f, .threshold = 0.6f, .type = 3}};
	return file;
}

creature_desires::DesireSetup::Spread FakeSpreadOf(uint32_t type)
{
	return {.value = 0.01f * static_cast<float>(type), .threshold = 0.1f * static_cast<float>(type)};
}

} // namespace

// The game makes each source of a mind file as a new source first: it draws the spread value, then the spread
// threshold of the source's kind, in the order of the file
TEST(CreatureFilesDraws, AMindFileDrawsTheSpreadsOfEachSourceInTheOrderOfTheFile)
{
	std::vector<float> draws;
	creature_mind_model::DrawAsSourcesAreRead(TwoDesireFile(), FakeSpreadOf, [&draws](float most) {
		draws.push_back(most);
		return 0.0f;
	});
	const std::vector<float> expected {2.0f * 0.03f, 2.0f * 0.3f, 2.0f * 0.07f, 2.0f * 0.7f, 2.0f * 0.03f, 2.0f * 0.3f};
	EXPECT_EQ(draws, expected);
}

// A file with no sources draws nothing
TEST(CreatureFilesDraws, AMindFileWithNoSourcesDrawsNothing)
{
	creaturemind::MindFileData file;
	file.desires.resize(3);
	int draws = 0;
	creature_mind_model::DrawAsSourcesAreRead(file, FakeSpreadOf, [&draws](float /*most*/) {
		++draws;
		return 0.0f;
	});
	EXPECT_EQ(draws, 0);
}

// A told sit lasts ten seconds and the game's float draw of up to five more
TEST(CreatureFilesDraws, AToldSitLastsTenSecondsAndAFloatDrawOfUpToFive)
{
	std::vector<float> draws;
	const auto sit = creature_mind::SitDown([&draws](float most) {
		draws.push_back(most);
		return 2.75f;
	});
	ASSERT_EQ(draws.size(), 1);
	EXPECT_EQ(draws[0], 5.0f);
	EXPECT_EQ(sit.seconds, 12.75f);
}
