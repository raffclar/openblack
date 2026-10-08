/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Creature Mode's system on fakes: C and the double click lock onto a creature, the camera is taken from the player's
// model and handed back as it was, and the cursor keys, a fresh grip, the creature going, a script's camera and a
// script's cinema bars end it. Its input only comes from real keys and mouse, which no recorded run presses, so these
// tests stand in for them.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <chrono>
#include <initializer_list>
#include <memory>
#include <optional>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/CreatureFollow.h"
#include "Camera/ScriptCamera.h"
#include "Creature/CreatureMode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/CreatureModeSystem.h"
#include "ECS/Systems/Implementations/InputState.h"
#include "ECS/Systems/Implementations/ScriptState.h"
#include "Input/GameActionMapInterface.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace std::chrono_literals;
using input::BindableActionMap;
using input::UnbindableActionMap;
using openblack::ecs::systems::CreatureModeSystem;
using Frame = openblack::ecs::systems::CreatureModeSystemInterface::Frame;

namespace
{
constexpr glm::vec3 k_CreatureAt {1000.0f, 0.0f, 1000.0f};
constexpr glm::vec3 k_OtherAt {1200.0f, 0.0f, 900.0f};
constexpr auto k_Step = 16ms;

/// The keys and buttons held this frame, and those of them pressed this frame
class FakeActions final: public input::GameActionInterface
{
public:
	void Hold(std::initializer_list<BindableActionMap> held)
	{
		_held = 0;
		for (const auto action : held)
		{
			_held |= static_cast<uint64_t>(action);
		}
		_pressed = 0;
		_doubleClick = false;
		_doubleClickPressed = false;
	}
	/// Pressed this frame: held, and changed
	void Press(BindableActionMap action)
	{
		Hold({});
		_held = static_cast<uint64_t>(action);
		_pressed = _held;
	}
	void DoubleClick(bool pressedThisFrame)
	{
		Hold({});
		_doubleClick = true;
		_doubleClickPressed = pressedThisFrame;
	}

	[[nodiscard]] bool GetBindable(BindableActionMap action) const override
	{
		return (_held & static_cast<uint64_t>(action)) != 0;
	}
	[[nodiscard]] bool GetUnbindable(UnbindableActionMap action) const override
	{
		return action == UnbindableActionMap::DOUBLE_CLICK && _doubleClick;
	}
	[[nodiscard]] bool GetBindableChanged(BindableActionMap action) const override
	{
		return (_pressed & static_cast<uint64_t>(action)) != 0;
	}
	[[nodiscard]] bool GetUnbindableChanged(UnbindableActionMap action) const override
	{
		return action == UnbindableActionMap::DOUBLE_CLICK && _doubleClickPressed;
	}
	[[nodiscard]] bool GetBindableRepeat(BindableActionMap) const override { return false; }
	[[nodiscard]] bool GetUnbindableRepeat(UnbindableActionMap) const override { return false; }
	[[nodiscard]] glm::uvec2 GetMousePosition() const override { return {}; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const override { return {}; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const override { return {}; }
	void Frame() override {}
	void ProcessEvent(const SDL_Event&) override {}

private:
	uint64_t _held {0};
	uint64_t _pressed {0};
	bool _doubleClick {false};
	bool _doubleClickPressed {false};
};

/// A camera with the player's own model, a land with the player's creature and another god's, the keys, and the
/// script's and the interface's state, all put back as they were afterwards. No temple
class CreatureModeSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::scriptState::emplace<ecs::systems::ScriptState>();
		Locator::inputState::emplace<ecs::systems::InputState>();
		Locator::temple::reset();
		_actions = &static_cast<FakeActions&>(Locator::gameActionSystem::emplace<FakeActions>());
		_camera = &Locator::camera::emplace(glm::vec3(0.0f));
		_playerModel = &_camera->GetModel();
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		_yours = MakeCreature(registry, PlayerNames::PLAYER_ONE, k_CreatureAt);
		_theirs = MakeCreature(registry, PlayerNames::PLAYER_TWO, k_OtherAt);
	}

	static entt::entity MakeCreature(ecs::Registry& registry, PlayerNames owner, glm::vec3 at)
	{
		const auto creature = registry.Create();
		registry.Assign<ecs::components::Creature>(creature, ecs::components::Creature {
		                                                         .owner = owner,
		                                                         .species = CreatureType::Cow,
		                                                         .size = 1.0f,
		                                                     });
		registry.Assign<ecs::components::Transform>(creature, at, glm::mat3(1.0f), glm::vec3(1.0f));
		return creature;
	}

