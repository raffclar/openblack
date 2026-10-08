/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"

using namespace openblack;
using namespace openblack::creature;

using A = CreatureBody::Appearance;

namespace
{
/// The names that are no body log a line to the game's log, which the test run has to have
void NeedTheGameLog()
{
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
}
} // namespace

TEST(CreatureBody, NamesTheMeshOfEachAppearance)
{
	EXPECT_EQ(GetIdFromMeshName("A_Tiger2_Base"), GetIdFromType(CreatureType::Tiger, A::Base));
	EXPECT_EQ(GetIdFromMeshName("C_Cow_Boned"), GetIdFromType(CreatureType::Cow, A::Base));
	EXPECT_EQ(GetIdFromMeshName("A_Bear_Boned_Fat"), GetIdFromType(CreatureType::BrownBear, A::Fat));
	EXPECT_EQ(GetIdFromMeshName("C_wolf_evil2"), GetIdFromType(CreatureType::Wolf, A::Evil));
	EXPECT_EQ(GetIdFromMeshName("C_CHIMP_strong"), GetIdFromType(CreatureType::Chimp, A::Strong));
}

TEST(CreatureBody, TheOgresVariantsKeepItsBaseName)
{
	EXPECT_EQ(GetIdFromMeshName("A_Greek_Boned_base"), GetIdFromType(CreatureType::Ogre, A::Base));
	EXPECT_EQ(GetIdFromMeshName("A_Greek_Boned_Base_Evil"), GetIdFromType(CreatureType::Ogre, A::Evil));
	EXPECT_EQ(GetIdFromMeshName("A_Greek_Boned_Base_Thin"), GetIdFromType(CreatureType::Ogre, A::Thin));
}

TEST(CreatureBody, TheCreatureTablesStartWithTheGiantApe)
{
	EXPECT_EQ(InfoRow(CreatureType::GiantApe), 0u);
	EXPECT_EQ(InfoRow(CreatureType::Cow), 1u);
	EXPECT_EQ(InfoRow(CreatureType::Gorilla), 16u);
}

TEST(CreatureBody, AnUnknownAppearanceNamesNoMesh)
{
	NeedTheGameLog();
	// Artwork left in the creature mesh folder: the last part of its name is not an appearance, so it names no mesh
	// and cannot take the Tiger's base id
	EXPECT_FALSE(GetIdFromMeshName("A_Tiger2_Base - Realistic Face").has_value());
	EXPECT_EQ(GetSpeciesFromMeshName("A_Tiger2_Base - Realistic Face"), CreatureType::Unknown);
	EXPECT_TRUE(GetIdFromMeshName("A_Tiger2_Base").has_value());
}

TEST(CreatureBody, AnUnknownSpeciesStillNamesAMesh)
{
	NeedTheGameLog();
	// Only the appearance decides whether a file is a body; a species we do not know yet is read as Unknown
	EXPECT_TRUE(GetIdFromMeshName("C_Dragon_Base").has_value());
	EXPECT_EQ(GetSpeciesFromMeshName("C_Dragon_Base"), CreatureType::Unknown);
	EXPECT_EQ(GetSpeciesFromMeshName("A_Tiger2_Evil"), CreatureType::Tiger);
}

TEST(CreatureBody, EachSpeciesRigHasItsOwnId)
{
	EXPECT_NE(GetRigId(CreatureType::Tiger), GetRigId(CreatureType::Cow));
	EXPECT_EQ(GetRigId(CreatureType::Tiger), GetRigId(CreatureType::Tiger));
	EXPECT_NE(GetRigId(CreatureType::Tiger), GetIdFromType(CreatureType::Tiger, A::Base));
}
