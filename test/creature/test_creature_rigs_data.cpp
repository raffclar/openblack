/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Integration: every species' rig from the game's own .cbn files, and the skin meshes they name. Skipped without the
// game's data (OPENBLACK_GAME_PATH or OPENBLACK_TEST_GAME_PATH)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "FileSystem/FileSystemInterface.h"
#include "Resources/Resources.h"
#include "support/TestServices.h"

using namespace openblack;

TEST(CreatureRigsData, EverySpeciesLoadsWithItsSkinMeshes)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		game = std::getenv("OPENBLACK_TEST_GAME_PATH");
	}
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	if (!spdlog::get("game"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	const test::ScopedDefaultFileSystem fileSystem;
	Locator::filesystem::value().SetGamePath(game);
	resources::Resources resources;
	resources::LoadCreatureRigs(resources);

	auto& rigs = resources.GetCreatureRigs();
	const auto& files = resources.GetL3DFiles();
	const auto meshDirectory = Locator::filesystem::value().GetPath<filesystem::Path::CreatureMesh>();
	for (int32_t s = static_cast<int32_t>(CreatureType::Cow); s <= static_cast<int32_t>(CreatureType::GiantApe); ++s)
	{
		const auto species = static_cast<CreatureType>(s);
		const auto id = creature::GetRigId(species);
		ASSERT_TRUE(rigs.Contains(id)) << "species " << s;
		const auto& rig = *rigs.Handle(id);
		EXPECT_FALSE(rig.animations.front().empty()) << "species " << s;
		for (size_t m = 0; m < rig.meshNames.size(); ++m)
		{
			const auto& name = rig.meshNames.at(m);
			if (name.empty() || !rig.hasMesh.at(m) || !Locator::filesystem::value().Exists(meshDirectory / (name + ".l3d")))
			{
				continue;
			}
			EXPECT_TRUE(files.Contains(entt::hashed_string(("creature/skins/" + name).c_str()).value())) << name;
		}
	}
	const auto& ape = *rigs.Handle(creature::GetRigId(CreatureType::GiantApe));
	EXPECT_TRUE(ape.eyes.has_value());
	EXPECT_TRUE(ape.tattooSites.has_value());
	EXPECT_TRUE(ape.actionPoints.has_value());
	EXPECT_TRUE(ape.leashBone.has_value());
}
