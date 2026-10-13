/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PickingSystem.h"

#include <algorithm>
#include <iterator>
#include <span>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLine.h"
#include "3D/MapCoords.h"
#include "Creature/CreatureMorph.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AppleBarrel.h"
#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SeeThrough.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/UndrawnPickModel.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/Vortex.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlightRules.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/UndrawnPicks.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The land's corner heights as the game reads them for its line test: the cell's own block holds its far corners
std::optional<land_line::CellHeights> CornersOf(int32_t x, int32_t z)
{
	if (!Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	const auto* cell = Locator::terrainSystem::value().FindCell({static_cast<uint16_t>(x), static_cast<uint16_t>(z)});
	if (cell == nullptr)
	{
		return std::nullopt;
	}
	// Within a block's 17 by 17 cells, +1 is across z and +17 across x
	return land_line::CellHeights {
	    .here = cell[0].altitude, .acrossX = cell[17].altitude, .acrossZ = cell[1].altitude, .acrossBoth = cell[18].altitude};
}

float HeightAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

/// The middle of the model a seed shows in its picture, as the model is made (neither turned nor scaled), nothing for a
/// seed that shows no model
glm::vec3 SeedModelMiddle(SpellSeedType seed, float ownerAlignment)
{
	if (seed == SpellSeedType::None || !Locator::infoConstants::has_value() || !Locator::resources::has_value())
	{
		return glm::vec3(0.0f);
	}
	const auto model = magic::GetSeedModel(Locator::infoConstants::value(), seed, ownerAlignment);
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto id = model.has_value() ? resources::HashIdentifier(*model) : entt::id_type {0};
	if (!model.has_value() || !meshes.Contains(id))
	{
		return glm::vec3(0.0f);
	}
	return meshes.Handle(id)->GetBoundingBox().Center();
}

/// A player's alignment, nobody's being 0
float AlignmentOf(PlayerNames player)
{
	return Locator::alignmentSystem::has_value() && player != PlayerNames::NEUTRAL
	           ? Locator::alignmentSystem::value().GetPlayerAlignment(player)
	           : 0.0f;
}

/// The half extents of a mesh's box, none for a mesh that isn't there
glm::vec3 HalfExtentsOf(entt::id_type mesh)
{
	auto& meshes = Locator::resources::value().GetMeshes();
	return meshes.Contains(mesh) ? meshes.Handle(mesh)->GetBoundingBox().Size() * 0.5f : glm::vec3(0.0f);
}

} // namespace

std::optional<glm::vec3> PickingSystem::LandAlong(glm::vec3 from, glm::vec3 to) const
{
	if (const auto hit = land_line::LandAlong(from, to, CornersOf))
	{
		return glm::vec3(hit->x, HeightAt(*hit), hit->y);
	}
	return std::nullopt;
}

std::optional<glm::vec3> PickingSystem::LandOrSeaAlong(glm::vec3 from, glm::vec3 to, glm::vec3 camera) const
{
	if (const auto hit = land_line::LandOrSeaAlong(from, to, camera, CornersOf))
	{
		return glm::vec3(hit->x, HeightAt(*hit), hit->y);
	}
	return std::nullopt;
}

std::optional<glm::vec3> PickingSystem::LandUnderPixel(glm::vec3 camera, glm::vec3 nearPoint, Sea sea) const
{
	// The camera's own feel: the line from the camera through the pixel, carried on to the map's edge, meets the land, or
	// else the sea near the camera
	if (sea == Sea::NearCamera)
	{
		return LandOrSeaAlong(camera, nearPoint, camera);
	}
	if (const auto point = land_line::UnderPixel(camera, nearPoint, sea == Sea::Anywhere, CornersOf, HeightAt))
	{
		return land_line::KeptInReach(*point);
	}
	return std::nullopt;
}

PickingSystemInterface::Pick picking::Locked(const PickingSystemInterface::Pick& previous, std::optional<glm::vec3> land,
                                             float landDistance)
{
	auto pick = previous;
	if (land.has_value())
	{
		pick.land = land;
		pick.point = land;
		pick.distance = landDistance;
	}
	return pick;
}

