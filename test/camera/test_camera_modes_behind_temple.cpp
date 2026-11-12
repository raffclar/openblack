/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <tuple>

#include <SDL_mouse.h>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/CreatureCameraModel.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <Camera/FightCameraModel.h>
#include <Camera/WorldCamera.h>
#include <ECS/Components/Creature.h>
#include <ECS/Components/Transform.h>
#include <ECS/Registry.h>
#include <ECS/Systems/Implementations/CinematicDirectorSystem.h>
#include <ECS/Systems/Implementations/CreatureModeSystem.h>
#include <ECS/Systems/PlayerSystemInterface.h>
#include <Input/KeyBindings.h>
#include <Locator.h>

#include "scenarios/FakeTemple.h"
#include "scenarios/Mock.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{
constexpr glm::vec3 k_Origin {1000.0f, 60.0f, 1000.0f};
constexpr glm::vec3 k_Focus {1000.0f, 0.0f, 1060.0f};
/// The creature stands far from where the camera looks
constexpr glm::vec3 k_CreatureAt {1500.0f, 0.0f, 1000.0f};
constexpr auto k_Frame = 20ms;

/// No land under the cursor
class NoLandPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
};

/// The player's creature is the creature of the test
class FakePlayers final: public ecs::systems::PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity /*playerEntity*/) override {}
	[[nodiscard]] std::optional<entt::entity> FindPlayer(PlayerNames /*name*/) const override { return std::nullopt; }
	entt::entity EnsurePlayer(PlayerNames /*name*/) override { return entt::null; }
	void AddCreature(entt::entity /*creature*/) override {}
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames name) const override
	{
		return name == PlayerNames::PLAYER_ONE ? std::optional(creature) : std::nullopt;
	}
	void KeepForNextLand() override {}
	void ForgetKept() override {}
	[[nodiscard]] bool KeepAlignmentAt(uint32_t /*turn*/) override { return false; }
	void TakeUpKept(entt::entity /*playerEntity*/) override {}

	entt::entity creature {entt::null};
};

/// Zoom To Creature has the middle button and its key. The test presses the one or the other.
class CreatureKeyActions final: public input::GameActionInterface
{
public:
	CreatureKeyActions()
	    : _bindings(input::DefaultKeyBindings())
	{
		for (auto& binding : _bindings)
		{
			if (binding.action == input::BindableActionMap::ZOOM_TO_CREATURE)
			{
				binding.mouse = input::MouseInput::MiddleButton;
			}
		}
	}

