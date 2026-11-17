/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <utility>

#include "3D/L3DMesh.h"
#include "CHLApi/NativeHelpers.h"
#include "Common/ErrorReportsInterface.h"
#include "Common/ErrorReportsRules.h"
#include "Common/EventManager.h"
#include "Common/GameRandom.h"
#include "Common/MainThreadQueue.h"
#include "Common/RandomNumberManager.h"
#include "Creature/BonusCreatures.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Systems/ClanCreatureSystemInterface.h"
#include "ECS/Systems/ComputerSettingsSystemInterface.h"
#include "ECS/Systems/GatheringBoxSystemInterface.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/LobbyRoomSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/ToolRunSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameProviderShared.h"
#include "Help/ScriptSpirits.h"
#include "Locator.h"
#include "Online/HttpClientInterface.h"
#include "Online/LobbyRoom.h"
#include "Online/NetworkSession.h"
#include "Online/WebSocketInterface.h"
#include "Profiler.h"
#include "Resources/ResourcesInterface.h"
#include "Visual/RendererRequestsInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

// The engine's services

namespace
{

/// The names of the breeder's bonus creatures the debug option unlocks
Json UnlockedBonusCreatureNames(const creature_swap::Unlocked& unlocked)
{
	auto names = Json::array();
	for (size_t i = 0; i < creature_swap::k_BonusCount; ++i)
	{
		if (unlocked.test(i))
		{
			names.push_back(std::string(creature_swap::NameOf(static_cast<creature_swap::Bonus>(i))));
		}
	}
	return names;
}

} // namespace

