/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <fstream>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <Creature/CreatureDesires.h>
#include <Creature/CreatureIdleMind.h>
#include <Creature/CreaturePlanner.h>
#include <ECS/Components/Abode.h>
#include <ECS/Components/AudioEmitter.h>
#include <ECS/Components/CreatureMind.h>
#include <ECS/Components/Mesh.h>
#include <ECS/Components/Physics.h>
#include <ECS/Components/SoundTag.h>
#include <ECS/Components/Town.h>
#include <ECS/Components/Transform.h>
#include <ECS/Components/Tree.h>
#include <ECS/Components/VillageTotem.h>
#include <ECS/Map.h>
#include <ECS/Registry.h>
#include <Inspector.h>
#include <Inspector/ComponentReflection.h>
#include <Inspector/EntityDescription.h>
#include <Inspector/GameControls.h>
#include <Inspector/GameInput.h>
#include <Inspector/GameProviders.h>
#include <Inspector/GameWorldEdit.h>
#include <Inspector/InputControl.h>
#include <Inspector/LevelControl.h>
#include <Inspector/RegistryProviders.h>
#include <Inspector/RunControl.h>
#include <Inspector/SystemProviders.h>
#include <Inspector/WorldProviders.h>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;

namespace
{

/// The source tree these tests read and update. ctest passes it when the test runs, because a compiler cache shared
/// between worktrees can hand this test an object built in another worktree, whose built-in path names that worktree.
std::string SourceDir()
{
	if (const auto* dir = std::getenv("OPENBLACK_SOURCE_DIR"); dir != nullptr && *dir != '\0')
	{
		return dir;
	}
	return OPENBLACK_SOURCE_DIR;
}

QueryResult AskAny(const Inspector& inspector, const std::string& line)
{
	const auto decoded = DecodeRequest(line);
	EXPECT_TRUE(std::holds_alternative<Request>(decoded)) << line;
	if (!std::holds_alternative<Request>(decoded))
	{
		return QueryResult::Error("not a request");
	}
	return inspector.Answer(std::get<Request>(decoded));
}

Json Ask(const Inspector& inspector, const std::string& line)
{
	const auto answer = AskAny(inspector, line);
	EXPECT_TRUE(answer.Ok()) << line << ": " << answer.error;
	return answer.value;
}

std::string IdOf(entt::entity entity)
{
	return std::to_string(entt::to_integral(entity));
}

entt::entity Placed(ecs::Registry& registry, glm::vec3 position)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	return entity;
}

/// A map whose cells are given by the test
class FakeMap final: public ecs::MapInterface
{
public:
	[[nodiscard]] std::span<const entt::entity> GetFixedInGridCell(const CellId& /*cellId*/) const override { return {}; }
	[[nodiscard]] std::span<const entt::entity> GetFixedInGridCell(const glm::vec3& /*pos*/) const override { return {}; }
	[[nodiscard]] std::span<const entt::entity> GetMobileInGridCell(const CellId& /*cellId*/) const override { return {}; }
	[[nodiscard]] std::span<const entt::entity> GetMobileInGridCell(const glm::vec3& /*pos*/) const override { return {}; }
	[[nodiscard]] std::vector<entt::entity> GetAllInCell(glm::ivec2 cell) const override
	{
		return cell == glm::ivec2(12, 34) ? contents : std::vector<entt::entity> {};
	}
	void Sync() override {}
	void Refile(entt::entity /*entity*/) override {}

	std::vector<entt::entity> contents;
};

