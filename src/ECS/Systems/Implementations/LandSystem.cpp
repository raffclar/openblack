/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LandSystem.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <LHVM.h>
#include <LNDFile.h>
#include <OnlineMapFile.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/FlatLand.h"
#include "3D/GodsRingLand.h"
#include "3D/LandData.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandReuse.h"
#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/StartCamera.h"
#include "Common/LoadTimer.h"
#include "Common/MachineClock.h"
#include "Common/MainThreadQueue.h"
#include "Common/StringUtils.h"
#include "Debug/TestbedDispenserGrid.h"
#include "ECS/Archetypes/CloudArchetype.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PrayerPower.h"
#include "ECS/Components/Transform.h"
#include "ECS/ComputerPlayerStart.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Map.h"
#include "ECS/ProfileAlignment.h"
#include "ECS/RealWeatherLand.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AppleBarrelSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/FloatingNumberSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/InspectorLoading.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RealWeatherSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SimulationSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Help/HelpProfile.h"
#include "Help/Spirits.h"
#include "LHScriptX/MapScript.h"
#include "LHScriptX/Script.h"
#include "LandLaidOutOrder.h"
#include "Level.h"
#include "Locator.h"
#include "MainThreadWork.h"
#include "Online/GodsRingMap.h"
#include "Online/GodsRingMapFile.h"
#include "Online/OnlineMapLand.h"
#include "Profiler.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptStart.h"
#include "Serializer/FotFile.h"
#include "Sound/SoundRequestInterface.h"
#include "Temple/TempleHelp.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::ecs::systems
{
namespace
{

/// How far the testbed's player's influence reaches from its middle, and the prayer power their worship has stored
constexpr float k_TestbedInfluenceRadius = 400.0f;
constexpr float k_TestbedPrayer = 1.0e6f;
/// How many times longer a turn takes than at the normal speed, at the normal speed
constexpr float k_NormalTurnDurationMultiplier = 1.0f;

/// The landscape that the file of a land of a network game holds
std::vector<uint8_t> LandscapeBytes(const onlinemap::OnlineMapFile& file)
{
	std::vector<uint8_t> bytes(file.landscape.size());
	std::ranges::transform(file.landscape, bytes.begin(), [](std::byte b) { return static_cast<uint8_t>(b); });
	return bytes;
}

} // namespace

void SendAdvisorsHome(bool helpScript)
{
	if (!Locator::advisorSystem::has_value() || !Locator::advisorSystem::value().IsLoaded())
	{
		return;
	}
	auto& advisors = Locator::advisorSystem::value().GetController();
	advisors.SpiritHome(1, helpScript);
	advisors.SpiritHome(2, helpScript);
}

std::filesystem::path LandSystem::ChallengePath()
{
	if (auto path = View().ChallengeFile(); path.has_value())
	{
		return *path;
	}
	return Locator::filesystem::value().GetPath<filesystem::Path::Quests>() / "challenge.chl";
}

bool LandSystem::LoadMap(const std::filesystem::path& path, loading::LoadingClock::Mode look, land_change::Kind change)
{
	// A land loads on the main thread. A turn on another thread, such as a script of the story that goes to the next land,
	// waits while the main thread loads it, and then goes on as it does on the main thread.
	if (InTurnOnOtherThread())
	{
		return CallOnMainThread(MainThreadApi::Renderer, [this, &path, look, change] { return LoadMap(path, look, change); });
	}
	const InspectorLoading loading(path.filename().generic_string());
	auto& fileSystem = Locator::filesystem::value();

	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Could not find script {}", path.generic_string());
		return false;
	}

	LoadTimer timer(fmt::format("land {}", path.stem().string()));
	View().BeginLoadingScreen(look);
	const auto data = fileSystem.ReadAll(path);
	auto source = std::string(reinterpret_cast<const char*>(data.data()), data.size());
	// A land of a network game holds its script and its landscape in one file. Its script loads the landscape that it
	// holds.
	_mapLandscape.clear();
	if (online_map_land::IsOnlineMap(path.generic_string()))
	{
		const auto file = onlinemap::Parse(std::as_bytes(std::span(data)));
		if (!file.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The land {} is not a land of a network game", path.generic_string());
			View().EndLoadingScreen();
			return false;
		}
		source = std::string(file->script);
		_mapLandscape = LandscapeBytes(*file);
	}

	PrepareNewLand(change);
	// The script of the land gives the start place of the camera again, if it gives one
	_startCameraPlace.reset();
	timer.Step("reset");
	View().ShowLoadingFrame();

	// A playground land is played as a skirmish, in which losing a temple doesn't end the game. A land of a network game
	// is played the same way: no story starts on it and losing a temple does not end the game.
	Locator::entitiesRegistry::value().Context().skirmish =
	    IsPlaygroundLand(path) || online_map_land::IsOnlineMap(path.generic_string());
	_landPath = path;

	lhscriptx::Script script;
	_runsNetworkMapScript = _humansFromSession;
	try
	{
		// The tips screen is drawn again as the map's commands are carried out, when it is due
		script.Load(source, [this]() { View().ShowLoadingFrame(); });
	}
	catch (const std::exception& e)
	{
		// A script that can't be read leaves the land as far as it got, rather than ending the game
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Error in the map script {}: {}", path.generic_string(), e.what());
	}
	_runsNetworkMapScript = false;
	_mapLandscape.clear();

	timer.Step("map script");
	// Every land of a network game fills its slots with no human when the land is laid out. Without a network session,
	// only a land made for network play does.
	const bool fillsSlots = _humansFromSession || _allComputerGods || online_map_land::IsOnlineMap(path.generic_string());
	LayOutLand(path, fillsSlots ? std::optional<std::span<const PlayerNames>>(computer_player::k_SlotGods) : std::nullopt);

	timer.Step("laid out");
	// A new game starts the player as good or evil as their profile last kept them, once the land is set up
	if (land_change::RulesFor(change).alignmentFromProfile)
	{
		StartPlayerAtProfileAlignment();
	}
	// The square of the player of this computer in the sheet of the players' symbols takes the profile's symbol
	View().MakeSymbolSheet();
	// A game that starts here can start with the camera at the player's temple
	if (start_camera::StartsAtTemple(
	        {.change = change, .networkGame = Locator::time::value().IsNetworkGame(), .scriptPlace = _startCameraPlace}))
	{
		View().StartCameraAtTemple();
	}
	View().ShowLoadingFrame();
	StartNewLand(change);
	timer.Step("start");
	View().EndLoadingScreen();
	return true;
}