void picking::CarryHover(const PickingSystemInterface::Pick& previous, PickingSystemInterface::Pick& next, float seconds)
{
	if (next.object == previous.hoverObject)
	{
		next.hoverObject = previous.hoverObject;
		next.hoverSeconds = previous.hoverSeconds + seconds;
	}
	else
	{
		next.hoverObject = next.object;
		next.hoverSeconds = 0.0f;
	}
}

void picking::WeighScreenShapes(PickingSystemInterface::Pick& pick, const PickingSystemInterface::ScreenShapes& shapes,
                                bool objectIsCreature)
{
	pick.textBubble = false;
	pick.rope = false;
	std::optional<float> bubble = shapes.textBubble;
	// The text bubble nearer than the object drops the object; no nearer, the object keeps the cursor
	if (pick.object.has_value())
	{
		if (bubble.has_value() && *bubble < pick.distance)
		{
			pick.object.reset();
		}
		else
		{
			bubble.reset();
		}
	}
	// With no object, the land nearer than the bubble drops it
	if (!pick.object.has_value() && bubble.has_value() && pick.landDistance < *bubble)
	{
		bubble.reset();
	}
	// The rope touched strictly nearer than what is left takes the cursor, but never from a creature
	if (shapes.rope.has_value())
	{
		const float rope = *shapes.rope;
		if (pick.object.has_value())
		{
			if (!objectIsCreature && rope < pick.distance)
			{
				pick.object.reset();
				pick.rope = true;
			}
		}
		else if (bubble.has_value())
		{
			if (rope < *bubble)
			{
				bubble.reset();
				pick.rope = true;
			}
		}
		else if (rope < pick.landDistance)
		{
			pick.rope = true;
		}
	}
	if (pick.object.has_value())
	{
		return;
	}
	if (bubble.has_value())
	{
		pick.textBubble = true;
		pick.point = shapes.textBubblePoint;
		pick.distance = *bubble;
		return;
	}
	pick.point = pick.land;
	pick.distance = pick.landDistance;
}

namespace
{
/// Along a line, how far (in its direction's lengths) it first meets a broken building's triangles, from either side; a
/// triangle nearly edge on to the line is passed by
std::optional<float> BrokenAlong(const physics::damage::Mesh& mesh, glm::vec3 from, glm::vec3 direction)
{
	constexpr float k_EdgeOn = 0.005f;
	std::optional<float> nearest;
	for (const auto& primitive : mesh.primitives)
	{
		for (const auto& triangle : primitive.triangles)
		{
			const auto a = triangle.corners[0].position;
			const auto b = triangle.corners[1].position;
			const auto c = triangle.corners[2].position;
			auto normal = glm::cross(b - a, c - a);
			if (normal != glm::vec3(0.0f))
			{
				normal = glm::normalize(normal);
			}
			const float facing = glm::dot(direction, normal);
			if (std::abs(facing) <= k_EdgeOn)
			{
				continue;
			}
			const float along = (glm::dot(normal, a) - glm::dot(from, normal)) / facing;
			const auto point = from + direction * along;
			// Inside when the point is on the same side of all three sides
			const auto side = [&normal, &point](glm::vec3 p, glm::vec3 q) {
				return glm::dot(glm::cross(point - p, q - p), normal) > 0.0f ? 1 : 0;
			};
			const int inside = side(a, b) + side(b, c) + side(c, a);
			if ((inside == 0 || inside == 3) && (!nearest.has_value() || along < *nearest))
			{
				nearest = along;
			}
		}
	}
	return nearest;
}
} // namespace

