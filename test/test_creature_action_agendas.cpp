/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>

#include <Common/GameRandom.h>
#include <gtest/gtest.h>

#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;
namespace plan_actions = openblack::creature_plan_actions;

namespace
{
uint32_t AlwaysFirst(uint32_t /*range*/)
{
	return 0;
}

uint32_t NeverFirst(uint32_t range)
{
	return range - 1;
}
} // namespace

// "force MyCreature CREATURE_HURL TinyStone with Trainer": with its hand empty it may show its anger, picks up the trainer,
// goes to where it can throw at the stone and throws the trainer at it
TEST(CreatureActionAgendas, HurlingPicksUpWhatToThrowThenThrowsItAtTheThing)
{
	const auto agenda = HurlAt(4, 9, {10.0f, 20.0f}, 15.0f, false, AlwaysFirst);
	ASSERT_EQ(agenda.size(), 4);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[0].animation, animations::k_Angry);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Amazed);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.object, 4u);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::ToThrowPosition);
	EXPECT_EQ(agenda[2].movement.object, 9u);
	EXPECT_FLOAT_EQ(agenda[2].movement.maxDistance, 15.0f);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[3].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[3].order.point, glm::vec2(10.0f, 20.0f));
	// The throw finishes the action, which sees to whichever desire it served
	EXPECT_EQ(agenda[3].effect, Effect::None);
	ASSERT_TRUE(agenda[3].gaze.has_value());
	EXPECT_EQ(agenda[3].gaze->object, 9u);
	// Most of the time it doesn't show its anger first; with something in its hand already it throws that
	EXPECT_EQ(HurlAt(4, 9, {}, 15.0f, false, NeverFirst).size(), 3);
	const auto holding = HurlAt(4, 9, {}, 15.0f, true, AlwaysFirst);
	ASSERT_EQ(holding.size(), 2);
	EXPECT_EQ(holding[0].movement.kind, Movement::Kind::ToThrowPosition);
}

// A hurl, a script's or the creature's own, is made from the thing to throw it is given
TEST(CreatureActionAgendas, AScriptsHurlNeedsNoPlaceOfItsOwn)
{
	const auto* hurl = plan_actions::For("Hurl");
	ASSERT_NE(hurl, nullptr);
	plan_actions::Situation situation;
	EXPECT_FALSE(plan_actions::Agenda(*hurl, 9, {1.0f, 2.0f}, situation, NeverFirst).has_value());
	situation.instrument = 4;
	situation.height = 15.0f;
	const auto agenda = plan_actions::Agenda(*hurl, 9, {1.0f, 2.0f}, situation, NeverFirst);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ(agenda->front().order.object, 4u);
	EXPECT_EQ(agenda->back().order.point, glm::vec2(1.0f, 2.0f));
}

// "force Guide CREATURE_SLEEP Guide": the guide goes home to sleep
TEST(CreatureActionAgendas, SleepingAtHomeWalksHomeFacesDownTheSlopeAndSleepsUntilRested)
{
	const auto agenda = SleepAtHome({100.0f, 200.0f}, 30.0f, AlwaysFirst);
	ASSERT_EQ(agenda.size(), 5);
	EXPECT_EQ(agenda[0].animation, animations::k_Tired);
	EXPECT_TRUE(agenda[0].sleepyEyes);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[1].movement.point, glm::vec2(100.0f, 200.0f));
	// No further than 5 from home, however tall it is
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 5.0f);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::FaceDownSlope);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Static);
	EXPECT_TRUE(agenda[3].untilRested);
	EXPECT_TRUE(agenda[3].closedEyes);
	EXPECT_EQ(agenda[3].effect, Effect::Slept);
	EXPECT_EQ(agenda[4].animation, animations::k_Confused);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Grimace);
	EXPECT_FLOAT_EQ(SleepAtHome({}, 3.0f, NeverFirst).front().movement.maxDistance, 3.0f);
	// Without a home it can't
	const auto* sleep = plan_actions::For("SleepAtHome");
	ASSERT_NE(sleep, nullptr);
	EXPECT_FALSE(plan_actions::Possible(*sleep, {}));
}

TEST(CreatureActionAgendas, APooAtHomeWalksHomeThenDoesThePooByItself)
{
	const auto agenda = PooAtHome({100.0f, 200.0f}, 30.0f, 8, NeverFirst);
	ASSERT_EQ(agenda.size(), 4);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(100.0f, 200.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 60.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(agenda[1].movement.object, 8u);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::TurnToFaceAwayFromObject);
	EXPECT_EQ(agenda[3].effect, Effect::Poo);
	// Only with a home within 20 of it, by the game's map distance, which is near but not exact
	const auto* poo = plan_actions::For("PooAtHome");
	ASSERT_NE(poo, nullptr);
	EXPECT_FALSE(plan_actions::Possible(*poo, {}));
	EXPECT_TRUE(plan_actions::Possible(*poo, {.home = glm::vec2(19.9f, 0.0f)}));
	EXPECT_FALSE(plan_actions::Possible(*poo, {.home = glm::vec2(20.1f, 0.0f)}));
}

// "force MyCreature CREATURE_EXAMINE_BY_LOOKING Guide"
TEST(CreatureActionAgendas, LookingSomethingOverGoesNearItFacesItAndWatchesItsFoot)
{
	const auto agenda = ExamineByLooking(3, 10.0f, 40.0f);
	ASSERT_EQ(agenda.size(), 4);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	// The thing's height when it is taller than two and a half of its own
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 40.0f);
	EXPECT_FLOAT_EQ(ExamineByLooking(3, 10.0f, 5.0f)[0].movement.maxDistance, 25.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 2.0f);
	EXPECT_FLOAT_EQ(agenda[2].seconds, 1.1f);
	ASSERT_TRUE(agenda[2].gaze.has_value());
	EXPECT_TRUE(agenda[2].gaze->bottom);
	// Then it takes in what kind of thing it is
	EXPECT_EQ(agenda[3].kind, Step::Kind::Observe);
	EXPECT_EQ(agenda[3].object, 3u);
}

