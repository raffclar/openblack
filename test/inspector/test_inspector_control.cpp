/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <Inspector.h>
#include <Inspector/CameraControl.h>
#include <Inspector/GuiControl.h>
#include <Inspector/LevelControl.h>
#include <Inspector/ScriptControl.h>
#include <gtest/gtest.h>

using namespace openblack::inspector;

namespace
{

Json Ask(Inspector& inspector, const std::string& line)
{
	const auto decoded = DecodeRequest(line);
	const auto answer = inspector.Answer(std::get<Request>(decoded));
	EXPECT_TRUE(answer.Ok()) << line << ": " << answer.error;
	return answer.value;
}

std::string Refused(Inspector& inspector, const std::string& line)
{
	const auto decoded = DecodeRequest(line);
	return inspector.Answer(std::get<Request>(decoded)).error;
}

/// A camera on flat land at height 10, which a path may hold
class FakeCamera final: public CameraControlInterface
{
public:
	[[nodiscard]] std::optional<CameraState> State() const override
	{
		return CameraState {.origin = pose.origin, .focus = pose.focus, .model = "world", .heldByPath = held};
	}
	std::string Set(const CameraPose& to) override
	{
		if (held)
		{
			return "held";
		}
		pose = to;
		++sets;
		return {};
	}
	std::string Fly(const CameraPose& to) override
	{
		flewTo = to;
		return {};
	}
	[[nodiscard]] float GroundHeight(glm::vec2 /*point*/) const override { return 10.0f; }

	CameraPose pose {.origin = {0.0f, 100.0f, -100.0f}, .focus = {0.0f, 0.0f, 0.0f}};
	std::optional<CameraPose> flewTo;
	bool held {false};
	int sets {0};
};

void ExpectNear(const Json& point, glm::vec3 expected)
{
	ASSERT_TRUE(point.is_array());
	EXPECT_NEAR(point[0].get<double>(), expected.x, 1e-3);
	EXPECT_NEAR(point[1].get<double>(), expected.y, 1e-3);
	EXPECT_NEAR(point[2].get<double>(), expected.z, 1e-3);
}

} // namespace

TEST(InspectorCamera, StateGivesItsAnglesAndDistance)
{
	FakeCamera camera;
	Inspector inspector;
	inspector.Add(MakeCameraProvider(camera));
	const auto state = Ask(inspector, R"({"query": "camera.state"})");
	EXPECT_NEAR(state["yaw"].get<double>(), 0.0, 1e-4);
	EXPECT_NEAR(state["pitch"].get<double>(), 45.0, 1e-4);
	EXPECT_NEAR(state["distance"].get<double>(), std::sqrt(20000.0), 1e-3);
	EXPECT_EQ(state["model"], "world");
}

TEST(InspectorCamera, SetKeepsWhatIsntGiven)
{
	FakeCamera camera;
	Inspector inspector;
	inspector.Add(MakeCameraProvider(camera));

	// A new focus on the land keeps the angles and distance
	Ask(inspector, R"({"query": "camera.set", "params": {"focus": [50, 20]}})");
	ExpectNear(Json::array({camera.pose.focus.x, camera.pose.focus.y, camera.pose.focus.z}), {50.0f, 10.0f, 20.0f});
	ExpectNear(Json::array({camera.pose.origin.x, camera.pose.origin.y, camera.pose.origin.z}), {50.0f, 110.0f, -80.0f});

	// Turning about the focus: looking along +x from 30 away, level
	const auto turned = Ask(inspector, R"({"query": "camera.set", "params": {"yaw": 90, "pitch": 0, "distance": 30}})");
	ExpectNear(turned["set_to"]["origin"], {20.0f, 10.0f, 20.0f});
	EXPECT_NEAR(turned["set_to"]["yaw"].get<double>(), 90.0, 1e-3);

	// A position alone keeps the way it looks
	Ask(inspector, R"({"query": "camera.set", "params": {"position": [0, 50, 0]}})");
	ExpectNear(Json::array({camera.pose.focus.x, camera.pose.focus.y, camera.pose.focus.z}), {30.0f, 50.0f, 0.0f});

	// Both
	Ask(inspector, R"({"query": "camera.set", "params": {"position": [1, 2, 3], "focus": [4, 5, 6]}})");
	ExpectNear(Json::array({camera.pose.origin.x, camera.pose.origin.y, camera.pose.origin.z}), {1.0f, 2.0f, 3.0f});

	EXPECT_FALSE(Refused(inspector, R"({"query": "camera.set", "params": {}})").empty());
	EXPECT_FALSE(Refused(inspector, R"({"query": "camera.set", "params": {"position": [0, 0, 0], "yaw": 3}})").empty());
	EXPECT_FALSE(Refused(inspector, R"({"query": "camera.set", "params": {"pitch": 95}})").empty());
	EXPECT_FALSE(Refused(inspector, R"({"query": "camera.set", "params": {"position": [1, 2]}})").empty());
}