void PickingSystem::PickUnderCursor(const Frame& frame)
{
	const auto& view = frame.view;

	// The land or sea under the cursor, counted a little further than it is
	float landDistance = std::numeric_limits<float>::max();
	std::optional<glm::vec3> land;
	if (const auto under = land_line::UnderPixel(view.camera, frame.nearPoint, true, CornersOf, HeightAt))
	{
		landDistance = screen_pick::Depth(view, *under) + land_line::k_LandDistanceAllowance;
		land = land_line::KeptInReach(*under);
	}

	// Gripping the land, the interface keeps what it picked and follows only the land
	_locked = frame.locked;
	_seconds = frame.seconds;
	if (frame.locked)
	{
		_pick = picking::Locked(_pick, land, landDistance);
		return;
	}

	const auto previous = _pick;
	_previous = previous;
	_pick = {};
	_pick.land = land;
	_pick.landDistance = landDistance;
	if (!Locator::rendereringSystem::has_value() || !Locator::resources::has_value())
	{
		_pick.point = _pick.land;
		_pick.distance = landDistance;
		picking::CarryHover(previous, _pick, frame.seconds);
		return;
	}

	// The objects drawn this frame, in the order they are drawn: a hand, what a hand holds, a see-through copy and a
	// building not begun are not there for the cursor
	const auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	_candidates.clear();
	_candidateEntities.clear();
	_candidateModels.clear();
	_candidateMeshes.clear();
	// The miracle held in the hand is one of the things the hand holds
	const auto heldSeed =
	    Locator::magicSystem::has_value() ? Locator::magicSystem::value().GetHeldSeed() : std::optional<entt::entity> {};
	// Models that are never drawn but take the cursor, each tested just before the drawn thing it goes with
	const auto& drawnObjects = Locator::rendereringSystem::value().GetContext().drawnObjects;
	_drawnEntities.clear();
	std::ranges::transform(drawnObjects, std::back_inserter(_drawnEntities), &RenderContext::DrawnObject::entity);
	_undrawnShapes.clear();
	registry.Each<const UndrawnPickModel>([this](const entt::entity entity, const UndrawnPickModel& shape) {
		if (shape.takesCursor)
		{
			_undrawnShapes.push_back({.entity = entity, .drawnWith = shape.drawnWith});
		}
	});
	ecs::undrawn_picks::TestOrder(_drawnEntities, _undrawnShapes, _testOrder);
	for (const auto& turn : _testOrder)
	{
		if (turn.undrawn.has_value())
		{
			const auto entity = *turn.undrawn;
			const auto& shape = registry.Get<const UndrawnPickModel>(entity);
			const auto* transform = registry.TryGet<const Transform>(entity);
			if (transform == nullptr || !meshes.Contains(shape.mesh))
			{
				continue;
			}
			// Placed, turned, then scaled, as a drawn model is
			const auto model = glm::translate(glm::mat4(1.0f), transform->position) * glm::mat4(transform->rotation) *
			                   glm::scale(glm::mat4(1.0f), transform->scale);
			const auto box = meshes.Handle(shape.mesh)->GetBoundingBox();
			const auto halfExtents = box.Size() * 0.5f;
			_candidates.push_back({
			    .centre = glm::vec3(model * glm::vec4(box.Center(), 1.0f)),
			    .radius = glm::length(halfExtents) * transform->scale.x,
			    .origin = transform->position,
			    .halfExtents = glm::vec2(halfExtents.x, halfExtents.z),
			});
			_candidateEntities.push_back(entity);
			_candidateModels.push_back(model);
			_candidateMeshes.push_back(shape.mesh);
			continue;
		}
		const auto& [entity, model] = drawnObjects[turn.drawnIndex];
		if (!registry.Valid(entity) || registry.AnyOf<Hand, InHand, SeeThrough>(entity) || heldSeed == entity)
		{
			continue;
		}
		// The apple barrel and its worm are only drawn: the cursor meets the land behind them
		if (registry.AnyOf<AppleBarrel, AppleBarrelWorm>(entity))
		{
			continue;
		}
		// So are the lanterns at a temple's entrance, by its creature's pen, and the explorers' boat film: the cursor
		// meets what is behind them
		if (registry.AnyOf<TempleLantern, BoatFilmPart>(entity))
		{
			continue;
		}
		// A gate stone on a plinth is found only while the plinth stands closed
		if (const auto* stone = registry.TryGet<const PlinthStone>(entity); stone != nullptr && !stone->pickable)
		{
			continue;
		}
		if (const auto* progress = registry.TryGet<const BuildProgress>(entity);
		    progress != nullptr && registry.AllOf<Abode>(entity) && progress->built == 0.0f)
		{
			continue;
		}
		const auto* meshComponent = registry.TryGet<const Mesh>(entity);
		if (meshComponent == nullptr || !meshes.Contains(meshComponent->id))
		{
			continue;
		}
		const auto mesh = meshes.Handle(meshComponent->id);
		const auto* transform = registry.TryGet<const Transform>(entity);
		const float scale = transform != nullptr ? transform->scale.x : 1.0f;
		const auto box = mesh->GetBoundingBox();
		const auto halfExtents = box.Size() * 0.5f;
		_candidates.push_back({
		    .centre = glm::vec3(model * glm::vec4(box.Center(), 1.0f)),
		    .radius = glm::length(halfExtents) * scale,
		    .origin = glm::vec3(model[3]),
		    .halfExtents = glm::vec2(halfExtents.x, halfExtents.z),
		});
		_candidateEntities.push_back(entity);
		_candidateModels.push_back(model);
		_candidateMeshes.push_back(meshComponent->id);
		// A script's scroll is also picked by a ball a little larger than its model, about the model's middle
		if (const auto* highlight = registry.TryGet<const ScriptHighlight>(entity);
		    highlight != nullptr && ecs::script_highlights::PickedByBall(highlight->kind))
		{
			AddBall(entity, model, meshComponent->id, halfExtents,
			        {.centre = highlight->centre, .radius = highlight->radius + ecs::script_highlights::k_PickBallMargin});
		}
		// A miracle dispenser is also picked by a wide ball round the bubble it floats, which takes the dispenser itself
		if (registry.AllOf<SpellDispenser>(entity) && transform != nullptr)
		{
			AddBall(entity, model, meshComponent->id, halfExtents,
			        ecs::pick_shapes::DispenserBall(transform->position, halfExtents, scale));
		}
		// A miracle icon is also picked by a ball round the miracle's picture it holds up
		if (const auto* shown = registry.TryGet<const IconSeed>(entity); shown != nullptr && shown->seed != SpellSeedType::None)
		{
			const auto owner = ecs::world_objects::IconPlayer(registry, entity);
			AddBall(entity, model, meshComponent->id, halfExtents,
			        ecs::pick_shapes::PictureBall(shown->middle, ecs::pick_shapes::k_IconPictureSize,
			                                      SeedModelMiddle(shown->seed, AlignmentOf(owner))));
		}
	}
	AddBubbles(view.camera);
	AddFireBalls();
	AddVortices();
	AddTeleportStones();
	AddLeashPosts();

	// How far along the view an object's first triangle under the cursor is
	const auto distanceOf = [&](size_t i) -> std::optional<float> {
		const auto entity = _candidateEntities[i];
		const auto& model = _candidateModels[i];
		// People are picked by their bounding sphere alone: anywhere in its circle on the screen, at the sphere's
		// distance rather than their body's
		if (registry.AllOf<Villager>(entity))
		{
			const auto* transform = registry.TryGet<const Transform>(entity);
			const float scale = transform != nullptr ? transform->scale.x : 1.0f;
			const float halfHeight = meshes.Handle(_candidateMeshes[i])->GetBoundingBox().Size().y * 0.5f;
			return screen_pick::SphereDistance(view, _candidates[i].origin, halfHeight * scale, _candidates[i].radius);
		}
		// A broken building is picked by its broken model's triangles, along the cursor's line
		if (const auto* broken = registry.TryGet<const BuildingDamage>(entity); broken != nullptr && broken->drawMesh != 0)
		{
			const auto along = BrokenAlong(broken->mesh, view.camera, frame.nearPoint - view.camera);
			return along.has_value() ? std::optional<float>(*along * view.near) : std::nullopt;
		}
		const auto mesh = meshes.Handle(_candidateMeshes[i]);
		// A tree of a forest is picked through its leaves' holes
		const auto* info = world_objects::InfoOf(entity);
		const bool throughHoles = info != nullptr && info->type == ObjectType::ForestTree;
		std::optional<float> distance;
		for (size_t s = 0; s < mesh->GetSubMeshes().size() && !distance.has_value(); ++s)
		{
			const auto& subMesh = *mesh->GetSubMeshes()[s];
			if (!ecs::posed_model::IsDrawn(subMesh))
			{
				continue;
			}
			ecs::posed_model::Place(registry, entity, *mesh, s, model, _placed);
			const auto& geometry = subMesh.GetBodyGeometry();
			_corners.clear();
			for (size_t c = 0; c < _placed.size(); ++c)
			{
				_corners.push_back(
				    screen_pick::ToClip(view, _placed[c], c < geometry.uvs.size() ? geometry.uvs[c] : glm::vec2(0.0f)));
			}
			const auto& indices = geometry.indices;
			for (const auto& primitive : subMesh.GetPrimitives())
			{
				if (primitive.indicesOffset + primitive.indicesCount > indices.size())
				{
					continue;
				}
				distance = screen_pick::FirstHit(
				    view,
				    {.corners = _corners,
				     .indices = std::span(indices).subspan(primitive.indicesOffset, primitive.indicesCount),
				     .twoSided = primitive.twoSided,
				     .mask = throughHoles ? mesh->GetSkinMask(primitive.skinID) : nullptr},
				    _scratch);
				if (distance.has_value())
				{
					break;
				}
			}
		}
		if (!distance.has_value())
		{
			return std::nullopt;
		}
		// A building following the land that is only partly built is hit only below what of it stands
		if (const auto* progress = registry.TryGet<const BuildProgress>(entity);
		    progress != nullptr && registry.AllOf<MorphWithTerrain>(entity) && progress->built < 1.0f)
		{
			const auto* transform = registry.TryGet<const Transform>(entity);
			const float scale = transform != nullptr ? transform->scale.x : 1.0f;
			const auto hit = view.camera + (frame.nearPoint - view.camera) * (*distance / view.near);
			const float halfHeight = mesh->GetBoundingBox().Size().y * 0.5f;
			if (hit.y > _candidates[i].origin.y + halfHeight * 2.0f * scale * progress->built)
			{
				return std::nullopt;
			}
		}
		return distance;
	};
	auto picked = screen_pick::PickAmong(view, _candidates, distanceOf);

	// The land wins over the object when it is as near or nearer and its point is off the object's footprint
	if (picked.has_value() && _pick.land.has_value())
	{
		const auto landPoint = map_coords::ToMetres(map_coords::FromMetres({_pick.land->x, _pick.land->z}));
		const auto& candidate = _candidates[picked->index];
		if (screen_pick::LandHidesObject(landDistance, picked->distance, landPoint, {candidate.origin.x, candidate.origin.z},
		                                 candidate.halfExtents))
		{
			picked.reset();
		}
	}
	if (picked.has_value())
	{
		auto entity = _candidateEntities[picked->index];
		// A gate stone on a plinth is the plinth to the cursor
		if (const auto* stone = registry.TryGet<const PlinthStone>(entity); stone != nullptr && registry.Valid(stone->plinth))
		{
			entity = stone->plinth;
		}
		_pick.object = entity;
		const auto* transform = registry.TryGet<const Transform>(entity);
		_pick.point = transform != nullptr ? transform->position : _candidates[picked->index].origin;
		_pick.distance = picked->distance;
	}
	else
	{
		_pick.point = _pick.land;
		_pick.distance = landDistance;
	}
	picking::CarryHover(previous, _pick, frame.seconds);
}

