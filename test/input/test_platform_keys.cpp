/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <algorithm>
#include <array>

#include <SDL_keycode.h>
#include <SDL_scancode.h>
#include <gtest/gtest.h>

#include "Input/KeyBindings.h"
#include "Input/PlatformKeys.h"

using namespace openblack;
using namespace openblack::input;
using namespace openblack::input::platform_keys;

namespace
{

/// The function keys that a web browser uses for its own commands: help, find, page load, address bar, caret browsing,
/// reader view, menu, full screen and the developer tools
constexpr std::array k_BrowserFunctionKeys {
    SDL_SCANCODE_F1, SDL_SCANCODE_F3,  SDL_SCANCODE_F5,  SDL_SCANCODE_F6,  SDL_SCANCODE_F7,
    SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12,
};

/// The rows that the web defaults change, with their web keys
struct WebChange
{
	BindableActionMap action;
	KeyChord key;
};

constexpr std::array k_WebChanges {
    WebChange {BindableActionMap::HELP, KeyChord {SDL_SCANCODE_H}},
    WebChange {BindableActionMap::ZOOM_TO_REALM, KeyChord {SDL_SCANCODE_R}},
    WebChange {BindableActionMap::ZOOM_TO_CREATURE_ROOM, KeyChord {SDL_SCANCODE_K}},
    WebChange {BindableActionMap::ZOOM_TO_CHALLENGE_ROOM, KeyChord {SDL_SCANCODE_J}},
    WebChange {BindableActionMap::ZOOM_TO_SAVE_GAME_ROOM, KeyChord {SDL_SCANCODE_G}},
    WebChange {BindableActionMap::ZOOM_TO_LIBRARY, KeyChord {SDL_SCANCODE_I}},
    WebChange {BindableActionMap::QUICK_SAVE, KeyChord {SDL_SCANCODE_S, Modifier::Alt}},
    WebChange {BindableActionMap::QUICK_LOAD, KeyChord {SDL_SCANCODE_L, Modifier::Alt}},
};

constexpr Modifiers k_None {};
constexpr Modifiers k_Shift {.shift = true};
constexpr Modifiers k_Ctrl {.ctrl = true};
constexpr Modifiers k_Alt {.alt = true};
constexpr Modifiers k_CtrlShift {.shift = true, .ctrl = true};
constexpr Modifiers k_CtrlAlt {.ctrl = true, .alt = true};

} // namespace

TEST(PlatformKeys, DesktopSelectsTheGameDefaults)
{
	EXPECT_EQ(&DefaultKeyBindings(Platform::Desktop), &k_DefaultKeyBindings);
	EXPECT_EQ(&DefaultKeyBindings(Platform::Web), &k_WebDefaultKeyBindings);
	if constexpr (k_Platform == Platform::Desktop)
	{
		EXPECT_EQ(&DefaultKeyBindings(), &k_DefaultKeyBindings);
	}
}

TEST(PlatformKeys, WebDefaultsChangeOnlyTheBrowserKeys)
{
	for (size_t i = 0; i < k_KeyBindingCount; ++i)
	{
		const auto& desktop = k_DefaultKeyBindings.at(i);
		const auto& web = k_WebDefaultKeyBindings.at(i);
		SCOPED_TRACE(desktop.name);
		EXPECT_EQ(web.action, desktop.action);
		EXPECT_EQ(web.name, desktop.name);
		EXPECT_EQ(web.category, desktop.category);
		EXPECT_EQ(web.mouse, desktop.mouse);
		EXPECT_EQ(web.status, desktop.status);
		const auto change = std::ranges::find(k_WebChanges, desktop.action, &WebChange::action);
		if (change != k_WebChanges.end())
		{
			EXPECT_EQ(web.key, change->key);
		}
		else
		{
			EXPECT_EQ(web.key, desktop.key);
		}
	}
}