TEST(InspectorCamera, FlyGoesThroughTheFlightAndAPathRefusesASet)
{
	FakeCamera camera;
	Inspector inspector;
	inspector.Add(MakeCameraProvider(camera));
	const auto flying = Ask(inspector, R"({"query": "camera.fly", "params": {"focus": [100, 0, 100], "distance": 50}})");
	ASSERT_TRUE(camera.flewTo.has_value());
	ExpectNear(flying["flying_to"]["focus"], {100.0f, 0.0f, 100.0f});
	EXPECT_EQ(camera.sets, 0);

	camera.held = true;
	EXPECT_EQ(Refused(inspector, R"({"query": "camera.set", "params": {"yaw": 10}})"), "held");
}

TEST(InspectorCamera, AnglesRoundTrip)
{
	const CameraPose pose {.origin = {3.0f, 40.0f, -7.0f}, .focus = {20.0f, 5.0f, 9.0f}};
	const auto angles = AnglesOf(pose);
	const auto origin = OriginFor(pose.focus, angles);
	EXPECT_NEAR(origin.x, pose.origin.x, 1e-3);
	EXPECT_NEAR(origin.y, pose.origin.y, 1e-3);
	EXPECT_NEAR(origin.z, pose.origin.z, 1e-3);
}

namespace
{

class FakeGui final: public GuiTargetInterface
{
public:
	[[nodiscard]] std::vector<WindowInfo> Windows() const override
	{
		return {{.name = "Moon", .kind = "debug", .open = moonOpen, .page = {}, .buttons = {}},
		        {.name = "menu", .kind = "game", .open = false, .page = "main", .buttons = {"Continue"}}};
	}
	std::string Open(std::string_view window) override
	{
		if (window != "Moon")
		{
			return "no window";
		}
		moonOpen = true;
		return {};
	}
	std::string Close(std::string_view /*window*/) override
	{
		moonOpen = false;
		return {};
	}
	std::string Press(std::string_view window, const std::vector<ButtonPathStep>& path) override
	{
		pressed = {std::string(window), path};
		return moonOpen ? std::string {} : "not open";
	}

	bool moonOpen {false};
	std::pair<std::string, std::vector<ButtonPathStep>> pressed;
};

} // namespace

TEST(InspectorGui, WindowsOpenCloseAndPressByName)
{
	FakeGui gui;
	Inspector inspector;
	inspector.Add(MakeGuiProvider(gui));
	EXPECT_EQ(Ask(inspector, R"({"query": "gui.windows"})")["total"], 2);
	EXPECT_EQ(Ask(inspector, R"({"query": "gui.open", "params": {"window": "Moon"}})")["open"], true);
	EXPECT_FALSE(Refused(inspector, R"({"query": "gui.open", "params": {"window": "Nope"}})").empty());

	Ask(inspector, R"({"query": "gui.press", "params": {"window": "Moon", "button": "Remove", "path": ["Creatures", 3]}})");
	EXPECT_EQ(gui.pressed.first, "Moon");
	const std::vector<ButtonPathStep> expected {std::string("Creatures"), 3, std::string("Remove")};
	EXPECT_EQ(gui.pressed.second, expected);

	EXPECT_EQ(Ask(inspector, R"({"query": "gui.close", "params": {"window": "Moon"}})")["open"], false);
	EXPECT_FALSE(Refused(inspector, R"({"query": "gui.press", "params": {"window": "Moon", "button": "Go"}})").empty());
	EXPECT_FALSE(Refused(inspector, R"({"query": "gui.press", "params": {"window": "Moon"}})").empty());
}

