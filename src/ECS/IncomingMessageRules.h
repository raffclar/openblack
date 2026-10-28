/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <deque>
#include <string>
#include <string_view>

/// The incoming messages at the bottom left of the screen. Each message shows as a button with a picture and a line of
/// text to the right of the button. The game shows one message at a time, in the order that the messages come in.
namespace openblack::incoming_messages
{

/// What a message is about. Each kind has its own picture on the button.
enum class Kind : uint8_t
{
	/// A line of chat from another player
	Chat,
	/// The announcement of an e-mail
	Mail,
	/// The real weather of the player's town
	Weather,
};

/// How long the top message shows, in milliseconds of real time
constexpr uint32_t k_ShowMilliseconds = 10000;
/// The most characters of a message that the game keeps
constexpr size_t k_KeptLength = 1023;
/// The most characters of the top message that the line of text shows
constexpr size_t k_ShownLength = 255;
/// The number of weather pictures
constexpr uint8_t k_WeatherPictures = 9;
/// The time between two steps of the flash of a chat or e-mail button, in milliseconds
constexpr uint32_t k_FlashStepMilliseconds = 100;

/// One message in the list
struct Message
{
	Kind kind {Kind::Chat};
	/// Cut to k_KeptLength characters
	std::u16string text;
	/// The weather picture, 0 to k_WeatherPictures - 1. Only a weather message uses it.
	uint8_t picture {0};
	/// The real time at which the message came in, or at which it became the top message after the message before it
	/// went
	uint32_t since {0};
};

/// The picture on the button of the top message
enum class Look : uint8_t
{
	/// A speech bubble
	Speech,
	/// A speech bubble with an arrow
	SpeechArrow,
	/// An envelope
	Envelope,
	/// An envelope with an arrow
	EnvelopeArrow,
	/// One of the weather pictures
	Weather,
};

struct ButtonLook
{
	Look look {Look::Speech};
	/// The weather picture, for Look::Weather
	uint8_t picture {0};
	bool operator==(const ButtonLook&) const = default;
};

/// The picture of a message's button at a real time. A chat or an e-mail button flashes: it shows the picture with the
/// arrow for three steps of 100 milliseconds, then the picture without the arrow for one step. A weather button shows
/// its picture all the time.
[[nodiscard]] ButtonLook LookAt(const Message& message, uint32_t milliseconds) noexcept;

/// The messages in the order that they came in
class Queue
{
public:
	/// Adds a message at the end of the list. The time it came in is now.
	void Add(Kind kind, std::u16string_view text, uint8_t picture, uint32_t now);
	/// Removes the top message when it showed for more than k_ShowMilliseconds. The next message then starts to show
	/// now. Call this only while the button of the message shows. True when a message went.
	bool Expire(uint32_t now);
	/// Removes the top message. The next message then starts to show now.
	void Dismiss(uint32_t now);

	[[nodiscard]] bool Empty() const noexcept { return _messages.empty(); }
	[[nodiscard]] size_t Count() const noexcept { return _messages.size(); }
	/// The message that shows, none when the list is empty
	[[nodiscard]] const Message* Top() const noexcept { return _messages.empty() ? nullptr : &_messages.front(); }
	/// The line of text of the top message: its first k_ShownLength characters
	[[nodiscard]] std::u16string_view ShownText() const noexcept;

private:
	std::deque<Message> _messages;
};

/// The facts that decide whether a click on the button moves the camera to the player's creature
struct ClickFacts
{
	Kind kind {Kind::Chat};
	/// The click was with the right mouse button
	bool rightButton {false};
	/// The camera is the player's own camera, not a camera that follows a thing, a script or the temple
	bool playersCamera {false};
	/// The scripts let the camera move across the land
	bool cameraMoves {true};
	/// The scripts let the player use the camera keys and the keys that fly the camera to places
	bool cameraKeys {true};
	bool placeKeys {true};
	/// The player has a creature
	bool hasCreature {false};
};

/// Whether a click on the button of a chat or weather message starts to follow the player's creature with the camera.
/// An e-mail never does: it moves the camera to the villager of the e-mail.
[[nodiscard]] bool FollowsCreature(const ClickFacts& facts) noexcept;

/// The text of the weather message: the heading from the text table, a space, then the place
[[nodiscard]] std::u16string WeatherText(std::u16string_view heading, std::u16string_view place);

/// The number of the heading of the weather message in the text table ("Weather Information from:")
constexpr uint32_t k_WeatherHeadingText = 3569;

} // namespace openblack::incoming_messages