// Taking in what a thing is happens at once, as the step starts, and says which thing it was
TEST(CreatureActionAgendas, TakingInAThingIsDoneAtOnce)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, {{.kind = Step::Kind::Observe, .object = 3}});
	const auto commands = Think(mind, {.seconds = 0.1f}, NeverFirst);
	EXPECT_EQ(commands.effect, Effect::Observed);
	EXPECT_EQ(commands.effectObject, 3u);
	EXPECT_EQ(mind.step, 1u);
}

// "InspectCreature": near it at four of its own heights, a tenth of a second facing it, curious, then watching it
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureActionAgendas, LookingACreatureOverGoesNearItFacesItAndWatchesIt)
{
	const auto agenda = InspectCreature(7, 10.0f);
	ASSERT_EQ(agenda.size(), 3);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 40.0f);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[1].seconds, 0.1f);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Curiosity);
	EXPECT_EQ(agenda[2].kind, Step::Kind::Inspect);
	EXPECT_EQ(agenda[2].object, 7u);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Curiosity);
	ASSERT_TRUE(agenda[2].gaze.has_value());
	EXPECT_EQ(agenda[2].gaze->object, 7u);
	EXPECT_NE(plan_actions::For("InspectCreature"), nullptr);
}

// The watch lasts five seconds and a draw of up to ten more, in whole turns from the turn after it starts, and then it
// has looked the creature over
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureActionAgendas, LookingACreatureOverLastsFiveToFifteenSeconds)
{
	for (const auto& [draw, turns] : {std::pair {0u, 50u}, std::pair {0xfffeu, 149u}, std::pair {0x8000u, 100u}})
	{
		SCOPED_TRACE(draw);
		IdleMind mind;
		Plan(mind, Activity::Planned, InspectCreature(7, 10.0f));
		mind.step = 2;
		// The faces it pulls draw too; only the time's draw is fixed
		const auto random = [draw](uint32_t range) { return range == game_random::k_FloatRandRange ? draw : 0u; };
		const creature_mind::Senses senses {.seconds = 0.1f};
		const auto faces = [](uint32_t /*range*/) { return 0u; };
		auto commands = Think(mind, senses, random);
		EXPECT_EQ(mind.inspectTurns, turns);
		uint32_t watched = 0;
		while (commands.effect != Effect::Inspected && watched < 1000)
		{
			commands = Think(mind, senses, faces);
			++watched;
		}
		EXPECT_EQ(watched, turns);
		EXPECT_EQ(commands.effectObject, 7u);
		EXPECT_EQ(mind.step, 3u);
	}
}

// "LookAtFlyingObject": half the time pointing at it two seconds first, then watching it, amazed, its head on the thing
// itself
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureActionAgendas, LookingAtAFlyingThingPointsHalfTheTimeThenWatches)
{
	const auto pointing = LookAtFlyingObject(4, [](uint32_t range) {
		EXPECT_EQ(range, 2u);
		return 0u;
	});
	ASSERT_EQ(pointing.size(), 2);
	EXPECT_EQ(pointing[0].kind, Step::Kind::Object);
	EXPECT_EQ(pointing[0].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(pointing[0].order.object, 4u);
	EXPECT_EQ(pointing[0].order.seconds, std::optional(2.0f));
	EXPECT_EQ(pointing[0].face, creature_face::Cue::Amazed);
	EXPECT_EQ(pointing[1].kind, Step::Kind::WatchFlying);
	EXPECT_EQ(pointing[1].face, creature_face::Cue::Amazed);
	ASSERT_TRUE(pointing[1].gaze.has_value());
	EXPECT_TRUE(pointing[1].gaze->itself);
	const auto watching = LookAtFlyingObject(4, [](uint32_t /*range*/) { return 1u; });
	ASSERT_EQ(watching.size(), 1);
	EXPECT_EQ(watching[0].kind, Step::Kind::WatchFlying);
	EXPECT_NE(plan_actions::For("LookAtFlyingObject"), nullptr);
}

// The watch goes on while the thing flies and its whole seconds come under the limit drawn each turn
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureActionAgendas, WatchingAFlyingThingLastsWhileItFliesUpToFourSeconds)
{
	EXPECT_TRUE(KeepsWatchingFlying(19, 10, 2.0f, true));
	EXPECT_FALSE(KeepsWatchingFlying(20, 10, 2.0f, true));
	EXPECT_TRUE(KeepsWatchingFlying(39, 10, 3.5f, true));
	EXPECT_FALSE(KeepsWatchingFlying(1, 10, 3.5f, false));
	// The lowest draw every turn: it counts the turn it starts on, and stops on the twentieth
	IdleMind mind;
	Plan(mind, Activity::Planned, LookAtFlyingObject(4, [](uint32_t /*range*/) { return 1u; }));
	creature_mind::Senses senses {.seconds = 0.1f, .objectInTheAir = true};
	const auto lowest = [](uint32_t /*range*/) { return 0u; };
	uint32_t turns = 0;
	while (mind.step == 0 && turns < 100)
	{
		(void)Think(mind, senses, lowest);
		++turns;
	}
	EXPECT_EQ(turns, 20u);
	// Down on the land, it stops at once
	Plan(mind, Activity::Planned, LookAtFlyingObject(4, [](uint32_t /*range*/) { return 1u; }));
	senses.objectInTheAir = false;
	(void)Think(mind, senses, lowest);
	EXPECT_EQ(mind.step, 1u);
}

// "LookAtHand": watching the player's hand, curious, its head on the hand, for a turn and a half of its own
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureActionAgendas, LookingAtTheHandWatchesIt)
{
	const auto agenda = LookAtHand();
	ASSERT_EQ(agenda.size(), 1);
	EXPECT_EQ(agenda[0].kind, Step::Kind::WatchHand);
	EXPECT_EQ(agenda[0].turns, std::optional<uint32_t>(15));
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Curiosity);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_TRUE(agenda[0].gaze->hand);
	ASSERT_NE(plan_actions::For("LookAtHand"), nullptr);
	EXPECT_EQ(plan_actions::For("LookAtHand")->build, plan_actions::Build::LookAtHand);
}

