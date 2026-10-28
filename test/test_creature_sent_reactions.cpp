/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The reactions to strokes and slaps that the hand sent, and how a reaction that waits for its turn stops a new one

#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureSentReactions.h"
#include "Creature/CreatureShownFace.h"
#include "ECS/Components/ShownCreatureFace.h"
#include "ECS/Components/Transform.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ECS/Registry.h"
#include "ECS/ShownCreatureFaces.h"
#include "ECS/SyncedState.h"

using namespace openblack;
using creature_hand::SentReactions;
using creature_hand::TakesReaction;

namespace
{
constexpr auto k_Tiger = static_cast<entt::entity>(3);
constexpr auto k_Cow = static_cast<entt::entity>(7);
} // namespace

TEST(CreatureSentReactions, ACountedReactionWaitsUntilATurnDoesIt)
{
	SentReactions sent;
	EXPECT_FALSE(sent.Waiting(k_Tiger));
	sent.Sent(k_Tiger, true);
	EXPECT_TRUE(sent.Waiting(k_Tiger));
	EXPECT_FALSE(sent.Waiting(k_Cow));
	sent.Done();
	EXPECT_FALSE(sent.Waiting(k_Tiger));
	EXPECT_TRUE(sent.Empty());
}

TEST(CreatureSentReactions, AReactionThatTheHandDidNotCountDoesNotWait)
{
	// A stroke that the creature refused still pulls the face, so the hand sends it, but it does not count
	SentReactions sent;
	sent.Sent(k_Tiger, false);
	EXPECT_FALSE(sent.Waiting(k_Tiger));
	EXPECT_FALSE(sent.Empty());
}

TEST(CreatureSentReactions, TheTurnsDoTheReactionsInTheOrderThatTheHandSentThem)
{
	SentReactions sent;
	sent.Sent(k_Tiger, false);
	sent.Sent(k_Tiger, true);
	// The turn did the refused stroke: the counted one still waits
	sent.Done();
	EXPECT_TRUE(sent.Waiting(k_Tiger));
	sent.Done();
	EXPECT_FALSE(sent.Waiting(k_Tiger));
	// A turn that does a reaction of this computer with none sent changes nothing
	sent.Done();
	EXPECT_TRUE(sent.Empty());
}

TEST(CreatureSentReactions, ClearForgetsAllTheReactions)
{
	SentReactions sent;
	sent.Sent(k_Tiger, true);
	sent.Sent(k_Cow, true);
	sent.Clear();
	EXPECT_FALSE(sent.Waiting(k_Tiger));
	EXPECT_FALSE(sent.Waiting(k_Cow));
}

TEST(CreatureSentReactions, AWaitingReactionStopsASlapAndAStroke)
{
	// The slap comes a frame after a stroke that waits for its turn: the creature does not reel and the slap does not count
	EXPECT_FALSE(TakesReaction(true, creature_feedback::k_SlapInterruptsAfter, true));
	EXPECT_FALSE(TakesReaction(true, creature_feedback::k_StrokeInterruptsAfter, true));
	// With no reaction that waits, the body decides
	EXPECT_TRUE(TakesReaction(false, creature_feedback::k_SlapInterruptsAfter, true));
	EXPECT_FALSE(TakesReaction(false, creature_feedback::k_SlapInterruptsAfter, false));
}

TEST(CreatureSentReactions, AReactionWithNoLimitInterruptsAWaitingReaction)
{
	// A reaction that interrupts an action at any point also interrupts one at its start
	EXPECT_TRUE(TakesReaction(true, 0.0f, true));
	// The body still has to take it
	EXPECT_FALSE(TakesReaction(true, 0.0f, false));
}

TEST(CreatureShownFace, TheBodyShowsTheShownFaceInPlaceOfTheFaceOfTheWorld)
{
	const creature_face::Request smile {
	    .face = creature_face::Face::Smile, .milliseconds = 2000.0f, .cue = creature_face::Cue::None};
	const creature_layers::FaceLayer world {};
	const auto shown = creature_shown_face::Pull(world, smile);
	EXPECT_EQ(shown.wanted, creature_face::AnimationOf(creature_face::Face::Smile));
	EXPECT_EQ(&creature_shown_face::Shown(world, &shown), &shown);
	EXPECT_EQ(&creature_shown_face::Shown(world, nullptr), &world);
	// The shown face goes when it is the same as the face of the world
	EXPECT_TRUE(creature_shown_face::StaysAfterPose(shown, world));
	EXPECT_FALSE(creature_shown_face::StaysAfterPose(world, world));
}

TEST(CreatureShownFace, AFaceThatTheWorldChangesInADifferentWayReplacesTheShownFace)
{
	const creature_layers::FaceLayer world {.current = 1, .timeMs = 40.0f, .wanted = 1, .remainingMs = 500.0f};
	const creature_layers::FaceLayer shown {.current = 5, .timeMs = 120.0f, .wanted = 5, .remainingMs = 1800.0f};
	EXPECT_EQ(creature_shown_face::AfterWorldWrites(shown, world, world), shown);
	const auto written = creature_layers::PullFace(world, 3, 1000.0f, creature_face::Cue::Anger);
	EXPECT_EQ(creature_shown_face::AfterWorldWrites(shown, world, written), written);
}

