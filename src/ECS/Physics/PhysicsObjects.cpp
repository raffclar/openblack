/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysicsObjects.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <string>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Buildings.h"
#include "Camera/Camera.h"
#include "CollisionSounds.h"
#include "Debug/DebugEnv.h"
#include "Dust.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/FishShoals.h"
#include "ECS/HeldApply.h"
#include "ECS/Life.h"
#include "ECS/LivingPhysics.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjectsState.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/Rocks.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/PhysicsObjectsSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/VillagerDrowning.h"
#include "ECS/WaterRings.h"
#include "FileSystem/FileSystemInterface.h"
#include "FragMesh.h"
#include "FromHand.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MapShield.h"
#include "ParticleCarriedObjects.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;
using openblack::ecs::life::LifeOf;

namespace
{

using openblack::ecs::physics::k_DefaultConstants;
using openblack::ecs::physics::k_NumConstants;
using openblack::ecs::physics::Prediction;

/// The physics objects' state (Locator::physicsObjectsSystem)
openblack::ecs::physics::State& PhysicsState()
{
	return openblack::Locator::physicsObjectsSystem::value().GetState();
}

void EndPrediction()
{
	// the prediction stops and its body is released
	if (!PhysicsState().prediction.active)
	{
		return;
	}
	// (openblack) the drawn pose goes with the prediction, unless the object already has a body of its own
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(PhysicsState().prediction.object) && registry.AllOf<PhysicsDrawPose>(PhysicsState().prediction.object) &&
	    PhysicsObjects::Find(PhysicsState().prediction.object) == nullptr)
	{
		registry.Remove<PhysicsDrawPose>(PhysicsState().prediction.object);
	}
	PhysicsState().prediction = Prediction {};
}

PhysicsObject* Add(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity, entt::entity thrower, bool fromHand,
                   bool spreadReaction, bool initialiseClass);

ImpactInfo ImpactOf(const PhysicsObject& po)
{
	ImpactInfo info {
	    .g = po.GLoad(),
	    .hitBy = po.hitBy != nullptr ? po.hitBy->entity : entt::entity {entt::null},
	    .thrower = po.thrower,
	    .byPlayer = po.byPlayer,
	};
	return info;
}

const graphics::L3DMesh* MeshOf(entt::entity entity)
{
	const auto* mesh = Locator::entitiesRegistry::value().TryGet<const Mesh>(entity);
	if (mesh == nullptr)
	{
		return nullptr;
	}
	// the Mesh component: a broken building's stays its intact model (its FragMesh is a components::DrawMesh)
	const auto id = mesh->id;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(id))
	{
		return nullptr;
	}
	return &(*meshes.Handle(id));
}

bool MeshIs(entt::entity entity, uint32_t meshIndex)
{
	const auto* mesh = Locator::entitiesRegistry::value().TryGet<const Mesh>(entity);
	return mesh != nullptr && mesh->id == resources::HashIdentifier(static_cast<MeshId>(meshIndex));
}

const GObjectInfo* InfoOf(entt::entity entity)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& info = Locator::infoConstants::value();
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* c = registry.TryGet<const MobileStatic>(entity))
	{
		return &info.mobileStatic.at(static_cast<size_t>(c->type));
	}
	if (const auto* c = registry.TryGet<const Pot>(entity); c != nullptr && c->type != PotInfo::_COUNT)
	{
		return &info.pot.at(static_cast<size_t>(c->type));
	}
	if (const auto* c = registry.TryGet<const MobileObject>(entity))
	{
		return &info.mobileObject.at(static_cast<size_t>(c->type));
	}
	if (registry.AllOf<OneOffSpellSeed>(entity))
	{
		// a one-shot orb takes the mobile object info right after the whale's (inferred: the whale's entry is not
		// verified either)
		return &info.mobileObject.at(static_cast<size_t>(MobileObjectInfo::OneOffSpellSeed));
	}
	if (const auto* c = registry.TryGet<const Tree>(entity))
	{
		return &info.tree.at(static_cast<size_t>(c->type));
	}
	if (const auto* c = registry.TryGet<const DeadTree>(entity))
	{
		return &info.tree.at(static_cast<size_t>(c->type));
	}
	if (const auto* c = registry.TryGet<const MapShield>(entity))
	{
		// the physical shield's info (its weight of 50000 gives it a heavy body)
		return &info.mapShield.at(c->kind == MapShield::Kind::Physical ? 1 : 0);
	}
	return nullptr;
}

/// The mesh data of a body: the submeshes flagged isPhysics, otherwise the LOD 0 ones; every vertex and triangle of
/// them.
bool CollectVertices(const graphics::L3DMesh& mesh, std::vector<glm::vec3>& positions,
                     std::vector<std::array<uint32_t, 3>>& triangles)
{
	const auto& subMeshes = mesh.GetSubMeshes();
	const bool anyPhysics = std::any_of(subMeshes.begin(), subMeshes.end(), [](const auto& s) { return s->IsPhysics(); });
	const bool anyLod0 =
	    std::any_of(subMeshes.begin(), subMeshes.end(), [](const auto& s) { return (s->GetFlags().lodMask & 1) != 0; });
	for (const auto& subMesh : subMeshes)
	{
		const bool use = anyPhysics ? subMesh->IsPhysics() : (!anyLod0 || (subMesh->GetFlags().lodMask & 1) != 0);
		if (!use)
		{
			continue;
		}
		const auto base = static_cast<uint32_t>(positions.size());
		const auto& p = subMesh->GetCollisionPositions();
		positions.insert(positions.end(), p.begin(), p.end());
		const auto& indices = subMesh->GetCollisionIndices();
		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			triangles.push_back({base + indices[i], base + indices[i + 1], base + indices[i + 2]});
		}
	}
	return !positions.empty();
}

/// A tree's body: 16 points (3 rings of 4, the top, and the bottom point three times) and 24 faces along the trunk;
/// rooted trees have their centre of mass at 0.4 H, dead ones at 0.5 H.
void SetUpTreeBody(PhysicsBody& body, float height, float radius, float scale, bool rooted, const Transform& transform)
{
	const float h = (rooted ? 0.6f : 0.5f) * height;
	const glm::vec3 com(0.0f, (rooted ? 0.4f : 0.5f) * height / scale, 0.0f);
	std::vector<glm::vec3> v;
	for (const auto& [r, y] :
	     {std::pair {0.65f * radius, -0.6f * h}, std::pair {radius, 0.0f}, std::pair {0.65f * radius, 0.6f * h}})
	{
		v.emplace_back(r, y, 0.0f);
		v.emplace_back(0.0f, y, -r);
		v.emplace_back(-r, y, 0.0f);
		v.emplace_back(0.0f, y, r);
	}
	v.emplace_back(0.1f * radius, -h, 0.0f);
	v.emplace_back(0.0f, h, 0.0f);
	v.emplace_back(0.1f * radius, -h, 0.0f);
	v.emplace_back(0.1f * radius, -h, 0.0f);
	std::vector<std::array<uint32_t, 3>> f;
	for (const uint32_t b : {0u, 4u})
	{
		f.push_back({b, b + 1, b + 5});
		f.push_back({b, b + 5, b + 4});
		f.push_back({b + 1, b + 2, b + 6});
		f.push_back({b + 1, b + 6, b + 5});
		f.push_back({b + 2, b + 3, b + 7});
		f.push_back({b + 2, b + 7, b + 6});
		f.push_back({b + 3, b, b + 4});
		f.push_back({b + 3, b + 4, b + 7});
	}
	for (const std::array<uint32_t, 3> t : {std::array<uint32_t, 3> {12, 1, 0},
	                                        {12, 2, 1},
	                                        {12, 3, 2},
	                                        {12, 0, 3},
	                                        {13, 8, 9},
	                                        {13, 9, 10},
	                                        {13, 10, 11},
	                                        {13, 11, 8}})
	{
		f.push_back(t);
	}
	body.BuildShape(v, f, com, std::max(h, radius), 0.3f, transform.rotation, transform.position);
}

/// A villager's or an animal's body (SetUpLivingBody): 12 points on three levels (head, waist, feet) turned by pi/2
/// about Y, 20 faces, centre of mass at half height, drag x 2.
/// The draw's quarter turn of an animated (boned) mesh, after the rows are normalised: c, s are the cosine and sine
/// of the float pi / 2 kept in double precision, each product and sum rounded to float: r0' = c r0 + s r2,
/// r2' = c r2 - s r0, row 1 unchanged. Villager / Animal bodies are in the original's axes (no turn); their Transform
/// is the drawn rotation, so it is the body's rows turned.
constexpr double k_QuarterTurnCos = -4.371139000186241e-08; // cosine of the float pi / 2
constexpr double k_QuarterTurnSin = 0.9999999999999990;     // sine of the float pi / 2
glm::mat3 DrawQuarterTurn(const glm::mat3& rows)
{
	glm::mat3 out = rows;
	for (int k = 0; k < 3; ++k)
	{
		const auto cr0 = static_cast<float>(k_QuarterTurnCos * rows[0][k]);
		const auto sr2 = static_cast<float>(k_QuarterTurnSin * rows[2][k]);
		const auto cr2 = static_cast<float>(k_QuarterTurnCos * rows[2][k]);
		const auto sr0 = static_cast<float>(k_QuarterTurnSin * rows[0][k]);
		out[0][k] = cr0 + sr2;
		out[2][k] = cr2 - sr0;
	}
	return out;
}