	[[nodiscard]] bool GetBindable(input::BindableActionMap action) const override { return Pressed(action); }
	[[nodiscard]] bool GetUnbindable(input::UnbindableActionMap /*action*/) const override { return false; }
	[[nodiscard]] bool GetBindableChanged(input::BindableActionMap action) const override { return Pressed(action); }
	[[nodiscard]] bool GetUnbindableChanged(input::UnbindableActionMap /*action*/) const override { return false; }
	[[nodiscard]] bool GetBindableRepeat(input::BindableActionMap /*action*/) const override { return false; }
	[[nodiscard]] bool GetUnbindableRepeat(input::UnbindableActionMap /*action*/) const override { return false; }
	[[nodiscard]] glm::uvec2 GetMousePosition() const override { return k_MockMousePos; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const override { return {}; }
	[[nodiscard]] float GetMouseWheelDelta() const override { return 0.0f; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const override { return {}; }
	[[nodiscard]] std::span<const input::KeyBinding> GetKeyBindings() const override { return _bindings; }
	[[nodiscard]] uint32_t GetPointerButtons() const override { return byMouse ? SDL_BUTTON_MMASK : 0; }
	void Frame() override {}
	void ProcessEvent(const SDL_Event& /*event*/) override {}

	/// Zoom To Creature goes down in this frame
	bool pressed {false};
	/// The press comes from the middle button, else from the key
	bool byMouse {false};

private:
	[[nodiscard]] bool Pressed(input::BindableActionMap action) const
	{
		return pressed && action == input::BindableActionMap::ZOOM_TO_CREATURE;
	}

	input::KeyBindingTable _bindings;
};

/// The player's camera, the camera of the world behind the temple, and the creature that the camera can follow
class CameraModesBehindTemple: public testing::Test
{
protected:
	void SetUp() override
	{
		// The camera reports its flights to the game's log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Locator::windowing::emplace<MockWindowingSystem>();
		_actions = new CreatureKeyActions();
		Locator::gameActionSystem::reset<input::GameActionInterface>(_actions);
		auto& camera = Locator::camera::emplace<Camera>();
		auto* picking = new NoLandPicking();
		picking->camera = &camera;
		Locator::pickingSystem::reset<MockPickingSystem>(picking);
		_temple = new FakeTemple();
		Locator::temple::reset<TempleInteriorInterface>(_temple);
		_players = new FakePlayers();
		Locator::playerSystem::reset<ecs::systems::PlayerSystemInterface>(_players);
		camera.SetProjectionMatrixPerspective(70.0f, Locator::windowing::value().GetAspectRatio(), 1.0f);
		std::ignore = camera.SetModel(std::make_unique<DefaultWorldCameraModel>(k_Origin, k_Focus));
		camera.SetOrigin(k_Origin).SetFocus(k_Focus);
		_creature = registry.Create();
		registry.Assign<ecs::components::Creature>(_creature);
		registry.Assign<ecs::components::Transform>(_creature, k_CreatureAt, glm::mat3(1.0f), glm::vec3(1.0f));
		_players->creature = _creature;
	}

	void TearDown() override
	{
		_behind.reset();
		Locator::cinematicDirectorSystem::reset();
		Locator::playerSystem::reset();
		Locator::temple::reset();
		Locator::camera::reset();
		Locator::pickingSystem::reset();
		Locator::gameActionSystem::reset();
		Locator::windowing::reset();
		Locator::terrainSystem::reset();
		Locator::audio::reset();
		Locator::entitiesRegistry::reset();
	}

	/// The player goes into the temple: the world's camera continues behind the temple with its model
	void GoIn()
	{
		auto& camera = Locator::camera::value();
		_behind = camera.ContinueWith(camera.SetModel(std::make_unique<DefaultWorldCameraModel>(k_Origin, k_Focus)));
		_temple->worldCamera = _behind.get();
		_temple->active = true;
	}

	/// The player comes out of the temple with no place picked on the map: the player's camera takes back the world's
	void ComeOut()
	{
		Locator::camera::value().TakeBackFrom(*_behind);
		_behind.reset();
		_temple->worldCamera = nullptr;
		_temple->active = false;
	}

	/// Frames of the game: the creature mode, then the camera of the world
	void RunFrames(int frames, ecs::systems::CreatureModeSystemInterface::Frame input = {})
	{
		for (int frame = 0; frame < frames; ++frame)
		{
			_mode.Update(k_Frame, input);
			world_camera::Get().Update(k_Frame);
		}
	}

	/// The camera of the world looks near the creature
	[[nodiscard]] static bool LooksAtTheCreature(const Camera& camera)
	{
		const auto focus = camera.GetFocus();
		return glm::distance(glm::vec2(focus.x, focus.z), glm::vec2(k_CreatureAt.x, k_CreatureAt.z)) < 50.0f;
	}

	ecs::systems::CreatureModeSystem _mode;
	CreatureKeyActions* _actions {nullptr};
	FakeTemple* _temple {nullptr};
	FakePlayers* _players {nullptr};
	std::unique_ptr<Camera> _behind;
	entt::entity _creature {entt::null};
};
} // namespace

TEST(WorldCameraControls, InTheTempleOrUnderABoxOfTheFrontEndThePlayersCameraTakesNoControls)
{
	EXPECT_FALSE(world_camera::InputsOff(false, false));
	EXPECT_TRUE(world_camera::InputsOff(true, false));
	EXPECT_TRUE(world_camera::InputsOff(false, true));
	EXPECT_TRUE(world_camera::InputsOff(true, true));
}

TEST_F(CameraModesBehindTemple, ABoxOfTheFrontEndStopsTheControlsAtTheEndOfTheFrameAndItsGoStartsThemAgain)
{
	auto& world = dynamic_cast<DefaultWorldCameraModel&>(Locator::camera::value().GetModel());
	world_camera::EndFrameControls(true);
	EXPECT_TRUE(world.InputsOff());
	// The frame after the box goes takes the controls again
	world_camera::EndFrameControls(false);
	EXPECT_FALSE(world.InputsOff());
}

TEST_F(CameraModesBehindTemple, ABoxOfTheFrontEndStopsTheControlsOfThePlayersCameraUnderTheFightView)
{
	auto& camera = Locator::camera::value();
	auto fight = std::make_unique<FightCameraModel>(nullptr, fight_view::View {.origin = k_Origin, .focus = k_Focus});
	auto* view = fight.get();
	view->SetPlayerModel(camera.SetModel(std::move(fight)));
	auto* world = world_camera::PlayersWorldModel(camera.GetModel());
	ASSERT_NE(world, nullptr);
	world_camera::EndFrameControls(true);
	EXPECT_TRUE(world->InputsOff());
	// The fight view starts again as the player comes out of the temple: the player's camera takes the controls again
	view->StartAgain();
	EXPECT_FALSE(world->InputsOff());
}

TEST_F(CameraModesBehindTemple, BehindTheTempleTheCameraOfTheWorldTakesNoControlsWithOrWithoutABox)
{
	GoIn();
	auto& world = dynamic_cast<DefaultWorldCameraModel&>(_behind->GetModel());
	world_camera::EndFrameControls(false);
	EXPECT_TRUE(world.InputsOff());
}

TEST_F(CameraModesBehindTemple, TheFollowCameraContinuesBehindTheTempleAndStaysWhenThePlayerComesOut)
{
	ASSERT_TRUE(_mode.Enter(_creature, creature_follow::StartDistance::KeepWhenClose));
	const auto* follow = &Locator::camera::value().GetModel();
	GoIn();
	RunFrames(300);
	// The mode keeps the camera of the world behind the temple, which goes to the creature
	EXPECT_TRUE(_mode.IsActive());
	EXPECT_EQ(&_behind->GetModel(), follow);
	EXPECT_TRUE(_mode.GetView().has_value());
	EXPECT_TRUE(LooksAtTheCreature(*_behind));
	ComeOut();
	RunFrames(1);
	EXPECT_TRUE(_mode.IsActive());
	EXPECT_EQ(&Locator::camera::value().GetModel(), follow);
}

TEST_F(CameraModesBehindTemple, InsideTheTempleTheMouseButtonOfZoomToCreatureMakesTheFollowCamera)
{
	GoIn();
	const auto* temple = &Locator::camera::value().GetModel();
	// The key does nothing inside the temple
	_actions->pressed = true;
	_actions->byMouse = false;
	RunFrames(1);
	EXPECT_FALSE(_mode.IsActive());
	// The middle button locks the camera of the world behind the temple onto the creature
	_actions->byMouse = true;
	RunFrames(1);
	_actions->pressed = false;
	EXPECT_TRUE(_mode.IsActive());
	EXPECT_NE(dynamic_cast<const CreatureCameraModel*>(&_behind->GetModel()), nullptr);
	// The temple keeps its own camera
	EXPECT_EQ(&Locator::camera::value().GetModel(), temple);
	RunFrames(300);
	EXPECT_TRUE(LooksAtTheCreature(*_behind));
	// The follow camera stays when the player comes out
	ComeOut();
	RunFrames(1);
	EXPECT_TRUE(_mode.IsActive());
	EXPECT_NE(dynamic_cast<const CreatureCameraModel*>(&Locator::camera::value().GetModel()), nullptr);
}

TEST_F(CameraModesBehindTemple, InsideTheTempleASecondPressOfTheMouseButtonGivesTheCameraOfTheWorldBack)
{
	GoIn();
	_actions->pressed = true;
	_actions->byMouse = true;
	RunFrames(1);
	ASSERT_TRUE(_mode.IsActive());
	RunFrames(1);
	_actions->pressed = false;
	EXPECT_FALSE(_mode.IsActive());
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_behind->GetModel()), nullptr);
}