	/// The system with the player's creature found by the lookup
	[[nodiscard]] std::unique_ptr<CreatureModeSystem> WithYourCreature() const
	{
		const auto yours = _yours;
		return std::make_unique<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
			if (player == PlayerNames::PLAYER_ONE)
			{
				return yours;
			}
			return std::nullopt;
		});
	}

	[[nodiscard]] bool PlayerHasTheCamera() const { return &_camera->GetModel() == _playerModel; }
	[[nodiscard]] CreatureCameraModel* CreatureCamera() const
	{
		return dynamic_cast<CreatureCameraModel*>(&_camera->GetModel());
	}

	FakeActions* _actions {nullptr};
	Camera* _camera {nullptr};
	const CameraModel* _playerModel {nullptr};
	entt::entity _yours {entt::null};
	entt::entity _theirs {entt::null};

private:
	test::RestoreService<Locator::scriptState> _restoreScript;
	test::RestoreService<Locator::inputState> _restoreInput;
	test::RestoreService<Locator::temple> _restoreTemple;
	test::RestoreService<Locator::gameActionSystem> _restoreActions;
	test::RestoreService<Locator::camera> _restoreCamera;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

TEST_F(CreatureModeSystemTest, WithoutALookupNoPlayerHasACreatureAndCDoesNothing)
{
	// As the game has it until the leash service finds the players' creatures
	CreatureModeSystem mode;
	EXPECT_FALSE(mode.PlayersCreature().has_value());
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode.Update(k_Step, {});
	EXPECT_FALSE(mode.IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(mode.GetView().has_value());
}

TEST_F(CreatureModeSystemTest, CLocksOntoThePlayersCreatureAndLetsGoAgain)
{
	const auto mode = WithYourCreature();
	ASSERT_EQ(mode->PlayersCreature(), _yours);

	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());
	EXPECT_EQ(mode->GetCreature(), _yours);
	ASSERT_NE(CreatureCamera(), nullptr);
	EXPECT_TRUE(mode->GetView().has_value());

	// Still held, it is not pressed again
	_actions->Hold({BindableActionMap::ZOOM_TO_CREATURE});
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());

	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, EnterStartsFromWhereTheCameraIsHeadingAndLeaveHandsThePlayersModelBack)
{
	const auto mode = WithYourCreature();
	const auto origin = _camera->GetOriginZoomer().GetDestination();
	const auto focus = _camera->GetFocusZoomer().GetDestination();
	ASSERT_TRUE(mode->Enter(_yours));
	ASSERT_NE(CreatureCamera(), nullptr);
	const auto height = creature_mode::CreatureHeight(1.0f);
	const auto start = creature_follow::Start(origin, focus, k_CreatureAt, height);
	EXPECT_FLOAT_EQ(CreatureCamera()->GetView().yaw, start.yaw);
	EXPECT_FLOAT_EQ(CreatureCamera()->GetView().pitch, start.pitch);
	EXPECT_FLOAT_EQ(CreatureCamera()->GetView().distance, start.distance);

	mode->Leave();
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(mode->GetView().has_value());
}

TEST_F(CreatureModeSystemTest, EntersOnlyOnACreatureAndOnlyFromThePlayersModel)
{
	const auto mode = WithYourCreature();
	auto& registry = Locator::entitiesRegistry::value();
	const auto stone = registry.Create();
	registry.Assign<ecs::components::Transform>(stone, k_CreatureAt, glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FALSE(mode->Enter(stone));
	EXPECT_TRUE(PlayerHasTheCamera());

	// Another model has the camera, as the editor's or the temple's: it is not taken from it
	auto other = std::make_unique<CreatureCameraModel>(glm::vec3(0.0f, 50.0f, 0.0f), glm::vec3(0.0f), k_OtherAt, 15.0f);
	const auto* otherModel = other.get();
	auto player = _camera->SetModel(std::move(other));
	EXPECT_FALSE(mode->Enter(_yours));
	EXPECT_FALSE(mode->IsActive());
	EXPECT_EQ(&_camera->GetModel(), otherModel);
	_camera->SetModel(std::move(player));
}

TEST_F(CreatureModeSystemTest, FollowsTheCreatureAsItMoves)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));
	const glm::vec3 moved {1010.0f, 4.0f, 1020.0f};
	Locator::entitiesRegistry::value().Get<ecs::components::Transform>(_yours).position = moved;
	mode->Update(k_Step, {});
	ASSERT_NE(CreatureCamera(), nullptr);
	EXPECT_EQ(CreatureCamera()->GetTargetFocus(), creature_follow::Focus(moved, creature_mode::CreatureHeight(1.0f)));
}