/// The world matrix rows for a yaw a and a scale S, no quarter turn: a != 0 and S != 1: (fl(c S), 0, fl(s S)),
/// (0, S, 0), (-fl(s S), 0, fl(c S)) with c, s the cosine and sine of a unrounded; a != 0 and S == 1: c unrounded,
/// s stored as a float; a == 0: diag(S) or the identity. The "0 +" forms make a -0 a +0. glm: column k = the
/// original's row k.
glm::mat3 WorldMatrixRows(float a, float S)
{
	glm::mat3 m(0.0f);
	if (a != 0.0f && S != 1.0f)
	{
		const double ca = std::cos(static_cast<double>(a));
		const double sa = std::sin(static_cast<double>(a));
		const auto cS = static_cast<float>(ca * S);
		const auto sS = static_cast<float>(sa * S);
		m[0] = glm::vec3(0.0f + cS, 0.0f, 0.0f + sS);
		m[1] = glm::vec3(0.0f, S, 0.0f);
		m[2] = glm::vec3(0.0f - sS, 0.0f, cS - 0.0f);
	}
	else if (a != 0.0f)
	{
		const double ca = std::cos(static_cast<double>(a));
		const auto sa = static_cast<float>(std::sin(static_cast<double>(a)));
		m[0] = glm::vec3(static_cast<float>(0.0 + ca), 0.0f, 0.0f + sa);
		m[1] = glm::vec3(0.0f, 1.0f, 0.0f);
		m[2] = glm::vec3(0.0f - sa, 0.0f, static_cast<float>(ca - 0.0));
	}
	else
	{
		m = glm::mat3(S);
	}
	return m;
}

void SetUpLivingBody(PhysicsBody& body, float height, float radius, float scale, bool animal, const glm::mat3& rows,
                     glm::vec3 origin, bool rowsScaled)
{
	const float y = 0.5f * height;
	const float a = (animal ? 0.25f : 0.8f) * radius;
	const float z = (animal ? 0.8f : 0.32f) * radius;
	const float b = (animal ? 0.3f : 0.9f) * radius;
	const float c = (animal ? 0.9f : 0.32f) * radius;
	const std::array<glm::vec3, 12> raw = {{{a, y, z},
	                                        {-a, y, -z},
	                                        {-a, y, z},
	                                        {a, y, -z},
	                                        {b, 0.0f, c},
	                                        {-b, 0.0f, -c},
	                                        {-b, 0.0f, c},
	                                        {b, 0.0f, -c},
	                                        {a, -y, z},
	                                        {-a, -y, -z},
	                                        {-a, -y, z},
	                                        {a, -y, -z}}};
	// the vertices turned by RotY((float)pi / 2) with c stored as a float, -4.371139e-8f, and s = 1.0f:
	// x' = fl(-z + fl(c x)), z' = fl(x + fl(c z)); the radius is sqrt((z z + y y) + x x) of the turned vertex
	constexpr float k_C = -4.371139e-8f;
	std::vector<glm::vec3> v;
	float farthest = 0.0f;
	for (const auto& p : raw)
	{
		v.emplace_back(-p.z + k_C * p.x, p.y, p.x + k_C * p.z);
		const auto& q = v.back();
		farthest = std::max(farthest, std::sqrt((q.z * q.z + q.y * q.y) + q.x * q.x));
	}
	const std::vector<std::array<uint32_t, 3>> f = {{1, 2, 0},  {0, 3, 1},  {5, 1, 3},  {7, 5, 3},   {7, 3, 0},
	                                                {4, 7, 0},  {6, 4, 0},  {2, 6, 0},  {6, 2, 1},   {5, 6, 1},
	                                                {9, 5, 7},  {11, 9, 7}, {11, 7, 4}, {8, 11, 4},  {10, 8, 4},
	                                                {6, 10, 4}, {10, 6, 5}, {9, 10, 5}, {11, 10, 9}, {8, 10, 11}};
	body.BuildShape(v, f, glm::vec3(0.0f, y / scale, 0.0f), farthest, 2.0f, rows, origin, 1.0f, rowsScaled);
}

/// PhysicsBody::Initialise and the class's body setup from the entity's transform and mesh.
bool SetUpBody(entt::entity entity, PhysicsBody& body, bool dynamic, bool handPose = false)
{
	if (PhysicsObjects::ClassOf(entity) == PhysicsClass::Creature)
	{
		// a creature builds its own body, from its skeleton (pending)
		const auto& handlers = PhysicsObjects::Handlers(entity);
		return handlers.setUpBody && handlers.setUpBody(entity, body);
	}
	const auto* mesh = MeshOf(entity);
	if (mesh == nullptr)
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(entity);
	// a shield's body has its collision scale, which lags the drawn one (Magic/Objects/MapShield)
	const float scale = registry.AllOf<MapShield>(entity) ? magic::map_shield::CollisionScale(entity) : transform.scale.x;
	const auto size = mesh->GetBoundingBox().Size();
	const float height = size.y * scale;                          // the object's height
	const float radius = 0.5f * std::max(size.x, size.z) * scale; // the object's 2D radius
	if (const auto* fragment = registry.TryGet<const Fragment>(entity); fragment != nullptr && fragment->mesh)
	{
		// a fragment: the distinct vertices and a copy of each 0.45 behind along its normal (a slab), no faces (nothing is
		// hit by a fragment), about the fragment's origin; mass 30 x area; drag x 2
		std::vector<glm::vec3> points;
		std::vector<glm::vec3> normals;
		fragment->mesh->UniqueVertices(points, normals);
		std::vector<glm::vec3> local;
		float r = 0.0f;
		for (size_t i = 0; i < points.size(); ++i)
		{
			local.push_back(points[i]);
			local.push_back(points[i] - 0.45f * normals[i]);
			r = std::max({r, glm::length(local[local.size() - 2]), glm::length(local.back())});
		}
		// Initialise takes the half height of the rock mesh the fragment is built as (the third mobile static, scale 1).
		// openblack guard: the piece's own half height without info.dat
		float rockHalfHeight = 0.5f * size.y;
		if (Locator::infoConstants::has_value())
		{
			const auto rockMesh = resources::HashIdentifier(Locator::infoConstants::value().mobileStatic.at(2).meshId);
			if (const auto half = ecs::object::MeshHalfExtents(rockMesh))
			{
				rockHalfHeight = half->y;
			}
		}
		body.Initialise(scale, rockHalfHeight);
		body.SetUpConstants(PhysicsObjects::Weight(entity), PhysicsObjects::Constants(11), true);
		body.BuildShape(local, {}, glm::vec3(0.0f), r, 2.0f, transform.rotation, transform.position);
		return true;
	}
	const bool tree = registry.AllOf<Tree>(entity) || (registry.AllOf<DeadTree>(entity) && !MeshIs(entity, 406));
	const bool living = registry.AnyOf<Villager, Animal>(entity);
	if (tree || living)
	{
		body.Initialise(scale, 0.5f * size.y);
		body.SetUpConstants(PhysicsObjects::Weight(entity), PhysicsObjects::Constants(PhysicsObjects::ConstantsType(entity)),
		                    true);
		if (tree)
		{
			SetUpTreeBody(body, height, radius, scale, registry.AllOf<Tree>(entity), transform);
		}
		else
		{
			// the body in the original's axes, no quarter turn: in the map the start of physics writes the world matrix
			// (S R(yaw)) into the 3D matrix; from the hand SetUpPos takes the held matrix (the hand's angles at a throw, or
			// what the hand left for a put-down), which the hand's Transform already is ((inferred) the hand's drawn pose; the
			// prediction's: pending). SetUpPos normalises the rows.
			// The world matrix rows carry S already, so SetUpPos takes them as they are; the Transform's are unit rows
			const auto rows = handPose ? transform.rotation : WorldMatrixRows(ecs::living::ObjectYAngle(entity), scale);
			SetUpLivingBody(body, height, radius, scale, registry.AllOf<Animal>(entity), rows, transform.position, !handPose);
		}
		return true;
	}
	std::vector<glm::vec3> positions;
	std::vector<std::array<uint32_t, 3>> triangles;
	if (!CollectVertices(*mesh, positions, triangles))
	{
		return false;
	}
	body.Initialise(scale, 0.5f * size.y);
	body.SetUpConstants(PhysicsObjects::Weight(entity), PhysicsObjects::Constants(PhysicsObjects::ConstantsType(entity)),
	                    dynamic);
	body.Build(positions, triangles, transform.rotation, transform.position);
	return true;
}

