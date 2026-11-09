/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <bitset>

#include <SDL_events.h>
#include <gtest/gtest.h>

#include "Input/GameActionMap.h"
#include "Input/InjectedInput.h"
#include "Input/InputLock.h"
#include "Input/PlatformKeys.h"

using namespace openblack;

// The game's own mouse and keyboard events are told from the player's, which an agent driving the game keeps out
TEST(InjectedInput, TheGamesOwnEventsAreToldFromThePlayers)
{
	constexpr std::array<uint32_t, 6> k_DeviceEvents {SDL_MOUSEMOTION, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP,
	                                                  SDL_MOUSEWHEEL,  SDL_KEYDOWN,         SDL_KEYUP};
	for (const auto type : k_DeviceEvents)
	{
		SDL_Event event {};
		event.type = type;
		EXPECT_TRUE(input::IsPlayerDeviceEvent(event)) << type;
		EXPECT_FALSE(input::IsInjected(event)) << type;
		input::MarkInjected(event);
		EXPECT_TRUE(input::IsInjected(event)) << type;
	}
	// The touch mouse is the player's too
	SDL_Event touch {};
	touch.type = SDL_MOUSEBUTTONDOWN;
	touch.button.which = SDL_TOUCH_MOUSEID;
	EXPECT_FALSE(input::IsInjected(touch));

	// The window's own events, such as closing it, always come through
	SDL_Event quit {};
	quit.type = SDL_QUIT;
	EXPECT_FALSE(input::IsPlayerDeviceEvent(quit));
	SDL_Event text {};
	text.type = SDL_TEXTINPUT;
	EXPECT_TRUE(input::IsPlayerDeviceEvent(text));
	// The letters that the game types are its own: the mark is the last byte, after the end of the letters
	EXPECT_FALSE(input::IsInjected(text));
	text.text.text[0] = 'a';
	input::MarkInjected(text);
	EXPECT_TRUE(input::IsInjected(text));
	EXPECT_STREQ(text.text.text, "a");
}

namespace
{
SDL_Event Event(uint32_t type)
{
	SDL_Event event {};
	event.type = type;
	return event;
}
SDL_Event Key(SDL_Scancode key, uint16_t mod)
{
	auto event = Event(SDL_KEYDOWN);
	event.key.keysym.scancode = key;
	event.key.keysym.mod = mod;
	return event;
}
} // namespace

// Locked, the player's input is dropped, the game's own gets through, and so do the window's events
TEST(InputLock, DropsThePlayersInputOnly)
{
	input::InputLock lock;
	lock.SetMode(input::LockMode::Locked);
	ASSERT_TRUE(lock.Locked());
	for (const auto type : {SDL_MOUSEMOTION, SDL_MOUSEBUTTONDOWN, SDL_MOUSEWHEEL, SDL_KEYDOWN, SDL_KEYUP, SDL_TEXTINPUT})
	{
		auto event = Event(type);
		EXPECT_EQ(lock.Filter(event), input::LockVerdict::Drop) << type;
		if (type != SDL_TEXTINPUT)
		{
			input::MarkInjected(event);
			EXPECT_EQ(lock.Filter(event), input::LockVerdict::Pass) << type;
		}
	}
	for (const auto type : {SDL_QUIT, SDL_WINDOWEVENT, SDL_RENDER_TARGETS_RESET})
	{
		EXPECT_EQ(lock.Filter(Event(type)), input::LockVerdict::Pass) << type;
	}
	// Unlocked, everything passes
	lock.SetMode(input::LockMode::Unlocked);
	EXPECT_EQ(lock.Filter(Event(SDL_MOUSEMOTION)), input::LockVerdict::Pass);
}

// Ctrl+Alt+Shift+F12 on the keyboard always gets through, and hands the game back to the player
TEST(InputLock, TheEmergencyChordReleasesIt)
{
	input::InputLock lock;
	lock.SetMode(input::LockMode::Locked);
	EXPECT_EQ(lock.Filter(Key(SDL_SCANCODE_F12, KMOD_LCTRL | KMOD_LALT)), input::LockVerdict::Drop);
	EXPECT_EQ(lock.Filter(Key(SDL_SCANCODE_F11, KMOD_LCTRL | KMOD_LALT | KMOD_LSHIFT)), input::LockVerdict::Drop);
	EXPECT_TRUE(lock.Locked());
	EXPECT_EQ(lock.Filter(Key(SDL_SCANCODE_F12, KMOD_RCTRL | KMOD_LALT | KMOD_RSHIFT)), input::LockVerdict::Release);
	EXPECT_FALSE(lock.Locked());
	EXPECT_EQ(lock.Mode(), input::LockMode::Unlocked);
	EXPECT_EQ(lock.Filter(Event(SDL_MOUSEMOTION)), input::LockVerdict::Pass);
}

