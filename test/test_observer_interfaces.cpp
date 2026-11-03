/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Commands/PlayerCommands.h"
#include "ECS/Components/ObserverInterfaces.h"
#include "ECS/ObserverInterfaceRules.h"
#include "ECS/PlayerColour.h"

using namespace openblack;
using ecs::components::ObserverInterfaces;

namespace
{
/// The numbers of the observers whose cameras show a sparkle
std::vector<uint8_t> ShownNumbers(const ObserverInterfaces& interfaces, std::optional<uint8_t> own)
{
	std::vector<uint8_t> numbers;
	for (const auto& observer : observer_interfaces::ShownObservers(interfaces, own))
	{
		numbers.push_back(observer.number);
	}
	return numbers;
}
} // namespace

// The god of the observers takes the name of each observer that comes in, so it has the name of the last one. A leave
// does not change the name.
TEST(ObserverInterfaces, TheGodHasTheNameOfTheLastObserverThatCameIn)
{
	ObserverInterfaces interfaces;
	observer_interfaces::Come(interfaces, 1, u"carol");
	observer_interfaces::Come(interfaces, 2, u"dave");
	EXPECT_EQ(interfaces.name, u"dave");
	observer_interfaces::Go(interfaces, 2);
	EXPECT_EQ(interfaces.name, u"dave");
	ASSERT_EQ(interfaces.observers.size(), 1u);
	EXPECT_EQ(interfaces.observers[0].number, 1u);
	observer_interfaces::AllGone(interfaces);
	EXPECT_TRUE(interfaces.observers.empty());
	EXPECT_EQ(interfaces.name, u"dave");
}

// Each observer has its own camera. A report changes only the camera of its own observer.
TEST(ObserverInterfaces, EachObserverReportsItsOwnHandAndCamera)
{
	ObserverInterfaces interfaces;
	observer_interfaces::Come(interfaces, 1, u"carol");
	observer_interfaces::Come(interfaces, 2, u"dave");
	observer_interfaces::Report(interfaces, 2, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f});
	EXPECT_FALSE(interfaces.observers[0].reported);
	EXPECT_TRUE(interfaces.observers[1].reported);
	EXPECT_EQ(interfaces.observers[1].handPoint, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(interfaces.observers[1].camera, glm::vec3(4.0f, 5.0f, 6.0f));
	// A report of an observer that is not in the game changes nothing
	observer_interfaces::Report(interfaces, 7, {9.0f, 9.0f, 9.0f}, {9.0f, 9.0f, 9.0f});
	EXPECT_EQ(interfaces.observers.size(), 2u);
	// An observer that comes in again keeps its place and its camera
	observer_interfaces::Come(interfaces, 2, u"dave");
	EXPECT_EQ(interfaces.observers.size(), 2u);
	EXPECT_TRUE(interfaces.observers[1].reported);
}

// A computer shows a sparkle at the camera of each observer that reported it, except at the camera of its own observer
TEST(ObserverInterfaces, TheSparklesShowForEachOtherObserverThatReported)
{
	ObserverInterfaces interfaces;
	observer_interfaces::Come(interfaces, 3, u"carol");
	observer_interfaces::Come(interfaces, 1, u"dave");
	observer_interfaces::Come(interfaces, 2, u"erin");
	observer_interfaces::Report(interfaces, 3, {}, {1.0f, 0.0f, 0.0f});
	observer_interfaces::Report(interfaces, 1, {}, {2.0f, 0.0f, 0.0f});
	// The computer of a god shows each observer that reported, in the order that they came in
	EXPECT_EQ(ShownNumbers(interfaces, std::nullopt), (std::vector<uint8_t> {3, 1}));
	// The computer of an observer does not show its own observer
	EXPECT_EQ(ShownNumbers(interfaces, uint8_t {3}), (std::vector<uint8_t> {1}));
	EXPECT_EQ(ShownNumbers(interfaces, uint8_t {2}), (std::vector<uint8_t> {3, 1}));
}

// An observer sends only the report of its hand and camera and the lines of the chat
TEST(ObserverInterfaces, AnObserverSendsOnlyItsReportAndItsChat)
{
	using namespace player_commands;
	EXPECT_TRUE(ObserverSends(HandReport {}));
	EXPECT_TRUE(ObserverSends(ChatSpeech {}));
	EXPECT_TRUE(ObserverSends(ChatTaunt {}));
	EXPECT_FALSE(ObserverSends(HandTap {}));
	EXPECT_FALSE(ObserverSends(ViewReport {}));
	EXPECT_FALSE(ObserverSends(HandPick {}));
	EXPECT_FALSE(ObserverSends(ObserverIn {}));
	EXPECT_FALSE(ObserverSends(PlayerLeft {}));
}

// The god of the observers shows the orange of player seven, also when it is a god after the seventh (deviation 218)
TEST(ObserverInterfaces, TheGodOfTheObserversShowsTheColourOfPlayerSeven)
{
	const auto god8 = static_cast<PlayerNames>(8);
	const auto god33 = static_cast<PlayerNames>(33);
	EXPECT_EQ(player_colour::ShownAs(god8, god8), PlayerNames::PLAYER_SEVEN);
	EXPECT_EQ(player_colour::ShownAs(god33, god33), PlayerNames::PLAYER_SEVEN);
	EXPECT_EQ(player_colour::OnLand(player_colour::ShownAs(god8, god8), 0),
	          player_colour::OnLand(PlayerNames::PLAYER_SEVEN, 0));
	// Every other player shows its own colour, also when the game has no god of the observers
	EXPECT_EQ(player_colour::ShownAs(god8, god33), god8);
	EXPECT_EQ(player_colour::ShownAs(PlayerNames::PLAYER_TWO, std::nullopt), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(player_colour::ShownAs(PlayerNames::PLAYER_SEVEN, god8), PlayerNames::PLAYER_SEVEN);
}
