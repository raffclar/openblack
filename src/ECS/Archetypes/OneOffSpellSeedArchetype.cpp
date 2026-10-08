/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OneOffSpellSeedArchetype.h"

#include "ECS/Components/Alpha.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "Graphics/ArgbColour.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Worship/SpellSeedGraphic.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity OneOffSpellSeedArchetype::Create(const glm::vec3& position, SpellSeedType seedType, int powerUp, float scale)
{
	const auto index = static_cast<int>(seedType);
	if (index <= -1 || index >= 30) // only seed types 0..29
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	// A mobile object (pos, info, parent 0, y angle 0, scale 1): the orb is always drawn at scale 1; the orb keeps `scale`
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(entity, resources::HashIdentifier(k_MeshName), static_cast<int8_t>(0), static_cast<int8_t>(0));
	// The orb's defaults, then the Create arguments
	auto& orb = registry.Assign<OneOffSpellSeed>(entity, seedType, scale, entt::null, 0.0f, powerUp);
	// The 4x4 texture animation goes through the object's UV offset
	registry.Assign<UvScroll>(entity);
	// The draw tints the object with 0x96FFFFFF (diffuse alpha 0xFF * 0x96 >> 8 = 0x95) and sets the global alpha,
	// which switches to the alternative mode table. The cap's material is already mode 12 (set at load, Game.cpp),
	// which that table keeps: additive SRCALPHA / ONE, alpha = texture x diffuse (0x95), Z write
	registry.Assign<Alpha>(
	    entity, static_cast<float>(argb_colour::Alpha(argb_colour::MultiplyArgbShift8(0xFF000000u, 0x96FFFFFFu))) / 255.0f);
	// Unless an object flag that is never set on a new orb, the seed inside: seed_graphic::Create(pos, seed, the local
	// player, 1.0, power up), with SetAutoUpdate(0) (Worship/SpellSeedGraphic.cpp)
	orb.graphic = worship::seed_graphic::Create(position, seedType, PlayerNames::PLAYER_ONE, 1.0f, powerUp);
	if (orb.graphic != entt::null)
	{
		worship::seed_graphic::SetAutoUpdate(orb.graphic, false);
	}
	return entity;
}
