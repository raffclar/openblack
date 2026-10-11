/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Help/ControlIcons.h"
#include "Input/KeyBindings.h"

using namespace openblack::help::control_icons;
using openblack::input::BindableActionMap;
using openblack::input::KeyBindingTable;
using openblack::input::MouseInput;

namespace
{
/// Key names as a keyboard would give them
std::u16string FakeKeyName(SDL_Scancode key)
{
	switch (key)
	{
	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT:
		return u"Shift";
	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL:
		return u"Ctrl";
	case SDL_SCANCODE_LEFT:
		return u"Left";
	case SDL_SCANCODE_RIGHT:
		return u"Right";
	case SDL_SCANCODE_UP:
		return u"Up";
	case SDL_SCANCODE_DOWN:
		return u"Down";
	case SDL_SCANCODE_SPACE:
		return u"Space";
	case SDL_SCANCODE_S:
		return u"S";
	default:
		return u"?";
	}
}

const KeyNameFn k_KeyName = FakeKeyName;

/// The keys the default bindings give the four keyboard lessons' codes
std::optional<std::u16string> KeysOf(const std::vector<int>& codes, const KeyBindingTable& table)
{
	return KeyIconText(codes, table, k_KeyName);
}

KeyBindingTable Without(KeyBindingTable table, BindableActionMap action)
{
	for (auto& binding : table)
	{
		if (binding.action == action)
		{
			binding.key = std::nullopt;
			binding.mouse = MouseInput::None;
		}
	}
	return table;
}

constexpr const KeyBindingTable& k_Defaults = openblack::input::k_DefaultKeyBindings;
} // namespace

TEST(ControlIcons, TheKeyboardLessonsShowTheirKeys)
{
	// "You can also use the keyboard to rotate. $m15 $m7 $m8" and the pitch, move and zoom lessons
	EXPECT_EQ(KeysOf({15, 7, 8}, k_Defaults), u"Shift + Left / Right");
	EXPECT_EQ(KeysOf({15, 9, 10}, k_Defaults), u"Shift + Up / Down");
	EXPECT_EQ(KeysOf({7, 8, 9, 10}, k_Defaults), u"Left + Right / Up / Down");
	EXPECT_EQ(KeysOf({6, 9, 10}, k_Defaults), u"Ctrl + Up / Down");
}

TEST(ControlIcons, OnlyTheCameraKeysGoOnTheKeyCap)
{
	EXPECT_TRUE(IsCameraKey(6));
	EXPECT_TRUE(IsCameraKey(15));
	EXPECT_FALSE(IsCameraKey(5));
	EXPECT_FALSE(IsCameraKey(16));
	EXPECT_FALSE(KeysOf({1, 2, 17}, k_Defaults).has_value());
	EXPECT_FALSE(KeysOf({}, k_Defaults).has_value());
}

TEST(ControlIcons, AnUnboundKeyIsLeftOut)
{
	EXPECT_EQ(KeysOf({15, 7, 8}, Without(k_Defaults, BindableActionMap::MOVE_LEFT)), u"Shift + Right");
	// The first bound key makes the icon
	EXPECT_EQ(KeysOf({7, 15, 8}, Without(k_Defaults, BindableActionMap::MOVE_LEFT)), u"Shift + Right");
	EXPECT_FALSE(KeysOf({15}, Without(k_Defaults, BindableActionMap::ROTATE_ON)).has_value());
}

TEST(ControlIcons, TheKeyThatMadeTheIconStartsItsWordsAgain)
{
	EXPECT_EQ(KeysOf({7, 8, 7}, k_Defaults), u"Left");
	EXPECT_EQ(KeysOf({7, 8, 7, 9}, k_Defaults), u"Left + Up");
}

TEST(ControlIcons, OtherActionsShowTheirMouseButtons)
{
	const auto click = [](std::vector<int> codes, const KeyBindingTable& table = k_Defaults) {
		const auto picture = ActionIcon(codes, table, k_KeyName);
		return picture.has_value() && std::holds_alternative<MouseClick>(*picture)
		           ? std::optional(std::get<MouseClick>(*picture))
		           : std::nullopt;
	};
	EXPECT_EQ(click({1}), MouseClick::LeftButton);
	EXPECT_EQ(click({2}), MouseClick::RightButton);
	EXPECT_EQ(click({3}), MouseClick::WheelDown);
	EXPECT_EQ(click({4}), MouseClick::WheelUp);
	EXPECT_EQ(click({16}), MouseClick::MiddleButton);
	// The last bound one wins; the camera keys and unknown actions are passed over
	EXPECT_EQ(click({1, 2, 15, 7, 40, -1}), MouseClick::RightButton);
	EXPECT_FALSE(click({15, 7, 8}).has_value());
	// A zoom with no mouse binding takes the two buttons together, while move and action are on separate buttons
	EXPECT_EQ(click({4}, Without(k_Defaults, BindableActionMap::ZOOM_IN)), MouseClick::BothButtons);
	EXPECT_FALSE(click({4}, Without(Without(k_Defaults, BindableActionMap::ZOOM_IN), BindableActionMap::ACTION)).has_value());
	EXPECT_FALSE(click({5}).has_value()); // talk is on a key
}

