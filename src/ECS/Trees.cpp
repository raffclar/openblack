/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The trees' own turn: growth and the forest ids the hand plants into.

#include "Trees.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Audio/Services/AnimationSounds.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Debug/DebugEnv.h"
#include "ECS/Archetypes/DeadTreeArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/Utils.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/ForestTreeBuckets.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/HandSystemDetail.h"
#include "ECS/Systems/TreeSystemInterface.h"
#include "ECS/Weather/Weather.h"
#include "Game.h"
#include "GameClock.h"
#include "Graphics/ModelLight.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The game's forests; stops with a message when there are none (before the game or after it has gone)
openblack::ecs::systems::ForestSystemInterface& ForestList()
{
	if (!Locator::forestSystem::has_value())
	{
		std::fputs("ecs forests: no forests in the locator (Locator::forestSystem)\n", stderr);
		std::abort();
	}
	return Locator::forestSystem::value();
}

/// What the game's trees share (the brightness, the deletion listeners); stops with a message when it is missing
openblack::ecs::systems::TreeSystemInterface& TreeState()
{
	if (!Locator::treeSystem::has_value())
	{
		std::fputs("ecs trees: no tree state in the locator (Locator::treeSystem)\n", stderr);
		std::abort();
	}
	return Locator::treeSystem::value();
}

/// What the tree trace keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct TreesDebugHooksState
{
	float traceSince {0.0f}; // OPENBLACK_TREE_TRACE: seconds since the last brightness line
};

TreesDebugHooksState& TreesDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs trees: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<TreesDebugHooksState>();
}

/// from + an offset of `metres` along `angle`, added as map coordinates on the (x, z) in metres (each held as map
/// coordinates, as the original holds them)
glm::vec2 PosFromAngle(const glm::vec3& from, float angle, float metres)
{
	namespace map_coords = openblack::map_coords;
	const auto at = map_coords::FromMetres(glm::vec2(from.x, from.z)) + gutils::GetPosFromAngle(angle, metres);
	return map_coords::ToMetres(at);
}

/// Free when a 0.5 m circle at the point touches no collide data of the cell's fixed list, or else when the cell is
/// water. Read literally: a water cell is always free; neither this nor PlantTreeNear tests land before the tree is
/// made. Off the map CollideWithFixed has every bit set and IsWater is true (no cell)
bool IsFreeForTree(const openblack::map_coords::MapCoords& coords)
{
	if ((openblack::ecs::map_cells::CollideWithFixed(coords) & openblack::ecs::sea_cells::k_CollideFixed) == 0)
	{
		return true;
	}
	return openblack::ecs::sea_cells::IsWater(Locator::terrainSystem::value(), openblack::map_coords::Cell(coords));
}
} // namespace

uint32_t openblack::ecs::CreateForest(uint32_t id, glm::vec3 centre)
{
	return ForestList().Create(id, centre);
}

std::vector<uint32_t> openblack::ecs::ForestsNewestFirst()
{
	std::vector<std::pair<uint32_t, uint32_t>> order;
	for (const auto& [id, forest] : ForestList().All())
	{
		order.emplace_back(forest.created, id);
	}
	std::ranges::sort(order, [](const auto& lhs, const auto& rhs) { return lhs.first > rhs.first; });
	std::vector<uint32_t> ids;
	for (const auto& [created, id] : order)
	{
		ids.push_back(id);
	}
	return ids;
}

glm::vec3 openblack::ecs::ForestCentre(uint32_t forestId)
{
	const auto& forests = ForestList().All();
	const auto it = forests.find(forestId);
	return it != forests.end() ? it->second.centre : glm::vec3(0.0f);
}

size_t openblack::ecs::ForestTreeCount(uint32_t forestId)
{
	size_t count = 0;
	Locator::entitiesRegistry::value().Each<const Tree>(
	    [&](const Tree& tree) { count += forestId != 0 && tree.forestId == forestId ? 1 : 0; });
	return count;
}

std::vector<entt::entity> openblack::ecs::GrownTreesByDistance(uint32_t forestId)
{
	std::vector<std::pair<float, entt::entity>> grown;
	const auto centre = ForestCentre(forestId);
	Locator::entitiesRegistry::value().Each<const Tree, const Transform>(
	    [&](entt::entity entity, const Tree& tree, const Transform& transform) {
		    if (forestId != 0 && tree.forestId == forestId && (!tree.growing || transform.scale.x >= tree.maxSize))
		    {
			    // Distance to the forest centre in metres, x and z only
			    grown.emplace_back(gutils::GetDistanceInMetres(glm::vec2(transform.position.x, transform.position.z),
			                                                   glm::vec2(centre.x, centre.z)),
			                       entity);
		    }
	    });
	std::ranges::stable_sort(grown, [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
	std::vector<entt::entity> trees;
	for (const auto& [distance, entity] : grown)
	{
		trees.push_back(entity);
	}
	return trees;
}

bool openblack::ecs::IsInForest(uint32_t forestId)
{
	return forestId != 0 && ForestList().All().contains(forestId);
}

uint32_t openblack::ecs::ResolveForestId(int32_t scriptForestId)
{
	const auto id = static_cast<uint32_t>(scriptForestId);
	return IsInForest(id) ? id : 0u;
}

void openblack::ecs::ClearForests()
{
	ForestList().Clear();
}

uint32_t openblack::ecs::TownForestId(uint32_t townId, glm::vec3 /*at*/)
{
	// Over the town's list, every scenic forest becomes the best at distance 0, so the last of them wins; with none the
	// tree stays without a forest
	uint32_t best = 0;
	const auto& lists = ForestList().TownLists();
	if (const auto it = lists.find(townId); it != lists.end())
	{
		for (const auto id : it->second)
		{
			if (IsScenicForest(id))
			{
				best = id;
			}
		}
	}
	return best;
}

std::optional<uint32_t> openblack::ecs::NearestForest(glm::vec3 at, float radius)
{
	std::optional<uint32_t> best;
	float nearest = radius;
	for (const auto& [id, forest] : ForestList().All())
	{
		const float d = glm::distance(glm::vec2(at.x, at.z), glm::vec2(forest.centre.x, forest.centre.z));
		if (d <= nearest)
		{
			nearest = d;
			best = id;
		}
	}
	return best;
}

void openblack::ecs::SetTreeForest(entt::entity entity, uint32_t forestId)
{
	if (auto* tree = Locator::entitiesRegistry::value().TryGet<Tree>(entity); tree != nullptr)
	{
		tree->forestId = IsInForest(forestId) ? forestId : 0u;
	}
}

entt::entity openblack::ecs::PlantTreeNear(uint32_t forestId, entt::entity parent)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* parentTree = registry.TryGet<const Tree>(parent);
	const auto* parentTransform = registry.TryGet<const Transform>(parent);
	if (parentTree == nullptr || parentTransform == nullptr || !Locator::terrainSystem::has_value())
	{
		return entt::null;
	}
	const auto type = parentTree->type;
	const auto origin = parentTransform->position;
	// A random angle once; a random whole radius of 5-9 at the start of each ring
	float angle = game_random::GameFloatRand(glm::two_pi<float>());
	for (int ring = 0; ring < 32; ++ring)
	{
		auto radius = game_random::GameRand(5) + 5u;
		for (int attempt = 0; attempt < 5; ++attempt)
		{
			// origin + an offset along the angle at that radius, as map coordinates; tested for a free spot
			const auto coords = map_coords::FromMetres(glm::vec2(origin.x, origin.z)) +
			                    gutils::GetPosFromAngle(angle, static_cast<float>(radius));
			const glm::vec2 at = map_coords::ToMetres(coords);
			const glm::vec3 point(at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y);
			if (IsFreeForTree(coords))
			{
				ForestList().SetLastTreeCreatedTurn(game_clock::Turn());
				// The tree's arguments right to left: size 0.1, then a random yAngle, then maxSize 0.8 + rand(0.4)
				const float yAngle = game_random::GameFloatRand(glm::two_pi<float>());
				const float maxSize = game_random::GameFloatRand(0.4f) + 0.8f;
				const auto tree = archetypes::TreeArchetype::Create(forestId, point, type, true, yAngle, maxSize, 0.1f);
				registry.SetDirty();
				static const debug_env::Variable k_TreeTrace("OPENBLACK_TREE_TRACE");
				if (k_TreeTrace.Get() != nullptr)
				{
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree trace: forest {} planted {} at ({:.1f}, {:.1f})", forestId,
					                   static_cast<uint32_t>(tree), at.x, at.y);
				}
				return tree;
			}
			radius = (radius + 2) % 10;
		}
		angle += 0.19635f;
	}
	return entt::null;
}

