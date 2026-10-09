/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's kinds of thing in the physics, in a world of their own: shields pay for blows, creatures are hurt and
// angered by what strikes them, a temple's heart passes blows on to its towns and beams at what takes them, resources
// go into stores, an animal dropped in the sea teaches the dropper's creature, and the player's creature learns from
// what the hand threw.

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Creature/CreatureDesires.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PhysicsGameHooks.h"
#include "ECS/PhysicsHooksWorld.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Physics/Body.h"
#include "Physics/LivingRules.h"
#include "Physics/TempleHeart.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace living = openblack::physics::living;
namespace temple_heart = openblack::physics::temple_heart;

namespace
{
/// Things that break buildings are the ones given bodies here
class FakeDynamics final: public DynamicsSystemInterface
{
public:
	PhysicsEntry* Find(entt::entity object) override
	{
		const auto found = std::ranges::find(entries, object, [](const auto& entry) { return entry->entity; });
		return found != entries.end() ? found->get() : nullptr;
	}
	[[nodiscard]] bool PhysicallyDestroysAbodes(entt::entity object) const override
	{
		return std::ranges::find(rocks, object) != rocks.end();
	}
	PhysicsStarted InitialisePhysics(entt::entity object, const PhysicsStart& start) override
	{
		launched.emplace_back(object, start);
		return {};
	}

	/// A rock flying with a velocity, a cube two metres across weighing ten
	PhysicsEntry& AddRock(entt::entity rock, glm::vec3 velocity, std::optional<PlayerNames> player)
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
		entry.body->SetUpPose({.axes = glm::mat3(1.0f), .origin = glm::vec3(5.0f, 1.0f, 5.0f)});
		entry.body->velocity = velocity;
		return entry;
	}

	std::vector<std::unique_ptr<PhysicsEntry>> entries;
	std::vector<entt::entity> rocks;
	std::vector<std::pair<entt::entity, PhysicsStart>> launched;
};

