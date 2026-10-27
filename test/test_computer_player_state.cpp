/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/PlayerRelations.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ECS/SyncedState.h"

using namespace openblack;
using namespace openblack::ecs::reflection;
using openblack::ecs::components::ComputerAttitudes;
using openblack::ecs::components::ComputerPlayer;
using openblack::ecs::components::PlayerAllies;
using openblack::ecs::components::PlayerInfluencePower;

namespace
{

/// A computer god in the middle of an action: its mind chose a leaf, its hand has steps, and it cast a miracle
ComputerPlayer GodInAnAction(entt::entity town, entt::entity spell)
{
	ComputerPlayer god {.switchedOn = true, .marked = true, .started = true, .creatureLike = PlayerNames::PLAYER_ONE};
	god.mind = computer_mind::MakeMind();
	god.mind.nodes.at(3).suppression = 0.25f;
	god.mind.nodes.at(3).object = town;
	god.mind.nodes.at(3).bestLeaf = 9;
	god.mind.doing = 0.75f;
	god.mind.lastEmptyTurn = 120;
	god.mind.castRequested.at(4) = 7;
	god.hand.hand = {10.0f, 20.0f, 30.0f};
	god.hand.part = 1;
	god.hand.counter = 12;
	god.hand.castTarget = town;
	computer_hand::Add(god.hand, computer_hand::StepKind::CastSpellAtObject,
	                   {computer_hand::Argument {.object = town}, computer_hand::Argument {.number = 3}});
	god.casts.push_back(spell);
	return god;
}

/// Saves the world of the registry and loads it into a new registry. The loaded world must give the same bytes again.
entt::registry SavedAndLoaded(const entt::registry& original, const entt::meta_ctx& context)
{
	WorldSaveReport report;
	const auto bytes = SaveWorld(original, context, &report);
	EXPECT_TRUE(report.unsavedValues.empty());
	entt::registry loaded;
	EXPECT_TRUE(LoadWorld(loaded, context, bytes, WorldLoadMode::Exact).has_value());
	EXPECT_EQ(SaveWorld(loaded, context), bytes);
	return loaded;
}

/// The hash of one part of the synced state, none when the state has no such part
std::optional<uint64_t> PartOf(const synced_state::Hash& hash, std::string_view name)
{
	const auto found = std::ranges::find(hash.parts, std::string(name), [](const auto& part) { return part.first; });
	return found != hash.parts.end() ? std::optional(found->second) : std::nullopt;
}

} // namespace

// The mind, the hand and the casts of a computer god are in the synced state of each turn, as the part
// "ComputerPlayer", and the hash of each field names them
TEST(ComputerPlayerState, TheMindTheHandAndTheCastsAreInTheSyncedHash)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry registry;
	const auto god = registry.create();
	const auto town = registry.create();
	const auto spell = registry.create();
	registry.emplace<ComputerPlayer>(god, GodInAnAction(town, spell));
	const auto before = synced_state::HashOf(registry, context, synced_state::PresentationParts(), true);
	EXPECT_TRUE(before.unregistered.empty());
	ASSERT_TRUE(PartOf(before, "ComputerPlayer").has_value());
	for (const auto* field : {"ComputerPlayer.mind", "ComputerPlayer.hand", "ComputerPlayer.casts"})
	{
		EXPECT_TRUE(PartOf(before, field).has_value()) << field;
	}

	// A change deep in each field changes the hash, the part and the hash of that field only
	const auto changes = [&](auto&& change, std::string_view field) {
		auto& state = registry.get<ComputerPlayer>(god);
		const auto kept = state;
		change(state);
		const auto after = synced_state::HashOf(registry, context, synced_state::PresentationParts(), true);
		EXPECT_NE(after.hash, before.hash) << field;
		EXPECT_NE(PartOf(after, "ComputerPlayer"), PartOf(before, "ComputerPlayer")) << field;
		for (const auto* each : {"ComputerPlayer.mind", "ComputerPlayer.hand", "ComputerPlayer.casts"})
		{
			EXPECT_EQ(PartOf(after, each) != PartOf(before, each), field == each) << field << " " << each;
		}
		state = kept;
	};
	changes([](ComputerPlayer& state) { state.mind.nodes.at(40).lastPerformed = 99; }, "ComputerPlayer.mind");
	changes([](ComputerPlayer& state) { state.mind.castRequested.at(4) = 8; }, "ComputerPlayer.mind");
	changes([](ComputerPlayer& state) { state.hand.steps.at(0).arguments.at(1).number = 4; }, "ComputerPlayer.hand");
	changes([](ComputerPlayer& state) { state.hand.progress = 0.5f; }, "ComputerPlayer.hand");
	changes([town](ComputerPlayer& state) { state.casts.push_back(town); }, "ComputerPlayer.casts");
	EXPECT_EQ(synced_state::HashOf(registry, context, synced_state::PresentationParts(), true).hash, before.hash);
}

