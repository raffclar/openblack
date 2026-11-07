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

#include <fmt/format.h>

#include "3D/TempleInteriorInterface.h"
#include "Camera/CameraZones.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/LeashOrders.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/TempleVillagersSurvey.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLeashRope.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Components/Workshop.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePenSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/IncomingMessageSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "GameProviderShared.h"
#include "Gui/ScreenFade.h"
#include "Gui/TextDatabase.h"
#include "Help/HelpProfile.h"
#include "Help/Spirits.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

// The creatures' other systems

namespace
{

/// The tattoo places on the cave's creature as the tattoo editor shows them, and the place under the pointer
void AddTattooSites(Json& cave, const ecs::systems::CreatureCaveSystemInterface& system)
{
	Json sites = Json::array();
	for (const auto& site : system.GetTattooSitesOnScreen())
	{
		sites.push_back(
		    {{"site", site.site}, {"screen", {site.screen.x, site.screen.y}}, {"facing_camera", site.facingCamera}});
	}
	cave["tattoo_sites"] = std::move(sites);
	if (Locator::tattooEditorSystem::has_value())
	{
		cave["tattoo_hovered"] = Optional(Locator::tattooEditorSystem::value().GetHoveredSite());
	}
}

} // namespace

namespace openblack::inspector::game_providers
{

std::unique_ptr<ProviderInterface> CreaturesProvider()
{
	auto provider = std::make_unique<FunctionProvider>("creatures");
	provider->Add(Query("systems", "The creature systems' own state: the creature followed or in the cave, the hand on a "
	                               "creature, the hair, voices, footprints, physiology, the mind kept between lands and the "
	                               "web page"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::creatureModeSystem::has_value())
		              {
			              const auto& mode = Locator::creatureModeSystem::value();
			              result["mode"] = {{"active", mode.IsActive()}, {"creature", Id(mode.GetCreature())}};
		              }
		              if (Locator::creatureCaveSystem::has_value())
		              {
			              const auto& cave = Locator::creatureCaveSystem::value();
			              result["cave"] = {{"open", cave.IsOpen()},
			                                {"in_temple", cave.InTemple()},
			                                {"creature", Id(cave.GetCreature())},
			                                {"creature_shown", cave.IsCreatureShown()},
			                                {"copy", Id(cave.GetCaveCreature())}};
			              // The copy of the creature by the cave's fire, and what it is doing
			              const auto copy = cave.GetCaveCreature();
			              if (copy.has_value() && Locator::temple::has_value() &&
			                  Locator::temple::value().GetRooms().Valid(*copy))
			              {
				              // The copy is a thing of the temple's rooms
				              const auto& registry = Locator::temple::value().GetRooms();
				              const auto* self = registry.TryGet<const ecs::components::CreatureCopy>(*copy);
				              const auto* state = registry.TryGet<const ecs::components::CaveCreature>(*copy);
				              const auto* transform = registry.TryGet<const ecs::components::Transform>(*copy);
				              const auto* animation = registry.TryGet<const ecs::components::CreatureAnimation>(*copy);
				              if (self != nullptr && state != nullptr && transform != nullptr && animation != nullptr)
				              {
					              const auto& s = state->state;
					              result["cave"]["copy_state"] = {
					                  {"size", self->size},
					                  {"position", Point(transform->position)},
					                  {"heading", s.heading},
					                  {"rest", s.rest == cave_creature::Rest::Sleep ? "sleep" : "sit"},
					                  {"turning", s.turn.has_value()},
					                  {"wanted_heading", s.wantedHeading.has_value() ? Json(*s.wantedHeading) : Json(nullptr)},
					                  {"zoomed", s.zoomed},
					                  {"body_animation", creature_layers::IsPlaying(animation->body)
					                                         ? Json(creature_layers::CurrentAnimation(animation->body))
					                                         : Json(nullptr)},
					                  {"body_time_ms", animation->body.timeMs},
					                  {"mirrored", animation->body.mirrored},
					                  {"gesture", animation->gesture.animation.has_value() ? Json(*animation->gesture.animation)
					                                                                       : Json(nullptr)},
					                  {"looks_at", animation->lookAt.has_value() ? Point(*animation->lookAt) : Json(nullptr)},
					                  {"light", Point(state->light)},
					                  {"fidgets", s.fidgets},
					                  {"face", Optional(animation->face.current)},
					              };
					              AddTattooSites(result["cave"], cave);
				              }
			              }
		              }
		              if (Locator::creatureHandSystem::has_value())
		              {
			              const auto& hand = Locator::creatureHandSystem::value();
			              result["hand"] = {{"creature", Id(hand.GetCreature())},
			                                {"under_cursor", Id(hand.CreatureUnderCursor())},
			                                {"feedback_sum", hand.GetFeedbackSum()}};
		              }
		              if (Locator::creatureHairSystem::has_value())
		              {
			              result["hair_shown"] = Locator::creatureHairSystem::value().IsShown();
		              }
		              if (Locator::creatureAudioSystem::has_value())
		              {
			              result["voices_muted"] = Locator::creatureAudioSystem::value().IsMuted();
		              }
		              if (Locator::footprintSystem::has_value())
		              {
			              result["footprints"] = Locator::footprintSystem::value().GetPrints().size();
		              }
		              if (Locator::creaturePhysiologySystem::has_value())
		              {
			              result["physiology_time_scale"] = Locator::creaturePhysiologySystem::value().GetTimeScale();
			              result["fainting"] = Locator::creaturePhysiologySystem::value().IsFaintingEnabled();
		              }
		              if (Locator::creatureFightSystem::has_value())
		              {
			              result["players_fighter"] = Id(Locator::creatureFightSystem::value().PlayersFighter());
		              }
		              if (Locator::creatureCarryOverSystem::has_value())
		              {
			              result["mind_kept"] = Locator::creatureCarryOverSystem::value().Kept() != nullptr;
		              }
		              // The web page: the captions of the five slots of pictures, and whether a picture waits for its frame
		              if (Locator::creatureWebPageSystem::has_value())
		              {
			              const auto& page = Locator::creatureWebPageSystem::value();
			              auto captions = Json::array();
			              for (const auto& caption : page.GetCaptions())
			              {
				              captions.push_back(gui::ToUtf8(caption));
			              }
			              result["web_page_captions"] = captions;
			              result["web_page_picture_waiting"] = page.AnyPictureWaiting();
		              }
		              // The temples' pens: each creature's shrunk size there is its Creature component's penSize
		              result["pens"] = Locator::creaturePenSystem::has_value();
		              if (Locator::creatureFizzSystem::has_value())
		              {
			              const auto scroll = Locator::creatureFizzSystem::value().EyeStaticScroll();
			              result["eye_static_scroll"] = {scroll.x, scroll.y};
		              }
		              // The tattoo editor: whether it is open, on which creature, and the tattoo site under the pointer
		              if (Locator::tattooEditorSystem::has_value())
		              {
			              const auto& editor = Locator::tattooEditorSystem::value();
			              result["tattoo_editor"] = {{"open", editor.IsOpen()},
			                                         {"creature", Id(editor.GetCreature())},
			                                         {"from_menu", editor.IsFromMenu()},
			                                         {"shown", Id(editor.GetShown())},
			                                         {"hovered_site", Optional(editor.GetHoveredSite())}};
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("status",
	                    "A creature's body systems: moving, what its hands do, its leash, its fight, its animation and "
	                    "its skin",
	                    {IdParameter("The creature's entity id")}),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) {
		              const auto entity = EntityParam(context.params);
		              if (!entity.has_value() || !registry.AllOf<Creature>(*entity))
		              {
			              return QueryResult::Error("no creature with that id");
		              }
		              const auto creature = *entity;
		              Json result = {{"id", ToId(creature)}};
		              if (Locator::creatureLocomotionSystem::has_value())
		              {
			              result["moving"] = Locator::creatureLocomotionSystem::value().IsMoving(creature);
		              }
		              if (Locator::creatureObjectActionSystem::has_value())
		              {
			              const auto& actions = Locator::creatureObjectActionSystem::value();
			              result["object_action"] = {{"state", static_cast<int>(actions.GetState(creature))},
			                                         {"progress", Optional(actions.GetProgress(creature))},
			                                         {"held", Id(actions.GetHeld(creature))},
			                                         {"catching", actions.IsCatching(creature)}};
		              }
		              if (Locator::leashSystem::has_value())
		              {
			              const auto& leash = Locator::leashSystem::value();
			              result["leash"] = {{"leashed", leash.IsLeashed(creature)},
			                                 {"type", static_cast<int>(leash.TypeOf(creature))},
			                                 {"tied_to", Id(leash.TiedTo(creature))},
			                                 {"order_target", Optional(leash.OrderTarget(creature))}};
		              }
		              if (Locator::creatureFightSystem::has_value())
		              {
			              const auto& fights = Locator::creatureFightSystem::value();
			              result["fight"] = {{"fighting", fights.IsFighting(creature)},
			                                 {"opponent", Id(fights.OpponentOf(creature))},
			                                 {"knocked_out", fights.IsKnockedOut(creature)},
			                                 {"forward_extent", fights.ForwardExtent(creature)}};
		              }
		              if (const auto* animation = registry.TryGet<const CreatureAnimation>(creature); animation != nullptr)
		              {
			              result["animation_system"] = Locator::creatureAnimationSystem::has_value();
		              }
		              if (const auto* skin = registry.TryGet<const CreatureSkin>(creature); skin != nullptr)
		              {
			              result["skin_revision"] = skin->revision;
			              result["skin_system"] = Locator::creatureSkinSystem::has_value();
		              }
		              return QueryResult::Value(std::move(result));
	              }));
	provider->Add(
	    Query("leash",
	          "A creature's leash (the local player's creature by default): the leashes it knows, the one worn and "
	          "picked, whether it works, is tied and to what, its rope (ends, lengths, tension), the area that holds the "
	          "creature and the place it walks back to, the order it carries out, the desire the leash makes rule it, and the "
	          "local "
	          "player's hand on the rope (away, over or holding)",
	          {{.name = "id", .type = "integer", .description = "The creature's entity id"}}),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) {
		    if (!Locator::leashSystem::has_value())
		    {
			    return QueryResult::Error("no leash system");
		    }
		    const auto& leash = Locator::leashSystem::value();
		    // The god of this computer, whose hand is at the rope
		    const auto local =
		        Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
		    auto entity = EntityParam(context.params);
		    if (!context.params.contains("id"))
		    {
			    entity = leash.PlayersCreature(local);
		    }
		    if (!entity.has_value() || !registry.AllOf<Creature>(*entity))
		    {
			    return QueryResult::Error("no creature with that id");
		    }
		    const auto creature = *entity;
		    Json result = {
		        {"id", ToId(creature)},
		        {"rope_hand", static_cast<int>(leash.RopeHand(local))},
		        {"rope_hand_name", std::array {"away", "over", "holding"}.at(static_cast<size_t>(leash.RopeHand(local)))}};
		    const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
		    if (leashes == nullptr)
		    {
			    result["leashed"] = false;
			    return QueryResult::Value(std::move(result));
		    }
		    result["known"] = {leashes->known.test(0), leashes->known.test(1), leashes->known.test(2)};
		    result["selected"] = static_cast<int>(leashes->selected);
		    result["leashed"] = leashes->worn.has_value();
		    result["area"] = {
		        {"centre", {leashes->confinementCentre.x, leashes->confinementCentre.y, leashes->confinementCentre.z}},
		        {"radius", leashes->confinementRadius}};
		    result["return_to"] = leashes->returnTo.has_value()
		                              ? Json {leashes->returnTo->x, leashes->returnTo->y, leashes->returnTo->z}
		                              : Json(nullptr);
		    result["order_target"] = Optional(leash.OrderTarget(creature));
		    if (Locator::creatureMindSystem::has_value())
		    {
			    const auto ruling = Locator::creatureMindSystem::value().DominantDesire(creature);
			    result["ruling_desire"] =
			        ruling.has_value() ? Json(std::string(creature_desires::Name(*ruling))) : Json(nullptr);
		    }
		    if (leashes->worn.has_value())
		    {
			    const auto& worn = *leashes->worn;
			    const auto* drawn = registry.TryGet<const ecs::components::CreatureLeashRope>(creature);
			    const auto rope = drawn != nullptr ? drawn->rope : leash_rope::Rope {};
			    result["type"] = static_cast<int>(worn.type);
			    result["works"] = worn.works;
			    result["tied_to"] = Id(worn.tiedTo);
			    result["held_rope"] = drawn != nullptr && drawn->held.has_value();
			    result["rope"] = {{"start", {rope.start.x, rope.start.y, rope.start.z}},
			                      {"end", {rope.end.x, rope.end.y, rope.end.z}},
			                      {"slack_length", worn.slackLength},
			                      {"max_length", worn.maxLength},
			                      {"tension", rope.tension}};
		    }
		    return QueryResult::Value(std::move(result));
	    }));
	auto fight = Query("fight",
	                   "Starts a fight between two creatures as the scripts and the leash start one, the first making the "
	                   "arena; how it went and both creatures' fight state",
	                   {IdParameter("The creature that starts the fight"),
	                    {.name = "opponent", .type = "integer", .description = "The other creature", .required = true}});
	fight.writes = true;
	provider->Add(std::move(fight), [](const QueryContext& context) {
		const auto* registry = Registry();
		if (registry == nullptr || !Locator::creatureFightSystem::has_value())
		{
			return QueryResult::Error("there are no fights without a land");
		}
		const auto creature = EntityParam(context.params);
		const auto opponentId = context.params.find("opponent");
		const auto opponent = opponentId == context.params.end() ? std::nullopt : FromId(*opponentId);
		if (!creature.has_value() || !opponent.has_value() || !registry->Valid(*opponent) ||
		    !registry->AllOf<Creature>(*creature) || !registry->AllOf<Creature>(*opponent))
		{
			return QueryResult::Error("creatures.fight needs two creatures: id and opponent");
		}
		auto& fights = Locator::creatureFightSystem::value();
		using Start = ecs::systems::CreatureFightSystemInterface::StartResult;
		constexpr std::array<std::string_view, 7> k_Results {"started",  "no_opponent", "busy",     "too_weak",
		                                                     "no_arena", "refused",     "finishing"};
		const auto started = fights.StartFight(*creature, *opponent);
		const auto index = std::min(static_cast<size_t>(started), k_Results.size() - 1);
		if (started != Start::Started)
		{
			return QueryResult::Error("the fight didn't start: " + std::string(k_Results.at(index)));
		}
		return QueryResult::Value({
		    {"result", k_Results.at(index)},
		    {"fighting", {fights.IsFighting(*creature), fights.IsFighting(*opponent)}},
		    {"opponent_of_id", Id(fights.OpponentOf(*creature))},
		});
	});
	auto cheat = Query("fight_cheat",
	                   "Sends the fight cheat of the player of this computer as a command: at the turn, the player's "
	                   "creature fights the nearest creature, or stops the duel that it is in. A game server takes the "
	                   "command only with its test commands on",
	                   {});
	cheat.writes = true;
	provider->Add(std::move(cheat), [](const QueryContext& /*context*/) {
		if (Registry() == nullptr || !Locator::playerCommandSystem::has_value())
		{
			return QueryResult::Error("there are no fights without a land");
		}
		Locator::playerCommandSystem::value().Submit(player_commands::CreatureFightCheat {});
		return QueryResult::Value({{"sent", true}});
	});
	auto move = Query("move",
	                  "Sends a creature walking or running to a point by its own legs (turning and stepping off as it "
	                  "does), or turning on the spot to face it",
	                  {IdParameter("The creature"),
	                   {.name = "point", .type = "array", .description = "[x, z] on the land", .required = true},
	                   {.name = "pace",
	                    .type = "string",
	                    .description = "walk (the default), run, face to turn on the spot towards the point, stop to stop "
	                                   "as the game stops a walk (the point is not used), or stop_at_once to stop dead",
	                    .required = false}});
	move.writes = true;
	provider->Add(std::move(move), [](const QueryContext& context) {
		const auto* registry = Registry();
		if (registry == nullptr || !Locator::creatureLocomotionSystem::has_value())
		{
			return QueryResult::Error("no creatures move without a land");
		}
		const auto creature = EntityParam(context.params);
		const auto point = context.params.find("point");
		if (!creature.has_value() || !registry->AllOf<Creature>(*creature) || point == context.params.end() ||
		    !point->is_array() || point->size() != 2 || !(*point)[0].is_number() || !(*point)[1].is_number())
		{
			return QueryResult::Error("creatures.move needs a creature's id and a point [x, z]");
		}
		const glm::vec2 target((*point)[0].get<float>(), (*point)[1].get<float>());
		const auto paceParam = context.params.find("pace");
		const auto pace =
		    paceParam != context.params.end() && paceParam->is_string() ? paceParam->get<std::string>() : std::string("walk");
		auto& locomotion = Locator::creatureLocomotionSystem::value();
		if (pace == "face")
		{
			return QueryResult::Value({{"result", locomotion.TurnToFace(*creature, target) ? "started" : "refused"}});
		}
		if (pace == "stop" || pace == "stop_at_once")
		{
			if (pace == "stop")
			{
				locomotion.Stop(*creature);
			}
			else
			{
				locomotion.StopAtOnce(*creature);
			}
			return QueryResult::Value({{"result", "stopped"}});
		}
		if (pace != "walk" && pace != "run")
		{
			return QueryResult::Error("pace is walk, run, face, stop or stop_at_once");
		}
		using Locomotion = ecs::systems::CreatureLocomotionSystemInterface;
		constexpr std::array<std::string_view, 3> k_Results {"invalid_destination", "busy", "started"};
		const auto result =
		    locomotion.MoveTo(*creature, target, pace == "run" ? Locomotion::Pace::Run : Locomotion::Pace::Walk, 0.0f, 1.0f);
		return QueryResult::Value({{"result", k_Results.at(static_cast<size_t>(result))}});
	});
	provider->Add(Query("route",
	                    "A creature's walk in full: where it is going, how far short of it it stops and how near it must get, "
	                    "the state of its walk, the route it follows (its legs, straight or round a circle, and points "
	                    "along it) and the circles it walks round (each grown by its own reach)",
	                    {IdParameter("The creature")}),
	              [](const QueryContext& context) {
		              const auto* registry = Registry();
		              const auto creature = EntityParam(context.params);
		              if (registry == nullptr || !creature.has_value())
		              {
			              return QueryResult::Error("creatures.route needs a creature's id");
		              }
		              const auto* locomotion = registry->TryGet<const CreatureLocomotion>(*creature);
		              if (locomotion == nullptr)
		              {
			              return QueryResult::Error("that isn't a creature that walks");
		              }
		              const auto& route = locomotion->follower.GetRoute();
		              Json points = Json::array();
		              for (const auto& point : route.Points(glm::radians(5.0f)))
		              {
			              points.push_back({point.x, point.y});
		              }
		              Json legs = Json::array();
		              for (const auto& leg : route.legs)
		              {
			              Json item {{"from", {leg.from.x, leg.from.y}}, {"to", {leg.to.x, leg.to.y}}, {"arc", leg.arc}};
			              if (leg.round.has_value())
			              {
				              item["centre"] = {leg.round->centre.x, leg.round->centre.y};
				              item["radius"] = leg.round->radius;
				              item["clockwise"] = leg.roundWay == creature_route::Way::Clockwise;
			              }
			              legs.push_back(std::move(item));
		              }
		              Json circles = Json::array();
		              for (const auto& obstacle : locomotion->follower.GetObstacles().All())
		              {
			              if (obstacle.active)
			              {
				              circles.push_back({{"centre", {obstacle.circle.centre.x, obstacle.circle.centre.y}},
				                                 {"radius", obstacle.circle.radius},
				                                 {"thing", Optional(obstacle.thing)}});
			              }
		              }
		              return QueryResult::Value({{"destination", Optional(locomotion->destination)},
		                                         {"motion", static_cast<int>(locomotion->motion)},
		                                         {"radius", locomotion->radius},
		                                         {"stop_short", locomotion->ring.min},
		                                         {"arrive_within", locomotion->ring.max},
		                                         {"failed", locomotion->failed},
		                                         {"state", static_cast<int>(locomotion->follower.GetState())},
		                                         {"segment", route.segment},
		                                         {"legs", std::move(legs)},
		                                         {"points", std::move(points)},
		                                         {"circles", std::move(circles)}});
	              });
	return provider;
}

} // namespace openblack::inspector::game_providers

