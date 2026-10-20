/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Common/PlayerProfiles.h"
#include "Input/KeyBindings.h"
#include "Input/KeyRebinding.h"

using namespace openblack::input;
using namespace openblack::input::rebinding;

namespace
{
size_t RowOf(const KeyBindingTable& bindings, BindableActionMap action)
{
	return IndexOf(bindings, action).value_or(bindings.size());
}
} // namespace

TEST(KeyRebinding, TheGameKeepsSomeKeysForItself)
{
	EXPECT_FALSE(IsBindable(SDL_SCANCODE_ESCAPE));
	EXPECT_FALSE(IsBindable(SDL_SCANCODE_TAB));
	EXPECT_FALSE(IsBindable(SDL_SCANCODE_P));
	EXPECT_FALSE(IsBindable(SDL_SCANCODE_F1));
	EXPECT_FALSE(IsBindable(SDL_SCANCODE_F2));
	EXPECT_FALSE(IsBindable(SDL_SCANCODE_KP_ENTER));
	// The main Enter key and the other function keys can be bound
	EXPECT_TRUE(IsBindable(SDL_SCANCODE_RETURN));
	EXPECT_TRUE(IsBindable(SDL_SCANCODE_F3));
	EXPECT_TRUE(IsBindable(SDL_SCANCODE_RCTRL));
	// A key that the game has no number for is ignored
	EXPECT_FALSE(IsBindable(SDL_SCANCODE_F20));
}

TEST(KeyRebinding, KeysHaveTheNumbersOfTheGame)
{
	EXPECT_EQ(GameKeyCode(SDL_SCANCODE_F1), 0x3B);
	EXPECT_EQ(GameKeyCode(SDL_SCANCODE_RCTRL), 0x9D);
	EXPECT_EQ(GameKeyCode(SDL_SCANCODE_UP), 0xC8);
	EXPECT_EQ(KeyOfGameCode(0x36), SDL_SCANCODE_RSHIFT);
	EXPECT_FALSE(KeyOfGameCode(0).has_value());
	EXPECT_FALSE(KeyOfGameCode(0x1FF).has_value());
}

TEST(KeyRebinding, KeysHaveTheSystemsNamesWithNoSides)
{
	EXPECT_EQ(KeyName(SDL_SCANCODE_RCTRL), u"Ctrl");
	EXPECT_EQ(KeyName(SDL_SCANCODE_RSHIFT), u"Shift");
	EXPECT_EQ(KeyName(SDL_SCANCODE_SPACE), u"Space");
	EXPECT_EQ(KeyName(SDL_SCANCODE_UP), u"Up");
	// The system swaps the names of the Num Lock and Pause keys
	EXPECT_EQ(KeyName(SDL_SCANCODE_NUMLOCKCLEAR), u"Pause");
	EXPECT_EQ(KeyName(SDL_SCANCODE_PAUSE), u"Num Lock");
	EXPECT_EQ(ChordName(KeyChord {.key = SDL_SCANCODE_S, .modifier = Modifier::Ctrl}), u"Ctrl + S");
	EXPECT_EQ(ChordName(std::nullopt), u"");
}

TEST(KeyRebinding, OnlyTheFirstHeldModifierCounts)
{
	EXPECT_EQ(CapturedKey(SDL_SCANCODE_S, {}), (KeyChord {.key = SDL_SCANCODE_S, .modifier = Modifier::None}));
	EXPECT_EQ(CapturedKey(SDL_SCANCODE_S, {.shift = true, .ctrl = true, .alt = true}),
	          (KeyChord {.key = SDL_SCANCODE_S, .modifier = Modifier::Shift}));
	EXPECT_EQ(CapturedKey(SDL_SCANCODE_S, {.shift = false, .ctrl = true, .alt = true}),
	          (KeyChord {.key = SDL_SCANCODE_S, .modifier = Modifier::Ctrl}));
	EXPECT_EQ(CapturedKey(SDL_SCANCODE_S, {.shift = false, .ctrl = false, .alt = true}),
	          (KeyChord {.key = SDL_SCANCODE_S, .modifier = Modifier::Alt}));
	// A modifier key pressed alone is the key itself
	EXPECT_EQ(CapturedKey(SDL_SCANCODE_RCTRL, {.shift = false, .ctrl = true, .alt = false}),
	          (KeyChord {.key = SDL_SCANCODE_RCTRL, .modifier = Modifier::None}));
	// A Shift key with Ctrl held is still the Shift key alone, because Shift comes first
	EXPECT_EQ(CapturedKey(SDL_SCANCODE_LSHIFT, {.shift = true, .ctrl = true, .alt = false}),
	          (KeyChord {.key = SDL_SCANCODE_LSHIFT, .modifier = Modifier::None}));
	// A Ctrl key with Shift held takes Shift
	EXPECT_EQ(CapturedKey(SDL_SCANCODE_LCTRL, {.shift = true, .ctrl = true, .alt = false}),
	          (KeyChord {.key = SDL_SCANCODE_LCTRL, .modifier = Modifier::Shift}));
}

