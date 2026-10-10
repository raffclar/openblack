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
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <Inspector.h>
#include <Inspector/CameraControl.h>
#include <Inspector/GuiControl.h>
#include <Inspector/LevelControl.h>
#include <Inspector/ScriptControl.h>
#include <glm/geometric.hpp>
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
	[[nodiscard]] std::optional<glm::vec3> EntityPosition(uint32_t id) const override
	{
		return id == 7 ? std::optional(walker) : std::nullopt;
	}

	CameraPose pose {.origin = {0.0f, 100.0f, -100.0f}, .focus = {0.0f, 0.0f, 0.0f}};
	/// Entity 7, which may walk about
	glm::vec3 walker {50.0f, 10.0f, 50.0f};
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
		        {.id = 5, .name = "UNWRITTEN", .in = 0, .out = 0, .implemented = false},
		        // Takes a text's number, a time and a truth, as a script gives them
		        {.id = 6,
		         .name = "SAY",
		         .in = 3,
		         .out = 0,
		         .implemented = true,
		         .slots = {ScriptValue::Type::Int, ScriptValue::Type::Float, ScriptValue::Type::Boolean}}};
	}
	std::variant<std::vector<ScriptValue>, std::string> CallNative(uint32_t id, const std::vector<ScriptValue>& args) override
	{
		called = id;
		given = args;
		if (id != 4)
		{
			return std::vector<ScriptValue> {};
		}
		return std::vector<ScriptValue> {{.type = ScriptValue::Type::Float, .number = args[0].number + args[1].number}};
	}

	ScriptValue stage {.type = ScriptValue::Type::Float, .number = 2.0f};
	ScriptValue hasCreature {.type = ScriptValue::Type::Boolean, .boolean = false};
	uint32_t called {0};
	std::vector<ScriptValue> given;
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

// A number reaches a native as the type it takes, as a script's call puts it on the stack: an integer for an integer
// (a text's number read as a float's bits would be another number), a float for a float
TEST(InspectorScripts, ANativeIsGivenTheTypesItTakes)
{
	FakeScripts scripts;
	auto provider = std::make_unique<FunctionProvider>("script");
	AddScriptControls(*provider, scripts);
	Inspector inspector;
	inspector.Add(std::move(provider));

	Ask(inspector, R"({"query": "script.call", "params": {"native": "SAY", "args": [1203, 2, 1]}})");
	EXPECT_EQ(scripts.called, 6u);
	ASSERT_EQ(scripts.given.size(), 3u);
	EXPECT_EQ(scripts.given[0].type, ScriptValue::Type::Int);
	EXPECT_EQ(scripts.given[0].integer, 1203);
	EXPECT_EQ(scripts.given[1].type, ScriptValue::Type::Float);
	EXPECT_FLOAT_EQ(scripts.given[1].number, 2.0f);
	EXPECT_EQ(scripts.given[2].type, ScriptValue::Type::Boolean);
	EXPECT_TRUE(scripts.given[2].boolean);

	// Given as an integer, a float slot still takes a float; true for the truth
	Ask(inspector, R"({"query": "script.call", "params": {"native": "SAY", "args": [{"int": 7}, {"int": 3}, true]}})");
	EXPECT_EQ(scripts.given[0].integer, 7);
	EXPECT_EQ(scripts.given[1].type, ScriptValue::Type::Float);
	EXPECT_FLOAT_EQ(scripts.given[1].number, 3.0f);

	// What can't be the slot's type is refused, naming the argument
	EXPECT_NE(Refused(inspector, R"({"query": "script.call", "params": {"native": "SAY", "args": [1.5, 2, true]}})")
	              .find("argument 1"),
	          std::string::npos);
	EXPECT_NE(
	    Refused(inspector, R"({"query": "script.call", "params": {"native": "SAY", "args": [3, 2, 5]}})").find("argument 3"),
	    std::string::npos);
	// A native whose types aren't known takes the values as given
	Ask(inspector, R"({"query": "script.call", "params": {"native": "ADD", "args": [2, 3]}})");
	EXPECT_EQ(scripts.given[0].type, ScriptValue::Type::Float);
}

