/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <Inspector.h>
#include <Inspector/InputControl.h>
#include <gtest/gtest.h>

using namespace openblack::inspector;

namespace
{

/// A window of 800 by 600 over a flat land seen straight down, a pixel a unit, with a little world of its own: a
/// pointer, buttons and keys held, and a count of clicks where the pointer was, from which a state hash is made
class FakeInput final: public InputTargetInterface
{
public:
	std::string Apply(const InputEvent& event) override
	{
		made.push_back({.frame = frame, .event = event});
		switch (event.kind)
		{
		case InputEvent::Kind::PointerTo:
			pointer = event.position;
			break;
		case InputEvent::Kind::ButtonDown:
			buttons |= 1u << event.button;
			clicks += static_cast<uint64_t>(pointer.x) * 1000u + static_cast<uint64_t>(pointer.y);
			break;
		case InputEvent::Kind::ButtonUp:
			buttons &= ~(1u << event.button);
			break;
		default:
			break;
		}
		return {};
	}
	[[nodiscard]] glm::ivec2 ScreenSize() const override { return {800, 600}; }
	[[nodiscard]] std::optional<glm::ivec2> WorldToScreen(glm::vec3 point) const override
	{
		if (point.x < 0.0f || point.z < 0.0f || point.x >= 800.0f || point.z >= 600.0f)
		{
			return std::nullopt;
		}
		return glm::ivec2(static_cast<int>(point.x), static_cast<int>(point.z));
	}
	[[nodiscard]] float GroundHeight(glm::vec2 /*point*/) const override { return 0.0f; }
	[[nodiscard]] bool HasKey(std::string_view name) const override { return name == "L" || name == "Left Shift"; }
	[[nodiscard]] bool HasAction(std::string_view name) const override { return name == "Zoom In"; }
	[[nodiscard]] bool HasGesture(std::string_view name) const override { return name == "Spiral"; }
	[[nodiscard]] std::vector<std::string> ActionNames() const override { return {"Zoom In"}; }
	[[nodiscard]] std::vector<std::string> GestureNames() const override { return {"Spiral"}; }
	[[nodiscard]] Json State() const override { return {{"pointer", {pointer.x, pointer.y}}, {"buttons", buttons}}; }
	void SetLockMode(std::string_view mode) override { lockMode = mode; }
	[[nodiscard]] Json LockState() const override { return {{"mode", lockMode}}; }

	/// What the world looks like after the input: the same input gives the same hash
	[[nodiscard]] uint64_t Hash() const
	{
		return clicks * 31u + static_cast<uint64_t>(pointer.x) * 7u + static_cast<uint64_t>(pointer.y) + buttons;
	}

	/// The frame the input is made in
	uint64_t frame {1};
	std::vector<RecordedInput> made;
	glm::ivec2 pointer {0, 0};
	uint32_t buttons {0};
	uint64_t clicks {0};
	std::string lockMode {"auto"};
};

/// The inspector with the input provider, run frame by frame as the game serves it: requests, then the frame's input
struct Rig
{
	Rig()
	{
		auto owned = std::make_unique<InputProvider>(target);
		provider = owned.get();
		inspector.Add(std::move(owned));
	}

	Json Ask(const std::string& line)
	{
		const auto decoded = DecodeRequest(line);
		const auto answer = inspector.Answer(std::get<Request>(decoded));
		EXPECT_TRUE(answer.Ok()) << line << ": " << answer.error;
		return answer.value;
	}

	std::string Refused(const std::string& line)
	{
		const auto decoded = DecodeRequest(line);
		return inspector.Answer(std::get<Request>(decoded)).error;
	}

	/// The next frame starts: what was asked is made, then what is due
	void Frame()
	{
		++frameNumber;
		target.frame = frameNumber;
		provider->Frame(frameNumber);
		// What is asked from now on is made as the next frame starts
		target.frame = frameNumber + 1;
	}

	FakeInput target;
	Inspector inspector;
	InputProvider* provider {nullptr};
	uint64_t frameNumber {0};
};

} // namespace

TEST(InspectorInput, PointerGoesToAPixelOrToWhereAPointOfTheWorldIs)
{
	Rig rig;
	EXPECT_EQ(rig.Ask(R"({"query": "input.pointer", "params": {"screen": [100, 200]}})")["pointer"], Json({100, 200}));
	EXPECT_EQ(rig.target.pointer, glm::ivec2(100, 200));
	EXPECT_EQ(rig.Ask(R"({"query": "input.pointer", "params": {"world": [300, 50]}})")["pointer"], Json({300, 50}));

	EXPECT_NE(rig.Refused(R"({"query": "input.pointer", "params": {"world": [900, 50]}})").find("isn't on the screen"),
	          std::string::npos);
	EXPECT_FALSE(rig.Refused(R"({"query": "input.pointer", "params": {"screen": [800, 0]}})").empty());
	EXPECT_FALSE(rig.Refused(R"({"query": "input.pointer"})").empty());
}

