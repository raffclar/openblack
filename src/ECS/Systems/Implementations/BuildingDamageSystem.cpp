/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "BuildingDamageSystem.h"

#include <algorithm>
#include <array>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>

#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "GameBuildingDamageWorld.h"
#include "Magic/MagicWorldInterface.h"
#include "Magic/SpellRules.h"
#include "Physics/Body.h"
#include "Physics/BodyShapes.h"
#include "Physics/LivingRules.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace damage = openblack::physics::damage;

namespace
{
/// The sound keys of a light knock, a heavier knock and a building breaking: the level of the sound, then its codes
constexpr std::array<int32_t, 5> k_KnockKeys {3, 0, 0x16, 0x10, 75};
constexpr std::array<int32_t, 5> k_HardKnockKeys {2, 0, 0x16, 0x10, 75};
constexpr std::array<int32_t, 5> k_CrashKeys {1, 0, 0x16, 9, 75};
/// A creature's blow on a building
constexpr std::array<int32_t, 5> k_SmashKeys {2, 1, 1, 9, 75};
/// How far behind each triangle its back face is drawn
constexpr float k_BackFace = 0.45f;
/// A creature's blow breaks a building this far round where it struck, for each of its size
constexpr float k_CreatureBlowReach = 3.75f;
/// The dust a piece sheds at each of its corners as it starts to fly
constexpr uint32_t k_PieceDustArgb = 0x80706050;
constexpr float k_PieceDustSize = 2.0f;
constexpr float k_PieceDustSpeed = 0.02f;
/// The random spread of a piece's spin and its dust, drawn from 0 to 200 about 100
constexpr uint32_t k_SpreadDraws = 201;
constexpr float k_SpreadMiddle = 100.0f;

/// A random draw of the game's synchronised numbers; nothing without them
uint32_t GameRand(building_world::World& world, uint32_t n)
{
	auto* random = world.Random();
	return random != nullptr ? random->GameRand(n) : 0;
}

/// A random spread about nothing, scaled
float Spread(building_world::World& world, float scale)
{
	return (static_cast<float>(GameRand(world, k_SpreadDraws)) - k_SpreadMiddle) * scale;
}

/// Three spreads drawn z first, as the game draws them
glm::vec3 SpreadVector(building_world::World& world, float scale)
{
	const float z = Spread(world, scale);
	const float y = Spread(world, scale);
	const float x = Spread(world, scale);
	return {x, y, z};
}

void PlaySound(building_world::World& world, entt::entity building, std::span<const int32_t> keys)
{
	const auto* transform = world.Entities().TryGet<const Transform>(building);
	world.PlaySound(keys, building, transform != nullptr ? transform->position : glm::vec3(0.0f));
}

/// Whether a building is built
bool IsBuilt(const building_world::World& world, entt::entity building)
{
	const auto* progress = world.Entities().TryGet<const BuildProgress>(building);
	return progress == nullptr || progress->built >= 1.0f;
}

/// How much of a building is drawn standing
float DrawShare(const building_world::World& world, entt::entity building)
{
	const auto& registry = world.Entities();
	const auto* progress = registry.TryGet<const BuildProgress>(building);
	// Drawn by its repair only while it is broken and has a site for the repair
	const auto* broken = registry.TryGet<const BuildingDamage>(building);
	const auto* site = registry.TryGet<const RepairSite>(building);
	return damage::DrawShare(world.LifeOf(building), progress != nullptr ? progress->built : 1.0f,
	                         broken != nullptr && site != nullptr ? std::optional(site->startLife) : std::nullopt);
}

/// The building's model, the drawn parts of its nearest level of detail, in the world
damage::Mesh MakeDamageMesh(const building_world::World& world, entt::entity building)
{
	damage::Mesh mesh;
	const auto& registry = world.Entities();
	const auto* meshComponent = registry.TryGet<const Mesh>(building);
	const auto* transform = registry.TryGet<const Transform>(building);
	if (meshComponent == nullptr || transform == nullptr)
	{
		return mesh;
	}
	const auto toWorld = [transform](glm::vec3 p) {
		return transform->position + transform->rotation * (p * transform->scale);
	};
	// A model lying over the land's shape is lowered with it, the land under each corner against the land under its origin
	const bool followsLand = registry.AllOf<MorphWithTerrain>(building);
	const float baseHeight = world.LandHeight(glm::vec2(transform->position.x, transform->position.z));
	for (auto& part : world.ModelParts(meshComponent->id))
	{
		damage::Primitive primitive {.material = part.material};
		for (auto& corners : part.triangles)
		{
			for (auto& corner : corners)
			{
				corner.position = toWorld(corner.position);
				if (followsLand)
				{
					corner.position.y -= baseHeight - world.LandHeight(glm::vec2(corner.position.x, corner.position.z));
				}
			}
			primitive.triangles.push_back(damage::MakeTriangle(corners));
		}
		damage::LinkNeighbours(primitive);
		mesh.primitives.push_back(std::move(primitive));
	}
	mesh.trianglesAtCreation = static_cast<uint32_t>(mesh.TriangleCount());
	mesh.remaining = 1.0f;
	return mesh;
}

/// The faces a broken mesh is drawn with: each triangle, its back face behind it and a wall on each open side, every
/// face lit flat by its own normal. Its corners are taken into the model's own space by a function.
std::vector<building_world::DrawnPart> DrawnFaces(const damage::Mesh& mesh, const std::function<glm::vec3(glm::vec3)>& toModel)
{
	std::vector<building_world::DrawnPart> parts;
	for (const auto& primitive : mesh.primitives)
	{
		if (primitive.triangles.empty())
		{
			continue;
		}
		building_world::DrawnPart made {.material = primitive.material};
		const auto add = [&made](glm::vec3 position, glm::vec2 uv, glm::vec3 normal) {
			made.vertices.push_back({.position = position, .uv = uv, .normal = normal});
			return static_cast<uint16_t>(made.vertices.size() - 1);
		};
		for (const auto& triangle : primitive.triangles)
		{
			const auto& c = triangle.corners;
			auto normal = glm::cross(c[1].position - c[0].position, c[2].position - c[0].position);
			const float length = glm::length(normal);
			normal = length > 0.0f ? normal / length : glm::vec3(0.0f, 1.0f, 0.0f);
			std::array<glm::vec3, 3> front {};
			std::array<glm::vec3, 3> back {};
			for (size_t i = 0; i < 3; ++i)
			{
				front.at(i) = toModel(c.at(i).position);
				back.at(i) = toModel(c.at(i).position - normal * k_BackFace);
			}
			// The light falls on each face as its own normal turns it, in the model's space
			auto frontNormal = glm::cross(front[1] - front[0], front[2] - front[0]);
			const float frontLength = glm::length(frontNormal);
			frontNormal = frontLength > 0.0f ? frontNormal / frontLength : glm::vec3(0.0f, 1.0f, 0.0f);
			const auto backNormal = -frontNormal;
			std::array<uint16_t, 3> f {};
			std::array<uint16_t, 3> b {};
			for (size_t i = 0; i < 3; ++i)
			{
				f.at(i) = add(front.at(i), c.at(i).uv, frontNormal);
			}
			for (size_t i = 0; i < 3; ++i)
			{
				b.at(i) = add(back.at(i), c.at(i).uv, backNormal);
			}
			made.indices.insert(made.indices.end(), {f[0], f[1], f[2], b[0], b[2], b[1]});
			// A wall joins the front and back faces along each side nothing joins on to
			for (size_t side = 0; side < 3; ++side)
			{
				if (triangle.neighbours.at(side) >= 0)
				{
					continue;
				}
				const auto next = (side + 1) % 3;
				const auto fa = add(front.at(side), c.at(side).uv, frontNormal);
				const auto fb = add(front.at(next), c.at(next).uv, frontNormal);
				const auto ba = add(back.at(side), c.at(side).uv, backNormal);
				const auto bb = add(back.at(next), c.at(next).uv, backNormal);
				made.indices.insert(made.indices.end(), {fa, ba, fb, fb, ba, bb});
			}
		}
		parts.push_back(std::move(made));
	}
	return parts;
}

/// Draws the building with what is left of its model
void RedrawBuilding(building_world::World& world, entt::entity building, BuildingDamage& broken)
{
	auto& registry = world.Entities();
	const auto& transform = registry.Get<const Transform>(building);
	const auto& meshComponent = registry.Get<const Mesh>(building);
	const bool followsLand = registry.AllOf<MorphWithTerrain>(building);
	const float baseHeight = world.LandHeight(glm::vec2(transform.position.x, transform.position.z));
	// Into the model's own space, undoing the lowering over the land, which its drawing lays on again
	const auto toModel = [&world, &transform, followsLand, baseHeight](glm::vec3 point) {
		if (followsLand)
		{
			point.y += baseHeight - world.LandHeight(glm::vec2(point.x, point.z));
		}
		return (glm::transpose(transform.rotation) * (point - transform.position)) / transform.scale;
	};
	world.EraseModel(broken.drawMesh);
	++broken.version;
	broken.drawMesh = world.MakeModel(fmt::format("broken/{}/{}", entt::to_integral(building), broken.version),
	                                  meshComponent.id, DrawnFaces(broken.mesh, toModel));
	registry.SetDirty();
}

void RemoveDamage(building_world::World& world, entt::entity building)
{
	auto& registry = world.Entities();
	if (auto* broken = registry.TryGet<BuildingDamage>(building))
	{
		world.EraseModel(broken->drawMesh);
		registry.Remove<BuildingDamage>(building);
	}
}

/// What makes a piece fly: its velocity, its spin about its own axes and the building it passes through
void Launch(building_world::World& world, entt::entity piece, entt::entity parent, glm::vec3 velocity, glm::vec3 spin)
{
	auto* dynamics = world.Dynamics();
	if (dynamics == nullptr)
	{
		return;
	}
	const auto started =
	    dynamics->InitialisePhysics(piece, {.velocity = velocity, .spin = spin, .thrower = parent, .add = true});
	if (started.entry != nullptr)
	{
		// Pieces hit nothing but the land
		started.entry->flags |= PhysicsEntry::k_NoObjectCollision;
	}
}

/// A piece sheds dust at its corners as it starts to fly, all but the first it finds; too thin a piece then goes
void ShedDustAndCheck(building_world::World& world, entt::entity piece)
{
	auto& registry = world.Entities();
	const auto& transform = registry.Get<const Transform>(piece);
	const auto& part = registry.Get<const BuildingPiece>(piece);
	if (part.mesh.primitives.empty())
	{
		return;
	}
	std::vector<std::array<glm::vec3, 3>> triangles;
	std::vector<glm::vec3> corners;
	for (const auto& triangle : part.mesh.primitives.front().triangles)
	{
		triangles.push_back({triangle.corners[0].position, triangle.corners[1].position, triangle.corners[2].position});
		for (const auto& corner : triangle.corners)
		{
			if (std::ranges::find(corners, corner.position) == corners.end())
			{
				corners.push_back(corner.position);
			}
		}
	}
	for (size_t i = 1; i < corners.size(); ++i)
	{
		const auto velocity = SpreadVector(world, k_PieceDustSpeed);
		if (auto* dynamics = world.Dynamics())
		{
			// Where snow lies a puff grows by the snow's depth and takes on the land's colour
			const auto at = transform.position + transform.rotation * corners[i];
			const auto snow = world.SnowAt(at);
			dynamics->AddPuff(at, velocity, k_PieceDustSize * (1.0f + static_cast<float>(snow) / 255.0f),
			                  world.DustTint(k_PieceDustArgb, snow));
		}
	}
	if (physics::shapes::Fragment(triangles).tooThin)
	{
		world.EraseModel(part.drawMesh);
		world.Remove(piece);
	}
}

/// A piece of a mesh: its triangles about their middle, an object of its own there
entt::entity MakePiece(building_world::World& world, damage::Primitive primitive, glm::vec3 offset, entt::entity parent,
                       entt::id_type sourceMesh, uint8_t snowLevel)
{
	auto& registry = world.Entities();
	const auto centre = damage::CentreOf(primitive);
	damage::Offset(primitive, -centre);
	damage::LinkNeighbours(primitive);
	const auto piece = registry.Create();
	registry.Assign<Transform>(piece, offset + centre, glm::mat3(1.0f), glm::vec3(1.0f));
	const auto triangles = static_cast<uint32_t>(primitive.triangles.size());
	auto& part = registry.Assign<BuildingPiece>(piece);
	part.mesh.primitives.push_back(std::move(primitive));
	part.mesh.trianglesAtCreation = triangles;
	part.mesh.snowLevel = snowLevel;
	part.mesh.snowFrozen = true;
	part.parent = parent;
	part.sourceMesh = sourceMesh;
	part.turnsLeft = triangles * damage::k_TurnsPerTriangle;
	return piece;
}

void DrawPiece(building_world::World& world, entt::entity piece)
{
	auto& registry = world.Entities();
	auto& part = registry.Get<BuildingPiece>(piece);
	world.EraseModel(part.drawMesh);
	part.drawMesh = world.MakeModel(fmt::format("piece/{}", entt::to_integral(piece)), part.sourceMesh,
	                                DrawnFaces(part.mesh, [](glm::vec3 p) { return p; }));
	registry.AssignOrReplace<Mesh>(piece, part.drawMesh, static_cast<int8_t>(0), static_cast<int8_t>(1));
	registry.SetDirty();
}

/// The parts of a mesh that no longer hold on fall away as pieces, spinning a little, from where they are
void SplitLooseParts(building_world::World& world, damage::Mesh& mesh, glm::vec3 offset, bool groundTest, entt::entity parent,
                     entt::id_type sourceMesh)
{
	const auto groups = damage::LabelGroups(mesh);
	std::optional<std::function<float(glm::vec2)>> land;
	if (groundTest)
	{
		// The mesh's corners are in the world when the land is tested
		land = [&world](glm::vec2 point) { return world.LandHeight(point); };
	}
	const auto anchors = damage::Anchors(mesh, groups, land);
	auto parts = damage::TakeAwayLooseGroups(mesh, anchors, groups);
	auto& registry = world.Entities();
	for (auto& part : parts)
	{
		const auto piece = MakePiece(world, std::move(part.primitive), offset, parent, sourceMesh, mesh.snowLevel);
		// As every piece is made, it lets go of its own parts that don't hold on to its first, before it spins away;
		// it stays about where it was made
		auto childMesh = std::move(registry.Get<BuildingPiece>(piece).mesh);
		const auto childPosition = registry.Get<const Transform>(piece).position;
		SplitLooseParts(world, childMesh, childPosition, false, parent, sourceMesh);
		registry.Get<BuildingPiece>(piece).mesh = std::move(childMesh);
		DrawPiece(world, piece);
		const auto spin = SpreadVector(world, damage::k_FallingSpin);
		Launch(world, piece, parent, glm::vec3(0.0f), spin);
		ShedDustAndCheck(world, piece);
	}
}

/// Breaks a building's model where a blow struck it: what broke off each primitive flies with the blow, and what no
/// longer holds on to the land falls away
void Strike(building_world::World& world, entt::entity building, BuildingDamage& broken, glm::vec3 point, glm::vec3 velocity,
            float reach)
{
	auto& registry = world.Entities();
	const auto sourceMesh = registry.Get<const Mesh>(building).id;
	auto brokenOff = damage::Strike(broken.mesh, point, velocity, reach);
	for (auto& primitive : brokenOff)
	{
		const auto piece = MakePiece(world, std::move(primitive), glm::vec3(0.0f), building, sourceMesh, broken.mesh.snowLevel);
		// The piece lets go of its own parts that don't hold on to its first, then gathers about what is left of it. Its
		// mesh is worked on apart, as the pieces falling from it are made.
		auto mesh = std::move(registry.Get<BuildingPiece>(piece).mesh);
		auto position = registry.Get<const Transform>(piece).position;
		SplitLooseParts(world, mesh, position, false, building, sourceMesh);
		if (!mesh.primitives.empty())
		{
			auto& kept = mesh.primitives.front();
			const auto centre = damage::CentreOf(kept);
			damage::Offset(kept, -centre);
			position += centre;
		}
		registry.Get<BuildingPiece>(piece).mesh = std::move(mesh);
		registry.Get<Transform>(piece).position = position;
		DrawPiece(world, piece);
		// Three draws the game makes and drops, then the spin
		for (int i = 0; i < 3; ++i)
		{
			GameRand(world, k_SpreadDraws);
		}
		const auto spin = SpreadVector(world, damage::k_BrokenSpin);
		Launch(world, piece, building, velocity, spin);
		ShedDustAndCheck(world, piece);
	}
	SplitLooseParts(world, broken.mesh, glm::vec3(0.0f), true, building, sourceMesh);
}

/// What breaking does to a building: it crashes, and loses life down to what is left of its model
void ApplyBreakage(building_world::World& world, entt::entity building, entt::entity hitter, std::optional<PlayerNames> player,
                   bool hitterIsCreature)
{
	PlaySound(world, building, k_CrashKeys);
	auto values = world.Crush();
	if (!values.has_value())
	{
		return;
	}
	const auto& registry = world.Entities();
	if (const auto* broken = registry.TryGet<const BuildingDamage>(building))
	{
		// The help's sprites about destroying buildings would show here for the local player's blow leaving less than
		// 0.4 of it; openblack has no help system yet
		values->Scale(damage::BreakageShare(world.LifeOf(building), broken->mesh.remaining));
		// Divided by its defences, which taking the effect multiplies back, so its life comes down to what is left
		if (const auto* info = world.InfoOf(building))
		{
			const auto defence = magic::EffectDefence::From(*info);
			for (size_t kind = 0; kind < values->numbers.size(); ++kind)
			{
				// (The game divides by a zero multiplier too; nothing ships one, and it is skipped here rather than made
				// infinite)
				if (defence.multipliers.at(kind) != 0.0f)
				{
					values->numbers.at(kind) /= defence.multipliers.at(kind);
				}
			}
		}
	}
	const bool validHitter = hitter != entt::null && registry.Valid(hitter);
	world.ApplyEffect(building, *values,
	                  magic::EffectSource {
	                      .player = player.value_or(PlayerNames::NEUTRAL),
	                      .casterCreature = hitterIsCreature ? hitter : entt::null,
	                      .appliedBy = validHitter ? hitter : entt::null,
	                      .playerless = !player.has_value(),
	                  });
}

/// After a blow: a building that lost nothing of its model forgets it was broken; otherwise it is drawn as it is left
bool Settle(building_world::World& world, entt::entity building, BuildingDamage& broken)
{
	const float remaining = damage::RemainingFraction(broken.mesh);
	if (remaining == 1.0f)
	{
		RemoveDamage(world, building);
		return false;
	}
	broken.mesh.remaining = remaining;
	RedrawBuilding(world, building, broken);
	return true;
}
} // namespace

