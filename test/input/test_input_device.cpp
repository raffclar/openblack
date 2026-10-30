/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>

#include <array>
#include <string>
#include <string_view>

#include <SDL_events.h>
#include <SDL_filesystem.h>
#include <SDL_keyboard.h>
#include <SDL_stdinc.h>
#include <gtest/gtest.h>

#include "Common/UserData.h"
#include "Input/NoInputDevice.h"

using namespace openblack;
using namespace openblack::input;

namespace
{

std::string Lower(std::string_view text)
{
	std::string lower(text);
	for (auto& c : lower)
	{
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return lower;
}

} // namespace

// The device with no input names each key as the window system does before its keyboard starts. No test starts the
// video of the window system, so the names of SDL here are the names before its keyboard starts.
TEST(InputDevice, defaultNamesAreTheNamesOfTheWindowSystem)
{
	for (int code = -1; code <= SDL_NUM_SCANCODES; ++code)
	{
		const auto scancode = static_cast<SDL_Scancode>(code);
		EXPECT_EQ(DefaultScancodeName(scancode), std::string_view(SDL_GetScancodeName(scancode))) << code;
	}
}

TEST(InputDevice, defaultScancodesOfNamesAreTheScancodesOfTheWindowSystem)
{
	for (int code = 0; code < SDL_NUM_SCANCODES; ++code)
	{
		const std::string name = SDL_GetScancodeName(static_cast<SDL_Scancode>(code));
		EXPECT_EQ(DefaultScancodeFromName(name), SDL_GetScancodeFromName(name.c_str())) << name;
		EXPECT_EQ(DefaultScancodeFromName(Lower(name)), SDL_GetScancodeFromName(Lower(name).c_str())) << name;
	}
	for (const std::string name : {"", "no such key", "Left Option", "keypad 1"})
	{
		EXPECT_EQ(DefaultScancodeFromName(name), SDL_GetScancodeFromName(name.c_str())) << name;
	}
}

TEST(InputDevice, noDeviceHasNoInput)
{
	NoInputDevice device;
	EXPECT_EQ(device.KeyboardFocus(), nullptr);
	EXPECT_EQ(device.MouseFocus(), nullptr);
	EXPECT_EQ(device.KeyboardState().size(), static_cast<size_t>(SDL_NUM_SCANCODES));
	for (const auto key : device.KeyboardState())
	{
		EXPECT_EQ(key, 0);
	}
	EXPECT_EQ(device.Mouse().buttons, 0u);
	EXPECT_EQ(device.Mouse().position, glm::ivec2(0, 0));
	EXPECT_EQ(device.KeyFromScancode(SDL_SCANCODE_A), SDLK_UNKNOWN);

	// The queue takes no events and gives none
	SDL_Event event {};
	event.type = SDL_KEYDOWN;
	device.PushEvent(event);
	EXPECT_LT(device.PeepEvents({}, SDL_PEEKEVENT), 0);
	std::array<SDL_Event, 2> events {};
	EXPECT_LT(device.PeepEvents(events, SDL_GETEVENT), 0);

	// The modifier keys stay as they were put, as in the window system
	EXPECT_EQ(device.ModState(), KMOD_NONE);
	device.SetModState(KMOD_LSHIFT);
	EXPECT_EQ(device.ModState(), KMOD_LSHIFT);

	// There is no relative mode to start, and to stop it does nothing
	EXPECT_FALSE(device.SetRelativePointer(true, false));
	EXPECT_TRUE(device.SetRelativePointer(false, false));
}

// The folder of the user's data is the folder of SDL for openblack
TEST(UserData, folderIsTheFolderOfTheWindowSystem)
{
	char* expected = SDL_GetPrefPath("openblack", "openblack");
	ASSERT_NE(expected, nullptr) << SDL_GetError();
	const std::string text(expected);
	SDL_free(expected);
	const auto folder = user_data::Folder();
	ASSERT_TRUE(folder.has_value());
	EXPECT_EQ(*folder, std::filesystem::path(std::u8string(text.begin(), text.end())));
	EXPECT_TRUE(folder->u8string() == std::u8string(text.begin(), text.end())) << text;
}
