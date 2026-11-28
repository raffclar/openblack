/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include "HandGrabSystemFixture.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace hand_grab_test;

TEST_F(HandGrabSystemWithWorld, ARockIsTakenOnceTheHandHasFadedIntoItsPull)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	EXPECT_TRUE(Press());
	EXPECT_TRUE(_system->IsBusy());
	// The pull waits for the 0.13 s fade, then anything but a tree comes free at once and is taken the frame after
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(*_system->GetHeld(), rock);
	EXPECT_TRUE(_world->registry.AllOf<InHand>(rock));
	EXPECT_EQ(_world->leftWorld, std::vector<entt::entity> {rock});
	// It sounds as it is picked up, and the people about the hand see it
	ASSERT_FALSE(_world->samples.empty());
	EXPECT_EQ(_world->samples.front(), 10u);
	ASSERT_FALSE(_world->reactions.empty());
	EXPECT_EQ(_world->reactions.front().type, Reaction::ReactToHandPickUp);
	EXPECT_EQ(_world->reactions.front().initiator, _world->hand);
	// Its time starts at once, so it ends also when nobody takes it up
	EXPECT_TRUE(_world->reactions.front().timedFromStart);
}

// The pick-up reaction is at the point under the cursor that the last report of the player gave, also when the hand is far
// from that point
TEST_F(HandGrabSystemWithWorld, ThePickUpReactionIsAtThePointUnderTheCursor)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	_world->reportedCollide = glm::vec3(30.0f, 2.0f, 40.0f);
	_world->reportedPlaces[_world->hand] = glm::vec3(-300.0f, 20.0f, -400.0f);
	EXPECT_TRUE(Press());
	for (int i = 0; i < 20 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	const auto pickUp =
	    std::ranges::find(_world->reactions, Reaction::ReactToHandPickUp, &decltype(_world->reactions)::value_type::type);
	ASSERT_NE(pickUp, _world->reactions.end());
	EXPECT_EQ(pickUp->position, glm::vec3(30.0f, 2.0f, 40.0f));
}

// The debug tools let go at once with no command. In a network game, each computer does this let go in its own turn, so
// the start must not come from the place that one computer drew.
TEST_F(HandGrabSystemWithWorld, InANetworkGameTheLetGoOfTheToolsDoesNotStartFromTheDrawnPlace)
{
	_world->networkGame = true;
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	ASSERT_TRUE(_system->TakeIntoHand(rock));
	_world->registry.Get<InHand>(rock).drawn = InHand::Drawn {.position = {1.0f, 2.0f, 3.0f}, .rotation = glm::mat3(1.0f)};
	ASSERT_TRUE(_system->LetGoAt(glm::vec3(0.0f)));
	ASSERT_EQ(_world->drawnAtStart.size(), 1u);
	EXPECT_FALSE(_world->drawnAtStart.front());
}

TEST_F(HandGrabSystemWithWorld, InAGameAloneTheLetGoOfTheToolsStartsFromTheDrawnPlace)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	ASSERT_TRUE(_system->TakeIntoHand(rock));
	_world->registry.Get<InHand>(rock).drawn = InHand::Drawn {.position = {1.0f, 2.0f, 3.0f}, .rotation = glm::mat3(1.0f)};
	ASSERT_TRUE(_system->LetGoAt(glm::vec3(0.0f)));
	ASSERT_EQ(_world->drawnAtStart.size(), 1u);
	EXPECT_TRUE(_world->drawnAtStart.front());
}

