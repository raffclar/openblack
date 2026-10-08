/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Buildings.h"

#include <cstdlib>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "CollisionSounds.h"
#include "Dust.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/DrawMesh.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireGraphic.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "FragMesh.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "PartialBuild.h"
#include "PhysicsObjects.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;

namespace
{
void EraseMesh(entt::id_type id)
{
	if (id != 0 && Locator::resources::value().GetMeshes().Contains(id))
	{
		Locator::resources::value().GetMeshes().Erase(id);
	}
}

/// Redraws the damaged building from its FragMesh (a building with one draws it instead of its mesh).
void RedrawBuilding(entt::entity building, BuildingDamage& damage)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(building);
	const auto toWorld = affine::Model(transform);
	// The FragMesh, then the intact model partly built at GetPercentForDrawBuilding. (pending, with the repairs) the
	// FragMesh made anew from the intact model when the percent is in [0.2, 1) at the next hit
	// The percent is how far the building is repaired since the damage: with a FragMesh and the repair site made at the
	// hit, (life - the site's starting life) / (1 - the site's starting life)
	const float percent = ecs::abodes::GetPercentForDrawBuilding(building);
	// the morphable's melting: the land deltas stay once MorphWithTerrain is taken off below
	PartialBuildOptions options;
	options.melting = damage.morphed || registry.AllOf<MorphWithTerrain>(building);
	auto partial = percent < 1.0f ? PartialBuild::Build(building, damage.intactMesh, percent, options)
	                              : std::vector<graphics::L3DSubMesh::GeneratedPrimitive> {};
	const auto id = damage.mesh->BuildMesh(glm::inverse(toWorld), "fragmesh", std::move(partial));
	auto& meshes = Locator::resources::value().GetMeshes();
	if (id != 0 && meshes.Contains(damage.intactMesh))
	{
		// the building's mark on the landscape stays (a broken building can still be repaired)
		meshes.Handle(id)->SetFootprintSource(meshes.Handle(damage.intactMesh).handle());
	}
	// the Mesh stays the intact model (components::DrawMesh: sizes, map cells, the static shadow, the body); the
	// FragMesh's model is the building's DrawMesh. A damaged building draws it instead of the partly built model, so the
	// construction draw goes as abodes::RedrawConstruction takes it away for HasDestructionMesh. Removed before the
	// Assign: the on_destroy<DrawMesh> (Abodes.cpp OnDrawMeshDestroyed) erases the model it held, ours or the
	// construction draw's
	const auto old = damage.generatedMesh;
	registry.Remove<AbodeConstructionDraw, DrawMesh, NotDrawn>(building);
	damage.generatedMesh = id;
	if (id != 0)
	{
		// every sub-mesh of the generated one (a big building needs several)
		const auto submesh = meshes.Handle(id)->GetNumSubMeshes() > 1 ? static_cast<int8_t>(-1) : static_cast<int8_t>(0);
		registry.Assign<DrawMesh>(building, id, submesh, registry.Get<const Mesh>(building).bbSubmeshId);
	}
	// that sink is connected only once some building site has been drawn: the previous model is erased here as well
	// (EraseMesh checks Contains: nothing to do after OnDrawMeshDestroyed)
	EraseMesh(old);
	// the FragMesh has the landscape morph baked in
	if (registry.AllOf<MorphWithTerrain>(building))
	{
		damage.morphed = true;
		registry.Remove<MorphWithTerrain>(building);
	}
	registry.SetDirty();
}

/// A piece of the building flies: the fragment is made and its physics set up.
void CreateFragment(const FragMesh::Piece& piece, entt::entity parent)
{
	auto& registry = Locator::entitiesRegistry::value();
	const float area = piece.mesh->Area();
	// the hull is the vertices and a copy 0.45 behind each: its radius is at least the farthest vertex
	const auto id = piece.mesh->BuildMesh(glm::mat4(1.0f), "fragment");
	if (id == 0)
	{
		return;
	}
	std::vector<glm::vec3> points;
	std::vector<glm::vec3> normals;
	piece.mesh->UniqueVertices(points, normals);
	float radius = 0.0f;
	for (size_t i = 0; i < points.size(); ++i)
	{
		radius = std::max({radius, glm::length(points[i]), glm::length(points[i] - 0.45f * normals[i])});
	}
	if (0.2f * radius > area / (2.0f * radius)) // a sliver goes at once
	{
		EraseMesh(id);
		return;
	}
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, piece.centre, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(entity, id, static_cast<int8_t>(0), static_cast<int8_t>(0));
	auto& fragment = registry.Assign<Fragment>(entity);
	fragment.mesh = piece.mesh;
	fragment.parent = parent;
	fragment.generatedMesh = id;
	fragment.turnsLeft = 100 * static_cast<int>(piece.lifeTriangles);
	fragment.area = area;
	if (std::getenv("OPENBLACK_PHYSICS_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: fragment {} tris area {:.1f} v ({:.1f},{:.1f},{:.1f}) w {:.2f}",
		                   piece.mesh->TriangleCount(), area, piece.velocity.x, piece.velocity.y, piece.velocity.z,
		                   glm::length(piece.angularVelocity));
	}
	// one dust puff per distinct vertex (colour 0x80706050, size 2, +-2 units per second)
	for (const auto& p : points)
	{
		Dust::Emit(piece.centre + p, Dust::SyncedRandomVelocity(), 0x80706050u, 2.0f);
	}
	if (auto* po = PhysicsObjects::AddObject(entity, piece.velocity, piece.angularVelocity, parent))
	{
		po->flags |= PhysicsObject::k_NoObjectCollision;
	}
	registry.SetDirty();
}
} // namespace