TEST(PlatformKeys, WebDefaultsUseNoBrowserKeys)
{
	for (const auto& binding : k_WebDefaultKeyBindings)
	{
		SCOPED_TRACE(binding.name);
		if (!binding.key.has_value())
		{
			continue;
		}
		EXPECT_NE(binding.key->modifier, Modifier::Ctrl);
		EXPECT_EQ(std::ranges::count(k_BrowserFunctionKeys, binding.key->key), 0);
	}
	EXPECT_TRUE(FindConflicts(k_WebDefaultKeyBindings).empty());
}

TEST(PlatformKeys, WebQuickSaveWinsOverVillagerDetails)
{
	EXPECT_EQ(ActionsForKeyDown(k_WebDefaultKeyBindings, SDL_SCANCODE_S, KMOD_LALT), BindableActionMap::QUICK_SAVE);
	EXPECT_EQ(ActionsForKeyDown(k_WebDefaultKeyBindings, SDL_SCANCODE_S, KMOD_NONE), BindableActionMap::SHOW_VILLAGER_DETAILS);
	EXPECT_EQ(ActionsForKeyDown(k_WebDefaultKeyBindings, SDL_SCANCODE_L, KMOD_RALT), BindableActionMap::QUICK_LOAD);
	EXPECT_EQ(ActionsForKeyDown(k_WebDefaultKeyBindings, SDL_SCANCODE_F1, KMOD_NONE), BindableActionMap::NONE);
}

TEST(PlatformKeys, DesktopBookmarkKeysAreTheGameKeys)
{
	EXPECT_EQ(BookmarkKeyFor(Platform::Desktop, k_None), BookmarkKey::Jump);
	EXPECT_EQ(BookmarkKeyFor(Platform::Desktop, k_Ctrl), BookmarkKey::Set);
	EXPECT_EQ(BookmarkKeyFor(Platform::Desktop, k_CtrlShift), BookmarkKey::Clear);
	EXPECT_EQ(BookmarkKeyFor(Platform::Desktop, Modifiers {.shift = true, .ctrl = true, .alt = true}), BookmarkKey::Clear);
	EXPECT_EQ(BookmarkKeyFor(Platform::Desktop, k_Shift), BookmarkKey::ToggleFollow);
	EXPECT_EQ(BookmarkKeyFor(Platform::Desktop, Modifiers {.shift = true, .alt = true}), BookmarkKey::ToggleFollow);
	EXPECT_EQ(BookmarkKeyFor(Platform::Desktop, k_Alt), BookmarkKey::None);
	EXPECT_EQ(BookmarkKeyFor(Platform::Desktop, k_CtrlAlt), BookmarkKey::None);
}

TEST(PlatformKeys, WebBookmarkKeysAvoidCtrlAndADigit)
{
	EXPECT_EQ(BookmarkKeyFor(Platform::Web, k_None), BookmarkKey::Jump);
	EXPECT_EQ(BookmarkKeyFor(Platform::Web, k_Ctrl), BookmarkKey::None);
	EXPECT_EQ(BookmarkKeyFor(Platform::Web, k_CtrlShift), BookmarkKey::Set);
	EXPECT_EQ(BookmarkKeyFor(Platform::Web, k_CtrlAlt), BookmarkKey::Clear);
	EXPECT_EQ(BookmarkKeyFor(Platform::Web, k_Shift), BookmarkKey::ToggleFollow);
	EXPECT_EQ(BookmarkKeyFor(Platform::Web, k_Alt), BookmarkKey::None);
}

TEST(PlatformKeys, SpeedKeys)
{
	EXPECT_EQ(SpeedKeyFor(Platform::Desktop, SDLK_1, k_Alt), SpeedKey::Slower);
	EXPECT_EQ(SpeedKeyFor(Platform::Desktop, SDLK_2, k_Alt), SpeedKey::Faster);
	EXPECT_EQ(SpeedKeyFor(Platform::Desktop, SDLK_1, k_CtrlAlt), SpeedKey::None);
	EXPECT_EQ(SpeedKeyFor(Platform::Desktop, SDLK_1, k_None), SpeedKey::None);
	EXPECT_EQ(SpeedKeyFor(Platform::Desktop, SDLK_MINUS, k_Alt), SpeedKey::None);
	EXPECT_EQ(SpeedKeyFor(Platform::Web, SDLK_MINUS, k_Alt), SpeedKey::Slower);
	EXPECT_EQ(SpeedKeyFor(Platform::Web, SDLK_EQUALS, k_Alt), SpeedKey::Faster);
	EXPECT_EQ(SpeedKeyFor(Platform::Web, SDLK_1, k_Alt), SpeedKey::None);
	EXPECT_EQ(SpeedKeyFor(Platform::Web, SDLK_EQUALS, Modifiers {.shift = true, .alt = true}), SpeedKey::None);
}