// The hand does not hold a creature as a thing. Because of this, the hand never gives a creature to a creature.
TEST_F(HandGrabSystemWithWorld, TheHandNeverTakesACreature)
{
	const auto creature = _world->registry.Create();
	_world->registry.Assign<Transform>(creature, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_world->registry.Assign<Creature>(creature);
	_world->underCursor = creature;
	Press();
	for (int i = 0; i < 20; ++i)
	{
		Frame(10);
	}
	Release();
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_FALSE(_world->registry.AllOf<InHand>(creature));
}

// Pressed with nothing under the cursor, the hand reaches for what stands nearest the land behind it; something under
// the cursor goes first
TEST_F(HandGrabSystemWithWorld, WithNothingUnderTheCursorThePressReachesForWhatIsNearTheHand)
{
	const auto near = _world->AddRock({1.0f, 0.0f, 0.0f});
	_world->nearHand = near;
	EXPECT_TRUE(Press());
	ASSERT_TRUE(_world->registry.TryGet<HandGrab>(_world->hand) != nullptr);
	EXPECT_EQ(_world->registry.Get<HandGrab>(_world->hand).object, near);
	Release();

	const auto under = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = under;
	_now += 1000;
	EXPECT_TRUE(Press());
	EXPECT_EQ(_world->registry.Get<HandGrab>(_world->hand).object, under);
}

TEST_F(HandGrabSystemWithWorld, TheHandKeepsToItsPullUntilItHoldsAndRisesTheFrameItTakes)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	EXPECT_TRUE(Press());
	const glm::vec3 cursor {0.0f, 5.0f, 0.0f};
	std::optional<glm::vec3> taken;
	for (int i = 0; i < 40 && !taken.has_value(); ++i)
	{
		const auto hand = Frame(10, cursor);
		if (_system->GetHeld().has_value())
		{
			taken = hand;
		}
		else
		{
			// Every frame before it is held, the frame it comes free too, the hand keeps the pose of its pull
			EXPECT_TRUE(_system->GetPullPose().has_value()) << "frame " << i;
		}
	}
	ASSERT_TRUE(taken.has_value());
	EXPECT_FALSE(_system->GetPullPose().has_value());
	// The frame it takes the thing the hand is already where it holds it, risen above the cursor as on the frames after
	const auto next = Frame(10, cursor);
	EXPECT_GT(next.y, cursor.y);
	EXPECT_FLOAT_EQ(taken->y, next.y);
}

TEST_F(HandGrabSystemWithWorld, AShortPressIsATap)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	EXPECT_TRUE(Press());
	Frame(50);
	const auto tapped = Release();
	ASSERT_TRUE(tapped.has_value());
	EXPECT_EQ(*tapped, rock);
	EXPECT_FALSE(_system->IsBusy());
	// The tap reaches the thing
	ASSERT_EQ(_world->tapped.size(), 1u);
	EXPECT_EQ(_world->tapped.front(), rock);
}

// A one-shot miracle's globe is a loose thing: a quick press taps it (for its miracle), a held one carries it from the
// side, hung seven tenths of its height below the hand
TEST_F(HandGrabSystemWithWorld, AShortPressTapsAGlobe)
{
	const auto globe = _world->AddGlobe({0.0f, 0.0f, 0.0f});
	_world->underCursor = globe;
	EXPECT_TRUE(Press());
	Frame(50);
	const auto tapped = Release();
	ASSERT_TRUE(tapped.has_value());
	EXPECT_EQ(_world->tapped, std::vector<entt::entity> {globe});
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_TRUE(_world->globesTaken.empty());
}

TEST_F(HandGrabSystemWithWorld, AHeldPressCarriesAGlobeFromTheSide)
{
	const auto globe = _world->AddGlobe({0.0f, 0.0f, 0.0f});
	_world->underCursor = globe;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(*_system->GetHeld(), globe);
	EXPECT_TRUE(_world->tapped.empty());
	// The player has had its miracle within reach
	ASSERT_EQ(_world->globesTaken.size(), 1u);
	EXPECT_EQ(_world->globesTaken.front().first, globe);
	const auto pose = _system->GetHeldPose();
	ASSERT_TRUE(pose.has_value());
	EXPECT_EQ(pose->hold, HoldType::Side);
	EXPECT_FLOAT_EQ(_world->registry.Get<HandGrab>(_world->hand).lowering, 0.7f * 2.0f);
}