// The watch is done once its whole seconds pass those of its turns, five at most; never while it turns to the hand
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureActionAgendas, WatchingTheHandLastsItsTurnsUpToFiveSeconds)
{
	// A turn and a half is one whole second: done on the twentieth turn
	EXPECT_FALSE(WatchedHandEnough(19, 15, 10));
	EXPECT_TRUE(WatchedHandEnough(20, 15, 10));
	// A minute or more is five seconds: done on the sixtieth
	EXPECT_FALSE(WatchedHandEnough(59, 900, 10));
	EXPECT_TRUE(WatchedHandEnough(60, 900, 10));

	IdleMind mind;
	Plan(mind, Activity::Planned, LookAtHand());
	creature_mind::Senses senses {.seconds = 0.1f};
	senses.hand = creature_mind::Senses::CameraView {.distance = 30.0f, .groundDistance = 20.0f, .point = {0.0f, 20.0f}};
	uint32_t turns = 0;
	while (mind.step == 0 && turns < 100)
	{
		const auto commands = Think(mind, senses, NeverFirst);
		EXPECT_FALSE(commands.move.has_value());
		++turns;
	}
	EXPECT_EQ(turns, 20u);
	// More than a quarter turn away it turns to the hand, and isn't done while it turns
	Plan(mind, Activity::Planned, LookAtHand());
	senses.hand->turn = 2.0f;
	for (int i = 0; i < 30; ++i)
	{
		const auto commands = Think(mind, senses, NeverFirst);
		ASSERT_TRUE(commands.move.has_value());
		EXPECT_EQ(commands.move->kind, Movement::Kind::TurnToFace);
	}
	EXPECT_EQ(mind.step, 0u);
	// Without a player there is no hand to watch
	senses.hand.reset();
	(void)Think(mind, senses, NeverFirst);
	EXPECT_EQ(mind.step, 1u);
}

// A body that turns or does an action does not start a new turn to the hand, so the turn that it does now comes to its
// end. The watch continues while the body moves.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureActionAgendas, WatchingTheHandDoesNotStartATurnAgainWhileTheBodyIsBusy)
{
	const creature_mind::Senses::CameraView behind {.distance = 30.0f, .groundDistance = 20.0f, .turn = 2.7f};
	EXPECT_TRUE(StartsWatchTurn(behind, false));
	EXPECT_FALSE(StartsWatchTurn(behind, true));
	auto overhead = behind;
	overhead.groundDistance = 0.05f;
	EXPECT_FALSE(StartsWatchTurn(overhead, false));
	auto ahead = behind;
	ahead.turn = -1.0f;
	EXPECT_FALSE(StartsWatchTurn(ahead, false));

	IdleMind mind;
	Plan(mind, Activity::Planned, LookAtHand());
	creature_mind::Senses senses {.seconds = 0.1f};
	senses.hand = behind;
	// The first turn starts the body's turn
	auto commands = Think(mind, senses, NeverFirst);
	ASSERT_TRUE(commands.move.has_value());
	EXPECT_EQ(commands.move->kind, Movement::Kind::TurnToFace);
	// While the body turns, no new turn is started and the watch does not end
	senses.bodyBusy = true;
	senses.moving = true;
	for (int i = 0; i < 100; ++i)
	{
		commands = Think(mind, senses, NeverFirst);
		EXPECT_FALSE(commands.move.has_value());
	}
	EXPECT_EQ(mind.step, 0u);
	// Turned to face the hand and still, the watch ends because it has watched long enough
	senses.bodyBusy = false;
	senses.moving = false;
	senses.hand->turn = 0.0f;
	(void)Think(mind, senses, NeverFirst);
	EXPECT_EQ(mind.step, 1u);
}

// A kind of thing it has looked over is a hundredth as interesting, unless the thing is up to something interesting
TEST(CreatureActionAgendas, LookedOverKindsAreFarLessInteresting)
{
	EXPECT_FLOAT_EQ(CuriosityShare(false, false), 1.0f);
	EXPECT_FLOAT_EQ(CuriosityShare(true, false), 0.01f);
	EXPECT_FLOAT_EQ(CuriosityShare(true, true), 1.0f);
}

// Curiosity and play make some things more useful to act on. The other desires do not.
TEST(CreatureActionAgendas, CuriosityAndPlayMultiplyUsefulness)
{
	using creature_desires::Desire;
	EXPECT_EQ(DesireMultiplier(Desire::Curiosity, {.doingSomethingInteresting = true}), 5.0f);
	EXPECT_EQ(DesireMultiplier(Desire::Curiosity, {.toy = true}), 1.0f);
	EXPECT_EQ(DesireMultiplier(Desire::Play, {.toy = true}), 3.0f);
	EXPECT_EQ(DesireMultiplier(Desire::Play, {.playingFootball = true, .toy = true}), 5.0f);
	EXPECT_EQ(DesireMultiplier(Desire::Play, {.doingSomethingInteresting = true}), 1.0f);
	EXPECT_EQ(DesireMultiplier(Desire::Anger, {.doingSomethingInteresting = true, .playingFootball = true, .toy = true}), 1.0f);
}

// Only objects can be of a kind that the creature examined: towns, forests, temples, miracles, flocks and mists are
// not objects
TEST(CreatureActionAgendas, OnlyObjectsHaveTheirKindLookedOver)
{
	namespace types = creature_tree::belief_types;
	for (const auto type : {types::k_Town, types::k_Forest, types::k_Citadel, types::k_Spell, types::k_Flock})
	{
		EXPECT_FALSE(IsLookedOverAsAKind(type, false)) << type;
	}
	for (const auto type :
	     {types::k_Abode, types::k_CitadelPart, types::k_Tree, types::k_Villager, types::k_Animal, types::k_Creature,
	      types::k_Field, types::k_Rock, types::k_Pot, types::k_SpellIcon, types::k_MobileObject, types::k_TotemStatue})
	{
		EXPECT_TRUE(IsLookedOverAsAKind(type, false)) << type;
	}
	EXPECT_TRUE(IsLookedOverAsAKind(types::k_SpellSeed, false));
	EXPECT_FALSE(IsLookedOverAsAKind(types::k_SpellSeed, true));
}

