/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InputControl.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <utility>

#include <InspectorQuery.h>
#include <glm/common.hpp>

using namespace openblack::inspector;

namespace
{

constexpr std::array<std::pair<InputEvent::Kind, std::string_view>, 9> k_KindNames {{
    {InputEvent::Kind::PointerTo, "pointer"},
    {InputEvent::Kind::ButtonDown, "button_down"},
    {InputEvent::Kind::ButtonUp, "button_up"},
    {InputEvent::Kind::KeyDown, "key_down"},
    {InputEvent::Kind::KeyUp, "key_up"},
    {InputEvent::Kind::Wheel, "wheel"},
    {InputEvent::Kind::Action, "action"},
    {InputEvent::Kind::Gesture, "gesture"},
    {InputEvent::Kind::Release, "release"},
}};

constexpr std::array<std::pair<std::string_view, uint8_t>, 3> k_Buttons {{
    {"left", 1},
    {"middle", 2},
    {"right", 3},
}};

std::optional<uint8_t> ButtonParam(const Json& params, std::string& error)
{
	const auto name = StringMember(params, "button").value_or("left");
	const auto found = std::find_if(k_Buttons.begin(), k_Buttons.end(), [&name](const auto& b) { return b.first == name; });
	if (found == k_Buttons.end())
	{
		error = "button is left, middle or right";
		return std::nullopt;
	}
	return found->second;
}

std::optional<uint32_t> FramesParam(const Json& params, uint32_t fallback, std::string& error)
{
	const auto value = NumberMember(params, "frames");
	if (!value.has_value())
	{
		return fallback;
	}
	if (*value < 1.0 || *value > InputProvider::k_LongestFrames || std::floor(*value) != *value)
	{
		error = "frames is a whole number from 1 to " + std::to_string(InputProvider::k_LongestFrames);
		return std::nullopt;
	}
	return static_cast<uint32_t>(*value);
}

QueryDescription Write(std::string name, std::string description, std::vector<ParameterDescription> parameters = {})
{
	return {.name = std::move(name),
	        .description = std::move(description),
	        .parameters = std::move(parameters),
	        .kind = ResultKind::Object,
	        .needsNear = false,
	        .writes = true};
}

ParameterDescription Optional(std::string name, std::string type, std::string description)
{
	return {.name = std::move(name), .type = std::move(type), .description = std::move(description), .required = false};
}

Json Point(glm::ivec2 point)
{
	return Json::array({point.x, point.y});
}

} // namespace

std::string_view openblack::inspector::KindName(InputEvent::Kind kind)
{
	const auto found = std::find_if(k_KindNames.begin(), k_KindNames.end(), [kind](const auto& k) { return k.first == kind; });
	return found != k_KindNames.end() ? found->second : "unknown";
}

Json openblack::inspector::ToJson(const InputEvent& event)
{
	Json json = {{"kind", KindName(event.kind)}};
	switch (event.kind)
	{
	case InputEvent::Kind::PointerTo:
		json["position"] = Point(event.position);
		break;
	case InputEvent::Kind::ButtonDown:
	case InputEvent::Kind::ButtonUp:
		json["button"] = event.button;
		break;
	case InputEvent::Kind::KeyDown:
	case InputEvent::Kind::KeyUp:
		json["key"] = event.key;
		break;
	case InputEvent::Kind::Wheel:
		json["notches"] = event.notches;
		break;
	case InputEvent::Kind::Action:
		json["name"] = event.name;
		break;
	case InputEvent::Kind::Gesture:
		json["name"] = event.name;
		json["hold_action"] = event.holdAction;
		break;
	case InputEvent::Kind::Release:
		break;
	}
	return json;
}

