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
#include <string_view>
#include <utility>

#include <entt/core/hashed_string.hpp>

#include "3D/L3DMesh.h"
#include "3D/TempleInteriorInterface.h"
#include "Debug/TempleVillagersSurvey.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "GameProviderShared.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Temple/TemplePeople.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

// The temple

namespace
{

// The name of a submesh of a temple room's mesh, or null when the room or the submesh is not known
Json TempleSubMeshName(TempleRoom room, std::optional<uint32_t> subMesh)
{
	constexpr std::array<std::string_view, 7> k_RoomMeshes {"challenge", "creature", "credits", "main",
	                                                        "multi",     "options",  "savegame"};
	const auto index = static_cast<size_t>(room);
	if (!subMesh.has_value() || index >= k_RoomMeshes.size() || !Locator::resources::has_value())
	{
		return nullptr;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto id = entt::hashed_string(("temple/interior/" + std::string(k_RoomMeshes[index]) + "_l3d").c_str()).value();
	if (!meshes.Contains(id))
	{
		return nullptr;
	}
	const auto& subMeshes = meshes.Handle(id)->GetSubMeshes();
	return *subMesh < subMeshes.size() ? Json(subMeshes[*subMesh]->GetName()) : Json(nullptr);
}

} // namespace

namespace openblack::inspector::game_providers
{

std::unique_ptr<ProviderInterface> TempleProvider()
{
	auto provider = std::make_unique<FunctionProvider>("temple");
	provider->Add(
	    Query("state", "The temples on the land: their owners, and whether they are being destroyed", {}, ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    registry.Each<const Temple>([&items, &registry](entt::entity entity, const Temple& temple) {
			    auto item = Listed(registry, entity);
			    item["owner"] = static_cast<int>(temple.owner);
			    item["destroying"] = temple.destroying;
			    item["last_hit_turn"] = temple.lastHitTurn;
			    item["has_exterior"] = registry.AllOf<TempleExterior>(entity);
			    items.push_back(std::move(item));
		    });
		    return items;
	    }));
	provider->Add(Query("interior", "The temple's inside: whether the player is in it, the room, the visits, the "
	                                "worshippers walking about it (how many came in, and where those drawn are), the "
	                                "seeds of the miracles over the map, how the map shows the influence and what the cursor "
	                                "is over (the room, its submesh and the submesh's name), and the last press for the camera "
	                                "(camera_press: taken_by none, held_from_outside, cinema_bars, control or fade; null "
	                                "before the first press of the visit)"),
	              Serve<Locator::temple>("the temple", [](const TempleInteriorInterface& temple, const QueryContext& /*c*/) {
		              const auto transition = temple.GetTransitionRoom();
		              Json drawn = Json::array();
		              for (const auto& person : temple.GetPeople())
		              {
			              drawn.push_back(Point(glm::vec3(person.model[3])));
		              }
		              Json seeds = Json::array();
		              for (const auto& seed : temple.GetMapSpellSeeds())
		              {
			              seeds.push_back({{"seed", static_cast<int>(seed.seed)},
			                               {"middle", Point(seed.middle)},
			                               {"spin", seed.spin},
			                               {"phial_frame", seed.phialFrame}});
		              }
		              const auto influence = temple.GetMapInfluence();
		              // What the cursor is over: the room, its submesh and the submesh's name, so that a tool can find a
		              // room's pictures, buttons and doors when it moves the pointer
		              Json cursor = nullptr;
		              if (const auto hit = temple.GetCursorHit(); hit.has_value())
		              {
			              cursor = Json {{"point", Point(hit->point)},
			                             {"room", static_cast<int>(hit->room)},
			                             {"sub_mesh", hit->subMesh.has_value() ? Json(*hit->subMesh) : Json(nullptr)},
			                             {"name", TempleSubMeshName(hit->room, hit->subMesh)}};
		              }
		              // The last press for the camera, and the rule that kept it from the camera, so that a tool knows why
		              // a press did not move it
		              Json press = nullptr;
		              if (const auto last = temple.GetLastCameraPress(); last.has_value() && last->presses > 0)
		              {
			              press = Json {{"taken_by", std::string(temple_press::Name(last->takenBy))},
			                            {"fade", last->fade},
			                            {"looking_at_scroll", last->lookingAtScroll},
			                            {"presses", last->presses}};
		              }
		              return Json {
		                  {"camera_press", std::move(press)},
		                  {"cursor", std::move(cursor)},
		                  {"map_spell_seeds", std::move(seeds)},
		                  {"map_influence", influence.has_value()
		                                        ? Json {{"offset", Point(influence->offset)}, {"scale", influence->scale}}
		                                        : Json(nullptr)},
		                  {"people_inside", temple.GetPeopleInside()},
		                  {"people_drawn", std::move(drawn)},
		                  {"active", temple.Active()},
		                  {"position", Point(temple.GetPosition())},
		                  {"room", static_cast<int>(temple.GetCurrentRoom())},
		                  {"transition_room", transition.has_value() ? Json(static_cast<int>(*transition)) : Json(nullptr)},
		                  {"visits", temple.GetVisits()}};
	              }));
	provider->Add(
	    Query("villagers",
	          "The villagers around the player's temple, as the Temple Villagers debug window shows them: its worship "
	          "sites and the places at them, the towns worshipping there, every villager worshipping, on its way or "
	          "within the radius with its state and walk, and those walking about inside the temple",
	          {{.name = "radius",
	            .type = "number",
	            .description = "Villagers this near the temple's middle, in metres, are listed too (60 if not given)",
	            .required = false},
	           {.name = "player", .type = "integer", .description = "The player, 0 if not given", .required = false}}),
	    [](const QueryContext& context) {
		    const auto player = static_cast<PlayerNames>(context.params.value("player", 0));
		    const auto survey = temple_villagers::Take(player, context.params.value("radius", 60.0f), false);
		    if (survey.temple == entt::null)
		    {
			    return QueryResult::Error("the player has no temple");
		    }
		    Json sites = Json::array();
		    for (const auto& site : survey.sites)
		    {
			    Json places = Json::object();
			    for (size_t i = 0; i < site.places.size(); ++i)
			    {
				    if (const auto& place = site.places.at(i); place.has_value())
				    {
					    places[std::string(temple_villagers::Name(static_cast<temple_villagers::SitePlace>(i)))] = {place->x,
					                                                                                                place->y};
				    }
			    }
			    Json worshippers = Json::array();
			    for (const auto villager : site.worshippers)
			    {
				    worshippers.push_back(Id(villager));
			    }
			    sites.push_back({{"id", Id(site.site)},
			                     {"tribe", static_cast<int>(site.tribe)},
			                     {"place", site.place},
			                     {"built", site.built},
			                     {"dancers", site.dancers},
			                     {"food_in_pot", site.foodInPot},
			                     {"worshippers", std::move(worshippers)},
			                     {"places", std::move(places)}});
		    }
		    Json towns = Json::array();
		    for (const auto& town : survey.towns)
		    {
			    towns.push_back({{"id", Id(town.town)},
			                     {"town_id", town.id},
			                     {"site", Id(town.site)},
			                     {"share", town.share},
			                     {"population", town.population},
			                     {"worshipping", town.worshipping},
			                     {"on_the_way", town.onTheWay},
			                     {"needed", town.needed}});
		    }
		    Json villagers = Json::array();
		    for (const auto& villager : survey.villagers)
		    {
			    Json item {{"id", Id(villager.villager)},
			               {"standing", std::string(temple_villagers::Name(villager.standing))},
			               {"tribe", static_cast<int>(villager.tribe)},
			               {"job", static_cast<int>(villager.number)},
			               {"child", villager.child},
			               {"age", villager.age},
			               {"life", villager.life},
			               {"town", Id(villager.town)},
			               {"site", Id(villager.site)},
			               {"state", static_cast<int>(villager.final)},
			               {"top_state", static_cast<int>(villager.top)},
			               {"turns_in_state", villager.turnsInState},
			               {"position", Point(villager.position)},
			               {"distance", villager.distance}};
			    if (villager.walk.has_value())
			    {
				    const auto& walk = *villager.walk;
				    Json walking {{"goal", {walk.goal.x, walk.goal.y}},
				                  {"move_state", std::string(walk.moveState)},
				                  {"speed", walk.speed}};
				    if (walk.stepGoal.has_value())
				    {
					    walking["step_goal"] = {walk.stepGoal->x, walk.stepGoal->y};
				    }
				    if (walk.circleCentre.has_value())
				    {
					    walking["circle"] = {{"centre", {walk.circleCentre->x, walk.circleCentre->y}},
					                         {"radius", walk.circleRadius}};
				    }
				    item["walk"] = std::move(walking);
			    }
			    villagers.push_back(std::move(item));
		    }
		    Json inside = Json::array();
		    for (const auto& walker : survey.inside.walkers)
		    {
			    inside.push_back({{"villager", Id(walker.villager)},
			                      {"position", Point(walker.position)},
			                      {"target", Point(walker.target)},
			                      {"walking", walker.walking},
			                      {"timer", walker.timer},
			                      {"pause", walker.pause}});
		    }
		    return QueryResult::Value({{"temple", Id(survey.temple)},
		                               {"temple_position", Point(survey.templePosition)},
		                               {"sites", std::move(sites)},
		                               {"towns", std::move(towns)},
		                               {"villagers", std::move(villagers)},
		                               {"in_temple", survey.inside.active},
		                               {"inside", std::move(inside)}});
	    });
	return provider;
}

} // namespace openblack::inspector::game_providers
