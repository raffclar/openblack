/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Gestures/GestureRequests.h"
#include "Gestures/MiracleSelection.h"
#include "Magic/SpellIcons.h"

using namespace openblack;
using namespace openblack::gesture;

namespace
{
using Kind = MiracleSelection::Outcome::Kind;

/// Two miracles of the player and one of the creature, with their chains as the game's tables have them
const std::array<SelectionOffer, 3> k_Offers {
    SelectionOffer {.seed = SpellSeedType::Food, .chain = {GestureType::Spiral, GestureType::SShape, GestureType::None}},
    SelectionOffer {.seed = SpellSeedType::Water, .chain = {GestureType::Spiral, GestureType::WShape, GestureType::None}},
    SelectionOffer {.seed = SpellSeedType::CreatureSpellBig,
                    .chain = {GestureType::InverseSpiral, GestureType::ForkUp, GestureType::None}},
};

bool AllValid(SpellSeedType /*seed*/)
{
	return true;
}
} // namespace

TEST(MiracleSelection, OpensOnlyForTheMiraclesOfItsGesture)
{
	MiracleSelection selection;
	EXPECT_FALSE(selection.Open(GestureType::Circle, k_Offers));
	EXPECT_FALSE(selection.IsOpen());

	ASSERT_TRUE(selection.Open(GestureType::Spiral, k_Offers));
	EXPECT_EQ(selection.OpenedWith(), GestureType::Spiral);
	const std::vector<SelectionWait> expected {{.gesture = GestureType::SShape, .seed = SpellSeedType::Food},
	                                           {.gesture = GestureType::WShape, .seed = SpellSeedType::Water}};
	EXPECT_EQ(selection.Waiting(), expected);

	ASSERT_TRUE(selection.Open(GestureType::InverseSpiral, k_Offers));
	EXPECT_EQ(selection.Waiting(),
	          (std::vector<SelectionWait> {{.gesture = GestureType::ForkUp, .seed = SpellSeedType::CreatureSpellBig}}));
}

TEST(MiracleSelection, TheGestureOfAMiracleChoosesItAndCloses)
{
	MiracleSelection selection;
	ASSERT_TRUE(selection.Open(GestureType::Spiral, k_Offers));
	const auto outcome = selection.Recognised(GestureType::WShape, AllValid);
	EXPECT_EQ(outcome.kind, Kind::Chosen);
	EXPECT_EQ(outcome.seed, SpellSeedType::Water);
	EXPECT_FALSE(selection.IsOpen());
	EXPECT_TRUE(selection.Waiting().empty());
}

TEST(MiracleSelection, AMiracleNoLongerValidDoesNotMatchAndTheSelectionCloses)
{
	MiracleSelection selection;
	ASSERT_TRUE(selection.Open(GestureType::Spiral, k_Offers));
	const auto outcome =
	    selection.Recognised(GestureType::WShape, [](SpellSeedType seed) { return seed != SpellSeedType::Water; });
	// The other miracle does not have the gesture, so it stops too
	EXPECT_EQ(outcome.kind, Kind::Unmatched);
	EXPECT_FALSE(selection.IsOpen());
}

TEST(MiracleSelection, ALongerChainWaitsForItsNextGesture)
{
	const std::array<SelectionOffer, 2> offers {
	    SelectionOffer {.seed = SpellSeedType::Food, .chain = {GestureType::Spiral, GestureType::SShape, GestureType::Heart}},
	    SelectionOffer {.seed = SpellSeedType::Heal, .chain = {GestureType::Spiral, GestureType::SShape, GestureType::Star}},
	};
	MiracleSelection selection;
	ASSERT_TRUE(selection.Open(GestureType::Spiral, offers));
	EXPECT_EQ(selection.Waiting(),
	          (std::vector<SelectionWait> {{.gesture = GestureType::SShape, .seed = SpellSeedType::Heal}}));
	EXPECT_EQ(selection.Recognised(GestureType::SShape, AllValid).kind, Kind::Advanced);
	ASSERT_TRUE(selection.IsOpen());
	EXPECT_EQ(selection.Waiting(), (std::vector<SelectionWait> {{.gesture = GestureType::Star, .seed = SpellSeedType::Heal},
	                                                            {.gesture = GestureType::Heart, .seed = SpellSeedType::Food}}));
	const auto outcome = selection.Recognised(GestureType::Heart, AllValid);
	EXPECT_EQ(outcome.kind, Kind::Chosen);
	EXPECT_EQ(outcome.seed, SpellSeedType::Food);
}

TEST(MiracleSelection, ClosesByItselfAfterTheTimeLimitAndAMatchStartsItAgain)
{
	const std::array<SelectionOffer, 1> offers {
	    SelectionOffer {.seed = SpellSeedType::Food, .chain = {GestureType::Spiral, GestureType::SShape, GestureType::Heart}}};
	MiracleSelection selection;
	ASSERT_TRUE(selection.Open(GestureType::Spiral, offers));
	selection.Update(20.0f, 30.0f);
	EXPECT_TRUE(selection.IsOpen());
	ASSERT_EQ(selection.Recognised(GestureType::SShape, AllValid).kind, Kind::Advanced);
	selection.Update(20.0f, 30.0f);
	EXPECT_TRUE(selection.IsOpen());
	selection.Update(10.0f, 30.0f);
	EXPECT_TRUE(selection.IsOpen());
	selection.Update(0.5f, 30.0f);
	EXPECT_FALSE(selection.IsOpen());
}

