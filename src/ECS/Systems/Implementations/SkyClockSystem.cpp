/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SkyClockSystem.h"

#include "ECS/Registry.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

SkyClockSystem::SkyClockSystem()
{
	Initialize();
}

void SkyClockSystem::Initialize()
{
	_entities = archetypes::SkyArchetype::Create();
	if (!_kept.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Get<SkyDome>(_entities.dome) = _kept->dome;
	registry.Get<DayNightCycle>(_entities.dome) = _kept->cycle;
	registry.Get<Sun>(_entities.sun) = _kept->sun;
	registry.Get<Moon>(_entities.moon) = _kept->moon;
	_kept.reset();
}

void SkyClockSystem::KeepForNextLand()
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(_entities.dome) || !registry.Valid(_entities.sun) || !registry.Valid(_entities.moon))
	{
		return;
	}
	_kept = Kept {
	    .dome = registry.Get<SkyDome>(_entities.dome),
	    .cycle = registry.Get<DayNightCycle>(_entities.dome),
	    .sun = registry.Get<Sun>(_entities.sun),
	    .moon = registry.Get<Moon>(_entities.moon),
	};
}

void SkyClockSystem::FindAfterLoad()
{
	auto& registry = Locator::entitiesRegistry::value();
	_entities = {};
	_kept.reset();
	registry.Each<const SkyDome>([this](entt::entity entity, const SkyDome& /*dome*/) { _entities.dome = entity; });
	registry.Each<const Sun>([this](entt::entity entity, const Sun& /*sun*/) { _entities.sun = entity; });
	registry.Each<const Moon>([this](entt::entity entity, const Moon& /*moon*/) { _entities.moon = entity; });
}

void SkyClockSystem::ProcessTurn()
{
	GetClock().ProcessTurn();
}

void SkyClockSystem::SetTime(float time)
{
	GetClock().SetScriptTime(time);
	// The shown dome jumps to the new sky at once. A computer that does not show the sky has no dome to build.
	if (Locator::skySystem::has_value())
	{
		Locator::skySystem::value().JumpToClock();
	}
}

float SkyClockSystem::GetCurrentSkyType() const
{
	const auto& clock = GetClock();
	return clock.SkyType(clock.GetVisualTime());
}

DayNightClock& SkyClockSystem::GetClock()
{
	return Locator::entitiesRegistry::value().Get<DayNightCycle>(_entities.dome).clock;
}

const DayNightClock& SkyClockSystem::GetClock() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Get<DayNightCycle>(_entities.dome).clock;
}