void SyncTransform(const PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(po.entity))
	{
		return;
	}
	auto& transform = registry.Get<Transform>(po.entity);
	transform.position = po.body.ObjectOrigin();
	// the object's angles from the body's matrix: a Living stays upright with its yaw, and so does a Tree (a tree
	// replanted from the hand stands straight); the others: the rows, (approximate) RotationYXZ of their angles equals them
	// only to rounding. A villager's or an animal's Transform is the drawn rotation: the yaw-only original rows (the body's
	// rows are the original's) with the draw's quarter turn (in flight and on the ground)
	if (registry.AnyOf<Villager, Animal>(po.entity))
	{
		transform.rotation = DrawQuarterTurn(PhysicsObjects::RotationFromRows(po.entity, po.body.Rotation()));
	}
	else if (registry.AllOf<Tree>(po.entity))
	{
		transform.rotation = PhysicsObjects::RotationFromRows(po.entity, po.body.Rotation());
	}
	else
	{
		transform.rotation = po.body.Rotation();
	}
	if (auto* fixed = registry.TryGet<Fixed>(po.entity))
	{
		fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
	}
}

void AddRipple(const PhysicsObject& po)
{
	const float radius = std::max(po.body.Radius(), 0.01f);
	const WaterRing ring {
	    .position = glm::vec3(po.body.Centre().x, 0.1f, po.body.Centre().z),
	    .growth = 2.0f * radius,
	    .rate = 1.0f / radius,
	    .cell = 0x30 // the bob ripple's cell (the water hit's ring takes 0x3F)
	};
	AddWaterRing(ring);
}

/// The impact reaction, per class. Returns false when the entry went away.
bool ReactToPhysicsImpact(PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = po.entity;
	if (registry.AllOf<MapShield>(entity))
	{
		magic::map_shield::ReactToPhysicsImpact(entity, po); // Magic/Objects/MapShield
		return registry.Valid(entity);
	}
	if (const auto& handlers = PhysicsObjects::Handlers(entity); handlers.reactToImpact)
	{
		return handlers.reactToImpact(entity, po, ImpactOf(po));
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return Buildings::ReactToPhysicsImpact(entity, po);
	}
	if (from_hand::IsFence(entity))
	{
		// a thrown fence (not indestructible) that hits a store of wood goes into it, with the thrower's interface
		const auto hit = po.hitBy != nullptr ? po.hitBy->entity : entt::entity {entt::null};
		if (held_apply::FenceImpactTakes(true, registry.AllOf<Indestructible>(entity),
		                                 resource_stores::IsResourceStore(hit, ResourceType::Wood)) &&
		    resource_stores::DeleteObjectAndTakeResource(hit, entity, held_apply::ThrowerInterface(po.byPlayer)))
		{
			return false;
		}
		return true;
	}
	const float g = po.GLoad();
	if (Rocks::IsRock(entity))
	{
		// a rock: not from another rock
		const bool byRock = po.hitBy != nullptr && registry.Valid(po.hitBy->entity) && Rocks::IsRock(po.hitBy->entity);
		if (g > 4.0f && Rocks::Height(entity) > 0.7f && !byRock)
		{
			auto& life = registry.AllOf<Life>(entity) ? registry.Get<Life>(entity) : registry.Assign<Life>(entity);
			life.value -= (g - 4.0f) * 0.005f;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: rock hit G {:.1f}, life {:.3f}", g, life.value);
			if (life.value < 0.01f)
			{
				// Rock::SplitInTwo: the halves carry on with its velocity and half its angular velocity
				const bool flying = !po.body.resting;
				const auto velocity = flying ? po.body.velocity : glm::vec3(0.0f);
				const auto spin = flying ? po.body.angularMomentum * 0.5f : glm::vec3(0.0f);
				Rocks::SplitInTwo(entity, velocity, spin);
				return false;
			}
		}
	}
	return true;
}

/// The end of physics at rest, the class's part. Returns the entity that stays as a resting proxy (entt::null:
/// none).
entt::entity EndPhysicsOfClass(PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	SyncTransform(po);
	auto entity = po.entity;
	// its own flying-object reactions go (the predators fleeing from it stop)
	ecs::animal_ai::EndReactionsOf(entity);
	if (const auto& handlers = PhysicsObjects::Handlers(entity); handlers.endPhysics)
	{
		entity = handlers.endPhysics(entity, po);
		registry.SetDirty();
		return entity;
	}
	if (registry.AllOf<Fragment>(entity))
	{
		return Buildings::FragmentEndPhysics(entity, po);
	}
	registry.SetDirty();
	return entity;
}

/// The end of physics at rest: the class's part, then the object goes back in the map cells, a fragment that stays
/// (FragmentEndPhysics) too; nothing for one that is gone. After the class's part, but Villager and Animal call it
/// themselves in the middle of theirs (BackInMap, ECS/LivingPhysics), and a tree that becomes a DeadTree never calls
/// it (the DeadTree is inserted with no bounds test and returned). A tree searches the cells before its insert, so a
/// replanted tree does not see itself. The object's own part always returns the object, never null.
/// With insert and the object available (ecs::IsAvailable): inside the 512 x 512 cells it goes back in the map
/// cells, outside it is deleted (ToBeDeleted). Returns entt::null when the object that stays is the deleted one;
/// GameTurnUpdate makes the returned object the resting proxy.
entt::entity EndPhysics(PhysicsObject& po)
{
	const auto entity = po.entity;
	auto& registry = Locator::entitiesRegistry::value();
	// Villager / Animal (ECS/LivingPhysics) and Scaffold put themselves back in the map, between their own work
	const auto& handlers = PhysicsObjects::Handlers(entity);
	const bool classCallsBackInMap =
	    handlers.endPhysics && (registry.AnyOf<Villager, Animal>(entity) || handlers.callsBackInMap);
	const bool wasTree = registry.AllOf<Tree>(entity);
	const auto kept = EndPhysicsOfClass(po);
	if (wasTree && registry.Valid(kept) && registry.AllOf<DeadTree>(kept))
	{
		// a tree's DeadTree goes in the cells with no bounds test and is returned; the object's own part is not called
		map_cells::InsertMapObject(kept);
		return kept;
	}
	// an UNAVAILABLE (marked) object is neither put back in the cells nor deleted again
	if (!classCallsBackInMap && ecs::IsAvailable(entity) && !PhysicsObjects::BackInMap(entity))
	{
		return kept == entity ? entt::null : kept;
	}
	return kept;
}

/// On rest or removal the object stops being a hitter: buildings forget it (FragMesh lastHitter) and bodies that
/// had it as their thrower (the pass-through pair) collide with it again.
void ForgetThrower(entt::entity entity)
{
	for (auto& other : PhysicsState().objects)
	{
		if (other->thrower == entity && other->entity != entity)
		{
			other->thrower = entt::null;
		}
	}
	Buildings::ForgetHitter(entity);
}

void RemoveAt(size_t index)
{
	for (auto& other : PhysicsState().objects)
	{
		if (other->hitBy == PhysicsState().objects[index].get())
		{
			other->hitBy = nullptr;
		}
		if (other->body.lastHit == &PhysicsState().objects[index]->body)
		{
			other->body.lastHit = nullptr;
		}
	}
	// out of the physics: drawn at its Transform again
	if (auto& registry = Locator::entitiesRegistry::value();
	    registry.Valid(PhysicsState().objects[index]->entity) &&
	    registry.AllOf<PhysicsDrawPose>(PhysicsState().objects[index]->entity))
	{
		registry.Remove<PhysicsDrawPose>(PhysicsState().objects[index]->entity);
	}
	PhysicsState().objects.erase(PhysicsState().objects.begin() + static_cast<std::ptrdiff_t>(index));
}

/// A corner of the physics' cell boxes (RaiseUntilNotIntersecting, BeginTurn): metres to fixed point, truncated,
/// and off the map each signed cell clamped to 0 .. 511
glm::ivec2 BoxCell(float x, float z)
{
	const auto clamped = [](float metres) {
		return std::clamp<int32_t>(map_coords::SignedCellOf(map_coords::ToFixed(metres)), 0,
		                           static_cast<int32_t>(map_coords::k_MapCells) - 1);
	};
	return {clamped(x), clamped(z)};
}

/// Grows the list's capacity by 16 when it is full
void MakeSureEndSlotIsFree()
{
	if (PhysicsState().objects.size() >= PhysicsState().capacity)
	{
		PhysicsState().capacity += 16;
	}
}

/// A resting body for an object a moving body may hit.
void AddProxy(entt::entity entity)
{
	MakeSureEndSlotIsFree();
	auto po = std::make_unique<PhysicsObject>();
	po->entity = entity;
	po->villager = Locator::entitiesRegistry::value().AllOf<Villager>(entity);
	po->kind = po->villager ? 1 : 0;
	const bool dynamic = PhysicsObjects::CanBecomeAPhysicsObject(entity);
	if (!SetUpBody(entity, po->body, dynamic))
	{
		return;
	}
	po->body.resting = true;
	po->flags = PhysicsObject::k_Awake;
	// SetUpPos: the turn-start matrix is the body's own (a proxy knocked this turn moves from where it rests)
	PhysicsObjects::SyncTurnStart(*po);
	po->turnStarted = true;
	if (Locator::entitiesRegistry::value().AnyOf<Abode, StoragePit>(entity))
	{
		Buildings::ForgetHitter(entt::null, entity);     // a building's body setup clears the FragMesh's last hitter
		po->flags |= PhysicsObject::k_NoObjectCollision; // buildings do not check their vertices against objects
	}
	PhysicsState().objects.push_back(std::move(po));
}

void BeginTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (size_t i = 0; i < PhysicsState().objects.size();)
	{
		auto& po = *PhysicsState().objects[i];
		// an object no longer available: its body is released and the entry goes (the last one moves into its slot)
		if (!ecs::IsAvailable(po.entity))
		{
			RemoveAt(i);
			continue;
		}
		if (LifeOf(po.entity) < 0.01f)
		{
			po.body.density += 0.01f; // dead: sink factor +0.01 a turn (corpses sink)
		}
		po.forceSum = glm::vec3(0.0f);
		// the end matrix becomes the turn-start one
		PhysicsObjects::SyncTurnStart(po);
		po.turnStarted = true;
		po.body.lastHit = nullptr;
		po.hitBy = nullptr;
		++i;
	}
	// after the loop over the list: a prediction whose object is no longer available ends; else the original copies
	// its matrix and clears a counter, not ported: the prediction's body is never stepped again and its drawing uses
	// the history
	if (PhysicsState().prediction.active && !ecs::IsAvailable(PhysicsState().prediction.object))
	{
		EndPrediction();
	}
	// awake flags: moving bodies are awake, resting ones only while something moving is near them
	for (auto& po : PhysicsState().objects)
	{
		if (po->body.resting)
		{
			po->flags &= ~PhysicsObject::k_Awake;
		}
		else
		{
			po->flags |= PhysicsObject::k_Awake;
		}
	}
	// each moving body wakes what is in the map cells of its box C -/+ (|v.xz| x 0.1 + R) (BoxCell), x outer and z
	// inner; a cell is walked once for all the bodies (the list of walked cells keeps at most 512). The fixed list, then
	// the mobile one (map_cells::ForEachInCell): what InteractsWithPhysicsObjects is woken when it is in the physics
	// already, else with a 3D object it gets a resting proxy (AddProxy) at once. The walk stops when the list's
	// allocated slots are used up (count >= capacity: before each x column, each z cell and each object): AddProxy does
	// not grow it here, so the condition stays true for the rest of the walk and for every later body of the turn
	constexpr size_t k_WalkedCells = 0x200;
	std::vector<glm::ivec2> walked;
	const auto full = []() { return PhysicsState().objects.size() >= PhysicsState().capacity; };
	const size_t bodies = PhysicsState().objects.size(); // the proxies added below rest: they have no box
	for (size_t b = 0; b < bodies && !full(); ++b)
	{
		if (PhysicsState().objects[b]->body.resting)
		{
			continue;
		}
		const auto centre = PhysicsState().objects[b]->body.Centre();
		const auto v = PhysicsState().objects[b]->body.velocity;
		const float half = glm::length(glm::vec2(v.x, v.z)) * 0.1f + PhysicsState().objects[b]->body.Radius();
		const auto first = BoxCell(centre.x - half, centre.z - half);
		const auto second = BoxCell(centre.x + half, centre.z + half);
		const auto low = glm::min(first, second);
		const auto high = glm::max(first, second);
		for (int32_t x = low.x; x <= high.x && !full(); ++x)
		{
			for (int32_t z = low.y; z <= high.y && !full(); ++z)
			{
				// in the map, then the walked list
				const glm::ivec2 cell(x, z);
				if (!map_coords::InBounds(cell) || std::find(walked.begin(), walked.end(), cell) != walked.end())
				{
					continue;
				}
				if (walked.size() < k_WalkedCells)
				{
					walked.push_back(cell);
				}
				map_cells::ForEachInCell(cell, [&registry, &full](entt::entity entity) {
					if (full())
					{
						return false;
					}
					if (!PhysicsObjects::InteractsWithPhysicsObjects(entity))
					{
						return true;
					}
					if (auto* po = PhysicsObjects::Find(entity))
					{
						po->flags |= PhysicsObject::k_Awake;
					}
					else if (registry.AllOf<Mesh>(entity))
					{
						AddProxy(entity);
					}
					return true;
				});
			}
		}
	}
	// the physical shields always stay in the system at their collision scale, whether or not something moves near
	// them (Magic/Objects/MapShield)
	for (const auto shield : magic::map_shield::Shields())
	{
		if (!registry.Valid(shield) || !magic::map_shield::InteractsWithPhysicsObjects(shield))
		{
			continue;
		}
		if (auto* po = PhysicsObjects::Find(shield))
		{
			po->flags |= PhysicsObject::k_Awake;
		}
		else
		{
			AddProxy(shield);
		}
	}
	for (size_t i = 0; i < PhysicsState().objects.size();)
	{
		if ((PhysicsState().objects[i]->flags & PhysicsObject::k_Awake) == 0)
		{
			RemoveAt(i);
			continue;
		}
		++i;
	}
}

void Substep()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (auto& po : PhysicsState().objects)
	{
		po->body.ZeroForces();
		po->body.GroundAndWater();
	}
	for (auto& a : PhysicsState().objects)
	{
		if ((a->flags & PhysicsObject::k_NoObjectCollision) != 0)
		{
			continue;
		}
		for (auto& b : PhysicsState().objects)
		{
			if (a == b)
			{
				continue;
			}
			if (a->body.resting && b->body.resting && !a->body.justSetUp && !b->body.justSetUp)
			{
				continue;
			}
			// a villager's body does not hit what a Living pushed (PushedByLiving: a push, a kicked ball, a felled tree)
			if ((a->villager && (b->flags & PhysicsObject::k_PushedByLiving) != 0) ||
			    (b->villager && (a->flags & PhysicsObject::k_PushedByLiving) != 0))
			{
				continue;
			}
			if (a->thrower == b->entity || b->thrower == a->entity)
			{
				continue;
			}
			const float reach = a->body.Radius() + b->body.Radius();
			const auto d = a->body.Centre() - b->body.Centre();
			if (glm::dot(d, d) < reach * reach)
			{
				a->body.CollideVertices(b->body);
			}
		}
	}
	for (auto& po : PhysicsState().objects)
	{
		po->body.ContactForces();
	}
	// the entry at i is still ours unless something removed it (a death removes the entry itself)
	const auto stillAt = [](size_t i, const PhysicsObject* self) {
		return i < PhysicsState().objects.size() && PhysicsState().objects[i].get() == self;
	};
	for (size_t i = 0; i < PhysicsState().objects.size();)
	{
		auto& po = *PhysicsState().objects[i];
		auto* self = &po;
		const float vyBefore = po.body.velocity.y;
		auto result = po.body.Integrate();
		if (!po.body.resting && po.body.Centre().y < po.body.Radius() * 0.5f)
		{
			// below half its radius: HasSunk (ECS/VillagerDrowning) stops the body and ends its physics as if at rest
			ecs::RememberLastPlayerToInteract(po.entity, po.byPlayer);
			const auto& handlers = PhysicsObjects::Handlers(po.entity);
			if (po.body.density > 1.0f && (handlers.hasSunk ? handlers.hasSunk(po.entity, po) : ecs::HasSunk(po.entity)))
			{
				if (!stillAt(i, self)) // sinking removed the animal (ToBeDeleted)
				{
					continue;
				}
				po.body.velocity = glm::vec3(0.0f);
				po.body.angularMomentum = glm::vec3(0.0f);
				result = PhysicsBody::Result::Stopped;
			}
			if (vyBefore * po.body.velocity.y < 0.0f)
			{
				AddRipple(po);
			}
		}
		if (po.body.touched)
		{
			po.forceSum += po.body.force;
		}
		// the fly-by whoosh: a body entering the 10 m sphere round the camera at more than 20 m/s (G_ROCKPAST_01..05)
		if (Locator::camera::has_value())
		{
			const auto d = po.body.Centre() - Locator::camera::value().GetOrigin();
			const float d2 = glm::dot(d, d);
			if (d2 < 100.0f && po.cameraDistance2 > 100.0f && glm::dot(po.body.velocity, po.body.velocity) > 400.0f)
			{
				// a 2D in-game sound, one of G_RockPast_01..05 picked by the tick count
				audio::PlaySoundEffect(audio::Owner::None(), 69 + static_cast<int>(audio::TickCount() % 5), 2, 0, false, false,
				                       audio::SfxBank::InGame);
			}
			po.cameraDistance2 = d2;
		}
		switch (result)
		{
		case PhysicsBody::Result::Stopped:
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: entity {} at rest at ({:.2f}, {:.2f}, {:.2f})",
			                   static_cast<uint32_t>(po.entity), po.body.Centre().x, po.body.Centre().y, po.body.Centre().z);
			ForgetThrower(po.entity);
			const auto kept = EndPhysics(po);
			if (!stillAt(i, self))
			{
				continue;
			}
			if (kept == entt::null || !registry.Valid(kept))
			{
				RemoveAt(i);
				continue;
			}
			// GameTurnUpdate's stop only (RemoveObjectWithEndPhysics does not do it again): the object EndPhysics returned
			// takes the body's angles. A DeadTree a tree's end of physics returned is a MobileStatic: it keeps the body's x, y,
			// z. (pending) the same call on any other returned object (a Living's Transform has the drawn quarter turn; the
			// others equal their rows to rounding)
			if (registry.AllOf<DeadTree, Transform>(kept))
			{
				registry.Get<Transform>(kept).rotation = PhysicsObjects::RotationFromRows(kept, po.body.Rotation());
			}
			po.entity = kept;
			po.body.resting = true;
			break;
		}
		case PhysicsBody::Result::Pushed:
		{
			// a knocked resting proxy: the class's InitialisePhysics (a Villager / Animal flies, FLYING and the hit clip),
			// woken only when it starts; without a class hook, the object's own (refused for what a particle system carries)
			const auto& handlers = PhysicsObjects::Handlers(po.entity);
			const bool started = handlers.initialisePhysicsKnocked ? handlers.initialisePhysicsKnocked(po.entity, po)
			                                                       : PhysicsObjects::CanWakeKnockedProxy(po.entity);
			if (started)
			{
				po.body.resting = false;
				po.flags |= PhysicsObject::k_Awake;
				// out of the map cells while it flies
				if (map_cells::IsObjectInMap(po.entity))
				{
					map_cells::RemoveMapObject(po.entity);
				}
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: knocked entity {}", static_cast<uint32_t>(po.entity));
			}
			break;
		}
		case PhysicsBody::Result::Delete:
		{
			const auto entity = po.entity;
			RemoveAt(i);
			// only an object still available is deleted
			if (!ecs::IsAvailable(entity))
			{
				continue;
			}
			if (registry.AllOf<Fragment>(entity))
			{
				Buildings::DestroyFragment(entity);
			}
			else
			{
				ecs::ToBeDeleted(entity); // fell below -4 R: the class's deletion
			}
			continue;
		}
		default:
			break;
		}
		++i;
	}
}

void EndTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (auto& po : PhysicsState().objects)
	{
		po->body.externalForce = glm::vec3(0.0f);
		po->body.externalTorque = glm::vec3(0.0f);
		// a felled tree (kind 2) that has toppled (its row 1's y < 0.98) sounds once when taller than 10: a 3D in-game
		// sound tag at its MapCoords, a random sample of 31; then kind 3
		if (po->kind == 2 && po->body.Rotation()[1].y < 0.98f && registry.Valid(po->entity))
		{
			if (ecs::object::GetHeight(po->entity) > 10.0f)
			{
				audio::tags::CreateAtMapCoords(ecs::object::MapCoordsOf(po->entity), audio::tags::RandomSample(31, 1), false, 3,
				                               0, false, true, audio::SfxBank::InGame, 0);
			}
			po->kind = 3;
		}
		// a resting proxy follows its object: villagers and animals walk while their proxy waits to be hit
		if (po->body.resting && registry.Valid(po->entity))
		{
			po->body.FollowObject(map_coords::ToWorld(object::MapCoordsOf(po->entity)));
		}
		const float sum2 = glm::dot(po->forceSum, po->forceSum);
		po->impact = sum2 > 0.0001f ? std::sqrt(sum2) * 0.05f : 0.0f;
		static const bool k_PhysicsTrace = debug_env::PhysicsTrace(); // read once: it is asked for each body each turn
		if (k_PhysicsTrace && !po->body.resting)
		{
			const auto c = po->body.Centre();
			SPDLOG_LOGGER_INFO(
			    spdlog::get("game"),
			    "Physics trace: entity {} at ({:.2f}, {:.2f}, {:.2f}) v {:.2f} contacts {} G {:.2f} density {:.4f} R {:.2f}",
			    static_cast<uint32_t>(po->entity), c.x, c.y, c.z, glm::length(po->body.velocity), po->body.numContacts,
			    po->GLoad(), po->body.density, po->body.Radius());
		}
		po->hitBy = nullptr;
		if (po->body.lastHit != nullptr)
		{
			for (auto& other : PhysicsState().objects)
			{
				if (&other->body == po->body.lastHit)
				{
					po->hitBy = other.get();
				}
			}
		}
		if (!po->byPlayer && po->hitBy != nullptr)
		{
			po->byPlayer = po->hitBy->byPlayer; // the damage goes to whoever threw what hit it
		}
	}
	// ReactToPhysicsImpact may split rocks or kill villagers, which changes the list: work on a snapshot
	std::vector<entt::entity> hit;
	for (auto& po : PhysicsState().objects)
	{
		if (po->impact <= 0.0f)
		{
			continue;
		}
		// the collision sample, the ground dust or the splash
		if (!po->body.resting && (po->hitBy != nullptr || po->impact > po->body.Mass() * 4.905f))
		{
			CollisionSounds::AttemptToAddSoundEvent(*po);
		}
		hit.push_back(po->entity);
	}
	for (const auto entity : hit)
	{
		if (auto* po = PhysicsObjects::Find(entity); po != nullptr && registry.Valid(entity))
		{
			if (!ReactToPhysicsImpact(*po) && !registry.Valid(entity))
			{
				PhysicsObjects::RemoveObject(entity);
			}
		}
	}
	CollisionSounds::EndTurn();
}
} // namespace

void PhysicsObjects::LoadConstants()
{
	PhysicsState().constants = k_DefaultConstants;
	auto& fileSystem = Locator::filesystem::value();
	std::vector<uint8_t> bytes;
	try
	{
		bytes = resources::LoadBlob(Locator::resources::value().GetBlobs(),
		                            fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "PhysicsConstants.txt"));
	}
	catch (const std::exception&)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Physics: Data/PhysicsConstants.txt not found, using the built-in table");
		return;
	}
	std::istringstream in(std::string(bytes.begin(), bytes.end()));
	int version = 0;
	int rows = 0;
	in >> version >> rows;
	// every column clamped to its own range
	constexpr std::array<float, 6> k_Min = {0.05f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f};
	constexpr std::array<float, 6> k_Max = {3.0f, 240.0f, 10.0f, 3.0f, 1.0f, 4.0f};
	for (size_t i = 0; i < k_NumConstants; ++i)
	{
		if (static_cast<int>(i) >= rows)
		{
			PhysicsState().constants.at(i) = PhysicsState().constants.at(0); // missing rows are copies of row 0
			continue;
		}
		std::array<float, 6> v {};
		for (auto& f : v)
		{
			in >> f;
		}
		for (size_t c = 0; c < v.size(); ++c)
		{
			v.at(c) = std::clamp(v.at(c), k_Min.at(c), k_Max.at(c));
		}
		PhysicsState().constants.at(i) = {v[0], v[1], v[2], v[3], v[4], v[5]};
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: {} constant rows (version {})", rows, version);
}

const PhysicsData& PhysicsObjects::Constants(int type)
{
	return PhysicsState().constants.at(static_cast<size_t>(std::clamp(type, 0, static_cast<int>(k_NumConstants) - 1)));
}

int PhysicsObjects::ConstantsType(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<MapShield>(entity))
	{
		return magic::map_shield::k_PhysicsConstantsType;
	}
	if (registry.AllOf<OneOffSpellSeed>(entity))
	{
		return 9; // a one-shot orb thrown from the hand
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return 0;
	}
	if (registry.AllOf<Creature>(entity))
	{
		return 0;
	}
	if (registry.AllOf<Fragment>(entity))
	{
		return 11;
	}
	if (registry.AllOf<Villager>(entity))
	{
		return 7;
	}
	if (registry.AllOf<Animal>(entity))
	{
		return 8;
	}
	if (registry.AllOf<Tree>(entity))
	{
		return 6;
	}
	if (registry.AllOf<DeadTree>(entity))
	{
		return MeshIs(entity, 406) ? 1 : 6; // MSH_O_WOOD_INHAND
	}
	if (registry.AllOf<Pot>(entity))
	{
		return MeshIs(entity, 323) ? 5 : 4; // MSH_I_OFFERING_FOOD
	}
	if (registry.AllOf<Scaffold>(entity))
	{
		return 17;
	}
	if (const auto* object = registry.TryGet<const MobileObject>(entity))
	{
		// Champi, MagicMushroom, Toadstool have their own rows
		const auto index = static_cast<int>(object->type);
		if (index >= 17 && index <= 19)
		{
			return 21 + index - 17;
		}
		return 1;
	}
	if (const auto* statics = registry.TryGet<const MobileStatic>(entity))
	{
		const auto index = static_cast<int>(statics->type);
		if ((index >= 49 && index <= 52) || index == 14 || index == 15 || index == 5 || Rocks::IsRock(entity))
		{
			return 3;
		}
		// a fence: row 18, before the toys
		if (from_hand::IsFence(entity))
		{
			return 18;
		}
		constexpr std::array<int, 5> k_Toys = {14, 20, 16, 15, 19}; // meshes 399..403
		for (uint32_t m = 0; m < k_Toys.size(); ++m)
		{
			if (MeshIs(entity, 399 + m))
			{
				return k_Toys.at(m);
			}
		}
		return 1; // a mobile static can always become a physics object
	}
	// otherwise 1 when it can become a physics object, else 0
	return CanBecomeAPhysicsObject(entity) ? 1 : 0;
}

