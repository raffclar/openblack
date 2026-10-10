/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <ECS/Components/Transform.h>
#include <ECS/Components/Tree.h>
#include <ECS/Components/Villager.h>
#include <ECS/Registry.h>
#include <Inspector.h>
#include <Inspector/ComponentReflection.h>
#include <Inspector/EditProviders.h>
#include <entt/meta/context.hpp>
#include <entt/meta/resolve.hpp>
#include <gtest/gtest.h>

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;

namespace
{

/// A world with no game behind it: things are made with a place, moved, and taken out, and every call is counted, so
/// that the tests see the edit provider goes through the world rather than the registry for what the game must do
class FakeWorld final: public WorldEditInterface
{
public:
	explicit FakeWorld(ecs::Registry& registry)
	    : _registry(registry)
	{
	}

	[[nodiscard]] std::vector<std::string> Kinds() const override { return {"tree", "villager"}; }
	[[nodiscard]] std::vector<std::string> TypeNames(std::string_view kind) const override
	{
		return kind == "tree" ? std::vector<std::string> {"Beech", "Oak"} : std::vector<std::string> {"Farmer"};
	}
	[[nodiscard]] float GroundHeight(glm::vec2 /*point*/) const override { return 7.0f; }

	std::variant<entt::entity, std::string> Create(std::string_view kind, int32_t type, glm::vec3 position,
	                                               float /*yaw*/) override
	{
		const auto entity = _registry.Create();
		_registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		if (kind == "tree")
		{
			_registry.Assign<Tree>(entity, static_cast<TreeInfo>(type), 1.0f);
		}
		inMap.push_back(entity);
		return entity;
	}
	std::string Move(entt::entity entity, glm::vec3 position) override
	{
		_registry.Get<Transform>(entity).position = position;
		++moves;
		return {};
	}
	std::string Turn(entt::entity /*entity*/, float /*yaw*/) override { return {}; }
	std::string Remove(entt::entity entity, RemoveHow how) override
	{
		if (_registry.AllOf<Villager>(entity) && how == RemoveHow::Remove)
		{
			return "refused";
		}
		removed.push_back(entity);
		std::erase(inMap, entity);
		_registry.Destroy(entity);
		return {};
	}
	void Changed(entt::entity /*entity*/, std::string_view component) override { changed.emplace_back(component); }
	[[nodiscard]] Presence PresenceOf(entt::entity entity, std::optional<glm::vec3> lastPosition) const override
	{
		Presence presence {.exists = _registry.Valid(entity), .cell = std::nullopt, .inCell = false, .inPhysics = false};
		if (presence.exists)
		{
			lastPosition = _registry.Get<Transform>(entity).position;
		}
		if (lastPosition.has_value())
		{
			presence.cell = glm::ivec2(static_cast<int>(lastPosition->x / 10.0f), static_cast<int>(lastPosition->z / 10.0f));
			presence.inCell = std::ranges::find(inMap, entity) != inMap.end();
		}
		return presence;
	}