bool LandSystem::IsPlaygroundLand(const std::filesystem::path& path) const
{
	// The script is matched by where its file is found, however its path was given
	const auto scriptFile = Level::ResolveScriptPath(path);
	bool playground = false;
	Locator::resources::value().GetLevels().Each([&scriptFile, &playground](entt::id_type /*id*/, const Level& level) {
		playground = playground || (level.GetType() == Level::LandType::Skirmish &&
		                            Level::IsSameScript(Level::ResolveScriptPath(level.GetScriptPath()), scriptFile));
	});
	return playground;
}

bool LandSystem::LoadMapWithFreshScripts(const std::filesystem::path& path, land_change::Kind change)
{
	if (InTurnOnOtherThread())
	{
		return CallOnMainThread(MainThreadApi::Renderer,
		                        [this, &path, change] { return LoadMapWithFreshScripts(path, change); });
	}
	// Outside the story, a land is loaded with the challenge's scripts started again from scratch: every task of the
	// last land stops, the scripts' variables are cleared and the challenge's scripts that start by themselves start
	// again. None of the last land's scripts go on running on the new land.
	if (Locator::vm::has_value())
	{
		// The program starting again frees every place of the scripts' objects, while the last land's objects are still
		// there to be let go back into the game
		if (Locator::scriptObjects::has_value())
		{
			Locator::scriptObjects::value().Reset();
		}
		if (Locator::chlapi::has_value())
		{
			Locator::chlapi::value().ResetSwitches();
		}
		auto& fileSystem = Locator::filesystem::value();
		const auto challengePath = ChallengePath();
		try
		{
			Locator::vm::value().LoadBinary(fileSystem.ReadAll(challengePath));
		}
		catch (const std::exception& err)
		{
			Locator::vm::value().StopAllTasks();
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to read challenge file at {}: {}", challengePath.generic_string(),
			                    err.what());
		}
	}
	return LoadMap(path, loading::LoadingClock::Mode::Tips, change);
}

void LandSystem::LoadTestbed()
{
	const InspectorLoading loading("testbed");
	// No script runs on the testbed: the story's would set its time of day and stop its clock a few turns in
	if (Locator::vm::has_value())
	{
		Locator::vm::value().StopAllTasks();
	}
	View().BeginLoadingScreen(loading::LoadingClock::Mode::Tips);
	PrepareNewLand(land_change::Kind::Testbed);
	_landPath = k_TestbedLandPath;
	// The game builds the flat land, so it has no landscape file. The name of the last landscape does not stay.
	_landscapePath.clear();
	View().ShowLoadingFrame();
	InitializeLevel(flat_land::Build());
	SetUpLandscape();
	View().ShowLoadingFrame();

	// Looking down over the middle of the map, from the south, a little to its east
	const auto& land = Locator::terrainSystem::value();
	const auto middle = (land.GetExtent().minimum + land.GetExtent().maximum) * 0.5f;
	const auto ground = land.GetHeightAt(middle);
	const float x = middle.x + flat_land::k_CameraAside;
	Locator::camera::value()
	    .SetOrigin({x, ground + flat_land::k_CameraHeight, middle.y - flat_land::k_CameraBack})
	    .SetFocus({x, ground, middle.y});

	// The testbed has no temple to give its player influence, so a source of influence over its middle gives them the
	// reach a citadel would: the miracles cast only in influence may be cast there, and nowhere beyond. Their worship
	// stands behind them with prayer power to spare, which the miracles still draw from. A dispenser of every miracle
	// stands in a grid in front of the camera.
	auto& registry = Locator::entitiesRegistry::value();
	const auto influence = registry.Create();
	registry.Assign<components::Transform>(influence, glm::vec3(middle.x, ground, middle.y), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<components::InfluenceSource>(influence, PlayerNames::PLAYER_ONE, k_TestbedInfluenceRadius);
	registry.Each<const components::Player>([&registry](entt::entity entity, const components::Player& player) {
		if (player.name == PlayerNames::PLAYER_ONE)
		{
			registry.AssignOrReplace<components::PrayerPower>(entity, k_TestbedPrayer, false);
		}
	});
	testbed_dispensers::PlaceGrid(middle);
	// The loading screen ends with the camera and the testbed's things in place, so that the first frame drawn behind
	// it draws what the game's first frames draw
	View().EndLoadingScreen();

	StartNewLand(land_change::Kind::Testbed);

	// The testbed comes with its window of scenarios to try out on it
	View().TestbedLoaded();
}

bool LandSystem::LoadGodsRingMap()
{
	// The village of each slot comes from a land of the game for network play, which the player has. The game makes the
	// map's file from it, and loads the file as a player loads the file from the lobby.
	const auto file = gods_ring_map_file::MakeFromGameFolder(Locator::filesystem::value());
	return file.has_value() && LoadGodsRingMapFile(*file);
}

bool LandSystem::LoadGodsRingMapFile(std::span<const uint8_t> file)
{
	const InspectorLoading loading {std::string(gods_ring_map::k_File)};
	auto parts = gods_ring_map_file::Read(file);
	if (!parts.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the map for 32 gods does not read");
		return false;
	}

	// No script of the story runs on the map, as on the testbed
	if (Locator::vm::has_value())
	{
		Locator::vm::value().StopAllTasks();
	}
	View().BeginLoadingScreen(loading::LoadingClock::Mode::Tips);
	PrepareNewLand(land_change::Kind::Testbed);
	_landPath = gods_ring_map::k_File;
	// The game builds this land too, so it has no landscape file
	_landscapePath.clear();
	View().ShowLoadingFrame();
	InitializeLevel(std::move(parts->land));
	SetUpLandscape();
	View().ShowLoadingFrame();

	const auto slots = gods_ring_map::LayOutSlots(gods_ring_land::k_MapMiddle);
	// The map has a god in each of its 32 slots (deviation 166). The land has the 8 players of a land; the gods after the
	// seventh get their players before the script, in the order of their numbers, as each slot of a land has its player.
	for (const auto& slot : slots)
	{
		if (slot.god.has_value() && !player_slots::IsLandPlayer(*slot.god))
		{
			Locator::playerSystem::value().EnsurePlayer(*slot.god);
		}
	}
	try
	{
		lhscriptx::Script script;
		script.Load(parts->script, [this]() { View().ShowLoadingFrame(); });
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Error in the script of the map for 32 gods: {}", e.what());
	}
	std::vector<PlayerNames> gods;
	for (const auto& slot : slots)
	{
		if (slot.god.has_value())
		{
			gods.push_back(*slot.god);
		}
	}
	// As on a land: the land is laid out, then the slots fill. The map has no footpath file.
	LayOutLand(std::nullopt, std::span<const PlayerNames>(gods));
	View().MakeSymbolSheet();

	// The camera looks at the first slot's village from beyond its citadel
	const auto& first = slots.front();
	const glm::vec2 outward {std::cos(first.outward), std::sin(first.outward)};
	const auto& land = Locator::terrainSystem::value();
	const auto eye = first.citadel + (outward * gods_ring_map::k_StartCameraBack);
	const auto focus = first.citadel - (outward * gods_ring_map::k_StartCameraAhead);
	Locator::camera::value()
	    .SetOrigin({eye.x, land.GetHeightAt(eye) + gods_ring_map::k_StartCameraHeight, eye.y})
	    .SetFocus({focus.x, land.GetHeightAt(focus), focus.y});
	// A network game on the map starts with the camera at the player's temple, as on every land of a network game
	if (Locator::time::value().IsNetworkGame())
	{
		View().StartCameraAtTemple();
	}
	View().EndLoadingScreen();
	StartNewLand(land_change::Kind::Testbed);
	return true;
}

void LandSystem::LayOutLand(const std::optional<std::filesystem::path>& footpathsOf,
                            std::optional<std::span<const PlayerNames>> slotGods)
{
	for (const auto step : land_laid_out::k_Order)
	{
		switch (step)
		{
		case land_laid_out::Step::PlaceThings:
			// The game puts each thing on the map as the script makes it: the things go into their map cells now
			Locator::entitiesMap::value().Sync();
			break;
		case land_laid_out::Step::FootpathObstacles:
			// No footpath goes round the things in the map cells
			if (Locator::footpathSystem::has_value())
			{
				Locator::footpathSystem::value().ProcessObstacleChanges();
			}
			break;
		case land_laid_out::Step::ScenicForests:
			if (Locator::forestSystem::has_value())
			{
				Locator::forestSystem::value().MakeScenicForests();
			}
			break;
		case land_laid_out::Step::ForestsToTowns:
			if (Locator::forestSystem::has_value())
			{
				Locator::forestSystem::value().AssignForestsToTowns();
			}
			break;
		case land_laid_out::Step::WorshipSites:
			if (Locator::worshipSiteSystem::has_value())
			{
				Locator::worshipSiteSystem::value().LandLaidOut();
			}
			break;
		case land_laid_out::Step::InfluencePower:
			if (Locator::influenceSystem::has_value())
			{
				Locator::influenceSystem::value().LandLaidOut();
			}
			break;
		case land_laid_out::Step::FootpathFile:
			if (footpathsOf.has_value())
			{
				LoadFootpathFile(*footpathsOf);
			}
			break;
		case land_laid_out::Step::FillSlots:
			if (slotGods.has_value())
			{
				FillSlotsOfNetworkLand(*slotGods);
			}
			break;
		}
	}
}

void LandSystem::LoadFootpathFile(const std::filesystem::path& landPath)
{
	// Each released map comes with an optional .fot file which contains the footpath information for the map
	auto& fileSystem = Locator::filesystem::value();
	const auto stem = string_utils::LowerCase(landPath.stem().generic_string());
	const auto fotPath = fileSystem.GetPath<filesystem::Path::Landscape>() / fmt::format("{}.fot", stem);
	if (fileSystem.Exists(fotPath))
	{
		FotFile fotFile;
		fotFile.Load(fotPath);
	}
	else
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The map at {} does not come with a footpath file. Expected {}",
		                   landPath.generic_string(), fotPath.generic_string());
	}
}

