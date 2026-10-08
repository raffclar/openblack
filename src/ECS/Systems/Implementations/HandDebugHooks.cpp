/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <L3DFile.h>
#include <LNDFile.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "ECS/Abodes.h"
#include "ECS/Animations.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Physics/FragMesh.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "ECS/SeaDebugHooks.h"
#include "ECS/Sharks.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownFeatures.h"
#include "ECS/Trees.h"
#include "ECS/Weather/Climate.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Graphics/Texture2D.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "InfoConstants.h"
#include "Input/GameCursor.h"
#include "Input/HandDemo.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

namespace
{
/// What these hooks keep between calls, in the debug hooks' store (Locator::debugHooks)
struct HandDebugHooksState
{
	int updateTestAbodeLeft {-1};
	int updateTestAbodeWanted {0};
	float updateTestAbodeDelay {2.0f};
	float updateTestAbodeInterval {0.4f};
	float updateTestAbodeNext {0.0f};
	int updateTestAbodeWanted2 {-1};
	float updateTestAbodeDelay2 {2.0f};
	bool updateTestAbodeDone {false};
	int updateTestAbodeWanted3 {-1};
	float updateTestAbodeDelay3 {2.0f};
	bool updateTestAbodeDone2 {false};
	int dumpEntityCountsUpdates {0};
};

HandDebugHooksState& HandDebugHooksData()
{
	return openblack::Locator::debugHooks::value().Get<HandDebugHooksState>();
}

/// The test tree's store: the nearest available storage pit whose centre is within max(8 m, its bounding radius) of
/// the point in XZ, if any
std::optional<entt::entity> NearestTestStore(glm::vec3 point)
{
	using namespace openblack::ecs::components;
	const auto& registry = openblack::Locator::entitiesRegistry::value();
	std::optional<entt::entity> store;
	float best = std::numeric_limits<float>::max();
	registry.Each<const StoragePit, const Transform>([&](entt::entity entity, const StoragePit&, const Transform& transform) {
		if (!openblack::ecs::IsAvailable(entity))
		{
			return;
		}
		float radius = 8.0f;
		if (const auto* fixed = registry.TryGet<const Fixed>(entity); fixed != nullptr)
		{
			radius = std::max(radius, fixed->boundingRadius);
		}
		const float distance =
		    glm::distance(glm::vec2(point.x, point.z), glm::vec2(transform.position.x, transform.position.z));
		if (distance <= radius && distance < best)
		{
			best = distance;
			store = entity;
		}
	});
	return store;
}
} // namespace

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

