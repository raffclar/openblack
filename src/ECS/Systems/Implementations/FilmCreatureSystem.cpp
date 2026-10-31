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
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/FilmCreature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
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
	if (!Locator::playerSystem::has_value() || !Locator::entitiesRegistry::has_value() || !Locator::drawnRegistry::has_value())
	{
		return;
	}
	const auto& players = Locator::playerSystem::value();
	const auto creature = players.GetPrimaryCreature(
	    (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE));
	const auto& land = Locator::entitiesRegistry::value();
	if (!creature.has_value() || !land.Valid(*creature) || !land.AllOf<Creature, Mesh, CreatureMorph>(*creature))
	{
		return;
	}
	const auto& body = land.Get<const Creature>(*creature);
	const auto size = ShownSize(body);

	// The copy stands at the world's origin, facing as its mesh does, at the creature's size. It is a thing of the drawn
	// registry, so that the film takes no entity of the land.
	auto& registry = Locator::drawnRegistry::value();
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
		Locator::creatureAnimationSystem::value().PoseCopy(registry, copy, static_cast<float>(film_creature::k_FirstPoseMs),
		                                                   Locator::entitiesRegistry::value());
	}
	if (auto action = creature_layers::PlayOnce(animation.body, film_creature::k_Action, false); action.has_value())
	{
		action->realTime = true;
		animation.body = *action;
	}
	animation.lookAt.reset();

	// It makes its first motes as it is first posed
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().AddAlignmentMotesOf(registry, copy);
	}
	// It has a glow on each hand
	auto& glows = registry.Assign<CreatureHandGlows>(copy);
	for (const auto hand : {creature_hand_glow::Place::LeftHand, creature_hand_glow::Place::RightHand})
	{
		creature_hand_glow::Set(glows.glows, hand, film_creature::k_GlowScale, film_creature::k_StartGlowPower);
	}

	// The stars and the rays of the burst of light are made now. The rays are made twice, and the second rays are kept.
	if (Locator::gameRandom::has_value())
	{
		auto& random = Locator::gameRandom::value();
		const film_effects::CrtRandom crt = [&random](float a, float b) { return random.CrtRandom(a, b); };
		const film_effects::LocalRandom local = [&random](int32_t n) { return random.LocalRand(n); };
		const film_effects::LocalFloatRandom localFloat = [&random](float x) { return random.LocalFloatRand(x); };
		auto& film = registry.Get<FilmCreature>(copy);
		film.stars = film_effects::MakeStars(crt);
		for (int i = 0; i < 2; ++i)
		{
			film.burst.rays = film_effects::MakeRays(localFloat, crt, local);
		}
	}
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
	auto& registry = Locator::drawnRegistry::value();
	auto* film = registry.Valid(*_copy) ? registry.TryGet<FilmCreature>(*_copy) : nullptr;
	if (film == nullptr)
	{
		return;
	}

	// The stars, the burst and the motes move on by the real time, at most once a frame of the screen's refresh
	const auto stepMs = static_cast<int32_t>(frame_pace::Step(film->effectsPace, frameRealTime, frameRealTime).count());

	// Its pose moves on with the film's time. It makes its motes from its last pose first, if the film showed it.
	if (const auto step = film_creature::Step(film->clock, filmMs); step.has_value())
	{
		const auto milliseconds = static_cast<float>(*step);
		if (Locator::creatureAnimationSystem::has_value())
		{
			auto& animations = Locator::creatureAnimationSystem::value();
			if (film->shown)
			{
				animations.AddAlignmentMotesOf(registry, *_copy);
			}
			animations.PoseCopy(registry, *_copy, milliseconds, Locator::entitiesRegistry::value());
		}
		if (Locator::creatureHairSystem::has_value())
		{
			Locator::creatureHairSystem::value().StepCopy(
			    registry, *_copy, std::chrono::duration<float, std::milli>(milliseconds), Locator::entitiesRegistry::value());
		}
	}

	// The stars start a time into the film, and the burst of light later
	if (!film->starsStarted && filmMs > film_effects::k_StarsFromMs)
	{
		film->starsStarted = true;
		film->starsOn = true;
	}
	if (film->starsStarted && !film->burstOn && filmMs > film_effects::k_BurstFromMs)
	{
		film->burstOn = true;
	}

	// The camera is where its path is at the film's time
	if (_path != nullptr)
	{
		_view = film_creature::View(_path->SampleAt(static_cast<uint32_t>(filmMs)));
	}

	// Each frame that shows it, its colours flicker, its hands glow by the power that they had, and then the glows take
	// their new powers
	film->shown = film_creature::Shown(filmMs);
	if (film->shown && (!film->look.has_value() || frame_pace::Due(film->pace, frameRealTime)))
	{
		auto& random = Locator::gameRandom::value();
		const auto red = random.LocalRand(32);
		const auto green = random.LocalRand(8);
		const auto blue = random.LocalRand(4);
		film->look = film_creature::LookAt(film_creature::Brightness(filmMs), red, green, blue);
	}
	if (auto* glows = registry.TryGet<CreatureHandGlows>(*_copy); film->shown && glows != nullptr)
	{
		if (Locator::creatureAnimationSystem::has_value())
		{
			Locator::creatureAnimationSystem::value().PlaceHandGlows(registry, *_copy, stepMs, 0);
		}
		for (const auto hand : {creature_hand_glow::Place::LeftHand, creature_hand_glow::Place::RightHand})
		{
			if (const auto power = film_creature::GlowPower(hand, filmMs); power.has_value())
			{
				creature_hand_glow::Set(glows->glows, hand, film_creature::k_GlowScale, *power);
			}
		}
	}

	// The stars fall and fade until the last one has faded
	if (film->starsOn && stepMs > 0)
	{
		film->starSprites.clear();
		for (size_t i = 0; i < film->stars.size(); ++i)
		{
			if (const auto sprite = film_effects::StepStar(film->stars.at(i), stepMs, static_cast<int32_t>(i));
			    sprite.has_value())
			{
				film->starSprites.push_back(*sprite);
			}
		}
		film->starsOn = !film->starSprites.empty();
	}

	// The motes, and the other puffs, live by the real time while the film pauses the game
	if (stepMs > 0 && Locator::liquidParticleSystem::has_value())
	{
		Locator::liquidParticleSystem::value().UpdateFrame(static_cast<float>(stepMs) * 0.001f, false);
	}

	// The burst of light is round the middle of the copy's body, when that is in front of the camera
	film->fans.clear();
	if (film->burstOn && Locator::creatureAnimationSystem::has_value() && Locator::camera::has_value())
	{
		const auto middle = Locator::creatureAnimationSystem::value().MiddleOfGroinAndLips(registry, *_copy);
		const auto& camera = Locator::camera::value();
		const auto projection = film_creature::Projection(camera.GetProjectionMatrix());
		const auto inCamera = middle.has_value() ? glm::vec3(_view * glm::vec4(*middle, 1.0f)) : glm::vec3(0.0f);
		if (middle.has_value() && inCamera.z >= camera.GetNearClip())
		{
			const glm::vec2 screen {inCamera.x * projection[0][0] / inCamera.z, inCamera.y * projection[1][1] / inCamera.z};
			const auto centre = film_effects::PixelOf(screen);
			if (stepMs > 0)
			{
				film->burstAlpha = film_effects::StepBurst(film->burst, stepMs);
			}
			for (const auto& fan : film_effects::Fans(film->burst, film->burstAlpha))
			{
				film->fans.push_back(film_effects::FanCorners(film->burst, fan, centre));
			}
		}
	}
}