void PickingSystem::ResolveScreenShapes(const ScreenShapes& shapes)
{
	if (_locked)
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const bool creature = _pick.object.has_value() && registry.Valid(*_pick.object) && registry.AllOf<Creature>(*_pick.object);
	const auto object = _pick.object;
	picking::WeighScreenShapes(_pick, shapes, creature);
	if (_pick.object != object)
	{
		picking::CarryHover(_previous, _pick, _seconds);
	}
}

void PickingSystem::FindBehindHand(glm::vec3 camera, std::optional<glm::vec3> handMiddle)
{
	_behindHand = handMiddle.has_value() ? LandOrSeaAlong(camera, *handMiddle, camera) : std::nullopt;
}

void PickingSystem::AddBall(entt::entity entity, const glm::mat4& model, entt::id_type mesh, glm::vec3 halfExtents,
                            const ecs::pick_shapes::Ball& ball)
{
	// Weighed against the land as the thing's own model is: by the model's footprint about its place
	_candidates.push_back({
	    .centre = ball.centre,
	    .radius = ball.radius,
	    .origin = glm::vec3(model[3]),
	    .halfExtents = glm::vec2(halfExtents.x, halfExtents.z),
	    .ball = true,
	});
	_candidateEntities.push_back(entity);
	_candidateModels.push_back(model);
	_candidateMeshes.push_back(mesh);
}