BuildingDamageSystem::BuildingDamageSystem()
    : BuildingDamageSystem(std::make_unique<GameBuildingDamageWorld>())
{
}

BuildingDamageSystem::BuildingDamageSystem(std::unique_ptr<building_world::World> world)
    : _world(std::move(world))
{
}

BuildingDamageSystem::~BuildingDamageSystem() = default;

void BuildingDamageSystem::ReactToImpact(DynamicsSystemInterface& dynamics, PhysicsEntry& entry, const ImpactInfo& impact)
{
	Blow(dynamics, entry.entity, entry, impact, false);
}

void BuildingDamageSystem::ReactToPassedOnImpact(DynamicsSystemInterface& dynamics, entt::entity building, PhysicsEntry& struck,
                                                 const ImpactInfo& impact)
{
	Blow(dynamics, building, struck, impact, true);
}

void BuildingDamageSystem::Blow(DynamicsSystemInterface& dynamics, entt::entity building, PhysicsEntry& entry,
                                const ImpactInfo& impact, bool passedOn)
{
	auto& world = *_world;
	auto& registry = world.Entities();
	if (!registry.Valid(building))
	{
		return;
	}
	const auto hitter = impact.hitBy;
	if (hitter == entt::null || !registry.Valid(hitter))
	{
		return;
	}
	auto* hitterEntry = dynamics.Find(hitter);
	// A creature may copy the player damaging the building by throwing at it, when the building's own body came from a
	// hand; a building's resting body never does, so this is never seen
	if (hitterEntry != nullptr && hitterEntry->player.has_value() && entry.Has(PhysicsEntry::k_FromHand))
	{
		const auto* place = registry.TryGet<const Transform>(building);
		world.PlayerDid(physics::living::k_DeedDamageByThrowingAt, place != nullptr ? place->position : glm::vec3(0.0f),
		                building, *hitterEntry->player);
	}
	if (!dynamics.PhysicallyDestroysAbodes(hitter) || hitterEntry == nullptr || hitterEntry->body == nullptr)
	{
		return;
	}
	const auto& rock = *hitterEntry->body;
	const float momentum = glm::length(rock.velocity) * rock.Mass();
	switch (damage::JudgeBlow(momentum))
	{
	case damage::Blow::Nothing:
		return;
	case damage::Blow::Knock:
		PlaySound(world, building, k_KnockKeys);
		return;
	case damage::Blow::HardKnock:
		PlaySound(world, building, k_HardKnockKeys);
		return;
	case damage::Blow::Breaks:
		break;
	}
	// TODO(buildings): while the blow's effects apply, the game marks a building struck by what a creature threw (a flag
	// whose reader wasn't found); the rock, not the creature, applies them
	if (IsBuilt(world, building))
	{
		auto* broken = registry.TryGet<BuildingDamage>(building);
		if (broken == nullptr || damage::RebuildsOnBlow(DrawShare(world, building)))
		{
			// A fresh broken model from the whole one, which no rock has struck yet
			if (broken != nullptr)
			{
				world.EraseModel(broken->drawMesh);
			}
			auto& made = registry.AssignOrReplace<BuildingDamage>(building);
			made.mesh = MakeDamageMesh(world, building);
			broken = &made;
		}
		else if (broken->lastHitter == hitter)
		{
			// The same rock again, hard, on the building's own body: the rock and the building pass through each other from
			// now on
			if (momentum > damage::k_PassThroughMomentum && entry.entity == building)
			{
				entry.thrower = hitter;
				hitterEntry->thrower = building;
			}
		}
		else
		{
			broken->lastHitter = hitter;
		}
		auto point = rock.Centre();
		auto velocity = rock.velocity * damage::k_PieceSpeedShare;
		if (passedOn)
		{
			// A blow passed on lands on a point of the model, from the game's synchronised random numbers, and its pieces
			// fly with none of the rock's speed: what it is multiplied by is never set in the game, so is always nothing
			if (auto* random = world.Random())
			{
				const auto landed =
				    damage::RandomSurfacePoint(broken->mesh, [random](float limit) { return random->GameFloatRand(limit); });
				point = landed.value_or(point);
			}
			velocity *= 0.0f;
		}
		Strike(world, building, *broken, point, velocity, rock.Radius() + damage::k_ReachBeyondRock);
		if (!Settle(world, building, registry.Get<BuildingDamage>(building)))
		{
			return;
		}
	}
	// The harm is put down to the player of the thing that struck, not the building's own
	ApplyBreakage(world, building, hitter, hitterEntry->player, false);
}

