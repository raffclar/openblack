/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What moves a species' body, read from a synthetic Creature block whose meshes are not there

#include <numbers>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Creature/CreatureRig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Resources/Loaders.h"
#include "creature/SyntheticCreatureBlock.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::creature;
using namespace openblack::test::creature_block;

namespace
{
class CreatureRigLoaderTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("game"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		_folder.emplace(std::string("openblack_test_creature_rig_loader_") +
		                ::testing::UnitTest::GetInstance()->current_test_info()->name());
		Locator::filesystem::value().SetGamePath(_folder->Path());
		WriteSpec(_folder->Path(), true, 90, {{"move", {"Cstand", "Wwalk"}}, {"face", {"Nsmile"}}});
	}

	[[nodiscard]] std::shared_ptr<CreatureRig> Load(const Block& block) const
	{
		return resources::CreatureRigLoader {}(resources::CreatureRigLoader::FromBufferTag {}, Build(block), _folder->Path(),
		                                       _folder->Path() / "CreatureMesh");
	}

	static Block Sample()
	{
		Block block;
		block.soundObject = 5;
		block.eyes.points[0].enabled = true;
		block.clips = {Clip {.durationMs = 1000,
		                     .events = {{.type = 2, .frame = 250, .action = 4, .mode = 0},
		                                {.type = 0, .frame = 600, .action = 7, .mode = 1}}},
		               std::nullopt, Clip {.durationMs = 500, .looping = 0}};
		block.sites[1] = {.enabled = true, .u = 30, .v = 40, .skin = 1, .size = 0.5f, .mirror = false, .rotation = 3};
		return block;
	}

	const test::ScopedDefaultFileSystem _fileSystem;
	std::optional<TempFolder> _folder;
};
} // namespace

TEST_F(CreatureRigLoaderTest, AnimationsFollowTheSpecFile)
{
	const auto rig = Load(Sample());
	EXPECT_EQ(rig->baseMeshName, "A_Ape_Base");
	EXPECT_TRUE(rig->hasMesh[0]);
	EXPECT_FALSE(rig->hasMesh[1]);
	for (const auto& animations : rig->animations)
	{
		EXPECT_EQ(animations.size(), 3u);
	}
	ASSERT_TRUE(rig->animations[0][0].has_value());
	EXPECT_EQ(rig->animations[0][0]->duration, 1000u);
	EXPECT_FALSE(rig->animations[0][1].has_value());
	ASSERT_TRUE(rig->animations[0][2].has_value());
	EXPECT_FALSE(rig->animations[0][2]->looping);
	// without animations of its own, a variant mesh plays the base's
	EXPECT_EQ(rig->GetAnimation(CreatureRig::Mesh::Evil, 2), &*rig->animations[0][2]);
	EXPECT_EQ(rig->GetAnimation(CreatureRig::Mesh::Fat, 1), nullptr);
	EXPECT_EQ(rig->GetAnimation(CreatureRig::Mesh::Base, 9), nullptr);
}

TEST_F(CreatureRigLoaderTest, SoundsLeashBoneActionPointsAndTattooSites)
{
	const auto rig = Load(Sample());
	ASSERT_EQ(rig->soundEvents.size(), 3u);
	ASSERT_EQ(rig->soundEvents[0].size(), 2u);
	EXPECT_EQ(rig->soundEvents[0][0], (creature_audio::SoundEvent {.kind = creature_audio::EventKind::Generic,
	                                                               .timeMs = 250,
	                                                               .action = audio::SoundAction::FootstepNormal,
	                                                               .mode = 0}));
	EXPECT_EQ(rig->soundEvents[0][1].kind, creature_audio::EventKind::Voice);
	EXPECT_EQ(rig->soundEvents[0][1].mode, 1);
	EXPECT_TRUE(rig->soundEvents[1].empty());
	EXPECT_EQ(rig->soundObject, 5);
	EXPECT_EQ(rig->soundBankName, "ape_voice");
	EXPECT_EQ(rig->leashBone, 11u);
	ASSERT_TRUE(rig->actionPoints.has_value());
	EXPECT_EQ(rig->actionPoints->rightHand, 4u);
	EXPECT_EQ(rig->actionPoints->groin, 10u);
	EXPECT_FLOAT_EQ(rig->actionPoints->throwMs, 700.0f);
	ASSERT_TRUE(rig->tattooSites.has_value());
	EXPECT_TRUE((*rig->tattooSites)[1].enabled);
	EXPECT_EQ((*rig->tattooSites)[1].u, 30);
	EXPECT_EQ((*rig->tattooSites)[1].rotation, 3);
	EXPECT_FALSE((*rig->tattooSites)[0].enabled);
}

TEST_F(CreatureRigLoaderTest, NoMeshesMeansNoEyesOrHair)
{
	// the eyes sit on triangles of the meshes, which are not in the folder
	const auto rig = Load(Sample());
	EXPECT_FALSE(rig->eyes.has_value());
	EXPECT_TRUE(rig->hairGroups.empty());
	// a negative bone leaves the action points out
	auto block = Sample();
	block.points.head = -1;
	EXPECT_FALSE(Load(block)->actionPoints.has_value());
	// not a block the parser can read
	EXPECT_THROW(static_cast<void>(resources::CreatureRigLoader {}(resources::CreatureRigLoader::FromBufferTag {},
	                                                               std::vector<uint8_t>(16), _folder->Path(), _folder->Path())),
	             std::runtime_error);
}

TEST(CreatureRig, PlacementAndPosedBone)
{
	const glm::vec3 position(10.0f, 2.0f, -4.0f);
	const glm::mat3 rotation(glm::rotate(glm::mat4(1.0f), std::numbers::pi_v<float> / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
	const auto placement = PlacementMatrix(position, rotation, glm::vec3(2.0f));
	// scaled, then turned, then moved to the position
	const glm::vec3 point(1.0f, 0.5f, 0.0f);
	const auto placed = glm::vec3(placement * glm::vec4(point, 1.0f));
	const auto expected = (rotation * (2.0f * point)) + position;
	EXPECT_NEAR(placed.x, expected.x, 1e-4f);
	EXPECT_NEAR(placed.y, expected.y, 1e-4f);
	EXPECT_NEAR(placed.z, expected.z, 1e-4f);

	const std::vector<glm::mat4> bones {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 0.0f))};
	EXPECT_EQ(PosedBone(1, bones, placement), placement * bones[1]);
	// a bone the pose lacks is placed with the creature alone
	EXPECT_EQ(PosedBone(7, bones, placement), placement);
}
