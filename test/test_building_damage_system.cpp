/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Breaking buildings as a system, in a world of its own: a creature's blow and a rock's break a wall where they strike,
// its pieces fly from the building and go when their time is up, big pieces at rest go back into it as rubble, a rock's
// blow is heard or breaks by its momentum, and the harm is put down to the right player.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "ECS/BuildingDamageWorld.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/BuildingDamageSystem.h"
#include "Physics/Body.h"
#include "Physics/LivingRules.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace damage = openblack::physics::damage;

namespace
{
constexpr entt::id_type k_House = entt::hashed_string("house").value();
constexpr glm::vec3 k_HousePosition {100.0f, 0.0f, 100.0f};

/// The keys of the buildings' sounds, as the system plays them
const std::vector<int32_t> k_Knock {3, 0, 0x16, 0x10, 75};
const std::vector<int32_t> k_HardKnock {2, 0, 0x16, 0x10, 75};
const std::vector<int32_t> k_Crash {1, 0, 0x16, 9, 75};
const std::vector<int32_t> k_Smash {2, 1, 1, 9, 75};

/// A flat wall of squares, each two triangles, from x = 0 to width and y = 0 to height in steps, moved along x
std::vector<std::array<damage::Corner, 3>> Wall(int width, int height, float step, float shift = 0.0f)
{
	std::vector<std::array<damage::Corner, 3>> triangles;
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			const glm::vec3 a(static_cast<float>(x) * step + shift, static_cast<float>(y) * step, 0.0f);
			const glm::vec3 b = a + glm::vec3(step, 0.0f, 0.0f);
			const glm::vec3 c = a + glm::vec3(step, step, 0.0f);
			const glm::vec3 d = a + glm::vec3(0.0f, step, 0.0f);
			triangles.push_back(
			    {damage::Corner {.position = a}, damage::Corner {.position = b}, damage::Corner {.position = c}});
			triangles.push_back(
			    {damage::Corner {.position = a}, damage::Corner {.position = c}, damage::Corner {.position = d}});
		}
	}
	return triangles;
}

/// Draws from the middle of every range, counting them
class MiddleRandom final: public GameRandomInterface
{
public:
	uint32_t GameRand(uint32_t n) override
	{
		++draws;
		return n / 2;
	}
	float GameFloatRand(float x) override
	{
		++draws;
		return x / 2.0f;
	}
	uint32_t LocalRand(int32_t n) override { return static_cast<uint32_t>(n) / 2; }
	float LocalFloatRand(float x) override { return x / 2.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t /*seed*/) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds /*seeds*/) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream /*stream*/) override {}

	int draws {0};
};

/// Records what is started in the physics; things that break buildings are the ones given bodies here
class FakeDynamics final: public DynamicsSystemInterface
{
public:
	struct Launched
	{
		entt::entity object;
		PhysicsStart start;
		PhysicsEntry* entry;
	};

	PhysicsStarted InitialisePhysics(entt::entity object, const PhysicsStart& start) override
	{
		auto& entry = *entries.emplace_back(std::make_unique<PhysicsEntry>());
		entry.entity = object;
		entry.thrower = start.thrower;
		launched.push_back({.object = object, .start = start, .entry = &entry});
		return {.entry = &entry, .started = true};
	}
	PhysicsEntry* Find(entt::entity object) override
	{
		const auto found = std::ranges::find(entries, object, [](const auto& entry) { return entry->entity; });
		return found != entries.end() ? found->get() : nullptr;
	}
	[[nodiscard]] bool PhysicallyDestroysAbodes(entt::entity object) const override
	{
		return std::ranges::find(rocks, object) != rocks.end();
	}
	entt::entity EndPhysicsAsObject(entt::entity object, bool /*insert*/, bool /*hasBody*/) override
	{
		++ended;
		return object;
	}
	void AddPuff(glm::vec3 /*position*/, glm::vec3 /*velocity*/, float /*size*/, uint32_t /*argb*/) override { ++puffs; }

