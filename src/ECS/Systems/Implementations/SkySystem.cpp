/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SkySystem.h"

#include <chrono>

#include "3D/LandLightFrame.h"
#include "3D/LandLightTable.h"
#include "3D/SkyFrame.h"
#include "Common/MachineClock.h"
#include "ECS/Registry.h"
#include "EngineConfig.h"
#include "Graphics/DetailLevel.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

SkySystem::SkySystem()
{
	Initialize();
}

void SkySystem::Initialize()
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

void SkySystem::KeepForNextLand()
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

void SkySystem::ProcessTurn()
{
	GetClock().ProcessTurn();
}

void SkySystem::UpdateFrame(bool skyDrawn)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& dome = registry.Get<SkyDome>(_entities.dome);
	const auto& clock = GetClock();

	// The land's light of the frame, which the renderer builds its light table from, and its palette, which gives the
	// moon its colour
	const LandLightPalette* palette = nullptr;
	if (const auto& palettes = Locator::resources::value().GetLandLightPalettes();
	    palettes.Contains(LandLightPalette::k_Id.value()))
	{
		palette = &*palettes.Handle(LandLightPalette::k_Id.value());
	}
	// The date as the game reads it: the wall clock's, or the one a seeded run pinned
	const auto now = std::chrono::seconds(machine_clock::UnixTime());
	sky_frame::Update(
	    {
	        .scriptHour = clock.GetScriptTime(),
	        .unixTime = now.count(),
	        .landLight = FrameLandLightInputs(),
	        .palette = palette,
	        .fog = graphics::detail_level::Fog(Locator::config::value().detailLevel),
	    },
	    dome, registry.Get<Sun>(_entities.sun), registry.Get<Moon>(_entities.moon));
	sky_frame::AdvanceDome(dome, GetCurrentSkyType(), skyDrawn);
}

void SkySystem::SetTime(float time)
{
	GetClock().SetScriptTime(time);
	Locator::entitiesRegistry::value().Get<SkyDome>(_entities.dome).follow.Jump(GetCurrentSkyType());
}

float SkySystem::GetCurrentSkyType() const
{
	const auto& clock = GetClock();
	return clock.SkyType(clock.GetVisualTime());
}

DayNightClock& SkySystem::GetClock()
{
	return Locator::entitiesRegistry::value().Get<DayNightCycle>(_entities.dome).clock;
}

const DayNightClock& SkySystem::GetClock() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Get<DayNightCycle>(_entities.dome).clock;
}