namespace
{

/// A script machine with two globals, a script and two natives: one that adds its two floats, one not written
class FakeScripts final: public ScriptTargetInterface
{
public:
	[[nodiscard]] bool Loaded() const override { return true; }
	[[nodiscard]] std::vector<ScriptInfo> Scripts() const override
	{
		return {{.name = "Land1Intro", .file = "Land1.txt", .type = "script", .parameters = 0}};
	}
	std::variant<uint32_t, std::string> Run(std::string_view name) override
	{
		if (name != "Land1Intro")
		{
			return std::string("no script");
		}
		return uint32_t {7};
	}
	std::string StopTask(uint32_t task) override { return task == 7 ? std::string {} : "no task"; }
	[[nodiscard]] std::vector<ScriptGlobal> Globals() const override
	{
		return {{.name = "Land1Stage", .value = stage}, {.name = "HasCreature", .value = hasCreature}};
	}
	std::string SetGlobal(std::string_view name, const ScriptValue& value) override
	{
		(name == "Land1Stage" ? stage : hasCreature) = value;
		return {};
	}
	[[nodiscard]] std::vector<ScriptNative> Natives() const override
	{
		return {{.id = 4, .name = "ADD", .in = 2, .out = 1, .implemented = true},
		        {.id = 5, .name = "UNWRITTEN", .in = 0, .out = 0, .implemented = false}};
	}
	std::variant<std::vector<ScriptValue>, std::string> CallNative(uint32_t id, const std::vector<ScriptValue>& args) override
	{
		called = id;
		return std::vector<ScriptValue> {{.type = ScriptValue::Type::Float, .number = args[0].number + args[1].number}};
	}

	ScriptValue stage {.type = ScriptValue::Type::Float, .number = 2.0f};
	ScriptValue hasCreature {.type = ScriptValue::Type::Boolean, .boolean = false};
	uint32_t called {0};
};

} // namespace

TEST(InspectorScripts, RunGlobalsAndNatives)
{
	FakeScripts scripts;
	auto provider = std::make_unique<FunctionProvider>("script");
	AddScriptControls(*provider, scripts);
	Inspector inspector;
	inspector.Add(std::move(provider));

	EXPECT_EQ(Ask(inspector, R"({"query": "script.scripts", "params": {"name": "land1.TXT"}})")["total"], 1);
	EXPECT_EQ(Ask(inspector, R"({"query": "script.run", "params": {"name": "Land1Intro"}})")["task"], 7);
	EXPECT_FALSE(Refused(inspector, R"({"query": "script.run", "params": {"name": "Nope"}})").empty());
	EXPECT_EQ(Ask(inspector, R"({"query": "script.stop", "params": {"task": 7}})")["stopped"], 7);

	const auto globals = Ask(inspector, R"({"query": "script.globals", "params": {"name": "land1"}})");
	ASSERT_EQ(globals["total"], 1);
	EXPECT_EQ(globals["items"][0]["type"], "float");
	EXPECT_EQ(Ask(inspector, R"({"query": "script.set_global", "params": {"name": "Land1Stage", "value": 5}})")["value"], 5.0);
	EXPECT_FLOAT_EQ(scripts.stage.number, 5.0f);
	// A global keeps its type
	EXPECT_FALSE(
	    Refused(inspector, R"({"query": "script.set_global", "params": {"name": "HasCreature", "value": 1}})").empty());
	Ask(inspector, R"({"query": "script.set_global", "params": {"name": "HasCreature", "value": true}})");
	EXPECT_TRUE(scripts.hasCreature.boolean);

	const auto added = Ask(inspector, R"({"query": "script.call", "params": {"native": "add", "args": [2, 3.5]}})");
	EXPECT_EQ(scripts.called, 4u);
	EXPECT_EQ(added["returned"][0]["value"], 5.5);
	EXPECT_FALSE(Refused(inspector, R"({"query": "script.call", "params": {"native": "ADD", "args": [1]}})").empty());
	EXPECT_FALSE(Refused(inspector, R"({"query": "script.call", "params": {"native": 5}})").empty());
	EXPECT_EQ(Ask(inspector, R"({"query": "script.functions", "params": {"name": "ad"}})")["total"], 1);
}

TEST(InspectorScripts, ArgumentsReadAsTheScriptsGiveThem)
{
	std::string error;
	const auto args = ArgumentsFromJson(Json::parse(R"([1.5, true, [1, 2, 3], {"object": 9}, {"int": -4}])"), error);
	ASSERT_TRUE(args.has_value()) << error;
	ASSERT_EQ(args->size(), 7u);
	EXPECT_EQ((*args)[0].type, ScriptValue::Type::Float);
	EXPECT_EQ((*args)[1].type, ScriptValue::Type::Boolean);
	EXPECT_EQ((*args)[2].type, ScriptValue::Type::Vector);
	EXPECT_FLOAT_EQ((*args)[4].number, 3.0f);
	EXPECT_EQ((*args)[5].object, 9u);
	EXPECT_EQ((*args)[6].integer, -4);
	EXPECT_FALSE(ArgumentsFromJson(Json::parse(R"(["text"])"), error).has_value());
}