TEST(ShownCreatureFaces, AFaceThatTheWorldPullsStartsFromTheShownFace)
{
	using ecs::components::ShownCreatureFace;
	ecs::Registry registry;
	const auto creature = registry.Create();
	const creature_layers::FaceLayer start {.current = 1, .timeMs = 40.0f, .wanted = 1, .remainingMs = 500.0f};
	const creature_layers::FaceLayer stroked {
	    .current = 5, .timeMs = 120.0f, .wanted = 5, .remainingMs = 1800.0f, .cue = creature_face::Cue::None};

	// Without a shown face, only the face of the world changes
	auto world = start;
	const auto pull = [](const creature_layers::FaceLayer& face) {
		return creature_layers::PullFace(face, 3, 1000.0f, creature_face::Cue::Anger);
	};
	ecs::shown_creature_faces::ChangeFace(registry, creature, world, pull);
	EXPECT_EQ(world, pull(start));
	EXPECT_FALSE(registry.AnyOf<ShownCreatureFace>(creature));

	// A pull of the world wants its face on the shown face too, and the face that shows now stays
	world = start;
	registry.Assign<ShownCreatureFace>(creature, ShownCreatureFace {.face = stroked, .worldFace = start});
	ecs::shown_creature_faces::ChangeFace(registry, creature, world, pull);
	const auto& shown = registry.Get<ShownCreatureFace>(creature);
	EXPECT_EQ(shown.face.current, stroked.current);
	EXPECT_EQ(shown.face.timeMs, stroked.timeMs);
	EXPECT_EQ(shown.face.wanted, 3u);
	EXPECT_EQ(shown.face.remainingMs, 1000.0f);
	EXPECT_EQ(shown.face.cue, creature_face::Cue::Anger);
	EXPECT_EQ(shown.worldFace, world);
	// A pull of the world goes through, so the next pose keeps the shown face
	EXPECT_EQ(creature_shown_face::AfterWorldWrites(shown.face, shown.worldFace, world), shown.face);

	// A relax of the world relaxes the shown face, also when the world has no wanted face
	auto relaxedWorld = creature_layers::RelaxFace(start);
	ecs::shown_creature_faces::ChangeFace(registry, creature, relaxedWorld, [](const creature_layers::FaceLayer& face) {
		return creature_layers::RelaxFace(face);
	});
	EXPECT_FALSE(registry.Get<ShownCreatureFace>(creature).face.wanted.has_value());
	EXPECT_EQ(registry.Get<ShownCreatureFace>(creature).face.current, stroked.current);

	// A drop of the world drops the face that shows
	ecs::shown_creature_faces::ChangeFace(
	    registry, creature, world, [](const creature_layers::FaceLayer& face) { return creature_layers::DropFace(face); });
	EXPECT_FALSE(registry.Get<ShownCreatureFace>(creature).face.current.has_value());
	EXPECT_FALSE(registry.Get<ShownCreatureFace>(creature).face.wanted.has_value());
}

TEST(ShownCreatureFaces, TheWorldSaveAndTheSyncedStateDoNotKeepTheShownFace)
{
	using namespace ecs::components;
	using namespace ecs::reflection;
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry registry;
	const auto creature = registry.create();
	registry.emplace<Transform>(creature,
	                            Transform {.position = {1.0f, 2.0f, 3.0f}, .rotation = {}, .scale = {1.0f, 1.0f, 1.0f}});
	const auto bytesBefore = SaveWorld(registry, context);
	const auto hashBefore = synced_state::HashOf(registry, context, {}).hash;
	const auto ordersBefore = RowOrders(registry, context);

	registry.emplace<ShownCreatureFace>(
	    creature, ShownCreatureFace {.face = {.current = 5, .timeMs = 120.0f, .wanted = 5, .remainingMs = 1800.0f}});
	// The archive knows the component, and it does not keep it
	EXPECT_TRUE(UnregisteredStorages(registry, context).empty());
	WorldSaveReport report;
	const auto bytes = SaveWorld(registry, context, &report);
	EXPECT_TRUE(report.unregisteredComponents.empty());
	EXPECT_EQ(bytes, bytesBefore);
	EXPECT_EQ(SaveWorld(*SnapshotWorld(registry, context), context), bytesBefore);
	// The hash of the synced state does not take it, also without the presentation parts
	EXPECT_EQ(synced_state::HashOf(registry, context, {}).hash, hashBefore);
	EXPECT_EQ(RowOrders(registry, context), ordersBefore);

	entt::registry loaded;
	ASSERT_TRUE(LoadWorld(loaded, context, bytes, WorldLoadMode::Exact).has_value());
	EXPECT_TRUE(loaded.all_of<Transform>(creature));
	EXPECT_FALSE(loaded.all_of<ShownCreatureFace>(creature));
}