void BuildingDamageSystem::Smash(entt::entity building, entt::entity creature, float creatureSize)
{
	auto& world = *_world;
	auto& registry = world.Entities();
	if (!registry.Valid(building) || !registry.AllOf<Mesh, Transform>(building))
	{
		return;
	}
	if (!registry.AllOf<BuildingDamage>(building))
	{
		registry.Assign<BuildingDamage>(building).mesh = MakeDamageMesh(world, building);
	}
	PlaySound(world, building, k_SmashKeys);
	// Struck straight down where the building stands
	const auto point = registry.Get<const Transform>(building).position;
	Strike(world, building, registry.Get<BuildingDamage>(building), point, glm::vec3(0.0f, -1.0f, 0.0f),
	       creatureSize * k_CreatureBlowReach);
	if (!Settle(world, building, registry.Get<BuildingDamage>(building)))
	{
		return;
	}
	ApplyBreakage(world, building, creature, world.PlayerOf(creature), true);
}

entt::entity BuildingDamageSystem::PieceAtRest(DynamicsSystemInterface& dynamics, PhysicsEntry* entry, entt::entity piece,
                                               bool insert)
{
	auto& world = *_world;
	auto& registry = world.Entities();
	const auto kept = dynamics.EndPhysicsAsObject(piece, insert, entry != nullptr);
	if (!registry.Valid(piece))
	{
		return kept;
	}
	auto& part = registry.Get<BuildingPiece>(piece);
	const auto& transform = registry.Get<const Transform>(piece);
	float area = 0.0f;
	if (!part.mesh.primitives.empty())
	{
		for (const auto& triangle : part.mesh.primitives.front().triangles)
		{
			const auto& c = triangle.corners;
			area += 0.5f * glm::length(glm::cross(c[1].position - c[0].position, c[2].position - c[0].position));
		}
	}
	// A big piece goes back into its building as rubble, where it lies
	auto* broken =
	    part.parent != entt::null && registry.Valid(part.parent) ? registry.TryGet<BuildingDamage>(part.parent) : nullptr;
	if (area > damage::k_RubbleArea && broken != nullptr && !part.mesh.primitives.empty())
	{
		auto rubble = part.mesh.primitives.front();
		for (auto& triangle : rubble.triangles)
		{
			for (auto& corner : triangle.corners)
			{
				corner.position = transform.position + transform.rotation * corner.position;
			}
		}
		damage::LinkNeighbours(rubble);
		broken->mesh.primitives.push_back(std::move(rubble));
		RedrawBuilding(world, part.parent, *broken);
		world.EraseModel(part.drawMesh);
		world.Remove(piece);
		return entt::null;
	}
	part.parent = entt::null;
	part.mesh.snowFrozen = false;
	return kept;
}