TEST(ControlIcons, AnActionOnAKeyShowsItsKeyCap)
{
	const auto keys = [](std::vector<int> codes) {
		const auto picture = ActionIcon(codes, k_Defaults, k_KeyName);
		return picture.has_value() && std::holds_alternative<std::u16string>(*picture)
		           ? std::optional(std::get<std::u16string>(*picture))
		           : std::nullopt;
	};
	EXPECT_EQ(keys({17}), u"Space");    // zoom to the temple
	EXPECT_EQ(keys({29}), u"Ctrl + S"); // quick save, with its modifier
}

TEST(ControlIcons, TheMouseCells)
{
	// The left button: the right one's lit picture, mirrored, on the wheel mouse's row
	auto cell = CellFor(MouseClick::LeftButton, 2, 0);
	EXPECT_EQ(cell.uvMin, glm::vec2(0.5f, 0.5f));
	EXPECT_EQ(cell.uvMax, glm::vec2(0.25f, 0.75f));
	cell = CellFor(MouseClick::RightButton, 2, 0);
	EXPECT_EQ(cell.uvMin, glm::vec2(0.25f, 0.5f));
	cell = CellFor(MouseClick::BothButtons, 2, 0);
	EXPECT_EQ(cell.uvMin, glm::vec2(0.5f, 0.5f));
	EXPECT_EQ(cell.uvMax, glm::vec2(0.75f, 0.75f));
	cell = CellFor(MouseClick::MiddleButton, 1, 0);
	EXPECT_EQ(cell.uvMin, glm::vec2(0.75f, 0.25f));
	// The wheel turning up runs the bottom row's frames, then rests on the lit wheel
	EXPECT_EQ(CellFor(MouseClick::WheelUp, 2, 0).uvMin, glm::vec2(0.0f, 0.75f));
	EXPECT_EQ(CellFor(MouseClick::WheelUp, 2, 399).uvMin, glm::vec2(0.75f, 0.75f));
	EXPECT_EQ(CellFor(MouseClick::WheelUp, 2, 450).uvMin, glm::vec2(0.75f, 0.5f));
	// and turning down runs them backwards
	EXPECT_EQ(CellFor(MouseClick::WheelDown, 2, 0).uvMin, glm::vec2(0.75f, 0.75f));
	EXPECT_EQ(CellFor(MouseClick::WheelDown, 2, 100).uvMin, glm::vec2(0.5f, 0.75f));
}

TEST(ControlIcons, TheKeyCapSitsAtTheRightOfTheBoxsTopThird)
{
	// A box from row 610 to 701: icons 30 high
	EXPECT_EQ(IconSize(610, 701), 30);
	EXPECT_EQ(KeyIconTop(610, 701), 610);
	EXPECT_EQ(ActionIconTop(610, 701), 672);
	EXPECT_FLOAT_EQ(KeyTextSize(30), 12.0f);
	EXPECT_FLOAT_EQ(KeyCapWidth(30, 80.0f), 100.0f);
	EXPECT_FLOAT_EQ(KeyCapWidth(30, 5.0f), 30.0f); // never narrower than square

	const auto placed = Place(1280, 610, 30, 100.0f, 1.0f);
	ASSERT_TRUE(placed.has_value());
	EXPECT_EQ(placed->icon.min, glm::vec2(1176.0f, 610.0f));
	EXPECT_EQ(placed->icon.max, glm::vec2(1276.0f, 640.0f));
	EXPECT_FLOAT_EQ(placed->alpha, 1.0f);
	EXPECT_FLOAT_EQ(placed->glowColour.a, 128.0f / 255.0f);

	const auto cap = LayoutKeyCap(*placed);
	EXPECT_EQ(cap.slices[0], (Box {{1176.0f, 610.0f}, {1183.0f, 640.0f}}));
	EXPECT_EQ(cap.slices[1], (Box {{1183.0f, 610.0f}, {1269.0f, 640.0f}}));
	EXPECT_EQ(cap.slices[2], (Box {{1269.0f, 610.0f}, {1276.0f, 640.0f}}));
	EXPECT_FLOAT_EQ(cap.uvs[0].min.x, 128.5f / 256.0f);
	EXPECT_FLOAT_EQ(cap.uvs[2].max.x, 191.5f / 256.0f);
	EXPECT_FLOAT_EQ(cap.uvs[1].max.y, 63.5f / 256.0f);
	EXPECT_EQ(cap.textAt, glm::vec2(1182.0f, 614.0f));
	EXPECT_FLOAT_EQ(cap.textSize, 12.0f);
	EXPECT_EQ(cap.textColour, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

TEST(ControlIcons, APartPixelWidthIsCutDown)
{
	const auto placed = Place(1280, 610, 30, 100.5f, 1.0f);
	ASSERT_TRUE(placed.has_value());
	EXPECT_EQ(placed->icon.min.x, 1175.0f);
	EXPECT_EQ(placed->icon.max.x, 1275.0f);
}

TEST(ControlIcons, TheFade)
{
	EXPECT_FALSE(Place(1280, 610, 30, 30.0f, 0.01f).has_value());
	const auto half = Place(1280, 610, 30, 30.0f, 0.5f);
	ASSERT_TRUE(half.has_value());
	EXPECT_FLOAT_EQ(half->alpha, 127.0f / 255.0f);
	EXPECT_FLOAT_EQ(half->glowColour.a, 64.0f / 255.0f);
	EXPECT_EQ(half->glowColour.r, 1.0f);
}
