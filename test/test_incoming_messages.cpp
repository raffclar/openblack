/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "ECS/IncomingMessageRules.h"
#include "Gui/IncomingMessageStrip.h"
#include "Gui/IncomingMessageStripRules.h"

// Enable this define because the test makes the system itself
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/IncomingMessageSystem.h"

using namespace openblack;
using namespace openblack::incoming_messages;
namespace strip = openblack::gui::incoming_message_strip;

TEST(IncomingMessages, MessagesShowInTheOrderThatTheyCameIn)
{
	Queue queue;
	EXPECT_EQ(queue.Top(), nullptr);
	EXPECT_TRUE(queue.ShownText().empty());
	queue.Add(Kind::Chat, u"first", 0, 100);
	queue.Add(Kind::Weather, u"second", 3, 200);
	ASSERT_NE(queue.Top(), nullptr);
	EXPECT_EQ(queue.Count(), 2U);
	EXPECT_EQ(queue.ShownText(), u"first");
	queue.Dismiss(300);
	EXPECT_EQ(queue.ShownText(), u"second");
	EXPECT_EQ(queue.Top()->picture, 3);
	// The next message starts to show when the message before it goes
	EXPECT_EQ(queue.Top()->since, 300U);
	queue.Dismiss(400);
	EXPECT_TRUE(queue.Empty());
	queue.Dismiss(500);
	EXPECT_TRUE(queue.Empty());
}

TEST(IncomingMessages, TheTopMessageGoesAfterMoreThanTenSeconds)
{
	Queue queue;
	queue.Add(Kind::Weather, u"weather", 0, 1000);
	queue.Add(Kind::Chat, u"chat", 0, 2000);
	EXPECT_FALSE(queue.Expire(11000));
	EXPECT_TRUE(queue.Expire(11001));
	EXPECT_EQ(queue.ShownText(), u"chat");
	// The time of the second message counts from when the first went, not from when it came in
	EXPECT_FALSE(queue.Expire(21001));
	EXPECT_TRUE(queue.Expire(21002));
	EXPECT_TRUE(queue.Empty());
	EXPECT_FALSE(queue.Expire(50000));
}

TEST(IncomingMessages, TheTimeGoesRoundAsTheMachinesCountDoes)
{
	Queue queue;
	queue.Add(Kind::Chat, u"chat", 0, 0xFFFFFF00U);
	EXPECT_FALSE(queue.Expire(100));
	EXPECT_TRUE(queue.Expire(0xFFFFFF00U + k_ShowMilliseconds + 1));
}

TEST(IncomingMessages, LongTextIsCut)
{
	Queue queue;
	queue.Add(Kind::Chat, std::u16string(2000, u'a'), 0, 0);
	EXPECT_EQ(queue.Top()->text.size(), k_KeptLength);
	EXPECT_EQ(queue.ShownText().size(), k_ShownLength);
}

TEST(IncomingMessages, ChatAndMailButtonsFlashAndWeatherButtonsDoNot)
{
	const Message chat {.kind = Kind::Chat};
	const Message mail {.kind = Kind::Mail};
	const Message weather {.kind = Kind::Weather, .picture = 6};
	EXPECT_EQ(LookAt(chat, 0).look, Look::Speech);
	EXPECT_EQ(LookAt(chat, 99).look, Look::Speech);
	EXPECT_EQ(LookAt(chat, 100).look, Look::SpeechArrow);
	EXPECT_EQ(LookAt(chat, 399).look, Look::SpeechArrow);
	EXPECT_EQ(LookAt(chat, 400).look, Look::Speech);
	EXPECT_EQ(LookAt(mail, 50).look, Look::Envelope);
	EXPECT_EQ(LookAt(mail, 250).look, Look::EnvelopeArrow);
	for (const uint32_t time : {0U, 150U, 250U, 350U})
	{
		EXPECT_EQ(LookAt(weather, time), (ButtonLook {.look = Look::Weather, .picture = 6}));
	}
}