// A villager is up to something interesting when its state teaches a skill the creature does not know yet: building,
// farming, the totem, the storage pit, fishing or dancing
TEST(CreatureActionAgendas, AVillagerIsInterestingAtASkillTheCreatureDoesNotKnow)
{
	const std::vector<bool> knowsFishing {false, false, false, false, true, false};
	EXPECT_FALSE(DoingSomethingInteresting(k_NoSkillTaught, knowsFishing));
	EXPECT_FALSE(DoingSomethingInteresting(4, knowsFishing));
	EXPECT_TRUE(DoingSomethingInteresting(0, knowsFishing));
	EXPECT_TRUE(DoingSomethingInteresting(5, knowsFishing));
	EXPECT_TRUE(DoingSomethingInteresting(3, {}));
}

// "force Guide CREATURE_SMILE_AT_FRIEND MyCreature" and "force MyCreature CREATURE_WAVE_AT_OBJECT Guide"
TEST(CreatureActionAgendas, SmilingAndWavingGoNearAndFaceTheOther)
{
	const auto smile = SmileAt(5, 10.0f, 2.0f, AlwaysFirst);
	ASSERT_EQ(smile.size(), 3);
	EXPECT_FLOAT_EQ(smile[0].movement.maxDistance, 30.0f);
	EXPECT_EQ(smile[0].face, creature_face::Cue::Smile);
	EXPECT_FLOAT_EQ(smile[1].seconds, 6.0f);
	EXPECT_EQ(smile[2].animation, animations::k_Happy);
	EXPECT_EQ(SmileAt(5, 10.0f, 0.0f, NeverFirst).size(), 2);

	const auto wave = WaveAt(5, 10.0f);
	ASSERT_EQ(wave.size(), 3);
	EXPECT_FLOAT_EQ(wave[0].movement.maxDistance, 35.0f);
	EXPECT_FLOAT_EQ(wave[1].seconds, 1.4f);
	EXPECT_EQ(wave[2].animation, animations::k_FriendlyWave);
}