namespace openblack::inspector::game_providers
{

namespace
{

/// The lobby list and the room of the lobby service, or null when the game has no lobby room system
[[nodiscard]] Json LobbyRoomJson()
{
	if (!Locator::lobbyRoom::has_value())
	{
		return nullptr;
	}
	const auto& system = Locator::lobbyRoom::value();
	Json room = nullptr;
	if (const auto* joined = system.Room(); joined != nullptr)
	{
		constexpr std::array<std::string_view, 6> k_PhaseNames {"open", "countdown", "queued", "assigned", "in_game", "ended"};
		const auto phase = static_cast<size_t>(joined->Phase());
		room = Json {
		    {"id", joined->Data().id},
		    {"name", joined->Data().name},
		    {"phase", phase < k_PhaseNames.size() ? Json(k_PhaseNames[phase]) : Json(phase)},
		    {"host", joined->IsHost()},
		    {"members", joined->Data().members.size()},
		    {"revision", joined->Revision()},
		};
	}
	return Json {
	    {"build_id", system.BuildId()},
	    {"listed", system.List().Entries().size()},
	    {"list_revision", system.List().Revision()},
	    {"room", std::move(room)},
	};
}

} // namespace

std::unique_ptr<ProviderInterface> EngineProvider()
{
	auto provider = std::make_unique<FunctionProvider>("engine");
	provider->Add(
	    Query("online",
	          "The web client, the WSS client, the lobby client, the lobby room and the network game: the requests that wait, "
	          "the open sockets, the session, the push channel, and the turns and checksums of the game server"),
	    [](const QueryContext& /*context*/) {
		    Json result {
		        {"http_pending", Locator::httpClient::has_value() ? Locator::httpClient::value().Pending() : 0},
		        {"websockets", Locator::webSocketClient::has_value() ? Locator::webSocketClient::value().Count() : 0},
		    };
		    const auto* session = Locator::networkGame::has_value() ? Locator::networkGame::value().Session() : nullptr;
		    if (session == nullptr)
		    {
			    result["network_game"] = nullptr;
		    }
		    else
		    {
			    const auto& compares = session->GetCompares();
			    const auto& joined = session->GetJoined();
			    const auto roundTrip = session->RoundTrip();
			    result["network_game"] = Json {
			        {"state", online::StateName(session->GetState())},
			        {"end", online::EndName(session->GetEnd())},
			        {"role", joined.has_value() ? Json(game_socket::RoleName(joined->role)) : Json(nullptr)},
			        {"slot", joined.has_value() && joined->slot.has_value() ? Json(*joined->slot) : Json(nullptr)},
			        {"turns_done", session->TurnsDone()},
			        {"checksums_compared", compares.compared},
			        {"checksums_same", compares.same},
			        {"first_different", compares.firstDifferent.has_value() ? Json(*compares.firstDifferent) : Json(nullptr)},
			        {"resyncing", session->ShowsResync()},
			        {"resync_tries", session->Resync().Tries()},
			        {"resumes", session->Resumes()},
			        {"buffer_ms", static_cast<double>(session->Playout().BufferMicroseconds()) / 1000.0},
			        {"round_trip_ms", roundTrip.has_value() ? Json(static_cast<double>(*roundTrip) / 1000.0) : Json(nullptr)},
			    };
		    }
		    // The store of the lands of network games that the lobby gives (deviation 169)
		    result["online_map_store"] = Locator::onlineMapStore::has_value();
		    // The clan creature: the clan that the player uses, the start creature and the creature file kept for the upload
		    if (Locator::clanCreatureSystem::has_value())
		    {
			    const auto& clan = Locator::clanCreatureSystem::value();
			    const auto used = clan.UsedClan();
			    const auto start = clan.StartCreature();
			    result["clan_creature"] = Json {
			        {"clan", used.has_value() ? Json(*used) : Json(nullptr)},
			        {"start_bytes", start.has_value() ? Json(start->file.size()) : Json(nullptr)},
			        {"start_alignment", start.has_value() ? Json(start->alignment) : Json(nullptr)},
			        {"kept_bytes", clan.KeptCreatureFile().size()},
			    };
		    }
		    else
		    {
			    result["clan_creature"] = nullptr;
		    }
		    result["lobby_room"] = LobbyRoomJson();
		    if (!Locator::lobbyClient::has_value())
		    {
			    result["lobby"] = nullptr;
			    return QueryResult::Value(std::move(result));
		    }
		    const auto& lobby = Locator::lobbyClient::value();
		    const auto state = lobby.GetState();
		    const auto account = lobby.GetAccount();
		    result["lobby"] = Json {
		        {"configured", lobby.IsConfigured()},
		        {"state", state == ecs::systems::LobbySessionState::LoggedIn    ? "logged_in"
		                  : state == ecs::systems::LobbySessionState::LoggingIn ? "logging_in"
		                                                                        : "logged_out"},
		        {"account", account.has_value() ? Json(account->name) : Json(nullptr)},
		        {"push_open", lobby.IsPushOpen()},
		        {"known_servers", lobby.GetKnownServers().size()},
		    };
		    return QueryResult::Value(std::move(result));
	    });
	auto away = Query("online_away",
	                  "Tools: the client of a network game drops its socket, as at a loss of the network, and tries to come "
	                  "back to the game server only after the seconds",
	                  {{.name = "seconds",
	                    .type = "number",
	                    .description = "The seconds before the client tries to come back, 0 to 600",
	                    .required = true}});
	away.writes = true;
	provider->Add(std::move(away), [](const QueryContext& context) {
		const auto seconds = context.params.find("seconds");
		if (seconds == context.params.end() || !seconds->is_number() || seconds->get<double>() < 0.0 ||
		    seconds->get<double>() > 600.0)
		{
			return QueryResult::Error("engine.online_away needs the seconds, 0 to 600");
		}
		const auto microseconds = static_cast<int64_t>(seconds->get<double>() * 1'000'000.0);
		if (!Locator::networkGame::has_value() || !Locator::networkGame::value().GoAwayForTest(microseconds))
		{
			return QueryResult::Error("engine.online_away needs a network game that plays");
		}
		return QueryResult::Value({{"away", true}, {"seconds", seconds->get<double>()}});
	});
	provider->Add(
	    Query("config", "The engine's settings: what is drawn, the display and the camera's clip"),
	    Serve<Locator::config>("the config", [](const EngineConfig& config, const QueryContext& /*context*/) {
		    return Json {
		        {"draw",
		         {{"sky", config.drawSky},
		          {"water", config.drawWater},
		          {"island", config.drawIsland},
		          {"entities", config.drawEntities},
		          {"vegetation", config.drawVegetation},
		          {"sprites", config.drawSprites},
		          {"bounding_boxes", config.drawBoundingBoxes}}},
		        {"shadows", {{"creatures", config.drawCreatureShadows}, {"dynamic_trees", config.dynamicTreeShadows}}},
		        {"wireframe", config.wireframe},
		        {"unlocked_bonus_creatures", UnlockedBonusCreatureNames(config.unlockedBonusCreatures)},
		        {"time_of_day", config.timeOfDay},
		        {"resolution", {config.resolution.x, config.resolution.y}},
		        {"vsync", config.vsync},
		        {"camera_fov", config.cameraXFov},
		        {"camera_near_clip", config.cameraNearClip},
		        {"gui_scale", config.guiScale},
		        {"running", config.running},
		    };
	    }));
	provider->Add(
	    Query("script_reads",
	          "What the scripts read of the bonus creatures and the advisors now. every_computer is what a script that each "
	          "computer runs reads (in a network game, the same on each computer). this_computer is what a help script of "
	          "this computer reads. The spirits are those of the script spirits 3 (by the alignment) and 4 (the opposite "
	          "of the alignment); the query draws no random number"),
	    [](const QueryContext& /*context*/) {
		    const auto reads = [](bool readsThisComputer) {
			    const auto unlocked = readsThisComputer && Locator::config::has_value()
			                              ? Locator::config::value().unlockedBonusCreatures
			                              : chlapi::ScriptBonusCreatures();
			    const auto alignmentSpirit = chlapi::ScriptHelpSpirit(3, readsThisComputer);
			    const auto oppositeSpirit = chlapi::ScriptHelpSpirit(4, readsThisComputer);
			    return Json {
			        {"unlocked_bonus_creatures", UnlockedBonusCreatureNames(unlocked)},
			        {"alignment_spirit", alignmentSpirit},
			        {"opposite_spirit", oppositeSpirit},
			        {"good_spirit_played", chlapi::ScriptSpiritPlayed(help::script_spirits::k_GoodSpirit, readsThisComputer)},
			        {"evil_spirit_played", chlapi::ScriptSpiritPlayed(help::script_spirits::k_EvilSpirit, readsThisComputer)},
			    };
		    };
		    const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
		    return QueryResult::Value(
		        {{"network_game", networkGame},
		         {"turn", Locator::time::has_value() ? Json(Locator::time::value().GetTurn()) : Json(nullptr)},
		         {"every_computer", reads(!networkGame)},
		         {"this_computer", reads(true)}});
	    });
	provider->Add(
	    Query("computer_settings", "The settings of this computer (settings.json) as the game read them at the start, with the "
	                               "changes kept since; a value the file does not keep is null"),
	    Serve<Locator::computerSettingsSystem>(
	        "the computer settings",
	        [](const ecs::systems::ComputerSettingsSystemInterface& system, const QueryContext& /*context*/) {
		        const auto& settings = system.GetSettings();
		        const auto value = [](const auto& kept) -> Json { return kept.has_value() ? Json(*kept) : Json(nullptr); };
		        return Json {
		            {"detail_level", value(settings.detailLevel)},
		            {"max_detail_level", value(settings.maxDetailLevel)},
		            {"effects_volume", value(settings.effectsVolume)},
		            {"music_volume", value(settings.musicVolume)},
		            {"width", value(settings.width)},
		            {"height", value(settings.height)},
		            {"window_mode", value(settings.windowMode)},
		            {"vsync", value(settings.vsync)},
		        };
	        }));
	provider->Add(Query("frame", "The last frame: its time, the window's size, the renderer's flags and how much it draws"),
	              [](const QueryContext& /*context*/) {
		              Json result = Json::object();
		              if (Locator::profiler::has_value())
		              {
			              const auto& profiler = Locator::profiler::value();
			              const auto& entry = profiler.GetEntries()[profiler.GetEntryIndex(-1)];
			              result["frame_ms"] =
			                  std::chrono::duration<double, std::milli>(entry.frameEnd - entry.frameStart).count();
		              }
		              if (Locator::windowing::has_value())
		              {
			              const auto size = Locator::windowing::value().GetSize();
			              result["window"] = {size.x, size.y};
		              }
		              if (Locator::rendererRequests::has_value())
		              {
			              result["renderer_debug"] = Locator::rendererRequests::value().GetDebug();
			              result["renderer_profile"] = Locator::rendererRequests::value().GetProfile();
		              }
		              if (Locator::rendereringSystem::has_value())
		              {
			              const auto& context = Locator::rendereringSystem::value().GetContext();
			              result["drawn_entities"] = context.entityDraws.size();
			              result["instances"] = context.instanceUniforms.size();
			              result["trees"] = context.treeInstanceCount;
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("resources", "How many of each kind of resource the caches hold"),
	              Serve<Locator::resources>("the resources",
	                                        [](resources::ResourcesInterface& resources, const QueryContext& /*context*/) {
		                                        return Json {
		                                            {"meshes", resources.GetMeshes().Size()},
		                                            {"textures", resources.GetTextures().Size()},
		                                            {"animations", resources.GetAnimations().Size()},
		                                            {"sounds", resources.GetSounds().Size()},
		                                            {"levels", resources.GetLevels().Size()},
		                                            {"creature_minds", resources.GetCreatureMinds().Size()},
		                                            {"particle_files", resources.GetParticleFiles().Size()},
		                                            {"camera_paths", resources.GetCameraPaths().Size()},
		                                        };
	                                        }));
	provider->Add(
	    Query("tool_run", "Which logs and tools of a tool's run are open: the file of the commands of each turn, the file "
	                      "of the commands of the testbed scenario, the file of the checksums, the guard of the frame steps "
	                      "and the fight autopilot"),
	    Serve<Locator::toolRunSystem>(
	        "the logs of a tool's run", [](const ecs::systems::ToolRunSystemInterface& toolRun, const QueryContext& /*c*/) {
		        const auto state = toolRun.GetState();
		        return Json {{"command_log", state.commandLog},         {"scenario_gives", state.scenarioGives},
		                     {"checksum_log", state.checksumLog},       {"frame_guard", state.frameGuard},
		                     {"fight_autopilot", state.fightAutopilot}, {"test_leave_done", state.testLeaveDone}};
	        }));
	provider->Add(Query("error_reports", "The error and crash reports to Sentry: whether the program has a DSN, whether "
	                                     "the reports go now, the option of the player, the role, the release, the "
	                                     "environment, the choice of the lobby account and whether the reports name the "
	                                     "account. The answer never holds the DSN or the name."),
	              Serve<Locator::errorReports>(
	                  "the error reports", [](const error_reports::ErrorReportsInterface& reports, const QueryContext& /*c*/) {
		                  const auto state = reports.GetState();
		                  return Json {{"has_dsn", state.hasDestination},
		                               {"running", state.running},
		                               {"enabled", state.enabled},
		                               {"role", std::string(error_reports::RoleTag(state.role))},
		                               {"release", state.release},
		                               {"environment", state.environment},
		                               {"account_choice",
		                                state.account.has_value()
		                                    ? Json(std::string(error_reports::ReportsConsentText(state.account->choice)))
		                                    : Json(nullptr)},
		                               {"personal_data", state.personalData}};
	                  }));
	provider->Add(Query("services", "Which of the engine's services are there, the game's random seeds, the game's path, the "
	                                "debug windows' focus and the inspector's port"),
	              [](const QueryContext& /*context*/) {
		              Json result = {
		                  {"info_tables", Locator::infoConstants::has_value()},
		                  {"events", Locator::events::has_value()},
		                  {"rng", Locator::rng::has_value()},
		              };
		              if (Locator::mainThreadQueue::has_value())
		              {
			              // The work that the turn gave to the main thread and that still waits
			              result["main_thread_work_waits"] = Locator::mainThreadQueue::value().GetPendingCount();
		              }
		              if (Locator::gameRandom::has_value())
		              {
			              const auto seeds = Locator::gameRandom::value().GetSeeds();
			              result["seeds"] = {{"synced", seeds.synced}, {"local", seeds.local}};
		              }
		              if (Locator::filesystem::has_value())
		              {
			              result["game_path"] = Locator::filesystem::value().GetGamePath().generic_string();
		              }
		              if (Locator::debugGui::has_value())
		              {
			              result["debug_gui_focus"] = Locator::debugGui::value().StealsFocus();
			              result["mouse_over_debug_window"] = Locator::debugGui::value().IsMouseOverWindow();
		              }
		              if (Locator::inspector::has_value())
		              {
			              result["inspector_port"] = Locator::inspector::value().GetPort();
		              }
		              return QueryResult::Value(std::move(result));
	              });
	return provider;
}

} // namespace openblack::inspector::game_providers