std::optional<InputEvent> openblack::inspector::InputEventFromJson(const Json& json, std::string& error)
{
	const auto kindName = StringMember(json, "kind");
	const auto kind = std::find_if(k_KindNames.begin(), k_KindNames.end(),
	                               [&kindName](const auto& k) { return kindName.has_value() && k.second == *kindName; });
	if (kind == k_KindNames.end())
	{
		error = "an event's kind is one of pointer, button_down, button_up, key_down, key_up, wheel, action, gesture, release";
		return std::nullopt;
	}
	InputEvent event {.kind = kind->first};
	switch (event.kind)
	{
	case InputEvent::Kind::PointerTo:
	{
		const auto it = json.find("position");
		if (it == json.end() || !it->is_array() || it->size() != 2 || !(*it)[0].is_number() || !(*it)[1].is_number())
		{
			error = "a pointer event needs a position [x, y] in pixels";
			return std::nullopt;
		}
		event.position = {static_cast<int>((*it)[0].get<double>()), static_cast<int>((*it)[1].get<double>())};
		break;
	}
	case InputEvent::Kind::ButtonDown:
	case InputEvent::Kind::ButtonUp:
	{
		const auto button = NumberMember(json, "button").value_or(0.0);
		if (button < 1.0 || button > 3.0)
		{
			error = "a button event's button is 1 (left), 2 (middle) or 3 (right)";
			return std::nullopt;
		}
		event.button = static_cast<uint8_t>(button);
		break;
	}
	case InputEvent::Kind::KeyDown:
	case InputEvent::Kind::KeyUp:
		event.key = StringMember(json, "key").value_or("");
		if (event.key.empty())
		{
			error = "a key event needs a key";
			return std::nullopt;
		}
		break;
	case InputEvent::Kind::Wheel:
		event.notches = static_cast<int32_t>(NumberMember(json, "notches").value_or(0.0));
		break;
	case InputEvent::Kind::Action:
	case InputEvent::Kind::Gesture:
		event.name = StringMember(json, "name").value_or("");
		if (const auto hold = json.find("hold_action"); hold != json.end() && hold->is_boolean())
		{
			event.holdAction = hold->get<bool>();
		}
		if (event.name.empty())
		{
			error = "an action or a gesture needs its name";
			return std::nullopt;
		}
		break;
	case InputEvent::Kind::Release:
		break;
	}
	return event;
}

void InputTimeline::Schedule(uint64_t frame, InputEvent event)
{
	// After every event due by then, so that those at the same frame keep their order
	const auto at = std::upper_bound(_pending.begin(), _pending.end(), frame,
	                                 [](uint64_t due, const Scheduled& scheduled) { return due < scheduled.frame; });
	_pending.insert(at, Scheduled {.frame = frame, .event = std::move(event)});
}

std::vector<std::string> InputTimeline::Frame(uint64_t frame, InputTargetInterface& target)
{
	std::vector<std::string> failures;
	while (!_pending.empty() && _pending.front().frame <= frame)
	{
		const auto scheduled = std::move(_pending.front());
		_pending.pop_front();
		if (auto failure = MakeNow(frame, scheduled.event, target); !failure.empty())
		{
			failures.push_back(std::move(failure));
		}
	}
	return failures;
}

std::string InputTimeline::MakeNow(uint64_t frame, const InputEvent& event, InputTargetInterface& target)
{
	auto failure = target.Apply(event);
	if (failure.empty())
	{
		Made(frame, event);
	}
	return failure;
}

std::optional<uint64_t> InputTimeline::LastScheduled() const
{
	return _pending.empty() ? std::nullopt : std::optional(_pending.back().frame);
}

void InputTimeline::StartRecording(uint64_t frame)
{
	_recordingFrom = frame;
	_recorded.clear();
}

std::vector<RecordedInput> InputTimeline::StopRecording()
{
	_recordingFrom.reset();
	return std::exchange(_recorded, {});
}

void InputTimeline::Made(uint64_t frame, const InputEvent& event)
{
	if (_recordingFrom.has_value() && frame >= *_recordingFrom)
	{
		_recorded.push_back({.frame = frame - *_recordingFrom, .event = event});
	}
}

InputProvider::InputProvider(InputTargetInterface& target)
    : _target(target)
{
}