	/// A rock flying with a speed, its body a cube of two metres across weighing ten
	PhysicsEntry& AddRock(entt::entity rock, glm::vec3 centre, glm::vec3 velocity, std::optional<PlayerNames> player)
	{
		rocks.push_back(rock);
		auto& entry = *entries.emplace_back(std::make_unique<PhysicsEntry>());
		entry.entity = rock;
		entry.player = player;
		physics::Shape cube;
		for (int i = 0; i < 8; ++i)
		{
			cube.points.emplace_back((i & 1) != 0 ? 1.0f : -1.0f, (i & 2) != 0 ? 1.0f : -1.0f, (i & 4) != 0 ? 1.0f : -1.0f);
		}
		cube.faces = {{0, 1, 3}};
		cube.radius = glm::length(glm::vec3(1.0f));
		entry.body =
		    std::make_unique<physics::Body>(physics::BodySetup {.scale = 1.0f, .halfHeight = 1.0f, .mass = 10.0f}, cube);
		entry.body->SetUpPose({.axes = glm::mat3(1.0f), .origin = centre});
		entry.body->velocity = velocity;
		return entry;
	}

	std::vector<std::unique_ptr<PhysicsEntry>> entries;
	std::vector<Launched> launched;
	std::vector<entt::entity> rocks;
	int ended {0};
	int puffs {0};
};

/// A flat land at the sea's level, the house's model a wall of 8 by 2 squares of 2 metres about its middle, and records of what
/// is drawn, heard, removed and applied
class FakeWorld final: public building_world::World
{
public:
	struct Effect
	{
		entt::entity object;
		magic::EffectValues values;
		magic::EffectSource source;
	};
	struct Deed
	{
		size_t deed;
		entt::entity object;
		PlayerNames player;
	};

	[[nodiscard]] Registry& Entities() override { return registry; }
	[[nodiscard]] const Registry& Entities() const override { return registry; }
	[[nodiscard]] float LandHeight(glm::vec2 /*point*/) const override { return 0.0f; }
	[[nodiscard]] std::vector<building_world::ModelPart> ModelParts(entt::id_type mesh) const override
	{
		if (mesh != k_House)
		{
			return {};
		}
		return {{.material = 0, .triangles = Wall(8, 2, 2.0f, -8.0f)}};
	}
	[[nodiscard]] entt::id_type MakeModel(const std::string& name, entt::id_type source,
	                                      std::span<const building_world::DrawnPart> parts) override
	{
		EXPECT_EQ(source, k_House);
		made.push_back(name);
		const auto id = entt::hashed_string(name.c_str()).value();
		models[id] = parts.size();
		return id;
	}
	void EraseModel(entt::id_type mesh) override
	{
		if (mesh != 0)
		{
			erased.push_back(mesh);
			models.erase(mesh);
		}
	}
	[[nodiscard]] DynamicsSystemInterface* Dynamics() override { return &dynamics; }
	[[nodiscard]] GameRandomInterface* Random() override { return &random; }
	[[nodiscard]] int32_t SnowAt(glm::vec3 /*point*/) const override { return 0; }
	[[nodiscard]] uint32_t DustTint(uint32_t argb, int32_t /*snow*/) const override { return argb; }
	void PlaySound(std::span<const int32_t> keys, entt::entity building, glm::vec3 /*position*/) override
	{
		EXPECT_EQ(building, house);
		sounds.emplace_back(keys.begin(), keys.end());
	}
	[[nodiscard]] float LifeOf(entt::entity /*object*/) const override { return 1.0f; }
	[[nodiscard]] const GObjectInfo* InfoOf(entt::entity /*object*/) const override { return nullptr; }
	[[nodiscard]] std::optional<PlayerNames> PlayerOf(entt::entity object) const override
	{
		const auto found = players.find(object);
		return found != players.end() ? std::optional(found->second) : std::nullopt;
	}
	void Remove(entt::entity object) override
	{
		removed.push_back(object);
		registry.Destroy(object);
	}
	[[nodiscard]] std::optional<magic::EffectValues> Crush() const override
	{
		magic::EffectValues values;
		values[magic::EffectKind::Crush] = 1.0f;
		return values;
	}
	void ApplyEffect(entt::entity object, const magic::EffectValues& values, const magic::EffectSource& source) override
	{
		effects.push_back({.object = object, .values = values, .source = source});
	}
	void PlayerDid(size_t deed, glm::vec3 /*point*/, entt::entity object, PlayerNames player) override
	{
		deeds.push_back({.deed = deed, .object = object, .player = player});
	}

