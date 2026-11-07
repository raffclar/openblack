/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>

#include <InspectorQuery.h>

#include "ECS/ArtefactRules.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AppleBarrel.h"
#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Football.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/WalkPath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/Workshop.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AbodeSoundSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/AppleBarrelSystemInterface.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/SharkSystemInterface.h"
#include "ECS/Systems/ThingsMadeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "GameProviderShared.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Nature/Fireflies.h"
#include "Online/NetworkSession.h"
#include "Profiler.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

// The things living on the land

namespace openblack::inspector::game_providers
{

std::unique_ptr<ProviderInterface> LivingProvider()
{
	auto provider = std::make_unique<FunctionProvider>("living");
	provider->Add(
	    Query("animated_statics", "The scenery the scripts open and close: its open word and its gate stones' value", {},
	          ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    registry.Each<const AnimatedStatic>([&items, &registry](entt::entity entity, const AnimatedStatic& scenery) {
			    auto item = Listed(registry, entity);
			    item["open_state"] = scenery.openState;
			    if (Locator::animatedStaticSystem::has_value())
			    {
				    item["gate_stones_value"] = Optional(Locator::animatedStaticSystem::value().GateStoneValue(entity));
			    }
			    items.push_back(std::move(item));
		    });
		    return items;
	    }));
	provider->Add(
	    Query("apple_barrel",
	          "The apple barrel beside the storage pit of the player at this computer nearest their temple: the pit (an "
	          "entity of the land), the barrel and its worm (entities of the drawn registry), where they stand and the "
	          "worm's place in its clip; none while no pit carries it",
	          {}),
	    ServeRegistry([](const ecs::Registry& /*registry*/, const QueryContext& /*c*/) {
		    const auto* drawn = DrawnRegistry();
		    const auto barrel =
		        Locator::appleBarrelSystem::has_value() ? Locator::appleBarrelSystem::value().GetBarrel() : entt::null;
		    if (drawn == nullptr || barrel == entt::null || !drawn->Valid(barrel) ||
		        !drawn->AllOf<AppleBarrel, Transform>(barrel))
		    {
			    return Json(nullptr);
		    }
		    const auto& found = drawn->Get<const AppleBarrel>(barrel);
		    Json result = {{"barrel", Id(barrel)},
		                   {"store", Id(found.store)},
		                   {"worm", Id(found.worm)},
		                   {"position", Point(drawn->Get<const Transform>(barrel).position)}};
		    if (drawn->Valid(found.worm) && drawn->AllOf<AppleBarrelWorm, Transform>(found.worm))
		    {
			    result["worm_position"] = Point(drawn->Get<const Transform>(found.worm).position);
			    result["worm_place"] = drawn->Get<const AppleBarrelWorm>(found.worm).place;
		    }
		    return result;
	    }));
	provider->Add(
	    Query("totems",
	          "The village centres' totems: their town centre and icon, the share they stand at, ease to and are "
	          "held at, how fast they move, whether the hand grips one, and their see-through second totem and "
	          "whether it shows",
	          {}, ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    const auto gripped =
		        Locator::villageTotemSystem::has_value() ? Locator::villageTotemSystem::value().GetGripped() : std::nullopt;
		    registry.Each<const VillageTotem>([&items, &registry, gripped](entt::entity entity, const VillageTotem& totem) {
			    auto item = Listed(registry, entity);
			    item["town_centre"] = Id(totem.townCentre);
			    item["icon"] = Id(totem.icon);
			    item["rest_y"] = totem.restY;
			    item["share"] = totem.ease.share;
			    item["speed"] = totem.ease.speed;
			    item["target"] = totem.ease.target;
			    item["moving"] = totem.ease.moving;
			    item["held"] = totem.held;
			    item["gripped"] = gripped.has_value() && *gripped == entity;
			    item["ghost"] = Id(totem.ghost);
			    item["ghost_shown"] = registry.Valid(totem.ghost) && !registry.AllOf<VillageTotemGhostHidden>(totem.ghost);
			    items.push_back(std::move(item));
		    });
		    return items;
	    }));
	provider->Add(
	    Query("artefacts",
	          "The artefacts: the town and worship site each last joined and whether it is among their artefacts now, its "
	          "owner and worth, its dance (started or not, its state, dancers and turns since it started), whether it "
	          "glows and shows its owner's symbol, and how many each player owns",
	          {}, ResultKind::Object),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    registry.Each<const TownArtefact>([&items, &registry](entt::entity entity, const TownArtefact& artefact) {
			    auto item = Listed(registry, entity);
			    item["town"] = Id(artefact.town);
			    item["site"] = Id(artefact.site);
			    item["in_town"] = artefact.inTown;
			    item["in_site"] = artefact.inSite;
			    item["player"] = artefact.player.has_value() ? Json(static_cast<int>(*artefact.player)) : Json(nullptr);
			    item["value"] = artefact.value;
			    item["dance"] = Id(artefact.dance);
			    item["dancing"] = artefact.dancing;
			    item["dance_started"] = artefact.danceStarted;
			    if (const auto* dance = registry.Valid(artefact.dance) ? registry.TryGet<const Dance>(artefact.dance) : nullptr)
			    {
				    item["dance_type"] = static_cast<int>(dance->type);
				    item["dance_state"] = dance->state == Dance::State::Dancing ? "dancing" : "stopped";
				    item["dancers"] = dance->dancers;
				    item["dance_duration"] = dance->duration;
				    item["dance_start_turn"] = dance->startTurn;
			    }
			    item["effect"] = artefact.effect;
			    item["glows"] = ecs::artefact_rules::Glows(artefact.value);
			    item["symbol"] = artefact.symbol && ecs::artefact_rules::ShowsSymbol(artefact.value);
			    items.push_back(std::move(item));
		    });
		    Json counts = Json::object();
		    if (Locator::artefactSystem::has_value())
		    {
			    for (int p = 0; p < static_cast<int>(PlayerNames::_COUNT); ++p)
			    {
				    counts[std::to_string(p)] = Locator::artefactSystem::value().CountFor(static_cast<PlayerNames>(p));
			    }
		    }
		    return QueryResult::Value(Json {{"artefacts", std::move(items)}, {"count_by_player", std::move(counts)}});
	    }));
	provider->Add({.name = "artefact_worth",
	               .description = "Sets an artefact's worth as its dances would raise it: it starts glowing once worth more "
	                              "than 1, and shows its owner's symbol once worth more than 2",
	               .parameters = {IdParameter("The artefact's entity id"),
	                              {.name = "worth", .type = "number", .description = "Its new worth", .required = true},
	                              {.name = "town",
	                               .type = "integer",
	                               .description = "A town to make it an artefact of first, as if put there by the player",
	                               .required = false},
	                              {.name = "player",
	                               .type = "integer",
	                               .description = "The player who puts it in the town (0 by default)",
	                               .required = false}},
	               .kind = ResultKind::Object,
	               .needsNear = false,
	               .writes = true},
	              [](const QueryContext& context) {
		              const auto entity = EntityParam(context.params);
		              const auto worth = context.params.find("worth");
		              if (!entity.has_value() || worth == context.params.end() || !worth->is_number() ||
		                  !Locator::artefactSystem::has_value())
		              {
			              return QueryResult::Error("give an artefact's id and a worth");
		              }
		              // Made an artefact of a town first, when one is named, as if its player had put it there
		              if (const auto town = context.params.find("town"); town != context.params.end())
		              {
			              const auto townEntity = FromId(*town);
			              const auto player = context.params.value("player", 0);
			              if (!townEntity.has_value() || player < 0 || player >= static_cast<int>(PlayerNames::_COUNT))
			              {
				              return QueryResult::Error("give a town's id and a player number");
			              }
			              Locator::artefactSystem::value().AddToTown(*entity, *townEntity, static_cast<PlayerNames>(player));
		              }
		              if (!Locator::artefactSystem::value().SetWorth(*entity, worth->get<float>()))
		              {
			              return QueryResult::Error("it isn't an artefact");
		              }
		              return QueryResult::Value(Json {{"id", Id(*entity)}, {"worth", worth->get<float>()}});
	              });
	provider->Add(Query("action", "A villager's or animal's action states: top, final, previous, and turns in them",
	                    {IdParameter("The living thing's entity id")}),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) {
		              const auto entity = EntityParam(context.params);
		              const auto* action = entity.has_value() ? registry.TryGet<const LivingAction>(*entity) : nullptr;
		              if (action == nullptr)
		              {
			              return QueryResult::Error("it has no living action");
		              }
		              Json result = {{"id", ToId(*entity)},
		                             {"top", action->states[0]},
		                             {"final", action->states[1]},
		                             {"previous", action->states[2]},
		                             {"turns_until_change", action->turnsUntilStateChange},
		                             {"turns_since_change", action->turnsSinceStateChange}};
		              if (Locator::livingActionSystem::has_value())
		              {
			              result["final_state"] =
			                  static_cast<int>(Locator::livingActionSystem::value().VillagerGetFinalState(*action));
		              }
		              return QueryResult::Value(std::move(result));
	              }));
	provider->Add(
	    Query("animal",
	          "An animal: its kind, state (and the game's number of the state it is in for itself), count of turns, "
	          "whether a kill made it start to die, clip, flock and town, its kind's radius, where it moves to, whether "
	          "a creature fears it, the hunger of a kind that is not a grazer, a grazer's needs, and a spell wolf's prey "
	          "and target",
	          {IdParameter("The animal's entity id")}),
	    Serve<Locator::animalSystem>(
	        "the animals", [](const ecs::systems::AnimalSystemInterface& animals, const QueryContext& context) {
		        const auto entity = EntityParam(context.params);
		        const auto* registry = Registry();
		        if (!entity.has_value() || registry == nullptr || !registry->AllOf<Animal>(*entity))
		        {
			        return QueryResult::Error("no animal with that id");
		        }
		        const auto& animal = registry->Get<const Animal>(*entity);
		        Json result = {{"id", ToId(*entity)},
		                       {"kind", static_cast<int>(animal.type)},
		                       {"state", static_cast<int>(animal.state)},
		                       {"final_game_state", animals.FinalGameStateOf(*entity).value_or(0)},
		                       {"turns_in_state", animal.turnsInState},
		                       {"turns_until_state_change", animal.turnsUntilStateChange},
		                       {"dying", animal.dying},
		                       {"hunger", animal.hunger},
		                       {"birth_turn", animal.birthTurn},
		                       {"clip", static_cast<int>(animal.animation)},
		                       {"speed", animal.move.speed},
		                       {"flock", Id(animal.flock)},
		                       {"town", Id(animal.town)},
		                       {"radius", animals.RadiusOf(*entity)},
		                       {"movement", Point(animals.MovementOf(*entity))},
		                       {"goal", Point(animals.GoalOf(*entity))},
		                       {"frightening_to_creature", animals.IsFrighteningToCreature(*entity)},
		                       {"player_can_pick_up", animals.CanPlayerPickUp(*entity)}};
		        // A grazer's needs; the grazing clips left of its meal are its count of turns
		        if (const auto* grazer = registry->TryGet<const Grazer>(*entity))
		        {
			        result["grazer"] = {
			            {"hunger", grazer->needs.hunger}, {"sleep", grazer->needs.sleep}, {"breed", grazer->needs.breed}};
		        }
		        // A spell wolf's prey that it hunts, and its target: the prey that it brought down and eats
		        if (const auto* run = registry->TryGet<const SpellWolf>(*entity))
		        {
			        result["spell_wolf"] = {{"prey", Id(run->hunt.prey)}, {"food", Id(run->hunt.food)}};
		        }
		        // A wild hunter's needs, the prey that it hunts, its target and the cell where its flock sleeps
		        if (const auto* hunter = registry->TryGet<const Hunter>(*entity))
		        {
			        result["hunter"] = {{"hunger", hunter->needs.hunger},
			                            {"sleep", hunter->needs.sleep},
			                            {"breed", hunter->needs.breed},
			                            {"prey", Id(hunter->hunt.prey)},
			                            {"food", Id(hunter->hunt.food)},
			                            {"sleeping_cell", hunter->sleepingCell.has_value()
			                                                  ? Json::array({hunter->sleepingCell->x, hunter->sleepingCell->y})
			                                                  : Json()}};
		        }
		        return QueryResult::Value(std::move(result));
	        }));
	provider->Add(
	    Query("villages", "Whether it is dark enough for the village lights, and how many there are"),
	    Serve<Locator::villageLightSystem>(
	        "the village lights", [](const ecs::systems::VillageLightSystemInterface& lights, const QueryContext& /*c*/) {
		        const auto* registry = Registry();
		        return Json {{"dark", lights.IsDark()}, {"lights", registry != nullptr ? CountOf<VillageLight>(*registry) : 0}};
	        }));
	provider->Add(
	    Query("abode_sounds", "How many sounds the buildings of the towns made by themselves"),
	    Serve<Locator::abodeSoundSystem>("the sounds of the buildings",
	                                     [](const ecs::systems::AbodeSoundSystemInterface& sounds, const QueryContext& /*c*/) {
		                                     return Json {{"sounds", sounds.GetSoundCount()}};
	                                     }));
	provider->Add(
	    Query("footpaths",
	          "The footpaths: how many there are and how many are being found; with an id, the paths that lead from that "
	          "thing and the villagers walking them; with from and to (metres), the nodes of a path the game would lay at once "
	          "between them",
	          {{.name = "id", .type = "integer", .description = "A thing's entity id", .required = false},
	           {.name = "from", .type = "point", .description = "[x, z] in metres", .required = false},
	           {.name = "to", .type = "point", .description = "[x, z] in metres", .required = false}}),
	    Serve<Locator::footpathSystem>("the footpaths", [](const ecs::systems::FootpathSystemInterface& footpaths,
	                                                       const QueryContext& context) {
		    const auto* registry = Registry();
		    Json result {{"footpaths", CountOf<Footpath>(*registry)}, {"finders", CountOf<FootpathFinder>(*registry)}};
		    const auto from = context.params.find("from");
		    const auto to = context.params.find("to");
		    if (from != context.params.end() && to != context.params.end())
		    {
			    const auto a = ReadPoint(*from);
			    const auto b = ReadPoint(*to);
			    if (!a.has_value() || !b.has_value())
			    {
				    return QueryResult::Error("from and to must be points");
			    }
			    const auto whole = [](const auto& point) {
				    return glm::ivec2(map_coords::ToFixed(static_cast<float>(point[0])),
				                      map_coords::ToFixed(static_cast<float>(point[2])));
			    };
			    const auto planned = footpaths.PlanBetween(whole(*a), whole(*b));
			    Json nodes = Json::array();
			    for (const auto& place : planned.value_or(std::vector<glm::ivec2> {}))
			    {
				    nodes.push_back({map_coords::ToMetres(place.x), map_coords::ToMetres(place.y)});
			    }
			    result["planned"] = planned.has_value() ? nodes : Json(nullptr);
		    }
		    if (const auto entity = EntityParam(context.params))
		    {
			    Json paths = Json::array();
			    if (const auto* link = registry->TryGet<const FootpathLink>(*entity))
			    {
				    for (const auto id : link->footpaths)
				    {
					    const auto path = static_cast<entt::entity>(id);
					    const auto* footpath = registry->Valid(path) ? registry->TryGet<const Footpath>(path) : nullptr;
					    if (footpath == nullptr)
					    {
						    continue;
					    }
					    Json nodes = Json::array();
					    for (const auto& node : footpath->nodes)
					    {
						    nodes.push_back({{"place", {node.place.x, node.place.y}},
						                     {"planned", node.planned},
						                     {"hidden", node.hidden},
						                     {"marker", node.marker}});
					    }
					    // The villagers that walk the path, each with the node it walks to
					    Json walkers = Json::array();
					    registry->Each<const FootpathWalk>([&walkers, path](entt::entity villager, const FootpathWalk& walk) {
						    if (walk.footpath == path)
						    {
							    walkers.push_back({{"id", ToId(villager)},
							                       {"node", walk.node.has_value() ? Json(*walk.node) : Json(nullptr)},
							                       {"towardsStart", walk.towardsStart}});
						    }
					    });
					    paths.push_back({{"id", ToId(path)}, {"nodes", std::move(nodes)}, {"walkers", std::move(walkers)}});
				    }
			    }
			    result["paths"] = std::move(paths);
		    }
		    return QueryResult::Value(std::move(result));
	    }));
	provider->Add(
	    Query("field", "A field's crop as it is drawn: its tint, height and sway", {IdParameter("The field's entity id")}),
	    Serve<Locator::fieldSystem>(
	        "the fields", [](const ecs::systems::FieldSystemInterface& fields, const QueryContext& context) {
		        const auto entity = EntityParam(context.params);
		        const auto* field = entity.has_value() ? Registry()->TryGet<const Field>(*entity) : nullptr;
		        if (field == nullptr)
		        {
			        return QueryResult::Error("no field with that id");
		        }
		        const auto look = fields.GetLook(*field);
		        if (!look.has_value())
		        {
			        return QueryResult::Value({{"id", ToId(*entity)}, {"crop", nullptr}});
		        }
		        return QueryResult::Value(
		            {{"id", ToId(*entity)}, {"tint", look->tint}, {"height", look->height}, {"sways", look->sways}});
	        }));
	provider->Add(Query("forests", "The land's forests: their wood and trees", {}, ResultKind::List),
	              Serve<Locator::forestSystem>(
	                  "the forests", [](const ecs::systems::ForestSystemInterface& forests, const QueryContext& /*c*/) {
		                  const auto* registry = Registry();
		                  Json items = Json::array();
		                  for (const auto forest : forests.LandForests())
		                  {
			                  auto item = registry != nullptr ? Listed(*registry, forest) : Json {{"id", ToId(forest)}};
			                  item["wood"] = forests.WoodOf(forest);
			                  item["trees"] = forests.TreesOf(forest, false).size();
			                  item["growing"] = forests.TreesOf(forest, true).size();
			                  items.push_back(std::move(item));
		                  }
		                  return items;
	                  }));
	provider->Add(Query("sharks", "The sharks: where each is this turn and was at its start, the way it faces, its clip "
	                              "and the wake's timer"),
	              Serve<Locator::sharkSystem>(
	                  "the sharks", [](const ecs::systems::SharkSystemInterface& sharks, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  if (const auto* registry = Registry(); registry != nullptr)
		                  {
			                  registry->Each<const Shark>([&items, registry](entt::entity entity, const Shark& shark) {
				                  auto item = Listed(*registry, entity);
				                  item["at"] = Point(shark.position);
				                  item["turn_start"] = Point(shark.turnStart);
				                  item["heading"] = shark.heading;
				                  item["clip_place"] = shark.clipPlace;
				                  items.push_back(std::move(item));
			                  });
		                  }
		                  return Json {{"wake_timer", sharks.GetWakeTimer()}, {"sharks", std::move(items)}};
	                  }));
	provider->Add(Query("intro", "The opening's light falling onto the boy and the god's hand lifting him and setting him "
	                             "down: the stage, the light's state and head, the chasing camera, the hand's clip time and "
	                             "where it holds the boy"),
	              Serve<Locator::introSystem>(
	                  "the opening's light and hand", [](const ecs::systems::IntroSystemInterface& intro, const QueryContext&) {
		                  const auto state = intro.GetState();
		                  Json json {
		                      {"stage", state.stage},
		                      {"light", state.light.has_value() ? Json(static_cast<int32_t>(*state.light)) : Json(nullptr)},
		                      {"light_head", state.lightHead.has_value() ? Point(*state.lightHead) : Json(nullptr)},
		                      {"light_elapsed", state.lightElapsed},
		                      {"camera_chasing", state.cameraChasing},
		                      {"hand", state.hand == entt::null ? Json(nullptr) : Json(ToId(state.hand))},
		                      {"hand_clip_time", state.handClipTime},
		                      {"hand_playing", state.handPlaying},
		                      {"holding_boy", state.holdingBoy},
		                      {"grip", state.grip.has_value() ? Point(*state.grip) : Json(nullptr)},
		                      {"lift_finished", state.liftFinished}};
		                  if (const auto view = intro.GetCameraView(); view.has_value())
		                  {
			                  json["camera"] = {{"origin", Point(view->origin)}, {"focus", Point(view->focus)}};
		                  }
		                  return json;
	                  }));
	provider->Add(
	    Query("boat_launch", "The explorers' boat film: the stage, its clocks, where it plays, where the boat is drawn and "
	                         "the things drawn in it with their clips' places"),
	    Serve<Locator::boatLaunchSystem>(
	        "the explorers' boat", [](const ecs::systems::BoatLaunchSystemInterface& film, const QueryContext&) {
		        const auto state = film.GetState();
		        Json wakes = Json::array();
		        for (const auto phase : state.clocks.wakes)
		        {
			        wakes.push_back(phase);
		        }
		        Json parts = Json::array();
		        if (const auto* registry = DrawnRegistry(); registry != nullptr)
		        {
			        // Only the things of the film that the frames show now, in the drawn registry
			        registry->Each<const BoatFilmPart, const Transform>(
			            [&parts, registry](entt::entity entity, const BoatFilmPart& part, const Transform& transform) {
				            if (registry->AllOf<HiddenByState>(entity))
				            {
					            return;
				            }
				            auto item = Listed(*registry, entity);
				            item["role"] = static_cast<int32_t>(part.role);
				            item["index"] = part.index;
				            item["at"] = Point(transform.position);
				            item["clip_time"] = part.clipTime;
				            parts.push_back(std::move(item));
			            });
		        }
		        Json json {{"playing", state.playing},
		                   {"stage", state.clocks.stage == ecs::boat_launch::Stage::Launch ? "launch" : "sailing"},
		                   {"elapsed", state.clocks.elapsed},
		                   {"boat_time", state.clocks.boatTime},
		                   {"pushing", state.clocks.pushing},
		                   {"wakes", std::move(wakes)},
		                   {"dock", Point(state.places.dock)},
		                   {"sailing_start", Point(state.places.sailingStart)},
		                   {"parts", std::move(parts)}};
		        if (state.boat.has_value())
		        {
			        json["boat"] = Point(glm::vec3((*state.boat)[3]));
		        }
		        return json;
	        }));
	provider->Add(Query("walk_paths", "The things walking the camera editor's tracks: the track, how far and up to where", {},
	                    ResultKind::List),
	              Serve<Locator::walkPathSystem>(
	                  "the walk paths", [](const ecs::systems::WalkPathSystemInterface& /*paths*/, const QueryContext&) {
		                  Json items = Json::array();
		                  if (const auto* registry = Registry(); registry != nullptr)
		                  {
			                  registry->Each<const WalkPath>([&items, registry](entt::entity entity, const WalkPath& path) {
				                  auto item = Listed(*registry, entity);
				                  item["track"] = path.number;
				                  item["forward"] = path.walk.forward;
				                  item["current"] = path.walk.current;
				                  item["to"] = path.walk.to;
				                  item["percentage"] = camera_track::Percentage(path.walk, *path.track);
				                  item["segment"] = path.walk.runner.GetSegment();
				                  items.push_back(std::move(item));
			                  });
		                  }
		                  return items;
	                  }));
	provider->Add(
	    Query("football",
	          "The football of the Soccer Add-On: whether the game plays it, and each pitch's match state, score, ball, "
	          "referee, teams, crowd and wave",
	          {}, ResultKind::Object),
	    Serve<Locator::footballSystem>(
	        "the football", [](const ecs::systems::FootballSystemInterface& football, const QueryContext&) {
		        Json pitches = Json::array();
		        if (const auto* registry = Registry(); registry != nullptr)
		        {
			        registry->Each<const ecs::components::FootballPitch>(
			            [&pitches, registry](entt::entity entity, const ecs::components::FootballPitch& pitch) {
				            auto item = Listed(*registry, entity);
				            const auto ids = [](const std::vector<entt::entity>& team) {
					            Json list = Json::array();
					            for (const auto villager : team)
					            {
						            list.push_back(ToId(villager));
					            }
					            return list;
				            };
				            item["functional"] = pitch.functional;
				            item["state"] = static_cast<int>(pitch.match.state);
				            item["turns"] = pitch.match.turns;
				            item["score"] = {pitch.match.score.at(0), pitch.match.score.at(1)};
				            item["town_match"] = pitch.match.townMatch;
				            item["ball"] = ToId(pitch.ball);
				            item["referee"] = ToId(pitch.referee);
				            item["home"] = ids(pitch.home);
				            item["away"] = ids(pitch.away);
				            item["crowd_places"] = pitch.crowdPlaces;
				            item["wave_next"] = pitch.wave.next;
				            item["wave_rest"] = pitch.wave.rest;
				            item["dead_ball_fetcher"] = ToId(pitch.deadBallFetcher);
				            pitches.push_back(std::move(item));
			            });
		        }
		        return Json {{"on", football.IsOn()}, {"pitches", std::move(pitches)}};
	        }));
	provider->Add(
	    Query("dances",
	          "The dances: what they are danced for, whether dancing, their speed and clock, and each group's shape, move "
	          "and dancers",
	          {}, ResultKind::List),
	    Serve<Locator::danceSystem>("the dances", [](const ecs::systems::DanceSystemInterface& /*dances*/,
	                                                 const QueryContext&) {
		    Json items = Json::array();
		    if (const auto* registry = Registry(); registry != nullptr)
		    {
			    registry->Each<const Dance>([&items, registry](entt::entity entity, const Dance& dance) {
				    auto item = Listed(*registry, entity);
				    item["type"] = static_cast<int>(dance.type);
				    item["owner"] = ToId(dance.owner);
				    item["dancing"] = dance.state == Dance::State::Dancing;
				    item["speed"] = dance.speed;
				    item["rate"] = dance.rate;
				    item["clock"] = dance.clock;
				    item["start_turn"] = dance.startTurn;
				    item["duration"] = dance.duration;
				    item["dancers"] = dance.dancers;
				    item["round"] = dance.groups.round;
				    Json groups = Json::array();
				    for (const auto& group : dance.groups.all)
				    {
					    Json dancers = Json::array();
					    for (const auto dancer : group.dancers)
					    {
						    dancers.push_back(ToId(dancer));
					    }
					    groups.push_back({{"name", group.name},
					                      {"limited", group.limited},
					                      {"quota", group.quota},
					                      {"weight", group.weight},
					                      {"sexes", group.sexes},
					                      {"shape", group.shape},
					                      {"radius", group.radius},
					                      {"offset", {group.offset.x, group.offset.y}},
					                      {"rotation", group.rotation},
					                      {"spin", group.spin},
					                      {"move", {group.move.action, group.move.first, group.move.second, group.move.end}},
					                      {"moved", group.moved},
					                      {"dancers", std::move(dancers)}});
				    }
				    item["groups"] = std::move(groups);
				    items.push_back(std::move(item));
			    });
		    }
		    return items;
	    }));
	provider->Add(Query("flocks",
	                    "The flocks, of animals and of the scripts' villagers: their place, their domain and flock distances, "
	                    "and their members from the first to the leader",
	                    {}, ResultKind::List),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              Json items = Json::array();
		              registry.Each<const Flock>([&items](entt::entity entity, const Flock& flock) {
			              Json members = Json::array();
			              for (const auto member : flock.members)
			              {
				              members.push_back(ToId(member));
			              }
			              const auto place = map_coords::ToMetres(flock.place);
			              items.push_back({{"id", ToId(entity)},
			                               {"place", {place.x, place.y}},
			                               {"domain_radius", flock.domainRadius},
			                               {"flock_distance", flock.flockDistance},
			                               {"calm", flock.calm},
			                               {"members", std::move(members)}});
		              });
		              return items;
	              }));
	provider->Add(
	    Query("fireflies",
	          "How many fireflies there are and how many hide, fly out, hover and fly home; whether the next nightfall "
	          "tops them up; the trees and the things standing over several cells they can hide in; and what the last "
	          "frame did with those out: in view, near enough to be seen, drawn, and whether their picture was found; and the "
	          "land's reward weights by magic type number, with their total"),
	    Serve<Locator::fireflySystem>(
	        "the fireflies", [](const ecs::systems::FireflySystemInterface& fireflies, const QueryContext& /*c*/) {
		        std::array<size_t, 4> states {};
		        size_t hidden = 0;
		        if (const auto* registry = Registry(); registry != nullptr)
		        {
			        for (const auto entity : fireflies.GetFireflies())
			        {
				        const auto& firefly = registry->Get<const Firefly>(entity);
				        ++states.at(static_cast<size_t>(firefly.state));
				        hidden += firefly.hidden ? 1 : 0;
			        }
		        }
		        const auto frame = fireflies.GetLastFrame();
		        // The land's reward weights that are not nothing, by the number of the magic type
		        Json weights = Json::object();
		        const auto& rewards = fireflies.GetRewards();
		        for (size_t kind = 0; kind < fireflies::k_RewardKinds; ++kind)
		        {
			        if (rewards.Weight(kind) != 0.0f)
			        {
				        weights[std::to_string(kind)] = rewards.Weight(kind);
			        }
		        }
		        return Json {{"fireflies", fireflies.GetFireflies().size()},
		                     {"reward_weights", std::move(weights)},
		                     {"reward_total", rewards.Total()},
		                     {"resting", states.at(static_cast<size_t>(Firefly::State::Resting))},
		                     {"flying_out", states.at(static_cast<size_t>(Firefly::State::FlyingOut))},
		                     {"hovering", states.at(static_cast<size_t>(Firefly::State::Hovering))},
		                     {"flying_home", states.at(static_cast<size_t>(Firefly::State::FlyingHome))},
		                     {"hidden", hidden},
		                     {"top_up_due", fireflies.IsTopUpDue()},
		                     {"trees", fireflies.GetTreeCount()},
		                     {"fixed", fireflies.GetFixedCount()},
		                     {"last_frame",
		                      {{"out", frame.out},
		                       {"in_view", frame.inView},
		                       {"in_reach", frame.inReach},
		                       {"drawn", frame.drawn},
		                       {"picture_ready", frame.pictureReady}}}};
	        }));
	provider->Add(Query("fish_farms", "The fish farms: their town, fish left, fishermen and shoal", {}, ResultKind::List),
	              Serve<Locator::fishFarmSystem>(
	                  "the fish farms", [](const ecs::systems::FishFarmSystemInterface& farms, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  const auto* registry = Registry();
		                  if (registry == nullptr)
		                  {
			                  return items;
		                  }
		                  registry->Each<const FishFarm>([&](entt::entity entity, const FishFarm& farm) {
			                  auto item = Listed(*registry, entity);
			                  item["town"] = farm.town == entt::null ? Json(nullptr) : Json(ToId(farm.town));
			                  item["fish_left"] = farms.FishLeft(entity);
			                  item["fishermen"] = farm.fishermen.size();
			                  item["shoal"] = farm.shoal.has_value() ? Point(farm.shoal->centre) : Json(nullptr);
			                  items.push_back(std::move(item));
		                  });
		                  return items;
	                  }));
	provider->Add(Query("workshops",
	                    "The workshops: their room for scaffolds, the turns left on a scaffold, and the scaffolds they count",
	                    {}, ResultKind::List),
	              Serve<Locator::workshopSystem>(
	                  "the workshops", [](const ecs::systems::WorkshopSystemInterface& workshops, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  const auto* registry = Registry();
		                  if (registry == nullptr)
		                  {
			                  return items;
		                  }
		                  registry->Each<const Workshop>([&](entt::entity entity, const Workshop& workshop) {
			                  auto item = Listed(*registry, entity);
			                  item["room"] = workshops.RoomOf(entity);
			                  item["making_turns"] = workshop.makingTurns;
			                  item["counted"] = workshop.counted;
			                  Json places = Json::array();
			                  for (const auto place : workshop.places)
			                  {
				                  places.push_back(static_cast<int>(place));
			                  }
			                  item["places"] = std::move(places);
			                  Json scaffolds = Json::array();
			                  for (const auto scaffold : workshop.scaffolds)
			                  {
				                  scaffolds.push_back(ToId(scaffold));
			                  }
			                  item["scaffolds"] = std::move(scaffolds);
			                  items.push_back(std::move(item));
		                  });
		                  return items;
	                  }));
	provider->Add(
	    Query("animated_statics",
	          "The scenery the scripts open and close (gates, the gate stone plinth, the piper's cave, the phone box): "
	          "its open word, place in its clip and resting place, whether it is in the land's draw list and was on "
	          "screen, its gate stones' value and the circles a creature's route goes round",
	          {}, ResultKind::List),
	    Serve<Locator::animatedStaticSystem>(
	        "the animated scenery", [](const ecs::systems::AnimatedStaticSystemInterface& scenery, const QueryContext& /*c*/) {
		        Json items = Json::array();
		        const auto* registry = Registry();
		        if (registry == nullptr)
		        {
			        return items;
		        }
		        registry->Each<const AnimatedStatic>([&](entt::entity entity, const AnimatedStatic& still) {
			        auto item = Listed(*registry, entity);
			        item["type"] = static_cast<int>(still.type);
			        item["open"] = still.openState;
			        if (const auto* pose = registry->TryGet<const AnimatedStaticPose>(entity); pose != nullptr)
			        {
				        item["place"] = pose->place;
				        item["resting_place"] = pose->restingPlace;
				        item["in_draw_list"] = pose->inDrawList;
				        item["on_screen"] = pose->onScreen;
				        item["stones_drawn"] = pose->stones.size();
			        }
			        item["stone_value"] = scenery.GateStoneValue(entity).value_or(0);
			        const auto circles = scenery.RouteCircles(entity);
			        item["route_circles"] = circles.has_value() ? Json(circles->size()) : Json(nullptr);
			        items.push_back(std::move(item));
		        });
		        return items;
	        }));
	provider->Add(Query("chimneys", "The chimneys smoking", {}, ResultKind::List),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              Json items = Json::array();
		              registry.Each<const ChimneySmoke>([&items](entt::entity entity, const ChimneySmoke& smoke) {
			              items.push_back({{"id", ToId(entity)},
			                               {"position", Point(smoke.chimney)},
			                               {"state", static_cast<int>(smoke.state)},
			                               {"system", Locator::chimneySmokeSystem::has_value()}});
		              });
		              return items;
	              }));
	provider->Add(Query("things_made",
	                    "The count of the things of the world made since the land was loaded: the place in the order of "
	                    "the next thing made",
	                    {}, ResultKind::Object),
	              Serve<Locator::thingsMadeSystem>("thingsMadeSystem",
	                                               [](const ecs::systems::ThingsMadeSystemInterface& count,
	                                                  const QueryContext& /*c*/) { return Json {{"made", count.Made()}}; }));
	provider->Add(
	    Query("sound_tags",
	          "The sounds tagged to things: sound, whether active, its emitter, the object it is attached to, whether it loops",
	          {}, ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    registry.Each<const SoundTag>([&items, &registry](entt::entity entity, const SoundTag& tag) {
			    auto item = Listed(registry, entity);
			    item["sound"] = tag.sound;
			    item["active"] = tag.active;
			    item["emitter"] = Id(tag.emitter);
			    item["owner"] = Id(tag.owner);
			    item["loops"] = tag.loops;
			    items.push_back(std::move(item));
		    });
		    return items;
	    }));
	provider->Add(Query("rewards", "The rewards on their way to the land, lying on it, or opened", {}, ResultKind::List),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              Json items = Json::array();
		              registry.Each<const Reward>([&items, &registry](entt::entity entity, const Reward& reward) {
			              auto item = Listed(registry, entity);
			              item["type"] = static_cast<int>(reward.type);
			              item["state"] = static_cast<int>(reward.state);
			              item["opened"] = reward.opened;
			              item["turns_left"] = reward.turnsLeft;
			              item["clip_place"] = reward.clipPlace;
			              item["landing_point"] = Point(reward.landingPoint);
			              items.push_back(std::move(item));
		              });
		              return items;
	              }));
	provider->Add(Query("pathfinding", "The walkers, and the circles of the things in their way they head for or go round"),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              size_t walkers = 0;
		              registry.Each<const WallHug>([&walkers](entt::entity, const WallHug&) { ++walkers; });
		              Json heading = Json::array();
		              registry.Each<const WallHugObjectReference>(
		                  [&heading, &registry](entt::entity entity, const WallHugObjectReference& reference) {
			                  auto item = Listed(registry, entity);
			                  item["obstacle"] = Id(reference.entity);
			                  item["centre"] = Point(reference.centre);
			                  item["radius"] = reference.radius;
			                  item["steps_away"] = reference.stepsAway;
			                  heading.push_back(std::move(item));
		                  });
		              return Json {{"walkers", walkers},
		                           {"heading_for", std::move(heading)},
		                           {"system", Locator::pathfindingSystem::has_value()}};
	              }));
	provider->Add(Query("resource", "What a thing holds as a resource, and the store a pile belongs to",
	                    {IdParameter("The thing's entity id")}),
	              Serve<Locator::resourceStoreSystem>(
	                  "the stores", [](const ecs::systems::ResourceStoreSystemInterface& stores, const QueryContext& context) {
		                  const auto entity = EntityParam(context.params);
		                  if (!entity.has_value())
		                  {
			                  return QueryResult::Error("no entity with that id");
		                  }
		                  const auto resource = stores.ResourceOf(*entity);
		                  return QueryResult::Value({{"id", ToId(*entity)},
		                                             {"type", static_cast<int>(resource.type)},
		                                             {"amount", resource.amount},
		                                             {"poisoned", resource.poisoned},
		                                             {"store", Id(stores.StoreOf(*entity))}});
	                  }));
	provider->Add(
	    Query("building", "How much of a building is drawn while it is built or broken",
	          {IdParameter("The building's entity id")}),
	    Serve<Locator::buildingDamageSystem>(
	        "the building damage", [](const ecs::systems::BuildingDamageSystemInterface& damage, const QueryContext& context) {
		        const auto entity = EntityParam(context.params);
		        if (!entity.has_value())
		        {
			        return QueryResult::Error("no entity with that id");
		        }
		        return QueryResult::Value({{"id", ToId(*entity)},
		                                   {"partial_share", Optional(damage.PartialShare(*entity))},
		                                   {"draws_whole", damage.DrawsWhole(*entity)}});
	        }));
	provider->Add(
	    Query("town_desires", "What a town wants most, and how much it wants each thing",
	          {IdParameter("The town's entity id")}),
	    Serve<Locator::townDesireSystem>("the town desires", [](const ecs::systems::TownDesireSystemInterface& desires,
	                                                            const QueryContext& context) {
		    const auto entity = EntityParam(context.params);
		    if (!entity.has_value())
		    {
			    return QueryResult::Error("no entity with that id");
		    }
		    Json each = Json::object();
		    for (int i = 0; i <= static_cast<int>(TownDesireInfo::ForSleep); ++i)
		    {
			    const auto desire = static_cast<TownDesireInfo>(i);
			    each[std::to_string(i)] = desires.GetDesire(*entity, desire);
		    }
		    Json raw = Json::object();
		    Json doing = Json::object();
		    const auto* townDesire = Locator::entitiesRegistry::value().TryGet<const ecs::components::TownDesire>(*entity);
		    for (int i = 0; i <= static_cast<int>(TownDesireInfo::ForSleep); ++i)
		    {
			    raw[std::to_string(i)] = desires.GetRawDesire(*entity, static_cast<TownDesireInfo>(i));
			    if (townDesire != nullptr)
			    {
				    doing[std::to_string(i)] = townDesire->doingNow.at(static_cast<size_t>(i));
			    }
		    }
		    // The wood the town wants before it sends villagers for more: the targets grow past a number of abodes
		    Json wood = Json::object();
		    if (Locator::infoConstants::has_value())
		    {
			    const auto& town = Locator::infoConstants::value().town;
			    const auto& multipliers =
			        Locator::infoConstants::value().townDesire.at(static_cast<size_t>(TownDesireInfo::ForWood)).tribeMultiplier;
			    wood = {{"minimum_for_desire", town.minimumWoodForDesire},
			            {"maximum_for_desire", town.maximumWoodForDesire},
			            {"buildings_for_desired_wood", town.numOfBuildingsForDesiredWood},
			            {"tribe_multipliers", std::vector<float>(multipliers.begin(), multipliers.end())}};
		    }
		    return QueryResult::Value({{"id", ToId(*entity)},
		                               {"most_wanted", static_cast<int>(desires.GetMostWanted(*entity))},
		                               {"desires", std::move(each)},
		                               {"raw", std::move(raw)},
		                               {"doing_now", std::move(doing)},
		                               {"wood_targets", std::move(wood)}});
	    }));
	return provider;
}

} // namespace openblack::inspector::game_providers
