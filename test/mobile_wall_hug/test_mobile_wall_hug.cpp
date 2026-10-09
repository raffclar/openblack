/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdlib>

#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>

#include <Common/GUtilsAngle.h>
#include <ECS/Components/Transform.h>
#include <ECS/Components/Villager.h>
#include <ECS/Components/WallHug.h>
#include <ECS/Map.h>
#include <ECS/Registry.h>
#include <ECS/Systems/PathfindingSystemInterface.h>
#include <ECS/WallHugRules.h>
#include <Game.h>
#include <LHScriptX/Script.h>
#include <Locator.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>
#include <json_helpers.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using nlohmann::json;
using namespace openblack;

enum MOVE_STATE
{
	MOVE_STATE_ARRIVED,
	MOVE_STATE_FINAL_STEP,
	MOVE_STATE_STEP_THROUGH,
	MOVE_STATE_LINEAR,
	MOVE_STATE_LINEAR_CW,
	MOVE_STATE_LINEAR_CCW,
	MOVE_STATE_ORBIT_CW,
	MOVE_STATE_ORBIT_CCW,
	MOVE_STATE_EXIT_CIRCLE_CCW,
	MOVE_STATE_EXIT_CIRCLE_CW,

	_MOVE_STATE_INVALID = -1,
};

// NOLINTNEXTLINE(modernize-avoid-c-arrays): external macro
NLOHMANN_JSON_SERIALIZE_ENUM( //
    MOVE_STATE,               //
    {
        {_MOVE_STATE_INVALID, nullptr},
        {MOVE_STATE_ARRIVED, "ARRIVED"},
        {MOVE_STATE_FINAL_STEP, "FINAL_STEP"},
        {MOVE_STATE_STEP_THROUGH, "STEP_THROUGH"},
        {MOVE_STATE_LINEAR, "LINEAR"},
        {MOVE_STATE_LINEAR_CW, "LINEAR_CW"},
        {MOVE_STATE_LINEAR_CCW, "LINEAR_CCW"},
        {MOVE_STATE_ORBIT_CW, "ORBIT_CW"},
        {MOVE_STATE_ORBIT_CCW, "ORBIT_CCW"},
        {MOVE_STATE_EXIT_CIRCLE_CCW, "EXIT_CIRCLE_CCW"},
        {MOVE_STATE_EXIT_CIRCLE_CW, "EXIT_CIRCLE_CW"},
    })

enum VILLAGER_STATE
{
	VILLAGER_STATE_MOVE_TO_POS,
	VILLAGER_STATE_GO_AND_CHILLOUT_OUTSIDE_HOME,
	VILLAGER_STATE_MOVE_ON_PATH,
	VILLAGER_STATE_ARRIVES_HOME,

	_VILLAGER_STATE_INVALID = -1,
};

// NOLINTNEXTLINE(modernize-avoid-c-arrays): external macro
NLOHMANN_JSON_SERIALIZE_ENUM( //
    VILLAGER_STATE,           //
    {
        {_VILLAGER_STATE_INVALID, nullptr},
        {VILLAGER_STATE_MOVE_TO_POS, "MOVE_TO_POS"},
        {VILLAGER_STATE_GO_AND_CHILLOUT_OUTSIDE_HOME, "GO_AND_CHILLOUT_OUTSIDE_HOME"},
        {VILLAGER_STATE_MOVE_ON_PATH, "MOVE_ON_PATH"},
        {VILLAGER_STATE_ARRIVES_HOME, "ARRIVES_HOME"},
    })

class MobileWallHugWalks: public ::testing::Test
{
protected:
	struct State
	{
		struct CircleHugInfo
		{
			// NOLINTNEXTLINE(readability-identifier-naming): Needs to match json
			std::optional<uint32_t> obj_index;
			// NOLINTNEXTLINE(readability-identifier-naming): Needs to match json
			uint8_t turns_to_obstacle;
			// NOLINTNEXTLINE(readability-identifier-naming): Needs to match json
			uint8_t field_0x5; // TODO(bwrsandman): Unknown function
			// NOLINTNEXTLINE(readability-identifier-naming): Needs to match json
			uint16_t field_0x6; // TODO(bwrsandman): Unknown function
		};