	/// The house, a wall standing on the land
	entt::entity MakeHouse()
	{
		house = registry.Create();
		registry.Assign<Transform>(house, k_HousePosition, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mesh>(house, k_House, static_cast<int8_t>(0), static_cast<int8_t>(1));
		return house;
	}

	[[nodiscard]] std::vector<entt::entity> Pieces()
	{
		std::vector<entt::entity> pieces;
		registry.Each<const BuildingPiece>([&pieces](entt::entity piece, const BuildingPiece&) { pieces.push_back(piece); });
		return pieces;
	}

	Registry registry;
	FakeDynamics dynamics;
	MiddleRandom random;
	entt::entity house {entt::null};
	std::map<entt::entity, PlayerNames> players;
	std::vector<std::string> made;
	std::map<entt::id_type, size_t> models;
	std::vector<entt::id_type> erased;
	std::vector<std::vector<int32_t>> sounds;
	std::vector<entt::entity> removed;
	std::vector<Effect> effects;
	std::vector<Deed> deeds;
};

struct Fixture
{
	Fixture()
	{
		auto made = std::make_unique<FakeWorld>();
		world = made.get();
		system = std::make_unique<BuildingDamageSystem>(std::move(made));
		house = world->MakeHouse();
	}

	/// A rock striking the house's wall from in front, at a point of it, with a momentum
	ImpactInfo RockStrikes(glm::vec3 point, float momentum, std::optional<PlayerNames> player, uint8_t houseFlags = 0)
	{
		const auto rock = world->registry.Create();
		auto& entry = world->dynamics.AddRock(rock, point + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f), player);
		entry.body->velocity *= momentum / entry.body->Mass();
		houseEntry.entity = house;
		houseEntry.flags = houseFlags;
		const ImpactInfo impact {.hitBy = rock, .player = player};
		system->ReactToImpact(world->dynamics, houseEntry, impact);
		return impact;
	}

	FakeWorld* world {nullptr};
	std::unique_ptr<BuildingDamageSystem> system;
	entt::entity house {entt::null};
	PhysicsEntry houseEntry;
};
} // namespace