// A world state of a turn keeps all of the mind of a computer god, as a client that comes back loads it
TEST(ComputerPlayerState, TheMindComesBackWholeFromAWorldState)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry original;
	const auto god = original.create();
	const auto town = original.create();
	const auto spell = original.create();
	original.emplace<ComputerPlayer>(god, GodInAnAction(town, spell));
	WorldSaveReport report;
	const auto bytes = SaveWorld(original, context, &report);
	EXPECT_TRUE(report.unsavedValues.empty());

	entt::registry loaded;
	ASSERT_TRUE(LoadWorld(loaded, context, bytes, WorldLoadMode::Exact).has_value());
	const auto& back = loaded.get<ComputerPlayer>(god);
	const auto& kept = original.get<ComputerPlayer>(god);
	EXPECT_TRUE(back.switchedOn && back.marked && back.started);
	EXPECT_EQ(back.creatureLike, kept.creatureLike);
	ASSERT_EQ(back.mind.nodes.size(), kept.mind.nodes.size());
	for (size_t i = 0; i < kept.mind.nodes.size(); ++i)
	{
		const auto& a = back.mind.nodes.at(i);
		const auto& b = kept.mind.nodes.at(i);
		EXPECT_EQ(a.kind, b.kind) << i;
		EXPECT_EQ(a.parent, b.parent) << i;
		EXPECT_EQ(a.children, b.children) << i;
		EXPECT_EQ(a.suppression, b.suppression) << i;
		EXPECT_EQ(a.weight, b.weight) << i;
		EXPECT_EQ(a.bestLeaf, b.bestLeaf) << i;
		EXPECT_EQ(a.object, b.object) << i;
	}
	EXPECT_EQ(back.mind.doing, kept.mind.doing);
	EXPECT_EQ(back.mind.lastEmptyTurn, kept.mind.lastEmptyTurn);
	EXPECT_EQ(back.mind.castRequested, kept.mind.castRequested);
	EXPECT_EQ(back.hand.hand, kept.hand.hand);
	EXPECT_EQ(back.hand.part, kept.hand.part);
	EXPECT_EQ(back.hand.counter, kept.hand.counter);
	EXPECT_EQ(back.hand.castTarget, town);
	ASSERT_EQ(back.hand.steps.size(), 1u);
	EXPECT_EQ(back.hand.steps.at(0).kind, computer_hand::StepKind::CastSpellAtObject);
	EXPECT_EQ(back.hand.steps.at(0).arguments.at(0).object, town);
	EXPECT_EQ(back.hand.steps.at(0).arguments.at(1).number, 3);
	ASSERT_EQ(back.casts.size(), 1u);
	EXPECT_EQ(back.casts.at(0), spell);

	// The loaded world gives the same synced hash, and its save the same bytes
	EXPECT_EQ(synced_state::HashOf(loaded, context, synced_state::PresentationParts(), false).hash,
	          synced_state::HashOf(original, context, synced_state::PresentationParts(), false).hash);
	EXPECT_EQ(SaveWorld(loaded, context), bytes);
}

// The weights and the suppressions that land scripts gave each node come back from a saved world
TEST(ComputerPlayerState, TheWeightsComeBackFromASavedWorld)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry original;
	const auto god = original.create();
	auto& state = original.emplace<ComputerPlayer>(god);
	state.mind = computer_mind::MakeMind();
	ASSERT_TRUE(computer_mind::SetPersonality(state.mind, "AttendTownDesires", 0.3125f));
	ASSERT_TRUE(computer_mind::SetSuppression(state.mind, "ExpandInfluence", 0.0625f));
	state.mind.nodes.back().weight = -2.5f;

	const auto loaded = SavedAndLoaded(original, context);
	const auto& back = loaded.get<ComputerPlayer>(god).mind.nodes;
	ASSERT_EQ(back.size(), state.mind.nodes.size());
	for (size_t i = 0; i < back.size(); ++i)
	{
		EXPECT_EQ(back.at(i).weight, state.mind.nodes.at(i).weight) << i;
		EXPECT_EQ(back.at(i).suppression, state.mind.nodes.at(i).suppression) << i;
	}
}