TEST(CreatureActionAgendas, ShowingOffGoesNearThePartFacesItAndPlaysTwoActions)
{
	const auto first = ShowImpressiveAnimation(5, 10.0f, [](uint32_t) { return 0u; });
	ASSERT_EQ(first.size(), 4);
	EXPECT_EQ(first[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(first[0].movement.maxDistance, 30.0f);
	EXPECT_EQ(first[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(first[1].seconds, 0.1f);
	EXPECT_EQ(first[2].animation, k_LookAtMeAnimation);
	EXPECT_EQ(first[3].animation, animations::k_Impress);
	const auto other = ShowImpressiveAnimation(std::nullopt, 10.0f, [](uint32_t) { return 1u; });
	ASSERT_EQ(other.size(), 2);
	EXPECT_EQ(other[0].animation, k_LookAtMeAnimation);
	EXPECT_EQ(other[1].animation, animations::k_Summon);
}

TEST(CreatureActionAgendas, FrightenedPuttingDownAndLyingDead)
{
	const auto frightened = BeFrightenedOnTheSpot();
	ASSERT_EQ(frightened.size(), 3);
	EXPECT_EQ(frightened[0].animation, animations::k_Frightened);
	EXPECT_FLOAT_EQ(frightened[1].seconds, 3.0f);
	EXPECT_EQ(frightened[1].face, creature_face::Cue::Frightened);
	EXPECT_EQ(frightened[2].animation, animations::k_Frightened);

	const auto putDown = PutDown();
	ASSERT_EQ(putDown.size(), 1);
	EXPECT_EQ(putDown[0].order.kind, ObjectOrder::Kind::Discard);
	EXPECT_EQ(putDown[0].order.animation, creature_throw::k_PutDown);

	const auto dead = DeadForever();
	ASSERT_EQ(dead.size(), 1);
	EXPECT_EQ(dead[0].kind, Step::Kind::Static);
	EXPECT_TRUE(dead[0].holdLoop);
	EXPECT_FLOAT_EQ(dead[0].seconds, 864000.0f);
	EXPECT_EQ(dead[0].effect, Effect::None);
	// It is a faint: those near it react as to a death, and its time counts from when it lies still
	EXPECT_TRUE(dead[0].faint);
}

// The actions Land 1's scripts force have an agenda, a casting one once it knows what it casts
TEST(CreatureActionAgendas, TheForcedActionsOfLandOneCanBeCarriedOut)
{
	// Holding a thing is the leash's orders' own agenda
	for (const auto* name : {"Hurl", "SleepAtHome", "ExamineByLooking", "SmileAtFriend", "PutDown", "WaveAtObject",
	                         "CastHealSpellPU1", "BeIdle", "BeFrightenedOnTheSpot", "DeadForever"})
	{
		SCOPED_TRACE(name);
		EXPECT_NE(plan_actions::For(name), nullptr);
	}
	const auto* heal = plan_actions::For("CastHealSpellPU1");
	ASSERT_NE(heal, nullptr);
	EXPECT_TRUE(plan_actions::IsCast(*heal));
	EXPECT_FALSE(plan_actions::Agenda(*heal, 3, {}, {}, NeverFirst).has_value());
	EXPECT_TRUE(plan_actions::Agenda(*heal, 3, {}, {}, NeverFirst, plan_actions::CastInfo {.magicType = 1, .height = 15.0f})
	                .has_value());
}

// A lesson's DEV_FUNCTION 6 has the creature point out the nearest highlight
TEST(CreatureActionAgendas, PointingOutAHighlightCallsAFarCameraFirst)
{
	const auto near = PointOutHighlight(8, 20.0f);
	ASSERT_EQ(near.size(), 2);
	EXPECT_EQ(near[0].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(near[0].order.object, 8u);
	EXPECT_EQ(near[0].order.seconds, 1.0f);
	EXPECT_EQ(near[0].face, creature_face::Cue::Amazed);
	EXPECT_FLOAT_EQ(near[1].seconds, 2.0f);
	ASSERT_TRUE(near[1].gaze.has_value());
	EXPECT_TRUE(near[1].gaze->camera);

	const auto far = PointOutHighlight(8, 31.0f);
	ASSERT_EQ(far.size(), 4);
	EXPECT_FLOAT_EQ(far[0].seconds, 1.0f);
	EXPECT_EQ(far[0].face, creature_face::Cue::Curiosity);
	EXPECT_EQ(far[1].animation, 69u);
	// Without a camera to show it to, it can't
	const auto* point = plan_actions::For("PointOutHighlight");
	ASSERT_NE(point, nullptr);
	EXPECT_FALSE(plan_actions::Possible(*point, {}));
}

// "force CreatureGuardian CREATURE_SLEEP_BY_OBJECT CreatureGuardian": it walks to the clear place it found until within
// its own radius, may yawn, faces down the slope, sleeps until rested and gets up with a grimace
TEST(CreatureActionAgendas, SleepingByAThingGoesToItsClearPlaceFirst)
{
	const auto agenda = SleepByObject({30.0f, 40.0f}, 6.0f, AlwaysFirst);
	ASSERT_EQ(agenda.size(), 5);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(30.0f, 40.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 6.0f);
	EXPECT_EQ(agenda[1].animation, animations::k_Tired);
	EXPECT_TRUE(agenda[1].sleepyEyes);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::FaceDownSlope);
	EXPECT_TRUE(agenda[3].untilRested);
	EXPECT_EQ(agenda[3].effect, Effect::Slept);
	EXPECT_EQ(agenda[4].animation, animations::k_Confused);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Grimace);
	// Half the time without the yawn
	EXPECT_EQ(SleepByObject({}, 6.0f, NeverFirst).size(), 4);
}

// Without a clear place by the thing it sleeps at home, and without either it can't
TEST(CreatureActionAgendas, SleepingByAThingFallsBackToHome)
{
	const auto* sleep = plan_actions::For("SleepByObject");
	ASSERT_NE(sleep, nullptr);
	plan_actions::Situation situation {.controlledByScript = true, .radius = 6.0f, .height = 15.0f};
	EXPECT_FALSE(plan_actions::Agenda(*sleep, 3, {}, situation, NeverFirst).has_value());
	situation.home = glm::vec2(100.0f, 200.0f);
	const auto atHome = plan_actions::Agenda(*sleep, 3, {}, situation, NeverFirst);
	ASSERT_TRUE(atHome.has_value());
	EXPECT_EQ(atHome->front().movement.point, glm::vec2(100.0f, 200.0f));
	situation.clearArea = glm::vec2(10.0f, 20.0f);
	const auto byThing = plan_actions::Agenda(*sleep, 3, {}, situation, NeverFirst);
	ASSERT_TRUE(byThing.has_value());
	EXPECT_EQ(byThing->front().movement.point, glm::vec2(10.0f, 20.0f));
}

// Left to itself, it sleeps by a thing only on the leash or far from home, with nothing frightening near it
TEST(CreatureActionAgendas, SleepingByAThingIsPlannedOnTheLeashOrFarFromHome)
{
	const auto* sleep = plan_actions::For("SleepByObject");
	ASSERT_NE(sleep, nullptr);
	plan_actions::Situation situation {.homeDistance = 10.0f, .hasTemple = true};
	EXPECT_TRUE(plan_actions::Possible(*sleep, situation));
	EXPECT_FALSE(plan_actions::Allowed(*sleep, situation));
	// With no temple of its player, it is as far from home as can be
	situation.hasTemple = false;
	EXPECT_TRUE(plan_actions::Allowed(*sleep, situation));
	situation.hasTemple = true;
	situation.leashed = true;
	EXPECT_TRUE(plan_actions::Allowed(*sleep, situation));
	situation.nothingFrighteningNear = false;
	EXPECT_FALSE(plan_actions::Allowed(*sleep, situation));
	situation.leashed = false;
	situation.nothingFrighteningNear = true;
	situation.homeDistance = std::nullopt;
	EXPECT_TRUE(plan_actions::Allowed(*sleep, situation));
}

// A fart is done by the thing that the planner gives: the creature goes to it and turns its back to it
TEST(CreatureActionAgendas, AFartIsDoneByTheThingThatThePlannerGives)
{
	const auto* fart = plan_actions::For("Fart");
	ASSERT_NE(fart, nullptr);
	const auto byThing = plan_actions::Agenda(*fart, 9, {}, {.self = 2}, NeverFirst);
	ASSERT_TRUE(byThing.has_value());
	ASSERT_EQ(byThing->size(), 3);
	EXPECT_EQ((*byThing)[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ((*byThing)[0].movement.object, 9u);
	ASSERT_TRUE((*byThing)[0].gaze.has_value());
	EXPECT_EQ((*byThing)[1].movement.kind, Movement::Kind::TurnToFaceAwayFromObject);
	EXPECT_EQ((*byThing)[1].movement.object, 9u);
	EXPECT_EQ((*byThing)[2].effect, Effect::Fart);
	// On itself it does not look at itself
	const auto byItself = plan_actions::Agenda(*fart, 2, {}, {.self = 2}, NeverFirst);
	ASSERT_TRUE(byItself.has_value());
	EXPECT_EQ((*byItself)[0].movement.object, 2u);
	EXPECT_FALSE((*byItself)[0].gaze.has_value());
}

// With something in its hand, it examines what it holds and does not pick the thing up
TEST(CreatureActionAgendas, ExaminingByPickingUpKeepsWhatItHolds)
{
	const auto empty = ExamineByPickingUp(4, false, NeverFirst);
	ASSERT_EQ(empty.size(), 4);
	EXPECT_EQ(empty[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(empty[0].order.object, 4u);
	const auto holding = ExamineByPickingUp(4, true, NeverFirst);
	ASSERT_EQ(holding.size(), 3);
	EXPECT_EQ(holding[0].order.kind, ObjectOrder::Kind::Keep);
	EXPECT_EQ(holding[0].effect, Effect::Examined);
}

// After it releases a thing that it picked up to examine, it knows the kind of thing, as after it examines by looking
TEST(CreatureActionAgendas, ExaminingByPickingUpEndsByTakingInTheKind)
{
	const auto agenda = ExamineByPickingUp(4, false, NeverFirst);
	ASSERT_FALSE(agenda.empty());
	EXPECT_EQ(agenda.back().kind, Step::Kind::Observe);
	EXPECT_EQ(agenda.back().object, 4u);
	EXPECT_EQ(ExamineByLooking(4, 1.0f, 1.0f).back().kind, Step::Kind::Observe);
}

// Examining by looking is done to things that are not creatures, fields or fish farms; by following, to a villager
// whose work is of interest
TEST(CreatureActionAgendas, ExaminingActionsHaveTheirOwnFilters)
{
	EXPECT_EQ(plan_actions::For("ExamineByLooking")->target, plan_actions::Target::Examinable);
	EXPECT_EQ(plan_actions::For("ExamineByFollowing")->target, plan_actions::Target::InterestingVillager);
}

// "force CreatureGuardian CREATURE_EAT_FROM_MAGIC_FOOD_PILE FoodFound": it walks up to the pile, faces it, takes a
// handful of food from it and eats that, leaving the pile where it is
TEST(CreatureActionAgendas, EatingFromAPileTakesAHandfulFromIt)
{
	const auto* eat = plan_actions::For("EatFromFoodPile");
	ASSERT_NE(eat, nullptr);
	EXPECT_EQ(eat->target, plan_actions::Target::MagicFoodPile);
	const auto agenda = plan_actions::Agenda(*eat, 9, {50.0f, 60.0f}, {.radius = 6.0f, .thingRadius = 4.0f}, NeverFirst);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 4);
	EXPECT_EQ((*agenda)[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ((*agenda)[0].movement.point, glm::vec2(50.0f, 60.0f));
	EXPECT_FLOAT_EQ((*agenda)[0].movement.maxDistance, 12.0f);
	EXPECT_EQ((*agenda)[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ((*agenda)[1].movement.object, 9u);
	EXPECT_FLOAT_EQ((*agenda)[1].seconds, 0.1f);
	EXPECT_EQ((*agenda)[2].order.kind, ObjectOrder::Kind::TakeFromPile);
	EXPECT_EQ((*agenda)[2].order.object, 9u);
	EXPECT_EQ((*agenda)[2].effect, Effect::TookFromPile);
	EXPECT_EQ((*agenda)[3].order.kind, ObjectOrder::Kind::Eat);
	EXPECT_EQ((*agenda)[3].effect, Effect::Eat);
	// With something it can eat in its hand, it makes no steps and does not go to the pile
	plan_actions::Situation holding {.radius = 6.0f, .thingRadius = 4.0f};
	holding.holdingEatable = true;
	EXPECT_FALSE(plan_actions::Agenda(*eat, 9, {50.0f, 60.0f}, holding, NeverFirst).has_value());
}

// Throwing a ball at something throws a toy it can pick up; kicking one about is only done to a ball
TEST(CreatureActionAgendas, BallGamesArePlayedWithToys)
{
	const auto* throwBall = plan_actions::For("ThrowBallAtObject");
	ASSERT_NE(throwBall, nullptr);
	EXPECT_EQ(throwBall->target, plan_actions::Target::Anything);
	EXPECT_EQ(throwBall->use, plan_actions::Target::Toy);
	const auto* kick = plan_actions::For("KickBallAround");
	ASSERT_NE(kick, nullptr);
	EXPECT_EQ(kick->target, plan_actions::Target::ToyBall);
}

// A creature that copies a deed of its player first shows the player that it saw the deed
TEST(CreatureActionAgendas, NoticingAHelpfulDeedPointsWatchesTheCameraThenShowsJoy)
{
	const glm::vec3 camera {10.0f, 50.0f, 20.0f};
	const auto agenda = Notice(NoticeKind::Helpful, 7, false, true, camera, AlwaysFirst);
	ASSERT_EQ(agenda.size(), 5);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(agenda[0].order.object, 7u);
	EXPECT_FLOAT_EQ(agenda[0].order.seconds.value_or(0.0f), 2.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Amazed);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_TRUE(agenda[0].gaze->itself);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[1].movement.point, glm::vec2(10.0f, 20.0f));
	EXPECT_EQ(agenda[2].kind, Step::Kind::WatchCamera);
	EXPECT_FLOAT_EQ(agenda[2].seconds, 1.0f);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Curiosity);
	EXPECT_EQ(agenda[3].face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda[4].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[4].animation, animations::k_Happy);
	EXPECT_EQ(Notice(NoticeKind::Helpful, 7, false, true, camera, NeverFirst).back().animation, k_AhaAnimation);
}

TEST(CreatureActionAgendas, NoticingAnAggressiveDeedPointsBrieflyAndIsAngry)
{
	const auto agenda = Notice(NoticeKind::Aggressive, 7, false, true, {}, AlwaysFirst);
	ASSERT_EQ(agenda.size(), 5);
	EXPECT_FLOAT_EQ(agenda[0].order.seconds.value_or(0.0f), 0.8f);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[3].animation, k_LaughAnimation);
	EXPECT_EQ(agenda[4].animation, animations::k_Angry);
	// Two times in three it does not laugh
	const auto noLaugh = Notice(NoticeKind::Aggressive, 7, false, true, {}, NeverFirst);
	ASSERT_EQ(noLaugh.size(), 4);
	EXPECT_EQ(noLaugh[3].animation, animations::k_Angry);
}

TEST(CreatureActionAgendas, EachNoticeEndsWithItsOwnAction)
{
	EXPECT_EQ(Notice(NoticeKind::Plain, 7, false, true, {}, AlwaysFirst).back().animation, k_AhaAnimation);
	EXPECT_EQ(Notice(NoticeKind::Stealing, 7, false, true, {}, AlwaysFirst).back().animation, animations::k_Scratch);
	EXPECT_EQ(Notice(NoticeKind::Playful, 7, false, true, {}, AlwaysFirst).back().animation, animations::k_Happy);
	for (const auto kind : {NoticeKind::Plain, NoticeKind::Stealing, NoticeKind::Playful})
	{
		const auto agenda = Notice(kind, 7, false, true, {}, AlwaysFirst);
		ASSERT_EQ(agenda.size(), 5);
		EXPECT_EQ(agenda[2].face, creature_face::Cue::Curiosity);
		EXPECT_EQ(agenda[3].face, creature_face::Cue::Curiosity);
	}
}

TEST(CreatureActionAgendas, NoticingWithAFullHandOrNoPlayerLeavesStepsOut)
{
	// With something in its hand it does not point
	const auto holding = Notice(NoticeKind::Plain, 7, true, true, {}, AlwaysFirst);
	ASSERT_EQ(holding.size(), 4);
	EXPECT_EQ(holding[0].movement.kind, Movement::Kind::TurnToFace);
	// Without a player it does not turn to the camera
	const auto alone = Notice(NoticeKind::Plain, 7, false, false, {}, AlwaysFirst);
	ASSERT_EQ(alone.size(), 4);
	EXPECT_EQ(alone[1].kind, Step::Kind::WatchCamera);
}

TEST(CreatureActionAgendas, TheFiveNoticeActionsCanBeCarriedOut)
{
	for (const auto* name :
	     {"NoticeHelpfulAction", "NoticeAggressiveAction", "NoticeAction", "NoticeStealingAction", "NoticePlayfulAction"})
	{
		SCOPED_TRACE(name);
		const auto* notice = plan_actions::For(name);
		ASSERT_NE(notice, nullptr);
		EXPECT_TRUE(NoticeKindOf(name).has_value());
		// It needs a thing, and a camera when it has a player
		EXPECT_FALSE(plan_actions::Agenda(*notice, std::nullopt, {}, {}, NeverFirst).has_value());
		EXPECT_TRUE(plan_actions::Agenda(*notice, 3, {}, {}, NeverFirst).has_value());
		EXPECT_FALSE(plan_actions::Agenda(*notice, 3, {}, {.hasPlayer = true}, NeverFirst).has_value());
		EXPECT_TRUE(plan_actions::Agenda(*notice, 3, {}, {.eye = glm::vec3(0.0f), .hasPlayer = true}, NeverFirst).has_value());
	}
	EXPECT_FALSE(NoticeKindOf("PointAtObject").has_value());
}

// "ExamineByFollowing": following the villager amazed and looking at it, then scratching its head, amazed
TEST(CreatureActionAgendas, WatchingAVillagerFollowsItThenScratchesItsHead)
{
	const auto agenda = ExamineByFollowing(9);
	ASSERT_EQ(agenda.size(), 2);
	EXPECT_EQ(agenda[0].kind, Step::Kind::FollowWatching);
	EXPECT_EQ(agenda[0].object, 9u);
	EXPECT_FLOAT_EQ(agenda[0].seconds, 30.0f);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 15.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Amazed);
	ASSERT_TRUE(agenda[0].gaze.has_value());
	EXPECT_EQ(agenda[0].gaze->object, 9u);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[1].animation, animations::k_Confused);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Amazed);
}

// Far from the villager it walks up to it, stopping within 15 of it, when it stands still; near, it stays
TEST(CreatureActionAgendas, WatchingAVillagerKeepsWithinFifteenOfIt)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, ExamineByFollowing(9));
	creature_mind::Senses senses {.seconds = 0.1f};
	senses.followed = Senses::FollowedView {.interesting = true, .distance = 20.0f};
	auto commands = Think(mind, senses, AlwaysFirst);
	ASSERT_TRUE(commands.move.has_value());
	EXPECT_EQ(commands.move->kind, Movement::Kind::ToObject);
	EXPECT_EQ(commands.move->object, 9u);
	EXPECT_FLOAT_EQ(commands.move->maxDistance, 15.0f);
	senses.moving = true;
	EXPECT_FALSE(Think(mind, senses, AlwaysFirst).move.has_value());
	senses.moving = false;
	senses.followed->distance = 15.0f;
	EXPECT_FALSE(Think(mind, senses, AlwaysFirst).move.has_value());
	EXPECT_EQ(mind.step, 0u);
}

// It watches up to thirty whole seconds, then holds one to four seconds before it scratches its head
TEST(CreatureActionAgendas, WatchingAVillagerLastsThirtySecondsThenHolds)
{
	for (const auto& [draw, hold] : {std::pair {0u, 10u}, std::pair {0xfffeu, 39u}})
	{
		SCOPED_TRACE(draw);
		IdleMind mind;
		Plan(mind, Activity::Planned, ExamineByFollowing(9));
		creature_mind::Senses senses {.seconds = 0.1f};
		senses.followed = Senses::FollowedView {.interesting = true, .distance = 1.0f};
		const auto random = [draw](uint32_t range) { return range == game_random::k_FloatRandRange ? draw : 0u; };
		uint32_t turns = 0;
		while (!mind.followHolding && turns < 1000)
		{
			(void)Think(mind, senses, random);
			++turns;
		}
		// Turn 310 is the first of 31 whole seconds
		EXPECT_EQ(turns, 310u);
		EXPECT_EQ(mind.followHoldTurns, hold);
		uint32_t held = 0;
		while (mind.step == 0 && held < 1000)
		{
			(void)Think(mind, senses, random);
			++held;
		}
		EXPECT_EQ(held, hold);
	}
}

// Once the villager stops doing what it learns from, or the creature knows it, the watch ends at once
TEST(CreatureActionAgendas, WatchingAVillagerEndsWhenItIsNoLongerInteresting)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, ExamineByFollowing(9));
	creature_mind::Senses senses {.seconds = 0.1f};
	senses.followed = Senses::FollowedView {.interesting = true, .distance = 1.0f};
	(void)Think(mind, senses, AlwaysFirst);
	EXPECT_FALSE(mind.followHolding);
	senses.followed->interesting = false;
	(void)Think(mind, senses, AlwaysFirst);
	EXPECT_TRUE(mind.followHolding);
	// With the villager gone too
	IdleMind gone;
	Plan(gone, Activity::Planned, ExamineByFollowing(9));
	(void)Think(gone, {.seconds = 0.1f}, AlwaysFirst);
	EXPECT_TRUE(gone.followHolding);
}