TEST(SelectionRequests, TheSpiralsAndTheROnlyWithAReadyHand)
{
	HandContext context;
	context.ownSelectionOffered = true;
	context.creatureSelectionOffered = true;
	context.repeatOffered = true;
	context.lastSeed = SpellSeedType::Heal;
	EXPECT_TRUE(Requests(context).empty());

	context.handReady = true;
	const std::vector<Request> expected {
	    {.gesture = GestureType::Spiral, .purpose = Purpose::OpenSelection},
	    {.gesture = GestureType::InverseSpiral, .purpose = Purpose::OpenSelection, .forCreature = true},
	    {.gesture = GestureType::RShape, .purpose = Purpose::RepeatMiracle, .seed = SpellSeedType::Heal},
	};
	EXPECT_EQ(Requests(context), expected);

	// A fighting creature stops all three
	context.creature = HandContext::Creature {.fighting = true};
	EXPECT_TRUE(Requests(context).empty());
}

TEST(SelectionRequests, TheOpenSelectionWaitsForItsScribbleAndGesturesFirst)
{
	HandContext context;
	context.handReady = true;
	context.ownSelectionOffered = true;
	context.creatureSelectionOffered = true;
	context.repeatOffered = true;
	context.creature = HandContext::Creature {.knows = {false, true, false}};
	context.selection = HandContext::Selection {
	    .openedWith = GestureType::Spiral,
	    .waiting = {{.gesture = GestureType::SShape, .seed = SpellSeedType::Food}},
	};
	const std::vector<Request> expected {
	    {.gesture = GestureType::Scribble, .purpose = Purpose::CloseSelection},
	    {.gesture = GestureType::SShape, .purpose = Purpose::SelectMiracle, .seed = SpellSeedType::Food},
	    // The other spiral opens the selection again for its own miracles; the R and the leash wait
	    {.gesture = GestureType::InverseSpiral, .purpose = Purpose::OpenSelection, .forCreature = true},
	};
	EXPECT_EQ(Requests(context), expected);

	// In the frame that its time runs out, the selection still waits for its gestures, but the rest act as if it closed
	context.selection->open = false;
	const auto requests = Requests(context);
	ASSERT_EQ(requests.size(), 6u);
	EXPECT_EQ(requests[2], (Request {.gesture = GestureType::Spiral, .purpose = Purpose::OpenSelection}));
	EXPECT_EQ(requests[4].purpose, Purpose::RepeatMiracle);
	EXPECT_EQ(requests[5].purpose, Purpose::LeashGesture);
}

TEST(SelectionRequests, OnlyTheScribbleIsNotShown)
{
	EXPECT_TRUE(ShowsRecognition(Purpose::OpenSelection));
	EXPECT_TRUE(ShowsRecognition(Purpose::SelectMiracle));
	EXPECT_TRUE(ShowsRecognition(Purpose::RepeatMiracle));
	EXPECT_FALSE(ShowsRecognition(Purpose::CloseSelection));
	EXPECT_TRUE(RestsAfter(Purpose::CloseSelection));
}

TEST(SpellIconRequest, ARequestPutsAChargedMiracleInTheHandOrStartsACharge)
{
	namespace spell_icons = openblack::magic::spell_icons;
	using spell_icons::IconCharge;
	using spell_icons::TapResult;
	const IconCharge empty {.required = 100.0f};
	// The site has prayer power: the icon starts to charge
	EXPECT_EQ(spell_icons::Request(empty, true, true, 10.0f), TapResult::StartedCharge);
	// The site has none and the icon stored nothing: nothing happens, which a tap would still start
	EXPECT_EQ(spell_icons::Request(empty, true, true, 0.0f), TapResult::Nothing);
	EXPECT_EQ(spell_icons::Tap(empty, true, true, true), TapResult::StartedCharge);
	// Something stored lets it start
	EXPECT_EQ(spell_icons::Request({.store = 5.0f, .required = 100.0f}, true, true, 0.0f), TapResult::StartedCharge);
	// Charged full: into the hand, if the hand is ready
	const IconCharge full {.store = 100.0f, .required = 100.0f};
	EXPECT_EQ(spell_icons::Request(full, true, true, 0.0f), TapResult::PutInHand);
	// A request never calls off the charge of its own hand
	EXPECT_EQ(spell_icons::Request({.charging = true, .chargedByThisHand = true, .required = 100.0f}, true, true, 10.0f),
	          TapResult::Nothing);
	// A miracle not enabled cannot start
	EXPECT_EQ(spell_icons::Request(empty, true, false, 10.0f), TapResult::Nothing);
}

TEST(SpellIconRequest, OnlyAWorkingIconIsValid)
{
	using namespace openblack::magic::spell_icons;
	const IconCharge empty {.required = 100.0f};
	EXPECT_TRUE(ValidForRequest(empty, true, true, true, 10.0f));
	EXPECT_FALSE(ValidForRequest(empty, false, true, true, 10.0f));
	EXPECT_FALSE(ValidForRequest(empty, true, true, true, std::nullopt));
	EXPECT_TRUE(ValidForRequest({.store = 100.0f, .required = 100.0f}, true, true, false, std::nullopt));
}