namespace
{
/// Plays a sound effect at a position, filled as the water miracle on a tree does: the bank's sample
/// ("<bank>.sad/<n>"), 3D, track 0, at the tree's point; one of the 16 channels, not started farther from the camera
/// than the sample's max distance. (approximate) The original sets the tree as the owner; this signature has no tree,
/// so the channel has none: only the sample matching by owner differs (two trees growing with the same sample share a
/// channel).
void PlayAt(const std::string& name, glm::vec3 position)
{
	audio::PlayOptions options;
	options.sound = entt::hashed_string(name.c_str()).value();
	options.is3D = true;
	options.track = false;
	options.position = position;
	audio::PlaySoundEffect(options);
}

/// The trees of a forest (both of the original's lists)
std::vector<entt::entity> ForestTrees(uint32_t forestId)
{
	std::vector<entt::entity> trees;
	Locator::entitiesRegistry::value().Each<const Tree>([&](entt::entity entity, const Tree& tree) {
		if (forestId != 0 && tree.forestId == forestId)
		{
			trees.push_back(entity);
		}
	});
	return trees;
}
} // namespace

float openblack::ecs::GrowAllTrees(uint32_t forestId, float amount)
{
	float total = 0.0f;
	for (const auto tree : ForestTrees(forestId))
	{
		total += std::max(0.0f, GrowTree(tree, amount, false));
	}
	return total;
}

float openblack::ecs::ShrinkAllTrees(uint32_t forestId, float amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	float total = 0.0f;
	for (const auto tree : ForestTrees(forestId))
	{
		auto& transform = registry.Get<Transform>(tree);
		const float size = transform.scale.x - amount;
		if (size <= 0.0f)
		{
			// A tree that would reach 0 is deleted and adds nothing
			DeleteTree(tree);
			continue;
		}
		transform.scale = glm::vec3(size);
		if (auto* fixed = registry.TryGet<Fixed>(tree); fixed != nullptr)
		{
			const auto& info =
			    Locator::infoConstants::value().tree.at(static_cast<size_t>(registry.Get<const Tree>(tree).type));
			const auto [point, radius] = archetypes::GetFixedObstacleBoundingCircle(info.normal, transform);
			fixed->boundingCenter = point;
			fixed->boundingRadius = radius;
		}
		total += amount;
	}
	registry.SetDirty();
	return total;
}

float openblack::ecs::TallestTreeHeight(uint32_t forestId)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	float tallest = 0.0f;
	for (const auto tree : ForestTrees(forestId))
	{
		const auto* mesh = registry.TryGet<const Mesh>(tree);
		if (mesh == nullptr || !meshes.Contains(mesh->id))
		{
			continue;
		}
		tallest = std::max(tallest, ecs::object::GetHeight(tree));
	}
	return tallest;
}

void openblack::ecs::AddTreeDeletedListener(TreeDeletedListener listener)
{
	TreeState().AddDeletedListener(std::move(listener));
}

void openblack::ecs::NotifyTreeDeleted(entt::entity tree, TreeDeletion how)
{
	for (const auto& listener : TreeState().DeletedListeners())
	{
		listener(tree, how);
	}
}

void openblack::ecs::DeleteTree(entt::entity tree)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(tree))
	{
		return;
	}
	NotifyTreeDeleted(tree, TreeDeletion::Removed);
	if (ecs::physics::PhysicsObjects::Find(tree) != nullptr)
	{
		ecs::physics::PhysicsObjects::RemoveObject(tree);
	}
	// Out of its cell's fixed list
	map_cells::RemoveMapObject(tree);
	registry.Destroy(tree);
	registry.SetDirty();
}

void openblack::ecs::DeleteForest(uint32_t forestId)
{
	for (const auto tree : ForestTrees(forestId))
	{
		DeleteTree(tree);
	}
	ForestList().All().erase(forestId);
	// Every town drops the forest from its list
	for (auto& [town, list] : ForestList().TownLists())
	{
		std::erase(list, forestId);
	}
}

float openblack::ecs::TreeWoodValue(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return 0.0f;
	}
	// openblack gives trees no Life component yet; without one the life is taken as 1 (inferred: a tree starts at
	// startLife 1 and only fire lowers it)
	const auto* life = registry.TryGet<const Life>(entity);
	const float lifeValue = life != nullptr ? life->value : 1.0f;
	const float scale = transform->scale.x;
	if (const auto* tree = registry.TryGet<const Tree>(entity); tree != nullptr)
	{
		const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(tree->type));
		return lifeValue * tree->woodValueMultiplier * static_cast<float>(info.woodValue) * scale *
		       openblack::land_balance::Get(5);
	}
	if (const auto* dead = registry.TryGet<const DeadTree>(entity); dead != nullptr)
	{
		const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(dead->type));
		return lifeValue * static_cast<float>(info.woodValue) * scale * scale * scale;
	}
	return 0.0f;
}

uint32_t openblack::ecs::TreeWood(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Tree>(entity))
	{
		return static_cast<uint32_t>(TreeWoodValue(entity));
	}
	const auto* dead = registry.TryGet<const DeadTree>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (dead == nullptr || transform == nullptr)
	{
		return 0;
	}
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(dead->type));
	// The wood value multiplier of the tree it was
	return static_cast<uint32_t>(static_cast<float>(info.woodValue) * dead->woodValueMultiplier * transform->scale.x);
}