TEST(KeyRebinding, TheLeftButtonComesFirstAndTheWheelLast)
{
	EXPECT_EQ(CapturedMouse(true, true, true, 1), MouseInput::LeftButton);
	EXPECT_EQ(CapturedMouse(false, true, true, 1), MouseInput::RightButton);
	EXPECT_EQ(CapturedMouse(false, false, true, 1), MouseInput::MiddleButton);
	EXPECT_EQ(CapturedMouse(false, false, false, 2), MouseInput::WheelUp);
	EXPECT_EQ(CapturedMouse(false, false, false, -1), MouseInput::WheelDown);
	EXPECT_FALSE(CapturedMouse(false, false, false, 0).has_value());
}

TEST(KeyRebinding, ANewKeyTakesItFromTheOtherActions)
{
	auto bindings = k_DefaultKeyBindings;
	const auto help = RowOf(bindings, BindableActionMap::HELP);
	const auto names = RowOf(bindings, BindableActionMap::SHOW_VILLAGER_NAMES);
	const auto quickSave = RowOf(bindings, BindableActionMap::QUICK_SAVE);
	const auto details = RowOf(bindings, BindableActionMap::SHOW_VILLAGER_DETAILS);

	AssignKey(bindings, help, KeyChord {.key = SDL_SCANCODE_N});
	EXPECT_EQ(bindings.at(help).key, (KeyChord {.key = SDL_SCANCODE_N}));
	EXPECT_FALSE(bindings.at(names).key.has_value());

	// Only the same key with the same modifier: Ctrl+S leaves S alone
	AssignKey(bindings, help, KeyChord {.key = SDL_SCANCODE_S, .modifier = Modifier::Ctrl});
	EXPECT_FALSE(bindings.at(quickSave).key.has_value());
	EXPECT_EQ(bindings.at(details).key, (KeyChord {.key = SDL_SCANCODE_S}));

	// No key unbinds the action's key and keeps its mouse input
	const auto move = RowOf(bindings, BindableActionMap::MOVE);
	AssignKey(bindings, move, std::nullopt);
	EXPECT_FALSE(bindings.at(move).key.has_value());
	EXPECT_EQ(bindings.at(move).mouse, MouseInput::LeftButton);
}

TEST(KeyRebinding, ANewMouseInputTakesItFromEveryAction)
{
	auto bindings = k_DefaultKeyBindings;
	const auto help = RowOf(bindings, BindableActionMap::HELP);
	const auto move = RowOf(bindings, BindableActionMap::MOVE);
	AssignMouse(bindings, help, MouseInput::LeftButton);
	EXPECT_EQ(bindings.at(help).mouse, MouseInput::LeftButton);
	EXPECT_EQ(bindings.at(move).mouse, MouseInput::None);
	// The action keeps its key
	EXPECT_EQ(bindings.at(help).key, (KeyChord {.key = SDL_SCANCODE_F1}));
}

TEST(KeyRebinding, TheProfileKeepsTheNumbersOfTheGame)
{
	auto bindings = k_DefaultKeyBindings;
	const auto kept = ToKept(bindings);
	ASSERT_EQ(kept.size(), k_KeyBindingCount);
	const auto quickSave = static_cast<uint32_t>(RowOf(bindings, BindableActionMap::QUICK_SAVE));
	EXPECT_EQ(kept.at(quickSave),
	          (openblack::player_profiles::KeptKey {.primary = 0x1F, .primaryModifier = 0x1D, .mouseButton = 0}));
	const auto zoomIn = static_cast<uint32_t>(RowOf(bindings, BindableActionMap::ZOOM_IN));
	EXPECT_EQ(kept.at(zoomIn), (openblack::player_profiles::KeptKey {.primary = 0, .primaryModifier = 0, .mouseButton = 3}));
	EXPECT_EQ(FromKept(kept), bindings);
}

TEST(KeyRebinding, AProfileWithoutBindingsHasTheDefaults)
{
	EXPECT_EQ(FromKept({}), k_DefaultKeyBindings);
	// A kept row changes only that row
	const auto help = RowOf(k_DefaultKeyBindings, BindableActionMap::HELP);
	const auto bindings =
	    FromKept({{static_cast<uint32_t>(help), {.primary = 0x2D, .primaryModifier = 0x9D, .mouseButton = 2}}});
	EXPECT_EQ(bindings.at(help).key, (KeyChord {.key = SDL_SCANCODE_X, .modifier = Modifier::Ctrl}));
	EXPECT_EQ(bindings.at(help).mouse, MouseInput::MiddleButton);
	EXPECT_EQ(bindings.at(help + 1), k_DefaultKeyBindings.at(help + 1));
}