std::vector<QueryDescription> InputProvider::Describe() const
{
	const ParameterDescription screen {
	    .name = "screen", .type = "array", .description = "A pixel of the window, [x, y] from its top left", .required = false};
	const ParameterDescription world {.name = "world",
	                                  .type = "point",
	                                  .description = "A point of the world, [x, z] on the land or [x, y, z]; it must be "
	                                                 "on the screen",
	                                  .required = false};
	const auto button = Optional("button", "string", "left (the default), middle or right");
	return {
	    {.name = "state",
	     .description = "The pointer, the buttons held, the hand, what the cursor picks, and the input still to come",
	     .parameters = {},
	     .kind = ResultKind::Object,
	     .needsNear = false},
	    {.name = "names",
	     .description = "The actions (by the options screen's names) and the gestures there are",
	     .parameters = {},
	     .kind = ResultKind::Object,
	     .needsNear = false},
	    Write("pointer",
	          "Moves the pointer, as the mouse would, to a pixel or to where a point of the world is on the "
	          "screen. The hand follows it this frame.",
	          {screen, world}),
	    Write("button",
	          "A mouse button where the pointer is: press, release, or click (pressed now, let go of the next "
	          "frame)",
	          {button, Optional("action", "string", "press, release or click (the default)")}),
	    Write("key",
	          "A key by its name (\"L\", \"Space\", \"Left Shift\"), or an action by the options screen's name, "
	          "pressed for a frame",
	          {Optional("key", "string", "The key's name, as SDL names it"),
	           Optional("action", "string", "The action's name, from input.names; pressed for one frame"),
	           Optional("how", "string", "For a key: press, release or tap (the default: down now, up the next frame)")}),
	    Write("wheel", "Turns the mouse wheel some notches, away from the player positive",
	          {{.name = "notches", .type = "integer", .description = "Notches", .required = true}}),
	    Write("drag",
	          "Holds a button where the pointer is, moves it evenly to a pixel or a point of the world over some "
	          "frames, then lets go",
	          {{.name = "to", .type = "point", .description = "{\"screen\": [x, y]} or {\"world\": [x, z]}", .required = true},
	           button,
	           Optional("frames", "integer", "Frames the move takes (30 by default)"),
	           Optional("from", "point", "Where it starts, as to; the pointer's place by default")}),
	    Write("path", "Moves the pointer through pixels, a frame or more each, with a button held throughout or none",
	          {{.name = "points", .type = "array", .description = "[[x, y], ...] in pixels", .required = true},
	           Optional("button", "string", "left, middle, right, or none (the default)"),
	           Optional("frames", "integer", "Frames for each point (1 by default)")}),
	    Write("gesture", "Draws a gesture through the recogniser as the hand would draw it, in the middle of the screen",
	          {{.name = "name", .type = "string", .description = "The gesture's name, from input.names", .required = true},
	           Optional("hold_action", "boolean", "The Action button held throughout, as a circle readying a miracle")}),
	    Write("release", "Lets go of every button and hands the mouse back to the player; forgets input still to come"),
	    Write("lock",
	          "Keeps the player's own mouse and keyboard out of the game, the debug windows and the camera: always "
	          "(the default), or with mode auto only while an inspector client is connected, as the game starts. "
	          "Ctrl+Alt+Shift+F12 on the keyboard always takes the game back",
	          {Optional("mode", "string", "locked (the default) or auto")}),
	    Write("unlock", "Lets the player's own mouse and keyboard into the game"),
	    Write("record", "Starts recording the input made, by frame, or stops and answers the recording",
	          {{.name = "action", .type = "string", .description = "start or stop", .required = true}}),
	    Write("replay", "Makes recorded input again, frame for frame, from this frame on",
	          {{.name = "events",
	            .type = "array",
	            .description = "The recording: [{\"frame\": n, \"event\": {...}}, ...] as input.record answers it",
	            .required = true}}),
	};
}