uint32_t openblack::ecs::RemoveWood(entt::entity entity, uint32_t amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* dead = registry.TryGet<const DeadTree>(entity);
	if (dead == nullptr)
	{
		return 0;
	}
	// The object's resource: its default resource when the type is its resource type (WOOD for a dead tree), 0
	// otherwise
	const uint32_t have = TreeWood(entity);
	if (have <= amount)
	{
		DeleteTree(entity);
		return have;
	}
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(dead->type));
	auto& transform = registry.Get<Transform>(entity);
	transform.scale =
	    glm::vec3(static_cast<float>(have - amount) / (static_cast<float>(info.woodValue) * dead->woodValueMultiplier));
	registry.SetDirty();
	return amount;
}

CarriedTreeType openblack::ecs::TreeCarriedType(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* tree = registry.TryGet<const Tree>(entity); tree != nullptr)
	{
		return Locator::infoConstants::value().tree.at(static_cast<size_t>(tree->type)).carriedType;
	}
	if (const auto* big = registry.TryGet<const BigForest>(entity); big != nullptr)
	{
		// (inferred) BigForestArchetype keeps no info index (BigForest::type stays 0); the four big forest rows of
		// info.dat all have carriedType 3, so the row is exact whatever it is
		const auto& rows = Locator::infoConstants::value().bigForest;
		return rows.at(std::min<size_t>(static_cast<size_t>(std::max(big->type, 0)), rows.size() - 1)).carriedType;
	}
	const auto* dead = registry.TryGet<const DeadTree>(entity);
	if (dead == nullptr)
	{
		return CarriedTreeType::None;
	}
	if (const auto* mesh = registry.TryGet<const Mesh>(entity); mesh != nullptr)
	{
		// The four carried logs, MeshPack 0x196 / 0x15B / 0x15C / 0x15D
		constexpr std::array<uint32_t, 4> k_Logs = {0x196, 0x15B, 0x15C, 0x15D};
		for (size_t i = 0; i < k_Logs.size(); ++i)
		{
			if (mesh->id == resources::HashIdentifier(static_cast<MeshId>(k_Logs[i])))
			{
				return static_cast<CarriedTreeType>(i);
			}
		}
	}
	return Locator::infoConstants::value().tree.at(static_cast<size_t>(dead->type)).carriedType;
}

uint32_t openblack::ecs::CreateDeadTreeReaction(entt::entity deadTree, PlayerNames player)
{
	// Reaction 12 with the dead tree's player
	return ecs::effects::reactions::CreateReaction(deadTree, Reaction::ReactToWood, player, false);
}

uint32_t openblack::ecs::DeadTreeEndPhysicsReaction(entt::entity deadTree, bool byPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	// A felled tree's landing only fixes it in place: no reaction
	if (!registry.Valid(deadTree) || !registry.AllOf<DeadTree>(deadTree) || registry.AllOf<FelledTree>(deadTree))
	{
		return 0;
	}
	// The player is the thrower's (the hand's) player, else none. (inferred) openblack's PhysicsObject::byPlayer is the
	// local hand's throw (HandPhysics: PLAYER_ONE)
	const auto player = byPlayer ? PlayerNames::PLAYER_ONE : PlayerNames::NEUTRAL;
	return ecs::effects::reactions::CreateReaction(deadTree, Reaction::ReactToWood, player, false);
}

entt::entity openblack::ecs::CreateDroppedLog(glm::vec3 position, uint32_t mesh, float woodMultiplier)
{
	// A dead Pine of no player, life 1.0, tilted pi/2 about x; the angles are DeadTreeArchetype::Create's
	const auto log = archetypes::DeadTreeArchetype::Create(position, TreeInfo::Pine, 1.0f, glm::half_pi<float>(), 0.0f, 0.0f);
	// No reaction here: the dropped log's constructor makes none; the log tells the villagers only when it lands
	// (DeadTreeEndPhysicsReaction)
	auto& registry = Locator::entitiesRegistry::value();
	// The mesh override is used instead of the tree type's mesh while the log is made
	if (mesh != 0)
	{
		registry.Get<Mesh>(log).id = resources::HashIdentifier(static_cast<MeshId>(mesh));
	}
	// The wood value multiplier
	registry.Get<DeadTree>(log).woodValueMultiplier = woodMultiplier;
	// The original also clears a flag bit that is not identified: (not ported), as in DeadTreeArchetype::Create
	return log;
}

entt::entity openblack::ecs::FellTree(entt::entity tree, entt::entity chopper)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* treeComponent = registry.TryGet<const Tree>(tree);
	const auto* mesh = registry.TryGet<const Mesh>(tree);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (treeComponent == nullptr || mesh == nullptr || !meshes.Contains(mesh->id) || !registry.Valid(chopper) ||
	    !registry.AllOf<Transform>(chopper))
	{
		return entt::null;
	}
	const auto& transform = registry.Get<const Transform>(tree);
	const auto from = registry.Get<const Transform>(chopper).position;
	// k = (height x 0.5) x 0.4, each product a float
	const float height = ecs::object::GetHeight(tree);
	const float halfHeight = height * 0.5f;
	const float k = halfHeight * 0.4f;
	// The direction from the forester to the tree (y 0) and its angle a = atan2(x, -z) (0 when shorter than
	// sqrt(0.001)): velocity (sin a, 0, -cos a) k = k along that direction; spin (cos a k, 0, sin a k) / h2 (about 0.4)
	const glm::vec3 d(transform.position.x - from.x, 0.0f, transform.position.z - from.z);
	// |d|^2 <= 0.001 gives a = 0; else the angle is kept as a double for the sin / cos, not rounded to a float
	const double a = glm::dot(d, d) <= 0.001f ? 0.0 : affine::GetYAngle(d);
	const auto cosK = static_cast<float>(std::cos(a) * k); // negated twice in the original: the same float
	const glm::vec3 velocity(static_cast<float>(std::sin(a) * k), 0.0f, -cosK);
	// The spin is a BODY-space angular velocity in the original's sign: the physics builds the angular momentum from it
	// summed over the rows of the new dead tree's matrix (the tree's yaw at scale 1.0, else the identity, below):
	// PhysicsObjects::BodySpin. w = (cos a k, 0, sin a k) x (1 / h2), each product a float (the stored velocity
	// components times the stored 1 / h2; about 0.4 (cos a, 0, sin a))
	const float invHalfHeight = 1.0f / halfHeight;
	const ecs::physics::PhysicsObjects::BodySpin spin {
	    glm::vec3(cosK * invHalfHeight, 0.0f * invHalfHeight, velocity.x * invHalfHeight)};

	// The dead tree takes over the tree's 3D object (mesh, matrix, scale) and info; setting its scale may rebuild the
	// matrix (below). The roots do not break off (only a tree that falls sets that flag)
	const auto type = treeComponent->type;
	// The forester then deletes the tree: it leaves its forest and whoever listens (hand, reactions) lets go of it. Its
	// fire moves to the dead tree: kept here because it is the same entity.
	NotifyTreeDeleted(tree, TreeDeletion::BecameDeadTree);
	const float multiplier = treeComponent->woodValueMultiplier;
	// Deleting the tree takes it out of the map. The dead tree enters the map when the physics puts it to rest
	map_cells::RemoveMapObject(tree);
	registry.Remove<Tree>(tree);
	registry.Assign<DeadTree>(tree, type, multiplier);
	registry.AssignOrReplace<FelledTree>(tree, chopper);
	// The dead tree's creation (its player the LOCAL one) ends in reaction 12 with that player: spread at once
	CreateDeadTreeReaction(tree, PlayerNames::PLAYER_ONE);
	// (inferred) the dead tree is a new object in the original, so it does not carry the tree's IMMOVABLE flag
	// (SET_ID_MOVEABLE) that would keep it out of the physics
	ecs::object_flags::SetMoveable(tree, true);
	// Setting the tree's scale: only when it differs from the dead tree's own 1.0 is the matrix rebuilt from the dead
	// tree's angles, all 0: axis-aligned. At scale 1.0 the tree's matrix, with its yaw, stays
	if (auto& transform = registry.Get<Transform>(tree); transform.scale.x != 1.0f)
	{
		transform.rotation = glm::mat3(1.0f);
	}
	registry.SetDirty();
	auto* po = ecs::physics::PhysicsObjects::AddObject(tree, velocity, spin, chopper, false);
	if (po != nullptr)
	{
		// After the physics starts: adjusted to the ground, flag 2 (what a Living sets moving; a villager's body does not
		// hit it), raised until not intersecting and kind 2 (a felled tree, its fall sounds)
		po->body.AdjustToGroundLevel(false, true);
		ecs::physics::PhysicsObjects::SyncTurnStart(*po); // AdjustToGroundLevel's copy into the turn-start matrix
		po->flags |= ecs::physics::PhysicsObject::k_PushedByLiving;
		ecs::physics::PhysicsObjects::RaiseUntilNotIntersecting(*po);
		po->kind = 2;
		// Reaction 12 with the local player, after RaiseUntilNotIntersecting
		ecs::effects::reactions::CreateReaction(tree, Reaction::ReactToWood, PlayerNames::PLAYER_ONE, false);
	}
	return tree;
}