TEST(BuildingDamageSystem, ACreaturesBlowBreaksTheBuildingWhereItStandsAndIsPutDownToItsPlayer)
{
	Fixture f;
	const auto creature = f.world->registry.Create();
	f.world->players[creature] = PlayerNames::PLAYER_TWO;
	f.system->Smash(f.house, creature, 1.0f);

	// The smash is heard first, then the building breaking
	ASSERT_EQ(f.world->sounds.size(), 2U);
	EXPECT_EQ(f.world->sounds.front(), k_Smash);
	EXPECT_EQ(f.world->sounds.back(), k_Crash);

	// The wall broke round its corner where the creature struck, and the rest stands drawn in its broken model
	const auto* broken = f.world->registry.TryGet<const BuildingDamage>(f.house);
	ASSERT_NE(broken, nullptr);
	EXPECT_LT(broken->mesh.remaining, 1.0f);
	EXPECT_GT(broken->mesh.remaining, 0.0f);
	EXPECT_EQ(broken->version, 1U);
	EXPECT_EQ(broken->drawMesh, entt::hashed_string(fmt::format("broken/{}/1", entt::to_integral(f.house)).c_str()).value());
	EXPECT_EQ(f.system->DrawnMesh(f.house, k_House), broken->drawMesh);
	EXPECT_TRUE(f.system->DrawsWhole(f.house));

	// What broke off flies down from the building, hitting nothing but the land, drawn with the house's skins
	ASSERT_FALSE(f.world->dynamics.launched.empty());
	for (const auto& launched : f.world->dynamics.launched)
	{
		EXPECT_EQ(launched.start.thrower, f.house);
		EXPECT_TRUE(launched.start.add);
		EXPECT_TRUE(launched.entry->Has(PhysicsEntry::k_NoObjectCollision));
		const auto* piece = f.world->registry.TryGet<const BuildingPiece>(launched.object);
		if (piece != nullptr)
		{
			EXPECT_EQ(piece->parent, f.house);
			EXPECT_EQ(piece->sourceMesh, k_House);
			EXPECT_NE(piece->drawMesh, 0U);
			EXPECT_EQ(piece->turnsLeft, piece->mesh.trianglesAtCreation * damage::k_TurnsPerTriangle);
		}
	}
	// Those struck off fly down with the blow, those that only fell loose with nothing
	EXPECT_TRUE(std::ranges::any_of(f.world->dynamics.launched, [](const auto& launched) {
		return launched.start.velocity == glm::vec3(0.0f, -1.0f, 0.0f);
	}));
	EXPECT_TRUE(std::ranges::all_of(f.world->dynamics.launched, [](const auto& launched) {
		return launched.start.velocity == glm::vec3(0.0f, -1.0f, 0.0f) || launched.start.velocity == glm::vec3(0.0f);
	}));
	EXPECT_GT(f.world->random.draws, 0);

	// The crush comes down to what is left of it, from the creature, as its player's
	ASSERT_EQ(f.world->effects.size(), 1U);
	const auto& effect = f.world->effects.front();
	EXPECT_EQ(effect.object, f.house);
	EXPECT_FLOAT_EQ(effect.values[magic::EffectKind::Crush], damage::BreakageShare(1.0f, broken->mesh.remaining));
	EXPECT_EQ(effect.source.player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(effect.source.casterCreature, creature);
	EXPECT_EQ(effect.source.appliedBy, std::optional(creature));
	EXPECT_FALSE(effect.source.playerless);
}

TEST(BuildingDamageSystem, ARocksBlowIsOnlyHeardBelowWhatBreaksTheBuilding)
{
	Fixture f;
	const glm::vec3 wall = k_HousePosition + glm::vec3(0.0f, 2.0f, 0.0f);
	f.RockStrikes(wall, damage::k_KnockMomentum - 1.0f, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(f.world->sounds.empty());
	f.RockStrikes(wall, damage::k_KnockMomentum + 1.0f, PlayerNames::PLAYER_ONE);
	f.RockStrikes(wall, damage::k_HardKnockMomentum + 1.0f, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(f.world->sounds.size(), 2U);
	EXPECT_EQ(f.world->sounds[0], k_Knock);
	EXPECT_EQ(f.world->sounds[1], k_HardKnock);
	EXPECT_FALSE(f.world->registry.AllOf<BuildingDamage>(f.house));
	EXPECT_TRUE(f.world->effects.empty());
	EXPECT_TRUE(f.world->dynamics.launched.empty());
}

TEST(BuildingDamageSystem, ABreakingRockBreaksAHoleWhereItStrikesAsItsThrowersHarm)
{
	Fixture f;
	const auto impact = f.RockStrikes(k_HousePosition + glm::vec3(0.0f, 2.0f, 0.0f), damage::k_BreakingMomentum + 1.0f,
	                                  PlayerNames::PLAYER_ONE);
	const auto* broken = f.world->registry.TryGet<const BuildingDamage>(f.house);
	ASSERT_NE(broken, nullptr);
	EXPECT_LT(broken->mesh.remaining, 1.0f);
	// A rock's first blow doesn't count it as the building's last hitter
	EXPECT_EQ(broken->lastHitter, entt::entity {entt::null});
	ASSERT_EQ(f.world->sounds.size(), 1U);
	EXPECT_EQ(f.world->sounds.front(), k_Crash);
	ASSERT_FALSE(f.world->dynamics.launched.empty());
	// The pieces fly on with some of the rock's speed
	const auto rockVelocity = f.world->dynamics.Find(impact.hitBy)->body->velocity;
	EXPECT_TRUE(std::ranges::any_of(f.world->dynamics.launched, [rockVelocity](const auto& launched) {
		return launched.start.velocity == rockVelocity * damage::k_PieceSpeedShare;
	}));
	ASSERT_EQ(f.world->effects.size(), 1U);
	const auto& effect = f.world->effects.front();
	EXPECT_EQ(effect.source.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(effect.source.casterCreature, entt::entity {entt::null});
	EXPECT_EQ(effect.source.appliedBy, std::optional(impact.hitBy));
	// The house's own resting body came from no hand, so no creature learns from the throw
	EXPECT_TRUE(f.world->deeds.empty());
}

TEST(BuildingDamageSystem, ACreatureMayLearnFromAThrowAtABuildingThatCameFromAHand)
{
	Fixture f;
	const auto impact = f.RockStrikes(k_HousePosition + glm::vec3(0.0f, 2.0f, 0.0f), damage::k_KnockMomentum + 1.0f,
	                                  PlayerNames::PLAYER_ONE, PhysicsEntry::k_FromHand);
	ASSERT_EQ(f.world->deeds.size(), 1U);
	EXPECT_EQ(f.world->deeds.front().deed, physics::living::k_DeedDamageByThrowingAt);
	EXPECT_EQ(f.world->deeds.front().object, f.house);
	EXPECT_EQ(f.world->deeds.front().player, PlayerNames::PLAYER_ONE);
	static_cast<void>(impact);
}

TEST(BuildingDamageSystem, PiecesLastByTheirTrianglesAndGoWhenTheirTimeIsUp)
{
	Fixture f;
	f.system->Smash(f.house, entt::null, 1.0f);
	const auto pieces = f.world->Pieces();
	ASSERT_FALSE(pieces.empty());
	const auto piece = pieces.front();
	const auto turns = f.world->registry.Get<const BuildingPiece>(piece).turnsLeft;
	const auto model = f.world->registry.Get<const BuildingPiece>(piece).drawMesh;
	ASSERT_GT(turns, 0U);
	for (uint32_t turn = 1; turn < turns; ++turn)
	{
		f.system->ProcessTurn();
	}
	EXPECT_TRUE(f.world->registry.Valid(piece));
	f.system->ProcessTurn();
	EXPECT_FALSE(f.world->registry.Valid(piece));
	EXPECT_NE(std::ranges::find(f.world->erased, model), f.world->erased.end());
}

TEST(BuildingDamageSystem, ABigPieceAtRestGoesBackIntoItsBuildingAsRubbleAndASmallOneStays)
{
	Fixture f;
	f.system->Smash(f.house, entt::null, 1.0f);
	auto& registry = f.world->registry;
	const auto primitivesBefore = registry.Get<const BuildingDamage>(f.house).mesh.primitives.size();
	const auto makeLoose = [&registry, &f](int size) {
		const auto piece = registry.Create();
		registry.Assign<Transform>(piece, k_HousePosition, glm::mat3(1.0f), glm::vec3(1.0f));
		auto& part = registry.Assign<BuildingPiece>(piece);
		damage::Primitive primitive;
		for (const auto& corners : Wall(size, size, 1.0f))
		{
			primitive.triangles.push_back(damage::MakeTriangle(corners));
		}
		part.mesh.primitives.push_back(primitive);
		part.mesh.snowFrozen = true;
		part.parent = f.house;
		part.sourceMesh = k_House;
		return piece;
	};

	// Four metres square is more than the rubble's least area
	const auto big = makeLoose(4);
	EXPECT_EQ(f.system->PieceAtRest(f.world->dynamics, nullptr, big, true), entt::entity {entt::null});
	EXPECT_FALSE(registry.Valid(big));
	EXPECT_EQ(registry.Get<const BuildingDamage>(f.house).mesh.primitives.size(), primitivesBefore + 1);
	EXPECT_EQ(registry.Get<const BuildingDamage>(f.house).version, 2U);

	// One metre square lies where it came to rest, no longer the building's, and takes snow again
	const auto small = makeLoose(1);
	EXPECT_EQ(f.system->PieceAtRest(f.world->dynamics, nullptr, small, true), small);
	ASSERT_TRUE(registry.Valid(small));
	EXPECT_EQ(registry.Get<const BuildingPiece>(small).parent, entt::entity {entt::null});
	EXPECT_FALSE(registry.Get<const BuildingPiece>(small).mesh.snowFrozen);
}

TEST(BuildingDamageSystem, ARockThatGoesIsForgottenAndAResetForgetsEveryBrokenBuilding)
{
	Fixture f;
	f.system->Smash(f.house, entt::null, 1.0f);
	auto& registry = f.world->registry;
	const auto rock = registry.Create();
	registry.Get<BuildingDamage>(f.house).lastHitter = rock;
	f.system->ForgetHitter(rock);
	EXPECT_EQ(registry.Get<const BuildingDamage>(f.house).lastHitter, entt::entity {entt::null});

	const auto model = registry.Get<const BuildingDamage>(f.house).drawMesh;
	f.system->Reset();
	EXPECT_FALSE(registry.AllOf<BuildingDamage>(f.house));
	EXPECT_NE(std::ranges::find(f.world->erased, model), f.world->erased.end());
	EXPECT_EQ(f.system->DrawnMesh(f.house, k_House), k_House);
}
