/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureModeSystem.h"

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/DanceCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "ECS/Components/Creature.h"
#include "ECS/FollowHeadingThing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::ecs::systems
{

namespace
{
bool TempleActive()
{
	return Locator::temple::has_value() && Locator::temple::value().Active();
}
} // namespace

CreatureModeSystem::CreatureModeSystem() = default;
CreatureModeSystem::~CreatureModeSystem() = default;

std::optional<entt::entity> CreatureModeSystem::PlayersCreature() const
{
	// The player's primary creature, the earliest they got that is still theirs
	if (!Locator::playerSystem::has_value())
	{
		return std::nullopt;
	}
	// C locks onto the creature of the player whose hand this computer moves
	const auto player =
	    Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
	return Locator::playerSystem::value().GetPrimaryCreature(player);
}

std::optional<creature_follow::View> CreatureModeSystem::GetView() const
{
	return OwnsCamera() ? std::optional(_model->GetView()) : std::nullopt;
}

bool CreatureModeSystem::OwnsCamera() const
{
	return _model != nullptr && Locator::camera::has_value() && &Locator::camera::value().GetModel() == _model;
}

void CreatureModeSystem::ReleaseCamera()
{
	if (OwnsCamera() && _playerModel != nullptr)
	{
		Locator::camera::value().SetModel(std::move(_playerModel));
	}
	_model = nullptr;
	_playerModel.reset();
	_leaving = false;
	_held = {};
}

std::optional<entt::entity> CreatureModeSystem::GetCreature() const
{
	if (!_thing.has_value() || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(*_thing) && registry.AllOf<components::Creature>(*_thing) ? _thing : std::nullopt;
}

bool CreatureModeSystem::ThingThere() const
{
	if (!_thing.has_value() || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	return CameraTargetOf(Locator::entitiesRegistry::value(), *_thing).has_value();
}

bool CreatureModeSystem::StillValid() const
{
	if (!ThingThere())
	{
		return false;
	}
	// A script's cinema bars take the camera
	return !Locator::cinematicDirectorSystem::has_value() || Locator::cinematicDirectorSystem::value().IsInterfaceActive();
}

bool CreatureModeSystem::Enter(entt::entity creature, creature_follow::StartDistance start)
{
	// The player locks onto creatures only
	if (!Locator::entitiesRegistry::has_value() || !Locator::entitiesRegistry::value().Valid(creature) ||
	    !Locator::entitiesRegistry::value().AllOf<components::Creature>(creature))
	{
		return false;
	}
	const auto taken = Take(creature, [start](const Camera& camera, glm::vec3 position, float height) {
		// The angles come from the points that the camera glides to, and the distance from the eye now
		const creature_follow::StartPoints points {
		    .origin = camera.GetOrigin(),
		    .targetOrigin = camera.GetOrigin(Camera::Interpolation::Target),
		    .targetFocus = camera.GetFocus(Camera::Interpolation::Target),
		};
		return std::make_unique<CreatureCameraModel>(points, position, height, start);
	});
	if (taken)
	{
		// The button that double clicked is still down: only a grip of the land after this one gives the camera back
		_wasGripping = true;
	}
	return taken;
}

bool CreatureModeSystem::EnterLoaded(entt::entity thing, const creature_follow::Loaded& loaded)
{
	const auto taken = Take(thing, [&loaded](const Camera& /*camera*/, glm::vec3 position, float height) {
		return std::make_unique<CreatureCameraModel>(loaded, position, height);
	});
	if (taken)
	{
		// No button took the camera
		_wasGripping = false;
	}
	return taken;
}

bool CreatureModeSystem::Take(entt::entity thing, const MakeModel& make)
{
	if (!Locator::camera::has_value() || !Locator::entitiesRegistry::has_value() || TempleActive())
	{
		return false;
	}
	const auto target = CameraTargetOf(Locator::entitiesRegistry::value(), thing);
	if (!target.has_value())
	{
		return false;
	}
	auto& camera = Locator::camera::value();
	if (OwnsCamera())
	{
		// Already locked on, it moves over to the other thing from where the camera is
		ReleaseCamera();
	}
	// Only the player's own camera and the camera of a dance are taken over. The temple's camera is not, and the editor's
	// camera is not while it follows what it picked.
	if (dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()) == nullptr &&
	    dynamic_cast<DanceCameraModel*>(&camera.GetModel()) == nullptr)
	{
		_thing.reset();
		return false;
	}
	auto model = make(camera, target->position, target->height);
	model->SetTarget(*target);
	_model = model.get();
	_returnOrigin = camera.GetModel().GetTargetOrigin();
	_returnFocus = camera.GetModel().GetTargetFocus();
	_playerModel = camera.SetModel(std::move(model));
	_thing = thing;
	_leaving = false;
	return true;
}

void CreatureModeSystem::Leave()
{
	_thing.reset();
	if (_model == nullptr)
	{
		return;
	}
	if (OwnsCamera())
	{
		ReleaseCamera();
		return;
	}
	// The temple or the editor took the camera from this mode's: it is given back once they return it
	_leaving = true;
}

void CreatureModeSystem::PressCreatureKey()
{
	const auto yours = PlayersCreature();
	switch (creature_mode::OnCreatureKey(_thing, yours))
	{
	case creature_mode::KeyAction::Enter:
		Enter(*yours, creature_follow::StartDistance::KeepWhenClose);
		break;
	case creature_mode::KeyAction::Leave:
		Leave();
		break;
	case creature_mode::KeyAction::None:
		break;
	}
}

bool CreatureModeSystem::Press(const creature_mode::Press& press)
{
	const auto creature = _doubleClicks.OnPress(press);
	return creature.has_value() && Enter(*creature, creature_follow::StartDistance::KeepWhenClose);
}

void CreatureModeSystem::HoldKeys(int across, int along, bool shift, bool ctrl, float seconds)
{
	_held = {.across = across, .along = along, .shift = shift, .ctrl = ctrl, .seconds = seconds};
}

void CreatureModeSystem::ClearView()
{
	if (!OwnsCamera() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	_model->ClearView({
	    .height = [&land](glm::vec2 point) { return land.GetHeightAt(point); },
	    .normal = [&land](glm::vec2 point) { return land.GetNormalAt(point); },
	});
}

void CreatureModeSystem::Update(std::chrono::microseconds dt, const Frame& frame)
{
	if (!Locator::camera::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	// The camera was handed back to this mode after leaving was asked for
	if (_leaving && OwnsCamera())
	{
		ReleaseCamera();
	}

	// C locks onto the player's creature, and lets go of it again
	if (Locator::gameActionSystem::has_value() && !TempleActive())
	{
		using input::BindableActionMap;
		const auto& actions = Locator::gameActionSystem::value();
		if (actions.Get(BindableActionMap::ZOOM_TO_CREATURE) && actions.GetChanged(BindableActionMap::ZOOM_TO_CREATURE))
		{
			PressCreatureKey();
		}
	}

	if (!_thing.has_value())
	{
		return;
	}
	// The temple and the editor take the camera, and the camera stays with them until they hand it back
	if (TempleActive())
	{
		Leave();
		return;
	}
	if (!OwnsCamera())
	{
		return;
	}
	// The cursor keys alone, or the hand taking a fresh grip of the land, give the player the camera back
	const bool gripped = frame.handGripping && !_wasGripping;
	_wasGripping = frame.handGripping;
	if (!StillValid() || _model->WantsToLeave() || gripped)
	{
		// Stopped at the fence, the player's camera starts afresh from where it is
		auto& camera = Locator::camera::value();
		if (_model->StoppedAtFence())
		{
			_playerModel = std::make_unique<DefaultWorldCameraModel>(camera.GetOrigin(), camera.GetFocus());
		}
		// When the thing has gone, the player's camera takes the camera again and flies back to where it was going.
		// The keys and the grip start the player's camera from where the camera is.
		const bool gone = !ThingThere();
		const auto returnOrigin = _returnOrigin;
		const auto returnFocus = _returnFocus;
		Leave();
		if (gone)
		{
			if (auto* world = dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()); world != nullptr)
			{
				world->Restart(camera.GetOrigin(), camera.GetFocus(), returnOrigin, returnFocus);
			}
		}
		return;
	}

	if (const auto target = CameraTargetOf(Locator::entitiesRegistry::value(), *_thing); target.has_value())
	{
		_model->SetTarget(*target);
	}

	if (_held.seconds > 0.0f)
	{
		const auto seconds = std::min(std::chrono::duration<float>(dt).count(), _held.seconds);
		_held.seconds -= seconds;
		auto keys = creature_follow::KeysFor(_held.across, _held.along, seconds);
		keys.shift = _held.shift;
		keys.ctrl = _held.ctrl;
		const auto width = Locator::windowing::has_value() ? static_cast<float>(Locator::windowing::value().GetSize().x) : 1.0f;
		if (_model->Turn(keys, width) == creature_follow::KeyOutcome::Leave)
		{
			Leave();
		}
	}
}

} // namespace openblack::ecs::systems