TEST(IncomingMessages, AClickFollowsTheCreatureOnlyWhenTheCameraCanMove)
{
	ClickFacts facts {.kind = Kind::Weather, .playersCamera = true, .hasCreature = true};
	EXPECT_TRUE(FollowsCreature(facts));
	facts.kind = Kind::Chat;
	EXPECT_TRUE(FollowsCreature(facts));
	auto changed = facts;
	changed.kind = Kind::Mail;
	EXPECT_FALSE(FollowsCreature(changed));
	changed = facts;
	changed.rightButton = true;
	EXPECT_FALSE(FollowsCreature(changed));
	changed = facts;
	changed.playersCamera = false;
	EXPECT_FALSE(FollowsCreature(changed));
	changed = facts;
	changed.cameraMoves = false;
	EXPECT_FALSE(FollowsCreature(changed));
	changed = facts;
	changed.cameraKeys = false;
	EXPECT_FALSE(FollowsCreature(changed));
	changed = facts;
	changed.placeKeys = false;
	EXPECT_FALSE(FollowsCreature(changed));
	changed = facts;
	changed.hasCreature = false;
	EXPECT_FALSE(FollowsCreature(changed));
}

TEST(IncomingMessages, TheWeatherTextIsTheHeadingASpaceAndThePlace)
{
	EXPECT_EQ(WeatherText(u"Weather Information from:", u"Guildford (Surrey,Great Britain)"),
	          u"Weather Information from: Guildford (Surrey,Great Britain)");
	EXPECT_EQ(WeatherText(u"", u"Oslo (Norway)"), u" Oslo (Norway)");
}

TEST(IncomingMessageSystem, TheListGoesOnlyWhileTheButtonShows)
{
	uint32_t now = 1000;
	ecs::systems::IncomingMessageSystem system([&now]() { return now; });
	system.AddWeather(u"Oslo (Norway)", 2);
	ASSERT_NE(system.GetQueue().Top(), nullptr);
	// Without a text table the heading is empty
	EXPECT_EQ(system.GetQueue().ShownText(), u" Oslo (Norway)");
	EXPECT_EQ(system.GetQueue().Top()->kind, Kind::Weather);
	now = 20000;
	system.Update(false);
	EXPECT_FALSE(system.GetQueue().Empty());
	// The time counted while the button did not show, so the message goes at once when it shows
	system.Update(true);
	EXPECT_TRUE(system.GetQueue().Empty());

	system.Add(Kind::Chat, u"name: text", 0);
	system.Dismiss();
	EXPECT_TRUE(system.GetQueue().Empty());
}

TEST(IncomingMessageStrip, TheLayoutIsTheGamesOnTheDefaultScreen)
{
	EXPECT_EQ(strip::Origin({800, 600}), glm::vec2(0.0f));
	EXPECT_EQ(strip::LayoutWidth({800, 600}), 800);
	EXPECT_EQ(strip::ButtonRect().min, glm::ivec2(0, 552));
	EXPECT_EQ(strip::ButtonRect().max, glm::ivec2(48, 600));
	EXPECT_EQ(strip::TextRect(800).min, glm::ivec2(48, 566));
	EXPECT_EQ(strip::TextRect(800).max, glm::ivec2(795, 586));
}

TEST(IncomingMessageStrip, TheLayoutStaysInTheBottomLeftCornerOfAWideScreen)
{
	// The layout is scaled as the front end is, 1440 / 600 = 2.4 times
	const glm::u16vec2 screen {3440, 1440};
	const auto origin = strip::Origin(screen);
	EXPECT_EQ(strip::LayoutWidth(screen), 1433);
	EXPECT_FLOAT_EQ(origin.y, 0.0f);
	// The layout's left edge is on the screen's left edge
	EXPECT_NEAR(origin.x + ((3440.0f - (800.0f * 2.4f)) / 2.0f), 0.0f, 1.0f);
}

