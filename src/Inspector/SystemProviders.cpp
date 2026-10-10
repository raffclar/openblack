/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SystemProviders.h"

#include <cmath>

#include <algorithm>
#include <numeric>
#include <string>
#include <utility>
#include <variant>

#include <InspectorQuery.h>

#include "Audio/Sound.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreaturePlanner.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;

namespace
{

constexpr std::string_view k_NoRegistry = "there is no registry: no land is loaded";
/// A creature's desires are given this many at a time unless asked for more
constexpr size_t k_DefaultTopDesires = 5;
/// An agenda's steps are given only this many from the current one
constexpr size_t k_MostSteps = 8;

Json Point(const glm::vec3& point)
{
	return {point.x, point.y, point.z};
}

Json Id(entt::entity entity)
{
	return entity == entt::null ? Json(nullptr) : Json(ToId(entity));
}

QueryDescription Query(std::string name, std::string description, std::vector<ParameterDescription> parameters = {},
                       ResultKind kind = ResultKind::Object, bool needsNear = false)
{
	return {.name = std::move(name),
	        .description = std::move(description),
	        .parameters = std::move(parameters),
	        .kind = kind,
	        .needsNear = needsNear,
	        .writes = false};
}

ParameterDescription Parameter(std::string name, std::string type, std::string description, bool required)
{
	return {.name = std::move(name), .type = std::move(type), .description = std::move(description), .required = required};
}

/// The entity a request names by its id, if it exists
std::optional<entt::entity> EntityParam(const ecs::Registry& registry, const Json& params, std::string_view key = "id")
{
	const auto it = params.find(key);
	const auto entity = it == params.end() ? std::nullopt : FromId(*it);
	if (!entity.has_value() || !registry.Valid(*entity))
	{
		return std::nullopt;
	}
	return entity;
}

const InfoConstants* Info(const WorldSources& sources)
{
	return sources.info ? sources.info() : nullptr;
}

const ecs::Registry* Registry(const WorldSources& sources)
{
	return sources.registry ? sources.registry() : nullptr;
}

/// The player a request names by number, the first player's by default
int PlayerParam(const Json& params)
{
	return static_cast<int>(NumberMember(params, "player").value_or(0.0));
}

// Creatures

/// The creature a request names, or the only creature when it names none and there is just one
std::variant<entt::entity, std::string> CreatureParam(const ecs::Registry& registry, const Json& params)
{
	if (params.contains("id"))
	{
		const auto entity = EntityParam(registry, params);
		if (!entity.has_value() || !registry.AllOf<CreatureMindState>(*entity))
		{
			return std::string("no creature with a mind has that id");
		}
		return *entity;
	}
	std::vector<entt::entity> creatures;
	registry.Each<const CreatureMindState>(
	    [&creatures](entt::entity entity, const CreatureMindState& /*mind*/) { creatures.push_back(entity); });
	if (creatures.size() == 1)
	{
		return creatures.front();
	}
	return std::string(creatures.empty() ? "there is no creature" : "there are several creatures: give an id (creature.list)");
}

std::string_view StepKindName(creature_mind::Step::Kind kind)
{
	using Kind = creature_mind::Step::Kind;
	switch (kind)
	{
	case Kind::Wait:
		return "wait";
	case Kind::Action:
		return "action";
	case Kind::Static:
		return "static";
	case Kind::Move:
		return "move";
	case Kind::Object:
		return "object";
	case Kind::Cast:
		return "cast";
	case Kind::Gesture:
		return "gesture";
	case Kind::WaitInMap:
		return "wait_in_map";
	case Kind::Douse:
		return "douse";
	}
	return "unknown";
}

Json OptionalNumber(const std::optional<uint32_t>& value)
{
	return value.has_value() ? Json(*value) : Json(nullptr);
}

Json PlanItem(const creature_planner::Plan& plan, const creature_mind_tables::Tables* tables)
{
	Json action = plan.action;
	if (tables != nullptr && plan.action < tables->actions.size())
	{
		action = tables->actions[plan.action].name;
	}
	return {
	    {"desire", creature_desires::Name(plan.desire)}, {"action", std::move(action)},
	    {"object", OptionalNumber(plan.object)},         {"priority", plan.priority},
	    {"action_priority", plan.actionPriority},        {"goal_usefulness", plan.goalUsefulness},
	};
}

// Towns

std::variant<std::pair<entt::entity, const Town*>, std::string> TownParam(const ecs::Registry& registry, const Json& params)
{
	const auto entity = EntityParam(registry, params);
	if (!entity.has_value())
	{
		return std::string("no entity with that id: give a town's id (town.list)");
	}
	const auto* town = registry.TryGet<const Town>(*entity);
	if (town == nullptr)
	{
		return std::string("that entity is not a town");
	}
	return std::pair {*entity, town};
}

Json Listed(const ecs::Registry& registry, const InfoConstants* info, entt::entity entity)
{
	if (!registry.Valid(entity))
	{
		return {{"id", ToId(entity)}, {"gone", true}};
	}
	return ToListItem(entity, Describe(registry, entity, info));
}

// Sounds

std::string_view StatusName(audio::AudioStatus status)
{
	switch (status)
	{
	case audio::AudioStatus::Initial:
		return "initial";
	case audio::AudioStatus::Playing:
		return "playing";
	case audio::AudioStatus::Paused:
		return "paused";
	case audio::AudioStatus::Stopped:
		return "stopped";
	}
	return "unknown";
}

std::string_view LoopName(audio::PlayType type)
{
	switch (type)
	{
	case audio::PlayType::Repeat:
		return "repeat";
	case audio::PlayType::Once:
		return "once";
	case audio::PlayType::Overlap:
		return "overlap";
	}
	return "unknown";
}

} // namespace

