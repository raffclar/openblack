/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FilmCreatureSystem.h"

#include <exception>
#include <filesystem>

#include <entt/core/hashed_string.hpp>
#include <glm/mat3x3.hpp>
#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureLayers.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/FilmCreature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
/// The film's camera path
const std::filesystem::path k_PathFile {"Data/Spells/fall/fall.cm2"};
constexpr entt::hashed_string k_PathId {"film/fall"};

std::shared_ptr<const FilmCameraPath> LoadPath()
{
	auto& paths = Locator::resources::value().GetFilmCameraPaths();
	if (paths.Contains(k_PathId.value()))
	{
		return paths.Handle(k_PathId.value()).handle();
	}
	try
	{
		const auto [it, loaded] = paths.Load(k_PathId.value(), resources::FilmCameraPathLoader::FromDiskTag {}, k_PathFile);
		return it->second ? static_cast<std::shared_ptr<const FilmCameraPath>>(it->second.handle()) : nullptr;
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Can't read the film camera path {}: {}", k_PathFile.generic_string(),
		                   error.what());
		return nullptr;
	}
}
} // namespace

void FilmCreatureSystem::Start()
{
	End();
	if (!Locator::playerSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& players = Locator::playerSystem::value();
	const auto creature = players.GetPrimaryCreature(players.GetLocalPlayer());
	auto& registry = Locator::entitiesRegistry::value();
	if (!creature.has_value() || !registry.Valid(*creature) || !registry.AllOf<Creature, Mesh, CreatureMorph>(*creature))
	{
		return;
	}
	const auto& body = registry.Get<const Creature>(*creature);
	const auto size = ShownSize(body);

	// The copy stands at the world's origin, facing as its mesh does, at the creature's size
	const auto copy = registry.Create();
	registry.Assign<Transform>(copy, glm::vec3(0.0f), glm::mat3(1.0f),
	                           glm::vec3(archetypes::CreatureArchetype::DrawnScale(body.species, size)));
	registry.Assign<CreatureCopy>(copy, *creature, size);
	auto& animation = registry.Assign<CreatureAnimation>(copy);
	registry.Assign<CreatureEyes>(copy);
	// It has hair of its own
	registry.Assign<CreatureHair>(copy);
	registry.Assign<FilmCreature>(copy);
	_copy = copy;
	_path = LoadPath();
	_lastFrame.reset();

	// It is posed a moment before it starts its action, on the right side and at the pace of real time. It looks
	// nowhere.
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().PoseCopy(copy, static_cast<float>(film_creature::k_FirstPoseMs));
	}
	if (auto action = creature_layers::PlayOnce(animation.body, film_creature::k_Action, false); action.has_value())
	{
		action->realTime = true;
		animation.body = *action;
	}
	animation.lookAt.reset();
}

void FilmCreatureSystem::Update(int32_t filmMs, std::chrono::steady_clock::time_point now)
{
	const auto frameRealTime = _lastFrame.has_value() ? std::chrono::duration_cast<std::chrono::milliseconds>(now - *_lastFrame)
	                                                  : std::chrono::milliseconds(0);
	_lastFrame = now;
	if (!_copy.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto* film = registry.TryGet<FilmCreature>(*_copy);
	if (film == nullptr)
	{
		return;
	}

	// Its pose moves on with the film's time
	if (const auto step = film_creature::Step(film->clock, filmMs); step.has_value())
	{
		const auto milliseconds = static_cast<float>(*step);
		if (Locator::creatureAnimationSystem::has_value())
		{
			Locator::creatureAnimationSystem::value().PoseCopy(*_copy, milliseconds);
		}
		if (Locator::creatureHairSystem::has_value())
		{
			Locator::creatureHairSystem::value().StepCopy(*_copy, std::chrono::duration<float, std::milli>(milliseconds));
		}
	}

	// The camera is where its path is at the film's time
	if (_path != nullptr)
	{
		_view = film_creature::View(_path->SampleAt(static_cast<uint32_t>(filmMs)));
	}

	// Each frame that shows it, its colours flicker
	film->shown = film_creature::Shown(filmMs);
	if (film->shown && (!film->look.has_value() || frame_pace::Due(film->pace, frameRealTime)))
	{
		auto& random = Locator::gameRandom::value();
		const auto red = random.LocalRand(32);
		const auto green = random.LocalRand(8);
		const auto blue = random.LocalRand(4);
		film->look = film_creature::LookAt(film_creature::Brightness(filmMs), red, green, blue);
	}
}

void FilmCreatureSystem::End()
{
	if (_copy.has_value() && Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(*_copy))
		{
			registry.Destroy(*_copy);
		}
	}
	_copy.reset();
	_path.reset();
	_lastFrame.reset();
}

std::optional<FilmCreatureSystemInterface::Shown> FilmCreatureSystem::GetShown() const
{
	if (!_copy.has_value() || !Locator::entitiesRegistry::has_value() || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto* film = Locator::entitiesRegistry::value().TryGet<const FilmCreature>(*_copy);
	if (film == nullptr || !film->shown || !film->look.has_value())
	{
		return std::nullopt;
	}
	const auto channels = [](uint32_t colour) {
		return glm::vec3(static_cast<float>((colour >> 16u) & 0xFFu), static_cast<float>((colour >> 8u) & 0xFFu),
		                 static_cast<float>(colour & 0xFFu));
	};
	return Shown {
	    .copy = *_copy,
	    .view = _view,
	    .projection = film_creature::Projection(Locator::camera::value().GetProjectionMatrix()),
	    .light = film_creature::k_Light,
	    .colour = channels(film->look->colour),
	    .added = channels(film->look->added) / 255.0f,
	};
}
