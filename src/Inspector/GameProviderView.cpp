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
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraZones.h"
#include "Creature/LeashOrders.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Components/GuidanceHeard.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/FilmCreatureSystemInterface.h"
#include "ECS/Systems/GatheringBoxSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/IncomingMessageSystemInterface.h"
#include "ECS/Systems/LandDetailSystemInterface.h"
#include "ECS/Systems/LoadingScreenSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/PauseTextSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/TurnCheerSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "GameProviderShared.h"
#include "GameSeamInterface.h"
#include "Gui/ScreenFade.h"
#include "Gui/TextDatabase.h"
#include "Help/AdvisorModel.h"
#include "Help/Spirits.h"
#include "Locator.h"
#include "Particles/LightSheet.h"
#include "Visual/RendererRequestsInterface.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

// The camera's helpers, the editor and the cinematics

namespace openblack::inspector::game_providers
{

/// The seam is the game on the player's computer, or null with none
std::unique_ptr<ProviderInterface> ViewProvider(const GameSeamInterface* seam)
{
	auto provider = std::make_unique<FunctionProvider>("view");
	provider->Add(Query("pause_text", "The words of the pause and the speed: which words show (paused, slower, faster or "
	                                  "none), their fade made smooth, and the turn of their letters"),
	              [](const QueryContext& /*c*/) {
		              Json result = {{"shown", false}};
		              if (!Locator::pauseTextSystem::has_value())
		              {
			              return QueryResult::Value(std::move(result));
		              }
		              if (const auto shown = Locator::pauseTextSystem::value().GetShown())
		              {
			              constexpr std::array<std::string_view, 3> k_Kinds {"paused", "slower", "faster"};
			              result["shown"] = true;
			              result["kind"] = shown->kind == gui::pause_text::Kind::None
			                                   ? std::string_view("none")
			                                   : k_Kinds.at(static_cast<size_t>(shown->kind));
			              result["eased"] = shown->eased;
			              result["turn"] = shown->turn;
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(
	    Query("incoming_messages",
	          "The incoming messages at the bottom left of the screen: the list in order (kind, text, weather picture, the "
	          "real time it started to show), the real time now, and whether the button shows with its look"),
	    [seam](const QueryContext& /*c*/) {
		    Json result = {{"count", 0}, {"messages", Json::array()}, {"shown", false}};
		    if (!Locator::incomingMessageSystem::has_value())
		    {
			    return QueryResult::Value(std::move(result));
		    }
		    const auto& system = Locator::incomingMessageSystem::value();
		    constexpr std::array<std::string_view, 3> k_Kinds {"chat", "mail", "weather"};
		    result["now"] = system.GetMilliseconds();
		    result["count"] = system.GetQueue().Count();
		    result["shown_text"] = gui::ToUtf8(system.GetQueue().ShownText());
		    // The list is read from the top message down, by dismissing copies
		    auto copy = system.GetQueue();
		    while (const auto* top = copy.Top())
		    {
			    result["messages"].push_back({{"kind", k_Kinds.at(static_cast<size_t>(top->kind))},
			                                  {"text", gui::ToUtf8(top->text)},
			                                  {"picture", top->picture},
			                                  {"since", top->since}});
			    copy.Dismiss(top->since);
		    }
		    if (seam != nullptr)
		    {
			    seam->AddIncomingMessages(result);
		    }
		    return QueryResult::Value(std::move(result));
	    });
	provider->Add(
	    Query("chat", "The chat of a network game: the names of the gods, the lines that came in (turn, sender, the "
	                  "players that get them, text or taunt, shown in the incoming messages), the speech bubbles over "
	                  "the creatures (lines, time to show, scroll), and the chat controls (shown, open, quick chat, "
	                  "the typed line, the list of players with its friends and others, and who gets a line), and the "
	                  "ping server of the gathering box (there or not, the online clock)"),
	    [seam](const QueryContext& /*c*/) {
		    Json result = {{"names", Json::object()}, {"received", Json::array()}, {"bubbles", Json::array()}};
		    if (Locator::gatheringBoxSystem::has_value())
		    {
			    const auto& gathering = Locator::gatheringBoxSystem::value();
			    result["ping_server"] = {{"present", gathering.HasPingServer()}, {"online_time", gathering.GetOnlineTime()}};
		    }
		    if (!Locator::chatSystem::has_value())
		    {
			    return QueryResult::Value(std::move(result));
		    }
		    auto& chat = Locator::chatSystem::value();
		    for (const auto& [slot, name] : chat.GetNames())
		    {
			    result["names"][std::to_string(static_cast<int>(slot))] = gui::ToUtf8(name);
		    }
		    for (const auto& line : chat.GetReceived())
		    {
			    Json recipients = Json::array();
			    for (const auto player : line.recipients)
			    {
				    recipients.push_back(static_cast<int>(player));
			    }
			    result["received"].push_back({{"turn", line.turn},
			                                  {"sender", static_cast<int>(line.sender)},
			                                  {"recipients", std::move(recipients)},
			                                  {"text", gui::ToUtf8(line.text)},
			                                  {"taunt", line.taunt.has_value() ? Json(*line.taunt) : Json(nullptr)},
			                                  {"shown", line.shown}});
		    }
		    for (const auto& [creature, bubble] : chat.GetBubbles())
		    {
			    Json lines = Json::array();
			    for (const auto& line : bubble.lines)
			    {
				    lines.push_back(
				        {{"text", gui::ToUtf8(line.text)},
				         {"sender", line.sender.has_value() ? Json(static_cast<int>(*line.sender)) : Json(nullptr)}});
			    }
			    result["bubbles"].push_back({{"creature", entt::to_integral(creature)},
			                                 {"lines", std::move(lines)},
			                                 {"display_time", bubble.displayTime},
			                                 {"scroll", bubble.scroll}});
		    }
		    if (seam != nullptr)
		    {
			    seam->AddChatControls(result);
		    }
		    return QueryResult::Value(std::move(result));
	    });
	provider->Add(Query("tooltip", "The tooltip that the hand shows: its index and text name, its mouse button and arrows, "
	                               "its fade, and the numbers or other words that it shows"),
	              [seam](const QueryContext& /*c*/) {
		              Json result = {{"shown", false}};
		              if (seam != nullptr)
		              {
			              seam->AddToolTip(result);
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("video", "The video playing: its frame, frame count and rate, its fade, the falling spell, its "
	                             "creature and its effects, whether films are on, and the 16-bit surface of the films"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              // The fade of the whole screen, which the video fades to white at its end
		              if (Locator::screenFade::has_value())
		              {
			              const auto& fade = Locator::screenFade::value();
			              result["screen_fade"] = {{"amount", fade.GetAmount()}, {"turns", fade.GetTurns()}};
		              }
		              if (Locator::videoSystem::has_value())
		              {
			              const auto& videos = Locator::videoSystem::value();
			              result["playing"] = videos.IsPlaying();
			              result["covers_screen"] = videos.CoversScreen();
			              result["hides_world"] = videos.HidesWorld();
			              result["films"] = videos.AreFilmsEnabled();
			              const auto surface = videos.GetFilmSurface();
			              result["sixteen_bit_colour"] = surface.has_value();
			              result["film_surface"] = "none";
			              if (surface)
			              {
				              result["film_surface"] = *surface == graphics::film_surface::Format::Rgb555 ? "555" : "565";
			              }
			              if (const auto status = videos.GetStatus())
			              {
				              result["frame"] = status->frame;
				              result["frame_count"] = status->frameCount;
				              result["fps"] = status->fps;
				              result["alpha"] = status->alpha;
				              result["falling_spell"] = status->fallingSpell;
				              result["falling_spell_state"] = status->fallingSpellState;
			              }
		              }
		              // The copy of the player's creature that the falling spell's film shows, and its colours now
		              if (Locator::filmCreatureSystem::has_value())
		              {
			              const auto& film = Locator::filmCreatureSystem::value();
			              Json creature = {{"running", film.IsRunning()}};
			              if (const auto shown = film.GetShown())
			              {
				              creature["copy"] = Id(shown->copy);
				              creature["colour"] = Point(shown->colour);
				              creature["added"] = Point(shown->added);
				              creature["eye"] = Point(glm::vec3(glm::inverse(shown->view)[3]));
				              creature["shown"] = shown->copyShown;
				              creature["stars"] = shown->stars.size();
				              Json stars = Json::array();
				              for (const auto& star : shown->stars)
				              {
					              stars.push_back({{"pixel", {star.pixel.x, star.pixel.y}},
					                               {"depth", star.depth},
					                               {"half_size", star.halfSize},
					                               {"argb", star.argb}});
				              }
				              creature["star_sprites"] = std::move(stars);
				              creature["fans"] = shown->fans.size();
				              if (!shown->fans.empty() && !shown->fans.front().empty())
				              {
					              const auto& middle = shown->fans.front().front();
					              creature["burst_pixel"] = {middle.pixel.x, middle.pixel.y};
					              creature["burst_alpha"] = middle.argb >> 24u;
				              }
			              }
			              result["film_creature"] = std::move(creature);
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("loading", "The screens of the start and of the loads: whether they have their textures, what a load "
	                               "draws at its redraw points, whether the tips screen faded in and has a tip, and the "
	                               "timers of the start"),
	              [](const QueryContext& /*c*/) {
		              Json result = {{"present", Locator::loadingScreenSystem::has_value()}};
		              if (Locator::loadingScreenSystem::has_value())
		              {
			              const auto& screens = Locator::loadingScreenSystem::value();
			              result["has_screen"] = screens.HasScreen();
			              result["loading"] = screens.IsLoading();
			              result["look"] = nullptr;
			              if (const auto look = screens.GetLoadingLook(); look.has_value())
			              {
				              using Mode = loading::LoadingClock::Mode;
				              result["look"] =
				                  *look == Mode::Tips ? "tips" : (*look == Mode::PleaseWait ? "please_wait" : "none");
			              }
			              result["tip_faded_in"] = screens.TipFadedIn();
			              result["tip_showing"] = screens.TipShowing();
			              result["times_startup"] = screens.TimesStartup();
			              result["times_prefetch"] = screens.TimesPrefetch();
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("state", "Whether a camera path holds the camera, the camera's help, the advisors' cheers as the "
	                             "camera turns, and the knock readout"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::cameraPathSystem::has_value())
		              {
			              auto& path = Locator::cameraPathSystem::value();
			              result["path"] = {
			                  {"pathing", path.IsPathing()}, {"paused", path.IsPaused()}, {"holds_camera", path.HoldsCamera()}};
		              }
		              if (Locator::cameraHelpSystem::has_value())
		              {
			              const auto& help = Locator::cameraHelpSystem::value().Get();
			              result["help"] = {{"features", help.features},
			                                {"auto_pitch", help.autoPitch},
			                                {"camera_keys", help.cameraKeys},
			                                {"hand_reach", help.handReach},
			                                {"interface_level",
			                                 help.interfaceLevel.has_value() ? Json(*help.interfaceLevel) : Json(nullptr)}};
		              }
		              if (Locator::turnCheerSystem::has_value())
		              {
			              const auto& cheer = Locator::turnCheerSystem::value();
			              const auto& cheering = cheer.GetCheering();
			              result["turn_cheer"] = {{"turn_speed", cheer.GetTurnSpeed()},
			                                      {"tilt_speed", cheer.GetTiltSpeed()},
			                                      {"watches_tilt", cheering.watch == help::turn_cheer::Watch::Tilt},
			                                      {"on", cheering.on},
			                                      {"last_cheer_turn", cheering.lastCheerTurn},
			                                      {"last_best_praise_turn", cheering.lastBestPraiseTurn}};
		              }
		              if (Locator::abodeKnockSystem::has_value())
		              {
			              const auto& knock = Locator::abodeKnockSystem::value();
			              result["knock_readout"] = {{"town", Id(knock.GetReadoutTown())}, {"scale", knock.GetReadoutScale()}};
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("bookmarks", "The camera's bookmarks: each slot, whether it is set, and whether the camera follows it",
	                    {}, ResultKind::List),
	              Serve<Locator::cameraBookmarkSystem>(
	                  "the bookmarks", [](const ecs::systems::CameraBookmarkSystemInterface& bookmarks, const QueryContext&) {
		                  // The markers are things that only the frames make
		                  const auto* registry = DrawnRegistry();
		                  Json items = Json::array();
		                  for (size_t i = 0; i < bookmarks.GetBookmarks().size(); ++i)
		                  {
			                  const auto entity = bookmarks.GetBookmarks()[i];
			                  if (entity == entt::null || registry == nullptr || !registry->Valid(entity))
			                  {
				                  continue;
			                  }
			                  auto item = Listed(*registry, entity);
			                  item["slot"] = i;
			                  item["set"] = bookmarks.IsSet(i);
			                  item["followed"] = bookmarks.GetFollowed() == i;
			                  items.push_back(std::move(item));
		                  }
		                  return items;
	                  }));
	provider->Add(
	    Query("zones", "The camera zones the land's scripts set: their file, the fence the camera is kept inside, its "
	                   "height limits and the places it is kept out of, whether the camera is inside the fence, and the "
	                   "force field the camera lights up where it hits the fence (its strongest point, how often hit)"),
	    Serve<Locator::cameraZoneSystem>(
	        "the camera zones", [](const ecs::systems::CameraZoneSystemInterface& system, const QueryContext&) {
		        const auto& zones = system.GetZones();
		        Json fence = Json::array();
		        for (const auto& corner : zones.fence)
		        {
			        fence.push_back(Json::array({corner.x, corner.y, corner.z}));
		        }
		        Json exclusions = Json::array();
		        for (const auto& exclusion : zones.exclusions)
		        {
			        exclusions.push_back(
			            {{"position", Json::array({exclusion.position.x, exclusion.position.y, exclusion.position.z})},
			             {"radius", exclusion.radius},
			             {"height", exclusion.height},
			             {"kind", exclusion.kind == camera_zones::Exclusion::Kind::Cylinder ? "cylinder" : "dome"}});
		        }
		        Json result {{"file", system.GetFileName()},
		                     {"exclusions_on", zones.exclusionsOn},
		                     {"fence_on", zones.fenceOn},
		                     {"max_altitude", zones.useMaxAltitude ? Json(zones.maxAltitude) : Json(nullptr)},
		                     {"height_above_land", zones.useHeightAboveLand ? Json(zones.heightAboveLand) : Json(nullptr)},
		                     {"fence", std::move(fence)},
		                     {"exclusions", std::move(exclusions)}};
		        // The wall of light the camera lights up where it hits the fence
		        const auto& forceField = system.GetForceField();
		        const auto points = std::min(forceField.Points(), forceField.Strengths().size());
		        const auto strengths = std::span(forceField.Strengths()).first(points);
		        result["force_field"] = {
		            {"drawn", !forceField.Hidden()},
		            {"showing", !forceField.Hidden() && forceField.Showing()},
		            {"points", points},
		            {"strongest", strengths.empty() ? 0.0f : std::ranges::max(strengths)},
		            {"hits", system.GetFenceHits()},
		        };
		        if (Locator::camera::has_value())
		        {
			        const auto& camera = Locator::camera::value();
			        const auto origin = camera.GetOrigin();
			        result["camera_inside"] =
			            camera_zones::CrossFence(zones.fence, zones.fenceOn, origin, camera.GetFocus() - origin).inside;
			        if (Locator::terrainSystem::has_value())
			        {
				        result["height_limit"] = camera_zones::HeightLimit(
				            zones, Locator::terrainSystem::value().GetHeightAt(glm::vec2(origin.x, origin.z)));
			        }
		        }
		        return result;
	        }));
	provider->Add(
	    Query("cinematic",
	          "The fade, the wide screen, whether the interface is shown, and the words the scripts write on the screen"),
	    Serve<Locator::cinematicDirectorSystem>(
	        "the cinematics", [](const ecs::systems::CinematicDirectorSystemInterface& director, const QueryContext&) {
		        auto texts = Json::array();
		        for (const auto& text : director.GetTexts())
		        {
			        texts.push_back({{"words", gui::ToUtf8(text.words)},
			                         {"corner", {text.corner.x, text.corner.y}},
			                         {"extent", {text.extent.x, text.extent.y}},
			                         {"size", text.size},
			                         {"colour", {text.colour.r, text.colour.g, text.colour.b}},
			                         {"alpha", text.alpha},
			                         {"target", text.target},
			                         {"rate", text.rate}});
		        }
		        return Json {{"fade_finished", director.IsFadeFinished()},
		                     {"fade_colour", director.GetFadeColour()},
		                     {"wide_screen", director.IsWideScreenOn()},
		                     {"wide_screen_fraction", director.GetWideScreenFraction()},
		                     {"interface_active", director.IsInterfaceActive()},
		                     {"close_clipping", director.IsCloseClipping()},
		                     {"texts", texts}};
	        }));
	provider->Add(
	    Query("draw_list",
	          "The land's draw list: the things on the blocks in view within 700 units that are drawn, at most 2999, the "
	          "nearest blocks first, and how many times the land's blocks were classified for the frames; with an id, "
	          "whether that thing is in it and drawn",
	          {{.name = "id", .type = "integer", .description = "A thing to look up in the list", .required = false}}),
	    Serve<Locator::objectDrawListSystem>(
	        "the land's draw list",
	        [](const ecs::systems::ObjectDrawListSystemInterface& drawList, const QueryContext& context) {
		        const auto summary = drawList.GetSummary();
		        Json result {{"active", summary.active},
		                     {"rebuilt", summary.frame.rebuilt},
		                     {"draw_all", summary.frame.drawAll},
		                     {"made_at_turn", Optional(summary.madeAt)},
		                     {"blocks_in_view", summary.blocksInView},
		                     {"blocks_walked", summary.blocksWalked},
		                     {"farthest_walked", summary.farthestWalked},
		                     {"things", summary.things},
		                     {"kept", summary.kept},
		                     {"full", summary.full},
		                     {"rebuilds_asked", summary.rebuildsAsked}};
		        // The list and the land's draw share one classification of the blocks in each frame
		        if (Locator::landDetailSystem::has_value())
		        {
			        result["land_detail_classifications"] = Locator::landDetailSystem::value().Classifications();
		        }
		        if (const auto entity = EntityParam(context.params); entity.has_value())
		        {
			        result["listed"] = drawList.Listed(*entity);
			        if (const auto* registry = Registry(); registry != nullptr)
			        {
				        result["drawn"] = drawList.Drawn(*registry, *entity);
			        }
		        }
		        return result;
	        }));
	provider->Add(
	    Query("script_control", "The tasks with the camera and the dialogue, the scripts' countdown timer, the scripts' "
	                            "spoken lines and the villagers drawn in high detail"),
	    [](const QueryContext& /*c*/) {
		    Json result = Json::object();
		    if (Locator::scriptControlSystem::has_value())
		    {
			    const auto& control = Locator::scriptControlSystem::value();
			    result["camera_owner"] = control.GetCameraOwner();
			    const auto& countdown = control.GetCountdownTimer();
			    result["countdown"] = {{"running", countdown.IsRunning()},
			                           {"visible", countdown.IsVisible()},
			                           {"turns_left", countdown.GetTurnsLeft()}};
		    }
		    if (Locator::dialogueControlSystem::has_value())
		    {
			    result["dialogue_owner"] = Locator::dialogueControlSystem::value().GetOwner();
		    }
		    if (Locator::helpSpeechSystem::has_value())
		    {
			    const auto& speech = Locator::helpSpeechSystem::value();
			    result["speech"] = {{"spoken_texts", speech.GetSpokenTextCount()}, {"lines_playing", speech.GetLineCount()}};
		    }
		    if (Locator::highDetailSystem::has_value())
		    {
			    Json items = Json::array();
			    if (const auto* registry = Registry(); registry != nullptr)
			    {
				    registry->Each<const HighDetail>([&items, registry](entt::entity entity, const HighDetail& detail) {
					    auto item = Listed(*registry, entity);
					    item["detailed_model"] = detail.usualModel.has_value();
					    item["face"] = detail.face.has_value() ? static_cast<int>(*detail.face) : -1;
					    item["follow_intro_hand"] = detail.orders.followIntroHand;
					    item["turn_at_once"] = detail.orders.turnAtOnce;
					    if (detail.eyes.has_value())
					    {
						    Json eyes = {{"blinking", detail.eyes->blink.blinking},
						                 {"until_next_blink", detail.eyes->blink.untilNext},
						                 {"into_blink", detail.eyes->blink.into},
						                 {"roll", detail.eyes->roll},
						                 {"drawn", detail.drawnEyes.has_value()}};
						    if (detail.drawnEyes.has_value())
						    {
							    const auto& right = detail.drawnEyes->eyes[0].eyeball[3];
							    eyes["closed"] = detail.drawnEyes->closed;
							    eyes["right_eye"] = {right.x, right.y, right.z};
						    }
						    item["eyes"] = std::move(eyes);
					    }
					    items.push_back(std::move(item));
				    });
			    }
			    result["high_detail"] = std::move(items);
		    }
		    return QueryResult::Value(std::move(result));
	    });
	provider->Add(
	    Query("editor", "The editor: open, its tool, its camera, what is selected (the main pick and all picks), the steps "
	                    "to undo and redo with their labels, and the counts of the streams and footpaths that it drew"),
	    Serve<Locator::editorSystem>("the editor",
	                                 [](const ecs::systems::EditorSystemInterface& editor, const QueryContext& /*c*/) {
		                                 auto picks = Json::array();
		                                 for (const auto entity : editor.GetSelection().All())
		                                 {
			                                 picks.push_back(Id(entity));
		                                 }
		                                 const auto& history = editor.GetHistory();
		                                 return Json {{"open", editor.IsOpen()},
		                                              {"tool", static_cast<int>(editor.GetTool())},
		                                              {"camera_mode", static_cast<int>(editor.GetCameraMode())},
		                                              {"stepping", editor.IsStepping()},
		                                              {"selected", Id(editor.GetSelection().Get())},
		                                              {"selected_all", std::move(picks)},
		                                              {"undo_steps", history.UndoCount()},
		                                              {"redo_steps", history.RedoCount()},
		                                              {"undo_label", history.UndoLabel().value_or("")},
		                                              {"redo_label", history.RedoLabel().value_or("")},
		                                              {"drawn_streams", editor.GetDrawnPaths().streams.size()},
		                                              {"drawn_footpaths", editor.GetDrawnPaths().footpaths.size()}};
	                                 }));
	return provider;
}

} // namespace openblack::inspector::game_providers
