/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

#include "Debug/ComputerActions.h"
#include "ECS/ComputerMind.h"

using namespace openblack;
using namespace openblack::computer_mind;

namespace
{

/// A world that wants nothing and keeps a record of the actions that start
class FakeWorld final: public World
{
public:
	float Intensity(Mind& /*mind*/, uint16_t /*node*/) override { return 0.0f; }
	bool Perform(Mind& mind, uint16_t node) override
	{
		performed.push_back(mind.nodes[node].kind);
		objects.push_back(mind.nodes[node].object);
		above.push_back(mind.nodes[node].parent.has_value() ? mind.nodes[*mind.nodes[node].parent].object : entt::null);
		return true;
	}
	[[nodiscard]] bool ActionBusy() const override { return false; }
	bool StepAction() override { return true; }
	void ClearAction() override {}
	void StartAction() override {}
	[[nodiscard]] bool ActionLocked() const override { return false; }
	void UnlockAction() override {}
	[[nodiscard]] std::optional<glm::vec3> MarkerPlace(entt::entity /*thing*/) const override { return std::nullopt; }
	[[nodiscard]] bool HeldByHelp() const override { return false; }

	std::vector<NodeKind> performed;
	std::vector<entt::entity> objects;
	std::vector<entt::entity> above;
};

} // namespace

// A script action of a node that the god never worked out does not start: the node chose nothing yet
TEST(DebugComputerActions, ANodeThatTheGodNeverWorkedOutDoesNotStart)
{
	auto mind = MakeMind();
	FakeWorld world;
	EXPECT_FALSE(ForceAction(mind, world, "CastShieldToProtect", entt::entity {1}, entt::entity {2}, 1.0f, 10));
	EXPECT_TRUE(world.performed.empty());
}

// The tool makes the tree of a new mind and lets the node choose itself. The script action then starts on the two things.
TEST(DebugComputerActions, TheToolLetsTheScriptActionStart)
{
	Mind mind;
	ASSERT_TRUE(debug::computer_actions::MakeReady(mind, "CastShieldToProtect"));
	ASSERT_FALSE(mind.nodes.empty());
	FakeWorld world;
	QueueAction(mind, "CastShieldToProtect", entt::entity {1}, entt::entity {2}, 10);
	EXPECT_TRUE(ForceNext(mind, world, 11));
	ASSERT_EQ(world.performed, std::vector {NodeKind::ObjectCastShieldToProtect});
	// The node acts on the second thing, and the node above gets the first thing
	EXPECT_EQ(world.objects.at(0), entt::entity {2});
	EXPECT_EQ(world.above.at(0), entt::entity {1});
	EXPECT_TRUE(mind.queue.empty());
}

// The tool keeps the tree of a mind that has one, and finds no node for a name that no node has
TEST(DebugComputerActions, AnUnknownNameMakesNothingReady)
{
	auto mind = MakeMind();
	const auto nodes = mind.nodes.size();
	EXPECT_FALSE(debug::computer_actions::MakeReady(mind, "NoNodeHasThisName"));
	EXPECT_EQ(mind.nodes.size(), nodes);
}

TEST(DebugComputerActions, TheToolCountsTheSendsOfAKindOfMiracle)
{
	openblack::computer_mind::Mind mind;
	EXPECT_TRUE(openblack::debug::computer_actions::SetUntaughtSends(mind, 1, 2));
	EXPECT_EQ(mind.untaughtSends, (std::array<uint32_t, 3> {0, 2, 0}));
	EXPECT_FALSE(openblack::debug::computer_actions::SetUntaughtSends(mind, 3, 1));
	EXPECT_EQ(mind.untaughtSends, (std::array<uint32_t, 3> {0, 2, 0}));
}