TEST(PlatformKeys, BookmarkMoves)
{
	EXPECT_EQ(BookmarkMoveFor(Platform::Desktop, SDLK_BACKSPACE, k_None), BookmarkMove::Back);
	EXPECT_EQ(BookmarkMoveFor(Platform::Desktop, SDLK_BACKSPACE, k_Ctrl), BookmarkMove::Back);
	EXPECT_EQ(BookmarkMoveFor(Platform::Desktop, SDLK_TAB, k_Shift), BookmarkMove::Previous);
	EXPECT_EQ(BookmarkMoveFor(Platform::Desktop, SDLK_TAB, k_Ctrl), BookmarkMove::Next);
	EXPECT_EQ(BookmarkMoveFor(Platform::Desktop, SDLK_TAB, k_None), BookmarkMove::NextTown);
	EXPECT_EQ(BookmarkMoveFor(Platform::Desktop, SDLK_a, k_None), BookmarkMove::None);
	EXPECT_EQ(BookmarkMoveFor(Platform::Web, SDLK_BACKSPACE, k_None), BookmarkMove::Back);
	EXPECT_EQ(BookmarkMoveFor(Platform::Web, SDLK_BACKSPACE, k_Ctrl), BookmarkMove::Next);
	EXPECT_EQ(BookmarkMoveFor(Platform::Web, SDLK_TAB, k_Shift), BookmarkMove::Previous);
	EXPECT_EQ(BookmarkMoveFor(Platform::Web, SDLK_TAB, k_None), BookmarkMove::NextTown);
}

TEST(PlatformKeys, RendererStatisticsKey)
{
	EXPECT_EQ(RendererStatisticsKey(Platform::Desktop), SDLK_F11);
	EXPECT_EQ(RendererStatisticsKey(Platform::Web), SDLK_INSERT);
}

TEST(PlatformKeys, DesktopEscapeAlwaysGoesToTheGame)
{
	EXPECT_EQ(EscapeOutcomeFor(Platform::Desktop, false), EscapeOutcome::ToGame);
	EXPECT_EQ(EscapeOutcomeFor(Platform::Desktop, true), EscapeOutcome::ToGame);
}

TEST(PlatformKeys, WebFirstEscapeFreesThePointerSecondGoesToTheGame)
{
	PointerHold hold;
	hold.SetLocked(true);
	ASSERT_TRUE(hold.Held());
	// The first Escape frees the pointer
	EXPECT_EQ(EscapeOutcomeFor(Platform::Web, hold.Held()), EscapeOutcome::FreePointer);
	hold.AskFree();
	// The page has not freed the pointer yet, but a second Escape goes to the game
	hold.SetLocked(true);
	EXPECT_FALSE(hold.Held());
	EXPECT_EQ(EscapeOutcomeFor(Platform::Web, hold.Held()), EscapeOutcome::ToGame);
	// The page frees the pointer; Escape still goes to the game
	hold.SetLocked(false);
	EXPECT_EQ(EscapeOutcomeFor(Platform::Web, hold.Held()), EscapeOutcome::ToGame);
	// A click takes the pointer again, and the next Escape frees it again
	hold.SetLocked(true);
	EXPECT_TRUE(hold.Held());
	EXPECT_EQ(EscapeOutcomeFor(Platform::Web, hold.Held()), EscapeOutcome::FreePointer);
}

TEST(PlatformKeys, PointerHoldWithoutALockAsksNothing)
{
	PointerHold hold;
	hold.AskFree();
	hold.SetLocked(true);
	EXPECT_TRUE(hold.Held());
}
