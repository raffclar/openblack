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
#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/DetailLevel.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

void SkySystem::UpdateFrame(bool skyDrawn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& skyClock = Locator::skyClockSystem::value();
	const auto& entities = skyClock.GetEntities();
	auto& dome = registry.Get<SkyDome>(entities.dome);
	const auto& clock = skyClock.GetClock();

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
	        .ticks = machine_clock::Ticks(),
	        .landLight = FrameLandLightInputs(),
	        .palette = palette,
	        .fog = Locator::config::value().detail.fog,
	    },
	    dome, registry.Get<Sun>(entities.sun), registry.Get<Moon>(entities.moon));
	sky_frame::AdvanceDome(dome, skyClock.GetCurrentSkyType(), skyDrawn);
}

void SkySystem::JumpToClock()
{
	const auto& skyClock = Locator::skyClockSystem::value();
	Locator::entitiesRegistry::value().Get<SkyDome>(skyClock.GetEntities().dome).follow.Jump(skyClock.GetCurrentSkyType());
}

entt::entity SkySystem::GetDome() const
{
	return Locator::skyClockSystem::value().GetEntities().dome;
}

entt::entity SkySystem::GetSun() const
{
	return Locator::skyClockSystem::value().GetEntities().sun;
}

entt::entity SkySystem::GetMoon() const
{
	return Locator::skyClockSystem::value().GetEntities().moon;
}