// In auto mode it locks while a client is connected, and a little after the last one leaves
TEST(InputLock, AutoLocksWhileAClientIsConnected)
{
	input::InputLock lock;
	lock.SetMode(input::LockMode::Auto);
	EXPECT_FALSE(lock.Locked());
	lock.Update(true, 0.016f);
	EXPECT_TRUE(lock.Locked());
	lock.Update(false, input::InputLock::k_LingerSeconds - 0.5f);
	EXPECT_TRUE(lock.Locked());
	lock.Update(false, 1.0f);
	EXPECT_FALSE(lock.Locked());
}

// Each Shift, Ctrl, Alt and system key held sets its own modifier bit, and other keys set none
TEST(InjectedInput, TheHeldModifierKeysMakeTheModifiers)
{
	std::bitset<SDL_NUM_SCANCODES> held;
	EXPECT_EQ(input::ModifiersOfHeldKeys(held), KMOD_NONE);
	held.set(SDL_SCANCODE_1);
	held.set(SDL_SCANCODE_LEFT);
	EXPECT_EQ(input::ModifiersOfHeldKeys(held), KMOD_NONE);
	held.set(SDL_SCANCODE_LCTRL);
	EXPECT_EQ(input::ModifiersOfHeldKeys(held), KMOD_LCTRL);
	held.set(SDL_SCANCODE_RSHIFT);
	held.set(SDL_SCANCODE_LALT);
	held.set(SDL_SCANCODE_RGUI);
	EXPECT_EQ(input::ModifiersOfHeldKeys(held), KMOD_LCTRL | KMOD_RSHIFT | KMOD_LALT | KMOD_RGUI);
	held.reset();
	held.set(SDL_SCANCODE_RCTRL);
	held.set(SDL_SCANCODE_LSHIFT);
	held.set(SDL_SCANCODE_RALT);
	held.set(SDL_SCANCODE_LGUI);
	EXPECT_EQ(input::ModifiersOfHeldKeys(held), KMOD_RCTRL | KMOD_LSHIFT | KMOD_RALT | KMOD_LGUI);
}

// The game's own key event has the key, its state and the modifiers, and is marked as the game's own
TEST(InjectedInput, TheGamesOwnKeyEventHasTheModifiers)
{
	const auto down =
	    input::MakeInjectedKey({.scancode = SDL_SCANCODE_1, .sym = SDLK_1, .down = true, .mod = KMOD_LCTRL, .windowId = 7});
	EXPECT_EQ(down.type, SDL_KEYDOWN);
	EXPECT_EQ(down.key.state, SDL_PRESSED);
	EXPECT_EQ(down.key.repeat, 0);
	EXPECT_EQ(down.key.windowID, 7u);
	EXPECT_EQ(down.key.keysym.scancode, SDL_SCANCODE_1);
	EXPECT_EQ(down.key.keysym.sym, SDLK_1);
	EXPECT_EQ(down.key.keysym.mod, KMOD_LCTRL);
	EXPECT_TRUE(input::IsInjected(down));

	const auto up = input::MakeInjectedKey({.scancode = SDL_SCANCODE_LEFT, .sym = SDLK_LEFT, .down = false, .repeat = true});
	EXPECT_EQ(up.type, SDL_KEYUP);
	EXPECT_EQ(up.key.state, SDL_RELEASED);
	EXPECT_EQ(up.key.repeat, 1);
	EXPECT_EQ(up.key.keysym.mod, KMOD_NONE);
	EXPECT_TRUE(input::IsInjected(up));
}

// A Ctrl that a scenario or the inspector holds is in the modifiers of the keys that the game presses after it, so that
// Ctrl and 1 keep a bookmark as on the keyboard. When Ctrl is let go, the key 1 jumps to the bookmark again.
TEST(InjectedInput, AHeldScriptedCtrlIsInTheModifiersOfTheNextKey)
{
	input::GameActionMap actions;
	EXPECT_EQ(actions.ScriptedModifiers(), KMOD_NONE);
	actions.HoldScriptedKey(SDL_SCANCODE_LCTRL, true);
	const auto withCtrl =
	    input::MakeInjectedKey({.scancode = SDL_SCANCODE_1, .sym = SDLK_1, .down = true, .mod = actions.ScriptedModifiers()});
	EXPECT_EQ(withCtrl.key.keysym.mod, KMOD_LCTRL);
	namespace keys = input::platform_keys;
	const auto held = [](const SDL_Event& event) {
		return keys::Modifiers {
		    .shift = (event.key.keysym.mod & KMOD_SHIFT) != 0,
		    .ctrl = (event.key.keysym.mod & KMOD_CTRL) != 0,
		    .alt = (event.key.keysym.mod & KMOD_ALT) != 0,
		};
	};
	EXPECT_EQ(keys::BookmarkKeyFor(Platform::Desktop, held(withCtrl)), keys::BookmarkKey::Set);

	actions.HoldScriptedKey(SDL_SCANCODE_LCTRL, false);
	EXPECT_EQ(actions.ScriptedModifiers(), KMOD_NONE);
	const auto plain =
	    input::MakeInjectedKey({.scancode = SDL_SCANCODE_1, .sym = SDLK_1, .down = true, .mod = actions.ScriptedModifiers()});
	EXPECT_EQ(keys::BookmarkKeyFor(Platform::Desktop, held(plain)), keys::BookmarkKey::Jump);
}