void LandSystem::FillSlotsOfNetworkLand(std::span<const PlayerNames> gods)
{
	if (!Locator::computerPlayerSystem::has_value() || !Locator::playerSystem::has_value())
	{
		return;
	}
	auto& minds = Locator::computerPlayerSystem::value();
	if (_allComputerGods)
	{
		// The AI soak: no human plays, so every god with a citadel gets a computer god as the land loads
		minds.FillEmptySlots(gods, {});
		return;
	}
	if (_humansFromSession)
	{
		minds.WaitForHumans();
		return;
	}
	// On this computer alone, the local player is the one human
	const std::array humans {Locator::playerSystem::value().GetLocalPlayer()};
	minds.FillEmptySlots(gods, humans);
}

bool LandSystem::LoadSavedLand(const std::filesystem::path& landPath, const std::filesystem::path& landscape,
                               const std::function<bool()>& restore, SavedLandUse use)
{
	const InspectorLoading loading(landPath.filename().generic_string());
	LoadTimer timer(fmt::format("saved land {}", landPath.stem().string()));
	// A world state of the land that is open keeps the land: the landscape is the same, so only the altitudes that the
	// world changed go back. The world loads its own changes after. The load is short and shows no loading screen.
	const bool builtByGame = landPath == k_TestbedLandPath || gods_ring_map::IsThisMap(landPath.generic_string());
	const bool keepsLand = land_reuse::KeepsLand({
	    .worldState = use == SavedLandUse::WorldState,
	    .landLoaded = Locator::terrainSystem::has_value() && Locator::terrainSystem::value().IsLoaded(),
	    .openLand = _landPath,
	    .openLandscape = _landscapePath,
	    .savedLand = landPath,
	    .savedLandscape = landscape,
	    .builtByGame = builtByGame,
	});
	if (!keepsLand)
	{
		View().BeginLoadingScreen(loading::LoadingClock::Mode::Tips);
	}
	PrepareNewLand(land_change::Kind::SavedGame);
	_landPath = landPath;
	timer.Step("reset");
	View().ShowLoadingFrame();
	try
	{
		if (keepsLand)
		{
			InitializeLevelOnOpenLand();
			SetUpLandscape();
		}
		// The testbed's flat land comes from no file: it is built again
		else if (landPath == k_TestbedLandPath)
		{
			_landscapePath.clear();
			InitializeLevel(flat_land::Build());
			SetUpLandscape();
		}
		else if (gods_ring_map::IsThisMap(landPath.generic_string()))
		{
			_landscapePath.clear();
			InitializeLevel(gods_ring_land::Build());
			SetUpLandscape();
		}
		else if (online_map_land::IsOnlineMap(landPath.generic_string()))
		{
			// The landscape of a land of a network game is in the land's own file, as when its script loaded it
			const auto data = Locator::filesystem::value().ReadAll(landPath);
			const auto file = onlinemap::Parse(std::as_bytes(std::span(data)));
			if (!file.has_value())
			{
				throw std::runtime_error("the land is not a land of a network game");
			}
			_mapLandscape = LandscapeBytes(*file);
			LoadLandscape(landscape);
			_mapLandscape.clear();
		}
		else
		{
			LoadLandscape(landscape);
		}
	}
	catch (const std::exception& e)
	{
		_mapLandscape.clear();
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The landscape {} of the saved game does not load: {}",
		                    landscape.generic_string(), e.what());
		View().EndLoadingScreen();
		return false;
	}
	timer.Step("landscape");

	// The saved world takes the place of every entity that the land made, the hand and the sky included. The players
	// that go with them are forgotten.
	auto& registry = Locator::entitiesRegistry::value();
	registry.Reset();
	// The things that only the frames make go with the things of the land that they go with. The sky's clouds stay, as
	// the land laid them out when it loaded: a save does not keep the clouds.
	if (Locator::drawnRegistry::has_value())
	{
		archetypes::CloudArchetype::RemoveAllButTheSky(Locator::drawnRegistry::value());
	}
	Locator::playerSystem::value().RegisterPlayers();
	const bool restored = restore();
	timer.Step("saved world");

	// The systems find their entities among the loaded ones, and the things go back into the map's cells. A world that
	// was saved with fewer players gets the players that it did not keep.
	Locator::playerSystem::value().RegisterPlayers();
	Locator::playerSystem::value().EnsureAllPlayers();
	Locator::handSystem::value().FindAfterLoad();
	Locator::skyClockSystem::value().FindAfterLoad();
	Locator::cameraBookmarkSystem::value().FindAfterLoad();
	if (Locator::appleBarrelSystem::has_value())
	{
		Locator::appleBarrelSystem::value().FindAfterLoad();
	}
	if (Locator::templeExteriorSystem::has_value())
	{
		Locator::templeExteriorSystem::value().FindAfterLoad();
	}
	if (Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().FileResidents();
	}
	if (Locator::objectDrawListSystem::has_value())
	{
		Locator::objectDrawListSystem::value().Reset();
	}
	RemakeModelsAfterLoad();
	registry.SetDirty();
	timer.Step("found again");
	// Each load of a saved game starts a game. The world of a game server continues as the server has it.
	if (use == SavedLandUse::SavedGame)
	{
		// The player is as good or evil as their profile keeps them, not as the save keeps them
		if (land_change::RulesFor(land_change::Kind::SavedGame).alignmentFromProfile)
		{
			SetPlayerAlignmentFromProfile();
		}
		// The players' statistics note the alignments they start the loaded game at, and the towns on the land
		player_statistics::GameStarts(registry);
	}
	// Each load starts a game: the square of the player of this computer takes the symbol of the profile played now. The
	// save keeps no symbol.
	View().MakeSymbolSheet();
	View().ShowLoadingFrame();
	// The creatures' map of the land that the saved world brings stays: the start of the land makes the map from the towns
	// of now, but the saved world made it from the towns at the start. A save with no map gets the map of now.
	auto exploration = std::move(registry.Context().explorationMap);
	StartNewLand(land_change::Kind::SavedGame);
	if (exploration.has_value())
	{
		registry.Context().explorationMap = std::move(exploration);
	}
	// A loaded skirmish starts as a skirmish starts: in the first turn the god of the player tries to bring the creature
	// that the profile keeps. When the save kept the creature of the god, the god brings no second creature. The world
	// of a game server continues and brings no creature.
	if (use == SavedLandUse::SavedGame &&
	    land_change::LoadBringsProfileCreature(registry.Context().skirmish, Locator::time::value().IsNetworkGame()))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "A saved skirmish loads: the god of the player tries to bring the creature again");
		Locator::playerCommandSystem::value().SetPlayersCreatureDue(true);
	}
	timer.Step("start");
	if (keepsLand)
	{
		View().EndLoadWithoutScreen();
	}
	else
	{
		View().EndLoadingScreen();
	}
	timer.Step("first frames");
	return restored;
}