Json openblack::inspector::BodyItem(const BodyInfo& body)
{
	return {
	    {"id", Id(body.entity)},
	    {"position", Point(body.centre)},
	    {"velocity", Point(body.velocity)},
	    {"speed", body.speed},
	    {"mass", body.mass},
	    {"radius", body.radius},
	    {"resting", body.resting},
	    {"in_water", body.inWater},
	    {"kind", body.kind},
	    {"flags", body.flags},
	    {"player", body.player.has_value() ? Json(*body.player) : Json(nullptr)},
	    {"thrower", Id(body.thrower)},
	    {"impact", body.impact},
	    {"contacts", body.contacts},
	};
}

std::unique_ptr<ProviderInterface> openblack::inspector::MakePhysicsProvider(PhysicsSources sources)
{
	auto provider = std::make_unique<FunctionProvider>("physics");
	const auto bodies = [sources]() { return sources.bodies ? sources.bodies() : std::nullopt; };
	constexpr std::string_view k_NoPhysics = "there is no physics: no land is loaded";
	provider->Add(Query("state", "How many bodies the physics has: flying, resting and in water"),
	              [bodies, k_NoPhysics](const QueryContext& /*context*/) {
		              const auto all = bodies();
		              if (!all.has_value())
		              {
			              return QueryResult::Error(std::string(k_NoPhysics));
		              }
		              const auto resting = std::ranges::count_if(*all, &BodyInfo::resting);
		              const auto inWater = std::ranges::count_if(*all, &BodyInfo::inWater);
		              return QueryResult::Value({{"bodies", all->size()},
		                                         {"flying", static_cast<int64_t>(all->size()) - resting},
		                                         {"resting", resting},
		                                         {"in_water", inWater}});
	              });
	provider->Add(Query("body", "One object's body in the physics: place, velocity, mass, resting, who threw it",
	                    {Parameter("id", "integer", "The object's entity id", true)}),
	              [bodies, k_NoPhysics](const QueryContext& context) {
		              const auto all = bodies();
		              if (!all.has_value())
		              {
			              return QueryResult::Error(std::string(k_NoPhysics));
		              }
		              const auto id = context.params.find("id");
		              const auto entity = id == context.params.end() ? std::nullopt : FromId(*id);
		              const auto found = std::ranges::find_if(
		                  *all, [&entity](const BodyInfo& body) { return entity.has_value() && body.entity == *entity; });
		              if (found == all->end())
		              {
			              return QueryResult::Error("it has no body: it isn't in the physics");
		              }
		              return QueryResult::Value(BodyItem(*found));
	              });
	provider->Add(Query("bodies", "The bodies about a point, nearest first", {}, ResultKind::List, true),
	              [bodies, k_NoPhysics](const QueryContext& /*context*/) {
		              const auto all = bodies();
		              if (!all.has_value())
		              {
			              return QueryResult::Error(std::string(k_NoPhysics));
		              }
		              Json items = Json::array();
		              for (const auto& body : *all)
		              {
			              items.push_back(BodyItem(body));
		              }
		              return QueryResult::Value(std::move(items));
	              });
	return provider;
}