/// A clock with no game behind it, for building the game's providers
class StillRunTarget final: public RunTargetInterface
{
public:
	[[nodiscard]] bool IsPaused() const override { return true; }
	void SetPaused(bool /*paused*/) override {}
	[[nodiscard]] uint32_t GetTurn() const override { return 0; }
	[[nodiscard]] float GetSpeed() const override { return 1.0f; }
	void SetSpeed(float /*speed*/) override {}
	void SetFixedFrameTime(std::optional<uint32_t> /*milliseconds*/) override {}
	[[nodiscard]] std::optional<uint32_t> GetFixedFrameTime() const override { return std::nullopt; }
	bool LoadScenario(std::string_view /*id*/) override { return false; }
	[[nodiscard]] std::vector<ScenarioSummary> Scenarios() const override { return {}; }
	[[nodiscard]] uint32_t GetSeed() const override { return 0; }
	void SetSeed(uint32_t /*seed*/, std::optional<int64_t> /*date*/) override {}
	[[nodiscard]] std::optional<int64_t> GetPinnedDate() const override { return std::nullopt; }
	[[nodiscard]] uint32_t GetTicks() const override { return 0; }
};

} // namespace

TEST(InspectorPhysics, BodyByEntityAndBodiesNearAPoint)
{
	std::vector<BodyInfo> bodies;
	for (int i = 0; i < 4; ++i)
	{
		bodies.push_back({.entity = static_cast<entt::entity>(10 + i),
		                  .centre = glm::vec3(i * 10.0f, 0.0f, 0.0f),
		                  .velocity = glm::vec3(1.0f, 0.0f, 0.0f),
		                  .resting = i == 3,
		                  .kind = "other"});
	}
	Inspector inspector;
	inspector.Add(MakePhysicsProvider({.bodies = [&bodies]() { return std::optional(bodies); }}));

	const auto body = Ask(inspector, R"({"query": "physics.body", "params": {"id": 12}})");
	EXPECT_EQ(body["id"], 12);
	EXPECT_EQ(body["position"], Json({20.0, 0.0, 0.0}));
	EXPECT_FALSE(AskAny(inspector, R"({"query": "physics.body", "params": {"id": 99}})").Ok());

	const auto near = Ask(inspector, R"({"query": "physics.bodies", "near": [31, 0, 0], "radius": 15, "limit": 1})");
	EXPECT_EQ(near["total"], 2);
	EXPECT_EQ(near["items"].size(), 1);
	EXPECT_EQ(near["items"][0]["id"], 13);

	const auto state = Ask(inspector, R"({"query": "physics.state"})");
	EXPECT_EQ(state["bodies"], 4);
	EXPECT_EQ(state["resting"], 1);
	EXPECT_EQ(state["flying"], 3);

	Inspector none;
	none.Add(MakePhysicsProvider({.bodies = []() { return std::optional<std::vector<BodyInfo>> {}; }}));
	EXPECT_FALSE(AskAny(none, R"({"query": "physics.state"})").Ok());
}

TEST(InspectorCreature, TopDesiresAndThePlan)
{
	ecs::Registry registry;
	const auto creature = Placed(registry, glm::vec3(0.0f));
	auto& mind = registry.Assign<CreatureMindState>(creature);
	mind.desires = creature_desires::Desires {};
	(*mind.desires)[creature_desires::Desire::Hunger].value = 0.9f;
	(*mind.desires)[creature_desires::Desire::Play].value = 0.5f;
	(*mind.desires)[creature_desires::Desire::Anger].value = 0.7f;
	// Not activated: left out however strong
	(*mind.desires)[creature_desires::Desire::Fear].value = 1.0f;
	(*mind.desires)[creature_desires::Desire::Fear].activated = false;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Hunger, .action = 3, .priority = 2.0f};
	mind.planActive = true;
	mind.idle.agenda = {creature_mind::Step {.kind = creature_mind::Step::Kind::Move, .seconds = 1.0f},
	                    creature_mind::Step {.kind = creature_mind::Step::Kind::Action, .animation = 7}};

	Inspector inspector;
	inspector.Add(MakeCreatureProvider({.world = {.registry = [&registry]() -> const ecs::Registry* { return &registry; },
	                                              .info = []() -> const InfoConstants* { return nullptr; }},
	                                    .tables = []() -> const creature_mind_tables::Tables* { return nullptr; }}));

	// The only creature needs no id
	const auto desires = Ask(inspector, R"({"query": "creature.desires", "params": {"top": 2}})");
	ASSERT_EQ(desires["desires"].size(), 2);
	EXPECT_EQ(desires["desires"][0]["desire"], "Hunger");
	EXPECT_EQ(desires["desires"][1]["desire"], "Anger");

	const auto plan = Ask(inspector, R"({"query": "creature.plan", "params": {"id": )" + IdOf(creature) + "}}");
	EXPECT_EQ(plan["plan"]["desire"], "Hunger");
	// Without the tables the action is its number
	EXPECT_EQ(plan["plan"]["action"], 3);
	EXPECT_EQ(plan["plan_active"], true);
	EXPECT_EQ(plan["agenda_steps"], 2);
	EXPECT_EQ(plan["next_steps"][0]["kind"], "move");
	EXPECT_EQ(plan["next_steps"][1]["animation"], 7);

	const auto list = Ask(inspector, R"({"query": "creature.list"})");
	EXPECT_EQ(list["total"], 1);
	EXPECT_EQ(list["items"][0]["strongest_desire"], "Hunger");

	// With two creatures an id is needed
	registry.Assign<CreatureMindState>(Placed(registry, glm::vec3(5.0f)));
	EXPECT_FALSE(AskAny(inspector, R"({"query": "creature.desires"})").Ok());
	EXPECT_FALSE(AskAny(inspector, R"({"query": "creature.desires", "params": {"id": 12345}})").Ok());
}