namespace
{
/// map_coords::Spiral with the steps {(1,0), (0,1), (-1,0), (0,-1)}, started with dir 1 and steps 1 (as the villager's
/// tree search and MakeScenicForest use it): each call does `if (--steps == 0) { ++dir; steps = dir / 2; }` and moves
/// by table[dir & 3]. The offsets it visits, the centre first:
/// (0,0) (-1,0) (-1,-1) (0,-1) (1,-1) (1,0) (1,1) (0,1) (-1,1) ...
std::vector<glm::ivec2> SpiralOffsets(size_t count)
{
	std::vector<glm::ivec2> offsets {glm::ivec2(0)};
	glm::ivec2 at(0);
	map_coords::Spiral spiral;
	while (offsets.size() < count)
	{
		const auto& step = spiral.Next();
		at += glm::ivec2(step.x, step.z);
		offsets.push_back(at);
	}
	return offsets;
}

/// The map cell (map_coords::CellOf: x * 6553.6f truncated toward zero, the unsigned high words)
glm::ivec2 CellOf(glm::vec3 position)
{
	return map_coords::CellOf(position);
}
} // namespace

float openblack::ecs::Object2DRadius(entt::entity entity)
{
	return ecs::object::Get2DRadius(entity);
}

glm::vec3 openblack::ecs::TreeWorkingPos(entt::entity tree, entt::entity who)
{
	// The tree + an offset along the angle towards `who`, at who's 2D radius + 0.9, with the tree's altitude; the point is
	// the ground there plus that altitude
	return map_coords::ToWorld(object::TreeGetWorkingPos(tree, who));
}

entt::entity openblack::ecs::FindTreeNearVillager(entt::entity who)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(who) || !registry.AllOf<Transform>(who))
	{
		return entt::null;
	}
	const auto& from = registry.Get<const Transform>(who).position;
	const auto cell = CellOf(from);
	std::vector<glm::ivec2> cells;
	for (const auto& offset : SpiralOffsets(9))
	{
		cells.push_back(cell + offset);
	}
	entt::entity best = entt::null;
	float nearest = 99999.0f;
	const map_cells::ReadBatch batch; // one read filter for the nine cells
	for (const auto& c : cells)
	{
		// Only the first FOREST_TREE of the cell's fixed list; when it is not a Tree the cell is skipped. INDESTRUCTIBLE
		// is never set on a tree outside puzzles
		const auto tree = map_cells::FindType(c, ObjectType::ForestTree);
		if (tree == entt::null || !registry.AllOf<Tree>(tree))
		{
			continue;
		}
		const auto working = TreeWorkingPos(tree, who);
		const float d = glm::distance(glm::vec2(from.x, from.z), glm::vec2(working.x, working.z));
		if (d < nearest)
		{
			nearest = d;
			best = tree;
		}
	}
	return best;
}

entt::entity openblack::ecs::ForestBigForest(uint32_t forestId)
{
	const auto& forests = ForestList().All();
	const auto it = forests.find(forestId);
	return it != forests.end() ? it->second.bigForest : entt::entity {entt::null};
}

void openblack::ecs::SetForestBigForest(uint32_t forestId, entt::entity bigForest)
{
	auto& forests = ForestList().All();
	if (const auto it = forests.find(forestId); it != forests.end())
	{
		it->second.bigForest = bigForest;
	}
}

bool openblack::ecs::IsScenicForest(uint32_t forestId)
{
	const auto& forests = ForestList().All();
	const auto it = forests.find(forestId);
	return it != forests.end() && it->second.scenic;
}

float openblack::ecs::ForestWood(uint32_t forestId)
{
	auto& registry = Locator::entitiesRegistry::value();
	float wood = 0.0f;
	if (const auto bigForest = ForestBigForest(forestId); bigForest != entt::null && registry.Valid(bigForest))
	{
		// Its wood value = life x wood
		if (const auto* forest = registry.TryGet<const BigForest>(bigForest); forest != nullptr)
		{
			const auto* life = registry.TryGet<const Life>(bigForest);
			wood += (life != nullptr ? life->value : 1.0f) * forest->wood;
		}
	}
	for (const auto tree : ForestTrees(forestId))
	{
		wood += TreeWoodValue(tree);
	}
	return wood;
}

entt::entity openblack::ecs::ForestCentreTree(uint32_t forestId)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto centre = ForestCentre(forestId);
	std::optional<std::pair<float, entt::entity>> grown;
	std::optional<std::pair<float, entt::entity>> growing;
	for (const auto tree : ForestTrees(forestId))
	{
		const auto& t = registry.Get<const Tree>(tree);
		const auto& transform = registry.Get<const Transform>(tree);
		// Distance to the forest centre in metres, 2D
		const float d =
		    gutils::GetDistanceInMetres(glm::vec2(transform.position.x, transform.position.z), glm::vec2(centre.x, centre.z));
		auto& head = (t.growing && transform.scale.x < t.maxSize) ? growing : grown;
		if (!head || d < head->first)
		{
			head = std::make_pair(d, tree);
		}
	}
	if (grown && (!growing || grown->first <= growing->first))
	{
		return grown->second;
	}
	return growing ? growing->second : entt::entity {entt::null};
}