class FakeWorld final: public physics_hooks::World
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
	struct Shield
	{
		entt::entity shield;
		entt::entity hitter;
		float momentum;
		std::optional<PlayerNames> player;
	};

	[[nodiscard]] Registry& Entities() override { return registry; }
	[[nodiscard]] std::optional<float> LandHeight(glm::vec2 /*point*/) const override { return 0.0f; }
	[[nodiscard]] uint32_t Turn() const override { return turn; }
	[[nodiscard]] GameRandomInterface* Random() override { return nullptr; }
	[[nodiscard]] std::optional<PlayerNames> LocalPlayer() const override { return PlayerNames::PLAYER_ONE; }
	[[nodiscard]] float LifeOf(entt::entity /*object*/) const override { return 1.0f; }
	[[nodiscard]] float HeightOf(entt::entity /*object*/) const override { return 10.0f; }
	[[nodiscard]] const GAbodeInfo* AbodeInfoOf(entt::entity /*object*/) const override { return nullptr; }
	[[nodiscard]] bool IsToy(entt::entity object) const override { return std::ranges::find(toys, object) != toys.end(); }
	[[nodiscard]] bool IsRock(entt::entity /*object*/) const override { return false; }
	[[nodiscard]] float WeightOf(entt::entity /*object*/) const override { return 1.0f; }
	void LeaveGhost(entt::entity object) override { ghosts.push_back(object); }
	void Remove(entt::entity object) override { removed.push_back(object); }
	[[nodiscard]] std::optional<magic::EffectValues> Crush() const override
	{
		magic::EffectValues values;
		values[magic::EffectKind::Crush] = 1.0f;
		return values;
	}
	[[nodiscard]] bool HasMagic() const override { return true; }
	void ApplyEffect(entt::entity object, const magic::EffectValues& values, const magic::EffectSource& source) override
	{
		effects.push_back({.object = object, .values = values, .source = source});
	}
	void ShieldImpact(entt::entity shield, entt::entity hitter, float momentum, std::optional<PlayerNames> player) override
	{
		shields.push_back({.shield = shield, .hitter = hitter, .momentum = momentum, .player = player});
	}
	[[nodiscard]] std::optional<VillagerStates> VillagerStateOf(entt::entity /*villager*/, bool /*final*/) const override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<uint32_t> StartHeartBeamSource(glm::vec3 /*position*/, PlayerNames player) override
	{
		beamSourcePlayers.push_back(player);
		return 7U;
	}
	void AddPlasma(uint32_t source, const particles::PlasmaCommand& command) override { plasma.emplace_back(source, command); }
	void PlaySound(entt::id_type sound, glm::vec3 /*position*/, entt::entity /*owner*/) override { sounds.push_back(sound); }
	void PassOnBlowToBuilding(DynamicsSystemInterface& /*dynamics*/, entt::entity building, PhysicsEntry& /*struck*/,
	                          const ImpactInfo& /*impact*/) override
	{
		passedOn.push_back(building);
	}
	void StrikeBuilding(DynamicsSystemInterface& /*dynamics*/, PhysicsEntry& entry, const ImpactInfo& /*impact*/) override
	{
		struckBuildings.push_back(entry.entity);
	}
	[[nodiscard]] std::optional<entt::entity> PieceAtRest(DynamicsSystemInterface& /*dynamics*/, PhysicsEntry* /*entry*/,
	                                                      entt::entity /*piece*/, bool /*insert*/) override
	{
		return std::nullopt;
	}
	[[nodiscard]] ResourceStoreSystemInterface::ObjectResource ResourceOf(entt::entity object) const override
	{
		const auto found = resources.find(object);
		return found != resources.end() ? found->second : ResourceStoreSystemInterface::ObjectResource {};
	}
	[[nodiscard]] bool HasStores() const override { return true; }
	[[nodiscard]] bool IsStore(entt::entity store, ResourceType /*type*/) const override
	{
		return std::ranges::find(stores, store) != stores.end();
	}
	bool TakeObject(entt::entity store, entt::entity object, std::optional<PlayerNames> /*giver*/) override
	{
		taken.emplace_back(store, object);
		return true;
	}
	void AddToPile(entt::entity /*pile*/, ResourceType /*type*/, uint32_t /*amount*/, bool /*poisoned*/) override {}
	[[nodiscard]] bool HasAnimals() const override { return false; }
	void SetDying(entt::entity /*animal*/) override {}
	[[nodiscard]] entt::entity LeaderOf(entt::entity /*flock*/) const override { return entt::null; }
	void SetFlockCentre(entt::entity /*flock*/, glm::vec2 /*centre*/) override {}
	[[nodiscard]] std::optional<glm::vec3> ForestLair(AnimalInfo /*kind*/, glm::vec3 /*from*/) const override
	{
		return std::nullopt;
	}
	[[nodiscard]] bool HasMinds() const override { return true; }
	[[nodiscard]] const creature_mind_tables::Tables* MindTables() const override { return nullptr; }
	void PlayerDid(size_t deed, glm::vec3 /*point*/, entt::entity object, PlayerNames player) override
	{
		deeds.push_back({.deed = deed, .object = object, .player = player});
	}
	void UpdateAttitudeFromFeedback(entt::entity /*creature*/, float feedback) override { feedbacks.push_back(feedback); }
	void ChangeDesireSource(entt::entity /*creature*/, uint32_t source, float amount) override
	{
		desireSources.emplace_back(source, amount);
	}
	bool ForceCatch(entt::entity /*creature*/, entt::entity /*object*/) override { return true; }
	[[nodiscard]] bool HasCreatureBodies() const override { return true; }
	void KickSway(entt::entity creature, glm::vec3 /*force*/, glm::vec3 point) override { sways.emplace_back(creature, point); }
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity /*creature*/, size_t /*animation*/) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<glm::vec3> AnimationTravel(entt::entity /*creature*/, size_t /*animation*/) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<float> CatchMs(entt::entity /*creature*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> HandHeldCreature() const override { return std::nullopt; }
	[[nodiscard]] bool HasCreatureActions() const override { return true; }
	[[nodiscard]] bool CanPickUp(entt::entity /*object*/) const override { return true; }
	void Catch(entt::entity /*creature*/, entt::entity /*object*/) override {}
	void StopCreature(entt::entity /*creature*/) override {}

	Registry registry;
	uint32_t turn {0};
	std::vector<entt::entity> toys;
	std::vector<entt::entity> stores;
	std::map<entt::entity, ResourceStoreSystemInterface::ObjectResource> resources;
	std::vector<entt::entity> ghosts;
	std::vector<entt::entity> removed;
	std::vector<Effect> effects;
	std::vector<Shield> shields;
	std::vector<PlayerNames> beamSourcePlayers;
	std::vector<std::pair<uint32_t, particles::PlasmaCommand>> plasma;
	std::vector<entt::id_type> sounds;
	std::vector<entt::entity> passedOn;
	std::vector<entt::entity> struckBuildings;
	std::vector<std::pair<entt::entity, entt::entity>> taken;
	std::vector<Deed> deeds;
	std::vector<float> feedbacks;
	std::vector<std::pair<uint32_t, float>> desireSources;
	std::vector<std::pair<entt::entity, glm::vec3>> sways;
};