void LandSystem::RemakeModelsAfterLoad()
{
	// The game made some models while it played: the worship sites' with their temple's skin, the
	// broken buildings' and the models of things in high detail. A load in a game that did not make them makes them again.
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().RemakeModelsAfterLoad();
	}
	if (Locator::buildingDamageSystem::has_value())
	{
		Locator::buildingDamageSystem::value().RemakeModelsAfterLoad();
	}
	if (Locator::highDetailSystem::has_value())
	{
		Locator::highDetailSystem::value().RemakeModelsAfterLoad();
	}
	// The log tells of the models that are still not there, so that the code that makes each one can make it again too
	const auto& meshes = Locator::resources::value().GetMeshes();
	size_t missing = 0;
	entt::id_type first = 0;
	Locator::entitiesRegistry::value().Each<const components::Mesh>([&](const components::Mesh& mesh) {
		if (!meshes.Contains(mesh.id))
		{
			first = missing == 0 ? mesh.id : first;
			++missing;
		}
	});
	if (missing > 0)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "After the load, {} things have a model that is not there, the first {}",
		                   missing, first);
	}
}

void LandSystem::PrepareNewLand(land_change::Kind change)
{
	++_landLoads;
	const bool toTestbed = change == land_change::Kind::Testbed;
	const auto rules = land_change::RulesFor(change);
	// The start-of-game question is about the land that goes now. Close the question with no answer. Then release the
	// pause that the question holds. If the question stays open, the hand stays hidden on the next land. A new game
	// asks the question again.
	if (View().WithdrawSkipBox())
	{
		Locator::time::value().SetPaused(false);
	}
	View().CloseMenuForNewLand();
	// A land that goes while the player is in the temple closes it first, so that nothing of its rooms, such as the
	// creature's room's flames and smoke, keeps entities of the land after they have gone
	if (Locator::temple::has_value())
	{
		Locator::temple::value().CloseForNewLand();
	}
	// The player's creature is kept with its mind and body before anything of the land goes, for a later land's script
	// to load it again, a new game's included: it is the creature the profile keeps. The players keep what is theirs
	// rather than the land's, such as their alignment, from one land to the next of a game; a new game makes them
	// afresh. Nothing is carried onto the testbed or off it: its creatures never replace the player's own kept creature,
	// and its players start afresh each time, with none of the last scenario's alignment, miracles or statistics.
	const bool carryOver = !toTestbed && _landPath != k_TestbedLandPath;
	if (Locator::creatureCarryOverSystem::has_value())
	{
		if (carryOver)
		{
			Locator::creatureCarryOverSystem::value().KeepPlayersCreature();
		}
		Locator::creatureCarryOverSystem::value().Reset();
		// The land to come keeps the player's creature in their files as it lives, but for the testbed
		Locator::creatureCarryOverSystem::value().SetLandKeepsCreatures(!toTestbed);
		// A new game is played as the profile picked now: the profile picked during the last game took over only now,
		// once that game's creature was kept for the profile it was played as
		if (rules.gameStarts)
		{
			Locator::creatureCarryOverSystem::value().StartGame();
		}
	}
	// As a game starts, the real-world weather asks the lobby for the weather of today, when the player turned it on
	if (rules.gameStarts && !toTestbed && Locator::realWeatherSystem::has_value())
	{
		ecs::RealWeatherLand land;
		Locator::realWeatherSystem::value().StartGame(land);
	}
	if (Locator::playerSystem::has_value())
	{
		if (carryOver && rules.playersCarried)
		{
			Locator::playerSystem::value().KeepForNextLand();
		}
		else
		{
			Locator::playerSystem::value().ForgetKept();
		}
	}
	// The reward table of the fireflies keeps its running totals from land to land. Nothing comes onto the testbed, so
	// its table starts empty, as on a computer that loaded no land before.
	if (toTestbed && Locator::fireflySystem::has_value())
	{
		Locator::fireflySystem::value().ForgetRewards();
	}
	// The players' hearts beat on from land to land; a new game starts them calm
	if (Locator::heartbeatSystem::has_value())
	{
		if (carryOver && rules.heartbeatsCarried)
		{
			Locator::heartbeatSystem::value().KeepForNextLand();
		}
		else
		{
			Locator::heartbeatSystem::value().ForgetKept();
		}
	}
	// The last land's scripts forget what they held: the land's objects go with it before anything could be let go back
	// into the game
	if (Locator::scriptObjects::has_value())
	{
		Locator::scriptObjects::value().ClearForNewLand();
	}
	// A new land has no weather of the last one
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().Reset();
		Locator::snowSystem::value().Reset();
		Locator::waterRingSystem::value().Reset();
	}
	// The scripts' hold on the screen goes on through the story's change of land: a fade to black made on the last land
	// stays black while the next loads, for that land's scripts to fade in. It goes when the scripts start again
	if (!rules.scriptScreenKept)
	{
		Locator::cinematicDirectorSystem::value().Reset();
	}
	// The opening's light and hand go
	Locator::introSystem::value().ReleaseAll();
	// Nor the explorers' boat
	Locator::boatLaunchSystem::value().Stop();
	// Nor does any script keep the camera, the game's speed or the dialogue
	if (Locator::camera::has_value())
	{
		auto& scriptControl = Locator::scriptControlSystem::value();
		if (const auto owner = scriptControl.GetCameraOwner(); owner != 0)
		{
			scriptControl.EndCameraControl(Locator::camera::value(), owner);
		}
		scriptControl.Reset();
	}
	Locator::dialogueControlSystem::value().Reset();
	// The camera's bookmarks a script put away come back (a land's first load makes them anew)
	if (Locator::cameraBookmarkSystem::has_value())
	{
		Locator::cameraBookmarkSystem::value().SetEnabled(true);
	}
	// The advisors carry on through the story's change of land, out or on their way home as they were. The land menu
	// starting the scripts again sends them home, and a new game starts them at home, out of sight
	if (Locator::advisorSystem::has_value())
	{
		switch (rules.advisors)
		{
		case land_change::Advisors::CarryOn:
			break;
		case land_change::Advisors::SentHome:
			SendAdvisorsHome(false);
			break;
		case land_change::Advisors::Afresh:
			Locator::advisorSystem::value().Reset();
			break;
		}
	}
	// The help's texts stay through the story's change of land, still to be read: the game's turns start again from
	// nothing on the next land, so a text timed by turns reads as read only once they pass its end on the last land, and
	// is clicked away only once they pass when it came. Its voices stop with every other sound. When the scripts start
	// again, the texts go with them
	if (rules.helpTextsKept)
	{
		Locator::helpTextSystem::value().StopVoices();
	}
	else
	{
		Locator::helpTextSystem::value().Reset();
	}
	Locator::guidanceSystem::value().Reset();
	Locator::cameraHelpSystem::value().Get().ResetForNewLand();
	// The help's clock, which times what the player did, starts again at 0 with the land. The counts stay.
	if (Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().Get().ResetClock();
	}
	// Nor do the camera's places to keep out of, its fence or its height limits
	if (Locator::cameraZoneSystem::has_value())
	{
		Locator::cameraZoneSystem::value().Reset();
	}
	// The turns of a new land start from zero, and so does the turn of the last autosave
	if (Locator::saveGameSystem::has_value())
	{
		Locator::saveGameSystem::value().NewLand();
	}
	Locator::influenceSystem::value().Reset();
	// Nor a scare of its fish. The creatures' footprints stay and fade on the new land, as in the game.
	Locator::fishFarmSystem::value().Reset();
	// Nor its miracles, nor their particle effects, nor its fires
	Locator::magicSystem::value().Reset();
	Locator::miracleFxSystem::value().Reset();
	Locator::fireSystem::value().Reset();
	// Nor the beat of its scrolls
	Locator::scriptHighlightSystem::value().Reset();
	Locator::tipBubbleSystem::value().Reset();
	if (Locator::chatSystem::has_value())
	{
		Locator::chatSystem::value().Reset();
	}
	Locator::handDemoSystem::value().Reset();
	// The commands that the players made on the old land do nothing on the new land, and no creature of a skirmish is due
	Locator::playerCommandSystem::value().Reset();
	Locator::playerCommandSystem::value().SetPlayersCreatureDue(false);
	Locator::creatureFightSystem::value().Reset();
	// The new land's temples get their own leash posts
	if (Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().Reset();
	}
	Locator::explosionSystem::value().Reset();
	Locator::floatingNumberSystem::value().Reset();
	Locator::magicSystem::value().SetIgnoreInfluence(false);
	Locator::animalSystem::value().Reset();
	if (Locator::appleBarrelSystem::has_value())
	{
		Locator::appleBarrelSystem::value().Reset();
	}
	Locator::magicShieldSystem::value().Reset();
	Locator::forestSystem::value().Reset();
	// Nor its fireflies, nor what they give
	Locator::fireflySystem::value().Reset();
	Locator::reactionSystem::value().Reset();
	Locator::teleportSystem::value().Reset();
	Locator::gestureEvents::value().Reset();
	Locator::particleRuleSystem::value().Reset();
	// The effects that the turns and the scripts ask for take numbers from the first again, on every computer
	if (Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().Reset();
	}
	// Nor its bodies in the physics
	if (Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().ResetSimulation();
	}
	// Nor its drops, dust, smoke and motes
	if (Locator::liquidParticleSystem::has_value())
	{
		Locator::liquidParticleSystem::value().Clear();
	}
	// Nor its broken buildings and their pieces
	if (Locator::buildingDamageSystem::has_value())
	{
		Locator::buildingDamageSystem::value().Reset();
	}
	// Nor anything in the hand
	if (Locator::handGrabSystem::has_value())
	{
		Locator::handGrabSystem::value().Reset();
	}
	// Nor the meshes made for its temples. A saved game keeps them, because the save can bring the same temples again.
	if (change != land_change::Kind::SavedGame && Locator::templeExteriorSystem::has_value())
	{
		Locator::templeExteriorSystem::value().ForgetLand();
	}

	// Every sound stops as the land goes, however it changes: the music at once, without fading, the atmosphere, and
	// every sound and voice still playing. They go before their entities do, which would leave them playing on
	if (Locator::sounds::has_value())
	{
		Locator::sounds::value().StopAllSounds();
	}

	// The sky goes on as it was: it is kept while every entity goes, and made again after
	Locator::skyClockSystem::value().KeepForNextLand();
	// The turns go back to 0 as the land clears, before the script of the next land starts. Thus the buildings that the
	// script makes are not made in play, and they get no path from their storage pit, as on the first land of a game.
	Locator::time::value().RestartTurns();
	// Reset everything. Deletes all entities and their components, and the things that only the frames make
	Locator::entitiesRegistry::value().Reset();
	if (Locator::drawnRegistry::has_value())
	{
		Locator::drawnRegistry::value().Reset();
	}
	// The new land makes the draw list again on its first frame only. Thus reset the draw list after the entities go:
	// a thing that the hand held asks for two frames when it goes.
	if (Locator::objectDrawListSystem::has_value())
	{
		Locator::objectDrawListSystem::value().Reset();
	}
	Locator::skyClockSystem::value().Initialize();
	// TODO(#661): split entities that are permanent from map entities and move hand and camera to init
	// We need a hand for the player
	Locator::handSystem::value().Initialize();

	// create our camera
	auto& config = Locator::config::value();
	const auto aspect = Locator::windowing::has_value() ? Locator::windowing::value().GetAspectRatio() : 1.0f;
	Locator::camera::value().SetProjectionMatrixPerspective(config.cameraXFov, aspect, config.cameraNearClip);
}