std::optional<glm::ivec2> InputProvider::ScreenPoint(const Json& params, std::string_view key, std::string& error) const
{
	const auto it = params.find(key);
	if (it == params.end())
	{
		return std::nullopt;
	}
	const auto& at = *it;
	if (const auto screen = at.find("screen"); at.is_object() && screen != at.end())
	{
		if (!screen->is_array() || screen->size() != 2 || !(*screen)[0].is_number() || !(*screen)[1].is_number())
		{
			error = "screen is a pixel [x, y]";
			return std::nullopt;
		}
		const glm::ivec2 pixel {static_cast<int>((*screen)[0].get<double>()), static_cast<int>((*screen)[1].get<double>())};
		const auto size = _target.ScreenSize();
		if (pixel.x < 0 || pixel.y < 0 || pixel.x >= size.x || pixel.y >= size.y)
		{
			error = "that pixel is outside the window, which is " + std::to_string(size.x) + " by " + std::to_string(size.y);
			return std::nullopt;
		}
		return pixel;
	}
	if (const auto worldPoint = at.find("world"); at.is_object() && worldPoint != at.end())
	{
		bool planar = false;
		const auto point = ReadPoint(*worldPoint, &planar);
		if (!point.has_value())
		{
			error = "world is a point, [x, z] or [x, y, z]";
			return std::nullopt;
		}
		glm::vec3 position {static_cast<float>((*point)[0]), static_cast<float>((*point)[1]), static_cast<float>((*point)[2])};
		if (planar)
		{
			position.y = _target.GroundHeight({position.x, position.z});
		}
		const auto pixel = _target.WorldToScreen(position);
		if (!pixel.has_value())
		{
			error = "that point isn't on the screen: move the camera to it first (camera.fly)";
		}
		return pixel;
	}
	error = "a point is {\"screen\": [x, y]} or {\"world\": [x, z] or [x, y, z]}";
	return std::nullopt;
}

std::string InputProvider::Now(const InputEvent& event)
{
	if (event.kind == InputEvent::Kind::PointerTo)
	{
		_pointer = event.position;
	}
	// Requests are answered as a frame starts, before its input is read: what is made now is part of that frame
	return _timeline.MakeNow(_frame + 1, event, _target);
}

Json InputProvider::Answer(Json made) const
{
	made["pending"] = _timeline.Pending();
	return made;
}

void InputProvider::Frame(uint64_t frame)
{
	_frame = frame;
	_timeline.Frame(frame, _target);
}

