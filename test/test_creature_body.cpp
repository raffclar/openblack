/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "3D/CreatureBody.h"

using namespace openblack;
using namespace openblack::creature;

using A = CreatureBody::Appearance;

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

TEST(CreatureBody, TheIdsAreTheHashesOfTheirTexts)
{
	// The ids are made without a text on the heap. They must stay the hashes of the texts that the caches use.
	for (uint32_t species = 0; species <= 255; ++species)
	{
		const auto type = static_cast<CreatureType>(species);
		const auto rig = fmt::format("creature/{}/rig", static_cast<uint8_t>(type));
		EXPECT_EQ(GetRigId(type), entt::hashed_string(rig.c_str()).value());
		for (uint32_t look = 0; look <= static_cast<uint32_t>(A::Thin); ++look)
		{
			const auto mesh = fmt::format("creature/{}/{}", static_cast<uint8_t>(type), look);
			EXPECT_EQ(GetIdFromType(type, static_cast<A>(look)), entt::hashed_string(mesh.c_str()).value());
		}
	}
	EXPECT_EQ(GetIdFromType(CreatureType::Cow, static_cast<A>(4'000'000'000U)),
	          entt::hashed_string("creature/1/4000000000").value());
}