void FilmCreatureSystem::End()
{
	if (_copy.has_value() && Locator::drawnRegistry::has_value())
	{
		auto& registry = Locator::drawnRegistry::value();
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
	if (!_copy.has_value() || !Locator::drawnRegistry::has_value() || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::drawnRegistry::value();
	const auto* film = registry.Valid(*_copy) ? registry.TryGet<const FilmCreature>(*_copy) : nullptr;
	if (film == nullptr)
	{
		return std::nullopt;
	}
	const auto channels = [](uint32_t colour) {
		return glm::vec3(static_cast<float>((colour >> 16u) & 0xFFu), static_cast<float>((colour >> 8u) & 0xFFu),
		                 static_cast<float>(colour & 0xFFu));
	};
	const auto& camera = Locator::camera::value();
	Shown shown {
	    .copy = *_copy,
	    .copyShown = film->shown && film->look.has_value(),
	    .view = _view,
	    .projection = film_creature::Projection(camera.GetProjectionMatrix()),
	    .light = film_creature::k_Light,
	    .stars = film->starSprites,
	    .fans = film->fans,
	    .fanDepth = camera.GetNearClip() * film_effects::k_FanDepth,
	};
	if (film->look.has_value())
	{
		shown.colour = channels(film->look->colour);
		shown.added = channels(film->look->added) / 255.0f;
	}
	return shown;
}