Json openblack::inspector::DesireItems(const creature_desires::Desires& desires, size_t top)
{
	std::vector<size_t> order(desires.desires.size());
	std::iota(order.begin(), order.end(), size_t {0});
	std::erase_if(order, [&desires](size_t i) { return !desires.desires.at(i).activated; });
	std::ranges::stable_sort(
	    order, [&desires](size_t a, size_t b) { return desires.desires.at(a).value > desires.desires.at(b).value; });
	Json items = Json::array();
	for (size_t i = 0; i < std::min(top, order.size()); ++i)
	{
		const auto& desire = desires.desires.at(order[i]);
		items.push_back({
		    {"desire", creature_desires::Name(static_cast<creature_desires::Desire>(order[i]))},
		    {"value", desire.value},
		    {"max", desire.max},
		    {"suppressed_turns", desire.suppressedTurns},
		    {"carried_out", desire.carriedOut},
		});
	}
	return items;
}

Json openblack::inspector::PlanJson(const CreatureMindState& mind, const creature_mind_tables::Tables* tables)
{
	Json steps = Json::array();
	const auto& agenda = mind.idle.agenda;
	for (size_t i = mind.idle.step; i < agenda.size() && steps.size() < k_MostSteps; ++i)
	{
		const auto& step = agenda[i];
		steps.push_back({
		    {"kind", StepKindName(step.kind)},
		    {"seconds", step.seconds},
		    {"animation", step.animation},
		    {"object", OptionalNumber(step.object)},
		});
	}
	return {
	    {"plan", mind.planner.current.has_value() ? PlanItem(*mind.planner.current, tables) : Json(nullptr)},
	    {"plan_active", mind.planActive},
	    {"activity", creature_mind::Name(mind.idle.activity)},
	    {"step", mind.idle.step},
	    {"step_started", mind.idle.stepStarted},
	    {"agenda_steps", agenda.size()},
	    {"next_steps", std::move(steps)},
	    {"turn", mind.turn},
	    {"planned_turn", mind.plannedTurn},
	};
}

