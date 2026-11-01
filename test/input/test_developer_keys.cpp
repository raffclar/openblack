/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>

#include <SDL_keycode.h>
#include <SDL_scancode.h>
#include <gtest/gtest.h>

#include "Common/Platform.h"
#include "Input/DeveloperKeys.h"
#include "Input/KeyBindings.h"
#include "Input/PlatformKeys.h"

using namespace openblack;
using namespace openblack::input;
using namespace openblack::input::developer_keys;
using platform_keys::Modifiers;

namespace
{

constexpr std::array k_Platforms {Platform::Desktop, Platform::Web};

/// Every set of Shift, Ctrl and Alt, as the modifier state of SDL
constexpr std::array<uint16_t, 8> k_AllModifiers {
    KMOD_NONE,
    KMOD_LSHIFT,
    KMOD_LCTRL,
    KMOD_LALT,
    KMOD_LSHIFT | KMOD_LCTRL,
    KMOD_LSHIFT | KMOD_LALT,
    KMOD_LCTRL | KMOD_LALT,
    KMOD_LSHIFT | KMOD_LCTRL | KMOD_LALT,
};

/// True when the game uses the key with one or more sets of modifiers, with the default bindings
bool GameUsesKeyWithAnyModifier(Platform platform, SDL_Scancode key)
{
	const auto& bindings = DefaultKeyBindings(platform);
	for (const auto mod : k_AllModifiers)
	{
		if (GameUsesKey(bindings, platform, key, mod))
		{
			return true;
		}
	}
	return false;
}

constexpr Modifiers k_None {};
constexpr Modifiers k_Shift {.shift = true};
constexpr Modifiers k_Ctrl {.ctrl = true};
constexpr Modifiers k_Alt {.alt = true};
constexpr Modifiers k_CtrlShift {.shift = true, .ctrl = true};
constexpr Modifiers k_CtrlAlt {.ctrl = true, .alt = true};

} // namespace

TEST(DeveloperKeys, TheGameKeepsItsOwnKeysWithAnyModifier)
{
	constexpr std::array k_Fixed {
	    SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F2,        SDL_SCANCODE_R,        SDL_SCANCODE_P,     SDL_SCANCODE_M, SDL_SCANCODE_1,
	    SDL_SCANCODE_2,      SDL_SCANCODE_3,         SDL_SCANCODE_4,        SDL_SCANCODE_5,     SDL_SCANCODE_6, SDL_SCANCODE_7,
	    SDL_SCANCODE_TAB,    SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_SPACE,
	};
	for (const auto platform : k_Platforms)
	{
		for (const auto key : k_Fixed)
		{
			for (const auto held : {k_None, k_Shift, k_Ctrl, k_Alt, k_CtrlShift, k_CtrlAlt})
			{
				EXPECT_TRUE(IsFixedGameKey(platform, key, held)) << key;
			}
		}
	}
}

TEST(DeveloperKeys, SomeFixedKeysNeedTheirModifiers)
{
	EXPECT_TRUE(IsFixedGameKey(Platform::Desktop, SDL_SCANCODE_T, k_CtrlAlt));
	EXPECT_FALSE(IsFixedGameKey(Platform::Desktop, SDL_SCANCODE_T, k_Ctrl));
	EXPECT_FALSE(IsFixedGameKey(Platform::Desktop, SDL_SCANCODE_T, k_None));
	// Alt and minus or equals change the speed on the web only
	EXPECT_TRUE(IsFixedGameKey(Platform::Web, SDL_SCANCODE_MINUS, k_Alt));
	EXPECT_TRUE(IsFixedGameKey(Platform::Web, SDL_SCANCODE_EQUALS, k_Alt));
	EXPECT_FALSE(IsFixedGameKey(Platform::Web, SDL_SCANCODE_EQUALS, k_Ctrl));
	EXPECT_FALSE(IsFixedGameKey(Platform::Web, SDL_SCANCODE_EQUALS, k_CtrlAlt));
	EXPECT_FALSE(IsFixedGameKey(Platform::Desktop, SDL_SCANCODE_MINUS, k_Alt));
	EXPECT_FALSE(IsFixedGameKey(Platform::Desktop, SDL_SCANCODE_F10, k_None));
}

TEST(DeveloperKeys, ABoundKeyIsTheGamesWithAnyModifier)
{
	const auto& bindings = DefaultKeyBindings(Platform::Desktop);
	// Z turns the camera: Ctrl and Z is the game's key too, because a binding with no modifier takes any modifier
	EXPECT_TRUE(GameUsesKey(bindings, Platform::Desktop, SDL_SCANCODE_Z, KMOD_NONE));
	EXPECT_TRUE(GameUsesKey(bindings, Platform::Desktop, SDL_SCANCODE_Z, KMOD_LCTRL));
	EXPECT_TRUE(GameUsesKey(bindings, Platform::Desktop, SDL_SCANCODE_F3, KMOD_NONE));
	EXPECT_FALSE(GameUsesKey(bindings, Platform::Desktop, SDL_SCANCODE_D, KMOD_NONE));
	// The web defaults move some actions to letters
	EXPECT_TRUE(GameUsesKey(DefaultKeyBindings(Platform::Web), Platform::Web, SDL_SCANCODE_H, KMOD_NONE));
	EXPECT_TRUE(GameUsesKey(DefaultKeyBindings(Platform::Web), Platform::Web, SDL_SCANCODE_G, KMOD_NONE));
}

