/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Editor/EditorHistory.h"

using namespace openblack;
using namespace openblack::editor;

using Entity = entt::entity;

namespace
{

/// A land of things with poses and nothing else. Each thing made gets a new entity, as the registry gives.
class FakeWorld final: public EditorWorld
{
public:
	entt::entity Add(const Pose& pose)
	{
		const auto entity = static_cast<entt::entity>(_next++);
		_poses[entity] = pose;
		return entity;
	}
	[[nodiscard]] bool Exists(entt::entity entity) const override { return _poses.contains(entity); }
	void SetPose(entt::entity entity, const Pose& pose) override { _poses.at(entity) = pose; }
	MakeResult Make(const Recipe& recipe) override
	{
		if (refuse)
		{
			return {};
		}
		const auto entity = Add(recipe.pose);
		_made[entity] = recipe;
		return {.entity = entity};
	}
	void Remove(entt::entity entity) override { _poses.erase(entity); }
	/// The game removes a thing on its own, as a villager that dies
	void Lose(entt::entity entity) { _poses.erase(entity); }

	[[nodiscard]] const Pose& PoseOf(entt::entity entity) const { return _poses.at(entity); }
	[[nodiscard]] const Recipe& MadeFrom(entt::entity entity) const { return _made.at(entity); }
	[[nodiscard]] size_t Count() const { return _poses.size(); }

	bool refuse {false};

private:
	uint32_t _next {1};
	std::map<entt::entity, Pose> _poses;
	std::map<entt::entity, Recipe> _made;
};

Pose At(float x, float z)
{
	return Pose {.position = {x, 0.0f, z}};
}

Recipe TreeAt(float x, float z)
{
	return Recipe {.item = {.kind = PlaceKind::Tree, .type = 3}, .pose = At(x, z)};
}

/// A land of a network game: the history sends commands, and nothing changes until the test gives the answers of the
/// turn. Each command takes a request from the history, as the writes of the editor do.
class NetworkWorld final: public EditorWorld
{
public:
	explicit NetworkWorld(EditorHistory& history)
	    : _history(history)
	{
	}

	struct Sent
	{
		enum class Kind : uint8_t
		{
			Pose,
			Make,
			Remove,
		};
		Kind kind;
		entt::entity entity {entt::null};
		Pose pose;
		Recipe recipe;
		Request request {0};
	};

	[[nodiscard]] bool Exists(entt::entity entity) const override { return entity != entt::null; }
	void SetPose(entt::entity entity, const Pose& pose) override
	{
		sent.push_back({.kind = Sent::Kind::Pose, .entity = entity, .pose = pose, .request = _history.NewRequest(turn)});
	}
	MakeResult Make(const Recipe& recipe) override
	{
		const auto request = _history.NewRequest(turn);
		sent.push_back({.kind = Sent::Kind::Make, .recipe = recipe, .request = request});
		return {.request = request};
	}
	void Remove(entt::entity entity) override
	{
		sent.push_back({.kind = Sent::Kind::Remove, .entity = entity, .request = _history.NewRequest(turn)});
	}

	/// The turn of each command that the history sent: the answer closes each request
	void AnswerAll(entt::entity made = entt::null)
	{
		for (const auto& command : sent)
		{
			const auto entity = command.kind == Sent::Kind::Make ? made : command.entity;
			_history.Answer(command.request, {.entity = entity, .recipe = command.recipe, .pose = command.pose});
		}
		sent.clear();
	}

	std::vector<Sent> sent;
	uint32_t turn {0};

private:
	EditorHistory& _history;
};

Entity Thing(uint32_t value)
{
	return static_cast<entt::entity>(value);
}

} // namespace