std::optional<uint32_t> openblack::ecs::FindForest(glm::vec3 at, float max, bool onlyEmpty)
{
	std::optional<uint32_t> best;
	for (const auto id : ForestsNewestFirst())
	{
		const auto& forest = ForestList().All().at(id);
		const float d = glm::distance(glm::vec2(at.x, at.z), glm::vec2(forest.centre.x, forest.centre.z));
		if (d >= max)
		{
			continue;
		}
		const size_t trees = ForestTreeCount(id);
		const bool fits = onlyEmpty ? (trees == 0 && forest.bigForest == entt::null) : trees > 0;
		if (fits)
		{
			max = d;
			best = id;
		}
	}
	return best;
}

namespace
{
/// The forest's point nearest `at`: a BigForest's nearest edge (or `at` itself when inside its 2D radius,
/// FindNearestForestToPos only), else its centre. The nearest edge is pos + an offset along the angle towards `at`, at
/// its 2D radius.
glm::vec2 ForestNearestPoint(uint32_t forestId, glm::vec3 at, bool insideIsZero)
{
	auto& registry = Locator::entitiesRegistry::value();
	const glm::vec2 target(at.x, at.z);
	const auto bigForest = openblack::ecs::ForestBigForest(forestId);
	if (bigForest != entt::null && registry.Valid(bigForest) && registry.AllOf<Transform>(bigForest))
	{
		const auto& p = registry.Get<const Transform>(bigForest).position;
		const glm::vec2 centre(p.x, p.z);
		const float radius = openblack::ecs::Object2DRadius(bigForest);
		const float d = glm::distance(centre, target);
		if (insideIsZero && d <= radius)
		{
			return target;
		}
		// Its nearest edge
		namespace map_coords = openblack::map_coords;
		return map_coords::ToMetres(openblack::ecs::object::GetNearestEdgeToPos(bigForest, map_coords::FromMetres(target)));
	}
	const auto c = openblack::ecs::ForestCentre(forestId);
	return {c.x, c.z};
}
} // namespace

void openblack::ecs::MakeScenicForest(uint32_t townId, glm::vec3 townCentre)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& list = ForestList().TownLists()[townId];
	std::optional<uint32_t> scenic;
	for (const auto id : list)
	{
		if (IsScenicForest(id))
		{
			scenic = id;
			break;
		}
	}
	// R = the town info's maxDistanceForTownForest (250) + 10. The spiral walks a copy of the town centre's map
	// coordinates, at most 99999 cells, and stops at the first one farther than R from the centre. In each cell the
	// fixed list, then the mobile one, each from its head; only trees. The trees join the scenic forest in that order
	const float radius = Locator::infoConstants::value().town.maxDistanceForTownForest + 10.0f;
	const auto centre = map_coords::FromMetres(glm::vec2(townCentre.x, townCentre.z));
	auto coords = centre;
	map_coords::Spiral spiral;
	const glm::vec2 centre2(townCentre.x, townCentre.z);
	std::vector<entt::entity> taken;
	for (int n = 99999; n != 0; --n)
	{
		if (gutils::GetDistanceInMetres(coords, centre) > radius)
		{
			break;
		}
		const auto cell = map_coords::Cell(coords);
		for (auto entity = map_cells::FindType(cell, ObjectType::Any); entity != entt::null;
		     entity = map_cells::FindType(cell, ObjectType::Any, entity))
		{
			const auto* tree = registry.TryGet<const Tree>(entity);
			if (tree == nullptr)
			{
				continue;
			}
			// Its forest: none, or a scenic one whose centre is farther from the tree than the town centre (then the tree
			// is taken out of it)
			if (!IsInForest(tree->forestId))
			{
				taken.push_back(entity);
				continue;
			}
			if (IsScenicForest(tree->forestId))
			{
				// Distances in metres, 2D
				const auto forestCentre = ForestCentre(tree->forestId);
				const auto& position = registry.Get<const Transform>(entity).position;
				const glm::vec2 at(position.x, position.z);
				if (gutils::GetDistanceInMetres(at, centre2) <
				    gutils::GetDistanceInMetres(at, glm::vec2(forestCentre.x, forestCentre.z)))
				{
					taken.push_back(entity);
				}
			}
		}
		map_coords::AddCells(coords, spiral.Next());
	}
	// The scenic forest is made only before its first tree: no trees, no forest
	if (taken.empty())
	{
		return;
	}
	if (!scenic)
	{
		scenic = CreateForest(0, townCentre);
		ForestList().All().at(*scenic).scenic = true;
	}
	for (const auto tree : taken)
	{
		SetTreeForest(tree, *scenic);
	}
}

void openblack::ecs::AssignForestsToTown(uint32_t townId, glm::vec3 reference)
{
	auto& list = ForestList().TownLists()[townId];
	list.clear();
	const float maxDistance = Locator::infoConstants::value().town.maxDistanceForTownForest;
	// The global list, newest first; each is inserted at the head (no duplicates)
	for (const auto id : ForestsNewestFirst())
	{
		const auto point = ForestNearestPoint(id, reference, false);
		if (glm::distance(point, glm::vec2(reference.x, reference.z)) < maxDistance && ForestWood(id) != 0.0f)
		{
			list.insert(list.begin(), id);
		}
	}
}

std::vector<uint32_t> openblack::ecs::TownForests(uint32_t townId)
{
	const auto& lists = ForestList().TownLists();
	const auto it = lists.find(townId);
	return it != lists.end() ? it->second : std::vector<uint32_t> {};
}

std::optional<uint32_t> openblack::ecs::FindNearestForestToPos(uint32_t townId, glm::vec3 at)
{
	const float maxDistance = Locator::infoConstants::value().town.maxDistanceForTownForest;
	float bestNormal = maxDistance;
	float bestScenic = maxDistance;
	std::optional<uint32_t> normal;
	std::optional<uint32_t> scenic;
	for (const auto id : TownForests(townId))
	{
		const float d = glm::distance(ForestNearestPoint(id, at, true), glm::vec2(at.x, at.z));
		if (IsScenicForest(id))
		{
			if (d < bestScenic)
			{
				bestScenic = d;
				scenic = id;
			}
		}
		else if (d < bestNormal)
		{
			bestNormal = d;
			normal = id;
		}
	}
	return normal ? normal : scenic;
}

glm::vec3 openblack::ecs::BigForestArrivePos(entt::entity bigForest, entt::entity who)
{
	// The forest + an offset along the angle towards `who`, at half its radius, with the forest's altitude; the point is
	// the ground there plus that altitude
	return map_coords::ToWorld(object::BigForestGetArrivePos(bigForest, who));
}