TEST_F(CreatureModeSystemTest, TheCursorKeysAloneGiveTheCameraBack)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));

	// With Shift they turn the camera round the creature, and it stays
	_actions->Hold({BindableActionMap::ROTATE_ON, BindableActionMap::MOVE_LEFT});
	const auto yaw = CreatureCamera()->GetView().yaw;
	_camera->HandleActions(k_Step);
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());
	ASSERT_NE(CreatureCamera(), nullptr);
	EXPECT_NE(CreatureCamera()->GetView().yaw, yaw);

	// Alone they ask for the player's camera back
	_actions->Hold({BindableActionMap::MOVE_LEFT});
	_camera->HandleActions(k_Step);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, OnlyAFreshGripOfTheLandGivesTheCameraBack)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));

	// The button that locked on is still down
	mode->Update(k_Step, {.handGripping = true});
	EXPECT_TRUE(mode->IsActive());
	mode->Update(k_Step, {.handGripping = false});
	EXPECT_TRUE(mode->IsActive());
	mode->Update(k_Step, {.handGripping = true});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, TheCreatureGoingEndsIt)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));
	Locator::entitiesRegistry::value().Destroy(_yours);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, AScriptsCameraRefusesIt)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	EXPECT_FALSE(mode->Enter(_yours));
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, AScriptsCameraEndsIt)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));
	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, AScriptsCinemaBarsRefuseAndEndIt)
{
	const auto mode = WithYourCreature();
	interface_active::SetActive(false);
	EXPECT_FALSE(mode->Enter(_yours));
	EXPECT_TRUE(PlayerHasTheCamera());

	interface_active::SetActive(true);
	ASSERT_TRUE(mode->Enter(_yours));
	interface_active::SetActive(false);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, ADoubleClickOnACreatureLocksOntoItBeforeTheCameraMoves)
{
	const auto mode = WithYourCreature();
	// Not on a creature: the player's camera flies there as before
	_actions->DoubleClick(true);
	mode->Update(k_Step, {.handGripping = true});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());

	// On another god's creature, the button that double clicked still down: the camera this frame moves is the
	// creature's
	mode->Update(k_Step, {.handGripping = true, .creatureUnderHand = _theirs, .doubleClickFlies = true});
	EXPECT_EQ(mode->GetCreature(), _theirs);
	EXPECT_NE(CreatureCamera(), nullptr);

	// The button still down is the same double click
	_actions->DoubleClick(false);
	mode->Update(k_Step, {.handGripping = true, .creatureUnderHand = _yours, .doubleClickFlies = true});
	EXPECT_EQ(mode->GetCreature(), _theirs);
}

TEST_F(CreatureModeSystemTest, ADoubleClickDoesNotLockOnWhenItsFlightIsNotAllowed)
{
	const auto mode = WithYourCreature();
	// The script has turned the double click's flight off: the lock-on, part of it, is off too
	_actions->DoubleClick(true);
	mode->Update(k_Step, {.handGripping = true, .creatureUnderHand = _theirs, .doubleClickFlies = false});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, MovingOverToAnotherCreatureStillHandsThePlayersModelBack)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_theirs));
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_EQ(mode->GetCreature(), _yours);

	mode->Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, LeavingWhileAnotherHasTheCameraWaitsForItBack)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));
	// Another model takes the camera from the creature's, as the temple's does
	auto creatureModel = _camera->SetModel(
	    std::make_unique<CreatureCameraModel>(glm::vec3(0.0f, 50.0f, 0.0f), glm::vec3(0.0f), k_OtherAt, 15.0f));
	mode->Leave();
	EXPECT_FALSE(mode->IsActive());
	mode->Update(k_Step, {});
	EXPECT_FALSE(PlayerHasTheCamera());

	// Given back to the creature's model, it goes back to the player's
	_camera->SetModel(std::move(creatureModel));
	mode->Update(k_Step, {});
	EXPECT_TRUE(PlayerHasTheCamera());
}