TEST(EditorHistory, UndoesAndRedoesAMove)
{
	FakeWorld world;
	EditorHistory history;
	const auto tree = world.Add(At(1.0f, 1.0f));
	world.SetPose(tree, At(5.0f, 1.0f));
	history.Push({.label = "Move",
	              .changes = {PoseChange {.ref = history.Track(tree), .before = At(1.0f, 1.0f), .after = At(5.0f, 1.0f)}}});
	EXPECT_TRUE(history.CanUndo());
	EXPECT_FALSE(history.CanRedo());
	EXPECT_EQ(history.UndoLabel(), "Move");

	const auto undone = history.Undo(world);
	ASSERT_TRUE(undone.has_value());
	EXPECT_EQ(undone->label, "Move");
	EXPECT_EQ(undone->touched, std::vector {tree});
	EXPECT_EQ(world.PoseOf(tree), At(1.0f, 1.0f));
	EXPECT_FALSE(history.CanUndo());
	EXPECT_EQ(history.RedoLabel(), "Move");

	ASSERT_TRUE(history.Redo(world).has_value());
	EXPECT_EQ(world.PoseOf(tree), At(5.0f, 1.0f));
	EXPECT_FALSE(history.Redo(world).has_value());
}

TEST(EditorHistory, KeepsAThingThroughRemovesAndMakes)
{
	FakeWorld world;
	EditorHistory history;
	const auto tree = world.Add(At(1.0f, 1.0f));
	const auto ref = history.Track(tree);
	EXPECT_EQ(history.Track(tree), ref);
	// The tree moves, then the editor removes it
	world.SetPose(tree, At(2.0f, 2.0f));
	history.Push({.label = "Move", .changes = {PoseChange {.ref = ref, .before = At(1.0f, 1.0f), .after = At(2.0f, 2.0f)}}});
	history.Push({.label = "Delete", .changes = {RemoveChange {.ref = ref, .recipe = TreeAt(2.0f, 2.0f)}}});
	world.Remove(tree);

	// Undo makes the tree again with a new entity, and the move still finds it
	const auto remade = history.Undo(world);
	ASSERT_TRUE(remade.has_value());
	ASSERT_EQ(remade->touched.size(), 1u);
	const auto again = remade->touched.front();
	EXPECT_NE(again, tree);
	EXPECT_EQ(history.Resolve(ref), again);
	EXPECT_EQ(world.MadeFrom(again), TreeAt(2.0f, 2.0f));
	ASSERT_TRUE(history.Undo(world).has_value());
	EXPECT_EQ(world.PoseOf(again), At(1.0f, 1.0f));

	// Redo moves it and removes it again
	ASSERT_TRUE(history.Redo(world).has_value());
	EXPECT_EQ(world.PoseOf(again), At(2.0f, 2.0f));
	const auto removed = history.Redo(world);
	ASSERT_TRUE(removed.has_value());
	EXPECT_TRUE(removed->touched.empty());
	EXPECT_FALSE(world.Exists(again));
	EXPECT_EQ(history.Resolve(ref), entt::entity {entt::null});
}

TEST(EditorHistory, UndoRemovesWhatTheEditorMade)
{
	FakeWorld world;
	EditorHistory history;
	const auto made = world.Make(TreeAt(4.0f, 4.0f)).entity;
	history.Push({.label = "Place Oak", .changes = {MakeChange {.ref = history.Track(made), .recipe = TreeAt(4.0f, 4.0f)}}});
	ASSERT_TRUE(history.Undo(world).has_value());
	EXPECT_EQ(world.Count(), 0u);
	const auto redone = history.Redo(world);
	ASSERT_TRUE(redone.has_value());
	ASSERT_EQ(redone->touched.size(), 1u);
	EXPECT_EQ(world.PoseOf(redone->touched.front()), At(4.0f, 4.0f));
}