uint32_t openblack::ecs::BigForestRemoveWood(entt::entity bigForest, uint32_t amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* forest = registry.TryGet<BigForest>(bigForest);
	if (forest == nullptr)
	{
		return 0;
	}
	const auto* life = registry.TryGet<const Life>(bigForest);
	const float lifeValue = life != nullptr ? life->value : 1.0f;
	const float wanted = static_cast<float>(amount) / lifeValue;
	const float woodValue = lifeValue * forest->wood;
	if (woodValue <= wanted)
	{
		const auto had = static_cast<uint32_t>(woodValue);
		forest->wood = 0.0f;
		// The forest loses its BigForest but stays
		SetForestBigForest(forest->forestId, entt::null);
		// Out of all its map cells
		map_cells::RemoveMapObject(bigForest);
		registry.Destroy(bigForest);
		registry.SetDirty();
		return had;
	}
	forest->wood -= wanted;
	auto& transform = registry.Get<Transform>(bigForest);
	const float drawn = lifeValue * transform.scale.x * forest->woodValue;
	if (std::abs(lifeValue * forest->wood - drawn) > 250.0f)
	{
		transform.scale = glm::vec3(lifeValue * forest->wood / (lifeValue * forest->woodValue));
		// Plant a sapling around it
		const auto position = transform.position;
		const float radius = Object2DRadius(bigForest);
		for (int attempt = 0; attempt < 10 && Locator::terrainSystem::has_value(); ++attempt)
		{
			const float angle = game_random::GameFloatRand(glm::two_pi<float>());
			// position + an offset along a random angle, at its radius
			const glm::vec2 point = PosFromAngle(position, angle, radius);
			const float ground = Locator::terrainSystem::value().GetHeightAt(point);
			// Only on land
			if (!systems::hand_detail::IsLand(glm::vec3(point.x, ground, point.y)))
			{
				continue;
			}
			// Over the point's cell (the fixed list, then the mobile one; the BigForest itself counts too): none whose
			// distance plus its radius (not its 2D radius) is under 4
			const auto pointCell = map_coords::CellOf(glm::vec3(point.x, 0.0f, point.y));
			bool blocked = false;
			for (auto other = map_cells::FindType(pointCell, ObjectType::Any); other != entt::null;
			     other = map_cells::FindType(pointCell, ObjectType::Any, other))
			{
				const auto& at = registry.Get<const Transform>(other).position;
				if (gutils::GetDistanceInMetres(glm::vec2(at.x, at.z), point) + object::GetRadius(other) < 4.0f)
				{
					blocked = true;
					break;
				}
			}
			if (blocked)
			{
				continue;
			}
			// A Pine of the BigForest's forest, maxSize 0.75 + rand(0.5), a random yAngle, size 0.05: maxSize drawn
			// first, then yAngle
			const float maxSize = game_random::GameFloatRand(0.5f) + 0.75f;
			const float yAngle = game_random::GameFloatRand(glm::two_pi<float>());
			archetypes::TreeArchetype::Create(forest->forestId, glm::vec3(point.x, ground, point.y), TreeInfo::Pine, false,
			                                  yAngle, maxSize, 0.05f);
			break;
		}
	}
	registry.SetDirty();
	return amount;
}

entt::entity openblack::ecs::ApplyWaterSpell(entt::entity entity, bool raiseMaximum)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* tree = registry.TryGet<Tree>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (tree == nullptr || transform == nullptr)
	{
		return entt::null;
	}
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(tree->type));
	const bool growing = tree->growing && transform->scale.x < tree->maxSize;
	if (growing || raiseMaximum)
	{
		float amount = info.waterSpellAcceleratorMultiplier * info.growthAmount;
		if (!growing)
		{
			// GetDistanceModifier(size, 3) = SigmoidThreshold(0.5, 1 - min(size, 3) / 3)
			amount *= 0.5f * gutils::GetDistanceModifier(transform->scale.x, 3.0f);
		}
		if (GrowTree(entity, amount, raiseMaximum) != 0.0f)
		{
			tree->growing = true;
			// Sample 120 + a tick count % 9 (InGame.sad 120-128, G_TreeGrow) at the tree: the real clock, not the random
			// generator (audio::TickCount)
			PlayAt(fmt::format("InGame.sad/{}", 120 + audio::TickCount() % 9), transform->position);
		}
	}
	if (!growing && IsInForest(tree->forestId) && !raiseMaximum && game_clock::Turn() - ForestList().LastTreeCreatedTurn() > 40)
	{
		return PlantTreeNear(tree->forestId, entity);
	}
	return entt::null;
}

float openblack::ecs::GrowTree(entt::entity entity, float amount, bool raiseMax)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* tree = registry.TryGet<Tree>(entity);
	auto* transform = registry.TryGet<Transform>(entity);
	if (tree == nullptr || transform == nullptr)
	{
		return 0.0f;
	}
	const float size = transform->scale.x;
	if (raiseMax)
	{
		tree->maxSize = std::max(tree->maxSize, size + amount);
	}
	if (size >= tree->maxSize)
	{
		return 0.0f;
	}
	const float grown = std::min(size + amount, tree->maxSize);
	transform->scale = glm::vec3(grown);
	// SetScale is virtual in the original and rebuilds the collide data: the obstacle circle follows the new size.
	if (auto* fixed = registry.TryGet<Fixed>(entity); fixed != nullptr)
	{
		const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(tree->type));
		const auto [point, radius] = archetypes::GetFixedObstacleBoundingCircle(info.normal, *transform);
		fixed->boundingCenter = point;
		fixed->boundingRadius = radius;
	}
	registry.SetDirty();
	return grown - size;
}

namespace
{
/// An entry of the bend table: a position and a radius
struct BendSource
{
	glm::vec3 position;
	float radius;
};

/// Half the mesh height on each axis: the length of the half extents, times the scale
float MeshHalfDiagonal(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || transform == nullptr || !meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	return transform->scale.x * glm::length(0.5f * meshes.Handle(mesh->id)->GetBoundingBox().Size());
}

/// The bend table is fed each frame: the object in the hand (slot 1, every held object is "drawn in hand"), the physics
/// objects in flight (slots 3-13 in turn) and the player's creature (slot 2, no creature yet). Each marks the trees of
/// the 3 x 3 map cells (10 x 10) around it, the last one wins. A marked tree is then bent when the crown is not below
/// the source (base y + height >= source y) and the source is closer than its radius r horizontally: by
/// 0.471239 x (1 - ((r - 0.75) d / r + 0.75) / r) rad, d the horizontal distance, about the horizontal axis across the
/// source-to-tree direction, the crown leaning away from the source. The rubbing sound (editor.sad key {c, 0, 0, 10, 75}
/// with c = 3 below 0.3 of the bend, 2 below 0.67, else 1; 3 has no samples) plays when a bend starts.
void UpdateTreeBends()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<entt::entity, BendSource>> sources;
	if (Locator::handSystem::has_value())
	{
		if (const auto held = Locator::handSystem::value().GetHeldObject();
		    held && registry.Valid(*held) && registry.AllOf<Transform>(*held))
		{
			// Where it is drawn in the hand
			sources.emplace_back(*held, BendSource {DrawnPosition(registry, *held), MeshHalfDiagonal(*held)});
		}
	}
	ecs::physics::PhysicsObjects::ForEach([&](const ecs::physics::PhysicsObject& po) {
		if (!registry.Valid(po.entity) || !registry.AllOf<Transform>(po.entity) ||
		    (po.flags & ecs::physics::PhysicsObject::k_Awake) == 0)
		{
			return;
		}
		// The interpolated position (PhysicsDrawPose)
		const auto* drawn = registry.TryGet<const PhysicsDrawPose>(po.entity);
		const auto at = drawn != nullptr ? drawn->position : registry.Get<const Transform>(po.entity).position;
		sources.emplace_back(po.entity, BendSource {at, MeshHalfDiagonal(po.entity)});
	});

