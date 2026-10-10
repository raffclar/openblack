/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <InspectorProvider.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::inspector
{

/// One input made as the player makes it: the pointer moved in the window, a mouse button or a key going down or up,
/// the wheel turned, an action of the options screen pressed, a gesture drawn, or the mouse handed back to the player
struct InputEvent
{
	enum class Kind : uint8_t
	{
		PointerTo,
		ButtonDown,
		ButtonUp,
		KeyDown,
		KeyUp,
		Wheel,
		Action,
		Gesture,
		Release,
	};
	Kind kind {Kind::PointerTo};
	/// Where the pointer goes, in the window's pixels from its top left
	glm::ivec2 position {0, 0};
	/// The mouse button: 1 left, 2 middle, 3 right
	uint8_t button {0};
	/// The key, by its name as SDL names keys ("L", "Space", "Left Shift")
	std::string key;
	/// Notches of the wheel, away from the player positive
	int32_t notches {0};
	/// The action as the options screen names it, or the gesture's name
	std::string name;
	/// A gesture drawn with the Action button held throughout, as a circle readying a miracle is
	bool holdAction {false};

	bool operator==(const InputEvent& other) const = default;
};

[[nodiscard]] std::string_view KindName(InputEvent::Kind kind);
[[nodiscard]] Json ToJson(const InputEvent& event);
/// An event from its JSON, as ToJson writes it; none, with why not, when it doesn't read
[[nodiscard]] std::optional<InputEvent> InputEventFromJson(const Json& json, std::string& error);

/// An event made at a frame, counted from the start of a recording
struct RecordedInput
{
	uint64_t frame {0};
	InputEvent event;

	bool operator==(const RecordedInput& other) const = default;
};

/// Where the inspector's input goes: the game's action layer, the way the player's mouse and keyboard reach it
class InputTargetInterface
{
public:
	virtual ~InputTargetInterface() = default;
	/// Makes the event now; empty when it was made, else why not
	virtual std::string Apply(const InputEvent& event) = 0;
	[[nodiscard]] virtual glm::ivec2 ScreenSize() const = 0;
	/// Where a point of the world is in the window, as the camera sees it now; none when it is off the screen
	[[nodiscard]] virtual std::optional<glm::ivec2> WorldToScreen(glm::vec3 point) const = 0;
	/// The land's height at a point across the ground
	[[nodiscard]] virtual float GroundHeight(glm::vec2 point) const = 0;
	/// Whether there is a key, an action or a gesture of that name
	[[nodiscard]] virtual bool HasKey(std::string_view name) const = 0;
	[[nodiscard]] virtual bool HasAction(std::string_view name) const = 0;
	[[nodiscard]] virtual bool HasGesture(std::string_view name) const = 0;
	/// The names of the options screen's actions and of the gestures
	[[nodiscard]] virtual std::vector<std::string> ActionNames() const = 0;
	[[nodiscard]] virtual std::vector<std::string> GestureNames() const = 0;
	/// The pointer, the buttons held, the hand and what the cursor picks, as the game sees them now
	[[nodiscard]] virtual Json State() const = 0;
	/// The lock on the player's own mouse and keyboard: "locked", "auto" (while a client is connected) or "unlocked"
	virtual void SetLockMode(std::string_view mode) = 0;
	/// Its mode and whether it keeps the player's input out now
	[[nodiscard]] virtual Json LockState() const = 0;
};

/// The input to make at coming frames, and the record of what was made while recording. Each frame, every event due by
/// then is made in the order given.
class InputTimeline
{
public:
	/// An event to make at a frame; ones at the same frame keep their order
	void Schedule(uint64_t frame, InputEvent event);
	/// Makes the events due by this frame. Why any failed, if any did
	std::vector<std::string> Frame(uint64_t frame, InputTargetInterface& target);
	/// Makes an event at once, as part of a frame; empty when it was made, else why not
	std::string MakeNow(uint64_t frame, const InputEvent& event, InputTargetInterface& target);
	/// Forgets the events still to come
	void Clear() { _pending.clear(); }
	[[nodiscard]] size_t Pending() const { return _pending.size(); }
	[[nodiscard]] std::optional<uint64_t> LastScheduled() const;

	/// Records each event made from this frame on, by its frame counted from here
	void StartRecording(uint64_t frame);
	/// What was recorded, and recording stops
	std::vector<RecordedInput> StopRecording();
	[[nodiscard]] bool Recording() const { return _recordingFrom.has_value(); }
	[[nodiscard]] size_t Recorded() const { return _recorded.size(); }

private:
	/// Writes down an event made, while recording
	void Made(uint64_t frame, const InputEvent& event);

	struct Scheduled
	{
		uint64_t frame;
		InputEvent event;
	};
	std::deque<Scheduled> _pending;
	std::optional<uint64_t> _recordingFrom;
	std::vector<RecordedInput> _recorded;
};

///   input.state                                   the pointer, buttons, hand and pick, and the input still to come
///   input.names                                   the actions and gestures that can be pressed and drawn
///   input.pointer {screen | world}                moves the pointer to a pixel, or to where a point of the land is
///   input.button  {button, action}                presses, lets go of or clicks a mouse button where the pointer is
///   input.key     {key | action, how}             presses, lets go of or taps a key, or presses an action for a frame
///   input.wheel   {notches}                       turns the wheel
///   input.drag    {to, button, frames}            holds a button and moves the pointer there over some frames
///   input.path    {points, button?, frames}       moves the pointer along pixels, a button held throughout or not
///   input.gesture {name, hold_action?}            draws a gesture through the recogniser, as the hand would
///   input.release                                 hands the mouse back to the player
///   input.lock    {mode?}                         keeps the player's mouse and keyboard out: always, or while connected
///   input.unlock                                  lets the player's mouse and keyboard in
///   input.record  {action: start | stop}          records the input made, by frame, for replaying
///   input.replay  {events}                        makes recorded input again from the next frame on
/// All of it is made at the frame's start, before the game reads its input, so it acts that same frame.
class InputProvider final: public ProviderInterface
{
public:
	/// The most frames a drag or a path may take, and the most points a path may have
	static constexpr uint32_t k_LongestFrames = 6000;
	static constexpr size_t k_MostPoints = 4096;

	explicit InputProvider(InputTargetInterface& target);

	[[nodiscard]] std::string_view Name() const override { return "input"; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override;
	[[nodiscard]] QueryResult Run(std::string_view query, const QueryContext& context) override;

	/// Once a frame, as the inspector serves its requests: makes the input due
	void Frame(uint64_t frame);

	[[nodiscard]] const InputTimeline& Timeline() const { return _timeline; }

private:
	/// Where a request's point is in the window: a pixel, or a point of the land as it is on the screen
	[[nodiscard]] std::optional<glm::ivec2> ScreenPoint(const Json& params, std::string_view key, std::string& error) const;
	/// Makes an event at once, recording it; the error if it couldn't be made
	std::string Now(const InputEvent& event);
	[[nodiscard]] Json Answer(Json made) const;

	InputTargetInterface& _target;
	InputTimeline _timeline;
	uint64_t _frame {0};
	/// The pointer as the inspector last put it, for drags that start from it
	std::optional<glm::ivec2> _pointer;
};

} // namespace openblack::inspector