void PickingSystem::AddBubbles(glm::vec3 camera)
{
	// One-shot bubbles are drawn by the miracles' own pass, so they aren't among the drawn models: each is picked by its
	// model moved towards the camera by its width, and by a ball about its middle
	const auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto bubbleMesh = OneOffSpellSeed::k_MeshId.value();
	if (!meshes.Contains(bubbleMesh))
	{
		return;
	}
	const auto box = meshes.Handle(bubbleMesh)->GetBoundingBox();
	const auto halfExtents = box.Size() * 0.5f;
	registry.Each<const OneOffSpellSeed, const Transform>(
	    [&](entt::entity entity, const OneOffSpellSeed& globe, const Transform& transform) {
		    if (registry.AnyOf<InHand, SeeThrough>(entity))
		    {
			    return;
		    }
		    const float scale = transform.scale.x;
		    auto model = glm::mat4(transform.rotation * glm::mat3(glm::scale(glm::mat4(1.0f), transform.scale)));
		    model[3] = glm::vec4(transform.position, 1.0f);
		    const auto middle = glm::vec3(model * glm::vec4(box.Center(), 1.0f));
		    auto pushed = model;
		    pushed[3] += glm::vec4(ecs::pick_shapes::BubblePickOffset(middle, camera, halfExtents, scale), 0.0f);
		    _candidates.push_back({
		        .centre = glm::vec3(pushed * glm::vec4(box.Center(), 1.0f)),
		        .radius = glm::length(halfExtents) * scale,
		        .origin = transform.position,
		        .halfExtents = glm::vec2(halfExtents.x, halfExtents.z),
		    });
		    _candidateEntities.push_back(entity);
		    _candidateModels.push_back(pushed);
		    _candidateMeshes.push_back(bubbleMesh);
		    // Nobody owns a globe: no alignment chooses its flock's picture
		    AddBall(entity, model, bubbleMesh, halfExtents,
		            ecs::pick_shapes::BubbleBall(middle, scale, SeedModelMiddle(globe.seedType, 0.0f)));
	    });
}