TEST(CreatureActionAgendas, AYoungCreatureDoesNotLookOutToSea)
{
	const auto* sea = plan_actions::For("LookOutToSea");
	ASSERT_NE(sea, nullptr);
	EXPECT_FALSE(plan_actions::Possible(*sea, {.stage = 2}));
	EXPECT_TRUE(plan_actions::Possible(*sea, {.stage = 3}));
	// The other ways of looking about have no stage
	const auto* mountains = plan_actions::For("LookAtMountains");
	ASSERT_NE(mountains, nullptr);
	EXPECT_TRUE(plan_actions::Possible(*mountains, {.stage = 0}));
}

// An angry creature fights another creature only when it has enough life. The agenda asks the other creature and then
// waits for the fight to end.
TEST(CreatureActionAgendas, AFightAsksTheOtherCreatureAndWaitsUntilTheFightEnds)
{
	const auto* fight = plan_actions::For("Fight");
	ASSERT_NE(fight, nullptr);
	EXPECT_EQ(fight->target, plan_actions::Target::FightableCreature);
	EXPECT_FALSE(plan_actions::Possible(*fight, {}));
	const plan_actions::Situation healthy {.healthyEnoughToFight = true};
	EXPECT_TRUE(plan_actions::Possible(*fight, healthy));
	const auto agenda = plan_actions::Agenda(*fight, 7, {0.0f, 0.0f}, healthy, NeverFirst);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 1u);
	EXPECT_EQ(agenda->front().kind, Step::Kind::Fight);
	EXPECT_EQ(agenda->front().object, 7u);

	IdleMind mind;
	Plan(mind, Activity::Planned, *agenda);
	// It asks for the fight as the step starts, once only
	EXPECT_EQ(Think(mind, {.seconds = 0.1f}, AlwaysFirst).startFight, 7u);
	EXPECT_FALSE(Think(mind, {.seconds = 0.1f, .fighting = true}, AlwaysFirst).startFight.has_value());
	EXPECT_EQ(mind.step, 0u);
	// The fight has ended
	(void)Think(mind, {.seconds = 0.1f}, AlwaysFirst);
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(mind.gaveUp);
}

