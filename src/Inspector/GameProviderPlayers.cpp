/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

#include <fmt/format.h>

#include "3D/L3DSubMesh.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/GameStanding.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerRelations.h"
#include "ECS/PlayerRelations.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/IncomingMessageSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PlaygroundTripSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/SyncChecksumSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "GameProviderShared.h"
#include "GameProviders.h"
#include "Gui/ScreenFade.h"
#include "Gui/TextDatabase.h"
#include "Hand/HandTrace.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Sound/CustomMusicPlayerInterface.h"
#include "Sound/MusicMood.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

// The players, their hands and their input

namespace openblack::inspector::game_providers
{

std::unique_ptr<ProviderInterface> PlayersProvider()
{
	auto provider = std::make_unique<FunctionProvider>("players");
	provider->Add(Query("new_game", "How the new game started: whether the tutorial and the creature training are "
	                                "skipped and the old creature kept, and the player profiles"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::tutorialSkipSystem::has_value())
		              {
			              const auto& skip = Locator::tutorialSkipSystem::value().Get();
			              result["skip_tutorial"] = skip.skipTutorial;
			              result["skip_creature_training"] = skip.skipCreatureTraining;
			              result["keep_old_creature"] = skip.keepOldCreature;
			              result["start_with_cow"] = skip.startWithCow;
		              }
		              if (Locator::playerProfileSystem::has_value())
		              {
			              const auto& profiles = Locator::playerProfileSystem::value();
			              result["profiles"] = profiles.GetProfileCount();
			              result["profile_has_creature"] = profiles.CurrentProfileHasCreature();
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("playground_trip", "The trip to the Gods' Playground: whether every player's influence covers "
	                                       "the whole land, and whether a lesson runs"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::playgroundTripSystem::has_value())
		              {
			              const auto& trip = Locator::playgroundTripSystem::value();
			              result["influence_everywhere"] = trip.InfluenceEverywhere();
			              result["lesson_running"] = trip.IsLessonRunning();
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("profile_folder", "The profile folder, the folder of the player played in it, and whether that "
	                                      "folder holds the help record (helpstats.dat)"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (!Locator::profileFolderSystem::has_value())
		              {
			              return QueryResult::Value(std::move(result));
		              }
		              const auto& folder = Locator::profileFolderSystem::value();
		              const auto root = folder.GetFolder();
		              result["folder"] = root.has_value() ? Json(root->generic_string()) : Json(nullptr);
		              const auto profile = Locator::playerProfileSystem::has_value()
		                                       ? Locator::playerProfileSystem::value().GetCurrentProfile()
		                                       : std::nullopt;
		              const auto player = profile.has_value() ? gui::ToUtf16(profile->name) : std::u16string {};
		              const auto playerFolder = folder.GetPlayerFolder(player);
		              result["player_folder"] = playerFolder.has_value() ? Json(playerFolder->generic_string()) : Json(nullptr);
		              const auto record = folder.ReadPlayerFile(player, "helpstats.dat");
		              result["help_record_bytes"] = record.has_value() ? Json(record->size()) : Json(nullptr);
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("custom_music", "The custom music player (its tracks, the track that plays, its state and "
	                                    "position) and the music moods: the record of each track and the current feelings"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              auto* player = Locator::sounds::has_value() ? Locator::sounds::value().GetCustomMusic() : nullptr;
		              if (player != nullptr)
		              {
			              Json tracks = Json::array();
			              for (const auto& track : player->GetTracks())
			              {
				              tracks.push_back(track.fileName);
			              }
			              result["tracks"] = std::move(tracks);
			              const auto current = player->GetCurrentTrack();
			              result["current"] = current.has_value() ? Json(*current) : Json(nullptr);
			              result["song"] = player->GetSongName();
			              result["state"] = static_cast<int>(player->GetPlayState());
			              result["position_ms"] = player->GetPosition();
			              result["length_ms"] = player->GetLength();
			              result["volume"] = player->GetVolume();
			              result["track_id"] = music_mood::TrackId(player->GetSongName());
		              }
		              if (Locator::musicMoodSystem::has_value())
		              {
			              const auto& moods = Locator::musicMoodSystem::value();
			              Json records = Json::array();
			              for (const auto& record : moods.GetRecords())
			              {
				              Json item = Json::object();
				              item["id"] = record.id;
				              item["time"] = record.time;
				              item["mood"] = record.emotions.empty() ? 0.0f : record.emotions.front().mood;
				              item["fear"] = record.emotions.empty() ? 0.0f : record.emotions.front().fear;
				              records.push_back(std::move(item));
			              }
			              result["records"] = std::move(records);
			              const auto emotion = moods.GetCurrentEmotion();
			              result["current_mood"] = emotion.has_value() ? Json(emotion->mood) : Json(nullptr);
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("list",
	                    "The players: their entities, primary creatures, computer minds (the nodes wanted now; "
	                    "players.computer_nodes has every node) and alignment",
	                    {}, ResultKind::List),
	              Serve<Locator::playerSystem>("the players", [](const ecs::systems::PlayerSystemInterface& players,
	                                                             const QueryContext& /*c*/) {
		              Json items = Json::array();
		              const auto* registry = Registry();
		              if (registry == nullptr)
		              {
			              return items;
		              }
		              // The players the land has, by their own components: the system is asked only of those it knows
		              registry->Each<const Player>([&](entt::entity entity, const Player& player) {
			              const auto name = player.name;
			              Json item = {{"player", static_cast<int>(name)},
			                           {"id", Id(entity)},
			                           {"local", (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer()
			                                                                       : PlayerNames::PLAYER_ONE) == name},
			                           {"computer", players.IsComputerPlayer(name)},
			                           {"primary_creature", Id(players.GetPrimaryCreature(name))}};
			              // The computer mind of the player, when a land script switched it on or named a player for it
			              if (const auto* mind = registry->TryGet<const ecs::components::ComputerPlayer>(entity);
			                  mind != nullptr)
			              {
				              item["computer_mind"] = {{"switched_on", mind->switchedOn},
				                                       {"marked", mind->marked},
				                                       {"started", mind->started},
				                                       {"creature_like", mind->creatureLike.has_value()
				                                                             ? Json(static_cast<int>(*mind->creatureLike))
				                                                             : Json(nullptr)}};
				              // What the god wants and what its hand does: the nodes that were wanted when last looked at
				              auto& computer = item["computer_mind"];
				              computer["paused"] = mind->mind.paused;
				              computer["last_empty_turn"] = mind->mind.lastEmptyTurn;
				              computer["doing"] = mind->mind.doing;
				              computer["hand"] = {mind->hand.hand.x, mind->hand.hand.y, mind->hand.hand.z};
				              Json steps = Json::array();
				              for (const auto& step : mind->hand.steps)
				              {
					              steps.push_back(static_cast<int>(step.kind));
				              }
				              computer["steps"] = std::move(steps);
				              computer["step_part"] = mind->hand.part;
				              Json wanted = Json::array();
				              for (const auto& node : mind->mind.nodes)
				              {
					              if (node.value != 0.0f)
					              {
						              wanted.push_back({{"name", computer_mind::InfoOf(node.kind).name},
						                                {"value", node.value},
						                                {"suppression", node.suppression},
						                                {"object", Id(node.object)}});
					              }
				              }
				              computer["wanted"] = std::move(wanted);
				              Json requests = Json::object();
				              for (size_t type = 0; type < mind->mind.castRequested.size(); ++type)
				              {
					              if (mind->mind.castRequested.at(type) != 0)
					              {
						              requests[std::to_string(type)] = mind->mind.castRequested.at(type);
					              }
				              }
				              computer["cast_requests"] = std::move(requests);
				              computer["casts"] = mind->casts.size();
			              }
			              if (Locator::alignmentSystem::has_value())
			              {
				              item["alignment"] = Locator::alignmentSystem::value().GetPlayerAlignment(name);
				              item["pending_alignment"] = Locator::alignmentSystem::value().GetPendingAlignment(name);
			              }
			              items.push_back(std::move(item));
		              });
		              return items;
	              }));
	// The whole tree of a computer mind, where the list of the players shows only the nodes that are wanted now
	provider->Add(
	    Query("computer_nodes",
	          "Every node of the tree of one computer god's mind: its name, personality weight, suppression, how much it "
	          "was wanted when last looked at, its place in the tree and the node above. Pages with limit and cursor. "
	          "name keeps only the nodes that the script name finds",
	          {{.name = "player",
	            .type = "integer|string",
	            .description = "The god: its number (0 to 7) or its name, such as PLAYER_TWO",
	            .required = true},
	           {.name = "name",
	            .type = "string",
	            .description = "Only the nodes that a script finds with this name, such as ExpandInfluence",
	            .required = false}},
	          ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) -> QueryResult {
		    const auto player = NamedPlayerParam(context.params);
		    if (!player.has_value())
		    {
			    return QueryResult::Error("player must be a number from 0 to 7 or a name such as PLAYER_TWO");
		    }
		    const auto entity = PlayerEntityOf(registry, *player);
		    const auto* computer =
		        entity.has_value() ? registry.TryGet<const ecs::components::ComputerPlayer>(*entity) : nullptr;
		    if (computer == nullptr)
		    {
			    return QueryResult::Error("that player has no computer mind");
		    }
		    const auto& mind = computer->mind;
		    const auto& names = SharedNodeNames(registry);
		    std::vector<uint16_t> shown;
		    if (const auto filter = StringMember(context.params, "name"); filter.has_value())
		    {
			    shown = computer_mind::FindNodes(mind, *filter, names);
		    }
		    else
		    {
			    shown.resize(mind.nodes.size());
			    std::ranges::generate(shown, [next = uint16_t {0}]() mutable { return next++; });
		    }
		    Json items = Json::array();
		    for (const auto index : shown)
		    {
			    const auto& node = mind.nodes.at(index);
			    items.push_back({{"index", index},
			                     {"name", computer_mind::NameOf(names, node.kind)},
			                     {"layer", static_cast<int>(computer_mind::InfoOf(node.kind).layer)},
			                     {"weight", node.weight},
			                     {"suppression", node.suppression},
			                     {"value", node.value},
			                     {"parent", node.parent.has_value() ? Json(*node.parent) : Json(nullptr)},
			                     {"children", node.childCount},
			                     {"object", Id(node.object)}});
		    }
		    return QueryResult::Value(std::move(items));
	    }));
	provider->Add(
	    Query("computer_attitudes",
	          "What one computer god's mind thinks of each player: the value that a script reads back, and the value the "
	          "mind keeps (twice as much)",
	          {{.name = "player",
	            .type = "integer|string",
	            .description = "The god: its number (0 to 7) or its name, such as PLAYER_TWO",
	            .required = true}}),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) -> QueryResult {
		    const auto player = NamedPlayerParam(context.params);
		    if (!player.has_value())
		    {
			    return QueryResult::Error("player must be a number from 0 to 7 or a name such as PLAYER_TWO");
		    }
		    const auto entity = PlayerEntityOf(registry, *player);
		    if (!entity.has_value() || !registry.AllOf<ecs::components::ComputerPlayer>(*entity))
		    {
			    return QueryResult::Error("that player has no computer mind");
		    }
		    // A mind that no script gave an attitude keeps the first value for every player
		    const auto* attitudes = registry.TryGet<const ecs::components::ComputerAttitudes>(*entity);
		    Json about = Json::array();
		    for (size_t other = 0; other < static_cast<size_t>(PlayerNames::_COUNT); ++other)
		    {
			    const auto kept =
			        attitudes != nullptr ? attitudes->kept.at(other) : ecs::components::ComputerAttitudes::k_StartValue;
			    about.push_back({{"player", other},
			                     {"name", k_PlayerNamesStrs.at(other)},
			                     {"attitude", player_relations::ReadAttitude(kept)},
			                     {"kept", kept}});
		    }
		    return QueryResult::Value({{"player", static_cast<int>(*player)}, {"attitudes", std::move(about)}});
	    }));
	provider->Add(
	    Query("commands", "The queue of the players' commands: the player of this computer, whose number the commands carry, "
	                      "and the commands that wait for the start of a turn"),
	    Serve<Locator::playerCommandSystem>(
	        "the players' commands", [](const ecs::systems::PlayerCommandSystemInterface& commands, const QueryContext& /*c*/) {
		        return Json {{"sender", static_cast<int>(commands.GetSender())}, {"pending", commands.PendingCount()}};
	        }));
	provider->Add({.name = "checksum",
	               .description = "The checksum of the synced world that a network game sends after each turn: whether it "
	                              "is on, and the turn and checksum of the last turn. With on, it turns it on or off",
	               .parameters = {{.name = "on",
	                               .type = "boolean",
	                               .description = "Turn the checksum on or off from the next turn",
	                               .required = false}},
	               .kind = ResultKind::Object,
	               .needsNear = false,
	               .writes = true},
	              [](const QueryContext& context) {
		              if (!Locator::syncChecksumSystem::has_value())
		              {
			              return QueryResult::Error("no checksum of the synced world");
		              }
		              auto& checksum = Locator::syncChecksumSystem::value();
		              if (const auto on = context.params.find("on"); on != context.params.end() && on->is_boolean())
		              {
			              checksum.SetEnabled(on->get<bool>());
		              }
		              const auto last = checksum.GetLast();
		              return QueryResult::Value(
		                  Json {{"on", checksum.IsEnabled()},
		                        {"turn", last.has_value() ? Json(last->turn) : Json(nullptr)},
		                        {"checksum", last.has_value() ? Json(fmt::format("{:016x}", last->checksum)) : Json(nullptr)}});
	              });
	provider->Add(Query("heartbeat", "Each player's heartbeat: danger, rate, beat and pulse", {}, ResultKind::List),
	              Serve<Locator::heartbeatSystem>(
	                  "the heartbeats", [](const ecs::systems::HeartbeatSystemInterface& hearts, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  const auto* registry = Registry();
		                  if (registry == nullptr)
		                  {
			                  return items;
		                  }
		                  registry->Each<const Player>([&](const Player& player) {
			                  const auto state = hearts.StateOf(player.name);
			                  if (!state.has_value())
			                  {
				                  return;
			                  }
			                  items.push_back({{"player", static_cast<int>(player.name)},
			                                   {"stress", state->stress},
			                                   {"rate", state->rate},
			                                   {"beats_per_second", state->rate * ecs::heartbeat::k_BeatsPerRate},
			                                   {"phase", state->phase},
			                                   {"pulse", state->pulse},
			                                   {"believer_share", state->believerShare},
			                                   {"influence_share", state->influenceShare},
			                                   {"destruction_beats", state->destructionBeats}});
		                  });
		                  return items;
	                  }));
	provider->Add(
	    Query("game_end", "The end of a skirmish or a network game: whether it is decided, and where each god stands"),
	    Serve<Locator::gameEndSystem>(
	        "the end of the game", [](const ecs::systems::GameEndSystemInterface& end, const QueryContext& /*c*/) {
		        Json gods = Json::array();
		        if (const auto* registry = Registry(); registry != nullptr)
		        {
			        registry->Each<const Player, const ecs::components::GameStanding>(
			            [&gods](const Player& player, const ecs::components::GameStanding& standing) {
				            gods.push_back({{"player", static_cast<int>(player.name)},
				                            {"out", standing.out},
				                            {"finished", standing.finished},
				                            {"share", standing.share},
				                            {"place", standing.place.has_value() ? Json(*standing.place) : Json(nullptr)},
				                            {"left", standing.reason == ecs::components::OutReason::Left}});
			            });
		        }
		        Json events = Json::array();
		        for (const auto& event : end.LastEvents())
		        {
			        events.push_back({{"god", static_cast<int>(event.god)}, {"kind", static_cast<int>(event.kind)}});
		        }
		        return Json {{"decided", end.IsDecided()}, {"gods", gods}, {"last_events", events}};
	        }));
	provider->Add(
	    Query("hand", "This computer's hands, the hands of its own god that it moves and draws: the player, where the hands "
	                  "are, and what the hand holds or takes"),
	    Serve<Locator::handSystem>("the hands", [](const ecs::systems::HandSystemInterface& hands, const QueryContext& /*c*/) {
		    Json result = OwnHandsJson(hands);
		    if (Locator::handGrabSystem::has_value())
		    {
			    const auto& grab = Locator::handGrabSystem::value();
			    result["held"] = Id(grab.GetHeld());
			    result["busy"] = grab.IsBusy();
			    result["in_influence"] = grab.IsInInfluence();
			    result["cursor_raise"] = grab.GetCursorRaise();
		    }
		    return result;
	    }));
	provider->Add(Query("pick", "What the cursor picks: the object, the point and the land under it, whether the help's text "
	                            "bubble or the leash's rope took the cursor, and the land behind the hand's "
	                            "middle, about which a press with nothing under the cursor reaches"),
	              Serve<Locator::pickingSystem>("the picking", [](const ecs::systems::PickingSystemInterface& picking,
	                                                              const QueryContext& /*c*/) {
		              const auto& pick = picking.GetPick();
		              return Json {{"object", Id(pick.object)},          {"point", Optional(pick.point)},
		                           {"land", Optional(pick.land)},        {"land_distance", pick.landDistance},
		                           {"text_bubble", pick.textBubble},     {"rope", pick.rope},
		                           {"distance", pick.distance},          {"hover_object", Id(pick.hoverObject)},
		                           {"hover_seconds", pick.hoverSeconds}, {"behind_hand", Optional(picking.GetBehindHand())}};
	              }));
	provider->Add(
	    Query("hand_trace",
	          "The player's hand frame by frame, for hand bugs: start keeps the last frames (frames, 600 by default) of every "
	          "part of the frame that put the hand somewhere, where it was placed, drawn and showed, on the screen too, and "
	          "what it did; read returns them (the last few with last) and the jumps among them: frames where the hand "
	          "moved on the screen more than threshold pixels (12) further than the cursor, came_back when it returned "
	          "within return_within frames (4); stop forgets them",
	          {{.name = "action", .type = "string", .description = "start, read (the default) or stop", .required = false},
	           {.name = "frames", .type = "integer", .description = "With start: how many frames to keep", .required = false},
	           {.name = "last", .type = "integer", .description = "With read: only the last frames", .required = false},
	           {.name = "threshold",
	            .type = "number",
	            .description = "With read: the pixels beyond the cursor's move that make a jump",
	            .required = false},
	           {.name = "return_within",
	            .type = "integer",
	            .description = "With read: the frames a jump may take to come back",
	            .required = false},
	           {.name = "jumps_only",
	            .type = "boolean",
	            .description = "With read: only the jumps, each with the frames around it",
	            .required = false}}),
	    [](const QueryContext& context) {
		    if (!Locator::handSystem::has_value() || Locator::handSystem::value().GetTrace() == nullptr)
		    {
			    return QueryResult::Error("the hands keep no trace");
		    }
		    auto& trace = *Locator::handSystem::value().GetTrace();
		    const auto action = context.params.value("action", std::string("read"));
		    if (action == "start")
		    {
			    const auto frames = context.params.value("frames", 600);
			    if (frames < 1 || frames > 100000)
			    {
				    return QueryResult::Error("frames is 1 to 100000");
			    }
			    trace.Enable(static_cast<size_t>(frames));
			    return QueryResult::Value(Json {{"tracing", true}, {"frames", frames}});
		    }
		    if (action == "stop")
		    {
			    trace.Disable();
			    return QueryResult::Value(Json {{"tracing", false}});
		    }
		    if (action != "read")
		    {
			    return QueryResult::Error("action is start, read or stop");
		    }
		    auto frames = trace.Frames();
		    const auto threshold = context.params.value("threshold", 12.0f);
		    const auto returnWithin = static_cast<size_t>(std::max(context.params.value("return_within", 4), 0));
		    const auto jumps = hand_trace::FindJumps(frames, threshold, returnWithin);
		    const auto vec = [](glm::vec3 v) { return Json {v.x, v.y, v.z}; };
		    const auto frameJson = [&vec](const hand_trace::Frame& frame) {
			    Json writes = Json::array();
			    for (const auto& write : frame.Writes())
			    {
				    writes.push_back({{"by", std::string(write.writer)}, {"at", vec(write.position)}});
			    }
			    return Json {{"frame", frame.frame},
			                 {"seconds", frame.seconds},
			                 {"cursor", {frame.cursor.x, frame.cursor.y}},
			                 {"placed", vec(frame.placed)},
			                 {"drawn", vec(frame.drawn)},
			                 {"visible", vec(frame.visible)},
			                 {"screen", frame.screen.has_value() ? Json {frame.screen->x, frame.screen->y} : Json(nullptr)},
			                 {"wrist", vec(frame.wrist)},
			                 {"wrist_screen", frame.wristScreen.has_value() ? Json {frame.wristScreen->x, frame.wristScreen->y}
			                                                                : Json(nullptr)},
			                 {"stage", frame.stage},
			                 {"busy", frame.busy},
			                 {"held", frame.held.has_value() ? Json(*frame.held) : Json(nullptr)},
			                 {"picked", frame.picked.has_value() ? Json(*frame.picked) : Json(nullptr)},
			                 {"cursor_land", frame.cursorLand.has_value() ? vec(*frame.cursorLand) : Json(nullptr)},
			                 {"rest_on_thing", frame.restOnThing.has_value() ? vec(*frame.restOnThing) : Json(nullptr)},
			                 {"held_at", frame.heldAt.has_value() ? vec(*frame.heldAt) : Json(nullptr)},
			                 {"writes", std::move(writes)},
			                 {"writes_lost", frame.writesLost}};
		    };
		    Json jumpItems = Json::array();
		    for (const auto& jump : jumps)
		    {
			    jumpItems.push_back({{"frame", jump.frame},
			                         {"pixels", jump.pixels},
			                         {"beyond_cursor", jump.beyondCursor},
			                         {"came_back", jump.cameBack},
			                         {"last_writer", std::string(jump.writer)}});
		    }
		    Json items = Json::array();
		    if (context.params.value("jumps_only", false))
		    {
			    // Each jump with the two frames before it and the few it may take to come back
			    std::vector<bool> keep(frames.size(), false);
			    for (const auto& jump : jumps)
			    {
				    const auto from = jump.index >= 2 ? jump.index - 2 : 0;
				    for (size_t i = from; i < frames.size() && i <= jump.index + returnWithin; ++i)
				    {
					    keep[i] = true;
				    }
			    }
			    for (size_t i = 0; i < frames.size(); ++i)
			    {
				    if (keep[i])
				    {
					    items.push_back(frameJson(frames[i]));
				    }
			    }
		    }
		    else
		    {
			    const auto last = static_cast<size_t>(std::max(context.params.value("last", 0), 0));
			    const size_t first = last > 0 && last < frames.size() ? frames.size() - last : 0;
			    for (size_t i = first; i < frames.size(); ++i)
			    {
				    items.push_back(frameJson(frames[i]));
			    }
		    }
		    return QueryResult::Value(Json {{"tracing", trace.Enabled()},
		                                    {"kept", frames.size()},
		                                    {"jumps", std::move(jumpItems)},
		                                    {"frames", std::move(items)}});
	    });
	provider->Add(Query("input", "The mouse and the hands as the game's actions see them"),
	              Serve<Locator::gameActionSystem>(
	                  "the game's actions", [](const input::GameActionInterface& actions, const QueryContext& /*c*/) {
		                  const auto mouse = actions.GetMousePosition();
		                  const auto hands = actions.GetHandPositions();
		                  return Json {{"mouse", {mouse.x, mouse.y}},
		                               {"wheel", actions.GetMouseWheelDelta()},
		                               {"hands", {Optional(hands[0]), Optional(hands[1])}},
		                               {"queued_presses", actions.HasQueuedPresses()},
		                               {"scripted_pointer", actions.GetScriptedPointer().has_value()},
		                               {"cursor_frozen", actions.IsCursorFrozen()}};
	                  }));
	provider->Add(Query("gestures", "The gesture being drawn and the last recognised"),
	              Serve<Locator::gestureSystem>(
	                  "the gestures", [](const ecs::systems::GestureSystemInterface& gestures, const QueryContext& /*c*/) {
		                  Json result = {{"drawing", gestures.IsDrawingPath()},
		                                 {"gesturing", gestures.IsGesturing()},
		                                 {"templates", gestures.GetTemplates().size()},
		                                 {"leash_picker_open", gestures.IsLeashPickerOpen()},
		                                 {"circle_seconds_left", gestures.GetCircleSecondsLeft()},
		                                 {"asked_power_up", static_cast<uint32_t>(gestures.GetAskedPowerUp())},
		                                 {"events_queue", Locator::gestureEvents::has_value()}};
		                  // The row of hints at the bottom right: each hinted gesture and its leash, and the row's fade
		                  Json hints = Json::array();
		                  for (const auto& hint : gestures.GetHints().hints)
		                  {
			                  hints.push_back({{"gesture", static_cast<uint32_t>(hint.gesture)},
			                                   {"leash", static_cast<int>(hint.leash)},
			                                   {"dot", hint.dot}});
		                  }
		                  result["hints"] = {{"alpha", gestures.GetHints().alpha}, {"shown", hints}};
		                  if (const auto last = gestures.GetLastRecognised(); last.has_value())
		                  {
			                  result["last_recognised"] = {{"number", last->number}, {"age", last->age}};
		                  }
		                  return result;
	                  }));
	return provider;
}

} // namespace openblack::inspector::game_providers