TEST(EditorHistory, GoesBackThroughAStepFromItsLastChange)
{
	FakeWorld world;
	EditorHistory history;
	// A copy moved in the same step: undo puts it back, then removes it
	const auto copy = world.Make(TreeAt(0.0f, 0.0f)).entity;
	const auto ref = history.Track(copy);
	world.SetPose(copy, At(3.0f, 0.0f));
	history.Push({.label = "Two changes",
	              .changes = {MakeChange {.ref = ref, .recipe = TreeAt(0.0f, 0.0f)},
	                          PoseChange {.ref = ref, .before = At(0.0f, 0.0f), .after = At(3.0f, 0.0f)}}});
	const auto undone = history.Undo(world);
	ASSERT_TRUE(undone.has_value());
	EXPECT_EQ(undone->skipped, 0u);
	EXPECT_TRUE(undone->touched.empty());
	EXPECT_EQ(world.Count(), 0u);
	const auto redone = history.Redo(world);
	ASSERT_TRUE(redone.has_value());
	EXPECT_EQ(redone->skipped, 0u);
	ASSERT_EQ(redone->touched.size(), 1u);
	EXPECT_EQ(world.PoseOf(redone->touched.front()), At(3.0f, 0.0f));
}

TEST(EditorHistory, SkipsTheThingsThatAreGone)
{
	FakeWorld world;
	EditorHistory history;
	const auto first = world.Add(At(0.0f, 0.0f));
	const auto second = world.Add(At(1.0f, 0.0f));
	history.Push({.label = "Move 2 things",
	              .changes = {PoseChange {.ref = history.Track(first), .before = At(0.0f, 0.0f), .after = At(0.0f, 5.0f)},
	                          PoseChange {.ref = history.Track(second), .before = At(1.0f, 0.0f), .after = At(1.0f, 5.0f)}}});
	world.Lose(first);
	const auto undone = history.Undo(world);
	ASSERT_TRUE(undone.has_value());
	EXPECT_EQ(undone->skipped, 1u);
	EXPECT_EQ(undone->touched, std::vector {second});
	EXPECT_EQ(world.PoseOf(second), At(1.0f, 0.0f));

	// A thing that the world cannot make again counts as skipped too
	history.Push({.label = "Delete", .changes = {RemoveChange {.ref = history.Track(second), .recipe = TreeAt(1.0f, 0.0f)}}});
	world.Remove(second);
	world.refuse = true;
	const auto refused = history.Undo(world);
	ASSERT_TRUE(refused.has_value());
	EXPECT_EQ(refused->skipped, 1u);
	EXPECT_TRUE(refused->touched.empty());
}

TEST(EditorHistory, ANewStepDropsTheRedoStepsAndTheOldestSteps)
{
	FakeWorld world;
	EditorHistory history;
	const auto tree = world.Add(At(0.0f, 0.0f));
	const auto ref = history.Track(tree);
	const auto move = [&](float from, float to) {
		history.Push({.label = std::to_string(to),
		              .changes = {PoseChange {.ref = ref, .before = At(from, 0.0f), .after = At(to, 0.0f)}}});
	};
	move(0.0f, 1.0f);
	move(1.0f, 2.0f);
	ASSERT_TRUE(history.Undo(world).has_value());
	EXPECT_TRUE(history.CanRedo());
	move(1.0f, 7.0f);
	EXPECT_FALSE(history.CanRedo());
	EXPECT_EQ(history.UndoCount(), 2u);

	// A step with no changes is not kept
	history.Push({.label = "Nothing"});
	EXPECT_EQ(history.UndoCount(), 2u);

	for (size_t i = 0; i < EditorHistory::k_MaxSteps + 10; ++i)
	{
		move(0.0f, static_cast<float>(i));
	}
	EXPECT_EQ(history.UndoCount(), EditorHistory::k_MaxSteps);
	EXPECT_EQ(history.UndoLabel(), std::to_string(static_cast<float>(EditorHistory::k_MaxSteps + 9)));

	history.Clear();
	EXPECT_FALSE(history.CanUndo());
	EXPECT_EQ(history.Resolve(ref), entt::entity {entt::null});
}

