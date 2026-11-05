/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <SDL_events.h>
#include <gtest/gtest.h>

#include "Input/PointerButtons.h"

using openblack::input::HandButtonsOf;
using openblack::input::PointerButtons;

namespace
{
SDL_Event ButtonEvent(uint32_t type, uint8_t button)
{
	SDL_Event event {};
	event.type = type;
	event.button.button = button;
	return event;
}

SDL_Event Motion(uint32_t state)
{
	SDL_Event event {};
	event.type = SDL_MOUSEMOTION;
	event.motion.state = state;
	return event;
}
} // namespace

TEST(PointerButtons, APressAndALetGoSetEachButton)
{
	PointerButtons buttons;
	EXPECT_FALSE(buttons.Apply(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT)));
	EXPECT_FALSE(buttons.Apply(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_MIDDLE)));
	EXPECT_FALSE(buttons.Apply(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT)));
	EXPECT_TRUE(buttons.left);
	EXPECT_TRUE(buttons.middle);
	EXPECT_TRUE(buttons.right);
	EXPECT_FALSE(buttons.Apply(ButtonEvent(SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT)));
	EXPECT_FALSE(buttons.Apply(ButtonEvent(SDL_MOUSEBUTTONUP, SDL_BUTTON_MIDDLE)));
	EXPECT_FALSE(buttons.left);
	EXPECT_FALSE(buttons.middle);
	EXPECT_TRUE(buttons.right);
}

TEST(PointerButtons, ALetGoOfTheRightButtonIsALetGoOnlyWhileTheButtonIsDown)
{
	PointerButtons buttons;
	// The menu took the press, so the game did not see the button go down
	EXPECT_FALSE(buttons.Apply(ButtonEvent(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT)));
	EXPECT_FALSE(buttons.Apply(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT)));
	EXPECT_TRUE(buttons.Apply(ButtonEvent(SDL_MOUSEBUTTONUP, SDL_BUTTON_RIGHT)));
	EXPECT_FALSE(buttons.right);
}

TEST(PointerButtons, TheMotionSetsTheButtonsFromItsMask)
{
	PointerButtons buttons;
	EXPECT_FALSE(buttons.Apply(Motion(SDL_BUTTON_LMASK | SDL_BUTTON_RMASK)));
	EXPECT_TRUE(buttons.left);
	EXPECT_FALSE(buttons.middle);
	EXPECT_TRUE(buttons.right);
	// A motion with no right button in its mask lets go of the button that was down
	EXPECT_TRUE(buttons.Apply(Motion(SDL_BUTTON_MMASK)));
	EXPECT_FALSE(buttons.left);
	EXPECT_TRUE(buttons.middle);
	EXPECT_FALSE(buttons.right);
	EXPECT_FALSE(buttons.Apply(Motion(0)));
	EXPECT_FALSE(buttons.middle);
}

TEST(PointerButtons, OtherEventsChangeNothing)
{
	PointerButtons buttons;
	EXPECT_FALSE(buttons.Apply(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT)));
	SDL_Event key {};
	key.type = SDL_KEYDOWN;
	EXPECT_FALSE(buttons.Apply(key));
	EXPECT_TRUE(buttons.right);
	EXPECT_FALSE(buttons.Apply(ButtonEvent(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_X1)));
	EXPECT_FALSE(buttons.left);
	EXPECT_FALSE(buttons.middle);
}

TEST(HandButtons, TheMiddleButtonGripsTheLandAndTurnsTheCamera)
{
	const auto hand = HandButtonsOf({.middle = true}, false, true, true);
	EXPECT_TRUE(hand.gripping);
	EXPECT_TRUE(hand.rotating);
	EXPECT_FALSE(hand.selectGrip);
	EXPECT_FALSE(hand.otherButton);
}

TEST(HandButtons, TheLeftButtonGripsTheLandOnlyOffACreatureAndOutOfAFight)
{
	EXPECT_TRUE(HandButtonsOf({.left = true}, false, false, false).gripping);
	EXPECT_TRUE(HandButtonsOf({.left = true}, false, false, false).selectGrip);
	EXPECT_FALSE(HandButtonsOf({.left = true}, false, true, false).gripping);
	EXPECT_FALSE(HandButtonsOf({.left = true}, false, false, true).selectGrip);
}

TEST(HandButtons, TheLeftAndTheRightButtonsTurnTheCamera)
{
	const auto hand = HandButtonsOf({.left = true, .right = true}, false, false, false);
	EXPECT_TRUE(hand.rotating);
	EXPECT_TRUE(hand.otherButton);
}

TEST(HandButtons, TheHandGripsNothingInTheTemple)
{
	const auto hand = HandButtonsOf({.left = true, .middle = true, .right = true}, true, false, false);
	EXPECT_FALSE(hand.gripping);
	EXPECT_FALSE(hand.rotating);
	EXPECT_FALSE(hand.selectGrip);
	// The other button is down in the temple too
	EXPECT_TRUE(hand.otherButton);
}
