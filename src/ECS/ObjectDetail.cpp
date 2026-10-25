/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectDetail.h"

#include "Animals/BirdRules.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/DrawImportance.h"
#include "ECS/Components/GroundMark.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/IntroHand.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/NeverFades.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SeeThrough.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Translucent.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Registry.h"

namespace openblack::ecs::object_detail
{

using namespace components;

namespace
{

/// Whether a vortex that leads away carries the thing to the next land
bool CarriedByVortex(const Registry& registry, entt::entity entity)
{
	return registry.AllOf<DrawnIntoVortex, CarriedByTornado>(entity);
}

} // namespace

bool Banded(const Registry& registry, entt::entity entity)
{
	// The creatures, the hands (the player's and the opening's) and the temple are drawn whole at any distance; so are the
	// things drawn see-through by their materials, which are sorted one by one
	return !registry.AnyOf<CreatureMorph, Hand, IntroHand, Temple, TempleInteriorPart, Translucent>(entity);
}

bool BandedNow(const Registry& registry, entt::entity entity)
{
	if (registry.AnyOf<InHand, HeldByCreature, HighDetail, SeeThrough>(entity))
	{
		return false;
	}
	// The roots of a fallen dead tree are drawn whole while they fade by their own alpha
	const auto* mark = registry.TryGet<const GroundMark>(entity);
	return mark == nullptr || !mark->alpha.has_value();
}

bool Disappears(const Registry& registry, entt::entity entity, bool townCentre)
{
	if (townCentre || registry.AnyOf<NeverFades, InPhysics, Ball, Vortex, ScriptHighlight>(entity) ||
	    CarriedByVortex(registry, entity))
	{
		return false;
	}
	// Of the shields, only the physical one's dome is drawn as a mesh
	if (const auto* shield = registry.TryGet<const MagicShield>(entity);
	    shield != nullptr && shield->kind == MagicShield::Kind::Physical)
	{
		return false;
	}
	const auto* animal = registry.TryGet<const Animal>(entity);
	return animal == nullptr || !animals::birds::IsBird(animal->type);
}

float Importance(const Registry& registry, entt::entity entity)
{
	if (CarriedByVortex(registry, entity))
	{
		return k_CarriedByVortexImportance;
	}
	if (const auto* detail = registry.TryGet<const DetailMeshes>(entity); detail != nullptr)
	{
		return detail->importance;
	}
	const auto* importance = registry.TryGet<const DrawImportance>(entity);
	return importance != nullptr ? importance->importance : 0.0f;
}

} // namespace openblack::ecs::object_detail