TEST(EditorHistoryNetwork, APlacementBecomesAStepAtItsAnswer)
{
	EditorHistory history;
	const auto request = history.NewRequest(10);
	history.Push({.label = "Place Oak", .changes = {MakeChange {.awaits = request}}});
	EXPECT_TRUE(history.Waiting());
	EXPECT_FALSE(history.CanUndo());
	EXPECT_EQ(history.AwaitedFrom(request), Awaited::Thing);

	// The turn makes the tree, and the history learns the thing and how to make it again
	history.Answer(request, {.entity = Thing(40), .recipe = TreeAt(4.0f, 4.0f)});
	EXPECT_FALSE(history.Waiting());
	ASSERT_TRUE(history.CanUndo());
	ASSERT_EQ(history.MadeRefs().size(), 1u);
	EXPECT_EQ(history.Resolve(history.MadeRefs().front()), Thing(40));

	// Undo sends the removal, and redo sends a make again that the next answer gives
	NetworkWorld world(history);
	ASSERT_TRUE(history.Undo(world).has_value());
	ASSERT_EQ(world.sent.size(), 1u);
	EXPECT_EQ(world.sent.front().kind, NetworkWorld::Sent::Kind::Remove);
	EXPECT_EQ(world.sent.front().entity, Thing(40));
	world.AnswerAll();
	const auto redone = history.Redo(world);
	ASSERT_TRUE(redone.has_value());
	EXPECT_TRUE(redone->touched.empty());
	EXPECT_EQ(redone->skipped, 0u);
	ASSERT_EQ(world.sent.size(), 1u);
	EXPECT_EQ(world.sent.front().kind, NetworkWorld::Sent::Kind::Make);
	EXPECT_EQ(world.sent.front().recipe, TreeAt(4.0f, 4.0f));
	EXPECT_FALSE(history.CanUndo());
	world.AnswerAll(Thing(41));
	EXPECT_EQ(history.Resolve(history.MadeRefs().front()), Thing(41));
	EXPECT_TRUE(history.CanUndo());
}

TEST(EditorHistoryNetwork, ARefusedPlacementLeavesNoStep)
{
	EditorHistory history;
	const auto request = history.NewRequest(0);
	history.Push({.label = "Place Oak", .changes = {MakeChange {.awaits = request}}});
	history.Answer(request, {});
	EXPECT_EQ(history.UndoCount(), 0u);
	EXPECT_FALSE(history.Waiting());
	EXPECT_TRUE(history.MadeRefs().empty());
}

TEST(EditorHistoryNetwork, APoseStepTakesThePoseAfterItsLastCommand)
{
	EditorHistory history;
	const auto tree = Thing(7);
	const auto ref = history.Track(tree);
	const auto request = history.NewRequest(0);
	history.Push({.label = "Move",
	              .changes = {PoseChange {.ref = ref, .before = At(1.0f, 1.0f), .after = At(1.0f, 1.0f), .awaits = request}}});
	EXPECT_EQ(history.AwaitedFrom(request), Awaited::Pose);
	history.Answer(request, {.entity = tree, .pose = At(6.0f, 2.0f)});

	// Undo sends the full pose from before, and redo the full pose from the answer
	NetworkWorld world(history);
	ASSERT_TRUE(history.Undo(world).has_value());
	ASSERT_EQ(world.sent.size(), 1u);
	EXPECT_EQ(world.sent.front().kind, NetworkWorld::Sent::Kind::Pose);
	EXPECT_EQ(world.sent.front().pose, At(1.0f, 1.0f));
	world.AnswerAll();
	ASSERT_TRUE(history.Redo(world).has_value());
	EXPECT_EQ(world.sent.front().pose, At(6.0f, 2.0f));

	// A command that left the pose as it was is no change
	const auto same = history.NewRequest(0);
	world.AnswerAll();
	history.Push({.label = "Move",
	              .changes = {PoseChange {.ref = ref, .before = At(6.0f, 2.0f), .after = At(6.0f, 2.0f), .awaits = same}}});
	EXPECT_EQ(history.UndoCount(), 2u);
	history.Answer(same, {.entity = tree, .pose = At(6.0f, 2.0f)});
	EXPECT_EQ(history.UndoCount(), 1u);
}