struct Fixture
{
	Fixture()
	{
		auto made = std::make_unique<FakeWorld>();
		world = made.get();
		hooks = std::make_unique<PhysicsGameHooks>(std::move(made));
	}

	/// Something struck by a rock of a momentum, thrown by a player
	ImpactInfo StrikeWithRock(entt::entity object, float momentum, std::optional<PlayerNames> player, float knock = 0.0f)
	{
		const auto rock = world->registry.Create();
		auto& rockEntry = dynamics.AddRock(rock, glm::vec3(0.0f, 0.0f, -1.0f), player);
		rockEntry.body->velocity *= momentum / rockEntry.body->Mass();
		struck.entity = object;
		const ImpactInfo impact {.impact = knock, .hitBy = rock, .player = player};
		hooks->ReactToImpact(dynamics, struck, impact);
		return impact;
	}

	FakeWorld* world {nullptr};
	std::unique_ptr<PhysicsGameHooks> hooks;
	FakeDynamics dynamics;
	PhysicsEntry struck;
};
} // namespace

TEST(PhysicsGameHooks, APhysicalShieldPaysForABlowByItsMomentum)
{
	Fixture f;
	const auto shield = f.world->registry.Create();
	f.world->registry.Assign<MagicShield>(shield, MagicShield {.kind = MagicShield::Kind::Physical});
	const auto impact = f.StrikeWithRock(shield, 1500.0f, PlayerNames::PLAYER_TWO);
	ASSERT_EQ(f.world->shields.size(), 1U);
	EXPECT_EQ(f.world->shields[0].shield, shield);
	EXPECT_EQ(f.world->shields[0].hitter, impact.hitBy);
	EXPECT_NEAR(f.world->shields[0].momentum, 1500.0f, 0.01f);
	EXPECT_EQ(f.world->shields[0].player, std::optional(PlayerNames::PLAYER_TWO));
}