std::unique_ptr<ProviderInterface> openblack::inspector::MakeCreatureProvider(CreatureSources sources)
{
	auto provider = std::make_unique<FunctionProvider>("creature");
	const ParameterDescription id =
	    Parameter("id", "integer", "The creature's entity id; may be left out when there is one creature", false);
	provider->Add(Query("list", "The creatures with minds: what each is doing and its strongest desire", {}, ResultKind::List),
	              [sources](const QueryContext& /*context*/) {
		              const auto* registry = Registry(sources.world);
		              if (registry == nullptr)
		              {
			              return QueryResult::Error(std::string(k_NoRegistry));
		              }
		              Json items = Json::array();
		              registry->Each<const CreatureMindState>([&](entt::entity entity, const CreatureMindState& mind) {
			              auto item = ToListItem(entity, Describe(*registry, entity, Info(sources.world)));
			              item["activity"] = creature_mind::Name(mind.idle.activity);
			              const auto strongest = mind.desires.has_value() ? DesireItems(*mind.desires, 1) : Json::array();
			              item["strongest_desire"] = strongest.empty() ? Json(nullptr) : strongest[0]["desire"];
			              items.push_back(std::move(item));
		              });
		              return QueryResult::Value(std::move(items));
	              });
	provider->Add(Query("desires", "A creature's strongest desires, strongest first",
	                    {id, Parameter("top", "integer", "How many desires (5 by default)", false)}),
	              [sources](const QueryContext& context) {
		              const auto* registry = Registry(sources.world);
		              if (registry == nullptr)
		              {
			              return QueryResult::Error(std::string(k_NoRegistry));
		              }
		              const auto creature = CreatureParam(*registry, context.params);
		              if (const auto* problem = std::get_if<std::string>(&creature); problem != nullptr)
		              {
			              return QueryResult::Error(*problem);
		              }
		              const auto entity = std::get<entt::entity>(creature);
		              const auto& mind = registry->Get<const CreatureMindState>(entity);
		              if (!mind.desires.has_value())
		              {
			              return QueryResult::Error("its desires aren't set up yet: its mind hasn't thought");
		              }
		              const auto top =
		                  static_cast<size_t>(std::max(1.0, NumberMember(context.params, "top").value_or(k_DefaultTopDesires)));
		              return QueryResult::Value(
		                  {{"id", ToId(entity)}, {"sum", mind.desires->sum}, {"desires", DesireItems(*mind.desires, top)}});
	              });
	provider->Add(
	    Query("known", "The ordinary skills and miracles a creature knows, and how often it has seen each miracle", {id}),
	    [sources](const QueryContext& context) {
		    const auto* registry = Registry(sources.world);
		    if (registry == nullptr)
		    {
			    return QueryResult::Error(std::string(k_NoRegistry));
		    }
		    const auto creature = CreatureParam(*registry, context.params);
		    if (const auto* problem = std::get_if<std::string>(&creature); problem != nullptr)
		    {
			    return QueryResult::Error(*problem);
		    }
		    const auto entity = std::get<entt::entity>(creature);
		    const auto& mind = registry->Get<const CreatureMindState>(entity);
		    if (!mind.learnt.has_value())
		    {
			    return QueryResult::Error("its learning isn't set up yet: its mind hasn't thought");
		    }
		    const auto* tables = sources.tables ? sources.tables() : nullptr;
		    const auto& knowledge = mind.learnt->knowledge;
		    auto skills = Json::array();
		    for (size_t i = 0; i < knowledge.skillsKnown.size(); ++i)
		    {
			    if (knowledge.skillsKnown[i])
			    {
				    skills.push_back(
				        {{"skill", i},
				         {"name", tables != nullptr && i < tables->skills.size() ? tables->skills[i].name : std::string()}});
			    }
		    }
		    auto miracles = Json::array();
		    for (size_t i = 0; i < knowledge.miraclesKnown.size(); ++i)
		    {
			    const auto seen = i < knowledge.miraclesSeen.size() ? knowledge.miraclesSeen[i].count : 0u;
			    if (knowledge.miraclesKnown[i] || seen > 0)
			    {
				    miracles.push_back(
				        {{"miracle", i},
				         {"name", tables != nullptr && i < tables->miracles.size() ? tables->miracles[i].name : std::string()},
				         {"known", static_cast<bool>(knowledge.miraclesKnown[i])},
				         {"seen", seen}});
			    }
		    }
		    return QueryResult::Value({{"id", ToId(entity)},
		                               {"skills", std::move(skills)},
		                               {"miracles", std::move(miracles)},
		                               {"pending_teaching", mind.pendingTeaching.size()}});
	    });
	provider->Add(Query("plan", "A creature's plan (desire, action, object, priority), activity and next agenda steps", {id}),
	              [sources](const QueryContext& context) {
		              const auto* registry = Registry(sources.world);
		              if (registry == nullptr)
		              {
			              return QueryResult::Error(std::string(k_NoRegistry));
		              }
		              const auto creature = CreatureParam(*registry, context.params);
		              if (const auto* problem = std::get_if<std::string>(&creature); problem != nullptr)
		              {
			              return QueryResult::Error(*problem);
		              }
		              const auto entity = std::get<entt::entity>(creature);
		              auto plan =
		                  PlanJson(registry->Get<const CreatureMindState>(entity), sources.tables ? sources.tables() : nullptr);
		              plan["id"] = ToId(entity);
		              return QueryResult::Value(std::move(plan));
	              });
	return provider;
}

std::unique_ptr<ProviderInterface> openblack::inspector::MakeMapProvider(MapSources sources)
{
	auto provider = std::make_unique<FunctionProvider>("map");
	provider->Add(
	    Query("cell",
	          "What stands in a map cell (10 by 10) as a search meets it: the things that stay put, then those that move",
	          {Parameter("position", "point", "A point in the cell, [x, z] or [x, y, z]", false),
	           Parameter("cell", "array", "The cell's [column, row] instead", false)},
	          ResultKind::List),
	    [sources](const QueryContext& context) {
		    const auto* registry = Registry(sources.world);
		    const auto* map = sources.map ? sources.map() : nullptr;
		    if (registry == nullptr || map == nullptr)
		    {
			    return QueryResult::Error("there is no map: no land is loaded");
		    }
		    std::optional<glm::ivec2> cell;
		    const auto& params = context.params;
		    if (const auto it = params.find("cell"); it != params.end())
		    {
			    if (it->is_array() && it->size() == 2 && (*it)[0].is_number_integer() && (*it)[1].is_number_integer())
			    {
				    cell = glm::ivec2((*it)[0].get<int>(), (*it)[1].get<int>());
			    }
		    }
		    else if (const auto position = params.find("position"); position != params.end())
		    {
			    if (const auto point = ReadPoint(*position); point.has_value())
			    {
				    constexpr double k_CellSize = 10.0;
				    cell = glm::ivec2(static_cast<int>(std::floor((*point)[0] / k_CellSize)),
				                      static_cast<int>(std::floor((*point)[2] / k_CellSize)));
			    }
		    }
		    if (!cell.has_value())
		    {
			    return QueryResult::Error("give position [x, z] or cell [column, row]");
		    }
		    Json items = Json::array();
		    for (const auto entity : map->GetAllInCell(*cell))
		    {
			    items.push_back(Listed(*registry, Info(sources.world), entity));
		    }
		    return QueryResult::Value(std::move(items));
	    });
	return provider;
}