TEST(InspectorMap, TheContentsOfACell)
{
	ecs::Registry registry;
	FakeMap map;
	map.contents = {Placed(registry, glm::vec3(125.0f, 0.0f, 345.0f)), Placed(registry, glm::vec3(121.0f, 0.0f, 341.0f))};
	registry.Assign<Tree>(map.contents[0], static_cast<TreeInfo>(0), 1.0f);
	Inspector inspector;
	inspector.Add(MakeMapProvider({.world = {.registry = [&registry]() -> const ecs::Registry* { return &registry; },
	                                         .info = []() -> const InfoConstants* { return nullptr; }},
	                               .map = [&map]() -> const ecs::MapInterface* { return &map; }}));

	const auto byPoint = Ask(inspector, R"({"query": "map.cell", "params": {"position": [125, 345]}})");
	EXPECT_EQ(byPoint["total"], 2);
	EXPECT_EQ(byPoint["items"][0]["kind"], "Tree");
	const auto byCell = Ask(inspector, R"({"query": "map.cell", "params": {"cell": [12, 34]}, "count": true})");
	EXPECT_EQ(byCell["count"], 2);
	EXPECT_EQ(Ask(inspector, R"({"query": "map.cell", "params": {"cell": [0, 0]}})")["total"], 0);
	EXPECT_FALSE(AskAny(inspector, R"({"query": "map.cell"})").Ok());
}

TEST(InspectorTown, HomesAndHomeless)
{
	ecs::Registry registry;
	const auto town = registry.Create();
	auto& townComponent = registry.Assign<Town>(town);
	const auto home = Placed(registry, glm::vec3(10.0f));
	auto& abode = registry.Assign<Abode>(home);
	abode.foodAmount = 5;
	abode.woodAmount = 6;
	const auto resident = Placed(registry, glm::vec3(11.0f));
	const auto homeless = Placed(registry, glm::vec3(12.0f));
	abode.inhabitants = {resident};
	townComponent.abodes = {home};
	townComponent.homelessVillagers = {homeless};

	Inspector inspector;
	inspector.Add(MakeTownProvider({.registry = [&registry]() -> const ecs::Registry* { return &registry; },
	                                .info = []() -> const InfoConstants* { return nullptr; }}));
	const auto homes = Ask(inspector, R"({"query": "town.homes", "params": {"id": )" + IdOf(town) + "}}");
	ASSERT_EQ(homes["total"], 1);
	EXPECT_EQ(homes["items"][0]["inhabitants"], Json({entt::to_integral(resident)}));
	EXPECT_EQ(homes["items"][0]["food"], 5);
	const auto without = Ask(inspector, R"({"query": "town.homeless", "params": {"id": )" + IdOf(town) + "}}");
	EXPECT_EQ(without["items"][0]["id"], entt::to_integral(homeless));
	const auto list = Ask(inspector, R"({"query": "town.list"})");
	EXPECT_EQ(list["items"][0]["homes"], 1);
	EXPECT_EQ(list["items"][0]["homeless"], 1);
	EXPECT_FALSE(AskAny(inspector, R"({"query": "town.homes", "params": {"id": )" + IdOf(home) + "}}").Ok());
}