	// No source and nothing left bent: every tree is already straight and unbent, so the pass would change nothing
	auto& state = TreeState();
	if (sources.empty() && !state.AnyBent())
	{
		return;
	}
	// Which source marks a tree this frame: the last one whose 3 x 3 map cells hold it
	const auto markedBy = [&sources](entt::entity entity, const Transform& transform) -> std::optional<size_t> {
		std::optional<size_t> marked;
		const auto cell = glm::floor(glm::vec2(transform.position.x, transform.position.z) * 0.1f);
		for (size_t i = 0; i < sources.size(); ++i)
		{
			if (sources[i].first == entity)
			{
				continue;
			}
			const auto sourceCell = glm::floor(glm::vec2(sources[i].second.position.x, sources[i].second.position.z) * 0.1f);
			if (std::abs(cell.x - sourceCell.x) <= 1.0f && std::abs(cell.y - sourceCell.y) <= 1.0f)
			{
				marked = i;
			}
		}
		return marked;
	};

	bool anyBent = false;
	auto& meshes = Locator::resources::value().GetMeshes();
	static const debug_env::Variable k_TreeTrace("OPENBLACK_TREE_TRACE");
	const bool trace = k_TreeTrace.Get() != nullptr;
	registry.Each<Tree, const Transform, const Mesh>(
	    [&](entt::entity entity, Tree& tree, const Transform& transform, const Mesh& mesh) {
		    float angle = 0.0f;
		    const auto marked = sources.empty() ? std::nullopt : markedBy(entity, transform);
		    if (marked && meshes.Contains(mesh.id))
		    {
			    const auto& source = sources[*marked].second;
			    // The scale x the mesh's half height, doubled, + the object's y
			    const float crown = transform.position.y + ecs::object::MeshHeight(mesh.id, ecs::object::GetScale(entity));
			    const glm::vec2 away(transform.position.x - source.position.x, transform.position.z - source.position.z);
			    const float d = glm::length(away);
			    const float r = source.radius;
			    if (crown >= source.position.y && d < r && r > 0.0f)
			    {
				    const float bend = 1.0f - ((r - 0.75f) * d / r + 0.75f) / r;
				    angle = 0.471239f * bend;
				    tree.bendDirection = d > 1e-4f ? away / d : glm::vec2(0.0f, 1.0f);
				    if (!tree.wasBent)
				    {
					    const int strength = bend < 0.3f ? 3 : bend < 0.67f ? 2 : 1;
					    audio::AnimationSounds::PlayFromTable(entity, transform.position, {strength, 2, 0, 10, 75});
					    if (trace)
					    {
						    SPDLOG_LOGGER_INFO(spdlog::get("game"),
						                       "Tree trace: {} bends {:.3f} rad (d {:.2f}, r {:.2f}), sound {}",
						                       static_cast<uint32_t>(entity), angle, d, r, strength);
					    }
				    }
			    }
		    }
		    tree.bendAngle = angle;
		    tree.wasBent = angle != 0.0f;
		    anyBent = anyBent || tree.wasBent;
	    });
	state.SetAnyBent(anyBent);
}
} // namespace

uint8_t openblack::ecs::TreeBrightness()
{
	return TreeState().Brightness();
}

void openblack::ecs::UpdateTrees(float seconds)
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	const auto& camera = Locator::camera::value();
	const auto cameraPosition = camera.GetOrigin();
	// b = 200 + 55 x (horizontal view direction . normalize(camera focus - light position)), floored at 200, and the
	// tree's colour is multiplied by b/256. The light is the one every model is lit with (src/Graphics/ModelLight.h),
	// placed once a frame by the renderer, which runs after this, so the trees use the last frame's as in the original:
	// by day the default sun (-500000, 500000, -500000), and only in full night (sky type > 1.5) 3 units from the
	// player's hand towards the camera.
	const auto light = model_light::Light();
	const auto toFocus = camera.GetFocus() - light;
	const auto forward = camera.GetForward();
	const glm::vec2 heading(forward.x, forward.z);
	uint8_t brightness = 255;
	if (glm::length(toFocus) > 1e-4f && glm::length(heading) > 1e-4f)
	{
		const auto d = glm::normalize(toFocus);
		const auto v = glm::normalize(heading);
		const float dot = v.x * d.x + v.y * d.z;
		brightness = static_cast<uint8_t>(dot < 0.0f ? 200 : std::min(255, static_cast<int>(200.0f + 55.0f * dot)));
	}
	TreeState().SetBrightness(brightness);
	static const debug_env::Variable k_TreeTrace("OPENBLACK_TREE_TRACE");
	if (k_TreeTrace.Get() != nullptr)
	{
		auto& since = TreesDebugHooksData().traceSince;
		since += seconds;
		if (since > 1.0f)
		{
			since = 0.0f;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree trace: brightness {} ({:.3f}) heading ({:.2f},{:.2f})", brightness,
			                   static_cast<float>(brightness) / 256.0f, heading.x, heading.y);
		}
	}

	UpdateTreeBends();

	// A tree over 10 tall whose position is within 10 of the camera in x and z (and 18 in y) rustles about once a second
	// (LocalRand(1000 / frame ms) == 1, so the chance per frame is the frame's seconds): one of the editor.sad ambient
	// samples of the tree group (G_TreeRustle / G_TreeCreak).
	// The draw comes first, for every tree drawn (before the distance tests): n = 1000 / frame ms truncated toward zero, and
	// with 0 ms (pause) 1000/0 is +inf, which the float to int conversion turns into the integer indefinite 0x80000000, so
	// LocalRand's unsigned division is then by 2^31. (approximate) every tree with a mesh draws, not only those in view.
	if (!Locator::resources::has_value())
	{
		return;
	}
	const auto frameMs = game_clock::FrameGameMs();
	const auto rustleRange =
	    frameMs == 0 ? std::numeric_limits<int32_t>::min() : static_cast<int32_t>(1000.0f / static_cast<float>(frameMs));
	auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	registry.Each<const Tree, const Transform, const Mesh>(
	    [&](entt::entity entity, const Tree&, const Transform& transform, const Mesh& mesh) {
		    if (!meshes.Contains(mesh.id))
		    {
			    return;
		    }
		    if (game_random::LocalRand(rustleRange) != 1)
		    {
			    return;
		    }
		    const auto& at = transform.position;
		    if (std::abs(at.x - cameraPosition.x) > 10.0f || std::abs(at.z - cameraPosition.z) > 10.0f ||
		        std::abs(cameraPosition.y - at.y) >= 18.0f)
		    {
			    return;
		    }
		    if (ecs::object::GetHeight(entity) <= 10.0f)
		    {
			    return;
		    }
		    // key {voice, 2, group 20 = tree, surface, soundId 70 = ambient}: the row is {*, *, 20, *, 70}
		    audio::AnimationSounds::PlayFromTable(entity, at, {0, 2, 20, 0, 70});
	    });
}