TEST(CreatureActionAgendas, StoningAndEatingThrowsTheRockAtTheRockItself)
{
	const auto agenda = StoneAndEat(4, 9, 15.0f, false);
	ASSERT_EQ(agenda.size(), 5);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[0].order.object, 4u);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::ToThrowPosition);
	EXPECT_EQ(agenda[1].movement.object, 9u);
	EXPECT_EQ(agenda[1].gaze->object, 9u);
	// The throw aims at the place of the rock in the hand, and the creature looks at the rock
	EXPECT_EQ(agenda[2].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[2].order.object, 4u);
	EXPECT_TRUE(agenda[2].order.atObject);
	EXPECT_EQ(agenda[2].gaze->object, 4u);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[3].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[3].order.object, 9u);
	EXPECT_EQ(agenda[4].order.kind, ObjectOrder::Kind::Eat);
	// With the rock in the hand already, it does not pick it up
	EXPECT_EQ(StoneAndEat(4, 9, 15.0f, true).size(), 4);
}

TEST(CreatureActionAgendas, AForcedSleepAtHomeNeedsOnlyAHome)
{
	const auto* sleep = plan_actions::For("SleepAtHome");
	ASSERT_NE(sleep, nullptr);
	// Far from home and with no temple, the planner does not choose it
	plan_actions::Situation situation;
	situation.home = glm::vec2(100.0f, 200.0f);
	situation.homeDistance = 5000.0f;
	situation.hasTemple = false;
	EXPECT_FALSE(plan_actions::Possible(*sleep, situation));
	// A script that forces it gets the agenda all the same
	EXPECT_TRUE(plan_actions::Buildable(*sleep, situation));
	const auto agenda = plan_actions::Agenda(*sleep, std::nullopt, {}, situation, AlwaysFirst);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ((*agenda)[1].movement.point, glm::vec2(100.0f, 200.0f));
	// Without a home it cannot
	EXPECT_FALSE(plan_actions::Buildable(*sleep, {}));
}

