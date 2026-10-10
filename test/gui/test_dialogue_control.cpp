/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <vector>

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/DialogueControlSystem.h"

using openblack::ecs::systems::DialogueControlSystem;

namespace
{
/// What the help system was told
struct FakeHelp
{
	std::vector<bool> sentHome;
	int taken {0};
	std::vector<bool> released;

	DialogueControlSystem::Hooks Hooks()
	{
		return {
		    .sendSpiritsHome = [this](bool help) { sentHome.push_back(help); },
		    .taken = [this]() { ++taken; },
		    .released = [this](bool help) { released.push_back(help); },
		};
	}
};
} // namespace

TEST(DialogueControl, AFreeDialogueGoesToTheTaskThatAsks)
{
	FakeHelp help;
	DialogueControlSystem dialogue;
	dialogue.SetHooks(help.Hooks());
	EXPECT_FALSE(dialogue.IsControlled(0));

	dialogue.Request(4, 0);
	EXPECT_EQ(dialogue.GetOwner(), 4u);
	EXPECT_EQ(help.taken, 1);
	EXPECT_TRUE(dialogue.IsControlled(0));

	// Taken, it stays with its task
	dialogue.Request(5, 0);
	EXPECT_EQ(dialogue.GetOwner(), 4u);
	EXPECT_EQ(help.taken, 1);
}

TEST(DialogueControl, AnotherTasksCinemaBarsKeepItTaken)
{
	FakeHelp help;
	DialogueControlSystem dialogue;
	dialogue.SetHooks(help.Hooks());
	EXPECT_TRUE(dialogue.IsControlled(9));
	dialogue.Request(4, 9);
	EXPECT_EQ(dialogue.GetOwner(), 0u);
	EXPECT_EQ(help.taken, 0);
}

TEST(DialogueControl, OnlyItsTaskGivesItBack)
{
	FakeHelp help;
	DialogueControlSystem dialogue;
	dialogue.SetHooks(help.Hooks());
	dialogue.Request(4, 0);

	EXPECT_FALSE(dialogue.Release(5, false));
	EXPECT_EQ(dialogue.GetOwner(), 4u);
	EXPECT_TRUE(help.released.empty());

	EXPECT_TRUE(dialogue.Release(4, true));
	EXPECT_EQ(dialogue.GetOwner(), 0u);
	ASSERT_EQ(help.released.size(), 1u);
	EXPECT_TRUE(help.released.front());

	dialogue.SendSpiritsHome(false);
	ASSERT_EQ(help.sentHome.size(), 1u);
	EXPECT_FALSE(help.sentHome.front());
}

TEST(DialogueControl, WithoutAHelpSystemNothingIsTold)
{
	DialogueControlSystem dialogue;
	dialogue.Request(1, 0);
	dialogue.SendSpiritsHome(true);
	EXPECT_TRUE(dialogue.Release(1, false));

	dialogue.Request(2, 0);
	dialogue.Reset();
	EXPECT_EQ(dialogue.GetOwner(), 0u);
}
