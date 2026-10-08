/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectMetrics.h"

#include <cmath>

#include <algorithm>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/MeshBoxProviderInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// Half
constexpr float k_Half = 0.5f;

/// The mesh's bounding box, or nothing when it is not loaded (Locator::meshBoxProvider)
std::optional<AxisAlignedBoundingBox> MeshBox(entt::id_type meshId)
{
	return Locator::meshBoxProvider::value().MeshBox(meshId);
}

const ecs::Registry* RegistryOrNull()
{
	return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
}

/// The info of a pot entity (its PotInfo row), when it has one
const GPotInfo* PotInfoOf(const ecs::Registry& registry, entt::entity object)
{
	const auto* pot = registry.TryGet<const Pot>(object);
	if (pot == nullptr || pot->type == PotInfo::_COUNT || !Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	return &Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
}

bool IsPileFood(const ecs::Registry& registry, entt::entity object)
{
	// Food, magic food and puzzle grain piles are the pots whose info potType is PileFood
	const auto* info = PotInfoOf(registry, object);
	return info != nullptr && info->potType == PotType::PileFood;
}
} // namespace

namespace openblack::ecs::object
{

glm::vec3 HalfExtents(const AxisAlignedBoundingBox& box)
{
	// size first, then halved: two roundings, as in the original
	const glm::vec3 size = box.maxima - box.minima;
	return {size.x * k_Half, size.y * k_Half, size.z * k_Half};
}

float HalfDiagonal(glm::vec3 half)
{
	// hz hz, + hy hy, + hx hx, then the root, in that order
	const float zz = half.z * half.z;
	const float yy = half.y * half.y;
	const float zy = zz + yy;
	const float xx = half.x * half.x;
	const float sum = zy + xx;
	return std::sqrt(sum);
}

float Radius2D(glm::vec3 half, float scale)
{
	// hx when hz < hx, else hz
	const float larger = half.z < half.x ? half.x : half.z;
	return larger * scale;
}

float Height(glm::vec3 half, float scale)
{
	// the product first, then doubled
	const float product = half.y * scale;
	return product + product;
}

std::optional<glm::vec3> MeshHalfExtents(entt::id_type meshId)
{
	const auto box = MeshBox(meshId);
	if (!box)
	{
		return std::nullopt;
	}
	return HalfExtents(*box);
}

float MeshHalfDiagonal(entt::id_type meshId)
{
	const auto half = MeshHalfExtents(meshId);
	return half ? HalfDiagonal(*half) : 0.0f;
}

float MeshRadius2D(entt::id_type meshId, float scale)
{
	const auto half = MeshHalfExtents(meshId);
	return half ? Radius2D(*half, scale) : 0.0f;
}

float MeshHeight(entt::id_type meshId, float scale)
{
	const auto half = MeshHalfExtents(meshId);
	return half ? Height(*half, scale) : 0.0f;
}

float MeshHalfHeight(entt::id_type meshId)
{
	const auto half = MeshHalfExtents(meshId);
	return half ? half->y : 0.0f;
}

std::optional<glm::vec3> ObjectHalfExtents(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return std::nullopt;
	}
	const auto* mesh = registry->TryGet<const Mesh>(object);
	if (mesh == nullptr)
	{
		return std::nullopt;
	}
	return MeshHalfExtents(mesh->id);
}

float PileFoodProportionRaised(uint32_t amount, uint32_t maxInPot)
{
	// The unsigned amount over the maximum. With max 0 the original gets inf (-> 1) or NaN
	// (-> 0), the same as dividing by 1
	float p = static_cast<float>(amount) / static_cast<float>(std::max(1u, maxInPot));
	if (p < 0.0f)
	{
		p = 0.0f; // and no floor
	}
	else
	{
		if (p > 1.0f)
		{
			p = 1.0f; // then the floor
		}
		if (p > 0.0f) // p == 0 keeps 0
		{
			const float rest = 1.0f - k_ProportionFloor;
			p = rest * p;
			p = p + k_ProportionFloor;
		}
	}
	const float q = 1.0f - p;
	const float qq = q * q;
	return std::clamp(1.0f - qq, 0.0f, 1.0f);
}

