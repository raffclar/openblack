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
	return !registry.AnyOf<InHand, HeldByCreature, HighDetail, SeeThrough>(entity);
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

Stores::Stores(const Registry& registry)
    : _creatureMorph(registry)
    , _hand(registry)
    , _introHand(registry)
    , _temple(registry)
    , _templeInteriorPart(registry)
    , _translucent(registry)
    , _inHand(registry)
    , _heldByCreature(registry)
    , _highDetail(registry)
    , _seeThrough(registry)
    , _neverFades(registry)
    , _inPhysics(registry)
    , _ball(registry)
    , _vortex(registry)
    , _scriptHighlight(registry)
    , _drawnIntoVortex(registry)
    , _carriedByTornado(registry)
    , _magicShield(registry)
    , _animal(registry)
    , _detailMeshes(registry)
    , _drawImportance(registry)
{
}

bool Stores::CarriedByVortex(entt::entity entity) const
{
	return _drawnIntoVortex.Has(entity) && _carriedByTornado.Has(entity);
}

bool Stores::Banded(entt::entity entity) const
{
	return !(_creatureMorph.Has(entity) || _hand.Has(entity) || _introHand.Has(entity) || _temple.Has(entity) ||
	         _templeInteriorPart.Has(entity) || _translucent.Has(entity));
}

bool Stores::BandedNow(entt::entity entity) const
{
	return !(_inHand.Has(entity) || _heldByCreature.Has(entity) || _highDetail.Has(entity) || _seeThrough.Has(entity));
}

bool Stores::Disappears(entt::entity entity, bool townCentre) const
{
	if (townCentre || _neverFades.Has(entity) || _inPhysics.Has(entity) || _ball.Has(entity) || _vortex.Has(entity) ||
	    _scriptHighlight.Has(entity) || CarriedByVortex(entity))
	{
		return false;
	}
	if (const auto* shield = _magicShield.TryGet(entity); shield != nullptr && shield->kind == MagicShield::Kind::Physical)
	{
		return false;
	}
	const auto* animal = _animal.TryGet(entity);
	return animal == nullptr || !animals::birds::IsBird(animal->type);
}

float Stores::Importance(entt::entity entity) const
{
	if (CarriedByVortex(entity))
	{
		return k_CarriedByVortexImportance;
	}
	if (const auto* detail = _detailMeshes.TryGet(entity); detail != nullptr)
	{
		return detail->importance;
	}
	const auto* importance = _drawImportance.TryGet(entity);
	return importance != nullptr ? importance->importance : 0.0f;
}

} // namespace openblack::ecs::object_detail