TEST(InspectorInput, AClickIsPressedNowAndLetGoTheNextFrame)
{
	Rig rig;
	rig.Ask(R"({"query": "input.button", "params": {"button": "right"}})");
	ASSERT_EQ(rig.target.made.size(), 1u);
	EXPECT_EQ(rig.target.made[0].event.kind, InputEvent::Kind::ButtonDown);
	EXPECT_EQ(rig.target.made[0].event.button, 3);
	rig.Frame();
	EXPECT_EQ(rig.target.made.size(), 1u);
	rig.Frame();
	ASSERT_EQ(rig.target.made.size(), 2u);
	EXPECT_EQ(rig.target.made[1].event.kind, InputEvent::Kind::ButtonUp);
	EXPECT_FALSE(rig.Refused(R"({"query": "input.button", "params": {"button": "fourth"}})").empty());
}

// A double click is a click, then a second press and its letting go a frame apart, which count two clicks as the mouse
// tells a double click
TEST(InspectorInput, ADoubleClickPressesASecondTimeCountingTwoClicks)
{
	Rig rig;
	const auto answer = rig.Ask(R"({"query": "input.button", "params": {"button": "left", "action": "double_click"}})");
	EXPECT_EQ(answer["action"], "double_click");
	for (int i = 0; i < 4; ++i)
	{
		rig.Frame();
	}
	ASSERT_EQ(rig.target.made.size(), 4u);
	const std::vector<std::pair<InputEvent::Kind, uint8_t>> expected {
	    {InputEvent::Kind::ButtonDown, 1},
	    {InputEvent::Kind::ButtonUp, 1},
	    {InputEvent::Kind::ButtonDown, 2},
	    {InputEvent::Kind::ButtonUp, 2},
	};
	for (size_t i = 0; i < expected.size(); ++i)
	{
		EXPECT_EQ(rig.target.made[i].event.kind, expected[i].first) << i;
		EXPECT_EQ(rig.target.made[i].event.button, 1) << i;
		EXPECT_EQ(rig.target.made[i].event.clicks, expected[i].second) << i;
		if (i > 0)
		{
			EXPECT_EQ(rig.target.made[i].frame, rig.target.made[i - 1].frame + 1) << i;
		}
	}
	EXPECT_EQ(rig.target.buttons, 0u);
	EXPECT_NE(rig.Refused(R"({"query": "input.button", "params": {"action": "triple_click"}})").find("double_click"),
	          std::string::npos);
}

// The left button never picks things up, which the answer says; the right (Action) button does, with no note
TEST(InspectorInput, ALeftPressSaysTheHandTakesThingsWithTheRight)
{
	Rig rig;
	const auto left = rig.Ask(R"({"query": "input.button", "params": {"button": "left", "action": "press"}})");
	EXPECT_NE(left["note"].get<std::string>().find("right button"), std::string::npos);
	EXPECT_NE(rig.Ask(R"({"query": "input.button"})")["note"].get<std::string>().find("right button"), std::string::npos);
	EXPECT_FALSE(rig.Ask(R"({"query": "input.button", "params": {"button": "right", "action": "press"}})").contains("note"));
	EXPECT_FALSE(rig.Ask(R"({"query": "input.button", "params": {"button": "left", "action": "release"}})").contains("note"));
}

TEST(InspectorInput, KeysAndActionsByName)
{
	Rig rig;
	EXPECT_EQ(rig.Ask(R"({"query": "input.key", "params": {"key": "L"}})")["how"], "tap");
	rig.Ask(R"({"query": "input.key", "params": {"action": "Zoom In"}})");
	rig.Frame();
	rig.Frame();
	ASSERT_EQ(rig.target.made.size(), 3u);
	EXPECT_EQ(rig.target.made[0].event.kind, InputEvent::Kind::KeyDown);
	EXPECT_EQ(rig.target.made[1].event.kind, InputEvent::Kind::Action);
	EXPECT_EQ(rig.target.made[2].event.kind, InputEvent::Kind::KeyUp);

	EXPECT_FALSE(rig.Refused(R"({"query": "input.key", "params": {"key": "Nope"}})").empty());
	EXPECT_FALSE(rig.Refused(R"({"query": "input.key", "params": {"action": "Nope"}})").empty());
	EXPECT_FALSE(rig.Refused(R"({"query": "input.key", "params": {"key": "L", "action": "Zoom In"}})").empty());
	EXPECT_FALSE(rig.Refused(R"({"query": "input.gesture", "params": {"name": "Nope"}})").empty());
	EXPECT_EQ(rig.Ask(R"({"query": "input.names"})")["gestures"], Json({"Spiral"}));
}

TEST(InspectorInput, ADragHoldsTheButtonAndMovesEvenlyAFrameAStep)
{
	Rig rig;
	rig.Ask(R"({"query": "input.pointer", "params": {"screen": [0, 0]}})");
	const auto drag = rig.Ask(R"({"query": "input.drag", "params": {"to": {"screen": [40, 0]}, "frames": 4}})");
	EXPECT_EQ(drag["pending"], 5);
	EXPECT_EQ(rig.target.buttons, 1u << 1);
	std::vector<int> xs;
	for (int frame = 0; frame < 6; ++frame)
	{
		rig.Frame();
		xs.push_back(rig.target.pointer.x);
	}
	EXPECT_EQ(xs, (std::vector<int> {0, 10, 20, 30, 40, 40}));
	EXPECT_EQ(rig.target.buttons, 0u);
	EXPECT_EQ(rig.provider->Timeline().Pending(), 0u);
}