float PileWoodProportionRaised(uint32_t amount, uint32_t maxInPot)
{
	float p = static_cast<float>(amount) / static_cast<float>(std::max(1u, maxInPot));
	if (p > 0.0f)
	{
		const float rest = 1.0f - k_ProportionFloor;
		p = rest * p;
		p = p + k_ProportionFloor;
	}
	return std::clamp(p, 0.0f, 1.0f);
}

const GPotInfo* PotInfoOf(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	return registry != nullptr && registry->Valid(object) ? ::PotInfoOf(*registry, object) : nullptr;
}

bool IsPileFood(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	return registry != nullptr && registry->Valid(object) && ::IsPileFood(*registry, object);
}

float GetProportionRaised(entt::entity pile)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(pile))
	{
		return 1.0f;
	}
	const auto* info = ::PotInfoOf(*registry, pile);
	if (info == nullptr)
	{
		return 1.0f;
	}
	const auto& pot = registry->Get<const Pot>(pile);
	switch (info->potType)
	{
	case PotType::PileFood:
		return PileFoodProportionRaised(pot.amount, info->maxAmountInPot);
	case PotType::PileWood:
		return PileWoodProportionRaised(pot.amount, info->maxAmountInPot);
	default:
		return 1.0f;
	}
}

float GetScaleField(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return 0.0f;
	}
	if (const auto* shield = registry->TryGet<const MapShield>(object))
	{
		// the shield's object scale; its Transform carries the drawn scale, which runs behind
		return shield->objectScale;
	}
	if (const auto* heart = registry->TryGet<const CitadelHeart>(object))
	{
		// the heart's scale is the plan's scale;
		// its Transform carries the drawn temple's, 1.0
		return heart->scale;
	}
	if (const auto* creature = registry->TryGet<const Creature>(object))
	{
		// a creature's user size; its Transform carries the drawn scale, which depends on its species' mesh
		return creature->size;
	}
	const auto* transform = registry->TryGet<const Transform>(object);
	// the uniform scale: every game object's Transform scale is glm::vec3(s)
	return transform != nullptr ? transform->scale.x : 0.0f;
}

float GetScale(entt::entity object)
{
	// the scale field; a creature's is its user size (Creature::size)
	return GetScaleField(object);
}

float FootprintRadius(entt::entity object)
{
	const auto half = ObjectHalfExtents(object);
	if (!half)
	{
		return 0.0f; // without a loaded mesh
	}
	return Radius2D(*half, GetScale(object)); // the virtual GetScale
}

float ObjectHeight(entt::entity object)
{
	const auto half = ObjectHalfExtents(object);
	if (!half)
	{
		return 0.0f;
	}
	return Height(*half, GetScaleField(object)); // the scale field, not the virtual GetScale
}

float Get2DRadius(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return 0.0f;
	}
	if (registry->AnyOf<Field, FishFarm>(object))
	{
		return k_FieldRadius;
	}
	if (registry->AllOf<MagicTeleport>(object))
	{
		return k_MagicTeleportRadius;
	}
	if (registry->AllOf<MagicFireBall>(object))
	{
		return GetScale(object) * k_MagicFireBallRadius;
	}
	if (::IsPileFood(*registry, object))
	{
		// the raised proportion, then the generic radius times it
		const float proportion = GetProportionRaised(object);
		return FootprintRadius(object) * proportion;
	}
	// A creature's reads its body (inferred: not ported, the generic formula stands in)
	return FootprintRadius(object);
}

float GetRadius(entt::entity object)
{
	return Get2DRadius(object);
}

float GetHeight(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return 0.0f;
	}
	if (registry->AllOf<MagicFireBall>(object))
	{
		return Get2DRadius(object);
	}
	if (registry->AllOf<Creature>(object))
	{
		// the body's user size (Creature::size) x 15
		return GetScale(object) * k_CreatureHeightPerScale;
	}
	return ObjectHeight(object);
}

float GetTopPos(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	const auto* transform =
	    registry != nullptr && registry->Valid(object) ? registry->TryGet<const Transform>(object) : nullptr;
	const float altitude = transform != nullptr ? map_coords::FromWorld(transform->position).altitude : 0.0f;
	if (registry != nullptr && registry->Valid(object) && registry->AllOf<MapShield>(object))
	{
		return 0.0f;
	}
	return GetHeight(object) + altitude;
}