bool Buildings::PhysicallyDestroysAbodes(entt::entity entity)
{
	const auto type = PhysicsObjects::ConstantsType(entity);
	return !Locator::entitiesRegistry::value().AllOf<Fragment>(entity) && (type == 3 || type == 20);
}

bool Buildings::ReactToPhysicsImpact(entt::entity building, PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* hit = po.hitBy;
	if (hit == nullptr || !registry.Valid(hit->entity))
	{
		return true;
	}
	// the player of the hand that threw it, inherited by what it hit (openblack's byPlayer, the local player's hand)
	const std::optional<PlayerNames> player = hit->byPlayer ? std::optional(PlayerNames::PLAYER_ONE) : std::nullopt;
	// Before the test of what hit it: with a player, and the building's own body marked as thrown from the hand, the
	// player's creature may learn to copy the damage by throwing. The test is on the building's body, as the original's
	// is; a building's body is a resting obstacle that nothing marks so, so in practice it does not happen
	if (ecs::creature_mimic::ShouldMimicBuildingHit(player, po.flags))
	{
		ecs::creature_mimic::Consider(*player, creature_watching::Deed::DamageByThrowingAt, building);
	}
	if (!PhysicallyDestroysAbodes(hit->entity))
	{
		return true;
	}
	// (not ported, no creature) byCreature when the thrower (the proxy's, else the hitter's) is a creature
	constexpr bool byCreature = false;
	const float p = glm::length(hit->body.velocity) * hit->body.Mass();
	if (std::getenv("OPENBLACK_PHYSICS_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: {} hit by {} p {:.0f} (v {:.1f}, m {:.0f})",
		                   static_cast<uint32_t>(building), static_cast<uint32_t>(hit->entity), p,
		                   glm::length(hit->body.velocity), hit->body.Mass());
	}
	if (p > 2000.0f && !ecs::abode_queries::IsBuilt(building))
	{
		// not built (a building site): no FragMesh, the damage goes straight to the life (the crush effect preset times
		// a crush defence of 0.2)
		return ecs::abodes::OnPhysicalDamage(building, {std::nullopt, hit->entity, player, byCreature});
	}
	if (p > 2000.0f)
	{
		auto* damage = registry.TryGet<BuildingDamage>(building);
		if (damage == nullptr)
		{
			damage = &registry.Assign<BuildingDamage>(building);
			damage->intactMesh = registry.Get<const Mesh>(building).id;
		}
		// (openblack, for the draw snapshot) the impact works on a FragMesh of its own: a new one, or a copy of the
		// building's, which takes the old one's place once broken (a FragMeshDraw taken before keeps the old one)
		std::shared_ptr<FragMesh> broken;
		if (!damage->mesh)
		{
			broken = FragMesh::FromEntity(building);
			if (!broken)
			{
				return true;
			}
		}
		else
		{
			if (damage->lastHitter == hit->entity)
			{
				// the same rock again: they stop colliding, so it goes through
				po.thrower = hit->entity;
				hit->thrower = building;
			}
			else
			{
				damage->lastHitter = hit->entity;
			}
			broken = std::make_shared<FragMesh>(*damage->mesh);
		}
		// (a repair would rebuild it from the intact model once the draw percent reaches 0.2; nobody repairs yet)
		auto pieces = broken->Impact(hit->body.Centre(), hit->body.velocity * 0.3f, hit->body.Radius() + 0.7f);
		const float remaining = broken->GetRemaining();
		damage->mesh = std::move(broken);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Buildings: impact p {:.0f}, {} pieces, {:.2f} left", p, pieces.size(),
		                   remaining);
		// the Fragments are made inside FragMesh::Impact, before the remaining part is read
		for (const auto& piece : pieces)
		{
			CreateFragment(piece, building);
		}
		if (remaining >= 1.0f)
		{
			// nothing counted as lost (the broken part was only halved triangles): the FragMesh goes and the building
			// draws whole again, with no damage and no sound
			Buildings::RemoveDamage(building);
			return true;
		}
		// the life (and the repair baseline) first: the redraw's partly built percent comes from them.
		if (!ecs::abodes::OnPhysicalDamage(building, {remaining, hit->entity, player, byCreature}))
		{
			return false;
		}
		RedrawBuilding(building, registry.Get<BuildingDamage>(building));
		return true;
	}
	const auto at = registry.Get<const Transform>(building).position;
	// p > 1000 level 2, p > 300 level 3: a rock on the ground sound from editor.sad's table (G_Rock_V_Ground_M / _S)
	if (p > 1000.0f)
	{
		CollisionSounds::PlayAnimEffect({2, 0, 0x16, 0x10, 75}, building, at, false);
	}
	else if (p > 300.0f)
	{
		CollisionSounds::PlayAnimEffect({3, 0, 0x16, 0x10, 75}, building, at, false);
	}
	return true;
}