// Environment-variable test hooks (OPENBLACK_HAND_TEST_*, OPENBLACK_CAMERA_FLY, ...), run once when the landscape exists.
void HandSystem::RunDebugHooks() noexcept
{
	ecs::RunSeaDebugHooks();  // OPENBLACK_TEST_SEA
	ecs::RunSharkDebugHook(); // OPENBLACK_TEST_SHARK
	// OPENBLACK_TEST_HAND_DEMO=<name>: PLAY_HAND_DEMO(name, 0, 0) at once (Data\HandDemo\<name>.hnd, Input/HandDemo)
	if (const char* demo = std::getenv("OPENBLACK_TEST_HAND_DEMO"); demo != nullptr)
	{
		hand_demo::Play(demo, 0, false, false);
	}
	if (const char* at = std::getenv("OPENBLACK_HAND_TEST_ROCK"); at != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		if (std::sscanf(at, "%f,%f", &x, &z) == 2)
		{
			const float y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
			if (std::getenv("OPENBLACK_HAND_TEST_NO_BOULDER") == nullptr)
				archetypes::MobileStaticArchetype::Create(
				    glm::vec3(x + 6.0f, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x + 6.0f, z)), z),
				    MobileStaticInfo::Boulder1Lime, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
			Locator::entitiesRegistry::value().SetDirty();
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: boulder spawned at ({}, {}, {})", x, y, z);
			// ...and a wood pile next to it, to test the multi pick up (HandWood) and special_hold fill.
			const float px = x;
			const float py = Locator::terrainSystem::value().GetHeightAt(glm::vec2(px, z));
			// OPENBLACK_HAND_TEST_FOOD=1 spawns a food pile instead.
			const bool food = std::getenv("OPENBLACK_HAND_TEST_FOOD") != nullptr;
			archetypes::PotArchetype::Create(glm::vec3(px, py, z), 0.0f, food ? PotInfo::MagicFood : PotInfo::WoodPile_1,
			                                 food ? 1000 : 4000);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: {} pile spawned at ({}, {}, {})", food ? "food" : "wood", px,
			                   py, z);
		}
	}
	// OPENBLACK_HAND_TEST_SPLIT="x,z,scale,rounds": a chalk boulder of that scale, tapped (the rock's tap), and its
	// halves tapped again for the given rounds while they are taller than 0.7
	if (const char* split = std::getenv("OPENBLACK_HAND_TEST_SPLIT"); split != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		float scale = 1.0f;
		int rounds = 1;
		if (std::sscanf(split, "%f,%f,%f,%d", &x, &z, &scale, &rounds) >= 2)
		{
			const auto rock = archetypes::MobileStaticArchetype::Create(
			    glm::vec3(x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)), z), MobileStaticInfo::Boulder1Lime,
			    0.0f, 0.0f, 0.0f, 0.0f, scale);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: rock radius {:.2f} height {:.2f} liftable {}",
			                   Rocks::Radius2D(rock), Rocks::Height(rock), Rocks::ValidForPlaceInHand(rock));
			std::vector<entt::entity> rocks {rock};
			for (int round = 0; round < rounds; ++round)
			{
				std::vector<entt::entity> next;
				for (const auto r : rocks)
				{
					if (!Rocks::ValidToTap(r))
					{
						next.push_back(r);
						continue;
					}
					const auto halves = Rocks::Tap(r, Locator::entitiesRegistry::value().Get<const Transform>(r).position);
					next.insert(next.end(), halves.begin(), halves.end());
				}
				rocks = next;
			}
		}
	}
	// OPENBLACK_TEST_PHYSICS="x,z,height,vx,vy,vz[,scale[,count]]": chalk boulders put in physics at height over the land
	// with that velocity (PhysicsObjects::AddObject), one every 3 units along x
	if (const char* test = std::getenv("OPENBLACK_TEST_PHYSICS"); test != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		float height = 10.0f;
		glm::vec3 v(0.0f);
		float scale = 0.5f;
		int count = 1;
		if (std::sscanf(test, "%f,%f,%f,%f,%f,%f,%f,%d", &x, &z, &height, &v.x, &v.y, &v.z, &scale, &count) >= 3)
		{
			for (int i = 0; i < count; ++i)
			{
				const glm::vec2 at(x + 3.0f * static_cast<float>(i), z);
				const auto rock = archetypes::MobileStaticArchetype::Create(
				    glm::vec3(at.x, Locator::terrainSystem::value().GetHeightAt(at) + height, at.y),
				    MobileStaticInfo::Boulder1Lime, 0.0f, 0.3f, 0.7f * static_cast<float>(i), 0.2f, scale);
				const auto* po = physics::PhysicsObjects::AddObject(rock, v, glm::vec3(0.0f), entt::null, true);
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Physics test: rock {} in physics {} (mass {:.2f}, radius {:.2f}, {} vertices)", i,
				                   po != nullptr, po != nullptr ? po->body.Mass() : 0.0f,
				                   po != nullptr ? po->body.Radius() : 0.0f, po != nullptr ? po->body.Vertices().size() : 0);
			}
			Locator::entitiesRegistry::value().SetDirty();
		}
	}
	// OPENBLACK_TEST_HIT_VILLAGER="speed[,scale[,index]]": a chalk boulder thrown at a villager from 8 units away
	if (const char* hitTest = std::getenv("OPENBLACK_TEST_HIT_VILLAGER"); hitTest != nullptr)
	{
		float speed = 20.0f;
		float scale = 0.5f;
		int index = 0;
		std::sscanf(hitTest, "%f,%f,%d", &speed, &scale, &index);
		auto& registry = Locator::entitiesRegistry::value();
		std::optional<entt::entity> target;
		int seen = 0;
		registry.Each<const Villager, const Transform>([&](entt::entity e, const Villager&, const Transform&) {
			if (!target && seen++ == index)
			{
				target = e;
			}
		});
		if (target)
		{
			const auto at = registry.Get<const Transform>(*target).position;
			const glm::vec2 from(at.x - 8.0f, at.z);
			const glm::vec3 start(from.x, Locator::terrainSystem::value().GetHeightAt(from) + 1.5f, from.y);
			const auto rock =
			    archetypes::MobileStaticArchetype::Create(start, MobileStaticInfo::Boulder1Lime, 0.0f, 0.0f, 0.0f, 0.0f, scale);
			const auto direction = glm::normalize(at + glm::vec3(0.0f, 0.8f, 0.0f) - start);
			physics::PhysicsObjects::AddObject(rock, direction * speed, glm::vec3(0.0f), entt::null, true);
			registry.SetDirty();
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Physics test: rock thrown at villager {} at ({:.1f}, {:.1f}, {:.1f}), life {:.2f}",
			                   static_cast<uint32_t>(*target), at.x, at.y, at.z, registry.Get<const Villager>(*target).life);
		}
	}
	// OPENBLACK_TEST_THROW_TREE="x,z,vx,vy,vz": a beech thrown from 3 units over the land (it lands as a DeadTree)
	if (const char* treeTest = std::getenv("OPENBLACK_TEST_THROW_TREE"); treeTest != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		glm::vec3 v(0.0f);
		if (std::sscanf(treeTest, "%f,%f,%f,%f,%f", &x, &z, &v.x, &v.y, &v.z) == 5)
		{
			const glm::vec3 at(x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) + 3.0f, z);
			const auto tree = archetypes::TreeArchetype::Create(1, at, TreeInfo::Beech, true, 0.0f, 1.0f, 1.0f);
			const auto* po = physics::PhysicsObjects::AddObject(tree, v, glm::vec3(0.0f), entt::null, true);
			UpdateRoots(tree); // with its roots, as a tree the hand pulled up (they follow its drawn pose: TreeRoots)
			Locator::entitiesRegistry::value().SetDirty();
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics test: tree {} thrown (mass {:.1f}, radius {:.2f})",
			                   static_cast<uint32_t>(tree), po != nullptr ? po->body.Mass() : 0.0f,
			                   po != nullptr ? po->body.Radius() : 0.0f);
		}
	}
	// OPENBLACK_TEST_HIT_ABODE="speed[,scale[,index[,count]]]": chalk boulders thrown at a house from 15 units away, one
	// every 1.5 s (the abode's physics impact / FragMesh)
	if (const char* abodeTest = std::getenv("OPENBLACK_TEST_HIT_ABODE"); abodeTest != nullptr)
	{
		float speed = 25.0f;
		float scale = 0.5f;
		int index = 0;
		int count = 1;
		std::sscanf(abodeTest, "%f,%f,%d,%d", &speed, &scale, &index, &count);
		auto& registry = Locator::entitiesRegistry::value();
		std::optional<entt::entity> target;
		int seen = 0;
		registry.Each<const Abode, const Transform>([&](entt::entity e, const Abode&, const Transform&) {
			if (!target && !registry.AllOf<StoragePit>(e) && seen++ == index)
			{
				target = e;
			}
		});
		if (target)
		{
			_testAbode = {*target, speed, scale, count, 0.0f};
			const auto at = registry.Get<const Transform>(*target).position;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics test: throwing at abode {} at ({:.1f}, {:.1f}, {:.1f})",
			                   static_cast<uint32_t>(*target), at.x, at.y, at.z);
		}
	}
	// OPENBLACK_TEST_FRAGMESH=1: which buildings can be broken (FragMesh) and how many triangles they have
	if (std::getenv("OPENBLACK_TEST_FRAGMESH") != nullptr)
	{
		auto& registry = Locator::entitiesRegistry::value();
		int ok = 0;
		int failed = 0;
		registry.Each<const Abode, const Transform>([&](entt::entity e, const Abode& abode, const Transform& t) {
			const auto mesh = physics::FragMesh::FromEntity(e);
			const auto* m = registry.TryGet<const Mesh>(e);
			std::string subs;
			if (m != nullptr && Locator::resources::value().GetMeshes().Contains(m->id))
			{
				for (const auto& sm : Locator::resources::value().GetMeshes().Handle(m->id)->GetSubMeshes())
				{
					subs += fmt::format(" [lod {} status {} phys {} tris {}]", sm->GetFlags().lodMask, sm->GetFlags().status,
					                    sm->IsPhysics(), sm->GetCollisionIndices().size() / 3);
				}
			}
			mesh ? ++ok : ++failed;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "FragMesh test: abode {} type {} at ({:.0f},{:.0f}) tris {}{}",
			                   static_cast<uint32_t>(e), static_cast<int>(abode.type), t.position.x, t.position.z,
			                   mesh ? mesh->TriangleCount() : 0, subs);
		});
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "FragMesh test: {} breakable, {} not", ok, failed);
	}
	// OPENBLACK_TEST_TUG="x,z,delay,hold": a beech at x,z, and the action button held for hold seconds after delay seconds
	// (the mouse stays still: the tree must lean at most, not come out)
	if (const char* tug = std::getenv("OPENBLACK_TEST_TUG"); tug != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		float delay = 2.0f;
		float hold = 2.0f;
		if (std::sscanf(tug, "%f,%f,%f,%f", &x, &z, &delay, &hold) >= 2)
		{
			const glm::vec3 at(x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)), z);
			archetypes::TreeArchetype::Create(1, at, TreeInfo::Beech, true, 0.0f, 1.0f, 1.0f);
			Locator::entitiesRegistry::value().SetDirty();
			_testActionDelay = delay;
			_testActionHold = hold;
		}
	}
	// OPENBLACK_HAND_TEST_HOLD=<scale>: the hand starts holding a chalk boulder of that scale (reflection tests)
	if (const char* hold = std::getenv("OPENBLACK_HAND_TEST_HOLD"); hold != nullptr)
	{
		const float scale = std::max(0.05f, static_cast<float>(std::atof(hold)));
		const auto rock = archetypes::MobileStaticArchetype::Create(glm::vec3(0.0f), MobileStaticInfo::Boulder1Lime, 0.0f, 0.0f,
		                                                            0.0f, 0.0f, scale);
		Locator::entitiesRegistry::value().SetDirty();
		PickUp(rock);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: holding a boulder of scale {}", scale);
	}
	// OPENBLACK_HAND_TEST_DROP="x,z,seconds[,kind]": the hand holds a rock (kind 0, scale 0.5), a hand pot of 300 food (1)
	// or of 300 wood (2), the first villager (3), the first tree (4) or the first animal (5), and puts it down gently at
	// (x, z) after that many seconds (HandSystem::Drop -> Release with zero velocity -> InitialisePhysicsFromHand)
	if (const char* drop = std::getenv("OPENBLACK_HAND_TEST_DROP"); drop != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		float delay = 2.0f;
		int kind = 0;
		if (std::sscanf(drop, "%f,%f,%f,%d", &x, &z, &delay, &kind) >= 2)
		{
			auto& registry = Locator::entitiesRegistry::value();
			std::optional<entt::entity> held;
			// made standing on the ground there (the hand keeps the altitude above the ground it was picked at)
			const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
			if (kind == 1 || kind == 2)
			{
				held = archetypes::PotArchetype::Create(glm::vec3(x, ground, z), 0.0f,
				                                        kind == 1 ? PotInfo::HandFood : PotInfo::HandWood, 300);
			}
			else if (kind == 3)
			{
				registry.Each<const Villager>([&](entt::entity e, const Villager&) {
					if (!held)
					{
						held = e;
					}
				});
			}
			else if (kind == 4)
			{
				registry.Each<const Tree>([&](entt::entity e, const Tree&) {
					if (!held)
					{
						held = e;
					}
				});
			}
			else if (kind == 5)
			{
				registry.Each<const Animal>([&](entt::entity e, const Animal&) {
					if (!held)
					{
						held = e;
					}
				});
			}
			else
			{
				held = archetypes::MobileStaticArchetype::Create(glm::vec3(x, ground, z), MobileStaticInfo::Boulder1Lime, 0.0f,
				                                                 0.0f, 0.0f, 0.0f, 0.5f);
			}
			registry.SetDirty();
			if (held && registry.Valid(*held))
			{
				if (kind == 1 || kind == 2)
				{
					ComputeHoldParameters(*held);
					_held = *held;
				}
				else
				{
					PickUp(*held);
				}
				_testDropAt = glm::vec3(x, 0.0f, z);
				_testDropIn = delay;
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: holding kind {} ({}), put down at ({}, {}) in {} s", kind,
				                   static_cast<uint32_t>(*held), x, z, delay);
			}
		}
	}
	// OPENBLACK_HAND_TEST_FISH=1: a splash next to the first shoal, then catching fish there with the action held 3 s
	if (std::getenv("OPENBLACK_HAND_TEST_FISH") != nullptr)
	{
		auto& registry = Locator::entitiesRegistry::value();
		std::optional<glm::vec3> point;
		registry.Each<const FishFarm>([&point](const FishFarm& farm) {
			if (!point && farm.shoal)
			{
				point = farm.shoal->fish[0].position;
			}
		});
		if (point)
		{
			SplashHand(*point);
			// as the press does: packet 0x1B, applied at the next turn's start
			const auto farm = FishFarmUnderHand(*point);
			const bool caught = farm.has_value();
			if (caught)
			{
				SendStartLockedSelect(*farm);
			}
			_pickPressHeld = caught;
			_testActionSeconds = 3.0f;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: fish at ({:.1f}, {:.1f}), catching {}", point->x, point->z,
			                   caught);
		}
	}
	// OPENBLACK_HAND_TEST_FIELD=<seconds>: taking food from the first field with the action held that long (3 s if 1)
	if (const char* fieldTest = std::getenv("OPENBLACK_HAND_TEST_FIELD"); fieldTest != nullptr)
	{
		std::optional<entt::entity> field;
		Locator::entitiesRegistry::value().Each<const Field>([&field](entt::entity entity, const Field&) {
			if (!field)
			{
				field = entity;
			}
		});
		if (field)
		{
			// as the press does: packet 0x1B, applied at the next turn's start
			const bool taking = FieldValidForLockedSelect(*field);
			if (taking)
			{
				SendStartLockedSelect(*field);
			}
			_pickPressHeld = taking;
			const float seconds = static_cast<float>(std::atof(fieldTest));
			_testActionSeconds = seconds > 1.0f ? seconds : 3.0f;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: field {}, taking {}", static_cast<uint32_t>(*field), taking);
		}
	}
	// OPENBLACK_HAND_TEST_FOREST=1: take a tree from the first big forest
	if (std::getenv("OPENBLACK_HAND_TEST_FOREST") != nullptr)
	{
		auto& registry = Locator::entitiesRegistry::value();
		std::optional<entt::entity> forest;
		registry.Each<const BigForest>([&forest](entt::entity entity, const BigForest&) {
			if (!forest)
			{
				forest = entity;
			}
		});
		if (forest)
		{
			const auto before = registry.Get<BigForest>(*forest).wood;
			const auto countTrees = [&registry]() {
				size_t n = 0;
				registry.Each<const Tree>([&n](const Tree&) { ++n; });
				return n;
			};
			const auto trees = countTrees();
			// the 0x13's handler at once (the test reads the result in the same frame)
			ApplyPlaceInHand(*forest);
			const bool taken = _held.has_value();
			const bool left = registry.Valid(*forest);
			SPDLOG_LOGGER_INFO(
			    spdlog::get("game"), "Hand test: forest wood {} -> {}, scale {}, trees {} -> {}, holding a tree {}", before,
			    left ? registry.Get<BigForest>(*forest).wood : 0.0f, left ? registry.Get<Transform>(*forest).scale.x : 0.0f,
			    trees, countTrees(), taken && _held && registry.AllOf<Tree>(*_held));
		}
	}
	// Debug: OPENBLACK_TIME_OF_DAY=<hour> sets the game time (night / dusk screenshots).
	// (the day / night clock lives as long as the Game)
	if (const char* hour = std::getenv("OPENBLACK_TIME_OF_DAY"); hour != nullptr && Locator::dayNightClock::has_value())
	{
		Game::SetTime(std::clamp(static_cast<float>(std::atof(hour)), 0.0f, 24.0f));
	}
	// Debug: OPENBLACK_CAMERA_FLY="ox,oy,oz,fx,fy,fz" flies the camera there (close-up screenshots).
	if (const char* fly = std::getenv("OPENBLACK_CAMERA_FLY"); fly != nullptr && Locator::camera::has_value())
	{
		glm::vec3 o(0.0f);
		glm::vec3 f(0.0f);
		if (std::sscanf(fly, "%f,%f,%f,%f,%f,%f", &o.x, &o.y, &o.z, &f.x, &f.y, &f.z) == 6)
		{
			Locator::camera::value().GetModel().SetFlight(o, f);
		}
	}
	// Debug: OPENBLACK_TEST_ANIM="clip[,milliseconds]" plays that AllAnims.anm clip (index) in a loop on every villager,
	// all at that time into it (default 0) and with speed 0 if a time is given (a still pose for screenshots).
	if (const char* anim = std::getenv("OPENBLACK_TEST_ANIM"); anim != nullptr)
	{
		int clip = 0;
		float time = -1.0f;
		std::sscanf(anim, "%d,%f", &clip, &time);
		auto& registry = Locator::entitiesRegistry::value();
		std::vector<entt::entity> villagers;
		registry.Each<const Villager>([&villagers](entt::entity e, const Villager&) { villagers.push_back(e); });
		for (const auto e : villagers)
		{
			auto& animation = registry.AssignOrReplace<SkeletalAnimation>(e);
			animation.clip = ecs::ClipId(static_cast<uint32_t>(clip));
			animation.clipIndex = clip;
			animation.locked = true;
			animation.hasClip = true;
			animation.time = std::max(0.0f, time);
			animation.speed = time >= 0.0f ? 0.0f : 1.0f;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animation test: clip {} on {} villagers", clip, villagers.size());
	}
	// Debug: OPENBLACK_TEST_CARRY=<CARRIED_OBJECT> every villager carries that (2 axe ... 15 tree 3; ECS/CarriedProps)
	if (const char* carry = std::getenv("OPENBLACK_TEST_CARRY"); carry != nullptr)
	{
		auto& registry = Locator::entitiesRegistry::value();
		std::vector<entt::entity> villagers;
		registry.Each<const Villager>([&villagers](entt::entity e, const Villager&) { villagers.push_back(e); });
		for (const auto e : villagers)
		{
			auto& animation = registry.AllOf<SkeletalAnimation>(e) ? registry.Get<SkeletalAnimation>(e)
			                                                       : registry.Assign<SkeletalAnimation>(e);
			animation.carriedObject = std::atoi(carry);
			animation.carriedLocked = true;
		}
	}
	// Debug: OPENBLACK_TEST_THROW_VILLAGER="n,vx,vy,vz" throws the n-th villager with that velocity (FLYING, LANDED clips)
	if (const char* throwTest = std::getenv("OPENBLACK_TEST_THROW_VILLAGER"); throwTest != nullptr)
	{
		int wanted = 0;
		glm::vec3 velocity(0.0f, 6.0f, 3.0f);
		std::sscanf(throwTest, "%d,%f,%f,%f", &wanted, &velocity.x, &velocity.y, &velocity.z);
		auto& registry = Locator::entitiesRegistry::value();
		std::optional<entt::entity> target;
		int seen = 0;
		registry.Each<const Villager>([&](entt::entity e, const Villager&) {
			if (!target && seen++ == wanted)
			{
				target = e;
			}
		});
		if (target && physics::PhysicsObjects::AddObject(*target, velocity, glm::vec3(0.0f), entt::null, true) != nullptr)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animation test: villager {} thrown", wanted);
		}
	}
	// Debug: OPENBLACK_TEST_VIEW_VILLAGER="n[,distance[,angle]]" flies the camera to look at the n-th villager from that
	// many metres (default 3), from that side (degrees around it, default 0 = +z), slightly above (villager close-ups).
	if (const char* view = std::getenv("OPENBLACK_TEST_VIEW_VILLAGER"); view != nullptr && Locator::camera::has_value())
	{
		int wanted = 0;
		float distance = 3.0f;
		float angle = 0.0f;
		std::sscanf(view, "%d,%f,%f", &wanted, &distance, &angle);
		auto& registry = Locator::entitiesRegistry::value();
		int index = 0;
		registry.Each<const Villager, const Transform>([&](const Villager&, const Transform& t) {
			if (index++ != wanted)
			{
				return;
			}
			const float radians = glm::radians(angle);
			const glm::vec3 focus = t.position + glm::vec3(0.0f, 0.8f, 0.0f);
			const glm::vec3 origin =
			    focus + glm::vec3(std::sin(radians) * distance, distance * 0.35f, std::cos(radians) * distance);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Villager view: villager {} at ({}, {}, {})", wanted, t.position.x,
			                   t.position.y, t.position.z);
			Locator::camera::value().GetModel().SetFlight(origin, focus);
		});
	}
	// Debug: OPENBLACK_PRINT_ALTITUDE="x,z" logs the landscape height there (GetHeightAt).
	if (const char* at = std::getenv("OPENBLACK_PRINT_ALTITUDE"); at != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		if (std::sscanf(at, "%f,%f", &x, &z) == 2)
		{
			// drawn: the landscape mesh (every vertex of altitude 3 or less at 0); unflattened: with the land flattening off
			const auto& land = Locator::terrainSystem::value();
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Altitude at ({}, {}): {:.7f} (drawn {:.7f}, unflattened {:.7f})", x, z,
			                   land.GetHeightAt(glm::vec2(x, z)), land.GetDrawnHeightAt(glm::vec2(x, z)),
			                   land.GetUnflattenedHeightAt(glm::vec2(x, z)));
			// Every mesh entity within 15 m: its kind and how far its lowest vertex is above the land.
			{
				auto& registry = Locator::entitiesRegistry::value();
				// how far the lowest drawn vertex of an entity's mesh is above the land (0 without a loaded mesh)
				const auto lowestGap = [&registry, &land](entt::entity e) {
					auto& meshes = Locator::resources::value().GetMeshes();
					const auto& mesh = registry.Get<const Mesh>(e);
					if (!meshes.Contains(mesh.id))
					{
						return 0.0f;
					}
					const auto& transform = registry.Get<const Transform>(e);
					float gap = std::numeric_limits<float>::max();
					for (const auto& subMesh : meshes.Handle(mesh.id)->GetSubMeshes())
					{
						if (subMesh->IsPhysics())
						{
							continue;
						}
						for (const auto& vertex : subMesh->GetCollisionPositions())
						{
							const auto world = transform.position + transform.rotation * (transform.scale * vertex);
							gap = std::min(gap, world.y - land.GetHeightAt(glm::vec2(world.x, world.z)));
						}
					}
					return gap == std::numeric_limits<float>::max() ? 0.0f : gap;
				};
				registry.Each<const Transform, const Mesh>([&](entt::entity e, const Transform& t, const Mesh&) {
					if (glm::distance(glm::vec2(t.position.x, t.position.z), glm::vec2(x, z)) > 15.0f)
					{
						return;
					}
					const char* kind = registry.AllOf<MobileStatic>(e)     ? "MobileStatic"
					                   : registry.AllOf<MobileObject>(e)   ? "MobileObject"
					                   : registry.AllOf<Feature>(e)        ? "Feature"
					                   : registry.AllOf<Tree>(e)           ? "Tree"
					                   : registry.AllOf<AnimatedStatic>(e) ? "AnimatedStatic"
					                                                       : "other";
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "  near: {} at ({:.1f},{:.2f},{:.1f}) scale {:.2f} gap {:.3f}",
					                   kind, t.position.x, t.position.y, t.position.z, t.scale.x, lowestGap(e));
				});
			}
			// OPENBLACK_MARK_LOWEST=1: a red dot at the lowest vertex of each mobile static nearby, and one on the land
			// under it (checks that the CPU copy of the mesh matches what is drawn).
			if (std::getenv("OPENBLACK_MARK_LOWEST") != nullptr)
			{
				auto& registry = Locator::entitiesRegistry::value();
				auto& meshes = Locator::resources::value().GetMeshes();
				auto& textures = Locator::resources::value().GetTextures();
				const auto textureId = entt::hashed_string("raw/S_SpriteSheet1a");
				if (!textures.Contains(textureId))
				{
					auto& fileSystem = Locator::filesystem::value();
					textures.Load(
					    textureId, resources::Texture2DLoader::FromDiskTag {},
					    fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Textures>() / "S_SpriteSheet1a.raw"));
				}
				const auto texture = textures.Handle(textureId)->GetNativeHandle();
				std::vector<glm::vec3> marks;
				registry.Each<const Transform, const Mesh, const MobileStatic>(
				    [&](entt::entity, const Transform& t, const Mesh& mesh, const MobileStatic&) {
					    if (glm::distance(glm::vec2(t.position.x, t.position.z), glm::vec2(x, z)) > 15.0f ||
					        !meshes.Contains(mesh.id))
					    {
						    return;
					    }
					    glm::vec3 lowest(0.0f, std::numeric_limits<float>::max(), 0.0f);
					    for (const auto& sub : meshes.Handle(mesh.id)->GetSubMeshes())
					    {
						    for (const auto& v : sub->GetCollisionPositions())
						    {
							    const auto w = t.position + t.rotation * (t.scale * v);
							    if (w.y < lowest.y)
							    {
								    lowest = w;
							    }
						    }
					    }
					    marks.push_back(lowest);
				    });
				for (const auto& m : marks)
				{
					const auto e = registry.Create();
					registry.Assign<Sprite>(e, texture, glm::vec2(0.0f), glm::vec2(1.0f / 8.0f),
					                        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), false);
					registry.Assign<Transform>(e, m, glm::mat3(1.0f), glm::vec3(0.35f));
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "  mark lowest ({:.2f},{:.2f},{:.2f}) land {:.2f}", m.x, m.y, m.z,
					                   Locator::terrainSystem::value().GetHeightAt(glm::vec2(m.x, m.z)));
				}
				registry.SetDirty();
			}
			// Compare with the drawn landscape mesh (the Bullet land blocks are built from it) on a small grid.
			for (int i = 0; i < 5; ++i)
			{
				for (int j = 0; j < 5; ++j)
				{
					const glm::vec2 p(x + static_cast<float>(i) * 2.5f, z + static_cast<float>(j) * 2.5f);
					const auto hit = Locator::dynamicsSystem::value().RayCastClosestHit(glm::vec3(p.x, 500.0f, p.y),
					                                                                    glm::vec3(0.0f, -1.0f, 0.0f), 1000.0f);
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "  ({:.1f},{:.1f}) GetHeightAt {:.3f} mesh {:.3f}", p.x, p.y,
					                   Locator::terrainSystem::value().GetHeightAt(p), hit ? hit->first.position.y : -1.0f);
				}
			}
		}
	}
	// Debug: OPENBLACK_DUMP_STATIC_GAPS=1 logs, for every MobileStatic, the gap between its lowest vertex and the
	// landscape under it (negative = buried), with its rotation and with the transposed rotation.
	if (std::getenv("OPENBLACK_DUMP_STATIC_GAPS") != nullptr)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto& meshes = Locator::resources::value().GetMeshes();
		const auto& terrain = Locator::terrainSystem::value();
		registry.Each<const Transform, const Mesh, const MobileStatic>(
		    [&](entt::entity, const Transform& t, const Mesh& mesh, const MobileStatic& statics) {
			    if (!meshes.Contains(mesh.id))
			    {
				    return;
			    }
			    const auto l3d = meshes.Handle(mesh.id);
			    float gapA = std::numeric_limits<float>::max();
			    float gapB = std::numeric_limits<float>::max();
			    const auto rt = glm::transpose(t.rotation);
			    for (const auto& sub : l3d->GetSubMeshes())
			    {
				    for (const auto& v : sub->GetCollisionPositions())
				    {
					    const auto a = t.position + t.rotation * (t.scale * v);
					    const auto b = t.position + rt * (t.scale * v);
					    gapA = std::min(gapA, a.y - terrain.GetHeightAt(glm::vec2(a.x, a.z)));
					    gapB = std::min(gapB, b.y - terrain.GetHeightAt(glm::vec2(b.x, b.z)));
				    }
			    }
			    SPDLOG_LOGGER_INFO(
			        spdlog::get("game"),
			        "Static gap: type {} mesh {} at ({:.2f},{:.2f}) scale {:.3f} gap {:.3f} transposed {:.3f} localMinY {:.3f}",
			        static_cast<int>(statics.type),
			        static_cast<int>(Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(statics.type)).meshId),
			        t.position.x, t.position.z, t.scale.x, gapA, gapB, l3d->GetBoundingBox().minima.y);
		    });
	}
	// Debug: OPENBLACK_HAND_TEST_STORE_TAKE="wood,food" takes that much from every storage pit (store visuals).
	if (const char* take = std::getenv("OPENBLACK_HAND_TEST_STORE_TAKE"); take != nullptr)
	{
		uint32_t wood = 0;
		uint32_t food = 0;
		if (std::sscanf(take, "%u,%u", &wood, &food) == 2)
		{
			std::vector<entt::entity> stores;
			Locator::entitiesRegistry::value().Each<const StoragePit>(
			    [&](entt::entity e, const StoragePit&) { stores.push_back(e); });
			for (const auto store : stores)
			{
				const auto w = StoragePitStore::RemoveResource(store, ResourceType::Wood, wood);
				const auto f = StoragePitStore::RemoveResource(store, ResourceType::Food, food);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: store took wood {} food {}, left wood {} food {}", w, f,
				                   StoragePitStore::GetResource(store, ResourceType::Wood),
				                   StoragePitStore::GetResource(store, ResourceType::Food));
			}
		}
	}
	// Debug: OPENBLACK_HAND_TEST_TREE="x,z" plants a beech there, and logs the nearest village store.
	if (const char* at = std::getenv("OPENBLACK_HAND_TEST_TREE"); at != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		if (std::sscanf(at, "%f,%f", &x, &z) == 2)
		{
			const glm::vec3 position(x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)), z);
			archetypes::TreeArchetype::Create(1, position, TreeInfo::Beech, true, 0.0f, 1.0f, 1.0f);
			Locator::entitiesRegistry::value().SetDirty();
			float best = std::numeric_limits<float>::max();
			glm::vec3 store(0.0f);
			Locator::entitiesRegistry::value().Each<const StoragePit, const Transform>(
			    [&](entt::entity, const StoragePit&, const Transform& t) {
				    if (glm::distance(t.position, position) < best)
				    {
					    best = glm::distance(t.position, position);
					    store = t.position;
				    }
			    });
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Hand test: beech planted at ({}, {}); nearest store at ({:.1f}, {:.1f}) d={:.1f}", x, z,
			                   store.x, store.z, best);
			// OPENBLACK_HAND_TEST_TREE="x,z,dead": a second beech 8 m away, felled as if thrown towards +x.
			if (std::strstr(at, "dead") != nullptr)
			{
				const glm::vec3 p2(x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z + 8.0f)), z + 8.0f);
				MakeDeadTree(archetypes::TreeArchetype::Create(1, p2, TreeInfo::Beech, true, 0.0f, 1.0f, 1.0f),
				             glm::vec3(1.0f, 0.0f, 0.0f));
			}
			// OPENBLACK_HAND_TEST_TREE="x,z,roots": MSH_T_ROOTS and MSH_T_ROOTS_PILE next to the tree, bounding boxes logged.
			if (std::strstr(at, "roots") != nullptr)
			{
				auto& registry = Locator::entitiesRegistry::value();
				auto& meshes = Locator::resources::value().GetMeshes();
				for (const auto& [id, dx] : {std::pair {MeshId::TreeRoots, -6.0f}, std::pair {MeshId::TreeRootsPile, 6.0f},
				                             std::pair {MeshId::TreeBeech, 12.0f}})
				{
					const auto meshId = resources::HashIdentifier(id);
					if (!meshes.Contains(meshId))
					{
						continue;
					}
					const auto& box = meshes.Handle(meshId)->GetBoundingBox();
					SPDLOG_LOGGER_INFO(spdlog::get("game"),
					                   "Hand test: mesh {} box min ({:.2f},{:.2f},{:.2f}) max ({:.2f},{:.2f},{:.2f})",
					                   static_cast<int>(id), box.minima.x, box.minima.y, box.minima.z, box.maxima.x,
					                   box.maxima.y, box.maxima.z);
					if (dx != 0.0f)
					{
						const auto e = registry.Create();
						const glm::vec3 p(x + dx, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x + dx, z)), z);
						registry.Assign<Transform>(e, p, glm::mat3(1.0f), glm::vec3(1.0f));
						registry.Assign<Mesh>(e, meshId, static_cast<int8_t>(0), static_cast<int8_t>(-1));
						if (std::strstr(at, "alpha") != nullptr)
						{
							registry.Assign<Alpha>(e, std::getenv("OPENBLACK_TEST_ALPHA")
							                              ? static_cast<float>(std::atof(std::getenv("OPENBLACK_TEST_ALPHA")))
							                              : 0.5f);
						}
					}
				}
				registry.SetDirty();
			}
			// OPENBLACK_HAND_TEST_TREE="x,z,store": an oak put straight into the nearest village store.
			if (std::strstr(at, "store") != nullptr)
			{
				const auto oak = archetypes::TreeArchetype::Create(1, store, TreeInfo::Oak, true, 0.0f, 1.0f, 1.0f);
				if (const auto pit = NearestTestStore(store); pit)
				{
					const auto dump = [&](const char* when) {
						std::string piles;
						for (const auto pile : Locator::entitiesRegistry::value().Get<StoragePit>(*pit).woodPiles)
						{
							piles += fmt::format(" {}", Locator::entitiesRegistry::value().Get<Pot>(pile).amount);
						}
						SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: store wood piles {}:{} abode wood {}", when, piles,
						                   Locator::entitiesRegistry::value().Get<Abode>(*pit).woodAmount);
					};
					dump("before");
					DepositInStore(oak, *pit, InterfaceStatus());
					dump("after");
				}
			}
		}
	}
	// Debug: OPENBLACK_TEST_TREE_GROWTH="x,z" plants two beech saplings (size 0.1, max 1.2) there, one in a forest and
	// one without (the original only grows the trees of a forest). With OPENBLACK_TREE_TRACE=1 every step is logged.
	if (const char* at = std::getenv("OPENBLACK_TEST_TREE_GROWTH"); at != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		if (std::sscanf(at, "%f,%f", &x, &z) == 2)
		{
			auto& registry = Locator::entitiesRegistry::value();
			const auto testForest = ecs::CreateForest(0, glm::vec3(x, 0.0f, z));
			for (const auto& [dx, forest] : {std::pair {0.0f, testForest}, std::pair {10.0f, 0u}})
			{
				const glm::vec2 point(x + dx, z);
				const glm::vec3 position(point.x, Locator::terrainSystem::value().GetHeightAt(point), point.y);
				const auto tree = archetypes::TreeArchetype::Create(forest, position, TreeInfo::Beech, true, 0.0f, 1.2f, 0.1f);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree test: sapling {} at ({:.1f}, {:.1f}) forest {} growing {}",
				                   static_cast<uint32_t>(tree), point.x, point.y, forest,
				                   registry.Get<const Tree>(tree).growing);
			}
			registry.SetDirty();
		}
	}
	// Debug: OPENBLACK_TEST_FELL="x,z": a beech at x,z felled by a stand-in forester 3 m west of it (FellTree), then 100
	// wood taken off another dead beech 8 m north (RemoveWood: it shrinks), logged.
	if (const char* at = std::getenv("OPENBLACK_TEST_FELL"); at != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		if (std::sscanf(at, "%f,%f", &x, &z) == 2)
		{
			auto& registry = Locator::entitiesRegistry::value();
			const auto& land = Locator::terrainSystem::value();
			const glm::vec3 position(x, land.GetHeightAt(glm::vec2(x, z)), z);
			// a yaw other than 0: the spin is in the tree's body space
			const float yaw = std::getenv("OPENBLACK_TEST_FELL_YAW") != nullptr
			                      ? static_cast<float>(std::atof(std::getenv("OPENBLACK_TEST_FELL_YAW")))
			                      : 0.0f;
			const auto tree = archetypes::TreeArchetype::Create(0, position, TreeInfo::Beech, true, yaw, 1.0f, 1.0f);
			const auto forester = registry.Create();
			registry.Assign<Transform>(forester, position - glm::vec3(3.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
			const auto log = ecs::FellTree(tree, forester);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree test: felled {} -> {} (in physics {}, carried type {}, wood {})",
			                   static_cast<uint32_t>(tree), static_cast<uint32_t>(log),
			                   physics::PhysicsObjects::Find(log) != nullptr, static_cast<int>(ecs::TreeCarriedType(log)),
			                   ecs::TreeWood(log));
			const glm::vec3 p2(x, land.GetHeightAt(glm::vec2(x, z + 8.0f)), z + 8.0f);
			const auto dead = archetypes::TreeArchetype::Create(0, p2, TreeInfo::Beech, true, 0.0f, 1.0f, 1.0f);
			MakeDeadTree(dead, glm::vec3(1.0f, 0.0f, 0.0f));
			const auto before = ecs::TreeWood(dead);
			const auto taken = ecs::RemoveWood(dead, 100);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree test: dead tree wood {} -> took {}, left {} (scale {:.3f})", before,
			                   taken, registry.Valid(dead) ? ecs::TreeWood(dead) : 0u,
			                   registry.Valid(dead) ? registry.Get<const Transform>(dead).scale.x : 0.0f);
			registry.SetDirty();
		}
	}
	// Debug: OPENBLACK_TEST_TREE_QUERIES="x,z": a stand-in villager at x,z; logs the tree FindTreeNearVillager picks and
	// its working position, then the town feature assignment and for every town the forest
	// FindNearestForestToPos gives, and the forest list with wood and BigForest; then takes 100 wood from the
	// first BigForest (BigForestRemoveWood) and logs its arrive position.
	if (const char* at = std::getenv("OPENBLACK_TEST_TREE_QUERIES"); at != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		if (std::sscanf(at, "%f,%f", &x, &z) == 2)
		{
			auto& registry = Locator::entitiesRegistry::value();
			const glm::vec3 position(x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)), z);
			const auto villager = registry.Create();
			registry.Assign<Transform>(villager, position, glm::mat3(1.0f), glm::vec3(1.0f));
			const auto tree = ecs::FindTreeNearVillager(villager);
			if (tree != entt::null)
			{
				const auto& t = registry.Get<const Transform>(tree).position;
				const auto w = ecs::TreeWorkingPos(tree, villager);
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Tree test: nearest tree {} at ({:.1f}, {:.1f}), working pos ({:.2f}, {:.2f})",
				                   static_cast<uint32_t>(tree), t.x, t.z, w.x, w.z);
			}
			else
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree test: no tree in the 9 cells");
			}
			ecs::town_features::AssignTownFeatures();
			registry.Each<const Town>([&](entt::entity, const Town& town) {
				glm::vec3 centre = position;
				if (town.centre != entt::null && registry.Valid(town.centre) && registry.AllOf<Transform>(town.centre))
				{
					centre = registry.Get<const Transform>(town.centre).position;
				}
				const auto nearest = ecs::FindNearestForestToPos(town.id, centre);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree test: town {} has {} forests, nearest {} (scenic {})", town.id,
				                   ecs::TownForests(town.id).size(), nearest.value_or(0),
				                   nearest && ecs::IsScenicForest(*nearest));
			});
			for (const auto id : ecs::ForestsNewestFirst())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree test: forest {} trees {} wood {:.0f} big forest {} scenic {}", id,
				                   ecs::ForestTreeCount(id), ecs::ForestWood(id),
				                   static_cast<uint32_t>(ecs::ForestBigForest(id)), ecs::IsScenicForest(id));
			}
			entt::entity firstBig = entt::null;
			registry.Each<const BigForest>([&](entt::entity e, const BigForest&) {
				if (firstBig == entt::null)
				{
					firstBig = e;
				}
			});
			if (firstBig != entt::null)
			{
				const auto arrive = ecs::BigForestArrivePos(firstBig, villager);
				const float before = registry.Get<const BigForest>(firstBig).wood;
				const auto taken = ecs::BigForestRemoveWood(firstBig, 100);
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Tree test: big forest {} arrive ({:.1f}, {:.1f}), wood {:.0f} -> took {} -> {:.0f}",
				                   static_cast<uint32_t>(firstBig), arrive.x, arrive.z, before, taken,
				                   registry.Valid(firstBig) ? registry.Get<const BigForest>(firstBig).wood : 0.0f);
			}
			registry.Destroy(villager);
			registry.SetDirty();
		}
	}
	// Debug: OPENBLACK_TEST_REPLANT="x,z,tilt" drops a beech there tilted by `tilt` degrees about x, straight through
	// InitialisePhysicsFromHand (a gentle release, dont_replant 0): upright on land it is replanted at once (LANDED,
	// the tree's end of physics), leaning (DecomposeYXZ |x| or |z| > 0.2) it stays in physics and falls (DeadTree).
	if (const char* at = std::getenv("OPENBLACK_TEST_REPLANT"); at != nullptr)
	{
		float x = 0.0f;
		float z = 0.0f;
		float tilt = 0.0f;
		if (std::sscanf(at, "%f,%f,%f", &x, &z, &tilt) >= 2)
		{
			auto& registry = Locator::entitiesRegistry::value();
			const glm::vec2 point(x, z);
			const glm::vec3 position(x, Locator::terrainSystem::value().GetHeightAt(point), z);
			const auto tree = archetypes::TreeArchetype::Create(0, position, TreeInfo::Beech, true, 0.0f, 1.0f, 1.0f);
			registry.Get<Transform>(tree).rotation =
			    glm::mat3(glm::eulerAngleX(glm::radians(tilt))) * registry.Get<const Transform>(tree).rotation;
			registry.SetDirty();
			if (!physics::from_hand::InitialisePhysicsFromHand(tree, glm::vec3(0.0f), false))
			{
				physics::from_hand::PlaceWithoutBody(tree);
			}
			const bool alive = registry.Valid(tree);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree test: dropped tilted {:.0f} deg at ({:.1f}, {:.1f}) -> {}", tilt, x,
			                   z,
			                   !alive                           ? "gone"
			                   : registry.AllOf<DeadTree>(tree) ? "dead tree"
			                   : physics::PhysicsObjects::Find(tree)
			                       ? "falling (physics)"
			                       : "replanted, forest " + std::to_string(registry.Get<const Tree>(tree).forestId));
		}
	}
}