void PickingSystem::AddFireBalls()
{
	// A fireball is picked by a ball of its size where its particle was last placed, while it shows bright enough to take
	// hold of. It has no model, so the land as near as it wins.
	const auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const MagicFireBall>([this](entt::entity entity, const MagicFireBall& fireBall) {
		if (!fireBall.handTarget.has_value())
		{
			return;
		}
		_candidates.push_back({
		    .centre = *fireBall.handTarget,
		    .radius = fireBall.radius,
		    .origin = *fireBall.handTarget,
		    .ball = true,
		});
		_candidateEntities.push_back(entity);
		_candidateModels.push_back(glm::mat4(1.0f));
		_candidateMeshes.push_back(0);
	});
}

void PickingSystem::AddVortices()
{
	// A vortex between the lands is picked by a ball at its middle, most of its cylinder's width; it is weighed against
	// the land by the cylinder's footprint
	const auto& registry = Locator::entitiesRegistry::value();
	const auto cylinder = resources::HashIdentifier(MeshId::SpellVortexCylinder);
	const auto halfExtents = HalfExtentsOf(cylinder);
	registry.Each<const Vortex>([&](entt::entity entity, const Vortex& vortex) {
		AddBall(entity, glm::translate(glm::mat4(1.0f), vortex.centre), cylinder, halfExtents,
		        ecs::pick_shapes::VortexBall(vortex.centre, halfExtents));
	});
}