TEST(CreatureActionAgendas, DrinkingIsAlwaysPossibleButNeedsAPlaceToBeMade)
{
	const auto* drink = plan_actions::For("DrinkFromTheSea");
	ASSERT_NE(drink, nullptr);
	plan_actions::Situation situation;
	EXPECT_TRUE(plan_actions::Possible(*drink, situation));
	EXPECT_FALSE(plan_actions::Agenda(*drink, std::nullopt, {}, situation, AlwaysFirst).has_value());
	situation.drink = creature_drink::Place {.walkTo = glm::vec2(5.0f, 6.0f)};
	situation.height = 12.0f;
	const auto agenda = plan_actions::Agenda(*drink, std::nullopt, {}, situation, AlwaysFirst);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ(agenda->size(), 2);
}

TEST(CreatureActionAgendas, ItPoosOnlyByATallThingThatIsNotAliveNorAFieldOrFishFarm)
{
	// A tree or a building taller than 1.5 m
	EXPECT_TRUE(plan_actions::CanPooBy(false, false, 6.0f));
	// Exactly 1.5 m is not tall enough
	EXPECT_FALSE(plan_actions::CanPooBy(false, false, plan_actions::k_PooedOnTallerThan));
	EXPECT_TRUE(plan_actions::CanPooBy(false, false, plan_actions::k_PooedOnTallerThan + 0.01f));
	// Never by a living thing, a field or a fish farm, however tall
	EXPECT_FALSE(plan_actions::CanPooBy(true, false, 6.0f));
	EXPECT_FALSE(plan_actions::CanPooBy(false, true, 6.0f));
}

TEST(CreatureActionAgendas, EachActionNameFindsTheFirstExecutorOfThatName)
{
	const auto all = plan_actions::All();
	for (const auto& executor : all)
	{
		const auto first = std::ranges::find(all, executor.action, &plan_actions::Executor::action);
		EXPECT_EQ(plan_actions::For(executor.action), &*first) << executor.action;
	}
	EXPECT_EQ(plan_actions::For("NoSuchAction"), nullptr);
}