void HandSystem::UpdateTestAbode(float seconds) noexcept
{
	// OPENBLACK_TEST_TUG_MOUSE2="x,y": 0.5 s into the held action the fixed test cursor moves there
	if (_testMouseMoveIn >= 0.0f)
	{
		_testMouseMoveIn -= seconds;
		if (_testMouseMoveIn < 0.0f)
		{
			input::OverrideMouseAt(std::getenv("OPENBLACK_TEST_TUG_MOUSE2"));
		}
	}
	if (_testDropAt)
	{
		_testDropIn -= seconds;
		auto& registry = Locator::entitiesRegistry::value();
		if (_testDropIn <= 0.0f && _held && registry.Valid(*_held))
		{
			auto& transform = registry.Get<Transform>(*_held);
			transform.position = glm::vec3(_testDropAt->x, transform.position.y, _testDropAt->z);
			const auto entity = *_held;
			Drop();
			if (registry.Valid(entity))
			{
				const auto& after = registry.Get<const Transform>(entity);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: put down {} at ({:.1f}, {:.1f}, {:.1f}), in physics {}",
				                   static_cast<uint32_t>(entity), after.position.x, after.position.y, after.position.z,
				                   physics::PhysicsObjects::Find(entity) != nullptr);
			}
			else
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: put down {}: gone (a pot put down)",
				                   static_cast<uint32_t>(entity));
			}
			_testDropAt.reset();
		}
		else if (_testDropIn <= 0.0f)
		{
			_testDropAt.reset();
		}
	}
	// Debug: OPENBLACK_TEST_KNOCK="[count[,index[,delay[,interval]]]]" taps the index-th living-quarters abode `count`
	// times, `interval` seconds apart (0.4 by default) once `delay` seconds have passed (2 by default, so that
	// OPENBLACK_CAMERA_FLY has arrived), through the hand's tap (SendTap, as a click): the abode's tap knocks and
	// the hand plays Ctap_house. With OPENBLACK_SFX_TRACE=1 the log must show G_KnockRoofMulti 110..118 and 110 again
	// (the roof-knock sound check).
	if (const char* knock = std::getenv("OPENBLACK_TEST_KNOCK"); knock != nullptr)
	{
		auto& s_Left = HandDebugHooksData().updateTestAbodeLeft;
		auto& s_Wanted = HandDebugHooksData().updateTestAbodeWanted;
		auto& s_Delay = HandDebugHooksData().updateTestAbodeDelay;
		auto& s_Interval = HandDebugHooksData().updateTestAbodeInterval;
		auto& s_Next = HandDebugHooksData().updateTestAbodeNext;
		if (s_Left < 0)
		{
			s_Left = 10;
			std::sscanf(knock, "%d,%d,%f,%f", &s_Left, &s_Wanted, &s_Delay, &s_Interval);
			s_Next = s_Delay;
		}
		if (s_Left > 0)
		{
			s_Next -= seconds;
			if (s_Next <= 0.0f)
			{
				s_Next = s_Interval;
				auto& registry = Locator::entitiesRegistry::value();
				std::optional<entt::entity> target;
				int index = 0;
				registry.Each<const Abode, const Transform>([&](entt::entity e, const Abode&, const Transform&) {
					const auto type = abodes::TypeOf(e);
					if (target || !type.has_value() ||
					    (static_cast<uint32_t>(*type) & static_cast<uint32_t>(AbodeType::LivingQuarters)) == 0)
					{
						return;
					}
					if (index++ == s_Wanted)
					{
						target = e;
					}
				});
				if (target)
				{
					const auto point = registry.Get<const Transform>(*target).position;
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "Knock test: abode {} tapped at ({:.1f}, {:.1f}, {:.1f}), {} left",
					                   static_cast<uint32_t>(*target), point.x, point.y, point.z, s_Left);
					SendTap(*target);
				}
				--s_Left;
			}
		}
	}
	// Debug: OPENBLACK_TEST_PICK_VILLAGER="[index[,delay]]" puts the index-th villager in the hand `delay` seconds in
	// (2 by default, so that OPENBLACK_TEST_VIEW_VILLAGER has arrived): the generic pick-up. The trace must show two
	// samples, G_PickUpObject 10 and the scream of its kind (180 / 187 / 194 + rand 7).
	if (const char* pick = std::getenv("OPENBLACK_TEST_PICK_VILLAGER"); pick != nullptr)
	{
		auto& s_Wanted = HandDebugHooksData().updateTestAbodeWanted2;
		auto& s_Delay = HandDebugHooksData().updateTestAbodeDelay2;
		auto& s_Done = HandDebugHooksData().updateTestAbodeDone;
		if (s_Wanted < 0)
		{
			s_Wanted = 0;
			std::sscanf(pick, "%d,%f", &s_Wanted, &s_Delay);
		}
		if (!s_Done)
		{
			s_Delay -= seconds;
			if (s_Delay <= 0.0f)
			{
				s_Done = true;
				auto& registry = Locator::entitiesRegistry::value();
				std::optional<entt::entity> target;
				int index = 0;
				registry.Each<const Villager, const Transform>([&](entt::entity e, const Villager&, const Transform&) {
					if (!target && index++ == s_Wanted)
					{
						target = e;
					}
				});
				if (target)
				{
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "Pick test: villager {} picked up", static_cast<uint32_t>(*target));
					PickUp(*target);
				}
			}
		}
	}
	// Debug (an openblack test hook, off unless the variable is set): OPENBLACK_TEST_TAP_SCROLL="[index[,delay]]" taps the
	// index-th «Did you know?» / challenge scroll (a
	// ScriptHighlight) `delay` seconds in (2 by default) through the hand's tap path: SendTap -> the tap packet -> the
	// scroll's tap (the bubble opens on the help system's next turn)
	if (const char* tap = std::getenv("OPENBLACK_TEST_TAP_SCROLL"); tap != nullptr)
	{
		auto& s_Wanted = HandDebugHooksData().updateTestAbodeWanted3;
		auto& s_Delay = HandDebugHooksData().updateTestAbodeDelay3;
		auto& s_Done = HandDebugHooksData().updateTestAbodeDone2;
		if (s_Wanted < 0)
		{
			s_Wanted = 0;
			std::sscanf(tap, "%d,%f", &s_Wanted, &s_Delay);
		}
		if (!s_Done)
		{
			s_Delay -= seconds;
			if (s_Delay <= 0.0f)
			{
				s_Done = true;
				auto& registry = Locator::entitiesRegistry::value();
				std::optional<entt::entity> target;
				int index = 0;
				registry.Each<const ScriptHighlight>([&](entt::entity e, const ScriptHighlight&) {
					if (!target && index++ == s_Wanted)
					{
						target = e;
					}
				});
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tap test: scroll {} of {}", target ? static_cast<int>(*target) : -1,
				                   index);
				if (target)
				{
					SendTap(*target);
				}
			}
		}
	}
	if (_testActionHold > 0.0f)
	{
		_testActionDelay -= seconds;
		if (_testActionDelay <= 0.0f)
		{
			_testActionSeconds = _testActionHold;
			_testActionHold = 0.0f;
			_testMouseMoveIn = std::getenv("OPENBLACK_TEST_TUG_MOUSE2") != nullptr ? 0.5f : -1.0f;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand test: action held, hovered {}", _hovered.has_value());
		}
	}
	if (!_testAbode || _testAbode->count <= 0)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(_testAbode->abode))
	{
		_testAbode.reset();
		return;
	}
	_testAbode->timer -= seconds;
	if (_testAbode->timer > 0.0f)
	{
		return;
	}
	_testAbode->timer = 1.5f;
	--_testAbode->count;
	const auto at = registry.Get<const Transform>(_testAbode->abode).position;
	const glm::vec2 from(at.x - 15.0f, at.z + 2.0f * static_cast<float>(_testAbode->count % 3 - 1));
	const glm::vec3 start(from.x, Locator::terrainSystem::value().GetHeightAt(from) + 3.0f, from.y);
	const auto rock = archetypes::MobileStaticArchetype::Create(start, MobileStaticInfo::Boulder1Lime, 0.0f, 0.0f, 0.0f, 0.0f,
	                                                            _testAbode->scale);
	const auto direction = glm::normalize(at + glm::vec3(0.0f, 3.0f, 0.0f) - start);
	physics::PhysicsObjects::AddObject(rock, direction * _testAbode->speed, glm::vec3(0.0f), entt::null, true);
	registry.SetDirty();
}