bool PhysicsObjects::InteractsWithPhysicsObjects(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<MapShield>(entity))
	{
		return magic::map_shield::InteractsWithPhysicsObjects(entity); // physical shield: yes, magic shield: no
	}
	if (registry.AnyOf<Tree, Field, BigForest, Fragment>(entity))
	{
		return false; // standing trees: thrown objects go through them; fragments only hit the landscape
	}
	if (registry.AllOf<Creature>(entity))
	{
		return true;
	}
	if (const auto* pot = registry.TryGet<const Pot>(entity))
	{
		// piles do not interact
		return pot->type == PotInfo::HandWood || pot->type == PotInfo::HandFood;
	}
	// fields are never hit; buildings only while standing (more than 10% built and life > 0.01)
	if (const auto* abode = registry.TryGet<const Abode>(entity); abode != nullptr && abode->type == AbodeNumber::Field)
	{
		return false;
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		if (const auto* life = registry.TryGet<const Life>(entity); life != nullptr && life->value <= 0.01f)
		{
			return false;
		}
	}
	return registry.AnyOf<MobileStatic, MobileObject, Villager, Animal, DeadTree, Abode, StoragePit>(entity);
}

bool PhysicsObjects::SetUpBody(entt::entity entity, PhysicsBody& body, bool dynamic)
{
	// the prediction's body is the held object's: the hand's pose (a Living's Transform as it is)
	return ::SetUpBody(entity, body, dynamic, true);
}

bool PhysicsObjects::CanWakeKnockedProxy(entt::entity entity)
{
	return CanBecomeAPhysicsObject(entity) && !particle_carried_objects::IsCarried(entity);
}

bool PhysicsObjects::CanBecomeAPhysicsObject(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<Abode, StoragePit, Field, BigForest>(entity))
	{
		return false;
	}
	if (registry.AllOf<Scaffold>(entity))
	{
		// the class's handler (not in a workshop slot, no site), else a mobile object's answer: yes
		const auto& handlers = PhysicsObjects::Handlers(entity);
		return handlers.canBecomePhysicsObject ? handlers.canBecomePhysicsObject(entity) : true;
	}
	// a one-shot orb is a MobileObject too
	return registry.AnyOf<MobileStatic, MobileObject, Villager, Animal, Tree, DeadTree, Pot, Fragment, OneOffSpellSeed>(entity);
}

const GObjectInfo* PhysicsObjects::ObjectInfo(entt::entity entity)
{
	if (const auto* info = InfoOf(entity))
	{
		return info;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& constants = Locator::infoConstants::value();
	if (const auto* animal = registry.TryGet<const Animal>(entity))
	{
		return &constants.animal.at(static_cast<size_t>(animal->type));
	}
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		for (const auto& info : constants.villager)
		{
			if (info.tribeType == villager->tribe && info.villagerNumber == villager->number)
			{
				return &info;
			}
		}
	}
	// a creature's is its species' row of the creature tables
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		const auto row = openblack::creature::InfoRow(creature->species);
		return row < constants.creature.size() ? &constants.creature.at(row) : nullptr;
	}
	return nullptr;
}

float PhysicsObjects::Weight(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return 2000.0f; // a building's body
	}
	if (const auto* fragment = registry.TryGet<const Fragment>(entity))
	{
		return std::max(30.0f * fragment->area, 0.01f); // 30 x area
	}
	if (const auto& handlers = PhysicsObjects::Handlers(entity); handlers.weight)
	{
		return handlers.weight(entity); // the class's own weight (a creature's), unclamped
	}
	// scale^3 x info weight
	float weight = 1.0f;
	if (const auto* info = InfoOf(entity))
	{
		weight = info->weight;
	}
	else if (const auto* animal = registry.TryGet<const Animal>(entity);
	         animal != nullptr && Locator::infoConstants::has_value())
	{
		weight = Locator::infoConstants::value().animal.at(static_cast<size_t>(animal->type)).weight;
	}
	else if (const auto* villager = registry.TryGet<const Villager>(entity);
	         villager != nullptr && Locator::infoConstants::has_value())
	{
		weight = 82.5f; // info.dat villagers
		for (const auto& info : Locator::infoConstants::value().villager)
		{
			if (info.tribeType == villager->tribe && info.villagerNumber == villager->number)
			{
				weight = info.weight;
				break;
			}
		}
	}
	float scale = registry.AllOf<Transform>(entity) ? registry.Get<const Transform>(entity).scale.x : 1.0f;
	if (registry.AllOf<MapShield>(entity))
	{
		scale = magic::map_shield::CollisionScale(entity); // the collision scale, not the drawn one
	}
	return std::max(scale * scale * scale * weight, 0.01f);
}

PhysicsObject* PhysicsObjects::AddObject(entt::entity entity, glm::vec3 velocity, BodySpin spin, entt::entity thrower,
                                         bool fromHand)
{
	// (order) AddObject below already runs the class's initialisePhysics and CheckAllCreaturesForCatching, and L is set
	// after them; the original sets L before the creature check. The same while those hooks do not read L (none does)
	auto* po = AddObject(entity, velocity, glm::vec3(0.0f), thrower, fromHand);
	if (po != nullptr)
	{
		po->body.SetAngularVelocityFromBody(spin.w);
	}
	return po;
}

PhysicsObject* PhysicsObjects::AddObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
                                         entt::entity thrower, bool fromHand)
{
	return Add(entity, velocity, angularVelocity, thrower, fromHand, fromHand, true);
}

PhysicsObject* PhysicsObjects::AddObjectFromHand(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity)
{
	// the class's initialisePhysics comes after the landing test: from_hand calls InitialisePhysicsOfClass
	return Add(entity, velocity, angularVelocity, entt::null, true, false, false);
}

void PhysicsObjects::ThrowFromHandOfClass(entt::entity entity, bool dontReplant)
{
	if (const auto& handlers = PhysicsObjects::Handlers(entity); handlers.throwFromHand)
	{
		handlers.throwFromHand(entity, dontReplant);
	}
}

const PhysicsObjects::ClassHandlers& PhysicsObjects::Handlers(entt::entity entity)
{
	return PhysicsState().classHandlers.at(static_cast<size_t>(ClassOf(entity)));
}

void PhysicsObjects::InitialisePhysicsOfClass(PhysicsObject& po, bool fromHand)
{
	if (const auto& handlers = PhysicsObjects::Handlers(po.entity); handlers.initialisePhysics)
	{
		handlers.initialisePhysics(po.entity, po, fromHand);
	}
}

void PhysicsObjects::SyncTurnStart(PhysicsObject& po)
{
	// the three rows and the translation, as SetUpPos, AdjustToGroundLevel and the turn start copy them
	po.turnStartRotation = po.body.Rotation();
	po.turnStartCentre = po.body.Centre();
}

PhysicsObject* PhysicsObjects::AddDroppedObject(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity,
                                                std::optional<glm::vec3> angularMomentum)
{
	auto* po = AddObject(entity, velocity, angularVelocity);
	if (po == nullptr)
	{
		return nullptr;
	}
	return FinishDroppedObject(*po, angularMomentum);
}

PhysicsObject* PhysicsObjects::AddDroppedObject(entt::entity entity, glm::vec3 velocity, BodySpin spin,
                                                std::optional<glm::vec3> angularMomentum)
{
	auto* po = AddObject(entity, velocity, spin);
	if (po == nullptr)
	{
		return nullptr;
	}
	return FinishDroppedObject(*po, angularMomentum);
}

PhysicsObject* PhysicsObjects::FinishDroppedObject(PhysicsObject& body, std::optional<glm::vec3> angularMomentum)
{
	auto* po = &body;
	if (angularMomentum)
	{
		po->body.angularMomentum = *angularMomentum;
	}
	po->flags |= PhysicsObject::k_NoObjectCollision;
	po->body.AdjustToGroundLevel(false, true);
	SyncTurnStart(*po); // the copy at the end of AdjustToGroundLevel
	RaiseUntilNotIntersecting(*po);
	return po;
}

namespace
{
PhysicsObject* Add(entt::entity entity, glm::vec3 velocity, glm::vec3 angularVelocity, entt::entity thrower, bool fromHand,
                   bool spreadReaction, bool initialiseClass)
{
	MakeSureEndSlotIsFree();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !PhysicsObjects::CanBecomeAPhysicsObject(entity))
	{
		return nullptr;
	}
	// an IMMOVABLE object (SET_ID_MOVEABLE) gets no physics
	if (object_flags::IsImmovable(entity))
	{
		return nullptr;
	}
	// already in physics: an object a particle system carries is in physics with no body
	if (particle_carried_objects::IsCarried(entity))
	{
		return nullptr;
	}
	if (auto* existing = PhysicsObjects::Find(entity))
	{
		if (!existing->body.resting)
		{
			return nullptr;
		}
		PhysicsObjects::RemoveObject(entity);
	}
	auto po = std::make_unique<PhysicsObject>();
	po->entity = entity;
	po->thrower = thrower;
	po->villager = registry.AllOf<Villager>(entity);
	po->kind = po->villager ? 1 : 0;
	if (!SetUpBody(entity, po->body, true, fromHand))
	{
		return nullptr;
	}
	// SetUpPos: the turn-start matrix is the body's own (drawn there until the next turn starts)
	PhysicsObjects::SyncTurnStart(*po);
	po->body.SetAngularVelocity(angularVelocity);
	const float speed = glm::length(velocity);
	po->body.velocity = speed > PhysicsBody::k_MaxSpeed ? velocity * (PhysicsBody::k_MaxSpeed / speed) : velocity;
	po->flags = PhysicsObject::k_Awake | (fromHand ? PhysicsObject::k_FromHand : 0);
	po->byPlayer = fromHand;
	if (initialiseClass)
	{
		// the start of physics of a villager or an animal (ECS/LivingPhysics). Not for AddObjectFromHand: the hand's start
		// calls it after its landing test
		PhysicsObjects::InitialisePhysicsOfClass(*po, fromHand);
	}
	PhysicsState().objects.push_back(std::move(po));
	// out of the map cells while it flies
	if (map_cells::IsObjectInMap(entity))
	{
		map_cells::RemoveMapObject(entity);
	}
	// before the fire's StartedMoving (AddObject's path; the hand's start calls it itself, physics::from_hand)
	if (auto* added = PhysicsObjects::Find(entity); initialiseClass && added != nullptr)
	{
		PhysicsObjects::CheckAllCreaturesForCatching(entity, *added);
	}
	// a burning object leaves its fire group (ECS/Fire)
	fire::StartedMoving(entity, false);
	if (spreadReaction)
	{
		// the flying-object reaction, once (the predators flee from it); the thrower: the hand's player (PLAYER_ONE's
		// interface here)
		ecs::animal_ai::SpreadFlyingObjectReaction(entity, PlayerNames::PLAYER_ONE);
	}
	return PhysicsState().objects.back().get();
}
} // namespace