TEST(PhysicsGameHooks, ACreatureStruckIsHurtAndItsOwnPlayersBlowMakesItThinkLessOfThePlayer)
{
	Fixture f;
	const auto creature = f.world->registry.Create();
	auto& body = f.world->registry.Assign<Creature>(creature);
	body.owner = PlayerNames::PLAYER_ONE;
	body.size = 1.0f;
	constexpr float k_Knock = 1.0e6f;
	const auto impact = f.StrikeWithRock(creature, 100.0f, PlayerNames::PLAYER_ONE, k_Knock);
	const auto crush = living::CreatureCrush(k_Knock, living::CreatureMass(1.0f, 0.0f, 0.0f));
	ASSERT_TRUE(crush.has_value());

	// It sways where the rock is, takes the rock's crush as the thrower's, applied by the rock
	ASSERT_EQ(f.world->sways.size(), 1U);
	EXPECT_EQ(f.world->sways[0].second, f.dynamics.Find(impact.hitBy)->body->Centre());
	ASSERT_EQ(f.world->effects.size(), 1U);
	EXPECT_FLOAT_EQ(f.world->effects[0].values[magic::EffectKind::Crush], *crush);
	EXPECT_EQ(f.world->effects[0].source.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(f.world->effects[0].source.appliedBy, std::optional(impact.hitBy));
	EXPECT_TRUE(f.world->effects[0].source.blow);
	EXPECT_EQ(f.world->effects[0].source.point, std::optional(f.dynamics.Find(impact.hitBy)->body->Centre()));
	// Its own player struck it, and every blow angers and frightens it
	EXPECT_EQ(f.world->feedbacks, std::vector<float> {-*crush});
	const std::vector<std::pair<uint32_t, float>> sources {{creature_desires::sources::k_FearFromDamage, *crush},
	                                                       {creature_desires::sources::k_AngerFromDamage, *crush}};
	EXPECT_EQ(f.world->desireSources, sources);
}

TEST(PhysicsGameHooks, AToyNeverHurtsACreatureNorDoesAnythingWhileAScriptControlsTheLocalOne)
{
	Fixture f;
	const auto creature = f.world->registry.Create();
	f.world->registry.Assign<Creature>(creature).owner = PlayerNames::PLAYER_ONE;
	const auto toy = f.world->registry.Create();
	f.world->toys.push_back(toy);
	f.struck.entity = creature;
	f.hooks->ReactToImpact(f.dynamics, f.struck, {.impact = 1.0e6f, .hitBy = toy});
	EXPECT_TRUE(f.world->effects.empty());
	EXPECT_TRUE(f.world->sways.empty());

	f.world->registry.Assign<ScriptControlled>(creature);
	f.StrikeWithRock(creature, 100.0f, PlayerNames::PLAYER_TWO, 1.0e6f);
	// Swayed, but not hurt
	EXPECT_EQ(f.world->sways.size(), 1U);
	EXPECT_TRUE(f.world->effects.empty());
	EXPECT_TRUE(f.world->desireSources.empty());
}

TEST(PhysicsGameHooks, ATemplesHeartPassesABlowOnToItsTownsBuildingAndBeamsAtItInTurn)
{
	Fixture f;
	auto& registry = f.world->registry;
	const auto heart = registry.Create();
	registry.Assign<Transform>(heart, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Temple>(heart, PlayerNames::PLAYER_TWO);
	const auto building = registry.Create();
	registry.Assign<Transform>(building, glm::vec3(50.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto town = registry.Create();
	registry.Assign<Town>(town, 1U, PlayerNames::PLAYER_TWO).abodes.push_back(building);

	const auto interval = temple_heart::BeamInterval(100);
	f.world->turn = interval + 1;
	f.StrikeWithRock(heart, 2500.0f, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(f.world->passedOn, std::vector<entt::entity> {building});
	// The heart's beams come from a spot visual of its player's, ending over the building's top
	EXPECT_EQ(f.world->beamSourcePlayers, std::vector<PlayerNames> {PlayerNames::PLAYER_TWO});
	ASSERT_EQ(f.world->plasma.size(), 1U);
	EXPECT_EQ(f.world->plasma[0].first, 7U);
	EXPECT_EQ(f.world->plasma[0].second.start, glm::vec3(0.0f, 10.0f, 0.0f));
	EXPECT_EQ(f.world->plasma[0].second.end, glm::vec3(50.0f, 10.0f, 0.0f));

	// Not again until the interval has gone by; then with the next of its sounds
	f.world->turn += interval;
	f.StrikeWithRock(heart, 2500.0f, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(f.world->plasma.size(), 1U);
	f.world->turn += 1;
	f.StrikeWithRock(heart, 2500.0f, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(f.world->plasma.size(), 2U);
	const std::vector<entt::id_type> sounds {entt::hashed_string("InGame.sad/206").value(),
	                                         entt::hashed_string("InGame.sad/207").value()};
	EXPECT_EQ(f.world->sounds, sounds);
	// The heart itself took nothing
	EXPECT_TRUE(f.world->effects.empty());
	EXPECT_EQ(f.world->beamSourcePlayers.size(), 1U);
}

TEST(PhysicsGameHooks, ATemplesHeartWithNothingToPassABlowToTakesItsHarm)
{
	Fixture f;
	auto& registry = f.world->registry;
	const auto heart = registry.Create();
	registry.Assign<Transform>(heart, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Temple>(heart, PlayerNames::PLAYER_TWO);
	f.world->turn = 5;
	const auto impact = f.StrikeWithRock(heart, 2500.0f, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(f.world->passedOn.empty());
	ASSERT_EQ(f.world->effects.size(), 1U);
	const auto& body = *f.dynamics.Find(impact.hitBy)->body;
	EXPECT_FLOAT_EQ(f.world->effects[0].values[magic::EffectKind::Hit], temple_heart::Harm(body.velocity, body.Mass()));
	EXPECT_EQ(f.world->effects[0].source.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(f.world->effects[0].source.appliedBy, std::optional(impact.hitBy));
	EXPECT_EQ(registry.Get<const Temple>(heart).lastHitTurn, 5U);
}

TEST(PhysicsGameHooks, AThingThatIsAResourceGoesIntoTheStoreItMeets)
{
	Fixture f;
	auto& registry = f.world->registry;
	const auto store = registry.Create();
	f.world->stores.push_back(store);
	const auto tree = registry.Create();
	f.world->resources[tree] = {.type = ResourceType::Wood, .amount = 100};
	f.struck.entity = tree;
	f.hooks->ReactToImpact(f.dynamics, f.struck, {.hitBy = store, .player = PlayerNames::PLAYER_ONE});
	const std::vector<std::pair<entt::entity, entt::entity>> taken {{store, tree}};
	EXPECT_EQ(f.world->taken, taken);
}

TEST(PhysicsGameHooks, RocksBreakBuildings)
{
	Fixture f;
	const auto building = f.world->registry.Create();
	f.world->registry.Assign<Abode>(building);
	f.StrikeWithRock(building, 2500.0f, std::nullopt);
	EXPECT_EQ(f.world->struckBuildings, std::vector<entt::entity> {building});
}

TEST(PhysicsGameHooks, AnAnimalTheHandDroppedInTheSeaTeachesTheLocalPlayersCreature)
{
	Fixture f;
	auto& registry = f.world->registry;
	const auto animal = registry.Create();
	registry.Assign<Animal>(animal);
	registry.Assign<Transform>(animal, glm::vec3(1.0f, -2.0f, 3.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto hand = registry.Create();
	registry.Assign<HandGrab>(hand).lastDropped = animal;
	PhysicsEntry entry {.entity = animal};
	EXPECT_TRUE(f.hooks->HasSunk(f.dynamics, entry));
	ASSERT_EQ(f.world->deeds.size(), 1U);
	EXPECT_EQ(f.world->deeds[0].deed, living::k_DeedThrowInTheSea);
	EXPECT_EQ(f.world->deeds[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(f.world->removed, std::vector<entt::entity> {animal});
}

TEST(PhysicsGameHooks, ThePlayersCreatureLearnsFromWhatTheHandThrewOnlyWhenItIsAliveOrARock)
{
	Fixture f;
	auto& registry = f.world->registry;
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager);
	registry.Assign<Transform>(villager, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	PhysicsEntry entry {.entity = villager, .player = PlayerNames::PLAYER_ONE};
	f.hooks->ImpactFeedback(f.dynamics, entry, true);
	EXPECT_TRUE(f.world->deeds.empty());
	entry.flags = PhysicsEntry::k_FromHand;
	f.hooks->ImpactFeedback(f.dynamics, entry, true);
	f.hooks->ImpactFeedback(f.dynamics, entry, false);
	ASSERT_EQ(f.world->deeds.size(), 2U);
	EXPECT_EQ(f.world->deeds[0].deed, living::k_DeedDamageByThrowingAt);
	EXPECT_EQ(f.world->deeds[1].deed, living::k_DeedDamageByThrowing);

	// A thing neither alive nor a rock teaches nothing
	const auto pot = registry.Create();
	registry.Assign<Transform>(pot, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	PhysicsEntry potEntry {.entity = pot, .player = PlayerNames::PLAYER_ONE, .flags = PhysicsEntry::k_FromHand};
	f.hooks->ImpactFeedback(f.dynamics, potEntry, true);
	EXPECT_EQ(f.world->deeds.size(), 2U);
}