TEST(IncomingMessageStrip, ThePicturesComeFromTheirCells)
{
	using strip::Sheet;
	// The first weather picture is the first cell of the second row of the bubbles texture
	const auto sunny = strip::PictureOf({.look = Look::Weather, .picture = 0}, false);
	EXPECT_EQ(sunny.sheet, Sheet::Bubbles);
	EXPECT_FLOAT_EQ(sunny.uvMin.x, 0.00390625f + 0.001953125f);
	EXPECT_FLOAT_EQ(sunny.uvMin.y, 0.25f + 0.00390625f + 0.001953125f);
	EXPECT_FLOAT_EQ(sunny.uvMax.x - sunny.uvMin.x, 0.2421875f - 0.00390625f);
	const auto last = strip::PictureOf({.look = Look::Weather, .picture = 8}, true);
	EXPECT_FLOAT_EQ(last.uvMin.x, 0.00390625f + 0.001953125f);
	EXPECT_FLOAT_EQ(last.uvMin.y, 0.75f + 0.00390625f + 0.001953125f);
	const auto fifth = strip::PictureOf({.look = Look::Weather, .picture = 5}, false);
	EXPECT_FLOAT_EQ(fifth.uvMin.x, 0.25f + 0.00390625f + 0.001953125f);
	EXPECT_FLOAT_EQ(fifth.uvMin.y, 0.5f + 0.00390625f + 0.001953125f);

	// The speech bubble lights up under the pointer, the envelope does not
	const auto speech = strip::PictureOf({.look = Look::Speech}, false);
	EXPECT_EQ(speech.sheet, Sheet::Buttons);
	EXPECT_FLOAT_EQ(speech.uvMin.x, 0.125f + 0.001953125f);
	EXPECT_FLOAT_EQ(speech.uvMin.y, 0.25f + 0.001953125f);
	EXPECT_FLOAT_EQ(strip::PictureOf({.look = Look::Speech}, true).uvMin.x, 0.5f + 0.001953125f);
	EXPECT_FLOAT_EQ(strip::PictureOf({.look = Look::SpeechArrow}, false).uvMin.y, 0.4375f + 0.001953125f);
	EXPECT_EQ(strip::PictureOf({.look = Look::Envelope}, false), strip::PictureOf({.look = Look::Envelope}, true));
	EXPECT_FLOAT_EQ(strip::PictureOf({.look = Look::EnvelopeArrow}, false).uvMin.y, 0.375f + 0.001953125f);
	EXPECT_FLOAT_EQ(strip::PictureOf({.look = Look::Envelope}, false).uvMax.x, 0.875f + 0.001953125f + 0.12109375f);
}

TEST(IncomingMessageStrip, AClickNeedsTheButtonToGoDownAndUpOverIt)
{
	gui::IncomingMessageStrip messages;
	const ButtonLook weather {.look = Look::Weather};
	messages.Update(weather, u"Weather", 800);
	EXPECT_TRUE(messages.IsActive());
	EXPECT_EQ(messages.GetText(), u"Weather");
	// The line of text takes no clicks
	messages.MouseMove({400, 570});
	EXPECT_FALSE(messages.WantsMouse(false));
	messages.MouseMove({10, 580});
	EXPECT_TRUE(messages.WantsMouse(false));
	EXPECT_FALSE(messages.WantsMouse(true));

	messages.MouseDown({10, 580}, true);
	EXPECT_TRUE(messages.WantsMouse(true));
	const auto click = messages.MouseUp({20, 590});
	ASSERT_TRUE(click.has_value());
	EXPECT_TRUE(click->rightButton);

	// Let go away from the button, nothing happens
	messages.MouseDown({10, 580}, false);
	EXPECT_FALSE(messages.MouseUp({300, 300}).has_value());

	// With no message the text stays as it was and nothing shows
	messages.Update(weather, std::nullopt, 800);
	EXPECT_EQ(messages.GetText(), u"Weather");
	messages.Update(std::nullopt, std::nullopt, 800);
	EXPECT_FALSE(messages.IsActive());
	EXPECT_FALSE(messages.WantsMouse(false));
}
