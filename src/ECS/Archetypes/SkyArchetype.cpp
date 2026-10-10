/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkyArchetype.h"

#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

SkyArchetype::Entities SkyArchetype::Create()
{
	auto& registry = Locator::entitiesRegistry::value();
	Entities entities {
	    .dome = registry.Create(),
	    .sun = registry.Create(),
	    .moon = registry.Create(),
	};

	DayNightClock clock;
	clock.Reset();
	const auto skyType = clock.SkyType(clock.GetVisualTime());
	registry.Assign<DayNightCycle>(entities.dome, clock);
	registry.Assign<SkyDome>(entities.dome, SkyDome {
	                                            .meshId = k_DomeMeshId.value(),
	                                            .textureId = k_DomeTextureId.value(),
	                                            .follow = sky_dome::Follow(skyType),
	                                        });

	registry.Assign<CelestialBody>(entities.sun, CelestialBody {
	                                                 .meshId = k_SunMeshId.value(),
	                                                 .textureId = k_SunTextureId.value(),
	                                                 .alphaTextureId = k_SunTextureId.value(),
	                                             });
	registry.Assign<Sun>(entities.sun);

	registry.Assign<CelestialBody>(entities.moon, CelestialBody {
	                                                  .meshId = k_MoonMeshId.value(),
	                                                  .textureId = k_MoonTextureId.value(),
	                                                  .alphaTextureId = k_MoonAlphaTextureId.value(),
	                                              });
	registry.Assign<CelestialGlow>(entities.moon, CelestialGlow {
	                                                  .textureId = k_GlowTextureId.value(),
	                                                  .alphaTextureId = k_GlowAlphaTextureId.value(),
	                                              });
	registry.Assign<Moon>(entities.moon);
	return entities;
}
