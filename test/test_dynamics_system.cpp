/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The physics as a system, in a world of its own: things thrown onto a flat piece of land fly, come to rest, go back
// into the map's cells and leave the physics, and a body at rest wakes again only while something moving comes near.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/DynamicsWorld.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "InfoConstants.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Land as high everywhere, all of it dry
class FlatIsland final: public LandIslandInterface
{
public:
	explicit FlatIsland(float height)
	    : _height(height)
	{
		_cell.altitude = static_cast<uint8_t>(height / LandIslandInterface::k_HeightUnit);
	}
	[[nodiscard]] float GetHeightAt(glm::vec2) const override { return _height; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const override { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const override { return _cell; }
	[[nodiscard]] const lnd::LNDCell* FindCell(const glm::u16vec2&) const override { return &_cell; }
	void DumpTextures() const override {}
	void DumpMaps() const override {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() override { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const override { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const override { throw std::logic_error("no countries"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::Texture2D& GetLuminosityMap() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::Texture2D& GetCellColourMap() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::Texture2D& GetBlockTextures() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::FrameBuffer& GetFootprintFramebuffer() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::FrameBuffer& GetLandAlphaFramebuffer() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const override { return {}; }
	[[nodiscard]] glm::mat4 GetOrthoView() const override { return glm::mat4(1.0f); }
	[[nodiscard]] glm::mat4 GetOrthoProj() const override { return glm::mat4(1.0f); }
	[[nodiscard]] Extent2 GetExtent() const override { return {}; }
	uint8_t GetNoise(glm::u8vec2) override { return 0; }

private:
	float _height;
	lnd::LNDCell _cell {};
};

constexpr physics::Material k_Stone {
    .density = 2.0f, .springK = 20.0f, .dampK = 1.0f, .friction = 1.0f, .spinKeptPerSecond = 0.7f, .drag = 0.0f};

/// A world of a flat island, or of open sea at the sea's level, a map of cells kept by hand, and every model a cube a
/// metre across
class FakeWorld final: public dynamics::World
{
public:
	explicit FakeWorld(float landHeight)
	    : _info(std::make_unique<InfoConstants>())
	    , _land(landHeight)
	    , _landHeight(landHeight)
	{
		auto& rock = _info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::RockChalk));
		rock.mobileType = MobileStaticInfo::Rock;
		rock.meshId = MeshId::Dummy;
		rock.weight = 500.0f;
		for (int i = 0; i < 8; ++i)
		{
			const float x = ((i & 1) ^ ((i >> 1) & 1)) != 0 ? 0.5f : -0.5f;
			const float y = (i & 2) != 0 ? 1.0f : 0.0f;
			const float z = (i & 4) != 0 ? 0.5f : -0.5f;
			_cube.emplace_back(x, y, z);
		}
		_cubeFaces = {0, 3, 2, 0, 2, 1, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
		              3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};
	}

	static constexpr float k_LandHeight = 20.0f;
	static constexpr float k_SeaLevel = 0.0f;
	[[nodiscard]] float LandHeight() const { return _landHeight; }

	[[nodiscard]] Registry& Entities() override { return _registry; }
	[[nodiscard]] const Registry& Entities() const override { return _registry; }
	[[nodiscard]] const LandIslandInterface* Land() const override { return &_land; }
	[[nodiscard]] const InfoConstants* Info() const override { return _info.get(); }
	[[nodiscard]] const GObjectInfo* InfoOf(entt::entity object) const override
	{
		const auto* still = _registry.TryGet<const MobileStatic>(object);
		return still != nullptr ? &_info->mobileStatic.at(static_cast<size_t>(still->type)) : nullptr;
	}
	[[nodiscard]] float LifeOf(entt::entity) const override { return 1.0f; }
	[[nodiscard]] float HeightOf(entt::entity) const override { return 1.0f; }
	void Remove(entt::entity object) override { _registry.Destroy(object); }
	[[nodiscard]] std::optional<dynamics::ModelSize> SizeOfModel(entt::id_type) const override
	{
		return dynamics::ModelSize {.size = {1.0f, 1.0f, 1.0f}};
	}
	[[nodiscard]] std::optional<dynamics::Model> ModelOf(entt::id_type) const override
	{
		dynamics::Model model;
		model.size = {1.0f, 1.0f, 1.0f};
		model.indices.push_back(_cubeFaces);
		model.parts.push_back({.positions = _cube, .indices = model.indices.back(), .isPhysics = true});
		model.bones.emplace_back();
		return model;
	}
	[[nodiscard]] physics::Material MaterialOf(physics::MaterialRow) const override { return k_Stone; }

	[[nodiscard]] bool HasMap() const override { return true; }
	[[nodiscard]] std::vector<entt::entity> FixedThenMobileInCell(glm::ivec2 cell) const override { return AllInCell(cell); }
	[[nodiscard]] std::vector<entt::entity> AllInCell(glm::ivec2 cell) const override
	{
		std::vector<entt::entity> found;
		for (const auto& [object, at] : filed)
		{
			if (at == cell && _registry.Valid(object))
			{
				found.push_back(object);
			}
		}
		return found;
	}
	void Refile(entt::entity object) override
	{
		const auto& position = _registry.Get<const Transform>(object).position;
		filed[object] = CellOf(position);
		++refiled;
	}
	/// The map cell a point is in
	[[nodiscard]] static glm::ivec2 CellOf(glm::vec3 position)
	{
		return {static_cast<int32_t>(position.x / 10.0f), static_cast<int32_t>(position.z / 10.0f)};
	}

	[[nodiscard]] std::optional<glm::vec3> CameraOrigin() const override { return std::nullopt; }
	[[nodiscard]] GameRandomInterface* Random() override { return nullptr; }
	void PlaySound(audio::SoundId) override {}
	std::optional<audio::AnimEffectPlay> PlayCollisionSound(std::span<const int32_t>, entt::entity, glm::vec3) override
	{
		return std::nullopt;
	}
	void MoveSound(entt::entity, glm::vec3) override {}
	void AddWaterRing(const water_rings::Ring&) override {}
	void ScareFish(glm::vec3) override {}
	[[nodiscard]] int32_t SnowAt(glm::vec3) const override { return 0; }
	void StartedMoving(entt::entity) override {}
	[[nodiscard]] bool IsOnFire(entt::entity) const override { return false; }
	void CreateReaction(const ReactionSystemInterface::Source&) override {}
	void RemoveReactions(entt::entity, Reaction) override {}
	void UntieLeashesTiedTo(entt::entity) override {}
	[[nodiscard]] bool IsComputerPlayer(PlayerNames) const override { return false; }
	void FitDeadTreeObstacle(entt::entity) override {}

	/// Where each object was last put into the map's cells
	std::map<entt::entity, glm::ivec2> filed;
	int refiled {0};

private:
	Registry _registry;
	std::unique_ptr<InfoConstants> _info;
	FlatIsland _land;
	float _landHeight;
	std::vector<glm::vec3> _cube;
	std::vector<uint32_t> _cubeFaces;
};

/// The kinds' own parts as for ordinary objects, without the game's systems
class PlainHooks final: public PhysicsClassHooks
{
public:
	[[nodiscard]] SoundCollisionType CollideSoundType(entt::entity) const override { return SoundCollisionType::Default; }
	void ForgetBuildingHitter(entt::entity) override {}
	[[nodiscard]] bool RaisesObjects(entt::entity) const override { return true; }
};

class DynamicsSystemTest: public ::testing::Test
{
protected:
	void SetUp() override { MakeWorld(FakeWorld::k_LandHeight); }

	/// The world the system works in: dry land at that height, or the open sea at the sea's level
	void MakeWorld(float landHeight)
	{
		auto world = std::make_unique<FakeWorld>(landHeight);
		_world = world.get();
		_dynamics = std::make_unique<DynamicsSystem>(std::move(world));
		_dynamics->SetClassHooks(std::make_unique<PlainHooks>());
	}

	/// A rock standing on the land, filed in the map's cells
	entt::entity Rock(glm::vec2 xz)
	{
		auto& registry = _world->Entities();
		const auto rock = registry.Create();
		registry.Assign<Transform>(rock, glm::vec3(xz.x, _world->LandHeight(), xz.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<MobileStatic>(rock, MobileStaticInfo::RockChalk);
		registry.Assign<Mesh>(rock, resources::HashIdentifier(MeshId::Dummy), int8_t {0}, int8_t {0});
		_world->filed[rock] = FakeWorld::CellOf(registry.Get<const Transform>(rock).position);
		return rock;
	}

	[[nodiscard]] size_t Entries() const
	{
		size_t count = 0;
		_dynamics->ForEachEntry([&count](const PhysicsEntry&) { ++count; });
		return count;
	}

	/// Runs game turns until nothing is left in the physics; the turns it took, or none
	std::optional<int> RunUntilEmpty(int mostTurns)
	{
		for (int turn = 1; turn <= mostTurns; ++turn)
		{
			_dynamics->ProcessTurn();
			if (Entries() == 0)
			{
				return turn;
			}
		}
		return std::nullopt;
	}

	FakeWorld* _world {nullptr};
	std::unique_ptr<DynamicsSystem> _dynamics;
};
} // namespace

TEST_F(DynamicsSystemTest, AThrownRockComesToRestGoesBackIntoTheMapAndLeavesThePhysics)
{
	const auto rock = Rock({1000.0f, 1000.0f});
	auto& transform = _world->Entities().Get<Transform>(rock);
	transform.position.y += 3.0f;
	const auto started = _dynamics->InitialisePhysics(rock, {.velocity = {4.0f, 2.0f, 0.0f}});
	ASSERT_TRUE(started.started);
	ASSERT_NE(started.entry, nullptr);
	EXPECT_TRUE(_world->Entities().AllOf<InPhysics>(rock));
	EXPECT_TRUE(_dynamics->IsFlying(rock));

	const auto turns = RunUntilEmpty(600);
	ASSERT_TRUE(turns.has_value()) << "it never left the physics";
	// It came to rest on the land, out of the physics and back in the map where it lies
	EXPECT_FALSE(_world->Entities().AllOf<InPhysics>(rock));
	EXPECT_FALSE(_world->Entities().AllOf<PhysicsDrawPose>(rock));
	EXPECT_EQ(_world->refiled, 1);
	EXPECT_EQ(_world->filed.at(rock), FakeWorld::CellOf(_world->Entities().Get<const Transform>(rock).position));
	EXPECT_NEAR(_world->Entities().Get<const Transform>(rock).position.y, FakeWorld::k_LandHeight, 0.3f);
	EXPECT_GT(_world->Entities().Get<const Transform>(rock).position.x, 1000.0f);

	// From then on it costs nothing: nothing is added back, and it stays where it lies
	const auto rested = _world->Entities().Get<const Transform>(rock).position;
	for (int turn = 0; turn < 50; ++turn)
	{
		_dynamics->ProcessTurn();
		_dynamics->UpdateFrame(0.5f, 0.0f);
		ASSERT_EQ(Entries(), 0u);
	}
	EXPECT_EQ(_world->Entities().Get<const Transform>(rock).position, rested);
}

TEST_F(DynamicsSystemTest, ARestingRockWakesOnlyWhileSomethingMovingIsNear)
{
	const auto lying = Rock({1000.0f, 1000.0f});
	const auto thrown = Rock({1003.0f, 1000.0f});
	_world->Entities().Get<Transform>(thrown).position.y += 2.0f;
	ASSERT_TRUE(_dynamics->InitialisePhysics(thrown, {.velocity = {0.5f, 0.0f, 0.0f}}).started);
	_dynamics->ProcessTurn();
	// The rock lying near the moving one is in the physics as a resting obstacle, not moving
	const auto* proxy = _dynamics->Find(lying);
	ASSERT_NE(proxy, nullptr);
	EXPECT_FALSE(proxy->IsFlying());
	EXPECT_FALSE(_world->Entities().AllOf<InPhysics>(lying));

	ASSERT_TRUE(RunUntilEmpty(600).has_value());
	// Once nothing moves, both are out of the physics and neither was moved by the other's waking
	EXPECT_EQ(_dynamics->Find(lying), nullptr);
	EXPECT_EQ(_dynamics->Find(thrown), nullptr);
	EXPECT_FALSE(_world->Entities().AllOf<InPhysics>(thrown));
}

TEST_F(DynamicsSystemTest, ARockFarFromAnythingMovingIsNeverWoken)
{
	const auto far = Rock({1500.0f, 1500.0f});
	const auto thrown = Rock({1000.0f, 1000.0f});
	ASSERT_TRUE(_dynamics->InitialisePhysics(thrown, {.velocity = {0.0f, 5.0f, 0.0f}}).started);
	for (int turn = 0; turn < 20; ++turn)
	{
		_dynamics->ProcessTurn();
		ASSERT_EQ(_dynamics->Find(far), nullptr);
	}
}

TEST_F(DynamicsSystemTest, ARockSinkingInTheSeaIsDrawnUntilWhollyUnderAndDeletedDeepBelow)
{
	MakeWorld(FakeWorld::k_SeaLevel);
	const auto rock = Rock({1000.0f, 1000.0f});
	_world->Entities().Get<Transform>(rock).position.y += 3.0f;
	ASSERT_TRUE(_dynamics->InitialisePhysics(rock, {.velocity = {0.0f, -1.0f, 0.0f}}).started);

	bool sawUnderSea = false;
	for (int turn = 0; turn < 600 && _world->Entities().Valid(rock); ++turn)
	{
		_dynamics->ProcessTurn();
		_dynamics->UpdateFrame(0.5f, 0.0f);
		std::optional<glm::vec3> centre;
		float radius = 0.0f;
		float highest = -1e9f;
		_dynamics->ForEachEntry([&](const PhysicsEntry& entry) {
			if (entry.entity == rock)
			{
				centre = entry.body->Centre();
				radius = entry.body->Radius();
				for (const auto& point : entry.body->Points())
				{
					highest = std::max(highest, point.world.y);
				}
			}
		});
		if (!_world->Entities().Valid(rock))
		{
			break;
		}
		ASSERT_TRUE(centre.has_value());
		// Deleted only once its centre is four radii under; until then it is still about
		EXPECT_GE(centre->y, -4.0f * radius - 1.0f);
		const auto* drawn = _world->Entities().TryGet<const PhysicsDrawPose>(rock);
		ASSERT_NE(drawn, nullptr);
		if (centre->y > -radius)
		{
			// Some of it may still be above the surface: it is drawn
			EXPECT_FALSE(drawn->underSea);
		}
		else
		{
			// Every point of it is under the surface: it isn't drawn
			EXPECT_LE(highest, FakeWorld::k_SeaLevel);
			EXPECT_TRUE(drawn->underSea);
			sawUnderSea = true;
		}
	}
	EXPECT_TRUE(sawUnderSea);
	EXPECT_FALSE(_world->Entities().Valid(rock)) << "it never sank deep enough to be deleted";
}