std::unique_ptr<ProviderInterface> openblack::inspector::MakeTownProvider(WorldSources sources)
{
	auto provider = std::make_unique<FunctionProvider>("town");
	const ParameterDescription id = Parameter("id", "integer", "The town's entity id (town.list)", true);
	provider->Add(Query("list", "The towns: number, owner, buildings and people without a home", {}, ResultKind::List),
	              [sources](const QueryContext& /*context*/) {
		              const auto* registry = Registry(sources);
		              if (registry == nullptr)
		              {
			              return QueryResult::Error(std::string(k_NoRegistry));
		              }
		              Json items = Json::array();
		              registry->Each<const Town>([&](entt::entity entity, const Town& town) {
			              auto item = ToListItem(entity, Describe(*registry, entity, Info(sources)));
			              item["number"] = town.id;
			              item["owner"] = static_cast<int>(town.owner);
			              item["homes"] = town.abodes.size();
			              item["homeless"] = town.homelessVillagers.size();
			              item["uninhabitable"] = town.uninhabitable;
			              items.push_back(std::move(item));
		              });
		              return QueryResult::Value(std::move(items));
	              });
	provider->Add(
	    Query("homes", "A town's buildings, newest first, with who lives in each and who is at home", {id}, ResultKind::List),
	    [sources](const QueryContext& context) {
		    const auto* registry = Registry(sources);
		    if (registry == nullptr)
		    {
			    return QueryResult::Error(std::string(k_NoRegistry));
		    }
		    const auto town = TownParam(*registry, context.params);
		    if (const auto* problem = std::get_if<std::string>(&town); problem != nullptr)
		    {
			    return QueryResult::Error(*problem);
		    }
		    Json items = Json::array();
		    for (const auto abode : std::get<0>(town).second->abodes)
		    {
			    auto item = Listed(*registry, Info(sources), abode);
			    if (const auto* home = registry->Valid(abode) ? registry->TryGet<const Abode>(abode) : nullptr)
			    {
				    Json people = Json::array();
				    for (const auto villager : home->inhabitants)
				    {
					    people.push_back(ToId(villager));
				    }
				    item["inhabitants"] = std::move(people);
				    item["present_at_home"] = home->presentAtHome;
				    item["food"] = home->foodAmount;
				    item["wood"] = home->woodAmount;
			    }
			    items.push_back(std::move(item));
		    }
		    return QueryResult::Value(std::move(items));
	    });
	provider->Add(Query("homeless", "A town's people without a home, newest first", {id}, ResultKind::List),
	              [sources](const QueryContext& context) {
		              const auto* registry = Registry(sources);
		              if (registry == nullptr)
		              {
			              return QueryResult::Error(std::string(k_NoRegistry));
		              }
		              const auto town = TownParam(*registry, context.params);
		              if (const auto* problem = std::get_if<std::string>(&town); problem != nullptr)
		              {
			              return QueryResult::Error(*problem);
		              }
		              Json items = Json::array();
		              for (const auto villager : std::get<0>(town).second->homelessVillagers)
		              {
			              items.push_back(Listed(*registry, Info(sources), villager));
		              }
		              return QueryResult::Value(std::move(items));
	              });
	return provider;
}