TEST(DeveloperKeys, APlayersBindingTakesADeveloperKey)
{
	auto bindings = DefaultKeyBindings(Platform::Desktop);
	EXPECT_FALSE(GameUsesKey(bindings, Platform::Desktop, platform_keys::EditorToggleKey(Platform::Desktop), KMOD_NONE));
	bindings[0].key = KeyChord {platform_keys::EditorToggleKey(Platform::Desktop)};
	EXPECT_TRUE(GameUsesKey(bindings, Platform::Desktop, platform_keys::EditorToggleKey(Platform::Desktop), KMOD_NONE));
	bindings[0].key = KeyChord {k_ConsoleKey, Modifier::Shift};
	EXPECT_TRUE(GameUsesKey(bindings, Platform::Desktop, k_ConsoleKey, KMOD_LSHIFT));
	EXPECT_FALSE(GameUsesKey(bindings, Platform::Desktop, k_ConsoleKey, KMOD_NONE));
}

TEST(DeveloperKeys, TheToggleKeysAreNotTheGamesKeys)
{
	for (const auto platform : k_Platforms)
	{
		EXPECT_FALSE(GameUsesKeyWithAnyModifier(platform, platform_keys::EditorToggleKey(platform)));
		EXPECT_FALSE(GameUsesKeyWithAnyModifier(platform, k_ConsoleKey));
		EXPECT_FALSE(GameUsesKeyWithAnyModifier(platform, RendererStatisticsScancode(platform)));
		EXPECT_NE(platform_keys::EditorToggleKey(platform), RendererStatisticsScancode(platform));
		EXPECT_NE(platform_keys::EditorToggleKey(platform), k_ConsoleKey);
	}
	EXPECT_EQ(platform_keys::EditorToggleKey(Platform::Desktop), SDL_SCANCODE_F10);
	// The browser uses F10 for its menu
	EXPECT_EQ(platform_keys::EditorToggleKey(Platform::Web), SDL_SCANCODE_END);
	EXPECT_EQ(platform_keys::EditorToggleKeyName(Platform::Desktop), "F10");
	EXPECT_EQ(platform_keys::EditorToggleKeyName(Platform::Web), "End");
}

TEST(DeveloperKeys, TheRendererStatisticsScancodeIsTheRendererStatisticsKey)
{
	EXPECT_EQ(platform_keys::RendererStatisticsKey(Platform::Desktop),
	          SDL_SCANCODE_TO_KEYCODE(RendererStatisticsScancode(Platform::Desktop)));
	EXPECT_EQ(platform_keys::RendererStatisticsKey(Platform::Web),
	          SDL_SCANCODE_TO_KEYCODE(RendererStatisticsScancode(Platform::Web)));
}

TEST(DeveloperKeys, NoEditorKeyIsAKeyOfTheGame)
{
	auto count = 0;
	for (auto code = 0; code < SDL_NUM_SCANCODES; ++code)
	{
		const auto key = static_cast<SDL_Scancode>(code);
		for (const auto mod : k_AllModifiers)
		{
			if (!EditorKeyActionFor(key, HeldModifiers(mod)).has_value())
			{
				continue;
			}
			++count;
			for (const auto platform : k_Platforms)
			{
				EXPECT_FALSE(GameUsesKey(DefaultKeyBindings(platform), platform, key, mod))
				    << "key " << code << " modifiers " << mod;
			}
		}
	}
	EXPECT_GT(count, 0);
}

TEST(DeveloperKeys, TheEditorKeysAndTheirActions)
{
	using Action = EditorKeyAction;
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_W, k_None), Action::SelectTool);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_E, k_None), Action::MoveTool);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_Y, k_None), Action::RotateTool);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_D, k_None), Action::HandTool);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_D, k_Ctrl), Action::Duplicate);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_LEFTBRACKET, k_Ctrl), Action::Undo);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_LEFTBRACKET, k_CtrlShift), Action::Redo);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_RIGHTBRACKET, k_Ctrl), Action::Redo);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_Y, k_Ctrl), Action::Redo);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_LEFTBRACKET, k_None), Action::TurnPlacementLeft);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_RIGHTBRACKET, k_None), Action::TurnPlacementRight);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_HOME, k_None), Action::Frame);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_O, k_None), Action::Orbit);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_O, k_Shift), Action::Follow);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_DELETE, k_None), Action::Remove);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_PERIOD, k_None), Action::StepTurn);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_EQUALS, k_Ctrl), Action::CameraFaster);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_MINUS, k_Ctrl), Action::CameraSlower);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_0, k_Ctrl), Action::CameraSpeedReset);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_X, k_None), Action::LetGo);
	EXPECT_EQ(EditorKeyActionFor(SDL_SCANCODE_EQUALS, k_None), std::nullopt);
	// The old keys that the game uses are not the editor's any more
	for (const auto key : {SDL_SCANCODE_R, SDL_SCANCODE_Z, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F2})
	{
		for (const auto modifiers : {k_None, k_Ctrl, k_CtrlShift})
		{
			EXPECT_EQ(EditorKeyActionFor(key, modifiers), std::nullopt) << key;
		}
	}
}