entt::entity Buildings::FragmentEndPhysics(entt::entity fragment, const PhysicsObject& /*po*/)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& f = registry.Get<Fragment>(fragment);
	// the parent must still be available
	auto* damage = ecs::IsAvailable(f.parent) ? registry.TryGet<BuildingDamage>(f.parent) : nullptr;
	if (f.area > 9.0f && damage != nullptr && damage->mesh)
	{
		const auto& transform = registry.Get<const Transform>(fragment);
		const auto toWorld = glm::translate(glm::mat4(1.0f), transform.position) * glm::mat4(transform.rotation);
		// (openblack, for the draw snapshot) merged into a copy, which takes the old FragMesh's place
		auto merged = std::make_shared<FragMesh>(*damage->mesh);
		merged->Merge(*f.mesh, toWorld);
		damage->mesh = std::move(merged);
		RedrawBuilding(f.parent, *damage);
		DestroyFragment(fragment);
		return entt::null;
	}
	f.parent = entt::null;
	return fragment;
}

void Buildings::Redraw(entt::entity building)
{
	if (auto* damage = Locator::entitiesRegistry::value().TryGet<BuildingDamage>(building); damage != nullptr && damage->mesh)
	{
		RedrawBuilding(building, *damage);
	}
}

void Buildings::RemoveDamage(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* damage = registry.TryGet<BuildingDamage>(building);
	if (damage == nullptr)
	{
		return;
	}
	damage->mesh.reset();
	damage->lastHitter = entt::null; // it went with the FragMesh
	if (damage->generatedMesh != 0 || damage->morphed)
	{
		// the FragMesh's DrawMesh goes (only ours: the Mesh was never changed), then its model (a no-op when
		// OnDrawMeshDestroyed has erased it)
		if (const auto* draw = registry.TryGet<const DrawMesh>(building); draw != nullptr && draw->id == damage->generatedMesh)
		{
			registry.Remove<DrawMesh>(building);
		}
		EraseMesh(damage->generatedMesh);
		damage->generatedMesh = 0;
		if (damage->morphed && !registry.AllOf<MorphWithTerrain>(building))
		{
			registry.Assign<MorphWithTerrain>(building);
		}
		damage->morphed = false;
		// without the FragMesh a building with a site is drawn partly built again (the construction DrawMesh; nothing for
		// one without a site)
		ecs::abodes::RedrawConstruction(building);
		registry.SetDirty();
	}
}

namespace
{
/// The mesh cache only (no component, no map cell, no physics: PhysicsObjects::RemoveObject would touch the
/// entity's PhysicsDrawPose and the cells, so it must not run inside the destroy signal). A no-op when the DrawMesh's
/// listener erased the same model (EraseMesh checks Contains). The component is still there during the signal.
void OnBuildingDamageDestroyed(entt::registry& registry, entt::entity entity)
{
	// (openblack, guard) a reset after the resources have gone (shutdown)
	if (Locator::resources::has_value())
	{
		EraseMesh(registry.get<BuildingDamage>(entity).generatedMesh);
	}
}
} // namespace

void Buildings::ConnectDamageListener()
{
	// entt's sink::connect disconnects the same listener first: idempotent
	Locator::entitiesRegistry::value().OnDestroy<BuildingDamage>().connect<&OnBuildingDamageDestroyed>();
}