// The queue of script actions, with the names, the things and the turns, comes back in its order
TEST(ComputerPlayerState, TheActionQueueComesBackFromASavedWorld)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry original;
	const auto god = original.create();
	const auto town = original.create();
	const auto villager = original.create();
	auto& state = original.emplace<ComputerPlayer>(god);
	state.mind = computer_mind::MakeMind();
	computer_mind::QueueAction(state.mind, "DIRECTPickupAndDropObject", villager, town, 40);
	computer_mind::QueueAction(state.mind, "GiveWorshippersFood", town, entt::null, 41);
	state.mind.scriptHold = true;
	state.mind.scriptHand = true;
	state.mind.paused = true;

	const auto loaded = SavedAndLoaded(original, context);
	const auto& back = loaded.get<ComputerPlayer>(god).mind;
	ASSERT_EQ(back.queue.size(), 2u);
	EXPECT_EQ(back.queue.at(0).name, "DIRECTPickupAndDropObject");
	EXPECT_EQ(back.queue.at(0).first, villager);
	EXPECT_EQ(back.queue.at(0).second, town);
	EXPECT_EQ(back.queue.at(0).turn, 40u);
	EXPECT_EQ(back.queue.at(1).name, "GiveWorshippersFood");
	EXPECT_EQ(back.queue.at(1).first, town);
	EXPECT_EQ(back.queue.at(1).second, entt::entity {entt::null});
	EXPECT_EQ(back.queue.at(1).turn, 41u);
	EXPECT_TRUE(back.scriptHold);
	EXPECT_TRUE(back.scriptHand);
	EXPECT_TRUE(back.paused);
}

// The seen list and the speed of the hand come back from a saved world, the newest thing first
TEST(ComputerPlayerState, TheSeenListAndTheSpeedComeBackFromASavedWorld)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry original;
	const auto god = original.create();
	const auto forest = original.create();
	const auto seed = original.create();
	auto& state = original.emplace<ComputerPlayer>(god);
	state.seen = {{.thing = seed, .turn = 900}, {.thing = forest, .turn = 12}};
	state.speed = 45.5f;
	state.hand.speed = 45.5f;

	const auto loaded = SavedAndLoaded(original, context);
	const auto& back = loaded.get<ComputerPlayer>(god);
	ASSERT_EQ(back.seen.size(), 2u);
	EXPECT_EQ(back.seen.at(0).thing, seed);
	EXPECT_EQ(back.seen.at(0).turn, 900u);
	EXPECT_EQ(back.seen.at(1).thing, forest);
	EXPECT_EQ(back.seen.at(1).turn, 12u);
	EXPECT_EQ(back.speed, 45.5f);
	EXPECT_EQ(back.hand.speed, 45.5f);
}

// The casts of the god come back from a saved world, the newest first
TEST(ComputerPlayerState, TheCastsComeBackFromASavedWorld)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry original;
	const auto god = original.create();
	const auto first = original.create();
	const auto second = original.create();
	original.emplace<ComputerPlayer>(god).casts = {second, first};

	const auto loaded = SavedAndLoaded(original, context);
	const auto& back = loaded.get<ComputerPlayer>(god).casts;
	ASSERT_EQ(back.size(), 2u);
	EXPECT_EQ(back.at(0), second);
	EXPECT_EQ(back.at(1), first);
}

// The attitudes, the allies and the influence power of a player come back from a saved world, each by player number
TEST(ComputerPlayerState, TheAttitudesTheAlliesAndTheInfluencePowerComeBackFromASavedWorld)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry original;
	const auto player = original.create();
	auto& attitudes = original.emplace<ComputerAttitudes>(player);
	attitudes.kept.at(0) = -1.5f;
	attitudes.kept.at(31) = 0.25f;
	auto& allies = original.emplace<PlayerAllies>(player);
	allies.strength.at(2) = 0.75f;
	allies.strength.at(30) = 0.05f;
	original.emplace<PlayerInfluencePower>(player, PlayerInfluencePower {.power = 812.5f, .average = 640.25f});

	const auto loaded = SavedAndLoaded(original, context);
	EXPECT_EQ(loaded.get<ComputerAttitudes>(player).kept, attitudes.kept);
	EXPECT_EQ(loaded.get<PlayerAllies>(player).strength, allies.strength);
	EXPECT_EQ(loaded.get<PlayerInfluencePower>(player).power, 812.5f);
	EXPECT_EQ(loaded.get<PlayerInfluencePower>(player).average, 640.25f);
}