// The game's natives take the types the language's table gives them: RUN_TEXT a truth and two integers, a camera move
// a position's three slots and a float
TEST(InspectorScripts, TheNativesTypesComeFromTheLanguage)
{
	using Type = ScriptValue::Type;
	EXPECT_EQ(NativeSlots("RUN_TEXT", 3), (std::vector<std::optional<Type>> {Type::Boolean, Type::Int, Type::Int}));
	EXPECT_EQ(NativeSlots("MOVE_CAMERA_POSITION", 4),
	          (std::vector<std::optional<Type>> {Type::Vector, Type::Vector, Type::Vector, Type::Float}));
	// Unknown, or not adding up to what the native takes: no types
	EXPECT_TRUE(NativeSlots("NO_SUCH_NATIVE", 2).empty());
	EXPECT_TRUE(NativeSlots("RUN_TEXT", 2).empty());
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
	std::string Capture(const std::filesystem::path& path, bool hideDebugGui) override
	{
		taken.push_back(path.generic_string());
		hidden.push_back(hideDebugGui);
		return {};
	}
	void HideDebugGui() override { ++hiddenFrames; }
	[[nodiscard]] std::filesystem::path Directory() const override { return "shots"; }

	std::vector<std::string> taken;
	std::vector<bool> hidden;
	/// Frames drawn without the debug windows, the picture's own aside
	int hiddenFrames {0};
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

// A picture at an exact frame; one with a camera is held there a few frames first
TEST(InspectorScreenshot, TakenAtTheFrameAskedWithTheCameraAsked)
{
	FakeScreenshots screenshots;
	FakeCamera camera;
	auto owned = std::make_unique<ScreenshotProvider>(screenshots, camera);
	auto* provider = owned.get();
	Inspector inspector;
	inspector.Add(std::move(owned));
	provider->Frame(10);

	// This frame, as it is
	const auto now = Ask(inspector, R"({"query": "screenshot.take"})");
	EXPECT_EQ(now["frame"], 11);
	ASSERT_EQ(screenshots.taken.size(), 1u);
	EXPECT_EQ(screenshots.taken[0], "shots/frame_11.png");

	// Three frames on, from a camera looking at a point: the camera goes there at frame 14, and the picture is taken
	// once it has stayed there a few frames
	const auto later = Ask(inspector, R"({"query": "screenshot.take", "params": {"in_frames": 3, "path": "a/b.png",
	                                     "camera": {"focus": [5, 0, 5], "distance": 20}}})");
	EXPECT_EQ(later["held_from"], 14);
	EXPECT_EQ(later["frame"], 14 + ScreenshotProvider::k_SettleFrames);
	for (uint64_t frame = 11; frame <= 13; ++frame)
	{
		provider->Frame(frame);
		provider->PlaceCamera();
	}
	EXPECT_EQ(camera.sets, 0);
	for (uint64_t frame = 14; frame < 14 + ScreenshotProvider::k_SettleFrames; ++frame)
	{
		provider->Frame(frame);
		provider->PlaceCamera();
		EXPECT_EQ(screenshots.taken.size(), 1u) << frame;
	}
	EXPECT_EQ(camera.sets, static_cast<int>(ScreenshotProvider::k_SettleFrames));
	provider->Frame(14 + ScreenshotProvider::k_SettleFrames);
	ASSERT_EQ(screenshots.taken.size(), 2u);
	EXPECT_EQ(screenshots.taken[1], "a/b.png");
	EXPECT_FLOAT_EQ(camera.pose.focus.x, 5.0f);

	EXPECT_FALSE(Refused(inspector, R"({"query": "screenshot.take", "params": {"at_frame": 3}})").empty());
	EXPECT_FALSE(Refused(inspector, R"({"query": "screenshot.take", "params": {"path": "x.bmp"}})").empty());
}

