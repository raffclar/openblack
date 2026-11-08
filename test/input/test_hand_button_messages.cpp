/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <tuple>
#include <vector>

#include <SDL_events.h>
#include <gtest/gtest.h>

#include "Input/GameActionMap.h"
#include "Input/HandButtonMessages.h"

using namespace openblack::input;
using hand_messages::Button;
using hand_messages::HandButtonMessages;
using hand_messages::Input;
using hand_messages::Message;
using hand_messages::Sent;

namespace
{

constexpr Input k_ActionPress {.kind = Input::Kind::Press, .button = Button::Action};
constexpr Input k_ActionLetGo {.kind = Input::Kind::LetGo, .button = Button::Action};
constexpr Input k_MovePress {.kind = Input::Kind::Press, .button = Button::Move};
constexpr Input k_MoveDoublePress {.kind = Input::Kind::DoublePress, .button = Button::Move};
constexpr Input k_MoveLetGo {.kind = Input::Kind::LetGo, .button = Button::Move};

using Inputs = std::vector<Input>;
using Messages = std::vector<Sent>;

} // namespace

TEST(HandButtonMessages, WithoutAltTheInputsGoToTheHandAsTheyCome)
{
	HandButtonMessages messages;
	EXPECT_FALSE(messages.Frame(Inputs {k_ActionPress}, false, 0).has_value());
	EXPECT_TRUE(messages.HandDown(Button::Action));
	EXPECT_FALSE(messages.Frame(Inputs {k_ActionLetGo}, false, 10).has_value());
	EXPECT_FALSE(messages.HandDown(Button::Action));
}

TEST(HandButtonMessages, WithAltHeldTheHandGetsNoMessage)
{
	HandButtonMessages messages;
	const auto sent = messages.Frame(Inputs {k_ActionPress}, true, 0);
	ASSERT_TRUE(sent.has_value());
	EXPECT_TRUE(sent->empty());
	EXPECT_FALSE(messages.HandDown(Button::Action));
	EXPECT_TRUE(messages.Waits(false));
}

TEST(HandButtonMessages, AClickWithAltSendsItsPressAndLetGoOnTheFirstFrameWithoutAlt)
{
	HandButtonMessages messages;
	std::ignore = messages.Frame(Inputs {k_ActionPress, k_ActionLetGo}, true, 0);
	std::ignore = messages.Frame(Inputs {}, true, 10);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 20),
	          (Messages {{Button::Action, Message::Down}, {Button::Action, Message::Up}}));
	EXPECT_FALSE(messages.Waits(false));
	EXPECT_FALSE(messages.Frame(Inputs {}, false, 30).has_value());
}

TEST(HandButtonMessages, APressHeldThroughAltSendsItsLetGoAsItComes)
{
	HandButtonMessages messages;
	std::ignore = messages.Frame(Inputs {k_MovePress}, true, 0);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 10), (Messages {{Button::Move, Message::Down}}));
	EXPECT_TRUE(messages.HandDown(Button::Move));
	EXPECT_FALSE(messages.Frame(Inputs {k_MoveLetGo}, false, 20).has_value());
	EXPECT_FALSE(messages.HandDown(Button::Move));
}

TEST(HandButtonMessages, TheActionButtonSendsBeforeTheMoveButton)
{
	HandButtonMessages messages;
	std::ignore = messages.Frame(Inputs {k_MovePress, k_MoveLetGo, k_ActionPress, k_ActionLetGo}, true, 0);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 10), (Messages {{Button::Action, Message::Down},
	                                                           {Button::Action, Message::Up},
	                                                           {Button::Move, Message::Down},
	                                                           {Button::Move, Message::Up}}));
}

TEST(HandButtonMessages, TheGameKeepsOnePressAndOneLetGoOfEachButton)
{
	HandButtonMessages messages;
	// Two clicks with Alt send one click
	std::ignore = messages.Frame(Inputs {k_ActionPress, k_ActionLetGo, k_ActionPress, k_ActionLetGo}, true, 0);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 10),
	          (Messages {{Button::Action, Message::Down}, {Button::Action, Message::Up}}));
	EXPECT_FALSE(messages.Waits(false));
}

TEST(HandButtonMessages, AButtonDownAfterTheMessagesSendsItsPressOnTheNextFrame)
{
	HandButtonMessages messages;
	std::ignore = messages.Frame(Inputs {k_ActionPress, k_ActionLetGo, k_ActionPress}, true, 0);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 10),
	          (Messages {{Button::Action, Message::Down}, {Button::Action, Message::Up}}));
	EXPECT_EQ(messages.Frame(Inputs {}, false, 20), (Messages {{Button::Action, Message::Down}}));
	EXPECT_FALSE(messages.Waits(false));
	// The press came from the held button, and its let go comes as the button goes up
	EXPECT_FALSE(messages.Frame(Inputs {k_ActionLetGo}, false, 30).has_value());
	EXPECT_FALSE(messages.HandDown(Button::Action));
}