// A held press on the player's own seed that shows over its miracle pulls it, then gives it back to the miracles. The
// grab of the hand ends: the hand holds the seed as its miracle, not as a thing.
TEST_F(HandGrabSystemWithWorld, AHeldPressTakesTheOwnShownSeedBackToTheMiracles)
{
	const auto seed = _world->AddShownSeed({0.0f, 0.0f, 0.0f}, PlayerNames::PLAYER_ONE);
	_world->underCursor = seed;
	EXPECT_EQ(_system->WhyNotTake(seed), "");
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && _world->seedsTakenBack.empty(); ++i)
	{
		Frame(10);
	}
	ASSERT_EQ(_world->seedsTakenBack.size(), 1u);
	EXPECT_EQ(_world->seedsTakenBack.front(), std::pair(seed, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_FALSE(_system->IsBusy());
	// The pick-up sound plays as the hand sends the take
	EXPECT_EQ(_world->samples.size(), 1u);
	// The help counts the take, and those about the hand see it pick something up
	EXPECT_EQ(_world->helpEvents, std::vector<uint32_t> {help::profile::k_PickUp});
	ASSERT_EQ(_world->reactions.size(), 1u);
	EXPECT_EQ(_world->reactions.front().type, Reaction::ReactToHandPickUp);
	EXPECT_TRUE(_world->reactions.front().timedFromStart);
	EXPECT_EQ(_world->spreadStopped, std::vector<entt::entity> {seed});
	EXPECT_EQ(_world->pathsForgotten, 1);
	// It never goes into the hand as a thing
	EXPECT_TRUE(_world->leftWorld.empty());
}

// A seed whose miracle has too little left goes at the take: the hand counts no take and nobody sees one
TEST_F(HandGrabSystemWithWorld, ASeedThatGoesAtTheTakeCountsNoTake)
{
	_world->seedKept = false;
	const auto seed = _world->AddShownSeed({0.0f, 0.0f, 0.0f}, PlayerNames::PLAYER_ONE);
	_world->underCursor = seed;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && _world->seedsTakenBack.empty(); ++i)
	{
		Frame(10);
	}
	ASSERT_EQ(_world->seedsTakenBack.size(), 1u);
	EXPECT_FALSE(_system->IsBusy());
	EXPECT_TRUE(_world->helpEvents.empty());
	EXPECT_TRUE(_world->reactions.empty());
	EXPECT_EQ(_world->pathsForgotten, 0);
}

// The hand takes back only a seed of its own player
TEST_F(HandGrabSystemWithWorld, TheHandLeavesTheShownSeedOfAnotherPlayer)
{
	const auto seed = _world->AddShownSeed({0.0f, 0.0f, 0.0f}, PlayerNames::PLAYER_TWO);
	_world->underCursor = seed;
	EXPECT_NE(_system->WhyNotTake(seed), "");
	EXPECT_FALSE(Press());
	for (int i = 0; i < 40; ++i)
	{
		Frame(10);
	}
	EXPECT_TRUE(_world->seedsTakenBack.empty());
}

// A seed that a hand holds is no thing for the hand to take
TEST_F(HandGrabSystemWithWorld, AHeldSeedIsNoThingToTake)
{
	const auto seed = _world->AddShownSeed({0.0f, 0.0f, 0.0f}, PlayerNames::PLAYER_ONE);
	_world->registry.Assign<HandHeldSeed>(_world->secondHand, seed);
	EXPECT_NE(_system->WhyNotTake(seed), "");
}

TEST_F(HandGrabSystemWithWorld, AHeldPressCarriesAScaffoldFromTheSideAndItsWorkshopIsTold)
{
	const auto scaffold = _world->AddScaffold({0.0f, 0.0f, 0.0f});
	_world->underCursor = scaffold;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_TRUE(_system->GetHeld().has_value());
	EXPECT_EQ(*_system->GetHeld(), scaffold);
	EXPECT_TRUE(_world->tapped.empty());
	// The hand tells the workshop one time, so that the workshop keeps the scaffold's place in its yard
	ASSERT_EQ(_world->scaffoldsTaken.size(), 1u);
	EXPECT_EQ(_world->scaffoldsTaken.front().first, scaffold);
	const auto pose = _system->GetHeldPose();
	ASSERT_TRUE(pose.has_value());
	EXPECT_EQ(pose->hold, HoldType::Side);
}