TEST(InspectorInput, ReleaseForgetsWhatIsStillToCome)
{
	Rig rig;
	rig.Ask(R"({"query": "input.path", "params": {"points": [[1, 1], [2, 2], [3, 3]], "button": "left"}})");
	EXPECT_GT(rig.provider->Timeline().Pending(), 0u);
	rig.Ask(R"({"query": "input.release"})");
	EXPECT_EQ(rig.provider->Timeline().Pending(), 0u);
	EXPECT_EQ(rig.target.made.back().event.kind, InputEvent::Kind::Release);
}

TEST(InspectorInput, EventsReadBackAsTheyAreWritten)
{
	const std::vector<InputEvent> events {
	    {.kind = InputEvent::Kind::PointerTo, .position = {3, 4}},
	    {.kind = InputEvent::Kind::ButtonUp, .button = 2},
	    {.kind = InputEvent::Kind::ButtonDown, .button = 1, .clicks = 2},
	    {.kind = InputEvent::Kind::KeyDown, .key = "Left Shift"},
	    {.kind = InputEvent::Kind::Wheel, .notches = -2},
	    {.kind = InputEvent::Kind::Action, .name = "Zoom In"},
	    {.kind = InputEvent::Kind::Gesture, .name = "Spiral", .holdAction = true},
	    {.kind = InputEvent::Kind::Release},
	};
	for (const auto& event : events)
	{
		std::string error;
		const auto read = InputEventFromJson(ToJson(event), error);
		ASSERT_TRUE(read.has_value()) << error;
		EXPECT_EQ(*read, event);
	}
	std::string error;
	EXPECT_FALSE(InputEventFromJson(Json {{"kind", "teleport"}}, error).has_value());
	EXPECT_FALSE(error.empty());
	// A single click's count isn't written, so recordings made before read back the same
	EXPECT_FALSE(ToJson({.kind = InputEvent::Kind::ButtonDown, .button = 1}).contains("clicks"));
	EXPECT_FALSE(InputEventFromJson(Json {{"kind", "button_down"}, {"button", 1}, {"clicks", 3}}, error).has_value());
}

// Deterministic replay: input recorded over frames and replayed from the same start is made at the same frames in the
// same order, and leaves the same state
TEST(InspectorInput, ReplayingARecordingGivesTheSameFramesAndState)
{
	Rig first;
	first.Frame();
	first.Ask(R"({"query": "input.record", "params": {"action": "start"}})");
	first.Ask(R"({"query": "input.pointer", "params": {"screen": [10, 10]}})");
	first.Ask(R"({"query": "input.button", "params": {"action": "click"}})");
	first.Frame();
	first.Frame();
	first.Ask(R"({"query": "input.drag", "params": {"to": {"screen": [50, 30]}, "frames": 3}})");
	first.Frame();
	first.Ask(R"({"query": "input.key", "params": {"key": "L"}})");
	for (int frame = 0; frame < 6; ++frame)
	{
		first.Frame();
	}
	const auto recording = first.Ask(R"({"query": "input.record", "params": {"action": "stop"}})");
	ASSERT_FALSE(recording["events"].empty());

	Rig second;
	second.Frame();
	second.Frame();
	second.Ask(Json {{"query", "input.replay"}, {"params", {{"events", recording["events"]}}}}.dump());
	for (int frame = 0; frame < 12; ++frame)
	{
		second.Frame();
	}

	// The same events, each the same number of frames after the start of the recording and of the replay
	const auto relative = [](const std::vector<RecordedInput>& made, uint64_t start) {
		std::vector<RecordedInput> shifted;
		for (const auto& each : made)
		{
			if (each.frame >= start)
			{
				shifted.push_back({.frame = each.frame - start, .event = each.event});
			}
		}
		return shifted;
	};
	EXPECT_EQ(relative(first.target.made, 2), relative(second.target.made, 3));
	EXPECT_EQ(first.target.Hash(), second.target.Hash());
	EXPECT_EQ(first.target.pointer, glm::ivec2(50, 30));
}

TEST(InspectorInput, ThePlayersDevicesAreKeptOutOrLetIn)
{
	Rig rig;
	EXPECT_EQ(rig.Ask(R"({"query": "input.lock"})")["input_lock"]["mode"], "locked");
	EXPECT_EQ(rig.Ask(R"({"query": "input.state"})")["input_lock"]["mode"], "locked");
	EXPECT_EQ(rig.Ask(R"({"query": "input.lock", "params": {"mode": "auto"}})")["input_lock"]["mode"], "auto");
	EXPECT_EQ(rig.Ask(R"({"query": "input.unlock"})")["input_lock"]["mode"], "unlocked");
	EXPECT_FALSE(rig.Refused(R"({"query": "input.lock", "params": {"mode": "sometimes"}})").empty());
}