TEST(EditorHistoryNetwork, UndoOfARemovalMakesTheThingAgainFromItsRecipe)
{
	EditorHistory history;
	const auto tree = Thing(9);
	const auto ref = history.Track(tree);
	history.Push({.label = "Delete", .changes = {RemoveChange {.ref = ref, .recipe = TreeAt(2.0f, 3.0f)}}});
	NetworkWorld world(history);
	ASSERT_TRUE(history.Undo(world).has_value());
	ASSERT_EQ(world.sent.size(), 1u);
	EXPECT_EQ(world.sent.front().recipe, TreeAt(2.0f, 3.0f));
	EXPECT_EQ(history.Resolve(ref), Entity {entt::null});
	world.AnswerAll(Thing(12));
	EXPECT_EQ(history.Resolve(ref), Thing(12));

	// The redo removes the new thing
	ASSERT_TRUE(history.Redo(world).has_value());
	ASSERT_EQ(world.sent.size(), 1u);
	EXPECT_EQ(world.sent.front().kind, NetworkWorld::Sent::Kind::Remove);
	EXPECT_EQ(world.sent.front().entity, Thing(12));
}

TEST(EditorHistoryNetwork, TheThingOfAnAnswerKeepsOneNumber)
{
	EditorHistory history;
	// The turn puts a placement in the made list before the answer comes, as each computer does
	history.AddMade(Thing(20));
	const auto request = history.NewRequest(0);
	history.Push({.label = "Place Oak", .changes = {MakeChange {.awaits = request}}});
	history.Answer(request, {.entity = Thing(20), .recipe = TreeAt(0.0f, 0.0f)});
	EXPECT_EQ(history.MadeRefs().size(), 1u);

	// A make again of that thing gives a new entity that the turn also puts in the made list. The old number stays.
	NetworkWorld world(history);
	ASSERT_TRUE(history.Undo(world).has_value());
	world.AnswerAll();
	ASSERT_TRUE(history.Redo(world).has_value());
	history.AddMade(Thing(21));
	world.AnswerAll(Thing(21));
	ASSERT_EQ(history.MadeRefs().size(), 1u);
	EXPECT_EQ(history.Resolve(history.MadeRefs().front()), Thing(21));
}

TEST(EditorHistoryNetwork, UndoAndRedoWaitForTheOpenRequests)
{
	EditorHistory history;
	const auto ref = history.Track(Thing(3));
	history.Push({.label = "Move", .changes = {PoseChange {.ref = ref, .before = At(0.0f, 0.0f), .after = At(1.0f, 0.0f)}}});
	// A move of a drag that has no answer yet holds undo
	const auto open = history.NewRequest(100);
	NetworkWorld world(history);
	EXPECT_FALSE(history.CanUndo());
	EXPECT_FALSE(history.Undo(world).has_value());
	EXPECT_TRUE(world.sent.empty());

	// A request with no answer for too long counts as refused
	history.ForgetOldRequests(100 + EditorHistory::k_MostAnswerTurns);
	EXPECT_TRUE(history.IsOpen(open));
	history.ForgetOldRequests(101 + EditorHistory::k_MostAnswerTurns);
	EXPECT_FALSE(history.IsOpen(open));
	EXPECT_TRUE(history.CanUndo());
}

TEST(EditorHistoryNetwork, ARequestNumberIsNeverZeroOrOpen)
{
	EditorHistory history;
	const auto first = history.NewRequest(0);
	EXPECT_NE(first, 0);
	// The numbers go round past 0 and past a number that still waits
	Request last = first;
	for (int i = 0; i < 65534; ++i)
	{
		last = history.NewRequest(0);
		history.Answer(last, {});
	}
	EXPECT_NE(last, 0);
	const auto next = history.NewRequest(0);
	EXPECT_NE(next, 0);
	EXPECT_NE(next, first);
	EXPECT_TRUE(history.IsOpen(first));

	// A clear, as at a new land, closes every request
	history.Clear();
	EXPECT_FALSE(history.Waiting());
}
