/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>

#include <SDL_events.h>
#include <gtest/gtest.h>

#include "Creature/CreatureHandRules.h"
#include "Creature/LeashKeys.h"
#include "Creature/LeashOwnership.h"
#include "Creature/LeashTakeOff.h"
#include "Input/GameActionMap.h"

using namespace openblack;
using namespace openblack::creature_leash;
using input::BindableActionMap;
using Kind = KeyCommand::Kind;

namespace
{
/// The player's own creature, leashable and knowing every leash
Candidate Own()
{
	return {.owner = PlayerNames::PLAYER_ONE, .leashable = true, .knowsLearningLeash = true, .knowsType = true};
}

std::bitset<k_Types.size()> Knowing(std::initializer_list<LeashType> types)
{
	std::bitset<k_Types.size()> known;
	for (const auto type : types)
	{
		known.set(*IndexOf(type));
	}
	return known;
}
} // namespace

TEST(LeashOwnership, ThePlayerLeadsTheirLeashableCreature)
{
	EXPECT_EQ(WhyNot(PlayerNames::PLAYER_ONE, Own()), Refusal::None);
	auto held = Own();
	held.heldBy = PlayerNames::PLAYER_ONE;
	EXPECT_EQ(WhyNot(PlayerNames::PLAYER_ONE, held), Refusal::None);
}

TEST(LeashOwnership, AnotherPlayersCreatureIsRefused)
{
	auto theirs = Own();
	theirs.owner = PlayerNames::PLAYER_TWO;
	EXPECT_EQ(WhyNot(PlayerNames::PLAYER_ONE, theirs), Refusal::OwnedByAnother);
	EXPECT_EQ(WhyNot(PlayerNames::PLAYER_TWO, theirs), Refusal::None);
	auto nobodys = Own();
	nobodys.owner = PlayerNames::NEUTRAL;
	EXPECT_EQ(WhyNot(PlayerNames::PLAYER_ONE, nobodys), Refusal::OwnedByAnother);
	EXPECT_EQ(WhyNot(PlayerNames::NEUTRAL, nobodys), Refusal::NoPlayer);
}

TEST(LeashOwnership, OnlyTheLeashableOneOfThePlayers)
{
	auto extra = Own();
	extra.leashable = false;
	EXPECT_EQ(WhyNot(PlayerNames::PLAYER_ONE, extra), Refusal::NotLeashable);
}

TEST(LeashOwnership, ItMustKnowTheLeashes)
{
	auto untaught = Own();
	untaught.knowsLearningLeash = false;
	EXPECT_EQ(WhyNot(PlayerNames::PLAYER_ONE, untaught), Refusal::DoesNotKnowLearningLeash);
	auto other = Own();
	other.knowsType = false;
	EXPECT_EQ(WhyNot(PlayerNames::PLAYER_ONE, other), Refusal::DoesNotKnowThatLeash);
}

TEST(LeashOwnership, AnotherHolderIsRefused)
{
	auto held = Own();
	held.heldBy = PlayerNames::PLAYER_TWO;
	EXPECT_EQ(WhyNot(PlayerNames::PLAYER_ONE, held), Refusal::HeldByAnother);
}

TEST(LeashOwnership, EveryRefusalIsDescribed)
{
	for (const auto refusal :
	     {Refusal::None, Refusal::NotACreature, Refusal::NoPlayer, Refusal::OwnedByAnother, Refusal::NotLeashable,
	      Refusal::DoesNotKnowLearningLeash, Refusal::DoesNotKnowThatLeash, Refusal::HeldByAnother})
	{
		EXPECT_STRNE(Describe(refusal), "unknown");
	}
}

TEST(LeashOwnership, MakingOneLeashableDisplacesTheOwnersOther)
{
	const std::array<Claim, 4> creatures {{
	    {.creature = 1, .owner = PlayerNames::PLAYER_ONE, .leashable = true},
	    {.creature = 2, .owner = PlayerNames::PLAYER_ONE, .leashable = false},
	    {.creature = 3, .owner = PlayerNames::PLAYER_TWO, .leashable = true},
	    {.creature = 4, .owner = PlayerNames::PLAYER_ONE, .leashable = false},
	}};
	EXPECT_EQ(Displaced(creatures, 2, PlayerNames::PLAYER_ONE), std::vector<uint32_t> {1});
	// Choosing the one already leashable displaces nobody, and other players' creatures are left alone
	EXPECT_TRUE(Displaced(creatures, 1, PlayerNames::PLAYER_ONE).empty());
	EXPECT_TRUE(Displaced(creatures, 4, PlayerNames::PLAYER_THREE).empty());
}