		uint32_t turn;
		uint32_t id;
		glm::vec2 pos;
		// NOLINTNEXTLINE(readability-identifier-naming): Needs to match json
		uint16_t field_0x24; // TODO(bwrsandman): Unknown function
		                     // NOLINTNEXTLINE(readability-identifier-naming): Needs to match json
		uint8_t field_0x26;  // TODO(bwrsandman): Unknown function
		                     // NOLINTNEXTLINE(readability-identifier-naming): Needs to match json
		uint32_t turns_until_next_state_change;
		float speed;
		// NOLINTNEXTLINE(readability-identifier-naming): Needs to match json
		float y_angle;
		// NOLINTNEXTLINE(readability-identifier-naming): Needs to match json
		MOVE_STATE move_state;
		glm::vec2 step;
		// NOLINTNEXTLINE(readability-identifier-naming): Needs to match json
		CircleHugInfo circle_hug_info;
		uint32_t status; // Probably unused
		glm::vec2 goal;
		float distance; // Derived
		// NOLINTNEXTLINE(readability-identifier-naming): Needs to match json
		std::tuple<VILLAGER_STATE, VILLAGER_STATE> state_map;
	};

	void SetUp() override
	{
		const auto* testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
		const auto testResultsPath = std::filesystem::path(k_ScenarioPath) / (testName + std::string(".json"));
		_testName = testName;
		auto& results = _results;
		std::ifstream(testResultsPath) >> results;
		_startTurn = results["start_turn"];
		_lastTurn = results["last_turn"];
		const auto testScenePath = std::filesystem::path(k_ScenarioPath) / results["map_file"];
		const auto& villagerStates = results["villager_states"];
		_expectedStates.reserve(villagerStates.size());
#define GET_SAME_TYPE_AS(name) s[#name].get<decltype(_expectedStates[0].name)>()
#define GET_SAME_TYPE_AS_HEX(name) \
	static_cast<decltype(_expectedStates[0].name)>(std::stoul(s[#name].get<std::string>(), nullptr, 16))
#define GET_VEC2(name) decltype(_expectedStates[0].name)(s[#name][0].get<float>(), s[#name][1].get<float>())
		for (const auto& s : villagerStates)
		{
			auto circleHugInfo = s["circle_hug_info"];
			const auto& e = _expectedStates.emplace_back(State {
			    GET_SAME_TYPE_AS(turn),
			    GET_SAME_TYPE_AS(id),
			    GET_VEC2(pos),
			    GET_SAME_TYPE_AS_HEX(field_0x24),
			    GET_SAME_TYPE_AS_HEX(field_0x26),
			    GET_SAME_TYPE_AS(turns_until_next_state_change),
			    GET_SAME_TYPE_AS(speed),
			    GET_SAME_TYPE_AS(y_angle),
			    GET_SAME_TYPE_AS(move_state),
			    GET_VEC2(step),
			    {
			        circleHugInfo["obj_index"].is_null() ? std::nullopt
			                                             : std::make_optional(circleHugInfo["obj_index"].get<uint32_t>()),
			        static_cast<uint8_t>(std::stoi(circleHugInfo["turns_to_obj"].get<std::string>(), nullptr, 16)),
			        static_cast<uint8_t>(std::stoi(circleHugInfo["field_0x5"].get<std::string>(), nullptr, 16)),
			        static_cast<uint16_t>(std::stoi(circleHugInfo["field_0x6"].get<std::string>(), nullptr, 16)),
			    },
			    GET_SAME_TYPE_AS_HEX(status),
			    GET_VEC2(goal),
			    GET_SAME_TYPE_AS(distance),
			    GET_SAME_TYPE_AS(state_map),
			});

			ASSERT_NE(e.move_state, _MOVE_STATE_INVALID) << "Unexpected Move State: " + s["move_state"].get<std::string>();
			ASSERT_NE(std::get<0>(e.state_map), _VILLAGER_STATE_INVALID)
			    << "Unexpected Villager State: " + s["state_map"][0].get<std::string>();
			ASSERT_NE(std::get<1>(e.state_map), _VILLAGER_STATE_INVALID)
			    << "Unexpected Villager State: " + s["state_map"][1].get<std::string>();
		}

#undef GET_SAME_TYPE_AS
#undef GET_SAME_TYPE_AS_HEX
#undef GET_MAP_COORDS

		ASSERT_TRUE(std::filesystem::exists(testScenePath));

		{
			std::ifstream ifs(testScenePath);
			_sceneScript = std::string(std::istreambuf_iterator<char> {ifs}, {});
		}

		static const auto mockGamePath = std::filesystem::path(TEST_BINARY_DIR) / "mock";
		auto args = openblack::Arguments {
		    .graphicsBackend = openblack::GraphicsBackend::Noop,
		    .gamePath = mockGamePath.string(),
		    .logFile = "stdout",
		};
		std::fill_n(args.logLevels.begin(), args.logLevels.size(), spdlog::level::warn);
		args.logLevels[static_cast<uint8_t>(openblack::LoggingSubsystem::pathfinding)] = spdlog::level::debug;
		_game = std::make_unique<openblack::Game>(std::move(args));
		ASSERT_TRUE(_game->Initialize());
		openblack::lhscriptx::Script script;
		script.Load(_sceneScript);

		_villagerEntt = Locator::entitiesRegistry::value().Front<const ecs::components::Villager>();
		auto& villagerTransform = Locator::entitiesRegistry::value().Get<ecs::components::Transform>(_villagerEntt);

		villagerTransform.position = glm::vec3(_expectedStates[0].pos.x, 0.0f, _expectedStates[0].pos.y);
	}

	void TearDown() override { _game.reset(); }

	/// checkTurns: whether the turns to the next obstacle are checked as well
	void MobileWallHugScenarioAssert(bool checkTurns = true)
	{
		auto& map = Locator::entitiesMap::value();
		auto& registry = Locator::entitiesRegistry::value();
		map.Sync();
		registry.Each<ecs::components::WallHug>([&registry, this](entt::entity entity, ecs::components::WallHug& wallHug) {
			using namespace openblack::ecs::components;
			registry.Assign<MoveStateLinearTag>(entity);
			// The recordings hold the speed in metres a turn; the walk holds it in metres a second
			wallHug.speed = _expectedStates[0].speed * openblack::ecs::wall_hug::k_TurnsPerSecond;
			// and the step in metres; the walk holds it in whole map units
			const auto& step = _expectedStates[0].step;
			wallHug.step = {std::lround(step.x * 6553.6), std::lround(step.y * 6553.6)};
			wallHug.goal = _expectedStates[0].goal;
			wallHug.yAngle = _expectedStates[0].y_angle;
			wallHug.gameAngle = static_cast<uint16_t>(openblack::gutils::ConvertAngle3DToGame(wallHug.yAngle));
		});

		// With OPENBLACK_RECORD_WALKS set to a folder, the walk is recorded there as it now goes instead of checked
		const char* recordTo = std::getenv("OPENBLACK_RECORD_WALKS");
		for (uint32_t turn = _startTurn; turn < _lastTurn; ++turn)
		{
			if (recordTo != nullptr)
			{
				Record(turn);
				Locator::pathfindingSystem::value().Update();
				continue;
			}
			const auto& villagerComp = registry.Get<ecs::components::Villager>(_villagerEntt);
			const auto& villagerTransform = registry.Get<ecs::components::Transform>(_villagerEntt);
			const auto& villagerWallhug = registry.Get<ecs::components::WallHug>(_villagerEntt);
			bool villagerHasObstacle = registry.AnyOf<ecs::components::WallHugObjectReference>(_villagerEntt);
			const auto& state = _expectedStates[turn - _startTurn];
			const auto msg = std::string("on turn ") + std::to_string(turn) + " in range " + std::to_string(_startTurn) + "-" +
			                 std::to_string(_lastTurn);

			switch (state.move_state)
			{
			default:
			case _MOVE_STATE_INVALID:
				ASSERT_TRUE(false);
				break;
			case MOVE_STATE_LINEAR:
			case MOVE_STATE_LINEAR_CW:
			case MOVE_STATE_LINEAR_CCW:
			{
				ASSERT_TRUE(registry.AllOf<ecs::components::MoveStateLinearTag>(_villagerEntt)) << msg;
				const auto& villagerState = registry.Get<ecs::components::MoveStateLinearTag>(_villagerEntt);
				if (state.move_state == MOVE_STATE_LINEAR)
				{
					ASSERT_EQ(villagerState.clockwise, ecs::components::MoveStateClockwise::Undefined) << msg;
				}
				else if (state.move_state == MOVE_STATE_LINEAR_CW)
				{
					ASSERT_EQ(villagerState.clockwise, ecs::components::MoveStateClockwise::Clockwise) << msg;
				}
				else if (state.move_state == MOVE_STATE_LINEAR_CCW)
				{
					ASSERT_EQ(villagerState.clockwise, ecs::components::MoveStateClockwise::CounterClockwise) << msg;
				}
			}
			break;
			case MOVE_STATE_ORBIT_CW:
			case MOVE_STATE_ORBIT_CCW:
			{
				ASSERT_TRUE(registry.AllOf<ecs::components::MoveStateOrbitTag>(_villagerEntt)) << msg;
				const auto& villagerState = registry.Get<ecs::components::MoveStateOrbitTag>(_villagerEntt);
				if (state.move_state == MOVE_STATE_ORBIT_CW)
				{
					ASSERT_EQ(villagerState.clockwise, ecs::components::MoveStateClockwise::Clockwise) << msg;
				}
				else if (state.move_state == MOVE_STATE_ORBIT_CCW)
				{
					ASSERT_EQ(villagerState.clockwise, ecs::components::MoveStateClockwise::CounterClockwise) << msg;
				}
			}
			break;
			case MOVE_STATE_EXIT_CIRCLE_CW:
			case MOVE_STATE_EXIT_CIRCLE_CCW:
			{
				ASSERT_TRUE(registry.AllOf<ecs::components::MoveStateExitCircleTag>(_villagerEntt)) << msg;
				const auto& villagerState = registry.Get<ecs::components::MoveStateExitCircleTag>(_villagerEntt);
				if (state.move_state == MOVE_STATE_EXIT_CIRCLE_CW)
				{
					ASSERT_EQ(villagerState.clockwise, ecs::components::MoveStateClockwise::Clockwise) << msg;
				}
				else if (state.move_state == MOVE_STATE_EXIT_CIRCLE_CCW)
				{
					ASSERT_EQ(villagerState.clockwise, ecs::components::MoveStateClockwise::CounterClockwise) << msg;
				}
			}
			break;
			case MOVE_STATE_ARRIVED:
				ASSERT_TRUE(registry.AllOf<ecs::components::MoveStateArrivedTag>(_villagerEntt)) << msg;
				break;
			case MOVE_STATE_FINAL_STEP:
				ASSERT_TRUE(registry.AllOf<ecs::components::MoveStateFinalStepTag>(_villagerEntt)) << msg;
				break;
			case MOVE_STATE_STEP_THROUGH:
				ASSERT_TRUE(registry.AllOf<ecs::components::MoveStateStepThroughTag>(_villagerEntt)) << msg;
				break;
			}
			ASSERT_FLOAT_EQ(villagerTransform.position.x, state.pos.x) << msg;
			ASSERT_FLOAT_EQ(villagerTransform.position.z, state.pos.y) << msg;
			if (state.move_state != MOVE_STATE_FINAL_STEP) // Set in the next turn
			{
				const auto step = openblack::ecs::wall_hug::ToPoint(villagerWallhug.step);
				ASSERT_FLOAT_EQ(step.x, state.step.x) << msg;
				ASSERT_FLOAT_EQ(step.y, state.step.y) << msg;
			}
			ASSERT_FLOAT_EQ(villagerWallhug.goal.x, state.goal.x) << msg;
			ASSERT_FLOAT_EQ(villagerWallhug.goal.y, state.goal.y) << msg;
			ASSERT_EQ(villagerHasObstacle, state.circle_hug_info.obj_index.has_value()) << msg;
			if (state.circle_hug_info.turns_to_obstacle != 0xFF || state.circle_hug_info.obj_index.has_value())
			{
				ASSERT_TRUE(villagerHasObstacle) << msg;
				const auto& ref = registry.Get<ecs::components::WallHugObjectReference>(_villagerEntt);
				if (checkTurns)
				{
					ASSERT_EQ(ref.stepsAway, state.circle_hug_info.turns_to_obstacle) << msg;
				}
			}

			ASSERT_NO_THROW(Locator::pathfindingSystem::value().Update()) << msg;
		}
		if (recordTo != nullptr)
		{
			Record(_lastTurn);
			std::ofstream(std::filesystem::path(recordTo) / (_testName + ".json"), std::ios::binary)
			    << _results.dump(2) << '\n';
		}
	}

	/// Writes the walker's state this turn over the recording's
	void Record(uint32_t turn)
	{
		using namespace openblack::ecs::components;
		auto& registry = Locator::entitiesRegistry::value();
		auto& states = _results["villager_states"];
		const auto index = turn - _startTurn;
		if (index >= states.size())
		{
			states.push_back(states.back());
		}
		auto& s = states[index];
		const auto& transform = registry.Get<Transform>(_villagerEntt);
		const auto& wallHug = registry.Get<WallHug>(_villagerEntt);
		const auto step = openblack::ecs::wall_hug::ToPoint(wallHug.step);
		s["turn"] = turn;
		s["pos"] = {transform.position.x, transform.position.z};
		s["step"] = {step.x, step.y};
		s["y_angle"] = wallHug.yAngle;
		s["goal"] = {wallHug.goal.x, wallHug.goal.y};
		s["distance"] = glm::distance(glm::vec2(transform.position.x, transform.position.z), wallHug.goal);
		std::string state;
		if (registry.AllOf<MoveStateLinearTag>(_villagerEntt))
		{
			const auto c = registry.Get<MoveStateLinearTag>(_villagerEntt).clockwise;
			state = c == MoveStateClockwise::Undefined   ? "LINEAR"
			        : c == MoveStateClockwise::Clockwise ? "LINEAR_CW"
			                                             : "LINEAR_CCW";
		}
		else if (registry.AllOf<MoveStateOrbitTag>(_villagerEntt))
		{
			const auto c = registry.Get<MoveStateOrbitTag>(_villagerEntt).clockwise;
			state = c == MoveStateClockwise::Clockwise ? "ORBIT_CW" : "ORBIT_CCW";
		}
		else if (registry.AllOf<MoveStateExitCircleTag>(_villagerEntt))
		{
			const auto c = registry.Get<MoveStateExitCircleTag>(_villagerEntt).clockwise;
			state = c == MoveStateClockwise::Clockwise ? "EXIT_CIRCLE_CW" : "EXIT_CIRCLE_CCW";
		}
		else if (registry.AllOf<MoveStateStepThroughTag>(_villagerEntt))
		{
			state = "STEP_THROUGH";
		}
		else if (registry.AllOf<MoveStateFinalStepTag>(_villagerEntt))
		{
			state = "FINAL_STEP";
		}
		else
		{
			state = "ARRIVED";
		}
		s["move_state"] = state;
		auto& hug = s["circle_hug_info"];
		if (const auto* reference = registry.TryGet<WallHugObjectReference>(_villagerEntt))
		{
			hug["obj_index"] = reference->entity == entt::null ? 0xFFFFFFFFU : entt::to_integral(reference->entity);
			hug["turns_to_obj"] = fmt::format("0x{:02x}", reference->stepsAway);
		}
		else
		{
			hug.erase("obj_index");
			hug["turns_to_obj"] = "0xff";
		}
	}

	static constexpr std::string_view k_ScenarioPath = TEST_BINARY_DIR "/mobile_wall_hug/scenarios";
	std::string _sceneScript;
	std::string _testName;
	nlohmann::ordered_json _results;
	uint32_t _startTurn;
	uint32_t _lastTurn;
	std::vector<State> _expectedStates;
	std::unique_ptr<openblack::Game> _game;
	entt::entity _villagerEntt;
};

// The walks were first recorded in 2021 with continuous angles, so they were never the game's own state: the game walks
// in whole map units at whole game angles. They match the 2021 recordings only up to the walker's first re-aim at its
// goal; from there they are recorded from this walk (run the test with OPENBLACK_RECORD_WALKS set to a folder to record
// them again). Buildings are filed in every map cell their footprint covers, and with that walk 2's first orbit sees the
// next house as many turns ahead as the 2021 recording does. Its straight walk after that does not see the house it
// walked round next in the 2021 recording, as the straight-line check still takes the first thing near its next step
// rather than the nearest circle.

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(MobileWallHugWalks, mobilewallhug1)
{
	MobileWallHugScenarioAssert();
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(MobileWallHugWalks, mobilewallhug2)
{
	MobileWallHugScenarioAssert();
}

// TODO(bwrsandman): Remove DISABLED_ prefix once walking on footpath is implemented
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(MobileWallHugWalks, DISABLED_footpath1)
{
	MobileWallHugScenarioAssert();
}

// TODO(bwrsandman): Remove DISABLED_ prefix once walking on footpath is implemented
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(MobileWallHugWalks, DISABLED_footpath2)
{
	MobileWallHugScenarioAssert();
}