namespace
{

/// Why the game ends an order to go to a thing at once, if it does: a cut scene of a script makes a creature that is free
/// to think for itself look at the camera, again and again, so it drops the order on the next turn
std::optional<std::string> CutSceneRefusal(const ecs::Registry& registry, entt::entity creature)
{
	if (registry.AllOf<ScriptControlled>(creature) || !Locator::cinematicDirectorSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& director = Locator::cinematicDirectorSystem::value();
	const bool barsIn = director.IsWideScreenOn();
	if (!creature_mind::StartsLookAtCameraInCutScene(barsIn, barsIn ? director.GetWideScreenOwner() : 0u, false))
	{
		return std::nullopt;
	}
	return "a cut scene is playing: the game makes a creature that is free to think for itself look at the camera "
	       "again on each turn, so it would drop the order at once. Give the order when the cut scene is over";
}

/// What can end an order to go to a thing after the creature took it. The creature sees a thing that is near; for one
/// that is farther it walks to where it believes the thing is, and it gives up a walk in which it has not moved for 50
/// turns, as it does while a long route is still searched.
std::string FarThingWarning(const ecs::Registry& registry, entt::entity creature, entt::entity object)
{
	const auto* from = registry.TryGet<const Transform>(creature);
	const auto* to = registry.TryGet<const Transform>(object);
	if (from == nullptr || to == nullptr)
	{
		return {};
	}
	const auto distance = gutils::GetDistanceInMetres(from->position, to->position);
	if (distance < creature_mind::k_SeenWithinMetres)
	{
		return {};
	}
	return fmt::format("the thing is {:.0f} m away: the creature may not see it and then walks to where it believes it is. It "
	                   "gives up the order when it has not moved for {} turns, which a long route search can cause. Place "
	                   "the creature near the thing first (edit.move) to be sure",
	                   distance, creature_cast_moves::k_StallTurns);
}

} // namespace

namespace openblack::inspector::game_providers
{

/// The inspector's order to a creature, given as the game gives its own orders: a forced plan of the creature's mind,
/// which its body does
CreatureOrderResult GiveCreatureOrder(entt::entity creature, const CreatureOrder& order)
{
	if (!Locator::creatureMindSystem::has_value() || !Locator::creatureObjectActionSystem::has_value())
	{
		return {.detail = "the game has no creature mind or hands"};
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return {.detail = "that is not a creature"};
	}
	auto& minds = Locator::creatureMindSystem::value();
	const auto& hands = Locator::creatureObjectActionSystem::value();
	using ForcedPlan = ecs::systems::CreatureMindSystemInterface::ForcedPlan;
	using Kind = CreatureOrder::Kind;
	const auto carried = hands.GetCarried(creature);
	switch (order.kind)
	{
	case Kind::PickUp:
	{
		if (!order.object.has_value() || !registry.Valid(*order.object))
		{
			return {.detail = "the thing is gone"};
		}
		if (carried == order.object)
		{
			return {.detail = "it holds that thing already"};
		}
		if (const auto refusal = CutSceneRefusal(registry, creature); refusal.has_value())
		{
			return {.detail = *refusal};
		}
		if (!hands.CanPickUp(creature, *order.object))
		{
			// Name the size or the weight when that is what stops it
			const auto lift = hands.LiftFactsOf(creature, *order.object);
			if (lift.has_value() && lift->thingRadius + lift->creatureRadius > lift->reach)
			{
				return {.detail = fmt::format("it cannot pick that thing up: its radius {:.2f} and the creature's "
				                              "radius {:.2f} are more than the reach {:.2f}",
				                              lift->thingRadius, lift->creatureRadius, lift->reach)};
			}
			if (lift.has_value() && lift->thingWeight > lift->creatureWeight * creature_object_actions::k_MostLiftedWeightShare)
			{
				return {.detail = fmt::format("it cannot pick that thing up: its weight {:.2f} is more than the "
				                              "creature can lift ({:.2f})",
				                              lift->thingWeight,
				                              lift->creatureWeight * creature_object_actions::k_MostLiftedWeightShare)};
			}
			return {.detail = "it cannot pick that thing up"};
		}
		// As a leash order on a thing it can lift when its desires choose nothing else
		const bool taken = minds.ForcePlan(creature, ForcedPlan {.desire = creature_desires::Desire::Curiosity,
		                                                         .action = creature_leash_orders::k_HoldAction,
		                                                         .object = order.object,
		                                                         .ordered = true});
		return {.taken = taken,
		        .detail = taken ? "picks it up and holds it" : "its mind refused the plan",
		        .warning = taken ? FarThingWarning(registry, creature, *order.object) : std::string()};
	}
	case Kind::PutDown:
	case Kind::Throw:
	{
		if (!carried.has_value())
		{
			return {.detail = "it carries nothing"};
		}
		if (!order.point.has_value())
		{
			const bool taken = minds.ForcePlan(
			    creature, ForcedPlan {.desire = creature_desires::Desire::ObeyPlayer, .action = "PutDown", .ordered = true});
			return {.taken = taken, .detail = taken ? "puts it down where it stands" : "its mind refused the plan"};
		}
		// As a leash order on the land while it carries something: it throws the thing there when it cannot stand at
		// the point, or on the aggression leash
		const auto height = creature_morph::k_HeightAtSizeOne * ShownSize(*body);
		const bool standable = !Locator::creatureLocomotionSystem::has_value() ||
		                       Locator::creatureLocomotionSystem::value().GetWalkableLand().IsValid(
		                           *order.point, creature_route::k_DestinationClearance);
		const auto leash = Locator::leashSystem::has_value() ? Locator::leashSystem::value().TypeOf(creature) : LeashType::None;
		const auto ground =
		    creature_leash_orders::OnGround(*order.point, {.carrying = true,
		                                                   .placeReachable = standable,
		                                                   .leash = leash == LeashType::None ? LeashType::Rope : leash,
		                                                   .height = height});
		const bool throws = order.kind == Kind::Throw || ground.kind == creature_leash_orders::GroundOrder::Kind::ThrowAt;
		const bool taken =
		    minds.ForcePlan(creature, ForcedPlan {.desire = creature_desires::Desire::ObeyPlayer,
		                                          .action = creature_leash_orders::k_MoveAction,
		                                          .agenda = throws ? creature_leash_orders::ThrowAt(*order.point, height)
		                                                           : creature_leash_orders::PutDownAt(*order.point, height),
		                                          .ordered = true,
		                                          .walksBack = true});
		return {.taken = taken,
		        .detail = !taken   ? "its mind refused the plan"
		                  : throws ? "carries it near the point and throws it there"
		                           : "carries it to the point and puts it down"};
	}
	case Kind::Eat:
	{
		if (!order.object.has_value() || !registry.Valid(*order.object))
		{
			return {.detail = "the thing is gone"};
		}
		const auto* executor = creature_plan_actions::For(order.action);
		if (executor == nullptr || executor->activity != creature_mind::Activity::Eat)
		{
			return {.detail = "no eat action is called " + order.action};
		}
		if (const auto refusal = CutSceneRefusal(registry, creature); refusal.has_value())
		{
			return {.detail = *refusal};
		}
		const bool taken = minds.ForcePlan(creature, ForcedPlan {.desire = creature_desires::Desire::Hunger,
		                                                         .action = executor->action,
		                                                         .object = order.object,
		                                                         .ordered = true});
		return {.taken = taken,
		        .detail = taken ? std::string(executor->action) : "its mind refused the plan",
		        .warning = taken ? FarThingWarning(registry, creature, *order.object) : std::string()};
	}
	}
	return {.detail = "no such order"};
}

} // namespace openblack::inspector::game_providers