void Buildings::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> expired;
	registry.Each<Fragment>([&](entt::entity entity, Fragment& f) {
		// a parent no longer available is forgotten
		if (f.parent != entt::null && !ecs::IsAvailable(f.parent))
		{
			f.parent = entt::null;
		}
		if (--f.turnsLeft <= 0)
		{
			expired.push_back(entity);
		}
	});
	for (const auto entity : expired)
	{
		DestroyFragment(entity);
	}
}

void Buildings::ForgetHitter(entt::entity hitter, entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (building != entt::null)
	{
		if (auto* damage = registry.TryGet<BuildingDamage>(building); damage != nullptr && damage->mesh)
		{
			damage->lastHitter = entt::null;
		}
		return;
	}
	registry.Each<BuildingDamage>([hitter](entt::entity, BuildingDamage& damage) {
		if (damage.mesh && damage.lastHitter == hitter)
		{
			damage.lastHitter = entt::null;
		}
	});
}

void Buildings::DestroyFragment(entt::entity fragment)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(fragment))
	{
		return;
	}
	PhysicsObjects::RemoveObject(fragment);
	if (const auto* f = registry.TryGet<const Fragment>(fragment))
	{
		EraseMesh(f->generatedMesh);
	}
	ecs::map_cells::RemoveMapObject(fragment); // out of the map cells, as any deleted object
	registry.Destroy(fragment);
	registry.SetDirty();
}

void Buildings::AppendFragMeshes(graphics::world_triangles::Frame& out, const FragMesh::FrameLight& frame, float turnTime)
{
	std::vector<FragMeshDraw> draws; // this frame's (no FragMesh held on)
	SnapshotFragMeshes(draws, frame, turnTime);
	AppendFragMeshes(out, draws);
}

void Buildings::AppendFragMeshes(graphics::world_triangles::Frame& out, const std::vector<FragMeshDraw>& draws)
{
	for (const auto& draw : draws)
	{
		draw.mesh->AppendDraw(out, draw.world ? &*draw.world : nullptr, draw.light);
	}
}

void Buildings::SnapshotFragMeshes(std::vector<FragMeshDraw>& out, const FragMesh::FrameLight& frame, float turnTime)
{
	out.clear();
	auto& registry = Locator::entitiesRegistry::value();
	// a broken building's FragMesh (only while it is what the building draws)
	registry.Each<const BuildingDamage, const Transform, const DrawMesh>(
	    [&](entt::entity entity, const BuildingDamage& damage, const Transform& transform, const DrawMesh& draw) {
		    if (!damage.mesh || damage.generatedMesh == 0 || draw.id != damage.generatedMesh)
		    {
			    return;
		    }
		    // with a fire the tint is the charring grey and the specular tint the glow, both with alpha 0xFF; else
		    // 0xFFFFFFFF / 0. The same pair as the other burning objects (RenderingSystem's Burning)
		    uint32_t tint = 0xFFFFFFFFu;
		    uint32_t tintSpecular = 0;
		    if (const auto* fire = ecs::fire::Find(entity); fire != nullptr)
		    {
			    const uint32_t grey = ecs::fire::graphic::CharringGrey(*fire);
			    // turnTime = the frame's DrawSceneDesc::clock: the same value as the live read
			    const auto glow = ecs::fire::graphic::CharringGlow(*fire, turnTime);
			    tint = argb_colour::Argb(grey, grey, grey, 0xFF);
			    tintSpecular = argb_colour::Argb(glow.r, glow.g, glow.b, 0xFF);
		    }
		    // no matrix (the triangles are in the world), the building's position
		    out.push_back({damage.mesh, std::nullopt, FragMesh::ObjectLight(transform.position, tint, tintSpecular, frame),
		                   transform.position, tint, tintSpecular});
	    });
	// a fragment's world matrix and its translation; its tint stays the FragMesh's default 0xFFFFFFFF / 0, nothing sets
	// it again. While it flies that matrix is the pose between its last two turns (components::PhysicsDrawPose), the
	// scale Transform's
	registry.Each<const Fragment, const Transform>(
	    [&](entt::entity entity, const Fragment& fragment, const Transform& transform) {
		    if (!fragment.mesh)
		    {
			    return;
		    }
		    const auto* flying = registry.TryGet<const PhysicsDrawPose>(entity);
		    const auto position = flying != nullptr ? flying->position : transform.position;
		    const auto matrix = flying != nullptr ? affine::Model(flying->position, flying->rotation, transform.scale)
		                                          : affine::Model(transform);
		    out.push_back(
		        {fragment.mesh, matrix, FragMesh::ObjectLight(position, 0xFFFFFFFFu, 0, frame), position, 0xFFFFFFFFu, 0});
	    });
}