void LandSystem::StartPlayerAtProfileAlignment()
{
	if (!Locator::playerProfileSystem::has_value() || !Locator::playerSystem::has_value())
	{
		return;
	}
	// A network game does not use the profile of this computer, so that each computer starts the same world. Each god
	// of a network game starts at the alignment that its player sent with the start, at the first turn.
	if (profile_alignment::StartsFromOwnProfile(Locator::time::value().IsNetworkGame()))
	{
		SetPlayerAlignmentFromProfile();
	}
	// The players' statistics note the alignments they start the game at, and the towns on the land
	player_statistics::GameStarts(Locator::entitiesRegistry::value());
}

void LandSystem::SetPlayerAlignmentFromProfile()
{
	if (!Locator::playerProfileSystem::has_value() || !Locator::playerSystem::has_value())
	{
		return;
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	const auto alignment = profile_alignment::Starting(profile.has_value() ? profile->alignment : 0.0);
	Locator::alignmentSystem::value().SetPlayerAlignment(Locator::playerSystem::value().GetLocalPlayer(), alignment);
}

void LandSystem::KeepAlignmentInProfile()
{
	// The player's alignment is kept with their profile every so often as they play, never the testbed's
	if (_landPath.empty() || _landPath == k_TestbedLandPath || !Locator::playerProfileSystem::has_value() ||
	    !Locator::playerSystem::has_value() || !profile_alignment::KeptInOwnProfile(Locator::time::value().IsNetworkGame()))
	{
		return;
	}
	auto& players = Locator::playerSystem::value();
	if (players.KeepAlignmentAt(Locator::time::value().GetTurn()))
	{
		Locator::playerProfileSystem::value().SetCurrentAlignment(
		    Locator::alignmentSystem::value().GetPlayerAlignment(players.GetLocalPlayer()));
	}
}

void LandSystem::StartNewLand(land_change::Kind change)
{
	// With the land and its towns ready, the creatures' map of the land gets its regions
	creature_exploration_land::MakeMap();
	_lastGameLoopTime = machine_clock::Ticks();
	_turnDeltaTime = std::chrono::steady_clock::duration::zero();
	auto& clock = Locator::time::value();
	if (land_change::RulesFor(change).gameClockRunsOn)
	{
		// The story's next land counts its turns from 0 again, but the game's clock runs on through the change at the
		// speed it had. The clock is then far ahead of the new turns, and the next turn gives that time up as it does
		// whenever the game falls more than two seconds behind
		clock.RestartTurns();
	}
	else
	{
		// The game starts running, at the normal speed, as Black & White does
		clock.StartGameClock(false);
		clock.SetSpeed(1.0f / k_NormalTurnDurationMultiplier);
	}
	// A new land starts with no music of the scripts
	if (Locator::scriptMusicSystem::has_value())
	{
		Locator::scriptMusicSystem::value().Reset();
	}
	View().StartLandAudio();
	// A skirmish starts: in the first turn the god of the player brings the creature that the profile keeps. The commands
	// of that turn come first, and the computer gods start after it.
	if (land_change::RulesFor(change).profileCreatureBrought && !clock.IsNetworkGame())
	{
		Locator::playerCommandSystem::value().SetPlayersCreatureDue(true);
	}
}

void LandSystem::LoadLandscape(const std::filesystem::path& path)
{
	// The script of a land of a network game loads the landscape that the land's file holds
	if (!_mapLandscape.empty() && online_map_land::IsOwnLandscape(_landPath.generic_string(), path.generic_string()))
	{
		lnd::LNDFile lnd;
		if (const auto result = lnd.Open(_mapLandscape); result != lnd::LNDResult::Success)
		{
			throw std::runtime_error("Could not read the landscape of " + _landPath.generic_string() + ": " +
			                         std::string(lnd::ResultToStr(result)));
		}
		LoadTimer timer(fmt::format("landscape {}", path.stem().string()));
		_landscapePath = path;
		InitializeLevel(LandData::FromLnd(lnd));
		SetUpLandscape();
		return;
	}
	auto& fileSystem = Locator::filesystem::value();

	auto fixedName = fileSystem.FindPath(filesystem::FileSystemInterface::FixPath(path));

	if (!fileSystem.Exists(fixedName))
	{
		throw std::runtime_error("Could not find landscape " + path.generic_string());
	}
	LoadTimer timer(fmt::format("landscape {}", path.stem().string()));
	_landscapePath = path;
	InitializeLevel(fixedName);
	SetUpLandscape();
}

void LandSystem::SetUpLandscape()
{
	// A land starts at noon on the game's cycle of day and night, which its script may change, under new clouds
	auto& sky = Locator::skyClockSystem::value();
	sky.GetClock().Reset();
	sky.SetTime(sky.GetClock().GetScriptTime());
	archetypes::CloudArchetype::CreateSky();
	if (Locator::cloudSystem::has_value())
	{
		Locator::cloudSystem::value().Reset();
	}

	// Before the land script runs, the land has the player of each of the seven god slots and the neutral player, in the
	// order of their numbers. A slot that nobody plays has its player too, so the scripts and the turns find it.
	for (uint8_t number = 0; number < static_cast<uint8_t>(PlayerNames::_COUNT); ++number)
	{
		Locator::playerSystem::value().AddPlayer(archetypes::PlayerArchetype::Create(static_cast<PlayerNames>(number)));
	}

	Locator::cameraBookmarkSystem::value().Initialize();
	Locator::playerSystem::value().RegisterPlayers();
	if (Locator::cameraPathSystem::value().IsPathing())
	{
		Locator::cameraPathSystem::value().Stop();
	}
}

std::optional<std::filesystem::path> MapScriptLand()
{
	auto& fileSystem = Locator::filesystem::value();
	const auto mapScript = fileSystem.GetPath<filesystem::Path::Scripts>() / lhscriptx::map_script::k_FileName;
	if (!fileSystem.Exists(mapScript))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Could not find the map script {}", mapScript.generic_string());
		return std::nullopt;
	}
	const auto data = fileSystem.ReadAll(mapScript);
	std::vector<std::string> lands;
	try
	{
		lands = lhscriptx::map_script::FeatureScripts(std::string(reinterpret_cast<const char*>(data.data()), data.size()));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Error in the map script {}: {}", mapScript.generic_string(), e.what());
		return std::nullopt;
	}
	if (lands.empty())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The map script {} names no land script", mapScript.generic_string());
		return std::nullopt;
	}
	const auto land = filesystem::FileSystemInterface::FixPath(lands.front());
	if (!fileSystem.Exists(land))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Could not find the land script {}", lands.front());
		return std::nullopt;
	}
	// The map script writes the path in lower case. Use the name of the file as it is on the disk where the system
	// can give it.
	const auto found = fileSystem.FindPath(land);
	std::error_code error;
	auto onDisk = std::filesystem::canonical(found, error);
	return error ? found : onDisk;
}