QueryResult InputProvider::Run(std::string_view query, const QueryContext& context)
{
	const auto& params = context.params;
	std::string error;
	const auto failed = [](std::string why) { return QueryResult::Error(std::move(why)); };

	if (query == "state")
	{
		auto state = _target.State();
		state["pending"] = _timeline.Pending();
		state["input_lock"] = _target.LockState();
		state["recording"] = _timeline.Recording() ? Json(_timeline.Recorded()) : Json(nullptr);
		return QueryResult::Value(std::move(state));
	}
	if (query == "names")
	{
		return QueryResult::Value({{"actions", _target.ActionNames()}, {"gestures", _target.GestureNames()}});
	}
	if (query == "pointer")
	{
		const auto at = params.contains("screen")  ? ScreenPoint(Json {{"at", {{"screen", params["screen"]}}}}, "at", error)
		                : params.contains("world") ? ScreenPoint(Json {{"at", {{"world", params["world"]}}}}, "at", error)
		                                           : std::nullopt;
		if (!at.has_value())
		{
			return failed(error.empty() ? "input.pointer needs screen [x, y] or world [x, z]" : error);
		}
		if (auto why = Now({.kind = InputEvent::Kind::PointerTo, .position = *at}); !why.empty())
		{
			return failed(why);
		}
		return QueryResult::Value(Answer({{"pointer", Point(*at)}}));
	}
	if (query == "button")
	{
		const auto button = ButtonParam(params, error);
		const auto action = StringMember(params, "action").value_or("click");
		if (!button.has_value() || (action != "press" && action != "release" && action != "click"))
		{
			return failed(error.empty() ? "action is press, release or click" : error);
		}
		const auto kind = action == "release" ? InputEvent::Kind::ButtonUp : InputEvent::Kind::ButtonDown;
		if (auto why = Now({.kind = kind, .button = *button}); !why.empty())
		{
			return failed(why);
		}
		if (action == "click")
		{
			_timeline.Schedule(_frame + 2, {.kind = InputEvent::Kind::ButtonUp, .button = *button});
		}
		return QueryResult::Value(Answer({{"button", *button}, {"action", action}}));
	}
	if (query == "key")
	{
		const auto key = StringMember(params, "key");
		const auto action = StringMember(params, "action");
		if (key.has_value() == action.has_value())
		{
			return failed("input.key needs either key or action");
		}
		if (action.has_value())
		{
			if (!_target.HasAction(*action))
			{
				return failed("no action " + *action + "; ask input.names");
			}
			if (auto why = Now({.kind = InputEvent::Kind::Action, .name = *action}); !why.empty())
			{
				return failed(why);
			}
			return QueryResult::Value(Answer({{"action", *action}}));
		}
		if (!_target.HasKey(*key))
		{
			return failed("no key " + *key + "; keys go by SDL's names, as \"L\", \"Space\" or \"Left Shift\"");
		}
		const auto how = StringMember(params, "how").value_or("tap");
		if (how != "press" && how != "release" && how != "tap")
		{
			return failed("how is press, release or tap");
		}
		const auto kind = how == "release" ? InputEvent::Kind::KeyUp : InputEvent::Kind::KeyDown;
		if (auto why = Now({.kind = kind, .key = *key}); !why.empty())
		{
			return failed(why);
		}
		if (how == "tap")
		{
			_timeline.Schedule(_frame + 2, {.kind = InputEvent::Kind::KeyUp, .key = *key});
		}
		return QueryResult::Value(Answer({{"key", *key}, {"how", how}}));
	}
	if (query == "wheel")
	{
		const auto notches = NumberMember(params, "notches");
		if (!notches.has_value() || *notches == 0.0 || std::abs(*notches) > 100.0 || std::floor(*notches) != *notches)
		{
			return failed("notches is a whole number, not 0, up to 100 either way");
		}
		if (auto why = Now({.kind = InputEvent::Kind::Wheel, .notches = static_cast<int32_t>(*notches)}); !why.empty())
		{
			return failed(why);
		}
		return QueryResult::Value(Answer({{"notches", *notches}}));
	}
	if (query == "drag")
	{
		const auto to = ScreenPoint(params, "to", error);
		const auto button = ButtonParam(params, error);
		const auto frames = FramesParam(params, 30, error);
		auto from = params.contains("from") ? ScreenPoint(params, "from", error) : _pointer;
		if (!from.has_value() && error.empty())
		{
			from = _target.ScreenSize() / 2;
		}
		if (!to.has_value() || !button.has_value() || !frames.has_value() || !from.has_value())
		{
			return failed(error.empty() ? "input.drag needs to: {screen} or {world}" : error);
		}
		// Down where it starts, then evenly to the end a step a frame, and let go there the frame after
		if (auto why = Now({.kind = InputEvent::Kind::PointerTo, .position = *from}); !why.empty())
		{
			return failed(why);
		}
		Now({.kind = InputEvent::Kind::ButtonDown, .button = *button});
		for (uint32_t step = 1; step <= *frames; ++step)
		{
			const auto t = static_cast<float>(step) / static_cast<float>(*frames);
			const auto at = glm::ivec2(glm::round(glm::mix(glm::vec2(*from), glm::vec2(*to), t)));
			_timeline.Schedule(_frame + 1 + step, {.kind = InputEvent::Kind::PointerTo, .position = at});
		}
		_timeline.Schedule(_frame + 2 + *frames, {.kind = InputEvent::Kind::ButtonUp, .button = *button});
		_pointer = to;
		return QueryResult::Value(Answer({{"from", Point(*from)}, {"to", Point(*to)}, {"frames", *frames}}));
	}
	if (query == "path")
	{
		const auto points = params.find("points");
		if (points == params.end() || !points->is_array() || points->empty() || points->size() > k_MostPoints)
		{
			return failed("points is a list of pixels [[x, y], ...], at most " + std::to_string(k_MostPoints));
		}
		std::vector<glm::ivec2> pixels;
		for (const auto& point : *points)
		{
			const auto pixel = ScreenPoint(Json {{"at", {{"screen", point}}}}, "at", error);
			if (!pixel.has_value())
			{
				return failed(error);
			}
			pixels.push_back(*pixel);
		}
		const auto frames = FramesParam(params, 1, error);
		const bool held = StringMember(params, "button").value_or("none") != "none";
		const auto button = held ? ButtonParam(params, error) : std::optional<uint8_t>(0);
		if (!frames.has_value() || !button.has_value() ||
		    static_cast<uint64_t>(*frames) * pixels.size() > static_cast<uint64_t>(k_LongestFrames))
		{
			return failed(error.empty() ? "the path would take too many frames" : error);
		}
		Now({.kind = InputEvent::Kind::PointerTo, .position = pixels.front()});
		if (held)
		{
			Now({.kind = InputEvent::Kind::ButtonDown, .button = *button});
		}
		uint64_t due = _frame + 1;
		for (size_t i = 1; i < pixels.size(); ++i)
		{
			due += *frames;
			_timeline.Schedule(due, {.kind = InputEvent::Kind::PointerTo, .position = pixels[i]});
		}
		if (held)
		{
			_timeline.Schedule(due + 1, {.kind = InputEvent::Kind::ButtonUp, .button = *button});
		}
		_pointer = pixels.back();
		return QueryResult::Value(Answer({{"points", pixels.size()}, {"frames", due + (held ? 1 : 0) - _frame}}));
	}
	if (query == "gesture")
	{
		const auto name = StringMember(params, "name").value_or("");
		if (!_target.HasGesture(name))
		{
			return failed("no gesture " + name + "; ask input.names");
		}
		const auto hold = params.find("hold_action");
		const bool holdAction = hold != params.end() && hold->is_boolean() && hold->get<bool>();
		if (auto why = Now({.kind = InputEvent::Kind::Gesture, .name = name, .holdAction = holdAction}); !why.empty())
		{
			return failed(why);
		}
		return QueryResult::Value(Answer({{"gesture", name}, {"hold_action", holdAction}}));
	}
	if (query == "release")
	{
		_timeline.Clear();
		Now({.kind = InputEvent::Kind::Release});
		_pointer.reset();
		return QueryResult::Value(Answer({{"released", true}}));
	}
	if (query == "lock" || query == "unlock")
	{
		const auto mode = query == "unlock" ? std::string("unlocked") : StringMember(params, "mode").value_or("locked");
		if (mode != "locked" && mode != "auto" && mode != "unlocked")
		{
			return failed("mode is locked or auto");
		}
		_target.SetLockMode(mode);
		return QueryResult::Value({{"input_lock", _target.LockState()}});
	}
	if (query == "record")
	{
		const auto action = StringMember(params, "action").value_or("");
		if (action == "start")
		{
			_timeline.StartRecording(_frame + 1);
			return QueryResult::Value({{"recording", true}, {"from_frame", _frame + 1}});
		}
		if (action == "stop")
		{
			Json events = Json::array();
			for (const auto& recorded : _timeline.StopRecording())
			{
				events.push_back({{"frame", recorded.frame}, {"event", ToJson(recorded.event)}});
			}
			return QueryResult::Value({{"recording", false}, {"events", std::move(events)}});
		}
		return failed("action is start or stop");
	}
	if (query == "replay")
	{
		const auto events = params.find("events");
		if (events == params.end() || !events->is_array())
		{
			return failed("events is a recording, as input.record answers it");
		}
		std::vector<RecordedInput> recording;
		for (const auto& each : *events)
		{
			const auto frame = NumberMember(each, "frame");
			const auto event =
			    each.is_object() && each.contains("event") ? InputEventFromJson(each["event"], error) : std::nullopt;
			if (!frame.has_value() || *frame < 0.0 || *frame > k_LongestFrames * 10.0 || !event.has_value())
			{
				return failed(error.empty() ? "each event is {\"frame\": n, \"event\": {...}}" : error);
			}
			recording.push_back({.frame = static_cast<uint64_t>(*frame), .event = *event});
		}
		for (auto& each : recording)
		{
			_timeline.Schedule(_frame + 1 + each.frame, std::move(each.event));
		}
		return QueryResult::Value(Answer({{"scheduled", recording.size()}, {"from_frame", _frame + 1}}));
	}
	return failed("no query input." + std::string(query));
}