TEST(InspectorInfluence, TheHandsShare)
{
	Inspector inspector;
	inspector.Add(MakeInfluenceProvider({
	    .hand = [](int player) -> std::optional<glm::vec3> {
		    return player == 0 ? std::optional(glm::vec3(1.0f, 2.0f, 3.0f)) : std::nullopt;
	    },
	    .at = [](int /*player*/, glm::vec3 point) -> std::optional<InfluenceAt> {
		    return InfluenceAt {.influence = point.x, .handPoint = 0.5f, .raw = -0.25f};
	    },
	    .borderShown = [](int /*player*/) { return true; },
	    .circles = []() -> size_t { return 3; },
	}));
	const auto hand = Ask(inspector, R"({"query": "influence.hand"})");
	EXPECT_EQ(hand["hand"], Json({1.0, 2.0, 3.0}));
	EXPECT_FLOAT_EQ(hand["influence"].get<float>(), 1.0f);
	EXPECT_EQ(hand["inside"], true);
	EXPECT_FLOAT_EQ(hand["raw"].get<float>(), -0.25f);
	EXPECT_FALSE(AskAny(inspector, R"({"query": "influence.hand", "params": {"player": 2}})").Ok());
	EXPECT_FLOAT_EQ(Ask(inspector, R"({"query": "influence.at", "params": {"position": [7, 9]}})")["influence"].get<float>(),
	                7.0f);
	EXPECT_EQ(Ask(inspector, R"({"query": "influence.state"})")["circles"], 3);
}

TEST(InspectorAudio, SoundsNearAPoint)
{
	ecs::Registry registry;
	const auto near = registry.Create();
	registry.Assign<AudioEmitter>(near, AudioEmitter {.spatial = true, .position = glm::vec3(5.0f, 0.0f, 0.0f)});
	const auto far = registry.Create();
	registry.Assign<AudioEmitter>(far, AudioEmitter {.spatial = true, .position = glm::vec3(500.0f, 0.0f, 0.0f)});
	const auto flat = registry.Create();
	registry.Assign<AudioEmitter>(flat, AudioEmitter {.spatial = false});
	registry.Get<AudioEmitter>(near).state = audio::AudioStatus::Playing;

	Inspector inspector;
	inspector.Add(
	    MakeAudioProvider({.world = {.registry = [&registry]() -> const ecs::Registry* { return &registry; },
	                                 .info = []() -> const InfoConstants* { return nullptr; }},
	                       .state = []() -> std::optional<AudioState> { return AudioState {.globalVolume = 1.0f}; }}));
	const auto sounds = Ask(inspector, R"({"query": "audio.sounds", "near": [0, 0, 0], "radius": 50})");
	ASSERT_EQ(sounds["total"], 1);
	EXPECT_EQ(sounds["items"][0]["id"], entt::to_integral(near));
	EXPECT_EQ(sounds["items"][0]["state"], "playing");
	EXPECT_EQ(Ask(inspector, R"({"query": "audio.state"})")["sounds_playing"], 1);
}

TEST(InspectorParticles, EmittersByOwner)
{
	std::vector<ParticleEffectInfo> effects(3);
	effects[0].id = 1;
	effects[0].owner = static_cast<entt::entity>(7);
	effects[1].id = 2;
	effects[2].id = 3;
	effects[2].owner = static_cast<entt::entity>(7);
	Inspector inspector;
	inspector.Add(std::make_unique<ParticlesProvider>(ParticleSources {
	    .rings = {},
	    .effects = [&effects]() { return effects; },
	}));
	const auto emitters = Ask(inspector, R"({"query": "particles.emitters", "params": {"owner": 7}})");
	ASSERT_EQ(emitters["total"], 2);
	EXPECT_EQ(emitters["items"][0]["id"], 1);
	EXPECT_EQ(emitters["items"][1]["owner"], 7);
}