bool LandSystem::StartChallengeScripts(bool startStory)
{
	auto& fileSystem = Locator::filesystem::value();
	const auto challengePath = ChallengePath();
	if (!fileSystem.Exists(challengePath))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Challenge file not found at {}",
		                    (fileSystem.GetGamePath() / challengePath).generic_string());
		return false;
	}
	auto& chlapi = Locator::chlapi::value();
	auto& lhvm = Locator::vm::value();
	// The virtual machine's errors go to the scripting log, where the editor's Scripts panel shows them
	// The land's scripts start with every place of the scripts' object table free; each native tells the table
	// whether it takes control of what it is given, and the scripts' variables keep their objects' references
	Locator::scriptObjects::value().Reset();
	chlapi.ResetSwitches();
	lhvm.Initialise(
	    &chlapi.GetFunctionsTable(),
	    [](uint32_t func) {
		    Locator::scriptObjects::value().EnterNative(func);
		    Locator::chlapi::value().EnterNative(func);
	    },
	    nullptr, [](uint32_t task) { chlapi::CHLApi::TaskStopped(task); },
	    [](lhvm::ErrorCode code, const std::string& text, uint32_t number) {
		    SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script error: {} ({} {})",
		                        lhvm::k_ErrorMsg.at(static_cast<size_t>(code)), text, number);
	    },
	    [](uint32_t object) { Locator::scriptObjects::value().AddReference(static_cast<entt::entity>(object)); },
	    [](uint32_t object) { Locator::scriptObjects::value().RemoveReference(static_cast<entt::entity>(object)); });
	try
	{
		lhvm.LoadBinary(fileSystem.ReadAll(challengePath));
		if (startStory)
		{
			StartStoryScripts();
		}
	}
	catch (const std::runtime_error& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to read challenge file at {}: {}",
		                    (fileSystem.GetGamePath() / challengePath).generic_string(), err.what());
	}
	return true;
}