// The camera asked for is put in place after anything else moved it each frame, from the first frame held until a few
// frames after the picture, so that whichever of those frames the picture is read from shows it: never the view from
// before, nor the last picture's
TEST(InspectorScreenshot, TheCameraIsHeldAroundThePicture)
{
	FakeScreenshots screenshots;
	FakeCamera camera;
	auto owned = std::make_unique<ScreenshotProvider>(screenshots, camera);
	auto* provider = owned.get();
	Inspector inspector;
	inspector.Add(std::move(owned));
	provider->Frame(10);
	const auto asked =
	    Ask(inspector, R"({"query": "screenshot.take", "params": {"camera": {"position": [1, 2, 3], "focus": [4, 5, 6]}}})");
	const auto shot = asked["frame"].get<uint64_t>();
	EXPECT_EQ(shot, 11 + ScreenshotProvider::k_SettleFrames);
	// Frame 11 is being made: the request was answered as it started
	uint64_t frame = 11;
	for (int each = 0; each < 3 * static_cast<int>(ScreenshotProvider::k_SettleFrames); ++each)
	{
		// The player's camera moves on each frame, before the picture's is put back
		camera.pose = {.origin = {9.0f, 9.0f, 9.0f}, .focus = {0.0f, 0.0f, 0.0f}};
		provider->PlaceCamera();
		if (frame <= shot + ScreenshotProvider::k_SettleFrames)
		{
			EXPECT_FLOAT_EQ(camera.pose.origin.x, 1.0f) << frame;
			EXPECT_FLOAT_EQ(camera.pose.focus.z, 6.0f) << frame;
		}
		else
		{
			// Let go of once the frames around the picture are drawn
			EXPECT_FLOAT_EQ(camera.pose.origin.x, 9.0f) << frame;
		}
		provider->Frame(++frame);
		EXPECT_EQ(screenshots.taken.size(), frame >= shot ? 1u : 0u) << frame;
	}

	// A second picture right after framing elsewhere is taken from its own camera
	Ask(inspector, R"({"query": "screenshot.take", "params": {"path": "c.png", "camera": {"position": [7, 7, 7],
	                  "focus": [0, 0, 0]}}})");
	for (uint64_t each = 0; each <= ScreenshotProvider::k_SettleFrames; ++each)
	{
		camera.pose = {.origin = {1.0f, 2.0f, 3.0f}, .focus = {4.0f, 5.0f, 6.0f}};
		provider->PlaceCamera();
		provider->Frame(++frame);
	}
	ASSERT_EQ(screenshots.taken.size(), 2u);
	EXPECT_FLOAT_EQ(camera.pose.origin.x, 7.0f);

	// Held by a camera path, the picture is refused rather than taken from elsewhere
	camera.held = true;
	EXPECT_FALSE(Refused(inspector, R"({"query": "screenshot.take", "params": {"camera": {"yaw": 3}}})").empty());
}

// Two held pictures asked for at once take turns: the second's camera goes in place once the first's frames are free,
// and each answer says the frame its picture is taken
TEST(InspectorScreenshot, HeldPicturesTakeTurns)
{
	FakeScreenshots screenshots;
	FakeCamera camera;
	auto owned = std::make_unique<ScreenshotProvider>(screenshots, camera);
	auto* provider = owned.get();
	Inspector inspector;
	inspector.Add(std::move(owned));
	provider->Frame(10);
	const auto first = Ask(inspector, R"({"query": "screenshot.take", "params": {"path": "1.png", "camera": {"position":
	                                     [1, 2, 3], "focus": [0, 0, 0]}}})");
	const auto second = Ask(inspector, R"({"query": "screenshot.take", "params": {"path": "2.png", "camera": {"position":
	                                      [7, 8, 9], "focus": [0, 0, 0]}}})");
	const auto firstShot = first["frame"].get<uint64_t>();
	const auto secondShot = second["frame"].get<uint64_t>();
	EXPECT_EQ(second["held_from"], firstShot + ScreenshotProvider::k_SettleFrames + 1);
	EXPECT_TRUE(second.contains("note"));
	std::vector<glm::vec3> seenAt;
	for (uint64_t frame = 11; frame <= secondShot; ++frame)
	{
		if (frame != 11)
		{
			provider->Frame(frame);
		}
		provider->PlaceCamera();
		if (frame == firstShot || frame == secondShot)
		{
			seenAt.push_back(camera.pose.origin);
		}
	}
	ASSERT_EQ(screenshots.taken, (std::vector<std::string> {"1.png", "2.png"}));
	ASSERT_EQ(seenAt.size(), 2u);
	EXPECT_FLOAT_EQ(seenAt[0].x, 1.0f);
	EXPECT_FLOAT_EQ(seenAt[1].x, 7.0f);
}

// Without the debug windows, they (and the input lock's notice) are kept out of every frame held around the picture,
// not only the picture's own
TEST(InspectorScreenshot, TheDebugWindowsAreKeptOutAroundThePicture)
{
	FakeScreenshots screenshots;
	FakeCamera camera;
	auto owned = std::make_unique<ScreenshotProvider>(screenshots, camera);
	auto* provider = owned.get();
	Inspector inspector;
	inspector.Add(std::move(owned));
	provider->Frame(10);
	const auto asked = Ask(inspector, R"({"query": "screenshot.take", "params": {"hide_gui": true}})");
	EXPECT_EQ(asked["frame"], 11 + ScreenshotProvider::k_SettleFrames);
	EXPECT_EQ(screenshots.hiddenFrames, 1);
	for (uint64_t frame = 12; frame <= 11 + 4 * ScreenshotProvider::k_SettleFrames; ++frame)
	{
		provider->Frame(frame);
	}
	ASSERT_EQ(screenshots.hidden.size(), 1u);
	EXPECT_TRUE(screenshots.hidden[0]);
	// Every frame from the first held to a few after the picture
	EXPECT_EQ(screenshots.hiddenFrames, static_cast<int>(2 * ScreenshotProvider::k_SettleFrames + 1));
}