/// Every service the locator holds has a query that inspects it: a service added to the locator without one fails here
TEST(InspectorCoverage, EveryLocatorServiceHasAQuery)
{
	std::ifstream header(SourceDir() + "/src/Locator.h");
	ASSERT_TRUE(header.good());
	std::stringstream text;
	text << header.rdbuf();
	const std::regex alias(R"(\n\tusing (\w+) = entt::locator<)");
	std::set<std::string> services;
	const auto content = text.str();
	for (auto it = std::sregex_iterator(content.begin(), content.end(), alias); it != std::sregex_iterator(); ++it)
	{
		services.insert((*it)[1].str());
	}
	ASSERT_GT(services.size(), 80);

	entt::meta_ctx reflection;
	reflection::RegisterComponents(reflection);
	StillRunTarget run;
	GameWorldEdit world;
	GameInput input;
	GameControlSet controls(input);
	Inspector inspector;
	AddGameProviders(inspector, reflection, run, world, controls.View());

	std::set<std::string> covered;
	for (const auto& [service, query] : CoveredServices())
	{
		covered.insert(std::string(service));
		const auto described =
		    AskAny(inspector, R"({"query": "describe", "params": {"query": ")" + std::string(query) + R"("}})");
		EXPECT_TRUE(described.Ok()) << service << " is covered by " << query << ", which isn't there";
	}
	for (const auto& service : services)
	{
		EXPECT_TRUE(covered.contains(service)) << "the locator's " << service << " has no inspector query";
	}
}

/// With no land loaded and none of the locator's services there, every query answers, if only to say why it can't
TEST(InspectorCoverage, EveryQueryAnswersWithNoGame)
{
	entt::meta_ctx reflection;
	reflection::RegisterComponents(reflection);
	StillRunTarget run;
	GameWorldEdit world;
	GameInput input;
	GameControlSet controls(input);
	Inspector inspector;
	AddGameProviders(inspector, reflection, run, world, controls.View());

	const auto described = AskAny(inspector, R"({"query": "describe"})");
	ASSERT_TRUE(described.Ok());
	// The whole catalogue comes back, however many queries it holds
	ASSERT_TRUE(described.value.contains("providers")) << described.value.dump().substr(0, 200);
	size_t queries = 0;
	for (const auto& [provider, list] : described.value["providers"].items())
	{
		for (const auto& [name, text] : list.items())
		{
			++queries;
			// Reads only: a write with no game is refused all the same, but needn't be tried
			const auto query = provider + "." + name;
			const auto answer = AskAny(
			    inspector,
			    R"({"query": ")" + query +
			        R"(", "params": {"id": 1, "position": [0, 0], "owner": 1, "cell": [0, 0], "kind": "tree", "type": 0, "component": "Transform", "field": "position", "value": [0, 0, 0]}, "near": [0, 0], "radius": 10})");
			EXPECT_TRUE(answer.Ok() || !answer.error.empty()) << query;
		}
	}
	EXPECT_GT(queries, 60);
}

/// The MCP adapter builds its tools' schemas (parameters, which are required) from the game's own descriptions of its
/// queries, kept in tools/inspector/inspector_queries.json. A query added or changed without that file written again
/// fails here; set OPENBLACK_UPDATE_INSPECTOR_QUERIES=1 and run this test to write it.
TEST(InspectorCoverage, TheAdaptersCatalogueOfQueriesIsUpToDate)
{
	entt::meta_ctx reflection;
	reflection::RegisterComponents(reflection);
	StillRunTarget run;
	GameWorldEdit world;
	GameInput input;
	GameControlSet controls(input);
	Inspector inspector;
	// The providers the game's inspector has, as its system adds them
	AddGameProviders(inspector, reflection, run, world, controls.View());
	inspector.Add(std::make_unique<ScreenshotProvider>(controls.screenshots, controls.camera));
	inspector.Add(std::make_unique<InputProvider>(input));
	const auto catalogue = inspector.Catalogue().dump(1) + "\n";

	const auto path = SourceDir() + "/tools/inspector/inspector_queries.json";
	if (const auto* update = std::getenv("OPENBLACK_UPDATE_INSPECTOR_QUERIES"); update != nullptr && std::string(update) == "1")
	{
		std::ofstream(path, std::ios::binary) << catalogue;
	}
	std::ifstream file(path, std::ios::binary);
	std::stringstream written;
	written << file.rdbuf();
	EXPECT_EQ(written.str(), catalogue) << path << " is out of date: run this test with OPENBLACK_UPDATE_INSPECTOR_QUERIES=1";
}