	std::vector<entt::entity> inMap;
	std::vector<entt::entity> removed;
	std::vector<std::string> changed;
	int moves {0};

private:
	ecs::Registry& _registry;
};

class InspectorEdit: public ::testing::Test
{
protected:
	void SetUp() override
	{
		reflection::RegisterComponents(_reflection);
		_villager = _registry.Create();
		_registry.Assign<Transform>(_villager, glm::vec3(21.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		_registry.Assign<Villager>(_villager);
		_registry.Get<Villager>(_villager).life = 0.5f;
		// A tree's storage, as a land with trees has
		_registry.Underlying().storage<Tree>();
		_inspector.Add(
		    std::make_unique<EditProvider>(EditSources {.registry = [this]() -> ecs::Registry* { return &_registry; },
		                                                .info = []() -> const InfoConstants* { return nullptr; }},
		                                   _reflection, _world));
		_inspector.SetWriteLog([this](const Request& request, const QueryResult& answer) {
			_logged.push_back(request.query + (answer.Ok() ? " ok" : " refused"));
		});
	}

	Json Ask(const std::string& line)
	{
		const auto decoded = DecodeRequest(line);
		EXPECT_TRUE(std::holds_alternative<Request>(decoded));
		const auto answer = _inspector.Answer(std::get<Request>(decoded));
		EXPECT_TRUE(answer.Ok()) << line << ": " << answer.error;
		return answer.value;
	}

	std::string Refusal(const std::string& line)
	{
		const auto decoded = DecodeRequest(line);
		EXPECT_TRUE(std::holds_alternative<Request>(decoded));
		const auto answer = _inspector.Answer(std::get<Request>(decoded));
		EXPECT_FALSE(answer.Ok()) << line;
		return answer.error;
	}

	[[nodiscard]] std::string Id() const { return std::to_string(entt::to_integral(_villager)); }

	ecs::Registry _registry;
	entt::meta_ctx _reflection;
	FakeWorld _world {_registry};
	Inspector _inspector;
	entt::entity _villager {entt::null};
	std::vector<std::string> _logged;
};

} // namespace

TEST_F(InspectorEdit, SetsAFieldAndAnswersItAsItNowIs)
{
	const auto answer = Ask(R"({"query": "edit.set", "params": {"id": )" + Id() +
	                        R"(, "component": "Villager", "field": "life", "value": 0.25}})");
	EXPECT_FLOAT_EQ(answer["value"].get<float>(), 0.25f);
	EXPECT_FLOAT_EQ(_registry.Get<Villager>(_villager).life, 0.25f);
	EXPECT_EQ(answer.size(), 4);
	EXPECT_EQ(_world.changed, std::vector<std::string> {"Villager"});

	const auto moved = Ask(R"({"query": "edit.set", "params": {"id": )" + Id() +
	                       R"(, "component": "Transform", "field": "position", "value": [1, 2, 3]}})");
	EXPECT_EQ(moved["value"], Json({1.0, 2.0, 3.0}));
	EXPECT_EQ(_registry.Get<Transform>(_villager).position, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(InspectorEdit, SettingChecksTheFieldsType)
{
	const auto before = _registry.Get<Villager>(_villager);
	EXPECT_NE(Refusal(R"({"query": "edit.set", "params": {"id": )" + Id() +
	                  R"(, "component": "Villager", "field": "life", "value": "full"}})")
	              .find("needs a number"),
	          std::string::npos);
	EXPECT_NE(Refusal(R"({"query": "edit.set", "params": {"id": )" + Id() +
	                  R"(, "component": "Transform", "field": "position", "value": [1, 2]}})")
	              .find("array of 3"),
	          std::string::npos);
	// An enumeration's number must fit it
	EXPECT_NE(Refusal(R"({"query": "edit.set", "params": {"id": )" + Id() +
	                  R"(, "component": "Villager", "field": "lifeStage", "value": 300}})")
	              .find("out of the field's range"),
	          std::string::npos);
	EXPECT_NE(Refusal(R"({"query": "edit.set", "params": {"id": )" + Id() +
	                  R"(, "component": "Villager", "field": "nonsense", "value": 1}})")
	              .find("no field nonsense"),
	          std::string::npos);
	EXPECT_NE(Refusal(R"({"query": "edit.set", "params": {"id": )" + Id() +
	                  R"(, "component": "Tree", "field": "maxSize", "value": 1}})")
	              .find("has no Tree"),
	          std::string::npos);
	EXPECT_FLOAT_EQ(_registry.Get<Villager>(_villager).life, before.life);
	EXPECT_EQ(_registry.Get<Villager>(_villager).lifeStage, before.lifeStage);
	EXPECT_TRUE(_world.changed.empty());
}

TEST_F(InspectorEdit, AddsAndRemovesComponents)
{
	const auto added = Ask(R"({"query": "edit.add", "params": {"id": )" + Id() +
	                       R"(, "component": "Tree", "fields": {"maxSize": 3.5, "growthCountdown": 4}}})");
	ASSERT_TRUE(_registry.AllOf<Tree>(_villager));
	EXPECT_FLOAT_EQ(_registry.Get<Tree>(_villager).maxSize, 3.5f);
	EXPECT_EQ(added["value"]["growthCountdown"], 4);

	EXPECT_NE(Refusal(R"({"query": "edit.add", "params": {"id": )" + Id() + R"(, "component": "Tree"}})").find("already"),
	          std::string::npos);
	// A field that doesn't fit leaves nothing half made
	const auto dead = _registry.Create();
	EXPECT_FALSE(Refusal(R"({"query": "edit.add", "params": {"id": )" + std::to_string(entt::to_integral(dead)) +
	                     R"(, "component": "Tree", "fields": {"maxSize": "big"}}})")
	                 .empty());
	EXPECT_FALSE(_registry.AllOf<Tree>(dead));

	const auto removed = Ask(R"({"query": "edit.remove_component", "params": {"id": )" + Id() + R"(, "component": "Tree"}})");
	EXPECT_FALSE(_registry.AllOf<Tree>(_villager));
	const auto names = removed["components"];
	EXPECT_EQ(std::find(names.begin(), names.end(), Json("Tree")), names.end());
	EXPECT_NE(std::find(names.begin(), names.end(), Json("Villager")), names.end());
	EXPECT_FALSE(Refusal(R"({"query": "edit.add", "params": {"id": )" + Id() + R"(, "component": "NoSuchThing"}})").empty());
}

TEST_F(InspectorEdit, CreatesThroughTheWorldOnTheLand)
{
	const auto made = Ask(R"({"query": "edit.create", "params": {"kind": "tree", "type": "oak", "position": [50, 60]}})");
	EXPECT_EQ(made["kind"], "Tree");
	EXPECT_EQ(made["position"], Json({50.0, 7.0, 60.0}));
	EXPECT_EQ(made["presence"]["exists"], true);
	EXPECT_EQ(made["presence"]["in_cell"], true);
	const auto entity = static_cast<entt::entity>(made["id"].get<uint32_t>());
	EXPECT_EQ(_registry.Get<Tree>(entity).type, static_cast<TreeInfo>(1));

	EXPECT_FALSE(Refusal(R"({"query": "edit.create", "params": {"kind": "tree", "type": 9, "position": [0, 0]}})").empty());
	EXPECT_FALSE(Refusal(R"({"query": "edit.create", "params": {"kind": "rock", "type": 0, "position": [0, 0]}})").empty());

	const auto kinds = Ask(R"({"query": "edit.kinds", "params": {"kind": "tree"}})");
	EXPECT_EQ(kinds["total"], 2);
	EXPECT_EQ(kinds["items"][1]["name"], "Oak");
}

TEST_F(InspectorEdit, MovesThroughTheWorld)
{
	const auto moved = Ask(R"({"query": "edit.move", "params": {"id": )" + Id() + R"(, "position": [30, 1, 40]}})");
	EXPECT_EQ(_world.moves, 1);
	EXPECT_EQ(moved["position"], Json({30.0, 1.0, 40.0}));
	EXPECT_EQ(moved["presence"]["cell"], Json({3, 4}));
}

TEST_F(InspectorEdit, DestroysThroughTheWorldsRemovalAndConfirmsItIsGone)
{
	const auto made = Ask(R"({"query": "edit.create", "params": {"kind": "tree", "type": 0, "position": [55, 0, 65]}})");
	const auto id = made["id"].get<uint32_t>();
	const auto gone = Ask(R"({"query": "edit.destroy", "params": {"id": )" + std::to_string(id) + "}}");
	ASSERT_EQ(_world.removed.size(), 1);
	EXPECT_EQ(entt::to_integral(_world.removed.front()), id);
	EXPECT_EQ(gone["presence"]["exists"], false);
	EXPECT_EQ(gone["presence"]["in_cell"], false);
	EXPECT_EQ(gone["presence"]["in_physics"], false);
	EXPECT_EQ(gone["presence"]["cell"], Json({5, 6}));
	EXPECT_TRUE(gone["was"].get<std::string>().starts_with("Tree"));

	// The world's refusal is the answer
	EXPECT_EQ(Refusal(R"({"query": "edit.destroy", "params": {"id": )" + Id() + "}}"), "refused");
	EXPECT_TRUE(_registry.Valid(_villager));
	EXPECT_FALSE(Refusal(R"({"query": "edit.destroy", "params": {"id": )" + Id() + R"(, "how": "explode"}})").empty());
}

TEST_F(InspectorEdit, WritesAreLoggedAndRemembered)
{
	Ask(R"({"query": "edit.set", "params": {"id": )" + Id() + R"(, "component": "Villager", "field": "food", "value": 1}})");
	Refusal(R"({"query": "edit.set", "params": {"id": )" + Id() +
	        R"(, "component": "Villager", "field": "food", "value": "x"}})");
	// Reading isn't a write
	Ask(R"({"query": "edit.kinds"})");
	EXPECT_EQ(_logged, (std::vector<std::string> {"edit.set ok", "edit.set refused"}));

	const auto writes = Ask(R"({"query": "writes"})");
	EXPECT_EQ(writes["total"], 2);
	EXPECT_EQ(writes["items"][0]["ok"], false);
	EXPECT_EQ(writes["items"][1]["ok"], true);

	const auto described = Ask(R"({"query": "describe", "params": {"query": "edit.destroy"}})");
	EXPECT_EQ(described["writes"], true);
}

TEST(InspectorReflectionCoverage, MostComponentsHaveTheirFieldsRegistered)
{
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	size_t components = 0;
	size_t reflected = 0;
	for (auto&& [id, type] : entt::resolve(context))
	{
		const reflection::ComponentInfo* info = type.custom();
		if (info == nullptr)
		{
			continue;
		}
		++components;
		reflected += type.data().begin() != type.data().end() ? 1 : 0;
	}
	EXPECT_GE(components, 150);
	EXPECT_GE(reflected, 140);
}