void LandSystem::StartStoryScripts()
{
	// Only the first land of the story starts the story. On another land the scripts stay ready, but none runs.
	const auto landNumber = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
	const bool started = new_game_choice::LandStartsStoryScript(landNumber) &&
	                     Locator::vm::value().StartScript("LandControlAll", script::start::StartableScripts()) != 0;
	// The question comes whether or not the story started
	const bool asked = AskNewGameChoice();
	// The story opens on a black screen: its first land comes up at noon, and its scripts set the dawn of the opening
	// scene and fade the picture in from black some turns later. When the game was held for the question, the land is
	// left as it is: the scripts that skip the opening fade it themselves.
	if (started && !asked)
	{
		Locator::cinematicDirectorSystem::value().StartStory();
	}
	// Not in the original game: a developer's direct start of a later land runs the control script of that land, which
	// the game runs only after the last land is left
	if (const auto land = new_game_choice::DirectStartLand(_newGameStart, landNumber); land.has_value())
	{
		const auto script = "LandControl" + std::to_string(*land);
		if (Locator::vm::value().StartScript(script, script::start::StartableScripts()) == 0)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The direct start could not start the script {}", script);
		}
	}
}

bool LandSystem::AskNewGameChoice()
{
	// Every new game starts with nothing skipped, and every player is asked
	auto& skip = Locator::tutorialSkipSystem::value();
	skip.Set({});
	const bool hasCreature = Locator::playerProfileSystem::value().CurrentProfileHasCreature();
	// For developers and agents: the question answered at once, as though the player had answered it before the
	// story's first turn. Keeping the old creature without one is an answer the box doesn't offer.
	if (const auto answer = new_game_choice::AnswerAtOnce(_newGameStart, hasCreature); answer.has_value())
	{
		if (*answer != _newGameStart->answer)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"),
			                   "The current profile has no creature to keep: skipping all of the creature tutorial instead");
		}
		skip.Set(new_game_choice::SkipFor(*answer));
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "New game started with the start-of-game question answered {}",
		                   static_cast<int>(*answer));
		return true;
	}
	// The game waits, paused, for the answer
	return View().AskStartQuestion(hasCreature);
}