float GetHeightForHandAboveInteractObject(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry != nullptr && registry->Valid(object) && registry->AllOf<FishFarm>(object))
	{
		return k_FieldRadius;
	}
	return GetHeight(object);
}

float GetMeshRadius(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return 0.0f;
	}
	if (registry->AnyOf<Field, FishFarm>(object))
	{
		return k_FieldRadius;
	}
	const auto half = ObjectHalfExtents(object);
	return half ? HalfDiagonal(*half) : 0.0f; // no scale
}

float GetHoldRadius(entt::entity object, bool holdTypeAbove)
{
	const auto* registry = RegistryOrNull();
	if (registry != nullptr && registry->Valid(object) && registry->AnyOf<Tree, DeadTree>(object))
	{
		return Get2DRadius(object) * k_TreeHoldFactor;
	}
	if (holdTypeAbove)
	{
		return GetHeight(object) * k_HoldAboveHeightFactor;
	}
	return Get2DRadius(object);
}

float GetDefaultFireRadius(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry != nullptr && registry->Valid(object))
	{
		if (registry->AllOf<DeadTree>(object))
		{
			return GetHeight(object) * k_DeadTreeFireFactor;
		}
		if (registry->AllOf<WorshipSite>(object))
		{
			return k_WorshipSiteRadius;
		}
	}
	return Get2DRadius(object);
}

namespace
{
float TreeHugRadius(entt::entity object)
{
	// Get2DRadius x 0.1, capped at 0.25
	const float scaled = Get2DRadius(object) * k_TreeHugFactor;
	return scaled > k_TreeHugMax ? k_TreeHugMax : scaled;
}
} // namespace

float GetVillagerHugRadius(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry != nullptr && registry->Valid(object) && registry->AllOf<Tree>(object))
	{
		return TreeHugRadius(object);
	}
	const float scaled = Get2DRadius(object) * k_HugRadiusFactor;
	return scaled + k_HugRadiusMargin;
}

float GetRoutePlanRadius(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry != nullptr && registry->Valid(object) && registry->AllOf<Tree>(object))
	{
		return TreeHugRadius(object);
	}
	if (registry != nullptr && registry->Valid(object) && registry->AllOf<Temple>(object))
	{
		return Get2DRadius(object) * k_CitadelHeartRoutePlanFactor;
	}
	return Get2DRadius(object); // with no creature
}

namespace
{
std::optional<glm::vec3> PositionOf(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return std::nullopt;
	}
	const auto* transform = registry->TryGet<const Transform>(object);
	return transform != nullptr ? std::optional(transform->position) : std::nullopt;
}
} // namespace

float GetDistanceFromObject(entt::entity object, entt::entity other)
{
	if (const auto* registry = RegistryOrNull();
	    registry != nullptr && registry->Valid(object) && registry->AllOf<WorshipSite, Transform>(object))
	{
		// the distance from the site's centre, less its real radius plus the other's Get2DRadius
		const auto b = PositionOf(other);
		const float distance = b ? gutils::GetDistanceInMetres(WorshipSiteCentre(object), *b) : 0.0f;
		const float otherRadius = Get2DRadius(other);
		const float radii = k_WorshipSiteRadius + otherRadius;
		return distance - radii;
	}
	const auto a = PositionOf(object);
	const auto b = PositionOf(other);
	const float distance = a && b ? gutils::GetDistanceInMetres(*a, *b) : 0.0f;
	const float otherRadius = Get2DRadius(other);
	const float radii = Get2DRadius(object) + otherRadius; // added first, then subtracted from the distance
	return distance - radii;
}

float GetDistanceFromObject(entt::entity object, glm::vec3 point)
{
	const auto a = PositionOf(object);
	const float distance = a ? gutils::GetDistanceInMetres(*a, point) : 0.0f;
	return distance - GetRadius(object);
}

bool IsTouching(entt::entity object, entt::entity other, float margin)
{
	return GetDistanceFromObject(object, other) <= margin;
}

bool IsTouching(entt::entity object, glm::vec3 point)
{
	return GetDistanceFromObject(object, point) <= 0.0f;
}

