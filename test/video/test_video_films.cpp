/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The switch of the game's films (ecs::systems::VideoSystem::FilmsEnabled) and the decoder the video player's game
// hooks make with it on and off.

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/VideoSystem.h"
#include "Locator.h"
#include "Video/VideoPlayer.h"
#include "support/RestoreService.h"

using namespace openblack;

TEST(VideoFilms, OnByDefault)
{
	ecs::systems::VideoSystem videos;
	EXPECT_TRUE(videos.FilmsEnabled());
	videos.SetFilmsEnabled(false);
	EXPECT_FALSE(videos.FilmsEnabled());
	videos.SetFilmsEnabled(true);
	EXPECT_TRUE(videos.FilmsEnabled());
}

TEST(VideoFilms, TheGameHooksFollowTheSwitch)
{
	const test::RestoreService<Locator::videoSystem> restore;
	Locator::videoSystem::emplace<ecs::systems::VideoSystem>();
	const auto hooks = video::VideoPlayer::GameHooks();
	ASSERT_TRUE(hooks.makeDecoder);
#ifdef OPENBLACK_USE_BINK
	EXPECT_NE(hooks.makeDecoder(), nullptr);
#else
	EXPECT_EQ(hooks.makeDecoder(), nullptr); // no decoder in this build
#endif
	// Off: no decoder, so the film is skipped as a build without one skips it
	Locator::videoSystem::value().SetFilmsEnabled(false);
	EXPECT_EQ(hooks.makeDecoder(), nullptr);
}