TEST(HandButtonMessages, ALetGoWithAltSendsTheLetGoOfAPressFromBeforeAlt)
{
	HandButtonMessages messages;
	EXPECT_FALSE(messages.Frame(Inputs {k_MovePress}, false, 0).has_value());
	std::ignore = messages.Frame(Inputs {k_MoveLetGo}, true, 10);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 20), (Messages {{Button::Move, Message::Up}}));
}

TEST(HandButtonMessages, ALetGoAndAPressWithAltSendTheLetGoAndThenThePress)
{
	HandButtonMessages messages;
	EXPECT_FALSE(messages.Frame(Inputs {k_MovePress}, false, 0).has_value());
	std::ignore = messages.Frame(Inputs {k_MoveLetGo, k_MovePress}, true, 10);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 20), (Messages {{Button::Move, Message::Up}}));
	EXPECT_EQ(messages.Frame(Inputs {}, false, 30), (Messages {{Button::Move, Message::Down}}));
}

TEST(HandButtonMessages, APressFromTheHeldButtonGetsItsLetGoOnlyWhenTheButtonIsUp)
{
	HandButtonMessages messages;
	std::ignore = messages.Frame(Inputs {k_ActionPress, k_ActionLetGo, k_ActionPress}, true, 0);
	std::ignore = messages.Frame(Inputs {}, false, 10);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 20), (Messages {{Button::Action, Message::Down}}));
	// A let go and a press with Alt leave the button down, so the hand gets nothing
	std::ignore = messages.Frame(Inputs {k_ActionLetGo, k_ActionPress}, true, 30);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 40), (Messages {}));
	EXPECT_TRUE(messages.HandDown(Button::Action));
}

TEST(HandButtonMessages, ADoubleClickWithAltSendsTheDoubleClickAndTheLetGo)
{
	HandButtonMessages messages;
	std::ignore = messages.Frame(Inputs {k_MovePress, k_MoveLetGo, k_MoveDoublePress, k_MoveLetGo}, true, 0);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 10), (Messages {{Button::Move, Message::Double}, {Button::Move, Message::Up}}));
}

TEST(HandButtonMessages, TwoPressesFromTheHeldButtonInAShortTimeSendADoubleClick)
{
	HandButtonMessages messages;
	// The first press from the held button
	std::ignore = messages.Frame(Inputs {k_ActionPress, k_ActionLetGo, k_ActionPress}, true, 1000);
	std::ignore = messages.Frame(Inputs {}, false, 1000);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 1010), (Messages {{Button::Action, Message::Down}}));
	std::ignore = messages.Frame(Inputs {k_ActionLetGo}, false, 1020);
	// The second press from the held button, 249 milliseconds after the first
	std::ignore = messages.Frame(Inputs {k_ActionPress, k_ActionLetGo, k_ActionPress}, true, 1100);
	std::ignore = messages.Frame(Inputs {}, false, 1200);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 1259), (Messages {{Button::Action, Message::Down}}));
	EXPECT_EQ(messages.Frame(Inputs {}, false, 1270), (Messages {{Button::Action, Message::Double}}));
	EXPECT_FALSE(messages.Waits(false));
}

TEST(HandButtonMessages, TwoPressesFromTheHeldButtonAtTheLimitSendNoDoubleClick)
{
	HandButtonMessages messages;
	std::ignore = messages.Frame(Inputs {k_ActionPress, k_ActionLetGo, k_ActionPress}, true, 1000);
	std::ignore = messages.Frame(Inputs {}, false, 1000);
	std::ignore = messages.Frame(Inputs {}, false, 1010);
	std::ignore = messages.Frame(Inputs {k_ActionLetGo}, false, 1020);
	std::ignore = messages.Frame(Inputs {k_ActionPress, k_ActionLetGo, k_ActionPress}, true, 1100);
	std::ignore = messages.Frame(Inputs {}, false, 1200);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 1260), (Messages {{Button::Action, Message::Down}}));
	EXPECT_FALSE(messages.Waits(false));
}

TEST(HandButtonMessages, AMotionWithAltTellsTheButtonsThatAreDown)
{
	HandButtonMessages messages;
	// The press went elsewhere, but the motion tells that the button is down
	std::ignore = messages.Frame(Inputs {{.kind = Input::Kind::Motion, .moveDown = true}}, true, 0);
	EXPECT_EQ(messages.Frame(Inputs {}, false, 10), (Messages {{Button::Move, Message::Down}}));
}