BoundingSphere GetBoundingSphere(entt::entity object)
{
	const float h = GetHeight(object) * k_Half;
	float r = Get2DRadius(object);
	if (const auto* registry = RegistryOrNull();
	    registry != nullptr && registry->Valid(object) &&
	    registry->AnyOf<Villager, Animal, MobileStatic, DeadTree, Fragment, MagicTeleport>(object))
	{
		// living things and mobile statics: the same routine with the radius halved before the square
		r = r * k_Half;
	}
	const float rr = r * r;
	const float hh = h * h;
	const float radius = std::sqrt(rr + hh);
	const auto position = PositionOf(object).value_or(glm::vec3(0.0f));
	// x, z from the map coords; y = ground + altitude, then + h
	auto centre = map_coords::ToWorld(map_coords::FromWorld(position));
	centre.y = centre.y + h;
	return {centre, radius};
}

glm::vec3 WorshipSiteCentre(entt::entity site)
{
	const auto* registry = RegistryOrNull();
	const auto* transform = registry != nullptr && registry->Valid(site) ? registry->TryGet<const Transform>(site) : nullptr;
	if (transform == nullptr)
	{
		return glm::vec3(0.0f);
	}
	// per component: (m[i] x 12.55 - m[6 + i] x 26.1) + m[9 + i], the right and forward rows and
	// the position; spelled out so that no FMA joins them
	const glm::vec3& right = transform->rotation[0];
	const glm::vec3& forward = transform->rotation[2];
	glm::vec3 centre;
	for (int i = 0; i < 3; ++i)
	{
		const float r = right[i] * k_WorshipSiteCentreRight;
		const float f = forward[i] * k_WorshipSiteCentreBack;
		const float d = r - f;
		centre[i] = d + transform->position[i];
	}
	return centre;
}

map_coords::MapCoords MapCoordsOf(entt::entity object)
{
	const auto position = PositionOf(object);
	return position ? map_coords::FromWorld(*position) : map_coords::MapCoords {};
}

map_coords::MapCoords GetNearestPosOfObject(entt::entity object, entt::entity other)
{
	const auto me = MapCoordsOf(object);
	const float angle = gutils::Get3DAngleFromXZ(me, MapCoordsOf(other));
	const float otherRadius = Get2DRadius(other);
	const float radius = Get2DRadius(object) + otherRadius;
	return me + gutils::GetPosFromAngle(angle, radius);
}

map_coords::MapCoords GetNearestEdgeToPos(entt::entity object, const map_coords::MapCoords& pos)
{
	const auto me = MapCoordsOf(object);
	const float angle = gutils::Get3DAngleFromXZ(me, pos);
	return me + gutils::GetPosFromAngle(angle, Get2DRadius(object));
}

map_coords::MapCoords GetNearestEdge(entt::entity object, float angle, float extra)
{
	const float radius = Get2DRadius(object) + extra;
	return MapCoordsOf(object) + gutils::GetPosFromAngle(angle, radius);
}

map_coords::MapCoords GetWorkingPos(entt::entity object, entt::entity other)
{
	const auto me = MapCoordsOf(object);
	const float angle = gutils::Get3DAngleFromXZ(me, MapCoordsOf(other));
	const float myRadius = GetRadius(object);
	const float radius = GetRadius(other) + myRadius;
	return me + gutils::GetPosFromAngle(angle, radius);
}

map_coords::MapCoords TreeGetWorkingPos(entt::entity tree, entt::entity other)
{
	const auto me = MapCoordsOf(tree);
	const float angle = gutils::Get3DAngleFromXZ(me, MapCoordsOf(other));
	const float radius = Get2DRadius(other) + k_TreeWorkingReach;
	return me + gutils::GetPosFromAngle(angle, radius);
}

map_coords::MapCoords BigForestGetArrivePos(entt::entity bigForest, entt::entity villager)
{
	const auto me = MapCoordsOf(bigForest);
	const float angle = gutils::Get3DAngleFromXZ(me, MapCoordsOf(villager));
	const float radius = GetRadius(bigForest) * k_Half;
	return me + gutils::GetPosFromAngle(angle, radius);
}

} // namespace openblack::ecs::object