void PhysicsObjects::RemoveObject(entt::entity entity)
{
	for (size_t i = 0; i < PhysicsState().objects.size(); ++i)
	{
		if (PhysicsState().objects[i]->entity == entity)
		{
			RemoveAt(i);
			ForgetThrower(entity);
			// (inferred, not read) out of the physics without EndPhysics: back in the map cells at once while it exists, not at
			// the next map_cells::Sync. Nothing for a resting proxy (it never left) or one held out (a tornado's). The
			// original's only insert is at the end of physics: never for an UNAVAILABLE (marked) object
			if (ecs::IsAvailable(entity))
			{
				map_cells::InsertMapObject(entity);
			}
			return;
		}
	}
}

void PhysicsObjects::RemoveObjectWithEndPhysics(entt::entity entity)
{
	for (size_t i = 0; i < PhysicsState().objects.size(); ++i)
	{
		if (PhysicsState().objects[i]->entity != entity)
		{
			continue;
		}
		auto* self = PhysicsState().objects[i].get();
		// the angles, position and altitude from the body, then EndPhysics(po, insert_back_into_map = true)
		const bool landed = (self->flags & PhysicsObject::k_Landed) != 0;
		const auto kept = EndPhysics(*self);
		// LANDED and the returned object on land -> its landing sound; the GameTurnUpdate stop never plays it
		if (auto& registry = Locator::entitiesRegistry::value(); landed && registry.Valid(kept))
		{
			if (const auto& handlers = PhysicsObjects::Handlers(kept);
			    handlers.dropSfx && sea_cells::IsLand(registry.Get<const Transform>(kept).position))
			{
				handlers.dropSfx(kept);
			}
		}
		for (size_t j = 0; j < PhysicsState().objects.size(); ++j)
		{
			if (PhysicsState().objects[j].get() == self)
			{
				RemoveAt(j);
				break;
			}
		}
		ForgetThrower(entity);
		return;
	}
}

void PhysicsObjects::RaiseUntilNotIntersecting(PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the cells of the two corners (C.x - R, C.z - R) and (C.x + R, C.z + R) (BoxCell: clamped into the map when off
	// it), the low and high of the two
	const auto centre = po.body.Centre();
	const float radius = po.body.Radius();
	const auto first = BoxCell(centre.x - radius, centre.z - radius);
	const auto second = BoxCell(centre.x + radius, centre.z + radius);
	const auto low = glm::min(first, second);
	const auto high = glm::max(first, second);
	// x outer, z inner, in the map, the fixed list then the mobile one (map_cells::ForEachInCell; a multi-cell building
	// is in every cell it covers). AddProxy puts each in the physics list at once, so a later cell of the same object
	// skips it ("not in the physics list yet"): here the candidates list does that
	std::vector<entt::entity> candidates;
	for (int32_t x = low.x; x <= high.x; ++x)
	{
		for (int32_t z = low.y; z <= high.y; ++z)
		{
			const glm::ivec2 cell(x, z);
			if (!map_coords::InBounds(cell))
			{
				continue;
			}
			map_cells::ForEachInCell(cell, [&](entt::entity entity) {
				// not the thrower && InteractsWithPhysicsObjects (false for a vortex or a map shield) && not in the physics
				// list yet && it has a 3D object
				if (entity != po.entity && entity != po.thrower && InteractsWithPhysicsObjects(entity) &&
				    Find(entity) == nullptr && registry.AllOf<Mesh>(entity) &&
				    std::find(candidates.begin(), candidates.end(), entity) == candidates.end())
				{
					candidates.push_back(entity);
				}
				return true;
			});
		}
	}
	for (const auto entity : candidates)
	{
		AddProxy(entity);
	}
	// go up by the first push over 0.001 and start again, until nothing pushes. The original has no limit; the 1000
	// rounds only guard openblack against a body that could never leave another.
	for (int round = 0; round < 1000; ++round)
	{
		float best = 0.0f;
		bool raised = false;
		for (const auto& other : PhysicsState().objects)
		{
			if (other.get() == &po)
			{
				continue;
			}
			// a villager is not raised over what a Living pushed, nor that over a villager
			if ((po.villager && (other->flags & PhysicsObject::k_PushedByLiving) != 0) ||
			    (other->villager && (po.flags & PhysicsObject::k_PushedByLiving) != 0))
			{
				continue;
			}
			const float reach = other->body.Radius() + po.body.Radius();
			const auto d = po.body.Centre() - other->body.Centre();
			if (!(reach * reach > glm::dot(d, d)))
			{
				continue;
			}
			const float down = po.body.PenetrationAlong(other->body, glm::vec3(0.0f, -1.0f, 0.0f));
			const float up = other->body.PenetrationAlong(po.body, glm::vec3(0.0f, 1.0f, 0.0f));
			best = std::max(best, std::max(down, up));
			if (best > 0.001f)
			{
				// C.y += best: the object's origin goes up, then PhysicsBody::SetUpPos
				po.body.SetUpPos(po.body.Rotation(), po.body.ObjectOrigin() + glm::vec3(0.0f, best, 0.0f));
				SyncTurnStart(po); // SetUpPos copies the matrix into the turn-start one
				raised = true;
				break;
			}
		}
		if (!raised)
		{
			break;
		}
	}
}

bool PhysicsObjects::BackInMap(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	// an UNAVAILABLE object (a zombie of the dead list) is neither put back in the cells nor deleted; for every caller
	// of the end of physics, the villager's and the animal's too
	if (!ecs::IsAvailable(entity))
	{
		return false;
	}
	// with insert and the object available: inside the map it goes back in the cells, outside it is deleted
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr || map_coords::InBounds(map_coords::FromWorld(nullptr, transform->position)))
	{
		map_cells::InsertMapObject(entity);
		return true;
	}
	ToBeDeleted(entity);
	return false;
}

PhysicsObject* PhysicsObjects::Find(entt::entity entity)
{
	for (auto& po : PhysicsState().objects)
	{
		if (po->entity == entity)
		{
			return po.get();
		}
	}
	return nullptr;
}

glm::mat3 PhysicsObjects::DrawQuarterTurn(const glm::mat3& rows)
{
	return ::DrawQuarterTurn(rows);
}

glm::mat3 PhysicsObjects::RotationFromRows(entt::entity entity, const glm::mat3& rows)
{
	const auto& registry = Locator::entitiesRegistry::value();
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	if (registry.AnyOf<Villager, Animal, Tree, Feature>(entity))
	{
		affine::DecomposeYXZ(rows, y, x, z);
		return affine::AngleY(y); // the yaw only
	}
	if (registry.AnyOf<MobileStatic, MobileObject, Pot, DeadTree, Fragment, Scaffold, OneOffSpellSeed>(entity))
	{
		affine::DecomposeYXZ(rows, y, x, z);
		return affine::RotationYXZ(y, x, z); // all three angles
	}
	return rows;
}

bool PhysicsObjects::IsFlying(entt::entity entity)
{
	const auto* po = Find(entity);
	return po != nullptr && !po->body.resting;
}

float PhysicsObjects::PushObject(entt::entity object, entt::entity pusher)
{
	(void)pusher; // the force does not depend on the pusher
	// the force is weight x 9.81. (approximate) Weight() is not the original's weight for Abode / StoragePit /
	// Fragment, and the dead tree's is not ported
	const float force = Weight(object) * 9.81f;
	PhysicsObject* po = nullptr;
	if (particle_carried_objects::IsCarried(object))
	{
		// carried by a particle system (in physics with no body): the start of physics gives no body to an object
		// already in physics: (inferred) nothing then, so no force
		return 0.0f;
	}
	if (IsFlying(object)) // in physics with a body
	{
		po = Find(object);
	}
	else
	{
		po = AddObject(object, glm::vec3(0.0f), glm::vec3(0.0f)); // no velocity, no spin
	}
	if (po == nullptr)
	{
		return 0.0f;
	}
	po->flags |= PhysicsObject::k_PushedByLiving;
	// the object's MapCoords as a point (with its altitude); d = T - it
	const auto at = map_coords::ToWorld(object::MapCoordsOf(object));
	auto d = po->body.Centre() - at;
	// all three exactly 0 adds nothing; else d x F / sqrt((dz dz + dy dy) + dx dx)
	if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f)
	{
		const float s = force / std::sqrt(d.z * d.z + d.y * d.y + d.x * d.x);
		d = glm::vec3(d.x * s, d.y * s, d.z * s);
	}
	po->body.externalForce.x += d.x;
	po->body.externalForce.y += d.y;
	po->body.externalForce.z += d.z;
	return 0.0f;
}