namespace
{

[[nodiscard]] SDL_Event ButtonEvent(bool down, uint8_t button, uint8_t clicks = 1)
{
	SDL_Event event {};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.button = button;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = clicks;
	event.button.x = 40;
	event.button.y = 30;
	return event;
}

[[nodiscard]] SDL_Event MotionEvent(uint32_t buttons)
{
	SDL_Event event {};
	event.type = SDL_MOUSEMOTION;
	event.motion.state = buttons;
	event.motion.x = 50;
	event.motion.y = 60;
	return event;
}

[[nodiscard]] SDL_Event AltEvent(bool down)
{
	SDL_Event event {};
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.keysym.scancode = SDL_SCANCODE_LALT;
	event.key.keysym.mod = down ? KMOD_LALT : KMOD_NONE;
	return event;
}

/// The game sends each mouse event to the hand, and then to the action map
void Send(GameActionMap& map, const SDL_Event& event)
{
	if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEMOTION)
	{
		map.KeepHandEvent(event, event.type == SDL_MOUSEMOTION);
	}
	map.ProcessEvent(event);
}

[[nodiscard]] std::vector<SDL_Event> Take(GameActionMap& map)
{
	std::vector<SDL_Event> events;
	map.TakeHandEvents([&events](const SDL_Event& event, bool /*movesPointer*/) { events.push_back(event); });
	return events;
}

} // namespace

TEST(GameActionMap, WithoutAltTheHandGetsTheMouseEventsAsTheyCame)
{
	GameActionMap map;
	map.Frame();
	Send(map, ButtonEvent(true, SDL_BUTTON_RIGHT));
	Send(map, MotionEvent(SDL_BUTTON_RMASK));
	const auto events = Take(map);
	ASSERT_EQ(events.size(), 2u);
	EXPECT_EQ(events[0].type, SDL_MOUSEBUTTONDOWN);
	EXPECT_EQ(events[1].type, SDL_MOUSEMOTION);
	EXPECT_EQ(events[1].motion.state, static_cast<uint32_t>(SDL_BUTTON_RMASK));
}

TEST(GameActionMap, WithAltHeldTheHandsButtonsWaitAndTheMotionsKeepTheHandsButtons)
{
	GameActionMap map;
	map.Frame();
	Send(map, AltEvent(true));
	std::ignore = Take(map);
	map.Frame();
	Send(map, ButtonEvent(true, SDL_BUTTON_RIGHT));
	Send(map, MotionEvent(SDL_BUTTON_RMASK));
	const auto events = Take(map);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].type, SDL_MOUSEMOTION);
	EXPECT_EQ(events[0].motion.state, 0u);
	EXPECT_EQ(map.GetButtonMessages(), 0u);
}

TEST(GameActionMap, TheFirstFrameWithoutAltSendsTheClickThatWaited)
{
	GameActionMap map;
	map.Frame();
	Send(map, AltEvent(true));
	Send(map, ButtonEvent(true, SDL_BUTTON_RIGHT));
	Send(map, ButtonEvent(false, SDL_BUTTON_RIGHT));
	EXPECT_TRUE(Take(map).empty());
	map.Frame();
	Send(map, AltEvent(false));
	const auto events = Take(map);
	ASSERT_EQ(events.size(), 2u);
	EXPECT_EQ(events[0].type, SDL_MOUSEBUTTONDOWN);
	EXPECT_EQ(events[0].button.button, SDL_BUTTON_RIGHT);
	EXPECT_EQ(events[0].button.x, 40);
	EXPECT_EQ(events[1].type, SDL_MOUSEBUTTONUP);
	EXPECT_EQ(map.GetButtonMessages(), 2u);
}

TEST(GameActionMap, AltDownLaterInTheFrameStopsTheClickOfTheFrame)
{
	GameActionMap map;
	map.Frame();
	Send(map, ButtonEvent(true, SDL_BUTTON_LEFT));
	Send(map, AltEvent(true));
	EXPECT_TRUE(Take(map).empty());
}

TEST(GameActionMap, TheDoubleClickMarkWaitsForAlt)
{
	GameActionMap map;
	map.Frame();
	Send(map, ButtonEvent(true, SDL_BUTTON_LEFT));
	Send(map, ButtonEvent(false, SDL_BUTTON_LEFT));
	std::ignore = Take(map);
	map.Frame();
	Send(map, ButtonEvent(true, SDL_BUTTON_LEFT, 2));
	Send(map, AltEvent(true));
	std::ignore = Take(map);
	EXPECT_FALSE(map.TakeDoubleClick());
	map.Frame();
	Send(map, AltEvent(false));
	const auto events = Take(map);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].button.clicks, 2);
	EXPECT_TRUE(map.TakeDoubleClick());
}