namespace
{

class FakeLevels final: public LevelTargetInterface
{
public:
	[[nodiscard]] std::vector<LevelInfo> Levels() const override
	{
		return {{.name = "Land 1", .kind = "story", .valid = true},
		        {.name = "Two Gods", .kind = "playground", .valid = true},
		        {.name = "Broken", .kind = "playground", .valid = false}};
	}
	std::string Load(std::string_view name, LoadHow how) override
	{
		current = std::string(name);
		lastHow = how;
		return {};
	}
	std::string LoadTestbed() override
	{
		current = "testbed";
		return {};
	}
	[[nodiscard]] std::string Current() const override { return current; }

	std::string current;
	std::optional<LoadHow> lastHow;
};

class FakeScreenshots final: public ScreenshotTargetInterface
{
public:
	std::string Capture(const std::filesystem::path& path) override
	{
		taken.push_back(path.generic_string());
		return {};
	}
	[[nodiscard]] std::filesystem::path Directory() const override { return "shots"; }

	std::vector<std::string> taken;
};

} // namespace

TEST(InspectorLevels, LoadByNameFreshOrAsTheStoryChangesLand)
{
	FakeLevels levels;
	Inspector inspector;
	inspector.Add(MakeLevelProvider(levels));
	EXPECT_EQ(Ask(inspector, R"({"query": "level.list"})")["total"], 3);
	EXPECT_EQ(Ask(inspector, R"({"query": "level.load", "params": {"name": "Land 1"}})")["land"], "Land 1");
	EXPECT_EQ(levels.lastHow, LoadHow::Fresh);
	Ask(inspector, R"({"query": "level.load", "params": {"name": "Two Gods", "how": "story"}})");
	EXPECT_EQ(levels.lastHow, LoadHow::Story);
	EXPECT_FALSE(Refused(inspector, R"({"query": "level.load", "params": {"name": "Broken"}})").empty());
	EXPECT_FALSE(Refused(inspector, R"({"query": "level.load", "params": {"name": "Nope"}})").empty());
	EXPECT_EQ(Ask(inspector, R"({"query": "level.testbed"})")["land"], "testbed");
}

// A picture at an exact frame, the camera put in place that same frame
TEST(InspectorScreenshot, TakenAtTheFrameAskedWithTheCameraAsked)
{
	FakeScreenshots screenshots;
	FakeCamera camera;
	auto owned = std::make_unique<ScreenshotProvider>(screenshots, camera);
	auto* provider = owned.get();
	Inspector inspector;
	inspector.Add(std::move(owned));
	provider->Frame(10);

	// This frame
	const auto now = Ask(inspector, R"({"query": "screenshot.take"})");
	EXPECT_EQ(now["frame"], 11);
	ASSERT_EQ(screenshots.taken.size(), 1u);
	EXPECT_EQ(screenshots.taken[0], "shots/frame_11.png");

	// Three frames on, from a camera looking at a point
	const auto later = Ask(inspector, R"({"query": "screenshot.take", "params": {"in_frames": 3, "path": "a/b.png",
	                                     "camera": {"focus": [5, 0, 5], "distance": 20}}})");
	EXPECT_EQ(later["frame"], 14);
	for (uint64_t frame = 11; frame <= 13; ++frame)
	{
		provider->Frame(frame);
	}
	EXPECT_EQ(screenshots.taken.size(), 1u);
	EXPECT_EQ(camera.sets, 0);
	provider->Frame(14);
	ASSERT_EQ(screenshots.taken.size(), 2u);
	EXPECT_EQ(screenshots.taken[1], "a/b.png");
	EXPECT_EQ(camera.sets, 1);
	EXPECT_FLOAT_EQ(camera.pose.focus.x, 5.0f);

	EXPECT_FALSE(Refused(inspector, R"({"query": "screenshot.take", "params": {"at_frame": 3}})").empty());
	EXPECT_FALSE(Refused(inspector, R"({"query": "screenshot.take", "params": {"path": "x.bmp"}})").empty());
}