void PhysicsObjects::GameTurnUpdate()
{
	// the fragments' timers run from Game::GameLogicLoop, not here
	BeginTurn();
	for (int substep = 0; substep < PhysicsBody::k_SubstepsPerTurn; ++substep)
	{
		Substep();
	}
	// the prediction counts its turns while active
	if (PhysicsState().prediction.active)
	{
		++PhysicsState().prediction.turns;
	}
	EndTurn();
	// the object's Pos and angles are set every substep (the game logic sees the end of the turn)
	auto& registry = Locator::entitiesRegistry::value();
	bool moved = false;
	for (const auto& po : PhysicsState().objects)
	{
		if (!po->body.resting && registry.Valid(po->entity))
		{
			moved = true;
			SyncTransform(*po);
			if (const auto& handlers = PhysicsObjects::Handlers(po->entity); handlers.moved)
			{
				handlers.moved(po->entity);
			}
		}
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

void PhysicsObjects::UpdateFrame(float turnFraction, float seconds)
{
	Dust::Update(std::min(seconds, 0.25f));
	// every frame for each awake entry, the turn-start matrix is lerped to the end one cell by cell by the turn
	// fraction, each row normalised and scaled, and T - R s com taken as the origin.
	// A body at rest has no entry of its own pose: it is drawn at its Transform (the end of the turn).
	// (pending) an animated object's frame, and the quarter turn of animated meshes other than villagers and animals
	auto& registry = Locator::entitiesRegistry::value();
	const float f = turnFraction;
	bool moving = false;
	for (const auto& po : PhysicsState().objects)
	{
		// (dead list plan) an UNAVAILABLE (marked) object is not posed: the original's pose pass has no availability test,
		// but a marked object's body goes at the next turn start and the object is out of the cells, so it is not drawn
		if (!ecs::IsAvailable(po->entity))
		{
			continue;
		}
		if (po->body.resting)
		{
			if (registry.AllOf<PhysicsDrawPose>(po->entity))
			{
				registry.Remove<PhysicsDrawPose>(po->entity);
			}
			continue;
		}
		// posed only when -Radius < T.y (the current centre), so a body sunk deeper than its radius keeps the pose it was
		// last drawn at
		if (!(-po->body.Radius() < po->body.Centre().y))
		{
			continue;
		}
		moving = true;
		const auto& r1 = po->body.Rotation();
		const auto& r0 = po->turnStarted ? po->turnStartRotation : r1;
		const auto c1 = po->body.Centre();
		const auto c0 = po->turnStarted ? po->turnStartCentre : c1;
		glm::mat3 rotation;
		for (int row = 0; row < 3; ++row)
		{
			rotation[row] = r0[row] + (r1[row] - r0[row]) * f; // in the original's operation order
		}
		affine::NormaliseRows(rotation);
		// an animated (boned) mesh takes the quarter turn before the scale and the origin.
		// (pending) the other boned objects (the reward chest, the dolphin, the shark, the ballista, some gates) keep
		// openblack's Transform convention; only Villager / Animal bodies are in the original's axes
		if (registry.AnyOf<Villager, Animal>(po->entity))
		{
			rotation = DrawQuarterTurn(rotation);
		}
		const auto centre = c0 + (c1 - c0) * f;
		auto& pose = registry.AllOf<PhysicsDrawPose>(po->entity) ? registry.Get<PhysicsDrawPose>(po->entity)
		                                                         : registry.Assign<PhysicsDrawPose>(po->entity);
		pose.rotation = rotation;
		pose.position = po->body.ObjectOrigin(rotation, centre);
	}
	// while the prediction is active and its object has a 3D object, its matrix = history[k] lerped to history[k + 1]
	// by the turn fraction, all 12 floats ((h1 - h0) f + h0) with k = the turns since < N, else history[N]; the rows
	// normalised, then PhysicsBody::DrawOrigin.
	// (pending) an animated mesh's frame; the draw itself is the renderer's, from PhysicsDrawPose. (pending) the hand
	// skips drawing the held object while it is the prediction's. (openblack guards) Valid, a mesh and a history stand
	// for the 3D object test
	if (PhysicsState().prediction.active && registry.Valid(PhysicsState().prediction.object) &&
	    MeshOf(PhysicsState().prediction.object) != nullptr && !PhysicsState().prediction.history.empty())
	{
		const auto& history = PhysicsState().prediction.history;
		const size_t n = history.size() - 1;
		glm::mat3 rotation;
		glm::vec3 centre;
		if (PhysicsState().prediction.turns < n)
		{
			const auto& [r0, c0] = history[PhysicsState().prediction.turns];
			const auto& [r1, c1] = history[PhysicsState().prediction.turns + 1];
			for (int row = 0; row < 3; ++row)
			{
				rotation[row] = (r1[row] - r0[row]) * f + r0[row];
			}
			centre = (c1 - c0) * f + c0;
		}
		else
		{
			rotation = history[n].first;
			centre = history[n].second;
		}
		affine::NormaliseRows(rotation);
		const auto object = PhysicsState().prediction.object;
		auto& pose = registry.AllOf<PhysicsDrawPose>(object) ? registry.Get<PhysicsDrawPose>(object)
		                                                     : registry.Assign<PhysicsDrawPose>(object);
		pose.rotation = rotation;
		pose.position = PhysicsState().prediction.body.DrawOrigin(rotation, centre);
		moving = true;
	}
	if (moving)
	{
		registry.SetDirty(); // the drawn instances change every frame while something flies
	}
}

void PhysicsObjects::BeginPrediction(entt::entity object)
{
	EndPrediction();
	PhysicsState().prediction.active = true;
	PhysicsState().prediction.object = object;
}

void PhysicsObjects::SetPrediction(entt::entity object, PhysicsBody body, std::vector<std::pair<glm::mat3, glm::vec3>> history)
{
	if (!PhysicsState().prediction.active || PhysicsState().prediction.object != object)
	{
		BeginPrediction(object);
	}
	PhysicsState().prediction.body = std::move(body);
	PhysicsState().prediction.history = std::move(history);
	PhysicsState().prediction.turns = 0;
}

bool PhysicsObjects::IsPredictionObject(entt::entity object)
{
	return PhysicsState().prediction.active && PhysicsState().prediction.object == object;
}

void PhysicsObjects::EndPredictionOf(entt::entity object)
{
	if (PhysicsState().prediction.active && PhysicsState().prediction.object == object)
	{
		EndPrediction();
	}
}

void PhysicsObjects::Clear()
{
	EndPrediction();
	PhysicsState().objects.clear();
	PhysicsState().capacity = 0;
}

PhysicsClass PhysicsObjects::ClassOf(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<MapShield>(entity))
	{
		return PhysicsClass::Shield;
	}
	if (registry.AllOf<Villager>(entity))
	{
		return PhysicsClass::Villager;
	}
	if (registry.AllOf<Animal>(entity))
	{
		return PhysicsClass::Animal;
	}
	if (registry.AllOf<Tree>(entity))
	{
		return PhysicsClass::Tree;
	}
	if (registry.AllOf<DeadTree>(entity))
	{
		return PhysicsClass::DeadTree;
	}
	if (registry.AllOf<Pot>(entity))
	{
		return PhysicsClass::Pot;
	}
	if (registry.AllOf<Fragment>(entity))
	{
		return PhysicsClass::Fragment;
	}
	if (registry.AnyOf<Abode, StoragePit>(entity))
	{
		return PhysicsClass::Building;
	}
	if (registry.AllOf<Scaffold>(entity))
	{
		return PhysicsClass::Scaffold;
	}
	if (Rocks::IsRock(entity))
	{
		return PhysicsClass::Rock;
	}
	if (registry.AllOf<Creature>(entity))
	{
		return PhysicsClass::Creature;
	}
	return PhysicsClass::Other;
}

void PhysicsObjects::CheckAllCreaturesForCatching(entt::entity object, PhysicsObject& po)
{
	// (pending) the walk of the creature list and each creature's tests and plan
	if (PhysicsState().creatureCatchHook)
	{
		PhysicsState().creatureCatchHook(object, po);
	}
}

void PhysicsObjects::SetCreatureCatchHook(std::function<void(entt::entity object, PhysicsObject& po)> hook)
{
	PhysicsState().creatureCatchHook = std::move(hook);
}

void PhysicsObjects::SetClassHandlers(PhysicsClass type, ClassHandlers handlers)
{
	PhysicsState().classHandlers.at(static_cast<size_t>(type)) = std::move(handlers);
}

void PhysicsObjects::ForEach(const std::function<void(const PhysicsObject&)>& func)
{
	for (const auto& object : PhysicsState().objects)
	{
		func(*object);
	}
}