TEST(LeashOwnership, EachPlayerHasOneLeashableCreature)
{
	const std::array<Claim, 3> creatures {{
	    {.creature = 7, .owner = PlayerNames::PLAYER_ONE, .leashable = false},
	    {.creature = 8, .owner = PlayerNames::PLAYER_ONE, .leashable = true},
	    {.creature = 9, .owner = PlayerNames::PLAYER_TWO, .leashable = true},
	}};
	EXPECT_EQ(LeashableOf(creatures, PlayerNames::PLAYER_ONE), 8u);
	EXPECT_EQ(LeashableOf(creatures, PlayerNames::PLAYER_TWO), 9u);
	EXPECT_FALSE(LeashableOf(creatures, PlayerNames::PLAYER_THREE).has_value());
}

TEST(LeashOwnership, APlayersFirstCreatureClaimsTheLeash)
{
	EXPECT_TRUE(ClaimsOnArrival({}, PlayerNames::PLAYER_ONE));
	const std::array<Claim, 1> one {{{.creature = 1, .owner = PlayerNames::PLAYER_ONE, .leashable = true}}};
	EXPECT_FALSE(ClaimsOnArrival(one, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(ClaimsOnArrival(one, PlayerNames::PLAYER_TWO));
	// A creature that belongs to nobody is never leashable
	EXPECT_FALSE(ClaimsOnArrival({}, PlayerNames::NEUTRAL));
	// Extra creatures that aren't leashable don't count
	const std::array<Claim, 1> extra {{{.creature = 1, .owner = PlayerNames::PLAYER_ONE, .leashable = false}}};
	EXPECT_TRUE(ClaimsOnArrival(extra, PlayerNames::PLAYER_ONE));
}

TEST(LeashKeys, ControlsMapToTheShortcuts)
{
	EXPECT_EQ(KeyFor(BindableActionMap::LEASH_UNLEASH_CREATURE), LeashKey::Leash);
	EXPECT_EQ(KeyFor(BindableActionMap::PREVIOUS_LEASH), LeashKey::PreviousLeash);
	EXPECT_EQ(KeyFor(BindableActionMap::NEXT_LEASH), LeashKey::NextLeash);
	EXPECT_FALSE(KeyFor(BindableActionMap::ACTION).has_value());
	EXPECT_FALSE(KeyFor(BindableActionMap::QUICK_LOAD).has_value());
}

TEST(LeashKeys, TheGameBindsLVAndB)
{
	// Whether pressing the key, with the modifier held, sets off the action
	const auto sets = [](SDL_Scancode key, uint16_t mod, BindableActionMap action) {
		input::GameActionMap actions;
		SDL_Event event {};
		event.type = SDL_KEYDOWN;
		event.key.keysym.scancode = key;
		event.key.keysym.mod = mod;
		actions.ProcessEvent(event);
		return actions.Get(action);
	};
	EXPECT_TRUE(sets(SDL_SCANCODE_L, KMOD_NONE, BindableActionMap::LEASH_UNLEASH_CREATURE));
	EXPECT_TRUE(sets(SDL_SCANCODE_V, KMOD_NONE, BindableActionMap::PREVIOUS_LEASH));
	EXPECT_TRUE(sets(SDL_SCANCODE_B, KMOD_NONE, BindableActionMap::NEXT_LEASH));
	// Ctrl+L is the quick load, not the leash
	EXPECT_FALSE(sets(SDL_SCANCODE_L, KMOD_LCTRL, BindableActionMap::LEASH_UNLEASH_CREATURE));
	EXPECT_TRUE(sets(SDL_SCANCODE_L, KMOD_LCTRL, BindableActionMap::QUICK_LOAD));
}

TEST(LeashKeys, TheLeashKeyPutsOnUntiesAndTakesOff)
{
	const auto all = Knowing({LeashType::Evil, LeashType::Rope, LeashType::Good});
	EXPECT_EQ(CommandFor(LeashKey::Leash, {.known = all, .selected = LeashType::Good}),
	          (KeyCommand {.kind = Kind::PutOn, .type = LeashType::Good}));
	EXPECT_EQ(CommandFor(LeashKey::Leash, {.worn = true, .tied = true, .known = all}),
	          (KeyCommand {.kind = Kind::UntieToHand}));
	EXPECT_EQ(CommandFor(LeashKey::Leash, {.worn = true, .known = all}), (KeyCommand {.kind = Kind::TakeOff}));
}

TEST(LeashKeys, TheLeashKeyNeedsTheLearningLeash)
{
	EXPECT_EQ(CommandFor(LeashKey::Leash, {.known = Knowing({LeashType::Good})}), KeyCommand {});
	// A picked leash it doesn't know falls back to the learning leash
	EXPECT_EQ(CommandFor(LeashKey::Leash, {.known = Knowing({LeashType::Rope}), .selected = LeashType::Evil}),
	          (KeyCommand {.kind = Kind::PutOn, .type = LeashType::Rope}));
}

TEST(LeashKeys, VStepsUpAndBDownThroughTheLeashNumbers)
{
	const auto all = Knowing({LeashType::Evil, LeashType::Rope, LeashType::Good});
	const auto change = [](LeashType type) { return KeyCommand {.kind = Kind::ChangeType, .type = type}; };
	// V: aggression 1, learning 2, compassion 3, and round again
	EXPECT_EQ(CommandFor(LeashKey::PreviousLeash, {.known = all, .selected = LeashType::Evil}), change(LeashType::Rope));
	EXPECT_EQ(CommandFor(LeashKey::PreviousLeash, {.known = all, .selected = LeashType::Rope}), change(LeashType::Good));
	EXPECT_EQ(CommandFor(LeashKey::PreviousLeash, {.known = all, .selected = LeashType::Good}), change(LeashType::Evil));
	// B: 1, 3, 2, and round again
	EXPECT_EQ(CommandFor(LeashKey::NextLeash, {.known = all, .selected = LeashType::Evil}), change(LeashType::Good));
	EXPECT_EQ(CommandFor(LeashKey::NextLeash, {.known = all, .selected = LeashType::Good}), change(LeashType::Rope));
	EXPECT_EQ(CommandFor(LeashKey::NextLeash, {.known = all, .selected = LeashType::Rope}), change(LeashType::Evil));
}

TEST(LeashKeys, AStepOntoAnUnknownLeashDoesNothing)
{
	// Knowing aggression and learning, V from learning would reach compassion, which it doesn't know, so it stays
	const auto two = Knowing({LeashType::Evil, LeashType::Rope});
	EXPECT_FALSE(StepType(LeashType::Rope, two, true).has_value());
	EXPECT_EQ(StepType(LeashType::Rope, two, false), LeashType::Evil);
	EXPECT_EQ(CommandFor(LeashKey::PreviousLeash, {.known = two, .selected = LeashType::Rope}), KeyCommand {});
	EXPECT_EQ(CommandFor(LeashKey::NextLeash, {.known = Knowing({LeashType::Rope}), .selected = LeashType::Rope}),
	          KeyCommand {});
}

TEST(LeashKeys, TheLeashKeyTakesTheLeashOffHoweverManyItKnows)
{
	const auto all = Knowing({LeashType::Evil, LeashType::Rope, LeashType::Good});
	EXPECT_EQ(CommandFor(LeashKey::Leash, {.worn = true, .known = all, .selected = LeashType::Good}),
	          (KeyCommand {.kind = Kind::TakeOff}));
}

TEST(LeashKeys, AClickOnTheCreatureIsTheLeashKey)
{
	const auto all = Knowing({LeashType::Evil, LeashType::Rope, LeashType::Good});
	// Off: the picked leash goes on, or the learning leash when it does not know the picked one
	EXPECT_EQ(CommandForClick({.known = all, .selected = LeashType::Evil}),
	          (KeyCommand {.kind = Kind::PutOn, .type = LeashType::Evil}));
	EXPECT_EQ(CommandForClick({.known = Knowing({LeashType::Rope}), .selected = LeashType::Good}),
	          (KeyCommand {.kind = Kind::PutOn, .type = LeashType::Rope}));
	// Worn: the leash comes off, whichever leash it is
	for (const auto type : {LeashType::Evil, LeashType::Rope, LeashType::Good})
	{
		EXPECT_EQ(CommandForClick({.worn = true, .known = all, .selected = type}), (KeyCommand {.kind = Kind::TakeOff}));
	}
	// Tied: the leash comes back to the hand
	EXPECT_EQ(CommandForClick({.worn = true, .tied = true, .known = all}), (KeyCommand {.kind = Kind::UntieToHand}));
	// With no learning leash known, the click does nothing
	EXPECT_EQ(CommandForClick({.known = Knowing({LeashType::Good})}), KeyCommand {});
	// Every state gives the command of the leash key
	for (const bool worn : {false, true})
	{
		for (const bool tied : {false, true})
		{
			const KeyState state {.worn = worn, .tied = worn && tied, .known = all, .selected = LeashType::Good};
			EXPECT_EQ(CommandForClick(state), CommandFor(LeashKey::Leash, state));
		}
	}
}

TEST(LeashTakeOff, TheMindForgetsTheLeash)
{
	ecs::components::CreatureMindState mind;
	mind.leash.learningInHand = true;
	mind.leash.miracleSightingWeight = MiracleSightingWeight(true);
	mind.ledByLeash = true;
	mind.leftAreaActions = {false, true, true};
	ForgetLeash(mind);
	EXPECT_FALSE(mind.leash.learningInHand);
	EXPECT_EQ(mind.leash.miracleSightingWeight, MiracleSightingWeight(false));
	// The leash no longer leads the plan
	EXPECT_FALSE(mind.ledByLeash);
	// The actions that the area refused are possible again
	EXPECT_TRUE(mind.leftAreaActions.empty());
}

TEST(CreatureHand, ALeashedCreatureIsHeldAsUsual)
{
	using creature_hand::CreaturePress;
	using creature_hand::PressOn;
	// The leash on its own creature does not stop the hand taking hold of it
	EXPECT_EQ(PressOn(true, true), CreaturePress::Hold);
	EXPECT_EQ(PressOn(true, false), CreaturePress::Hold);
	// With the leash on, a press on another creature ties the leash to it
	EXPECT_EQ(PressOn(false, true), CreaturePress::Leash);
	EXPECT_EQ(PressOn(false, false), CreaturePress::Hold);
}

TEST(CreatureHand, TheHandHoldsOnlyItsOwnPlayersCreature)
{
	using creature_hand::MayHold;
	constexpr auto k_Hand = PlayerNames::PLAYER_ONE;
	EXPECT_TRUE(MayHold({.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger}, k_Hand));
	// Never Khazar's, Lethys's or any other god's
	EXPECT_FALSE(MayHold({.owner = PlayerNames::PLAYER_THREE, .species = CreatureType::Tortoise}, k_Hand));
	EXPECT_FALSE(MayHold({.owner = PlayerNames::PLAYER_FOUR, .species = CreatureType::Wolf}, k_Hand));
	// Another god's hand holds that god's own
	EXPECT_TRUE(MayHold({.owner = PlayerNames::PLAYER_THREE, .species = CreatureType::Tortoise}, PlayerNames::PLAYER_THREE));
	// Not nobody's, nor an ogre, nor one asleep or frozen
	EXPECT_FALSE(MayHold({.owner = PlayerNames::NEUTRAL, .species = CreatureType::Tiger}, k_Hand));
	EXPECT_FALSE(MayHold({.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Ogre}, k_Hand));
	EXPECT_FALSE(MayHold({.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger, .asleep = true}, k_Hand));
	EXPECT_FALSE(MayHold({.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger, .frozen = true}, k_Hand));
	// Nor one out cold in a faint
	EXPECT_FALSE(MayHold({.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger, .fainting = true}, k_Hand));
}

TEST(CreatureHand, TheHandHoldsTheCreatureOfAnAlliedGodAsItsOwn)
{
	using creature_hand::MayHold;
	constexpr auto k_Hand = PlayerNames::PLAYER_ONE;
	const creature_hand::Holdable lethys {.owner = PlayerNames::PLAYER_TWO, .species = CreatureType::Cow};
	EXPECT_TRUE(MayHold(lethys, k_Hand, true));
	EXPECT_FALSE(MayHold(lethys, k_Hand, false));
	// An ally's creature that does not take hold is not held
	EXPECT_FALSE(MayHold({.owner = PlayerNames::PLAYER_TWO, .species = CreatureType::Cow, .asleep = true}, k_Hand, true));
	EXPECT_FALSE(
	    MayHold({.owner = PlayerNames::PLAYER_TWO, .species = CreatureType::Cow, .scriptControlled = true}, k_Hand, true));
	EXPECT_FALSE(MayHold({.owner = PlayerNames::PLAYER_TWO, .species = CreatureType::Ogre}, k_Hand, true));
}

// A changed client can send the feedback of a release for any creature. Only the creature that the player's hand can
// hold gets it.
TEST(CreatureHand, TheFeedbackOfAReleaseComesOnlyToTheCreatureOfThePlayerOrOfAnAlly)
{
	using creature_hand::FeelsHandOf;
	constexpr auto k_Hand = PlayerNames::PLAYER_ONE;
	EXPECT_TRUE(FeelsHandOf(PlayerNames::PLAYER_ONE, k_Hand, false));
	EXPECT_TRUE(FeelsHandOf(PlayerNames::PLAYER_TWO, k_Hand, true));
	EXPECT_FALSE(FeelsHandOf(PlayerNames::PLAYER_TWO, k_Hand, false));
	EXPECT_FALSE(FeelsHandOf(PlayerNames::NEUTRAL, k_Hand, false));
	EXPECT_FALSE(FeelsHandOf(PlayerNames::NEUTRAL, k_Hand, true));
}

TEST(CreatureHand, TheInteractTipIsForTheOwnCreatureAnywhereAndOthersInTheInfluence)
{
	using creature_hand::Holdable;
	using creature_hand::ShowsInteractTip;
	const Holdable own {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger};
	const Holdable other {.owner = PlayerNames::PLAYER_THREE, .species = CreatureType::Tiger};
	// The player's own creature, inside the influence or out
	EXPECT_TRUE(ShowsInteractTip(PlayerNames::PLAYER_ONE, own, false, true));
	EXPECT_TRUE(ShowsInteractTip(PlayerNames::PLAYER_ONE, own, false, false));
	// Another player's creature only inside it
	EXPECT_TRUE(ShowsInteractTip(PlayerNames::PLAYER_ONE, other, false, true));
	EXPECT_FALSE(ShowsInteractTip(PlayerNames::PLAYER_ONE, other, false, false));
	// A creature of no player never
	EXPECT_FALSE(ShowsInteractTip(PlayerNames::PLAYER_ONE, Holdable {.species = CreatureType::Tiger}, false, true));
}

TEST(CreatureHand, TheInteractTipShowsTheActionButtonOnlyWhereTheHandCanTakeHold)
{
	using creature_hand::Holdable;
	using creature_hand::InteractTip;
	using creature_hand::InteractTipOf;
	const Holdable own {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger};
	const Holdable other {.owner = PlayerNames::PLAYER_THREE, .species = CreatureType::Tiger};
	const Holdable asleep {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger, .asleep = true};
	// Within the influence, over a creature that takes hold: the Action button
	EXPECT_EQ(InteractTipOf(PlayerNames::PLAYER_ONE, own, false, true), InteractTip::WithAction);
	EXPECT_EQ(InteractTipOf(PlayerNames::PLAYER_ONE, other, false, true), InteractTip::WithAction);
	// Out of the influence, the player's own creature: the words with no mouse
	EXPECT_EQ(InteractTipOf(PlayerNames::PLAYER_ONE, own, false, false), InteractTip::WordsOnly);
	// A creature that does not take hold: the words with no mouse
	EXPECT_EQ(InteractTipOf(PlayerNames::PLAYER_ONE, asleep, false, true), InteractTip::WordsOnly);
	// No tooltip
	EXPECT_EQ(InteractTipOf(PlayerNames::PLAYER_ONE, other, false, false), InteractTip::None);
	EXPECT_EQ(InteractTipOf(PlayerNames::PLAYER_ONE, own, true, true), InteractTip::None);
}

TEST(CreatureHand, TheInteractTipIsNeverForACreatureUnderAScriptsControl)
{
	using creature_hand::Holdable;
	using creature_hand::ShowsInteractTip;
	const Holdable own {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger, .scriptControlled = true};
	EXPECT_FALSE(ShowsInteractTip(PlayerNames::PLAYER_ONE, own, false, true));
	EXPECT_FALSE(ShowsInteractTip(PlayerNames::PLAYER_ONE, own, false, false));
}

TEST(CreatureHand, ASleepingCreatureStillShowsTheInteractTip)
{
	using creature_hand::Holdable;
	using creature_hand::ShowsInteractTip;
	const Holdable own {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger, .asleep = true};
	const Holdable other {.owner = PlayerNames::PLAYER_THREE, .species = CreatureType::Tiger, .frozen = true};
	EXPECT_TRUE(ShowsInteractTip(PlayerNames::PLAYER_ONE, own, false, false));
	EXPECT_TRUE(ShowsInteractTip(PlayerNames::PLAYER_ONE, own, false, true));
	EXPECT_TRUE(ShowsInteractTip(PlayerNames::PLAYER_ONE, other, false, true));
	EXPECT_FALSE(ShowsInteractTip(PlayerNames::PLAYER_ONE, other, false, false));
}

TEST(CreatureHand, ACreatureAlreadyHeldOffersNoInteractTipWithinTheInfluence)
{
	using creature_hand::Holdable;
	using creature_hand::ShowsInteractTip;
	const Holdable own {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger};
	EXPECT_FALSE(ShowsInteractTip(PlayerNames::PLAYER_ONE, own, true, true));
	// Outside it the own creature is offered whatever holds it
	EXPECT_TRUE(ShowsInteractTip(PlayerNames::PLAYER_ONE, own, true, false));
	// One that wouldn't take hold isn't held by the hand in the first place, and is offered
	const Holdable asleep {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger, .asleep = true};
	EXPECT_TRUE(ShowsInteractTip(PlayerNames::PLAYER_ONE, asleep, true, true));
}

TEST(CreatureHand, ACreatureUnderAScriptsControlCantBeHeld)
{
	EXPECT_FALSE(creature_hand::MayHold(
	    {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Tiger, .scriptControlled = true}, PlayerNames::PLAYER_ONE));
}

TEST(CreatureHand, AQuickPressIsAClickAndALongOneAHold)
{
	using creature_hand::IsClick;
	EXPECT_TRUE(IsClick(150.0f, false));
	EXPECT_FALSE(IsClick(creature_hand::k_ClickMaxMs, false));
	// Stroking or slapping makes it a hold however quick
	EXPECT_FALSE(IsClick(150.0f, true));
	// A click can never last long enough to stroke
	EXPECT_LE(creature_hand::k_ClickMaxMs, creature_feedback::k_StrokeHoldMs);
}

TEST(LeashOwnership, TheLeashComesOffAtTheTempleOnlyWhenThePlayerHoldsIt)
{
	EXPECT_TRUE(ComesOffAtTemple(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(ComesOffAtTemple(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_TWO));
	// The creature wears no leash
	EXPECT_FALSE(ComesOffAtTemple(PlayerNames::PLAYER_ONE, std::nullopt));
	// Another player holds the leash
	EXPECT_FALSE(ComesOffAtTemple(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO));
	// Nobody goes into a temple
	EXPECT_FALSE(ComesOffAtTemple(PlayerNames::NEUTRAL, PlayerNames::NEUTRAL));
}

TEST(LeashOwnership, AnotherPlayersTapOnALeashPostDoesNothing)
{
	EXPECT_TRUE(MayTapPost(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(MayTapPost(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_TWO));
	// The turn does the tap for the player that sent it, never for the owner of the post alone
	EXPECT_FALSE(MayTapPost(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(MayTapPost(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_THREE));
	EXPECT_FALSE(MayTapPost(PlayerNames::NEUTRAL, PlayerNames::NEUTRAL));
}