void PickingSystem::AddTeleportStones()
{
	// A teleport stone is picked by a wide ball about it while its miracle is still held in the hand; it is weighed
	// against the land by the footprint of the model its kind is made with
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto model = resources::HashIdentifier(
	    Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Teleport)).meshId);
	const auto halfExtents = HalfExtentsOf(model);
	registry.Each<const TeleportStone, const Transform>(
	    [&](entt::entity entity, const TeleportStone& stone, const Transform& transform) {
		    const auto* spell = registry.Valid(stone.spell) ? registry.TryGet<const Spell>(stone.spell) : nullptr;
		    const auto* seed =
		        spell != nullptr && registry.Valid(spell->seed) ? registry.TryGet<const SpellSeed>(spell->seed) : nullptr;
		    if (seed == nullptr || seed->spell != stone.spell)
		    {
			    return;
		    }
		    AddBall(entity, glm::translate(glm::mat4(1.0f), transform.position), model, halfExtents,
		            ecs::pick_shapes::TeleportBall(transform.position));
	    });
}

void PickingSystem::AddLeashPosts()
{
	// A leash hanging at a temple is picked by a ball where it hangs, while it is shown, even when the picked one is
	// carried in the hand; it is weighed against the land by its collar's footprint where it is drawn
	const auto& registry = Locator::entitiesRegistry::value();
	const auto collar = LeashPost::k_MeshId;
	const auto halfExtents = HalfExtentsOf(collar);
	registry.Each<const LeashPost, const Transform>(
	    [&](entt::entity entity, const LeashPost& post, const Transform& transform) {
		    if (!post.hung || transform.scale.x == 0.0f)
		    {
			    return;
		    }
		    AddBall(entity, glm::translate(glm::mat4(1.0f), transform.position), collar, halfExtents,
		            ecs::pick_shapes::LeashPostBall(post.point));
	    });
}

void PickingSystem::RetargetObject(std::optional<entt::entity> object, float distance)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = object.has_value() && registry.Valid(*object) ? registry.TryGet<const Transform>(*object) : nullptr;
	if (transform == nullptr)
	{
		_pick.object.reset();
		_pick.point = _pick.land;
		_pick.distance = std::numeric_limits<float>::max();
		return;
	}
	_pick.object = object;
	_pick.point = transform->position;
	_pick.distance = distance;
}

std::optional<screen_pick::MeshHit> PickingSystem::FeelModel(entt::entity object, glm::vec3 origin, glm::vec3 direction) const
{
	if (!Locator::rendereringSystem::has_value() || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* meshComponent = registry.Valid(object) ? registry.TryGet<const Mesh>(object) : nullptr;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshComponent == nullptr || !meshes.Contains(meshComponent->id))
	{
		return std::nullopt;
	}
	const auto& drawn = Locator::rendereringSystem::value().GetContext().drawnObjects;
	const auto found = std::ranges::find(drawn, object, &RenderContext::DrawnObject::entity);
	if (found == drawn.end())
	{
		return std::nullopt;
	}
	const auto mesh = meshes.Handle(meshComponent->id);
	std::optional<screen_pick::MeshHit> nearest;
	for (size_t s = 0; s < mesh->GetSubMeshes().size(); ++s)
	{
		// The submeshes of the nearest level of detail
		if ((mesh->GetSubMeshes()[s]->GetFlags().lodMask & 1) == 0)
		{
			continue;
		}
		std::vector<glm::vec3> placed;
		ecs::posed_model::Place(registry, object, *mesh, s, found->model, placed);
		const auto hit = screen_pick::NearestIntersection(placed, mesh->GetSubMeshes()[s]->GetBodyGeometry().indices, origin,
		                                                  direction, false);
		if (hit.has_value() && (!nearest.has_value() || hit->distance < nearest->distance))
		{
			nearest = hit;
		}
	}
	return nearest;
}