std::unique_ptr<ProviderInterface> openblack::inspector::MakeInfluenceProvider(InfluenceSources sources)
{
	auto provider = std::make_unique<FunctionProvider>("influence");
	const ParameterDescription player = Parameter("player", "integer", "The player's number, 0 (the first) by default", false);
	const auto influenceJson = [](const InfluenceAt& at) {
		return Json {
		    {"influence", at.influence}, {"hand_point", at.handPoint}, {"raw", at.raw}, {"inside", at.handPoint > 0.0f}};
	};
	provider->Add(Query("hand",
	                    "The hand's share: the player's influence where their hand is (with what the hand keeps past the "
	                    "border), at the hand's own place, and their own; whether the hand is inside",
	                    {player}),
	              [sources, influenceJson](const QueryContext& context) {
		              const auto number = PlayerParam(context.params);
		              const auto hand = sources.hand ? sources.hand(number) : std::nullopt;
		              if (!hand.has_value())
		              {
			              return QueryResult::Error("the player has no hand");
		              }
		              const auto at = sources.at ? sources.at(number, *hand) : std::nullopt;
		              if (!at.has_value())
		              {
			              return QueryResult::Error("there is no influence: no land is loaded");
		              }
		              auto result = influenceJson(*at);
		              result["player"] = number;
		              result["hand"] = Point(*hand);
		              result["border_shown"] = sources.borderShown ? sources.borderShown(number) : false;
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(
	    Query("at", "A player's influence at a point", {Parameter("position", "point", "[x, z] or [x, y, z]", true), player}),
	    [sources, influenceJson](const QueryContext& context) {
		    const auto position = context.params.find("position");
		    const auto point = position == context.params.end() ? std::nullopt : ReadPoint(*position);
		    if (!point.has_value())
		    {
			    return QueryResult::Error("position must be [x, z] or [x, y, z]");
		    }
		    const glm::vec3 at3(static_cast<float>((*point)[0]), static_cast<float>((*point)[1]),
		                        static_cast<float>((*point)[2]));
		    const auto number = PlayerParam(context.params);
		    const auto at = sources.at ? sources.at(number, at3) : std::nullopt;
		    if (!at.has_value())
		    {
			    return QueryResult::Error("there is no influence: no land is loaded");
		    }
		    auto result = influenceJson(*at);
		    result["player"] = number;
		    return QueryResult::Value(std::move(result));
	    });
	provider->Add(Query("state", "How many circles of influence there are"), [sources](const QueryContext& /*context*/) {
		return QueryResult::Value({{"circles", sources.circles ? sources.circles() : 0}});
	});
	return provider;
}

std::unique_ptr<ProviderInterface> openblack::inspector::MakeAudioProvider(AudioSources sources)
{
	auto provider = std::make_unique<FunctionProvider>("audio");
	provider->Add(Query("state", "The volumes, whether music plays, and how many sounds play"),
	              [sources](const QueryContext& /*context*/) {
		              const auto state = sources.state ? sources.state() : std::nullopt;
		              if (!state.has_value())
		              {
			              return QueryResult::Error("there is no audio");
		              }
		              size_t playing = 0;
		              if (const auto* registry = Registry(sources.world); registry != nullptr)
		              {
			              registry->Each<const AudioEmitter>([&playing](entt::entity /*entity*/, const AudioEmitter& emitter) {
				              playing += emitter.state == audio::AudioStatus::Playing ? 1 : 0;
			              });
		              }
		              return QueryResult::Value({{"global_volume", state->globalVolume},
		                                         {"sfx_volume", state->sfxVolume},
		                                         {"music_volume", state->musicVolume},
		                                         {"music_active", state->musicActive},
		                                         {"sounds_playing", playing}});
	              });
	provider->Add(Query("sounds", "The sounds about a point, nearest first (sounds without a place are left out)", {},
	                    ResultKind::List, true),
	              [sources](const QueryContext& /*context*/) {
		              const auto* registry = Registry(sources.world);
		              if (registry == nullptr)
		              {
			              return QueryResult::Error(std::string(k_NoRegistry));
		              }
		              Json items = Json::array();
		              registry->Each<const AudioEmitter>([&items](entt::entity entity, const AudioEmitter& emitter) {
			              if (!emitter.spatial)
			              {
				              return;
			              }
			              items.push_back({
			                  {"id", ToId(entity)},
			                  {"position", Point(emitter.position)},
			                  {"sound", emitter.soundId},
			                  {"bank", emitter.bank},
			                  {"state", StatusName(emitter.state)},
			                  {"loop", LoopName(emitter.loop)},
			                  {"volume", emitter.volume},
			                  {"gain", emitter.gain},
			                  {"music", emitter.music},
			                  {"owner", Id(emitter.owner)},
			              });
		              });
		              return QueryResult::Value(std::move(items));
	              });
	return provider;
}