namespace
{
/// Once per turn. An empty forest waits 2000 turns and is deleted. Otherwise, besides growing its trees
/// (ProcessTreesTurn), a forest may plant a new tree: with r = 2000 + rand(1000), f = min(1, 0.05 x its full grown
/// trees), T = turns since the last tree the forests planted anywhere and c = its attempts so far (+1 each turn), when
/// c f T / 300 > r it plants one (next to one of the rand(n/2 + 1) full grown trees nearest its centre) and c goes back
/// to 0.
void ProcessForests(uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<uint32_t> empty;
	std::vector<std::pair<uint32_t, entt::entity>> plant;
	auto& forests = ForestList().All();
	// Every forest's trees in one pass over the trees, each forest's in the order the pass visits them
	ecs::ForestTreeBuckets buckets;
	buckets.Reset(forests | std::views::keys);
	registry.Each<const Tree, const Transform>([&buckets](entt::entity entity, const Tree& tree, const Transform& transform) {
		buckets.Add(tree.forestId, entity, !tree.growing || transform.scale.x >= tree.maxSize,
		            glm::vec2(transform.position.x, transform.position.z));
	});
	for (auto& [id, forest] : forests)
	{
		const auto& bucket = buckets.Of(id);
		const auto count = bucket.count;
		const auto& grownTrees = bucket.grown;
		// Empty: no BigForest and no trees in either list
		if (count == 0 && forest.bigForest == entt::null)
		{
			if (forest.emptyTimer == 0)
			{
				forest.emptyTimer = 2000;
			}
			else if (--forest.emptyTimer < 2)
			{
				empty.push_back(id);
			}
			continue;
		}
		// A town's scenic forest is not processed (its trees do not grow, it plants none)
		if (forest.scenic)
		{
			continue;
		}
		const float r = game_random::GameFloatRand(1000.0f) + 2000.0f;
		const float f = std::min(1.0f, 0.05f * static_cast<float>(grownTrees.size()));
		const float t = static_cast<float>(turn - ForestList().LastTreeCreatedTurn());
		const float c = static_cast<float>(++forest.attempts);
		if (c * f * t / 300.0f > r && !grownTrees.empty())
		{
			forest.attempts = 0;
			// Nearest the forest centre first (metres, x and z only)
			const auto grown = ecs::GrownByDistance(grownTrees, forest.centre);
			// rand(n / 2 + 1), n = the grown list's count, always < n
			const auto pick = game_random::GameRand(static_cast<uint32_t>(grown.size() / 2 + 1));
			plant.emplace_back(id, grown.at(pick).second);
		}
	}
	for (const auto& [id, parent] : plant)
	{
		openblack::ecs::PlantTreeNear(id, parent);
	}
	for (const auto id : empty)
	{
		openblack::ecs::DeleteForest(id);
	}
}
} // namespace

float openblack::ecs::TreeGrowthAmount(float growthAmount, float rainMultiplier, float rain, float landAlignment)
{
	// Computed in the original's order: x 0.01, x rainMultiplier, x growthAmount, + growthAmount, then x 0.5, + 1 and x
	// the running value
	return growthAmount * (1.0f + 0.01f * rainMultiplier * rain) * (1.0f + 0.5f * landAlignment);
}

void openblack::ecs::ProcessTreesTurn(uint32_t turn)
{
	ProcessForests(turn);
	auto& registry = Locator::entitiesRegistry::value();
	const auto& constants = Locator::infoConstants::value();
	static const debug_env::Variable k_TreeTrace("OPENBLACK_TREE_TRACE");
	const bool trace = k_TreeTrace.Get() != nullptr;
	std::vector<std::pair<entt::entity, float>> growing;
	registry.Each<Tree, const Transform>([&constants, &growing, trace](entt::entity entity, Tree& tree,
	                                                                   const Transform& transform) {
		// Only a tree in a forest is processed at all: the forest walks its own list.
		if (!IsInForest(tree.forestId) || IsScenicForest(tree.forestId))
		{
			return;
		}
		const auto& info = constants.tree.at(static_cast<size_t>(tree.type));
		// The counter ticks every turn and only the turn it reaches 0 does anything (growTurns = 10 for
		// every type, so once a second).
		if (tree.growCounter != 0 && --tree.growCounter != 0)
		{
			return;
		}
		tree.growCounter = static_cast<uint16_t>(std::max(1u, info.growsAfterNumGameTurns));
		if (!tree.growing)
		{
			return;
		}
		if (transform.scale.x >= tree.maxSize)
		{
			tree.growing = false;
			return;
		}
		// Rain and alignment are both asked at the tree's point, the land altitude plus the map coordinates' own y: that
		// is what Transform::position already holds for a tree (TreeArchetype is created at GetHeightAt)
		const float rain = weather::GetMaxRainingOrSnowingAt(transform.position);
		const float landAlignment = effects::alignment::LandAlignmentAt(transform.position);
		const float amount =
		    openblack::ecs::TreeGrowthAmount(info.growthAmount, info.rainingAcceleratorMultiplier, rain, landAlignment);
		if (trace)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Tree trace: {} forest {} growth {:.4f} = {:.4f} x rain {:.0f} (mult {:.2f}) x alignment {:.3f}",
			                   static_cast<uint32_t>(entity), tree.forestId, amount, info.growthAmount, rain,
			                   info.rainingAcceleratorMultiplier, landAlignment);
		}
		growing.emplace_back(entity, amount);
	});
	for (const auto& [entity, amount] : growing)
	{
		const float grown = GrowTree(entity, amount, false);
		auto* tree = registry.TryGet<Tree>(entity);
		if (tree == nullptr)
		{
			continue;
		}
		const auto& transform = registry.Get<const Transform>(entity);
		tree->growing = transform.scale.x < tree->maxSize;
		if (trace)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree trace: {} forest {} grew {:.4f} to {:.3f} of {:.3f}{}",
			                   static_cast<uint32_t>(entity), tree->forestId, grown, transform.scale.x, tree->maxSize,
			                   tree->growing ? "" : " (full grown)");
		}
	}
}