// A moving entity is framed where it is at the picture's frame, not where it was when the picture was asked for
TEST(InspectorScreenshot, AFramedEntityIsFramedWhereItIsAtThePicturesFrame)
{
	FakeScreenshots screenshots;
	FakeCamera camera;
	auto owned = std::make_unique<ScreenshotProvider>(screenshots, camera);
	auto* provider = owned.get();
	Inspector inspector;
	inspector.Add(std::move(owned));
	provider->Frame(10);

	const auto asked = Ask(inspector, R"({"query": "screenshot.take", "params": {"in_frames": 2, "hide_gui": true,
	                                     "frame": {"id": 7, "yaw": 90, "pitch": 0, "distance": 20}}})");
	EXPECT_EQ(asked["framing"], 7);
	EXPECT_EQ(asked["hide_gui"], true);
	camera.walker = {100.0f, 10.0f, 30.0f};
	const auto shot = asked["frame"].get<uint64_t>();
	EXPECT_EQ(shot, 13 + ScreenshotProvider::k_SettleFrames);
	for (uint64_t frame = 11; frame < shot; ++frame)
	{
		provider->Frame(frame);
		provider->PlaceCamera();
	}
	EXPECT_TRUE(screenshots.hidden.empty());
	provider->Frame(shot);
	provider->PlaceCamera();
	ASSERT_EQ(screenshots.hidden.size(), 1u);
	EXPECT_TRUE(screenshots.hidden[0]);
	ExpectNear(Json::array({camera.pose.focus.x, camera.pose.focus.y, camera.pose.focus.z}), {100.0f, 10.0f, 30.0f});
	// Looking along +x from 20 away, level
	ExpectNear(Json::array({camera.pose.origin.x, camera.pose.origin.y, camera.pose.origin.z}), {80.0f, 10.0f, 30.0f});

	// An id alone keeps the camera's angles and distance
	for (uint64_t frame = shot + 1; frame <= shot + 2 * ScreenshotProvider::k_SettleFrames; ++frame)
	{
		provider->Frame(frame);
	}
	Ask(inspector, R"({"query": "screenshot.take", "params": {"frame": 7}})");
	provider->PlaceCamera();
	EXPECT_NEAR(glm::distance(camera.pose.origin, camera.pose.focus), 20.0f, 1e-3f);

	EXPECT_FALSE(Refused(inspector, R"({"query": "screenshot.take", "params": {"frame": 8}})").empty());
	EXPECT_FALSE(Refused(inspector, R"({"query": "screenshot.take", "params": {"frame": "x"}})").empty());
	EXPECT_FALSE(Refused(inspector, R"({"query": "screenshot.take", "params": {"frame": 7, "camera": {"yaw": 3}}})").empty());
	EXPECT_FALSE(Refused(inspector, R"({"query": "screenshot.take", "params": {"frame": {"id": 7, "pitch": 95}}})").empty());
}

TEST(InspectorCamera, FrameLooksAtAnEntity)
{
	FakeCamera camera;
	Inspector inspector;
	inspector.Add(MakeCameraProvider(camera));
	const auto framed = Ask(inspector, R"({"query": "camera.frame", "params": {"id": 7, "distance": 30}})");
	EXPECT_EQ(framed["entity"], 7);
	ExpectNear(framed["set_to"]["focus"], {50.0f, 10.0f, 50.0f});
	EXPECT_NEAR(framed["set_to"]["distance"].get<double>(), 30.0, 1e-3);
	// The camera's own angles: yaw 0, 45 degrees down
	EXPECT_NEAR(framed["set_to"]["pitch"].get<double>(), 45.0, 1e-3);
	EXPECT_FALSE(Refused(inspector, R"({"query": "camera.frame", "params": {"id": 3}})").empty());
	EXPECT_FALSE(Refused(inspector, R"({"query": "camera.frame", "params": {}})").empty());
}