void BuildingDamageSystem::ForgetHitter(entt::entity rock)
{
	_world->Entities().Each<BuildingDamage>([rock](BuildingDamage& broken) {
		if (broken.lastHitter == rock)
		{
			broken.lastHitter = entt::null;
		}
	});
}

void BuildingDamageSystem::ProcessTurn()
{
	auto& world = *_world;
	auto& registry = world.Entities();
	std::vector<entt::entity> expired;
	registry.Each<BuildingPiece>([&registry, &expired](entt::entity piece, BuildingPiece& part) {
		if (part.parent != entt::null && !registry.Valid(part.parent))
		{
			part.parent = entt::null;
		}
		if (part.turnsLeft > 0)
		{
			--part.turnsLeft;
		}
		if (part.turnsLeft == 0)
		{
			expired.push_back(piece);
		}
	});
	for (const auto piece : expired)
	{
		world.EraseModel(registry.Get<BuildingPiece>(piece).drawMesh);
		world.Remove(piece);
	}
}

std::optional<float> BuildingDamageSystem::PartialShare(entt::entity building) const
{
	const auto& world = *_world;
	const auto& registry = world.Entities();
	// A broken building, one being repaired, or one still being built
	if (!registry.Valid(building) ||
	    (!registry.AllOf<BuildingDamage>(building) && !registry.AllOf<RepairSite>(building) && IsBuilt(world, building)))
	{
		return std::nullopt;
	}
	// Nothing is drawn at none, and the whole model at all of it
	const float share = DrawShare(world, building);
	if (!(share > 0.0f) || share >= 1.0f)
	{
		return std::nullopt;
	}
	return share;
}