void LandSystem::GameLogicLoop(simulation::TurnViewInterface& view)
{
	const auto currentTime = machine_clock::Ticks();
	const auto delta = std::chrono::milliseconds(currentTime - _lastGameLoopTime);
	auto& clock = Locator::time::value();

	// The game pauses the world while the player is in the temple. The temple's own turns, by real time whatever the
	// game's speed, run the temple's scripts and its help scripts, keep the audio going and have the advisors follow
	// what they point at. A network game does not pause: the world's turns go on while the player is in the temple.
	if (!clock.IsNetworkGame() && Locator::temple::has_value() && Locator::temple::value().Active())
	{
		if (temple_help::TurnDue(_lastTempleTurnTime, currentTime))
		{
			// The scripts' clock moves on by the temple's turn, and the dialogue takes the player's clicks
			if (Locator::helpTextSystem::has_value())
			{
				Locator::helpTextSystem::value().ProcessTurn(temple_help::k_TurnMilliseconds);
			}
			Locator::vm::value().LookIn(lhvm::ScriptType::TempleHelp | lhvm::ScriptType::TempleSpecial);
			// The bars that the temple's scripts wait for slide by the real time of the temple's turn, as the game's draw
			// slides them by real time in the temple
			Locator::cinematicDirectorSystem::value().ProcessTempleTurn();
			view.PlayTempleAudioTurn();
			Locator::advisorSystem::value().ProcessTurn();
			// The help's banter check runs in the temple too. The help's clock stands still there, so the banter starts
			// only when it is already due.
			if (Locator::helpMessageSystem::has_value())
			{
				Locator::helpMessageSystem::value().ProcessTurn();
			}
			_lastGameLoopTime = currentTime;
		}
		return;
	}

	if (clock.IsPaused())
	{
		// The ambience is silent while the game is paused
		Locator::sounds::value().ProcessAtmosphere(false);
		return;
	}

	// A network game waits for the commands of the turn from the game server. A computer that is behind the server's
	// turns catches up, up to 10 turns a frame: when the next turn is there too, the clock of this computer follows the
	// server's and the turn is due.
	const auto& commands = Locator::playerCommandSystem::value();
	while (commands.TurnReady(clock.GetTurn()))
	{
		if (commands.IsBehind(clock.GetTurn()))
		{
			clock.ResetTimerToTurn();
		}
		if (!clock.IsTurnDue())
		{
			return;
		}
		// The turn's steps go in the game's order, because many systems depend on it (see game_turn::k_Turn). A second
		// turn in the frame follows the turn before it at once: the frame steps only draw, so none come between them.
		// The tools that act by the turns act first, so that each computer of a network game does them before the same
		// turn.
		view.BeforeTurn();
		simulation::PlayTurn(view);

		_lastGameLoopTime = currentTime;
		_turnDeltaTime = delta;
	}
}

void LandSystem::PlayTurns(simulation::TurnViewInterface& view)
{
	auto& profiler = Locator::profiler::value();
	auto gameLogic = profiler.BeginScoped(Profiler::Stage::GameLogic);
	const auto turnBefore = Locator::time::value().GetTurn();
	const auto logicStart = std::chrono::steady_clock::now();
	PlayDueTurns(view);
	FinishTurns(turnBefore, logicStart, std::nullopt);
}

void LandSystem::PlayTurnSteps(simulation::TurnViewInterface& view)
{
	auto gameLogic = Locator::profiler::value().BeginScoped(Profiler::Stage::GameLogic);
	PlayDueTurns(view);
}

void LandSystem::PlayDueTurns(simulation::TurnViewInterface& view)
{
	// The queue knows that this thread does the turns, so that the check can find turn code that uses the services of the
	// main thread directly
	auto* queue = Locator::mainThreadQueue::has_value() ? &Locator::mainThreadQueue::value() : nullptr;
	std::optional<MainThreadQueue::TurnScope> turnScope;
	if (queue != nullptr)
	{
		turnScope.emplace(*queue);
	}
	GameLogicLoop(view);
}

void LandSystem::FinishTurns(uint32_t turnBefore, std::chrono::steady_clock::time_point start,
                             std::optional<std::chrono::steady_clock::time_point> end)
{
	// The main thread does the work that the turns gave it. Turns on the main thread do their work at once, so then no
	// work waits here.
	if (Locator::mainThreadQueue::has_value())
	{
		Locator::mainThreadQueue::value().RunPending();
	}
	// The run record keeps the time of each turn
	if (Locator::time::value().GetTurn() != turnBefore)
	{
		Locator::simulation::value().TurnPlayed(
		    std::chrono::duration<float, std::milli>(end.value_or(std::chrono::steady_clock::now()) - start).count());
	}
}

} // namespace openblack::ecs::systems