// Debug: OPENBLACK_DUMP_ENTITY_COUNTS=<hand updates> logs, once after that many hand updates, how many entities there are
// of each kind (map loading audit).
namespace
{
/// The GClimates of ECS/Weather/Climate (the world's one too)
size_t ClimateCount()
{
	size_t count = 0;
	openblack::weather::climate::ForEach([&count](const openblack::weather::climate::Climate&) { ++count; });
	return count;
}
} // namespace

void openblack::ecs::systems::hand_detail::DumpEntityCounts()
{
	static const char* env = std::getenv("OPENBLACK_DUMP_ENTITY_COUNTS");
	auto& updates = HandDebugHooksData().dumpEntityCountsUpdates;
	if (env == nullptr || updates < 0 || ++updates < std::atoi(env))
	{
		return;
	}
	updates = -1;
	auto& registry = Locator::entitiesRegistry::value();
	auto log = spdlog::get("game");
	SPDLOG_LOGGER_INFO(log,
	                   "Entity counts: Tree {} DeadTree {} MobileStatic {} MobileObject {} Feature {} AnimatedStatic {} "
	                   "Animal {} Flock {} Villager {} Creature {} Abode {} Town {} Field {} FishFarm {} Forest {} "
	                   "BigForest {} Pot {} Mist {} StreetLantern {} LanternLight {} Arena {} Climate {} DrinkWaypoint {} "
	                   "Stream {} Temple {} TotemStatue {} Mesh {}",
	                   registry.Size<Tree>(), registry.Size<DeadTree>(), registry.Size<MobileStatic>(),
	                   registry.Size<MobileObject>(), registry.Size<Feature>(), registry.Size<AnimatedStatic>(),
	                   registry.Size<Animal>(), registry.Size<Flock>(), registry.Size<Villager>(), registry.Size<Creature>(),
	                   registry.Size<Abode>(), registry.Size<Town>(), registry.Size<Field>(), registry.Size<FishFarm>(),
	                   registry.Size<Forest>(), registry.Size<BigForest>(), registry.Size<Pot>(), registry.Size<Mist>(),
	                   registry.Size<StreetLantern>(), registry.Size<LanternLight>(), registry.Size<Arena>(), ClimateCount(),
	                   registry.Size<DrinkWaypoint>(), registry.Size<Stream>(), registry.Size<Temple>(),
	                   registry.Size<TotemStatue>(), registry.Size<Mesh>());
	int bare = 0;
	registry.Each<const Transform>([&](entt::entity, const Transform&) { ++bare; }, entt::exclude<Mesh>);
	size_t planned = 0;
	registry.Each<const Town>([&](entt::entity, const Town& t) { planned += t.plannedAbodes.size(); });
	SPDLOG_LOGGER_INFO(log, "Entity counts: Transform without Mesh {} planned abodes {}", bare, planned);
	std::map<int, int> animals;
	registry.Each<const Animal>([&](entt::entity, const Animal& a) { ++animals[static_cast<int>(a.type)]; });
	for (const auto& [type, n] : animals)
	{
		SPDLOG_LOGGER_INFO(log, "Entity counts: Animal type {} x{}", type, n);
	}
	std::map<int, int> statics;
	registry.Each<const MobileStatic>([&](entt::entity, const MobileStatic& m) { ++statics[static_cast<int>(m.type)]; });
	for (const auto& [type, n] : statics)
	{
		SPDLOG_LOGGER_INFO(log, "Entity counts: MobileStatic type {} x{}", type, n);
	}
}