bool BuildingDamageSystem::DrawsWhole(entt::entity object) const
{
	const auto& world = *_world;
	const auto& registry = world.Entities();
	// A building with a site for its building or repair is drawn only as far up as it stands, nothing at none, the
	// whole of it once all of it stands; a broken one keeps its broken model drawn under that
	if (!registry.Valid(object) || registry.AllOf<BuildingDamage>(object) ||
	    (!registry.AllOf<RepairSite>(object) && IsBuilt(world, object)))
	{
		return true;
	}
	return DrawShare(world, object) >= 1.0f;
}

entt::id_type BuildingDamageSystem::DrawnMesh(entt::entity object, entt::id_type own) const
{
	const auto* broken = _world->Entities().TryGet<const BuildingDamage>(object);
	return broken != nullptr && broken->drawMesh != 0 ? broken->drawMesh : own;
}

void BuildingDamageSystem::Reset()
{
	auto& world = *_world;
	auto& registry = world.Entities();
	// The made models go, and the buildings forget they were broken (their pieces are taken with the level)
	registry.Each<BuildingDamage>([&world](BuildingDamage& broken) { world.EraseModel(broken.drawMesh); });
	registry.Each<BuildingPiece>([&world](BuildingPiece& part) { world.EraseModel(part.drawMesh); });
	std::vector<entt::entity> broken;
	registry.Each<const BuildingDamage>(
	    [&broken](entt::entity building, const BuildingDamage&) { broken.push_back(building); });
	for (const auto building : broken)
	{
		registry.Remove<BuildingDamage>(building);
	}
}