TEST_F(CameraModesBehindTemple, TheCinemaBarsOfAScriptDoNotEndTheFollowCamera)
{
	auto& director = Locator::cinematicDirectorSystem::emplace<ecs::systems::CinematicDirectorSystem>();
	ASSERT_TRUE(_mode.Enter(_creature, creature_follow::StartDistance::KeepWhenClose));
	const auto* follow = &Locator::camera::value().GetModel();
	// The bars of a script put the interface away. The follow camera continues and goes to the creature.
	director.SetWideScreen(true, 7);
	ASSERT_FALSE(director.IsInterfaceActive());
	RunFrames(300);
	EXPECT_TRUE(_mode.IsActive());
	EXPECT_EQ(&Locator::camera::value().GetModel(), follow);
	EXPECT_TRUE(LooksAtTheCreature(Locator::camera::value()));
	// The follow camera stays when the bars go
	director.SetWideScreen(false, 0);
	RunFrames(1);
	EXPECT_TRUE(_mode.IsActive());
	EXPECT_EQ(&Locator::camera::value().GetModel(), follow);
}

TEST_F(CameraModesBehindTemple, TheGripOfTheLandEndsTheFollowCameraOnlyWhenTheCameraReadsItsKeys)
{
	ASSERT_TRUE(_mode.Enter(_creature, creature_follow::StartDistance::KeepWhenClose));
	// Without the interface, or while a box has the keyboard, the camera does not read the grip
	RunFrames(1, {.handGripping = true, .readsKeys = false});
	EXPECT_TRUE(_mode.IsActive());
	RunFrames(1, {.handGripping = false, .readsKeys = false});
	// A fresh grip, read by the camera, gives the player the camera back
	RunFrames(1, {.handGripping = true, .readsKeys = true});
	EXPECT_FALSE(_mode.IsActive());
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&Locator::camera::value().GetModel()), nullptr);
}