// A thing is known by what it is, not by what many kinds of thing share: a totem drawn with a mesh and heard by its
// sound tag is a VillageTotem; one with only shared components is known by the first past its place
TEST(InspectorEntities, KindsNameWhatAThingIs)
{
	ecs::Registry registry;
	const auto drawn = Placed(registry, glm::vec3(0.0f));
	registry.Assign<ecs::components::Mesh>(drawn, entt::id_type {0}, int8_t {0}, int8_t {-1});
	const auto totem = Placed(registry, glm::vec3(1.0f));
	registry.Assign<ecs::components::Mesh>(totem, entt::id_type {0}, int8_t {0}, int8_t {-1});
	registry.Assign<ecs::components::SoundTag>(totem, entt::id_type {0}, glm::vec3(0.0f));
	registry.Assign<ecs::components::VillageTotem>(totem);

	EXPECT_EQ(Describe(registry, totem, nullptr).kind, "VillageTotem");
	EXPECT_EQ(Describe(registry, drawn, nullptr).kind, "Mesh");
	EXPECT_EQ(Describe(registry, registry.Create(), nullptr).kind, "Entity");
}

// A thing moving in the physics is drawn at its body, which may be far from where it stood: lists and searches give
// where it is drawn, and where it stands beside it
TEST(InspectorEntities, PositionsAreWhereThingsAreDrawn)
{
	ecs::Registry registry;
	const auto standing = Placed(registry, glm::vec3(2587.0f, 14.7f, 2690.0f));
	const auto flying = Placed(registry, glm::vec3(2587.0f, 14.7f, 2690.0f));
	registry.Assign<ecs::components::PhysicsDrawPose>(
	    flying,
	    ecs::components::PhysicsDrawPose {.axes = glm::mat3(1.0f), .origin = {2578.0f, 0.8f, 2790.0f}, .underSea = false});

	EXPECT_EQ(DrawnPosition(registry, standing), glm::vec3(2587.0f, 14.7f, 2690.0f));
	EXPECT_EQ(DrawnPosition(registry, flying), glm::vec3(2578.0f, 0.8f, 2790.0f));
	EXPECT_FALSE(DrawnPosition(registry, registry.Create()).has_value());

	const auto item = ToListItem(flying, Describe(registry, flying, nullptr));
	EXPECT_EQ(item["position"], Json({2578.0f, 0.8f, 2790.0f}));
	EXPECT_EQ(item["transform_position"], Json({2587.0f, 14.7f, 2690.0f}));
	EXPECT_FALSE(ToListItem(standing, Describe(registry, standing, nullptr)).contains("transform_position"));

	// A search around the body finds it, and one around where it stood finds only the one standing there
	QueryOptions nearBody;
	nearBody.near = Near {.point = {2578.0, 0.8, 2790.0}, .planar = true, .radius = 5.0};
	const auto found = FindEntities(registry, nullptr, Json::object(), nearBody);
	ASSERT_TRUE(found.Ok()) << found.error;
	ASSERT_EQ(found.value.size(), 1u);
	EXPECT_EQ(found.value[0]["id"], ToId(flying));
	QueryOptions nearStand;
	nearStand.near = Near {.point = {2587.0, 14.7, 2690.0}, .planar = true, .radius = 5.0};
	const auto stood = FindEntities(registry, nullptr, Json::object(), nearStand);
	ASSERT_EQ(stood.value.size(), 1u);
	EXPECT_EQ(stood.value[0]["id"], ToId(standing));
}