// A scaffold put down gently keeps what its building needs: the rest is a new scaffold, and the hand takes it
TEST_F(HandGrabSystemWithWorld, TheRestOfAScaffoldPutDownStaysInTheHand)
{
	const auto scaffold = _world->AddScaffold({0.0f, 0.0f, 0.0f});
	_world->underCursor = scaffold;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_EQ(_system->GetHeld(), std::optional(scaffold));
	_world->restOfScaffold = _world->AddScaffold({0.0f, 0.0f, 0.0f});
	_system->ForceDrop();
	ASSERT_EQ(_world->restsAsked, std::vector<entt::entity> {scaffold});
	EXPECT_EQ(_system->GetHeld(), std::optional(_world->restOfScaffold));
	EXPECT_TRUE(_world->registry.AllOf<InHand>(_world->restOfScaffold));
	EXPECT_FALSE(_world->registry.AllOf<InHand>(scaffold));
	// The workshop system hears of the new scaffold in the hand
	ASSERT_EQ(_world->scaffoldsTaken.size(), 2u);
	EXPECT_EQ(_world->scaffoldsTaken.back().first, _world->restOfScaffold);
}

// With no rest, the hand is empty after it lets go of a scaffold
TEST_F(HandGrabSystemWithWorld, AScaffoldWithNoRestLeavesTheHandEmpty)
{
	const auto scaffold = _world->AddScaffold({0.0f, 0.0f, 0.0f});
	_world->underCursor = scaffold;
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	ASSERT_EQ(_system->GetHeld(), std::optional(scaffold));
	_system->ForceDrop();
	ASSERT_EQ(_world->restsAsked, std::vector<entt::entity> {scaffold});
	EXPECT_FALSE(_system->GetHeld().has_value());
}

TEST_F(HandGrabSystemWithWorld, AShortPressOnAScaffoldTakesNothing)
{
	const auto scaffold = _world->AddScaffold({0.0f, 0.0f, 0.0f});
	_world->underCursor = scaffold;
	EXPECT_TRUE(Press());
	Frame(50);
	Release();
	EXPECT_FALSE(_system->GetHeld().has_value());
	EXPECT_TRUE(_world->scaffoldsTaken.empty());
}

// The hand takes a scaffold of the town of another god as any other scaffold, but only in its influence
TEST_F(HandGrabSystemWithWorld, AScaffoldOfAnotherGodsTownIsTakenOnlyInTheInfluence)
{
	const auto scaffold = _world->AddScaffold({0.0f, 0.0f, 0.0f});
	_world->registry.Get<Scaffold>(scaffold).town = _world->registry.Create();
	_world->underCursor = scaffold;
	_world->influenceOnlyOf = PlayerNames::PLAYER_TWO;
	EXPECT_FALSE(Press());
	EXPECT_TRUE(_world->scaffoldsTaken.empty());
	_world->influenceOnlyOf.reset();
	EXPECT_TRUE(Press());
	for (int i = 0; i < 40 && !_system->GetHeld().has_value(); ++i)
	{
		Frame(10);
	}
	EXPECT_EQ(_system->GetHeld(), std::optional(scaffold));
}

TEST_F(HandGrabSystemWithWorld, WithTheLeashHeldAShortPressIsAnOrderInsteadOfATap)
{
	const auto rock = _world->AddRock({0.0f, 0.0f, 0.0f});
	_world->underCursor = rock;
	_world->leashTakesTaps = true;
	EXPECT_TRUE(Press());
	Frame(50);
	const auto tapped = Release();
	ASSERT_TRUE(tapped.has_value());
	ASSERT_EQ(_world->leashTapped.size(), 1u);
	EXPECT_EQ(_world->leashTapped.front(), rock);
	EXPECT_TRUE(_world->tapped.empty());
}
