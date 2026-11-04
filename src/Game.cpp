/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Game.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <GameSocket.h>
#include <LHVM.h>
#include <LNDFile.h>
#include <SDL.h>
#include <SaveSlots.h>
#include <bgfx/bgfx.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/CreatureCaveTrophies.h"
#include "3D/DayNightClock.h"
#include "3D/FlatLand.h"
#include "3D/HandAnimation.h"
#include "3D/HandCycleChoice.h"
#include "3D/HandMorph.h"
#include "3D/HandNavigationPose.h"
#include "3D/HandOrientation.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightFrame.h"
#include "3D/LandLightTable.h"
#include "3D/MapCoords.h"
#include "3D/OceanInterface.h"
#include "3D/SeaTexture.h"
#include "3D/SkyDome.h"
#include "3D/SnowCover.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleRoomMeshes.h"
#include "3D/WaterRings.h"
#include "Audio/AtmosAudio.h"
#include "Audio/AudioManagerInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/DoubleClickFlight.h"
#include "Camera/FightCameraModel.h"
#include "Camera/FreeCameraModel.h"
#include "Camera/NearClipping.h"
#include "Camera/ScriptCameraModel.h"
#include "Commands/PlayerCommands.h"
#include "Common/BufferedLogSink.h"
#include "Common/BuildVersion.h"
#include "Common/DeveloperToolsGate.h"
#include "GameDetail.h"
#include "Sound/ClipSounds.h"
#include "Sound/GameMusic.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/HelpSpeech.h"
#include "Sound/ScriptSoundEffect.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "Common/EventManager.h"
#include "Common/FatalMessage.h"
#include "Common/GameLanguage.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Common/MainThreadQueue.h"
#include "Common/RandomNumberManager.h"
#include "Common/SideThread.h"
#include "Common/StringUtils.h"
#include "Common/UserData.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureFightHud.h"
#include "Creature/CreatureHandRules.h"
#include "Creature/CreatureNames.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"
#include "Creature/LeashOrders.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/FightAutopilotInterface.h"
#include "Debug/FrameStatsLog.h"
#include "Debug/FrameWriteGuard.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Archetypes/SkyArchetype.h"
#include "ECS/ChallengePictures.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/FloatingNumber.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/InterfaceLimits.h"
#include "ECS/Components/LearningMarker.h"
#include "ECS/Components/LivingListPlace.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PrayerPower.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/CreatureDanceMusic.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/DrawnPlace.h"
#include "ECS/FloatingNumber.h"
#include "ECS/GameEndConditions.h"
#include "ECS/GamePlayers.h"
#include "ECS/LearningMarker.h"
#include "ECS/Map.h"
#include "ECS/OtherPlayerSparkles.h"
#include "ECS/ProfileAlignment.h"
#include "ECS/RealWeatherLand.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/AbodeSoundSystemInterface.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/AppleBarrelSystemInterface.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/ClanCreatureSystemInterface.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/ComputerSettingsSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePenSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DesignedWaterfallSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/FilmCreatureSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/FloatingNumberSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GatheringBoxSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/IncomingMessageSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/InspectorLoading.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/MistSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/PauseTextSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PlaygroundTripSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/PuzzleGameSystemInterface.h"
#include "ECS/Systems/PuzzleTotemSystemInterface.h"
#include "ECS/Systems/RainSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RealWeatherSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SharkSystemInterface.h"
#include "ECS/Systems/SimulationSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/SnowfallSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/SyncChecksumSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TattooMenuSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/TornadoSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/TurnCheerSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TownDesireFlags.h"
#include "ECS/TreeRoots.h"
#include "ECS/VillageTotem.h"
#include "ECS/VillagerLabelSource.h"
#include "ECS/WorldObjects.h"
#include "ECS/WorshipChantMusic.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gestures/GestureTrailBuilder.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/WarmUpViews.h"
#include "Gui/Canvas.h"
#include "Gui/DialogPainter.h"
#include "Gui/EscapeKeyRules.h"
#include "Gui/GameInterface.h"
#include "Gui/HandToolTips.h"
#include "Gui/JoinOnlineEntry.h"
#include "Gui/LoadingScreen.h"
#include "Gui/MultiplayerFrontEnd.h"
#include "Gui/OnlineMapTexts.h"
#include "Gui/PingDisplay.h"
#include "Gui/ScreenFit.h"
#include "Gui/StartupRules.h"
#include "Gui/StatisticsBoxRules.h"
#include "Gui/WorldToolTipRules.h"
#include "Gui/WorldToolTips.h"
#include "Hand/HandBlend.h"
#include "Hand/HandFeel.h"
#include "Hand/HandGrabRules.h"
#include "Hand/HandReach.h"
#include "Hand/HandTrace.h"
#include "Hand/HandVisibility.h"
#include "Hand/ScriptHandState.h"
#include "Help/DialogueText.h"
#include "Help/HelpProfile.h"
#include "Help/SingAlong.h"
#include "Help/Spirits.h"
#include "Help/TurnCheer.h"
#include "Input/DeveloperKeys.h"
#include "Input/GameActionMapInterface.h"
#include "Input/KeyBindings.h"
#include "Input/KeyRebinding.h"
#include "Input/LandGrip.h"
#include "Input/LocalPlayer.h"
#include "Input/PlatformKeys.h"
#include "LHScriptX/MapScript.h"
#include "LHScriptX/Script.h"
#include "LivingTurnOrder.h"
#include "Locator.h"
#include "Magic/TownBelief.h"
#if !defined(__EMSCRIPTEN__)
#include "Online/Desktop/UdpLanDiscovery.h"
#endif
#include "Online/HttpClientInterface.h"
#include "Online/LanDiscoveryInterface.h"
#include "Online/NetworkLand.h"
#include "Online/NetworkSession.h"
#include "Online/OnlineMapLand.h"
#include "Online/OnlineMapStoreInterface.h"
#include "Online/OpenPage.h"
#include "Online/StartRules.h"
#include "Online/WebSocketInterface.h"
#include "Parsers/InfoFile.h"
#include "Particles/LiquidParticles.h"
#include "Physics/Materials.h"
#include "PlayerSlots.h"
#include "Profiler.h"
#include "Resources/GameAssets.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptInterfaceLevels.h"
#include "Serializer/FotFile.h"
#include "Simulation/ChecksumLog.h"
#include "Simulation/CommandLog.h"
#include "Simulation/FrameBody.h"
#include "Simulation/ProfileMenu.h"
#include "Simulation/TurnBody.h"
#include "Simulation/WorldStart.h"
#include "Temple/TempleHelp.h"
#include "Video/VideoRules.h"
#include "Visual/DetailLevel.h"

#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif
#if defined(__EMSCRIPTEN__)
#include "Web/WebPlatform.h"
#endif
#if defined(OPENBLACK_INSPECTOR)
#include "Inspector/GameSeam.h"
#endif

using namespace openblack;
using namespace openblack::lhscriptx;
using namespace std::chrono_literals;
using namespace openblack::game_detail;

namespace
{

/// The window mode of a name in the settings of this computer
windowing::DisplayMode DisplayModeOf(std::string_view name, windowing::DisplayMode otherwise)
{
	if (name == "windowed")
	{
		return windowing::DisplayMode::Windowed;
	}
	if (name == "fullscreen")
	{
		return windowing::DisplayMode::Fullscreen;
	}
	if (name == "borderless")
	{
		return windowing::DisplayMode::Borderless;
	}
	return otherwise;
}

/// The game plays in a browser page
#if defined(__EMSCRIPTEN__)
constexpr bool k_Browser = true;
#else
constexpr bool k_Browser = false;
#endif

/// Draws the player's creature in the falling spell's film, when the film shows it, and the film's effects
void DrawFilmCreature(const graphics::RendererInterface::DrawSceneDesc& drawDesc)
{
	if (!Locator::filmCreatureSystem::has_value() || !Locator::windowing::has_value() ||
	    !Locator::videoSystem::value().HidesWorld())
	{
		return;
	}
	const auto shown = Locator::filmCreatureSystem::value().GetShown();
	if (!shown.has_value())
	{
		return;
	}
	// The renderer draws the creature over the film's picture, where the picture is on the window
	const auto size = Locator::windowing::value().GetSize();
	const auto rect = video::LetterboxRect(size.x, size.y);
	Locator::rendererInterface::value().DrawFilmCreature(
	    drawDesc, {.copy = shown->copy,
	               .copyShown = shown->copyShown,
	               .view = shown->view,
	               .projection = shown->projection,
	               .viewOrigin = glm::vec2(static_cast<float>(rect.x), static_cast<float>(rect.y)),
	               .viewSize = glm::vec2(static_cast<float>(rect.width), static_cast<float>(rect.height)),
	               .light = shown->light,
	               .colour = shown->colour,
	               .added = shown->added,
	               .stars = shown->stars,
	               .fans = shown->fans,
	               .fanDepth = shown->fanDepth});
}

/// What the box of the winning conditions shows: the score history and the conditions of each god among the first four
/// players that plays, and the conditions of the player of this computer. A network game shows every god that plays,
/// so a game of more than 4 gods shows them 4 at a time (deviation 195).
gui::conditions_box::Figures ConditionsFigures()
{
	gui::conditions_box::Figures figures;
	if (!Locator::gameEndSystem::has_value() || !Locator::playerSystem::has_value())
	{
		return figures;
	}
	const auto local =
	    (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE);
	const auto landNumber = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
	const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
	for (const auto& [god, standing] : Locator::gameEndSystem::value().Standings())
	{
		if (god == local)
		{
			for (size_t number = 0; number < standing.conditions.size(); ++number)
			{
				if (const auto& condition = standing.conditions.at(number); condition.has_value())
				{
					figures.conditions.push_back({.number = number, .target = condition->target});
				}
			}
		}
		// The god of the observers has no column of its own, as in the original game
		if (!game_end::CanLose(god, networkGame) || god == Locator::gameEndSystem::value().ObserversGod())
		{
			continue;
		}
		const auto scores = game_end::ShownScores(standing);
		figures.gods.push_back({.player = god,
		                        .colour = gui::statistics_box::PlayerColour(god, landNumber),
		                        .scores = std::vector<float>(scores.begin(), scores.end()),
		                        .conditions = standing.conditions});
	}
	return figures;
}

} // namespace

namespace openblack::game_detail
{

PlayerNames LocalInputPlayer()
{
	return input::LocalPlayer(Locator::handSystem::has_value() ? &Locator::handSystem::value() : nullptr);
}

graphics::RendererInterface::DrawSceneDesc SceneDrawDesc(const EngineConfig& config, uint32_t time, bool drawHand)
{
	return {
	    .camera = &Locator::camera::value(),
	    .frameBuffer = nullptr,
	    .entities = Locator::entitiesRegistry::value(),
	    .time = time,
	    .timeOfDay = config.timeOfDay,
	    .smallBumpMapStrength = config.smallBumpMapStrength,
	    .viewId = graphics::RenderPass::Main,
	    .drawSky = config.drawSky,
	    .drawWater = config.drawWater,
	    .drawIsland = config.drawIsland,
	    .drawEntities = config.drawEntities,
	    .drawSprites = config.drawSprites,
	    .drawVegetation = config.drawVegetation,
	    .drawBoundingBoxes = config.drawBoundingBoxes,
	    .cullBack = false,
	    .wireframe = config.wireframe,
	    .drawHand = drawHand,
	};
}

bool InNetworkGame()
{
	return Locator::networkGame::has_value() && Locator::networkGame::value().Session() != nullptr &&
	       Locator::time::value().IsNetworkGame();
}

} // namespace openblack::game_detail

const std::string k_WindowTitle = "openblack";

Game* Game::sInstance = nullptr;

Game::Game(Arguments&& args) noexcept
    : _gamePath(args.gamePath)
    , _startMap(args.startLevel)
    , _playVideo(args.playVideo)
    , _filmSurface(args.filmSurface)
    , _preIntro(args.preIntro || args.launchSwitches.preIntroVideo)
    , _skipLogos(args.skipLogos)
    , _logoEffectAsked(args.logoEffect)
    , _newGameStart(args.newGameStart)
    , _newGameAsked(args.newGame || launch_switches::FirstGameOf(args.launchSwitches) == launch_switches::FirstGame::NewGame)
    , _continueAsked(args.continueGame)
    , _launchSwitches(args.launchSwitches)
    , _startTestbed(args.startTestbed || args.startGodsRingMap || args.scenario.has_value())
    , _startGodsRingMap(args.startGodsRingMap)
    , _scenarioRequest(args.scenario)
    , _inspectPort(args.inspectPort)
    , _fightAutopilot(args.fightAutopilot)
    , _fightTranscriptFolder(args.fightTranscriptFolder)
    , _simulation(args.simulation)
    , _profileFolder(args.profileFolder)
    // Any of these is a developer's or a tool's run; the game started plainly, or with only its window's size, mode,
    // backend, detail and logging chosen, is the player's
    , _automatedRun(args.startLevel != k_DefaultStartLevel || args.startTestbed || args.scenario.has_value() ||
                    args.newGameStart.has_value() || args.skipLogos || args.inspectPort.has_value() || args.seed.has_value() ||
                    !args.playVideo.empty() || args.numFramesToSimulate > 0 || args.requestScreenshot.has_value())
    , _screenshotRoot(args.screenshotRoot)
    , _seed(args.seed)
    , _networkGame(args.networkGame)
    , _throwLeadLatencyMs(args.throwLeadLatencyMs)
    , _networkJoin(args.networkJoin)
    , _networkLandPending(args.networkJoin.has_value())
    , _inspectInputLock(args.inspectInputLock)
#if defined(OPENBLACK_DEVELOPER_TOOLS)
    , _testbedWindow(!args.scenario.has_value() || !args.scenario->hideWindow)
#endif
    , _requestScreenshot(args.requestScreenshot)
    , _deviceRestoreFrame(args.deviceRestoreFrame)
    , _deviceLossFrame(args.deviceLossFrame)
    , _turnApiCheck(args.turnApiCheck)
    , _turnBesideDraw(args.turnBesideDraw)
{
	Locator::camera::emplace(glm::zero<glm::vec3>());
	// The log of each part of the game
	simulation::StartLogs(args.logFile, args.logLevels);
	sInstance = this;
	for (const auto& ignored : args.ignoredDeveloperOptions)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "This build has no developer tools: it ignores the option {}", ignored);
	}

	auto& config = Locator::config::emplace();
	config.numFramesToSimulate = args.numFramesToSimulate;
	config.frameStatsInterval = args.frameStatsInterval;
	config.frameStatsViews = args.frameStatsViews;
	config.aiTimesEvery = args.aiTimesEvery;
	config.turnStepTimesMs = args.turnStepTimesMs;
	config.frameStatsOut = args.frameStatsOut;
	config.warmUpFirstFrame = args.warmUpFirstFrame;
	config.parallelDrawEncode = args.parallelDrawEncode;
	config.lobbyOrigin = args.lobbyOrigin;
	config.lobbyTestAuthorities = args.lobbyTestAuthorities;
	config.useInternetWeather = args.useInternetWeather;
	config.showPing = args.showPing;
	config.dynamicTreeShadows = args.dynamicTreeShadows;
	config.unlockedBonusCreatures = args.unlockedBonusCreatures;
	config.footballAddOn = args.footballAddOn;
	// The settings of this computer give what the command line did not
	InitializeComputerSettings(player_profiles::SettingsFileFor(
	    {.profileFolder = _profileFolder, .userFolder = user_data::ProfileFolder(), .automated = _automatedRun},
	    user_data::SettingsFile()));
	const auto& kept = Locator::computerSettingsSystem::value().GetSettings();
	const auto start = computer_settings::Resolve({.width = static_cast<uint16_t>(args.windowWidth),
	                                               .height = static_cast<uint16_t>(args.windowHeight),
	                                               .vsync = args.vsync,
	                                               .detailLevel = args.detailLevel},
	                                              args.givenOnCommandLine, kept);
	config.resolution = {start.width, start.height};
	config.displayMode =
	    args.givenOnCommandLine.windowMode ? args.displayMode : DisplayModeOf(start.windowMode, args.displayMode);
	config.noDraw = args.noDraw;
	config.background = args.background;
	config.graphicsBackend = args.noDraw ? GraphicsBackend::Noop : args.graphicsBackend;
	config.vsync = start.vsync;
	config.clockedAudio = args.simulation.has_value() && args.simulation->frameTime.has_value();
	config.detailLevel = start.detailLevel;
	config.detail = graphics::detail_level::WithCustom(start.detailLevel, kept.customDetail);
	config.maxDetailLevel = start.maxDetailLevel;
	config.keptEffectsVolume = kept.effectsVolume;
	config.keptMusicVolume = kept.musicVolume;
	config.guiScale = args.guiScale;
}

Game::~Game() noexcept
{
	Locator::gameMusic::reset();
	// Its textures go before the renderer, and the temple's scrolls are written with its text
	if (Locator::temple::has_value())
	{
		Locator::temple::value().SetInterface(nullptr);
	}
	if (Locator::creatureCaveSystem::has_value())
	{
		Locator::creatureCaveSystem::value().SetInterface(nullptr);
	}
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().SetInterface(nullptr);
	}
	if (Locator::pauseTextSystem::has_value())
	{
		Locator::pauseTextSystem::value().SetInterface(nullptr);
	}
	if (Locator::incomingMessageSystem::has_value())
	{
		Locator::incomingMessageSystem::value().SetTexts(nullptr);
	}
	if (Locator::chatSystem::has_value())
	{
		Locator::chatSystem::value().SetTexts(nullptr);
	}
	if (Locator::gatheringBoxSystem::has_value())
	{
		Locator::gatheringBoxSystem::value().SetBox({});
	}
	if (Locator::saveGameSystem::has_value())
	{
		Locator::saveGameSystem::value().SetShowBox({});
		Locator::saveGameSystem::value().SetReportProblem({});
	}
	Locator::screenFade::reset();
	_interface.reset();
	_loadingScreen.reset();
	// What the scripts asked of openblack that it can't do yet, for the natives to write next
	if (Locator::chlapi::has_value())
	{
		Locator::chlapi::value().LogStubCalls();
	}
	// The game keeps the help record of the player when it closes
	SaveHelpRecord();
	// The profile keeps the time played as the game closes
	if (Locator::playerProfileSystem::has_value())
	{
		Locator::playerProfileSystem::value().StopPlaying(static_cast<int64_t>(std::time(nullptr)));
	}
	if (Locator::landSystem::has_value())
	{
		Locator::landSystem::value().SetView(nullptr);
	}
	ReportTurnApiCheck();
	ShutDownServices();
	// A later caller of Instance must not get this game once it is gone
	if (sInstance == this)
	{
		sInstance = nullptr;
	}
	SDL_Quit(); // todo: move to GameWindow
	spdlog::shutdown();
	// A later game, or a test, must not find this one through the static pointer
	if (sInstance == this)
	{
		sInstance = nullptr;
	}
}

void Game::RecordScenarioGive(uint32_t turn, size_t command)
{
	if (_scenarioGives != nullptr)
	{
		_scenarioGives->Write(turn, command);
	}
}

void Game::SetGameSpeed(float multiplier)
{
	Locator::time::value().SetSpeed(1.0f / multiplier);
}

float Game::GetGameSpeed() const
{
	return 1.0f / Locator::time::value().GetSpeed();
}

uint32_t Game::GetTurn() const
{
	return Locator::time::value().GetTurn();
}

bool Game::IsPaused() const
{
	return Locator::time::value().IsPaused();
}

bool Game::FrontEndBoxShows() const
{
	return _interface && (_interface->IsDialogOpen() || _interface->GetTattooEditor().IsOpen());
}

void Game::ProcessMusicTurn(glm::vec3 cameraPosition, bool inCitadel)
{
	// The game picks the music for the turn
	if (!_gameMusic)
	{
		return;
	}
	const auto& alignment = Locator::alignmentSystem::value();
	// The land's music waits while a script holds the cinema bars, and while they slide in or out
	bool cinema = false;
	if (Locator::cinematicDirectorSystem::has_value())
	{
		const auto& director = Locator::cinematicDirectorSystem::value();
		cinema =
		    (director.IsWideScreenOn() && director.GetWideScreenOwner() != 0) || !director.IsWideScreenTransitionFinished();
	}
	audio::GameMusic::TurnInputs music {
	    .turn = GetTurn(),
	    .camera = cameraPosition,
	    .groundHeight = Locator::terrainSystem::value().GetHeightAt(glm::xz(cameraPosition)),
	    .inCitadel = inCitadel,
	    .alignment = alignment.GetCameraAlignment(),
	    .playerAlignment = alignment.GetPlayerAlignment(
	        (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE)),
	    .cinema = cinema,
	    .towns = {},
	    .thingPosition = [](uint32_t thing) -> std::optional<glm::vec3> {
		    const auto& registry = Locator::entitiesRegistry::value();
		    const auto entity = static_cast<entt::entity>(thing);
		    if (!registry.Valid(entity) || !registry.AllOf<ecs::components::Transform>(entity))
		    {
			    return std::nullopt;
		    }
		    return registry.Get<const ecs::components::Transform>(entity).position;
	    },
	};
	music.worshipChant = ecs::worship_chant_music::Heard(cameraPosition);
	music.creatureDance = ecs::creature_dance_music::Heard(cameraPosition);
	// The music heard follows the world's clock of the scripts' music
	if (Locator::scriptMusicSystem::has_value())
	{
		const auto& script = Locator::scriptMusicSystem::value().GetMusic();
		music.script = {
		    .type = script.GetType(),
		    .started = script.IsStarted(),
		    .playCount = script.GetPlayCount(),
		    .place = script.GetPlace(),
		};
	}
	music.film = Locator::videoSystem::has_value() && Locator::videoSystem::value().IsPlaying();
	if (Locator::infoConstants::has_value())
	{
		const auto& sound = Locator::infoConstants::value().sound;
		music.townTriggerDistance = sound.townTriggerDistance;
		music.townTriggerOffDistance = sound.townTriggerOffDistance;
	}
	if (Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().FightMusicHeard(cameraPosition))
	{
		const auto& context = Locator::entitiesRegistry::value().Context();
		music.creatureFight = audio::GameMusic::SelectFightType(context.mapScriptGlobals.landNumber, context.skirmish);
	}
	Locator::entitiesRegistry::value().Each<const ecs::components::Town, const Tribe, const ecs::components::Transform>(
	    [&music](const ecs::components::Town& town, const Tribe tribe, const ecs::components::Transform& transform) {
		    music.towns.push_back({
		        .position = transform.position,
		        .tribe = static_cast<int32_t>(tribe),
		        .id = town.id,
		    });
	    });
	_gameMusic->ProcessTurn(music);
}

bool Game::Update() noexcept
{
	// The frame's steps go in the game's order, because many systems depend on it (see game_frame::k_Frame)
	FrameState frame;
	if (!game_frame::PlayFrame([this, &frame](game_frame::Step step) {
		    // The guard of a tool's run looks at each step but the turns, which alone may change the synced world. The guard
		    // does not tell the changes of the debug windows. The timeline of the testbed runner acts in them by the game
		    // time of the frame. It is a tool for the debug builds and is not part of the game. The guard tells only the
		    // turns of these changes, because the next steps can file the runner's new things in the map.
		    if (!_frameGuard || step == game_frame::Step::Turns ||
		        Locator::time::value().GetTurn() < _simulation->frameGuardFrom)
		    {
			    if (_frameGuard)
			    {
				    _frameGuard->Forget();
			    }
			    const bool played = PlayFrameStep(step, frame);
			    // The guard looks only at the frame writes of the steps that it looks at
			    if (_frameGuard && Locator::temple::has_value())
			    {
				    std::ignore = Locator::temple::value().TakeFrameWrites();
			    }
			    return played;
		    }
		    // The temple of a game alone writes the synced world in some frame steps, as the original game does. The guard
		    // knows these writers by name. All other writes in a frame step trip the guard. The temple can also write after
		    // the draw, between two frames. The guard gives these changes and their writers to the next step.
		    const auto frameWrites = [] {
			    return Locator::temple::has_value() ? Locator::temple::value().TakeFrameWrites() : temple::FrameWrites {};
		    };
		    const auto landLoads = [] {
			    return Locator::landSystem::has_value() ? Locator::landSystem::value().LandLoads() : 0U;
		    };
		    const auto loadsBefore = landLoads();
		    _frameGuard->Before();
		    const bool played = PlayFrameStep(step, frame);
		    const auto writers = frameWrites();
		    if (!frame_write_guard::LooksAtStep(loadsBefore, landLoads()))
		    {
			    // The step loaded a land, which makes a new synced world: the next step takes a new sample
			    _frameGuard->Forget();
			    return played;
		    }
		    if (step == game_frame::Step::DebugGui)
		    {
			    _frameGuard->AfterTool(game_frame::Name(step), Locator::time::value().GetTurn());
		    }
		    else
		    {
			    _frameGuard->After(game_frame::Name(step), Locator::time::value().GetTurn(), writers,
			                       Locator::time::value().IsNetworkGame());
		    }
		    return played;
	    }))
	{
		return false;
	}
	if (sim_run::Finished(Locator::simulation::value().GetSettings(), Locator::time::value().GetTurn()))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The simulation run is at its last turn, {}", Locator::time::value().GetTurn());
		LeaveAsObserverAtTheEndOfTheRun();
		return false;
	}
	// In a game alone, the temple holds the world. A run that stays there after its scenario does not come to its
	// last turn
	if (const auto& time = Locator::time::value();
	    sim_run::EndsHeldInTemple(Locator::simulation::value().GetSettings(), time.IsHeld(), time.IsNetworkGame(),
	                              _scenarioGaveAll && !_scenarioRequest.has_value() && _scenarioGaveAll()))
	{
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "The temple holds the world after the last command of the scenario: the simulation run ends at turn {}",
		    time.GetTurn());
		return false;
	}
	// A game that joined from the command line stops when its network game ends: its world turns no more
	// After the out-of-sync box, the game stops on No and plays on with Yes
	if (_leaveNetworkGame)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The player left the network game: the game stops");
		return false;
	}
	// A run of the tools stops after the last turn of a decided network game: no turn comes after it
	if (Locator::simulation::value().GetSettings().untilTurn.has_value() && Locator::networkGame::has_value())
	{
		const auto* session = Locator::networkGame::value().Session();
		const auto last = session != nullptr ? session->StoppedAfter() : std::nullopt;
		if (last.has_value() && Locator::time::value().GetTurn() > *last)
		{
			// The log names the last turn by the number of the world, as the line of the decision does
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The turns of the network game stopped after turn {}: the game stops",
			                   *last + 1);
			LeaveAsObserverAtTheEndOfTheRun();
			return false;
		}
	}
	if (_joinedFromCommandLine && Locator::networkGame::has_value() && !Locator::networkGame::value().InGame() &&
	    !_outOfSyncAsked && !_connectionLost.Told())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The network game ended: the game stops");
		return false;
	}
	const auto& config = Locator::config::value();
	return config.numFramesToSimulate == 0 || _frameCount < config.numFramesToSimulate;
}

void Game::UpdateOnline()
{
	// The web client and the WSS client move their transfers on and run the handlers. Then the lobby client moves on.
	if (Locator::httpClient::has_value())
	{
		Locator::httpClient::value().Poll();
	}
	if (Locator::webSocketClient::has_value())
	{
		Locator::webSocketClient::value().Poll();
	}
	if (Locator::onlineMapStore::has_value())
	{
		Locator::onlineMapStore::value().Poll();
	}
	if (Locator::lobbyClient::has_value())
	{
		Locator::lobbyClient::value().Update();
	}
	// The gathering box talks to the ping server with the time of the computer, not the time of the turns
	if (Locator::gatheringBoxSystem::has_value())
	{
		Locator::gatheringBoxSystem::value().Update(SDL_GetTicks());
	}
	// The network game takes the turns and the checksums that came, and comes back after a loss
	if (Locator::networkGame::has_value())
	{
		auto& network = Locator::networkGame::value();
		network.Update();
		const auto* session = network.Session();
		if (_interface)
		{
			// The words "Resynchronising..." show while the world resynchronises (user decision MI-4)
			_interface->ShowResync(session != nullptr && network.InGame() && session->ShowsResync());
			// After the last try the game asks whether the player plays on, as the game asks when the checksums differ
			// A run of the tools asks nothing: the game stops as its network game ends
			const bool toolRun = Locator::simulation::value().GetSettings().untilTurn.has_value();
			if (session != nullptr && !network.InGame() && session->GetEnd() == online::NetworkSession::End::OutOfSync &&
			    !_outOfSyncAsked && !toolRun)
			{
				_outOfSyncAsked = true;
				_interface->AskOutOfSync();
			}
			if (const auto answer = _interface->TakeOutOfSyncAnswer(); answer.has_value())
			{
				if (*answer)
				{
					network.PlayAlone();
				}
				else
				{
					_leaveNetworkGame = true;
				}
			}
			// After the last try to come back, the box tells the player that the connection to the game is lost
			if (_connectionLost.Update({
			        .hasSession = session != nullptr,
			        .started = session != nullptr && session->GetStart().has_value(),
			        .state = session != nullptr ? session->GetState() : online::NetworkSession::State::Idle,
			        .end = session != nullptr ? session->GetEnd() : online::NetworkSession::End::None,
			        .toolRun = toolRun,
			    }))
			{
				_interface->ShowConnectionLost();
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "The connection to the network game is lost ({}): the box tells the player",
				                   online::EndName(session->GetEnd()));
			}
			// The OK goes back to the network front end. A join from the command line has no front end: the game stops.
			// A box with no lost network session (a testbed scenario) only closes.
			if (_interface->TakeConnectionLostOk() && _connectionLost.Told())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "The player closed the box of the lost connection");
				if (_joinedFromCommandLine)
				{
					_leaveNetworkGame = true;
				}
				else
				{
					_gameEndAsked = game_session::End::Online;
				}
			}
		}
	}
}

void Game::FollowGameEnd()
{
	if (!_interface || !Locator::gameEndSystem::has_value())
	{
		return;
	}
	// Tests: at a turn, the player leaves the network game as a player does
	if (const auto leaveAt = Locator::simulation::value().GetSettings().leaveAt;
	    leaveAt.has_value() && !_testLeaveDone && Locator::time::value().GetTurn() >= *leaveAt && InNetworkGame())
	{
		_testLeaveDone = true;
		LeaveAsAPlayerDoes();
	}
	auto& gameEnd = Locator::gameEndSystem::value();
	const auto* session = Locator::networkGame::has_value() ? Locator::networkGame::value().Session() : nullptr;
	// The end box stops to show itself when it closes, or when the box of one of its tabs opens over it
	if (!_interface->GetEndGameBox().ShowsItself())
	{
		end_game_box::StoppedShowing(_endGameBoxState);
	}
	// The player's choice in the end box: Leave Game leaves the network game. In a skirmish it ends the skirmish, and the
	// skirmish box shows again.
	if (_interface->GetEndGameBox().TakeAction() == gui::EndGameBox::Action::Leave)
	{
		if (session != nullptr)
		{
			LeaveNetworkGame();
			return;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The end box: Leave Game ends the skirmish");
		_gameEndAsked = game_session::End::Skirmish;
	}
	const auto events = gameEnd.TakeEventsToShow();
	if (events.empty())
	{
		return;
	}
	const auto local = LocalGodForGameEnd();
	const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
	const auto opened = end_game_box::Follow(events, local, networkGame, _endGameBoxState);

	// The name of a god: the name of the player of its slot in a network game, the profile's name for the god of this
	// computer in another game
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	const auto nameOf = [session, local, &profile](PlayerNames god) {
		std::optional<std::u16string> human;
		if (session != nullptr && session->GetStart().has_value())
		{
			// The names follow the god slots: the gods after the neutral player have the slot before their number
			const auto& names = session->GetStart()->names;
			if (const auto slot = player_slots::SlotOfPlayer(god); slot.has_value() && *slot < names.size())
			{
				human = gui::ToUtf16(names.at(*slot));
			}
		}
		else if (god == local && profile.has_value())
		{
			human = gui::ToUtf16(profile->name);
		}
		return end_game_box::GodName(god, human.has_value() ? std::optional<std::u16string_view>(*human) : std::nullopt);
	};
	if (opened.godOut.has_value())
	{
		_interface->ShowGodOut(nameOf(*opened.godOut));
	}
	if (!opened.endBox.has_value())
	{
		return;
	}
	const auto& texts = _interface->GetTexts();
	const auto winner = gameEnd.LastGodIn();
	const auto winnerName = winner.has_value() ? nameOf(*winner) : std::u16string();
	const bool looksOn = end_game_box::LooksOn(local, networkGame);
	const auto title = end_game_box::Title(opened.endBox->kind, looksOn, winnerName,
	                                       {.leave = texts.Get("HELP_TEXT_DIALOG_AREYOUSUREQUIT"),
	                                        .lost = texts.Get("HELP_TEXT_DIALOG_ADDITION_152"),
	                                        .won = texts.Get("HELP_TEXT_WIN_GAME_04"),
	                                        .gameOver = texts.Get("HELP_TEXT_GAME_OVER"),
	                                        .winnerFormat = u"%s - %s Won"});
	// The band under the title has the colour of the player's god. A looker-on has no god that takes part, and the game
	// does not draw its band (vanilla-bugs 470).
	const auto landNumber = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
	const auto colour = looksOn ? glm::vec4(0.0f) : gui::statistics_box::PlayerColour(local, landNumber);
	_interface->OpenEndGameBox({.box = *opened.endBox, .title = title, .colour = colour});
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The end box opens for player {}: {}", static_cast<int>(local), gui::ToUtf8(title));
	// In a game on the internet, an end box of a decided game or of a god that is out sends the clan creature of the god
	// of this computer to the lobby, with the alignment that the god has now
	if (session != nullptr && session->Origin() == online::GameOrigin::InternetLobby &&
	    opened.endBox->kind != end_game_box::Kind::LeaveQuestion && session->OwnGod().has_value() &&
	    Locator::clanCreatureSystem::has_value())
	{
		const auto player = Locator::playerSystem::value().FindPlayer(*session->OwnGod());
		const auto* alignment =
		    player.has_value() ? Locator::entitiesRegistry::value().TryGet<const ecs::components::Alignment>(*player) : nullptr;
		static_cast<void>(Locator::clanCreatureSystem::value().SendAtGameEnd(alignment != nullptr ? alignment->value : 0.0f));
	}
	// The end box of a decided game asks the game server to stop the turns of the game
	if (session != nullptr && end_game_box::StopsTheGame(*opened.endBox, _endGameBoxState))
	{
		Locator::networkGame::value().StopGame();
	}
}

void Game::StartFrame(FrameState& frame)
{
	auto& profiler = Locator::profiler::value();

	profiler.Frame();

	auto& config = Locator::config::value();

	auto previous = profiler.GetEntries().at(profiler.GetEntryIndex(-1)).frameStart;
	auto current = profiler.GetEntries().at(profiler.GetEntryIndex(0)).frameStart;
	// Prevent spike at first frame
	if (previous.time_since_epoch().count() == 0)
	{
		current = previous;
	}
	frame.deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(current - previous);

	Locator::debugGui::value().SetScale(config.guiScale);
	// The clock of this frame moved on at the end of the last frame, when this frame's turns played beside its draw
	if (_frameLoop == nullptr || !_frameLoop->turnsPlayedBeside)
	{
		Locator::time::value().Update();
	}
	// The debug inspector answers what was asked since the last frame, and holds or releases the game for stepping,
	// before the frame's input and turn. When it answered before this frame, it does not answer again.
	if (Locator::inspector::has_value() && !std::exchange(_inspectorServedEarly, false))
	{
		Locator::inspector::value().Service();
	}
	// Stepped deterministically, a frame takes the fixed time the clock gives it, whatever the wall clock says
	if (const auto fixed = Locator::time::value().GetFixedFrameTime(); fixed.has_value())
	{
		frame.deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(*fixed);
	}

	// The physics world isn't stepped: the game's objects don't move as rigid bodies, and the world only answers the
	// rays cast for the hand, the camera and the like. Stepping it let the features fall and lose their turn.
}

std::chrono::duration<float, std::milli> Game::GetDeltaTime() const
{
	return Locator::landSystem::value().GetTurnDeltaTime();
}

const std::filesystem::path& Game::GetLandPath() const noexcept
{
	return Locator::landSystem::value().GetLandPath();
}

const std::filesystem::path& Game::GetLandscapePath() const noexcept
{
	return Locator::landSystem::value().GetLandscapePath();
}

bool Game::Initialize() noexcept
{
	auto& config = Locator::config::value();
	_startupTimer.emplace("startup", _launchTime);
	_startupTimer->Step("launch");

	if (config.graphicsBackend != GraphicsBackend::Noop || config.noDraw)
	{
		uint32_t extraFlags = 0;
		if (config.graphicsBackend == GraphicsBackend::Metal)
		{
			extraFlags |= SDL_WINDOW_METAL;
		}
		openblack::InitializeWindow(k_WindowTitle, config.resolution.x, config.resolution.y, config.displayMode, extraFlags,
		                            config.background);
	}

	using filesystem::Path;
	// The services that the game shares with the dedicated server come first, then the services that show the world
	InitializeSimulationCore();
	Locator::mainThreadQueue::value().SetCheck(_turnApiCheck);
	if (!InitializeEngine(config.graphicsBackend, config.vsync))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize engine services.");
		return false;
	}
	// The sound starts at the volumes that this computer keeps
	if (config.keptEffectsVolume.has_value())
	{
		Locator::audio::value().SetSfxVolume(*config.keptEffectsVolume);
	}
	if (config.keptMusicVolume.has_value())
	{
		Locator::audio::value().SetMusicVolume(*config.keptMusicVolume);
	}
	// The renderer starts at the window's size
	if (Locator::windowing::has_value())
	{
		_windowSize = windowing::SizeFollower(glm::u16vec2(Locator::windowing::value().GetSize()));
	}
	_startupTimer->Step("window, renderer and audio device");
	auto& fileSystem = Locator::filesystem::value();
	_buildVersion = game_assets::ReadBuildVersion(fileSystem);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "openblack build {}", _buildVersion.Text());
	auto& events = Locator::events::value();

	events.AddHandler(std::function([this, &config](const SDL_Event& event) {
		// If gui captures this input, do not propagate
		if (!Locator::debugGui::value().ProcessEvents(event))
		{
			// At each key that goes down, a repeat of a held key too, the game finds again whether a box has the keyboard
			if (event.type == SDL_KEYDOWN)
			{
				Locator::gameActionSystem::value().BoxKeyDown(_interface && _interface->BoxTakesKeyboard());
			}
			// A full-screen video takes Escape: it fades out, unless Shift or Ctrl is held
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    Locator::videoSystem::value().Escape((event.key.keysym.mod & KMOD_SHIFT) != 0,
			                                         (event.key.keysym.mod & KMOD_CTRL) != 0))
			{
				return;
			}
			// The tattoo editor's dialog takes the keyboard and mouse while it is open, Escape and Enter included
			if (_interface && _interface->GetTattooEditor().IsOpen() && Locator::windowing::has_value() &&
			    _interface->ProcessEvent(event, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize())))
			{
				return;
			}
			// The multiplayer front end's boxes take the keyboard and the mouse while they are up, Escape too
			if (_interface && _interface->GetMultiplayerBoxes().IsActive() && Locator::windowing::has_value() &&
			    _interface->ProcessEvent(event, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize())))
			{
				return;
			}
			// The line of the chat takes the keyboard while it has the focus, Escape and Return too
			if (_interface && _interface->ChatWantsKeys() && Locator::windowing::has_value() &&
			    _interface->ProcessEvent(event, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize())))
			{
				return;
			}
			// Inside the temple, Escape goes back to its main room and out, as the temple's keys do
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    Locator::temple::has_value() && Locator::temple::value().Active())
			{
				Locator::temple::value().Escape();
				return;
			}
			// The Creature Cave shown on its own closes on Escape
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    Locator::creatureCaveSystem::has_value() && Locator::creatureCaveSystem::value().Escape())
			{
				return;
			}
			// F2 in play asks the question of the trip to the Gods' Playground. The game takes the key, also when it asks
			// nothing.
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F2 && event.key.repeat == 0 && _interface)
			{
				HandleTripKey();
				return;
			}
			// Escape in play, with no dialog open. The box of a question of the trip takes Escape itself.
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && _interface && !_interface->IsDialogOpen() &&
			    !_interface->GetTattooEditor().IsOpen() && !_interface->IsTripQuestionUp())
			{
				HandleEscapeInPlay(event.key.keysym.mod);
				return;
			}
			// The game's menu takes Escape, and the keyboard and mouse while it is open
			if (_interface && Locator::windowing::has_value() &&
			    _interface->ProcessEvent(event, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize())))
			{
				HandleInterfaceAction();
				return;
			}
			// An event that stops the game keeps it stopped, whatever the events after it in the same frame are
			if (!this->ProcessEvents(event))
			{
				config.running = false;
			}
			Locator::gameActionSystem::value().ProcessEvent(event);
		}
	}));

	if (!fileSystem.IsPathValid(_gamePath))
	{
		// no key, don't guess, let the user know to set the command param
		ShowFatalMessage("Game Path missing", "Game path was not supplied, use the -g command parameter to set it.");
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to find the GameDir.");
		return false;
	}

	fileSystem.SetGamePath(_gamePath);

	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "The GamePath is \"{}\".", fileSystem.GetGamePath().generic_string());
	Locator::config::value().language = game_assets::ReadGameLanguage(fileSystem);

	if (std::filesystem::path(_startMap).is_absolute())
	{
		if (std::find(_startMap.begin(), _startMap.end(), "Scripts") != _startMap.end())
		{
			auto p = _startMap;
			while (p.filename() != "Scripts" && p != p.parent_path())
			{
				p = p.parent_path();
			}
			fileSystem.AddAdditionalPath(p.parent_path());
		}
		else
		{
			fileSystem.AddAdditionalPath(_startMap.parent_path());
		}
	}
	else if (const auto land = _startMap == k_DefaultStartLevel ? MapScriptLand() : std::nullopt; land)
	{
		// With no land asked for, a new game starts on the land that the map script names
		_startMap = *land;
	}
	else if (online_map_land::IsOnlineMap(_startMap.generic_string()))
	{
		// A land of a network game is in the game's folder, such as "Online Maps/mpm_2p_1.map"
	}
	else
	{
		_startMap = fileSystem.GetPath<Path::Scripts>() / _startMap;
	}

	const auto profiles = ProfileStore();
	_developerProfile = profiles.developer;
	if (!InitializeGame(profiles))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize game services.");
		return false;
	}
	_startupTimer->Step("game services and shaders");
	// The game is the view of the land's loads
	Locator::landSystem::value().SetView(this);
	Locator::landSystem::value().SetNewGameStart(_newGameStart);
	// A game that joins a game server gets the humans of its land from the network session
	Locator::landSystem::value().SetHumansFromSession(_networkJoin.has_value());
	// A deterministic run: every random number from the seed, the date pinned
	if (_seed.has_value())
	{
		Locator::rng::value().SetRunSeed(*_seed);
		Locator::time::value().RestartClock(ecs::systems::TimeSystemInterface::k_DeterministicDate);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Seeded run: seed {}, date pinned", *_seed);
	}
	// A simulation run: each frame takes a fixed game time from the first, so that the run plays the same turns at any
	// speed
	if (_simulation.has_value())
	{
		Locator::simulation::value().SetSettings(_simulation->settings);
		if (const auto refusal = simulation::ApplySoakTools(_simulation->soak); refusal.has_value())
		{
			SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The simulation run cannot start: {}", *refusal);
			return false;
		}
		if (_simulation->frameTime.has_value())
		{
			Locator::time::value().SetFixedFrameTime(*_simulation->frameTime);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Simulation run: fast {}, frame time {} us, draw every {}, until turn {}",
		                   _simulation->settings.fast, _simulation->frameTime.value_or(std::chrono::microseconds {0}).count(),
		                   _simulation->settings.drawEvery, _simulation->settings.untilTurn.value_or(0));
	}
	if (_throwLeadLatencyMs.has_value())
	{
		Locator::handSystem::value().FixReportLatency(_throwLeadLatencyMs);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The reports of the hand have a fixed latency of {} ms", *_throwLeadLatencyMs);
	}
	// A game that joins a server from the command line loads its land as a network game, as the server does
	if (_networkGame || _networkJoin.has_value())
	{
		Locator::time::value().SetNetworkGame(true);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The world plays as a network game");
	}
	// The debug inspector answers from the first frame; the game carries on without it if it can't listen. Until then,
	// while the game's data loads, it answers that the game is loading
	std::optional<ecs::systems::InspectorLoading> loadingData;
	if (_inspectPort.has_value())
	{
		// An agent drives it: the player's mouse and keyboard are kept out while a client is connected, so that a knock
		// of the mouse doesn't spoil its tests; from the start for a headless run, or never when asked
#if defined(OPENBLACK_INSPECTOR)
		const bool started = StartInspector(*_inspectPort, inspector::MakeGameSeam());
#else
		const bool started = StartInspector(*_inspectPort);
#endif
		if (started)
		{
			loadingData.emplace("game data");
			Locator::gameActionSystem::value().SetInputLockMode(_inspectInputLock);
			StartFightTools();
		}
	}
	StartSimulationLogs();

	// The logos, the first run's pre-intro and the tips screen, which stays up while the rest loads. Closing the window
	// meanwhile ends the game at its first frame
	PlayStartupScreens();

	// Every resource of the game's data, as the dedicated server registers them too. The game is the view of the start.
	if (!simulation::RegisterWorldData(*this))
	{
		return false;
	}
	_prefetchTimer.emplace("of every resource");
	return true;
}

const gui::TextDatabase* Game::MakeInterface()
{
	// The game's menu, which greets the player by their profile's name and shows their creature's name and their
	// symbol; before a first start's player has a profile, by the name they log in with
	auto [playerName, settings] = simulation::ProfileMenuSettings();
	auto& audio = Locator::audio::value();
	const auto kept = gui::ToProfileSettings(settings);
	const auto& pictures = Locator::tattooEditorSystem::value().GetPictures();
	_interface = gui::GameInterface::Create(playerName, std::move(settings), pictures);
	if (_interface)
	{
		// The films find the fade through the locator; the interface stays the owner
		Locator::screenFade::reset(&_interface->GetScreenFade(), [](gui::ScreenFade* /*unowned*/) {});
	}
	ApplyPlayerSettings(kept);
	LoadProfileKeys();
	RefreshPlayersPage();
	if (!_interface)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The game's menu is not available, Escape quits");
	}
	else
	{
		// The slider of the help's text speed shows only when the game has no sound
		_interface->GetMenu().SetSoundInstalled(audio.IsInstalled());
		// Each save and each load shows its box first
		if (Locator::saveGameSystem::has_value())
		{
			Locator::saveGameSystem::value().SetShowBox([this](bool loading, bool darken) { ShowSavingBox(loading, darken); });
			// A save that did not write its world files completely is told to the player in a box
			Locator::saveGameSystem::value().SetReportProblem([this]() { _interface->ShowSaveProblem(); });
		}
		// The Game Statistics box takes the players' figures as it opens, from the menu and from the end box
		const auto statistics = [] {
			const auto local =
			    Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
			return std::pair(player_statistics::Box(), local);
		};
		_interface->GetMenu().SetStatisticsSource(statistics);
		_interface->GetEndGameBox().SetStatisticsSource(statistics);
		// The box of the winning conditions takes the gods' standings as it opens
		_interface->GetMenu().SetConditionsSource(ConditionsFigures);
		_interface->GetEndGameBox().SetConditionsSource(ConditionsFigures);
		// Each time the end box shows that the player lost while the game goes on, the advisors can say their one-off
		// line about it
		_interface->GetEndGameBox().SetShownHandler([](const gui::EndGameBox::Contents& contents) {
			if (contents.box.kind == end_game_box::Kind::Lost && Locator::guidanceSystem::has_value())
			{
				Locator::guidanceSystem::value().SayOneOff(guidance::OneOff::NotVeryGood);
			}
		});
		if (Locator::temple::has_value())
		{
			Locator::temple::value().SetInterface(_interface.get());
		}
		if (Locator::creatureCaveSystem::has_value())
		{
			Locator::creatureCaveSystem::value().SetInterface(_interface.get());
		}
		if (Locator::miracleFxSystem::has_value())
		{
			Locator::miracleFxSystem::value().SetInterface(_interface.get());
		}
		if (Locator::pauseTextSystem::has_value())
		{
			Locator::pauseTextSystem::value().SetInterface(_interface.get());
		}
		if (Locator::incomingMessageSystem::has_value())
		{
			Locator::incomingMessageSystem::value().SetTexts(&_interface->GetTexts());
		}
		if (Locator::chatSystem::has_value())
		{
			Locator::chatSystem::value().SetTexts(&_interface->GetTexts());
		}
		if (Locator::gatheringBoxSystem::has_value())
		{
			auto* controls = &_interface->GetChatControls();
			Locator::gatheringBoxSystem::value().SetBox({
			    .people = [controls]() { return &controls->GetPeople(); },
			    .peopleChanged = [controls]() { controls->PeopleChanged(); },
			    .open = [controls]() { controls->Open(); },
			    .setFriendsOn = [controls](bool on) { controls->SetFriendsOn(on); },
			    .takeChangedFriends = [controls]() { return controls->TakeChangedFriends(); },
			    .setFriends =
			        [controls](std::span<const chat::people::FriendRecord> friends) { controls->SetFriends(friends); },
			});
		}
	}
	return _interface ? &_interface->GetTexts() : nullptr;
}

void Game::StartupStep(std::string_view step)
{
	if (_startupTimer.has_value())
	{
		_startupTimer->Step(step);
	}
}

void Game::StartSimulationLogs()
{
	if (!_simulation.has_value())
	{
		return;
	}
	if (_simulation->frameGuard != sim_run::HashDetail::None)
	{
		_frameGuard = std::make_unique<frame_write_guard::Guard>(_simulation->frameGuard == sim_run::HashDetail::Fields,
		                                                         _simulation->frameGuardSystems);
	}
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (_simulation->traceDrawsPath.has_value())
	{
		if (!draw_trace::Start(Locator::gameRandom::value(), *_simulation->traceDrawsPath))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the draw trace cannot open: {}",
			                    _simulation->traceDrawsPath->string());
		}
	}
#endif
	if (_simulation->checksumsPath.has_value())
	{
		_checksumLog = std::make_unique<checksum_log::Writer>(*_simulation->checksumsPath);
		if (_checksumLog->IsOpen())
		{
			auto& checksums = Locator::syncChecksumSystem::value();
			checksums.SetEnabled(true);
			checksums.SetKeepParts(_simulation->checksumParts != sim_run::HashDetail::None,
			                       _simulation->checksumParts == sim_run::HashDetail::Fields);
			if (_simulation->checksumDump.has_value())
			{
				checksums.SetDump(_simulation->checksumDump->first, _simulation->checksumDump->second);
			}
			checksums.SetTurnListener(
			    [log = _checksumLog.get()](const ecs::systems::SyncChecksumSystemInterface::TurnChecksum& turn) {
				    log->Write(turn.turn, turn.checksum, turn.parts, turn.unregistered);
				    if (!turn.dump.empty())
				    {
					    log->WriteDump(turn.turn, turn.dump);
				    }
			    });
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the checksums cannot open: {}",
			                    _simulation->checksumsPath->string());
		}
	}
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (_simulation->rowDumpPath.has_value())
	{
		// The rows come from the checksum of each turn, so the checksum is on also in a single player game
		auto& checksums = Locator::syncChecksumSystem::value();
		if (checksums.StartRowDump(*_simulation->rowDumpPath, _simulation->rowDump))
		{
			checksums.SetEnabled(true);
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the row dump cannot open: {}",
			                    _simulation->rowDumpPath->string());
		}
	}
	if (_simulation->worldStates.has_value())
	{
		Locator::syncChecksumSystem::value().WriteWorldStates(*_simulation->worldStates);
	}
#endif
	if (_simulation->playCommandsPath.has_value())
	{
		auto turns = command_log::ReadFile(*_simulation->playCommandsPath);
		if (!turns.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the commands does not read: {}",
			                    _simulation->playCommandsPath->string());
			return;
		}
		auto& commands = Locator::playerCommandSystem::value();
		for (auto& [turn, turnCommands] : *turns)
		{
			commands.Receive(turn, std::move(turnCommands));
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Commands of {} turns to play from {}", turns->size(),
		                   _simulation->playCommandsPath->string());
	}
}

void Game::StartFightTools()
{
	// A tool that steps a simulation run has each land start paused
	if (_simulation.has_value() && _simulation->startPaused && Locator::inspector::has_value())
	{
		Locator::inspector::value().PauseOnLoad(true);
	}
	// The run record of a simulation run starts with the inspector
	if (_simulation.has_value() && _simulation->recordPath.has_value() && Locator::inspector::has_value())
	{
		if (const auto error = Locator::inspector::value().StartSimRecord(*_simulation->recordPath, _simulation->recordEvery,
		                                                                  _simulation->limitsPath, _simulation->hashParts,
		                                                                  _simulation->recordFieldsEvery);
		    error.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The simulation record cannot start: {}", *error);
		}
	}
	// A join from the command line starts the input on the server's land
	if (!_networkLandPending)
	{
		StartInputTools();
	}
	// A tool's run can write the commands of each turn, for a dedicated server that does the same commands
	if (_simulation.has_value() && _simulation->commandsPath.has_value())
	{
		_commandLog = std::make_unique<command_log::Writer>(*_simulation->commandsPath);
		_scenarioGives = std::make_unique<command_log::GivesWriter>(command_log::GivesPathOf(*_simulation->commandsPath));
		if (_commandLog->IsOpen())
		{
			Locator::playerCommandSystem::value().SetTurnListener(
			    [log = _commandLog.get()](uint32_t turn, std::span<const player_commands::Command> commands) {
				    log->Write(turn, commands);
			    });
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the commands cannot open: {}",
			                    _simulation->commandsPath->string());
		}
	}
	if (_fightTranscriptFolder.has_value() && Locator::fightTranscript::has_value())
	{
		Locator::fightTranscript::value().SetOutputFolder(_fightTranscriptFolder);
	}
	if (!_fightAutopilot.has_value())
	{
		return;
	}
	auto* autopilot = Locator::inspector::has_value() ? Locator::inspector::value().GetFightAutopilot() : nullptr;
	const auto policy = fight_autopilot::PolicyFromName(_fightAutopilot->policy);
	const auto stage = fight_autopilot::LessonStageFromName(_fightAutopilot->stage);
	const auto sequence = fight_autopilot::ParseSequence(_fightAutopilot->sequence);
	if (autopilot == nullptr || !policy.has_value() || (!_fightAutopilot->stage.empty() && !stage.has_value()) ||
	    !sequence.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The fight autopilot cannot start: check its policy, stage and sequence");
		return;
	}
	const auto error = autopilot->Start(
	    {.policy = *policy, .seed = _fightAutopilot->seed, .stage = stage, .sequence = *sequence, .enterView = true});
	if (!error.empty())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The fight autopilot cannot start: {}", error);
	}
}

void Game::ShowSavingBox(bool loading, bool darken)
{
	// A run that draws no frame shows no box
	if (!_interface || !Locator::windowing::has_value() ||
	    !sim_run::DrawsFrame(Locator::simulation::value().GetSettings(), _frameCount))
	{
		return;
	}
	_interface->DrawSavingBox(static_cast<glm::u16vec2>(Locator::windowing::value().GetSize()), loading, darken);
	// A picture asked for this frame shows the box, as the screen does
	if (_requestScreenshot.has_value() && _requestScreenshot->first == _frameCount)
	{
		Locator::rendererInterface::value().RequestScreenshot(_requestScreenshot->second);
		_requestScreenshot.reset();
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The {} box shows at frame {}", loading ? "loading" : "saving", _frameCount);
	Locator::rendererInterface::value().Frame();
}

const std::filesystem::path& Game::ChallengePath()
{
	if (_challengePath.has_value())
	{
		return *_challengePath;
	}
	auto& fileSystem = Locator::filesystem::value();
	_challengePath = fileSystem.GetPath<filesystem::Path::Quests>() / "challenge.chl";
	if (!_launchSwitches.settingsFile.has_value())
	{
		return *_challengePath;
	}
	// The game opens the settings file from its own folder. When the file does not open, the usual folder stays.
	const std::filesystem::path settingsFile(*_launchSwitches.settingsFile);
	if (!fileSystem.Exists(settingsFile))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Settings file {} not found", settingsFile.generic_string());
		return *_challengePath;
	}
	const auto bytes = fileSystem.ReadAll(settingsFile);
	const auto folder =
	    launch_switches::QuestFolderOf(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
	if (folder.has_value())
	{
		_challengePath = std::filesystem::path(launch_switches::ChallengePath(*folder));
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Settings file {}: the challenge file is {}", settingsFile.generic_string(),
		                   _challengePath->generic_string());
	}
	return *_challengePath;
}

bool Game::Run() noexcept
{
	switch (StartRun())
	{
	case RunStart::Failed:
		return false;
	case RunStart::Done:
		return true;
	case RunStart::Play:
		break;
	}
	while (RunFrame())
	{
	}
	EndRun();
	return true;
}

Game::RunStart Game::StartRun() noexcept
{
	auto& config = Locator::config::value();

	// The -SKIRMISH switch shows the skirmish box before the first land, when the player has a profile. The
	// -MULTIPLAYER switch opens the multiplayer front end before the first frame, in the temple of the land that
	// starts, when the player has a profile.
	auto start = game_session::Start::NewGame;
	// A network game's land starts from the state that every computer of the game starts it from
	if (_networkGame || _networkJoin.has_value())
	{
		simulation::PrepareNetworkLand(Locator::rng::value().GetRunSeed());
	}
	if (_startGodsRingMap)
	{
		// The map for 32 gods, as a network game plays it
		if (!LoadGodsRingMap())
		{
			return RunStart::Failed;
		}
	}
	else if (_startTestbed)
	{
		LoadTestbed();
	}
	else
	{
		// A first start: the new player is asked their name before the first land; closing the window meanwhile ends
		// the game at its first frame. A first start always starts a new game.
		const bool hadProfile = HasPlayerProfiles();
		AskForNewPlayer();
		// The skirmish box comes only for a player who had a profile. Start Game starts the skirmish on the land picked;
		// Back continues as a plain start does, from the save on exit or with a new game.
		std::optional<std::filesystem::path> skirmishLand;
		bool skirmishBack = false;
		if (launch_switches::FirstGameOf(_launchSwitches) == launch_switches::FirstGame::Skirmish && hadProfile &&
		    !_newGameAsked)
		{
			skirmishLand = AskForSkirmishLand();
			skirmishBack = !skirmishLand.has_value();
			// The window closed, or the run reached its number of frames, while the box was up
			if (_quitRequested)
			{
				return RunStart::Done;
			}
		}
		// With no player profile, the -MULTIPLAYER switch starts a new game as with no switch. In a browser, the front
		// end opens after the new player's box.
		_frontEndOnStart = join_online_entry::FrontEndOnStart({
		    .multiplayerSwitch = launch_switches::FirstGameOf(_launchSwitches) == launch_switches::FirstGame::Multiplayer,
		    .hadProfile = hadProfile,
		    .hasProfile = HasPlayerProfiles(),
		    .browser = k_Browser,
		});
		// A plain start continues from the save on exit of the player. The continue loads the save once the scripts are
		// ready, because the save keeps the state of the scripts.
		start = game_session::FirstStart({.hadProfile = hadProfile,
		                                  .developerRun = _automatedRun,
		                                  .newGameAsked = _newGameAsked,
		                                  .continueAsked = _continueAsked});
		if (skirmishLand.has_value())
		{
			start = game_session::Start::Skirmish;
		}
		else if (skirmishBack)
		{
			start = game_session::Start::Continue;
		}
		start = game_session::Resolve(start,
		                              Locator::saveGameSystem::has_value() && Locator::saveGameSystem::value().HasExitSave());
		if (start == game_session::Start::Skirmish &&
		    !LoadMap(*skirmishLand, loading::LoadingClock::Mode::Tips, land_change::Kind::Skirmish))
		{
			return RunStart::Failed;
		}
		if (start == game_session::Start::NewGame &&
		    !LoadMap(_startMap, loading::LoadingClock::Mode::Tips, land_change::Kind::NewGame))
		{
			return RunStart::Failed;
		}
	}
	if (_startupTimer.has_value())
	{
		_startupTimer->Step("first land");
	}
	// The time played that the save game room shows counts from here, by the system clock
	if (Locator::playerProfileSystem::has_value())
	{
		Locator::playerProfileSystem::value().StartPlaying(static_cast<int64_t>(std::time(nullptr)));
	}

	// The story's scripts run the first land; on the testbed they would set its time of day and stop its clock. A
	// playground starts as the playground menu starts one: the scripts are made ready, but no story begins and the
	// start-of-game question isn't asked. A network game starts no story either.
	auto& lands = Locator::landSystem::value();
	const bool startStory = !_startTestbed && start == game_session::Start::NewGame &&
	                        new_game_choice::FirstLandStartsStory(Locator::entitiesRegistry::value().Context().skirmish,
	                                                              Locator::time::value().IsNetworkGame());
	if (!lands.StartChallengeScripts(startStory))
	{
		return RunStart::Failed;
	}
	// A continue loads the save on exit, or starts a new game when it cannot
	bool continued = false;
	if (start == game_session::Start::Continue)
	{
		continued = ContinueGame();
		if (lands.GetLandPath().empty())
		{
			return RunStart::Failed;
		}
	}
	if (_startupTimer.has_value())
	{
		_startupTimer->Step("story scripts");
	}
	// The -CONVERT switch: the game loads its first land and then closes before the first frame of play. The logo
	// effect does not play.
	if (_launchSwitches.convert && !_startTestbed)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The -CONVERT switch: the land is loaded, the game closes");
		return RunStart::Done;
	}
	// The world starts to play, with the game as its view
	// A run with a fixed frame time and a network game load every resource before the first frame, as the game server
	const bool loadAllFirst =
	    (_simulation.has_value() && _simulation->frameTime.has_value()) || _networkGame || _networkJoin.has_value();
	simulation::BeginPlay(continued, loadAllFirst, *this);
	_frameCount = 0;
	_frameLoop = std::make_unique<FrameLoop>();
	_frameLoop->startTicks = Locator::time::value().GetTicks();
	// The hash of the checksum of the last turn of a frame waits for the draw, and the threads make it while the draw
	// reads the world
	if (Locator::syncChecksumSystem::has_value())
	{
		Locator::syncChecksumSystem::value().SetHashBesideDraw(true);
	}
	if (config.aiTimesEvery > 0 && Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetTimesReport(config.aiTimesEvery);
	}
	if (config.frameStatsInterval > 0)
	{
		_frameLoop->frameStats.emplace(config.frameStatsInterval, config.frameStatsViews,
		                               Locator::rendererInterface::value().GetPipelinesMade(),
		                               std::filesystem::path(config.frameStatsOut));
		if (config.frameStatsViews)
		{
			Locator::rendererInterface::value().SetProfile(true);
		}
	}
	_frameLoop->frameStart = std::chrono::steady_clock::now();
	return RunStart::Play;
}

void Game::StartPlayView()
{
	if (_filmSurface == "555")
	{
		Locator::videoSystem::value().SetFilmSurface(graphics::film_surface::Format::Rgb555);
	}
	else if (_filmSurface == "565")
	{
		Locator::videoSystem::value().SetFilmSurface(graphics::film_surface::Format::Rgb565);
	}
	else if (_filmSurface == "none")
	{
		Locator::videoSystem::value().SetFilmSurface(std::nullopt);
	}
	if (_playVideo == "intro")
	{
		Locator::videoSystem::value().Play(ecs::systems::k_IntroFilm);
		Locator::videoSystem::value().ScheduleIntro();
	}
	else if (_playVideo == "fall")
	{
		Locator::videoSystem::value().StartFallingSpell();
	}
	else if (!_playVideo.empty())
	{
		Locator::videoSystem::value().Play(_playVideo);
	}

	// The custom music player gets the tracks of the custom music folder in the profile folder
	if (Locator::musicMoodSystem::has_value())
	{
		Locator::musicMoodSystem::value().LoadFolder();
	}
	// The game loop starts: the help record of the player comes from their profile, and the game writes it back at once
	LoadHelpRecord();
	SaveHelpRecord();
}

void Game::BeforeFirstFrame(bool continued)
{
	// A join from the command line: the land loaded, so the game joins the server and waits for its start
	if (auto join = std::exchange(_networkJoin, std::nullopt); join.has_value() && Locator::networkGame::has_value())
	{
		_joinedFromCommandLine = Locator::networkGame::value().Join(std::move(*join));
		if (!_joinedFromCommandLine)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The game cannot join the game server");
		}
		// The game is in a network game already, so the -MULTIPLAYER switch does not open the front end
		_frontEndOnStart = false;
	}
	// The first game of the program starts with the logo effect and its sound, before the menu shows
	if (startup::PlaysLogoEffect(_automatedRun, _logoEffectAsked, false))
	{
		PlayLogoEffect();
	}
	// The -MULTIPLAYER switch: the multiplayer front end opens in place of the menu
	if (std::exchange(_frontEndOnStart, false) && OpenFrontEndInPlaceOfMenu())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The -MULTIPLAYER switch: the multiplayer front end opens");
		return;
	}
	// A game that continued from the save on exit starts under the menu
	if (game_session::ShowsMenuOnStart(game_session::Start::Continue, continued))
	{
		ShowMenuOnStart();
	}
}

bool Game::ServeHeldRun()
{
	if (!Locator::inspector::has_value() || !Locator::time::has_value())
	{
		return false;
	}
	const bool steppedByTool = _simulation.has_value() && _simulation->startPaused;
	const bool workWaits = _scenarioRequest.has_value() || _requestScreenshot.has_value();
	if (!sim_run::SkipsHeldFrame(steppedByTool, Locator::time::value().IsPaused(), workWaits))
	{
		return false;
	}
	Locator::inspector::value().Service();
	// The window stays responsive. The events wait in the queue for the next frame that plays.
	SDL_PumpEvents();
	if (!Locator::time::value().IsPaused())
	{
		// The tool released the game: this frame plays, and it does not ask the inspector again
		_inspectorServedEarly = true;
		return false;
	}
#if !defined(__EMSCRIPTEN__)
	// The tool holds the game: the loop waits a short time, so that it does not use all of the processor
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
	return true;
}

bool Game::RunFrame() noexcept
{
	// A run that a tool steps plays no frame while the tool holds it. The wall clock sets the number of these frames.
	if (ServeHeldRun())
	{
		return true;
	}
	// The work of the frame starts here. In a web browser, the wait for the display frame is before this point.
	const auto workStart = std::chrono::steady_clock::now();
	const auto turnBefore = Locator::time::has_value() ? std::optional(Locator::time::value().GetTurn()) : std::nullopt;
	if (!Update())
	{
		// The game ends with this frame: the checksum of its last turn does not wait for a draw
		Locator::syncChecksumSystem::value().FinishHash();
		return false;
	}
	auto& config = Locator::config::value();
	auto& profiler = Locator::profiler::value();
	const auto startTicks = _frameLoop->startTicks;
	auto& frameStats = _frameLoop->frameStats;
	auto& frameStart = _frameLoop->frameStart;
	{
		// What the loading threads finished goes into the caches, and they're given a little more
		auto& resources = Locator::resources::value();
		resources.UpdateLoading(k_FrameLoadBudget, k_FrameUploads);
		if (_prefetchTimer.has_value() && resources.PendingCount() == 0)
		{
			_prefetchTimer.reset();
		}

		// The time since the loop started, in milliseconds, by the game's clock
		const uint32_t drawMs = Locator::time::value().GetTicks() - startTicks;
		// The frame is drawn from where the inspector shows the camera (an override, a picture's), while everything the
		// game did this frame went by its own camera, which it gets back once the frame is drawn
		ShowInspectorCamera(true);
		// The game's interface over the scene, its labels over things placed from the camera the scene is drawn from. It
		// is laid out before the scene, so that the creatures it shows over itself are drawn in the same frame as it.
		PlaceScreenLabels();
		PlaceLearningTexts();
		// In the fast simulation mode the game draws only some frames, or none. It also draws the frame of a picture
		// that a tool asks for.
		const bool pictureWanted = _requestScreenshot.has_value() && _requestScreenshot->first == _frameCount;
		const bool drawsFrame =
		    sim_run::DrawsFrame(Locator::simulation::value().GetSettings(), _frameCount, pictureWanted) && DoesFrameGraphics();
		if (drawsFrame && _interface && Locator::windowing::has_value())
		{
			const auto mouse = Locator::gameActionSystem::value().GetPointerPosition();
			_interface->Draw(static_cast<glm::u16vec2>(Locator::windowing::value().GetSize()), mouse, machine_clock::Ticks(),
			                 Locator::debugGui::value().IsMouseOverWindow());
			ShowTattooMenuCreature();
		}
		if (drawsFrame)
		{
			auto section = profiler.BeginScoped(Profiler::Stage::SceneDraw);
			const auto drawDesc = SceneDrawDesc(config, drawMs, IsHandDrawn());
			// The sun, the moon and the dome's blend of this frame, for drawing it
			if (Locator::skySystem::has_value())
			{
				Locator::skySystem::value().UpdateFrame(config.drawSky);
			}
			// Nothing of the world shows under a video that covers the screen, nor behind the falling spell's film
			const auto& videos = Locator::videoSystem::value();
			if (videos.CoversScreen() || videos.HidesWorld())
			{
				bgfx::touch(static_cast<bgfx::ViewId>(graphics::RenderPass::Main));
				// The interface's creatures of this frame are not shown, and are not kept for the next
				Locator::rendererInterface::value().ShowInterfaceCreatures({});
				DrawFilmCreature(drawDesc);
			}
			else
			{
				Locator::syncChecksumSystem::value().StartHashBesideDraw();
				Locator::rendererInterface::value().DrawScene(drawDesc);
			}
		}
		// The checksum of the turns of this frame is complete before anything changes the world again
		{
			auto section = profiler.BeginScoped(Profiler::Stage::SyncChecksum);
			Locator::syncChecksumSystem::value().FinishHash();
		}

		ResolveScreenShapes();

		{
			auto section = profiler.BeginScoped(Profiler::Stage::GuiDraw);
			const bool screenshotThisFrame = _requestScreenshot.has_value() && _requestScreenshot->first == _frameCount;
			if (screenshotThisFrame)
			{
				Locator::rendererInterface::value().RequestScreenshot(_requestScreenshot->second);
			}
			if (_deviceRestoreFrame == _frameCount)
			{
				Locator::rendererInterface::value().RequestDeviceRestore(graphics::device_restore::Request::MakeAgain);
			}
			if (_deviceLossFrame == _frameCount)
			{
				Locator::rendererInterface::value().RequestDeviceRestore(graphics::device_restore::Request::Lose);
			}
			// A picture without the debug windows: the frame's windows are made as ever but not drawn
			// nor in a challenge's picture
			const bool hiddenThisFrame =
			    _debugGuiHiddenFrame == _frameCount ||
			    ecs::challenge_pictures::AnyWaiting(Locator::entitiesRegistry::value()) ||
			    (Locator::creatureWebPageSystem::has_value() && Locator::creatureWebPageSystem::value().AnyPictureWaiting()) ||
			    (Locator::saveGameSystem::has_value() && Locator::saveGameSystem::value().AnyPictureWaiting());
			if (drawsFrame && (!screenshotThisFrame || !_screenshotHidesDebugGui) && !hiddenThisFrame)
			{
				Locator::debugGui::value().Draw();
			}
		}

		// The turns of the next frame may play beside the end of the draw (deviation row 153)
		const bool turnsBeside = StartTurnsBesideDraw();
		{
			auto section = profiler.BeginScoped(Profiler::Stage::RendererFrame);
			Locator::rendererInterface::value().Frame();
		}
		if (turnsBeside)
		{
			FinishTurnsBesideDraw();
		}
		// The challenges' pictures whose screens are back
		auto& renderer = Locator::rendererInterface::value();
		ecs::challenge_pictures::Collect(
		    Locator::entitiesRegistry::value(),
		    {.take = [&renderer](const std::string& name) { return renderer.TakeScreenPicture(name); },
		     .failed = [&renderer](const std::string& name) { return renderer.TakeScreenshotFailure(name).has_value(); }},
		    ecs::challenge_pictures::ProfileFiles());
		// The pictures of the saved games whose screens are back
		if (Locator::saveGameSystem::has_value())
		{
			Locator::saveGameSystem::value().CollectPictures(
			    {.take = [&renderer](const std::string& name) { return renderer.TakeScreenPicture(name); },
			     .failed = [&renderer](const std::string& name) { return renderer.TakeScreenshotFailure(name).has_value(); }});
			// The saves whose world files did not write completely are told to the player
			Locator::saveGameSystem::value().ReportWriteProblems();
		}
		// The player goes into the temple after the frame that the game took the picture of
		if (Locator::temple::has_value())
		{
			Locator::temple::value().FrameDrawn();
		}
		// The pictures of the player's creature whose screens are back
		if (Locator::creatureWebPageSystem::has_value())
		{
			Locator::creatureWebPageSystem::value().CollectPictures(
			    {.take = [&renderer](const std::string& name) { return renderer.TakeScreenPicture(name); },
			     .failed = [&renderer](const std::string& name) { return renderer.TakeScreenshotFailure(name).has_value(); }});
		}
		// A quick load that a command of the turns asked for loads after the draw of the frame, as the game does. The load
		// forgets the commands of the old world.
		if (Locator::saveGameSystem::has_value())
		{
			std::ignore = Locator::saveGameSystem::value().LoadAskedSlot();
		}
		// The turns beside the draw had the camera of the game back before they started
		if (!turnsBeside)
		{
			ShowInspectorCamera(false);
		}

		// Clear the stale screenshot request
		if (_requestScreenshot.has_value())
		{
			if (_requestScreenshot->first <= _frameCount)
			{
				_requestScreenshot = std::nullopt;
			}
		}

		_frameCount++;
		// The frame rate of the statistics uses the machine clock of the game. When each frame has a fixed time, each run
		// gets the same rate.
		_statisticsFrameRate.Frame(std::exchange(_frameLoop->statisticsTicks, std::nullopt).value_or(machine_clock::Ticks()),
		                           _frameCount);
		// The game is playable from its first frame drawn
		if (_startupTimer.has_value())
		{
			_startupTimer->Step("first frame");
			_startupTimer.reset();
		}

		const auto workEnd = std::chrono::steady_clock::now();
		// A simulation run at the normal speed waits until the frame took its fixed game time in real time too
		auto& simulation = Locator::simulation::value();
		if (const auto wait = sim_run::PaceWait(
		        simulation.GetSettings(), Locator::time::value().GetFixedFrameTime(),
		        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - frameStart));
		    wait.count() > 0)
		{
			std::this_thread::sleep_for(wait);
		}
		const auto frameEnd = std::chrono::steady_clock::now();
		simulation.FrameEnded(std::chrono::duration<float, std::milli>(frameEnd - frameStart).count());
		{
			// The run record keeps the time of each stage too, to find what costs more as a run goes on
			const auto& entry = profiler.GetEntries().at(profiler.GetEntryIndex(0));
			std::array<float, static_cast<size_t>(Profiler::Stage::_count)> stageMs {};
			std::ranges::transform(entry.stages, stageMs.begin(), [](const Profiler::Scope& scope) {
				return std::chrono::duration<float, std::milli>(scope.total).count();
			});
			simulation.StagesEnded(stageMs);
		}
		if (frameStats.has_value())
		{
			frameStats->Frame({.milliseconds = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count(),
			                   .workMilliseconds = std::chrono::duration<float, std::milli>(workEnd - workStart).count(),
			                   .playedTurn = Locator::time::has_value() && turnBefore != Locator::time::value().GetTurn(),
			                   .turn = Locator::time::value().GetTurn()},
			                  profiler, Locator::rendererInterface::value().GetPipelinesMade());
		}
		frameStart = frameEnd;
		// A close of the window ends the game here, at the end of the frame, but not during a script's cut scene
		const auto& director = Locator::cinematicDirectorSystem::value();
		if (game_session::CloseEndsGame(_closeAsked, director.IsWideScreenOn() && director.GetWideScreenOwner() != 0))
		{
			return false;
		}
	}
	return true;
}

void Game::EndRun() noexcept
{
	// As the player leaves the game, the game saves it into the slot of the save on exit
	SaveAsGameEnds(game_session::End::Quit, true);
	// As the game ends, the player's creature is written to their files as it is now
	if (Locator::creatureCarryOverSystem::has_value())
	{
		Locator::creatureCarryOverSystem::value().EndGame();
	}
	// The last line of an orderly end: a log that stops without it, and without a crash report, was ended from outside
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game ends after {} frames, at turn {}", _frameCount,
	                   Locator::time::value().GetTurn());
	_frameLoop.reset();
}

bool Game::LoadMap(const std::filesystem::path& path, loading::LoadingClock::Mode look, land_change::Kind change) noexcept
{
	return Locator::landSystem::value().LoadMap(path, look, change);
}

bool Game::IsPlaygroundLand(const std::filesystem::path& path)
{
	return Locator::landSystem::value().IsPlaygroundLand(path);
}

bool Game::LoadFromLandMenu(const std::filesystem::path& path) noexcept
{
	// A playground land starts a skirmish: a new game on that land, the last game's creature kept for the profile it was
	// played as, every player starting afresh and the player as good or evil as their profile last kept them
	return LoadMapWithFreshScripts(path, land_change::FromLandMenu(IsPlaygroundLand(path)));
}

bool Game::LoadMapWithFreshScripts(const std::filesystem::path& path, land_change::Kind change) noexcept
{
	return Locator::landSystem::value().LoadMapWithFreshScripts(path, change);
}

void Game::LoadTestbed() noexcept
{
	// The testbed is a developer tool. A live network game never loads it.
	if (!developer_tools::Allowed(developer_tools::Action::ScenarioLoad))
	{
		return;
	}
	static_cast<void>(Locator::landSystem::value().LoadTestbed());
}

bool Game::RequestScenario(ScenarioRequest request)
{
	if (!developer_tools::Allowed(developer_tools::Action::ScenarioLoad))
	{
		return false;
	}
	_scenarioRequest = std::move(request);
	return true;
}

bool Game::LoadGodsRingMap() noexcept
{
	return Locator::landSystem::value().LoadGodsRingMap();
}

bool Game::LoadSavedLand(const std::filesystem::path& landPath, const std::filesystem::path& landscape,
                         const std::function<bool()>& restore) noexcept
{
	return Locator::landSystem::value().LoadSavedLand(landPath, landscape, restore,
	                                                  ecs::systems::LandSystemInterface::SavedLandUse::SavedGame);
}

void Game::StartLandAudio()
{
	if (!_atmosAudio)
	{
		_atmosAudio = std::make_unique<audio::AtmosAudio>();
	}
	_atmosAudio->Init();
	if (!_gameMusic)
	{
		// The -NOLOADMUSIC switch: the game loads none of its music banks, so none of its music plays
		_gameMusic = std::make_unique<audio::GameMusic>(!_launchSwitches.noLoadMusic);
		// The script, the saves and the films find the music through the locator; the game stays the owner
		Locator::gameMusic::reset(_gameMusic.get(), [](audio::GameMusic* /*unowned*/) {});
		if (Locator::scriptMusicSystem::has_value())
		{
			Locator::scriptMusicSystem::value().SetBanksLoaded(!_launchSwitches.noLoadMusic);
			Locator::scriptMusicSystem::value().RegisterTimings();
		}
		if (!_launchSwitches.noLoadMusic)
		{
			audio::GameMusic::RegisterBanks();
		}
	}
	_gameMusic->Reset();
}

void Game::ShowLoadingFrame()
{
	RenderLoadingFrame();
}

bool Game::WithdrawSkipBox()
{
	return _interface && _interface->WithdrawSkipBox();
}

void Game::MakeSymbolSheet()
{
	MakePlayersSymbols();
}

void Game::TestbedLoaded()
{
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (Locator::debugGui::has_value() && _testbedWindow)
	{
		Locator::debugGui::value().OpenWindow(debug::gui::k_TestbedScenariosWindow);
	}
#endif
}

bool Game::AskStartQuestion(bool hasCreature)
{
	if (!_interface)
	{
		return false;
	}
	// The game waits, paused, for the answer
	Locator::time::value().SetPlayerPause(true);
	// The multiplayer front end is in front of the land: the question comes when the player leaves the front end to
	// the land. A join from the command line loads the server's land, which does not ask the question.
	if (_interface->GetMultiplayerBoxes().IsActive() || _frontEndOnStart || _networkLandPending)
	{
		_startQuestionAfterFrontEnd = hasCreature;
		return true;
	}
	_interface->ShowSkipBox(hasCreature);
	return true;
}

std::optional<std::filesystem::path> Game::ChallengeFile()
{
	return ChallengePath();
}

std::optional<std::filesystem::path> Game::MapScriptLand() const
{
	return ecs::systems::MapScriptLand();
}

bool Game::StartNewGame(std::optional<new_game_choice::NewGameStart> start) noexcept
{
	_newGameStart = start;
	Locator::landSystem::value().SetNewGameStart(start);
	_startTestbed = false;
	// A new game begins on the land that the map script names
	const auto land = MapScriptLand();
	if (!land || !LoadMapWithFreshScripts(*land, land_change::Kind::NewGame))
	{
		return false;
	}
	if (!Locator::vm::has_value())
	{
		return false;
	}
	try
	{
		Locator::landSystem::value().StartStoryScripts();
	}
	catch (const std::exception& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The story's scripts didn't start: {}", err.what());
		return false;
	}
	return true;
}

void Game::FollowPlaygroundTrip()
{
	if (!_tripQuestion.has_value() || !_interface)
	{
		return;
	}
	auto answer = _interface->TakeTripAnswer();
	if (!answer.has_value())
	{
		if (_interface->IsTripQuestionUp())
		{
			return;
		}
		// The box went with no answer: this is the same as No
		answer = false;
	}
	const auto question = *std::exchange(_tripQuestion, std::nullopt);
	// The answer gives the keyboard back to the game at once
	Locator::gameActionSystem::value().BoxAnswered();
	auto& trip = Locator::playgroundTripSystem::value();
	// Each answer unpauses the game, but Yes to leave the Playground. The menu then comes back if it is open, and its main
	// page pauses the game again.
	auto& menu = _interface->GetMenu();
	if (question == playground_trip::Question::Go || !*answer)
	{
		Locator::time::value().SetPlayerPause(false);
		if (menu.IsOpen() && playground_trip::PausesWhenBack(_interface->GetTripMenuBox()))
		{
			Locator::time::value().SetPlayerPause(true);
		}
	}
	if (!*answer)
	{
		return;
	}
	if (question == playground_trip::Question::Go)
	{
		// The saved game keeps the pause that the game has now. The land of the Playground then loads. The trip closes no
		// box: an open menu and the land information box stay over the new land.
		_boxesStayForTrip = true;
		if (!trip.SaveBeforeTrip())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The save before the trip to the Gods' Playground did not write");
		}
		if (!trip.Travel())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The trip to the Gods' Playground did not load the land");
		}
		_boxesStayForTrip = false;
		return;
	}
	// The saved game loads with its own pause. With no saved game, the game stays paused.
	if (!trip.Leave())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "No saved game loaded as the player left the Gods' Playground");
	}
}

void Game::HandleInterfaceAction()
{
	using Action = gui::GameMenu::Action;
	const auto action = _interface->TakeAction();

	// The answer to the start-of-game question tells the story what to skip, and the game goes on
	if (const auto answer = _interface->TakeSkipBoxAnswer(); answer.has_value())
	{
		Locator::tutorialSkipSystem::value().Set(new_game_choice::SkipFor(*answer));
		Locator::time::value().SetPlayerPause(false);
	}

	// The edits on the Players page are kept with the profile played, before anything its buttons do
	if (const auto edits = _interface->GetMenu().TakePlayerEdits(); edits.has_value())
	{
		Locator::playerProfileSystem::value().EditCurrentProfile(edits->creatureName, edits->symbol);
		NameCreatureAfterProfile();
		SaveHelpRecord();
	}
	// A profile made in the new player's box from the Players page is played from now on
	if (auto& box = _interface->GetNewPlayerBox(); !box.IsActive())
	{
		if (const auto answer = box.TakeAnswer(); answer.has_value())
		{
			Locator::playerProfileSystem::value().CreateProfile(answer->name, answer->symbol);
			PlayerProfileChanged();
		}
		(void)box.TakeCancelled();
	}

	// The settings the player changes take effect at once
	if (_interface->TakeSettingsChanged())
	{
		const auto& settings = _interface->GetMenu().GetSettings();
		auto& audio = Locator::audio::value();
		audio.SetSfxVolume(settings.sfxVolume);
		audio.SetMusicVolume(settings.musicVolume);
		// The detail and the volumes are kept for this computer, as the options show them
		if (Locator::computerSettingsSystem::has_value())
		{
			Locator::computerSettingsSystem::value().Keep({
			    .detailLevel = static_cast<uint8_t>(settings.detail),
			    .maxDetailLevel = Locator::config::value().maxDetailLevel,
			    .effectsVolume = settings.sfxVolume,
			    .musicVolume = settings.musicVolume,
			});
		}
		const auto kept = gui::ToProfileSettings(settings);
		ApplyPlayerSettings(kept);
		// The player's options are kept with their profile
		Locator::playerProfileSystem::value().SetCurrentSettings(kept);
		SaveHelpRecord();
	}

	// A binding changed on the Controls page takes effect at once
	auto& menu = _interface->GetMenu();
	if (menu.TakeControlsChanged() && Locator::gameActionSystem::has_value())
	{
		Locator::gameActionSystem::value().SetKeyBindings(menu.GetControlBindings());
	}
	// The menu reads the bindings from the profile as it opens. Back from the Controls page, and the menu as it
	// closes, keep the bindings with the profile.
	if (menu.IsOpen() && !_menuWasOpen)
	{
		LoadProfileKeys();
	}
	if (menu.TakeControlsToKeep() || (!menu.IsOpen() && _menuWasOpen))
	{
		KeepProfileKeys();
	}
	_menuWasOpen = menu.IsOpen();

	// The menu pauses the game while it is open
	if (const auto pause = _menuPause.Follow(_interface->GetMenu().IsOpen()); pause.has_value())
	{
		Locator::time::value().SetPlayerPause(*pause);
		if (!_menuPause.IsHeld())
		{
			_menuClosedAt = std::chrono::steady_clock::now();
		}
	}
	// The main menu comes back under each page that the menu shows, and pauses the game again. This is important after
	// the question of the trip unpaused the game over another page. Inside the temple, the options show alone.
	if (menu.TakePageShown() && menu.IsOpen() && !menu.IsInsideTemple())
	{
		Locator::time::value().SetPlayerPause(true);
	}

	switch (action)
	{
	case Action::Quit:
		RequestQuit();
		break;
	case Action::CreatePlayer:
		AskForAnotherPlayer(true);
		break;
	case Action::PickPlayer:
		if (const auto picked = _interface->GetMenu().GetPickedPlayer(); picked.has_value())
		{
			auto& profiles = Locator::playerProfileSystem::value();
			const auto& list = profiles.GetProfiles().profiles;
			if (*picked < list.size())
			{
				profiles.SelectProfile(gui::ToUtf16(list.at(*picked).name));
				PlayerProfileChanged();
			}
		}
		break;
	case Action::DeletePlayer:
	{
		auto& profiles = Locator::playerProfileSystem::value();
		// The player's folder is emptied first: its saved games, pictures and help record go, but the empty folder stays
		const auto deleted = _interface->GetMenu().GetPlayerToDelete();
		if (Locator::profileFolderSystem::has_value())
		{
			Locator::profileFolderSystem::value().EmptyPlayerFolder(deleted);
		}
		profiles.DeleteProfile(deleted);
		const auto next = _interface->GetMenu().GetPickedPlayer();
		const auto& list = profiles.GetProfiles().profiles;
		if (next.has_value() && *next < list.size())
		{
			profiles.SelectProfile(gui::ToUtf16(list.at(*next).name));
			PlayerProfileChanged();
		}
		else
		{
			// With no profile left the new player's box asks for one, and can't be cancelled
			PlayerProfileChanged();
			AskForAnotherPlayer(false);
		}
		break;
	}
	case Action::StartNewGame:
	case Action::Restart:
		// This game ends. Start a New Game starts a new game on the first land. A restart as the profile picked continues
		// from the save on exit of that profile, or starts a new game when it has none. The land changes between frames,
		// not while the game reads the input.
		_interface->GetMenu().Close();
		_gameEndAsked = action == Action::StartNewGame ? game_session::End::NewGame : game_session::End::Continue;
		break;
	case Action::EditTattoo:
		// The profile is kept first, then the editor shows the profile's creature over the menu
		if (Locator::tattooMenuSystem::has_value())
		{
			Locator::tattooMenuSystem::value().Open();
		}
		break;
	case Action::JoinOnline:
		if (!OpenMultiplayerFrontEnd(MakeMultiplayerFrontEnd()))
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The multiplayer front end needs the temple and the interface");
		}
		break;
	case Action::StartSkirmish:
		// The game ends at once, without a question. The story keeps its save on exit, then the skirmish box shows. The
		// land changes between frames, not while the game reads the input.
		_interface->GetMenu().Close();
		_gameEndAsked = game_session::End::Skirmish;
		break;
	case Action::LeaveSkirmish:
		// Yes to the question ends the skirmish, and the skirmish box shows again. A skirmish keeps no save on exit.
		_interface->GetMenu().Close();
		_gameEndAsked = game_session::End::Skirmish;
		break;
	case Action::ExitOnlineGame:
		ExitOnlineGame();
		break;
	case Action::AskToLeave:
		AskToLeaveNetworkGame();
		break;
	case Action::Continue:
	case Action::None:
		break;
	}
}

std::unique_ptr<gui::MultiplayerFrontEnd> Game::MakeMultiplayerFrontEnd()
{
	if (_lanDiscovery == nullptr)
	{
#if defined(__EMSCRIPTEN__)
		// A browser cannot search the local network
		_lanDiscovery = std::make_unique<online::NoLanDiscovery>();
#else
		_lanDiscovery = online::MakeUdpLanDiscovery([] {
			return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
			    .count();
		});
#endif
	}
	gui::MultiplayerFrontEnd::Services services {
	    .lobby = Locator::lobbyClient::has_value() ? &Locator::lobbyClient::value() : nullptr,
	    .network = Locator::networkGame::has_value() ? &Locator::networkGame::value() : nullptr,
	    .lan = _lanDiscovery.get(),
	    .openPage =
	        [](std::string_view url) {
		        online::OpenPage(url);
#if !defined(__EMSCRIPTEN__)
		        // The game makes its window small, so that the browser's page is in front
		        if (Locator::windowing::has_value())
		        {
			        SDL_MinimizeWindow(static_cast<SDL_Window*>(Locator::windowing::value().GetHandle()));
		        }
#endif
	        },
	    .owner = {},
	    .poll = {},
	    .maps = Locator::onlineMapStore::has_value() ? &Locator::onlineMapStore::value() : nullptr,
	    .clanCreature = Locator::clanCreatureSystem::has_value() ? &Locator::clanCreatureSystem::value() : nullptr,
	};
	gui::MultiplayerFrontEnd::Settings settings;
	// The registration page is on the lobby's site: Register on the site, then log in in the game
#if defined(__EMSCRIPTEN__)
	settings.browser = true;
	settings.registerPage = "/register/";
	settings.accountPage = "/account/";
#else
	const auto origin = Locator::config::value().lobbyOrigin.value_or(std::string());
	settings.registerPage = origin.empty() ? std::string() : origin + "/register/";
	settings.accountPage = origin.empty() ? std::string() : origin + "/account/";
#endif
	// The internet connection's name in the list is the lobby's host: in the browser, the host of the page
#if defined(__EMSCRIPTEN__)
	settings.internetName = gui::ToUtf16(web::PageHost());
#else
	const auto host = origin.find("://");
	settings.internetName = gui::ToUtf16(host == std::string::npos ? origin : origin.substr(host + 3));
#endif
	if (const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile(); profile.has_value())
	{
		settings.profileName = gui::ToUtf16(profile->name);
	}
	// A game server of another version shows "(Wrong Version)" in the game list. The lobby keeps 16 characters of a
	// server's version, so the game servers report their semantic version and the list compares that.
	settings.gameVersion = std::string(build_version::Semantic());
	auto maps = gui::ReadOnlineMapTexts();
	settings.localMaps = std::move(maps.localMaps);
	settings.conditionTemplate = std::move(maps.conditionTemplate);
	settings.showPing = Locator::config::value().showPing;
	return std::make_unique<gui::MultiplayerFrontEnd>(std::move(services), std::move(settings));
}

void Game::AnswerJoinOnline()
{
	if (!_joinOnlineAsked || !_interface)
	{
		return;
	}
	const auto& director = Locator::cinematicDirectorSystem::value();
	const bool networkGame = _networkLandPending || Locator::time::value().IsNetworkGame() ||
	                         (Locator::networkGame::has_value() && Locator::networkGame::value().InGame());
	const auto decision = join_online_entry::Decide({
	    .frontEndOpen = _interface->GetMultiplayerBoxes().IsActive(),
	    .networkGame = networkGame,
	    .boxToAnswer = _interface->GetNewPlayerBox().IsActive() || _interface->GetSaveGameBoxes().IsActive() ||
	                   _interface->GetTattooEditor().IsOpen() || _gameEnding.has_value() || _gameEndAsked.has_value(),
	    .scriptHoldsCinemaBars = director.IsWideScreenOn() && director.GetWideScreenOwner() != 0,
	    .startQuestionOpen = _interface->GetSkipBox().IsActive(),
	});
	switch (decision.outcome)
	{
	case join_online_entry::Outcome::Wait:
		return;
	case join_online_entry::Outcome::Ignore:
		_joinOnlineAsked = false;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Join Online Game: the front end is open or a network game plays");
		return;
	case join_online_entry::Outcome::Open:
		break;
	}
	_joinOnlineAsked = false;
	// The start-of-game question waits behind the front end, as it does for the -MULTIPLAYER switch
	std::optional<bool> question;
	if (decision.askQuestionAfter)
	{
		const bool hasCreature = _interface->GetSkipBox().IsOffered(new_game_choice::Choice::KeepOldCreature);
		if (_interface->WithdrawSkipBox())
		{
			question = hasCreature;
		}
	}
	if (!OpenMultiplayerFrontEnd(MakeMultiplayerFrontEnd()))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The multiplayer front end needs the temple and the interface");
		if (question.has_value())
		{
			_interface->ShowSkipBox(*question);
		}
		return;
	}
	if (question.has_value())
	{
		_startQuestionAfterFrontEnd = question;
	}
	if (decision.holdLand)
	{
		_frontEndHoldsLand = true;
		Locator::time::value().SetPlayerPause(true);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Join Online Game of the page: the multiplayer front end opens{}",
	                   decision.holdLand ? ", and the cut scene stops until the player leaves it" : "");
}

bool Game::OpenFrontEndInPlaceOfMenu()
{
	if (OpenMultiplayerFrontEnd(MakeMultiplayerFrontEnd()))
	{
		return true;
	}
	SPDLOG_LOGGER_WARN(spdlog::get("game"), "The multiplayer front end needs the temple and the interface");
	// The land's start-of-game question waited for the front end, which did not open
	if (const auto hasCreature = std::exchange(_startQuestionAfterFrontEnd, std::nullopt);
	    hasCreature.has_value() && _interface)
	{
		_interface->ShowSkipBox(*hasCreature);
	}
	return false;
}

void Game::StartAfterNetworkGame()
{
	// The network game of this computer ends: the next land is a land of this computer alone. As the -MULTIPLAYER switch
	// does, the start land loads as a plain start does, and the multiplayer front end opens in place of the menu.
	if (Locator::networkGame::has_value())
	{
		Locator::networkGame::value().EndNetworkGame();
	}
	_endGameBoxState = {};
	_frontEndOnStart = true;
	ContinueGame();
	_frontEndOnStart = false;
	if (OpenFrontEndInPlaceOfMenu())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The player left the network game: the multiplayer front end opens");
	}
}

void Game::AskToLeaveNetworkGame()
{
	if (!_interface)
	{
		return;
	}
	// The end box asks the question to leave, in place of the menu
	const auto box = end_game_box::AskToLeave(_endGameBoxState);
	const auto& texts = _interface->GetTexts();
	const auto title = end_game_box::Title(
	    box.kind, false, {},
	    {.leave = texts.Get("HELP_TEXT_DIALOG_AREYOUSUREQUIT"), .lost = {}, .won = {}, .gameOver = {}, .winnerFormat = {}});
	const auto local = LocalGodForGameEnd();
	const auto landNumber = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
	const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
	const auto colour =
	    end_game_box::LooksOn(local, networkGame) ? glm::vec4(0.0f) : gui::statistics_box::PlayerColour(local, landNumber);
	_interface->OpenEndGameBox({.box = box, .title = title, .colour = colour});
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The end box asks the question to leave the network game");
}

void Game::ExitOnlineGame()
{
	// The question to leave, unless the god of the player is out of the game already: then the player leaves at once
	if (Locator::gameEndSystem::has_value())
	{
		const auto local = LocalGodForGameEnd();
		for (const auto& [god, standing] : Locator::gameEndSystem::value().Standings())
		{
			if (god == local && standing.out)
			{
				if (_interface)
				{
					_interface->GetMenu().Close();
				}
				LeaveNetworkGame();
				return;
			}
		}
	}
	AskToLeaveNetworkGame();
}

void Game::LeaveAsObserverAtTheEndOfTheRun()
{
	if (!Locator::networkGame::has_value())
	{
		return;
	}
	auto& network = Locator::networkGame::value();
	const auto* session = network.Session();
	if (session == nullptr || session->GetState() == online::NetworkSession::State::Ended ||
	    !session->GetJoined().has_value() || session->GetJoined()->role != game_socket::Role::Observer)
	{
		return;
	}
	// The update closes the socket after the leave, so the leave goes before the game stops
	network.Leave();
	network.Update();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The observer leaves the network game at the end of the run");
}

void Game::LeaveNetworkGame(game_session::NetworkLeave leave)
{
	if (!Locator::networkGame::has_value() || Locator::networkGame::value().Session() == nullptr)
	{
		return;
	}
	// The game server makes the god of the player a leaver for every computer
	Locator::networkGame::value().Leave();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The player leaves the network game");
	// A join from the command line, and a run of the tools, stop the game. A join from the front end goes to the box
	// that the control gives.
	const bool toolRun = Locator::simulation::value().GetSettings().untilTurn.has_value();
	if (_joinedFromCommandLine || toolRun)
	{
		_leaveNetworkGame = true;
		return;
	}
	_gameEndAsked = game_session::EndAfterNetworkLeave(leave);
}

void Game::LeaveAsAPlayerDoes()
{
	// The centre of the control of a name, among the controls that a box shows now
	const auto centreOf = [](const std::vector<gui::Dialog::NamedControl>& controls,
	                         std::u16string_view name) -> std::optional<glm::ivec2> {
		const auto found = std::ranges::find(controls, name, &gui::Dialog::NamedControl::name);
		if (found == controls.end())
		{
			return std::nullopt;
		}
		return (found->rect.min + found->rect.max) / 2;
	};
	// Escape opens the menu, Exit Online Game asks the question to leave, and Leave Game leaves
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tests: the player leaves the network game at turn {}",
	                   Locator::time::value().GetTurn());
	auto& menu = _interface->GetMenu();
	TellMenuTheGame();
	_interface->OpenMenu();
	const auto& texts = _interface->GetTexts();
	if (const auto exit = centreOf(menu.GetNamedControls(), texts.Get("HELP_TEXT_FRONT_END_03")); exit.has_value())
	{
		menu.MouseDown(*exit);
		if (menu.MouseUp(*exit) == gui::GameMenu::Action::ExitOnlineGame)
		{
			ExitOnlineGame();
		}
	}
	auto& endBox = _interface->GetEndGameBox();
	if (const auto leave = centreOf(endBox.GetNamedControls(), texts.Get("HELP_TEXT_DIALOG_LEAVE_GAME")); leave.has_value())
	{
		endBox.MouseDown(*leave);
		std::ignore = endBox.MouseUp(*leave);
	}
}

void Game::TellLeaves()
{
	if (!_interface || !Locator::networkGame::has_value())
	{
		return;
	}
	auto& network = Locator::networkGame::value();
	const auto* session = network.Session();
	auto gods = Locator::gameEndSystem::has_value() ? Locator::gameEndSystem::value().TakeLeavesToShow()
	                                                : std::vector<game_end::Leave>();
	auto observers = network.TakeObserversLeft();
	if (session == nullptr)
	{
		return;
	}
	const auto own = session->OwnGod();
	std::vector<std::pair<game_end::Leave, std::string>> leaves;
	for (const auto& leave : gods)
	{
		// The player of this computer does not see its own leave
		if (!leave.god.has_value() || leave.god == own)
		{
			continue;
		}
		// The start gives the names by god slot, not by player number
		std::string name;
		const auto slot = player_slots::SlotOfPlayer(*leave.god);
		if (session->GetStart().has_value() && slot.has_value() && *slot < session->GetStart()->names.size())
		{
			name = session->GetStart()->names.at(*slot);
		}
		leaves.emplace_back(leave, std::move(name));
	}
	// An observer has no god, and its god is never out. Its leave comes at once, not at a turn.
	const bool decided = Locator::gameEndSystem::has_value() && Locator::gameEndSystem::value().IsDecided();
	for (auto& name : observers)
	{
		leaves.emplace_back(game_end::Leave {.god = std::nullopt, .godOut = false, .decided = decided}, std::move(name));
	}
	for (const auto& [leave, name] : leaves)
	{
		const auto notice = game_end::NoticeOf(leave);
		if (notice.remark && Locator::guidanceSystem::has_value())
		{
			Locator::guidanceSystem::value().SayOneOff(guidance::OneOff::EnemyLeaving);
		}
		if (notice.box)
		{
			_interface->ShowPlayerLeft(gui::ToUtf16(name));
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "{} left the network game{}", name,
		                   notice.box ? ": the box tells the players" : "");
	}
}

PlayerNames Game::LocalGodForGameEnd() const
{
	// A looker-on of a network game has no god. The original game names it player seven. In a network game every god
	// takes part, and player seven can be a god, so the looker-on gets the neutral player, which is no god.
	const auto* session = Locator::networkGame::has_value() ? Locator::networkGame::value().Session() : nullptr;
	const bool looksOn =
	    session != nullptr && session->GetJoined().has_value() && session->GetJoined()->role == game_socket::Role::Observer;
	return looksOn ? PlayerNames::NEUTRAL : LocalInputPlayer();
}

bool Game::OpenMultiplayerFrontEnd(std::unique_ptr<gui::MultiplayerFrontEnd> frontEnd)
{
	if (!_interface || !Locator::temple::has_value() || frontEnd == nullptr)
	{
		return false;
	}
	Locator::temple::value().EnterMultiplayerFrontEnd();
	_interface->OpenMultiplayer(std::move(frontEnd));
	return true;
}

void Game::UpdateMultiplayerFrontEnd()
{
	if (!_interface)
	{
		return;
	}
	// The game server started the game that the player joined in the front end: the front end closes, and the server's
	// land loads with the server's seed. The network game keeps its session with the server.
	if (Locator::networkGame::has_value())
	{
		auto& network = Locator::networkGame::value();
		if (const auto start = network.TakeStart(); start.has_value())
		{
			auto& startBoxes = _interface->GetMultiplayerBoxes();
			if (startBoxes.IsActive())
			{
				startBoxes.Close();
			}
			if (Locator::temple::has_value() && Locator::temple::value().IsMultiplayerFrontEnd())
			{
				Locator::temple::value().LeaveMultiplayerFrontEnd();
			}
			// The land of the question goes: the network game's land does not ask it. A land that the front end held
			// goes too.
			const bool held = std::exchange(_frontEndHoldsLand, false);
			if (std::exchange(_startQuestionAfterFrontEnd, std::nullopt).has_value() || held)
			{
				Locator::time::value().SetPlayerPause(false);
			}
			if (StartNetworkLand(*start))
			{
				network.BeginPlay();
			}
			else
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The land of the network game did not load");
				network.Leave();
			}
			return;
		}
	}
	auto& boxes = _interface->GetMultiplayerBoxes();
	if (!boxes.IsActive())
	{
		return;
	}
	// The cut scene stays stopped while the front end is open, also when the menu closed and let the game continue
	if (_frontEndHoldsLand)
	{
		Locator::time::value().SetPlayerPause(true);
	}
	const bool templeGone = !Locator::temple::has_value() || !Locator::temple::value().IsMultiplayerFrontEnd();
	if (boxes.TakeLeft() || templeGone)
	{
		boxes.Close();
		if (Locator::temple::has_value())
		{
			Locator::temple::value().LeaveMultiplayerFrontEnd();
		}
		// The land's start-of-game question waited for the front end
		const auto hasCreature = std::exchange(_startQuestionAfterFrontEnd, std::nullopt);
		if (hasCreature.has_value())
		{
			_interface->ShowSkipBox(*hasCreature);
		}
		// The cut scene that the front end stopped continues. The question keeps the land stopped until its answer.
		if (std::exchange(_frontEndHoldsLand, false) && !hasCreature.has_value())
		{
			Locator::time::value().SetPlayerPause(false);
		}
	}
}

bool Game::StartNetworkLand(const game_socket::Start& start)
{
	const auto scripts = Locator::filesystem::value().GetPath<filesystem::Path::Scripts>();
	const auto landPath = start.testbed || start.godsRingMap ? std::optional<std::filesystem::path>(std::filesystem::path())
	                      : online::start_rules::IsLandPath(start.land) ? network_land::PathOf(start.land, scripts)
	                                                                    : std::nullopt;
	if (!landPath.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The game server gives a land that is not a land script: {}", start.land);
		return false;
	}
	// A join from the command line: the server's land, seed and start take the place of those of this computer
	if (std::exchange(_networkLandPending, false))
	{
		const auto& lands = Locator::landSystem::value();
		const auto differences = network_land::OwnValuesThatDiffer({.land = lands.GetLandPath(),
		                                                            .testbed = _startTestbed,
		                                                            .godsRingMap = _startGodsRingMap,
		                                                            .seed = Locator::rng::value().GetRunSeed(),
		                                                            .opening = _newGameStart},
		                                                           start);
		if (!differences.empty())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "The land, the seed and the start of the game server win: this computer had another {}",
			                   differences);
		}
		StartInputTools();
	}
	// The client starts the land as the game server started it: the same new-game start and the same seed, as a network
	// game. Each computer reads the date from its own clock, as in the original game. Only a run of a tool keeps its
	// pinned date. No synced value reads the date.
	_newGameStart = start.opening.empty() ? std::nullopt : new_game_choice::ParseNewGameStart(start.opening);
	auto& lands = Locator::landSystem::value();
	lands.SetNewGameStart(_newGameStart);
	auto& clock = Locator::time::value();
	const auto pinnedDate = clock.GetPinnedDate();
	clock.RestartClock(pinnedDate);
	clock.SetNetworkGame(true);
	// The time limit of the game comes from the server, so every computer decides the game at the same turn
	if (Locator::gameEndSystem::has_value())
	{
		Locator::gameEndSystem::value().SetTimeLimit(start.timeLimit.value_or(game_end::k_DefaultTimeLimitMinutes));
		// The winning conditions come from the server too. The gods get them at the first turn.
		std::vector<game_end::ConditionPick> picks;
		for (const auto& each : start.conditions)
		{
			picks.push_back(
			    {.condition = static_cast<game_end::Condition>(each.condition), .value = static_cast<int32_t>(each.value)});
		}
		Locator::gameEndSystem::value().SetConditions(std::move(picks));
	}
	// The land starts from the state that the game server started it from
	simulation::PrepareNetworkLand(start.seed);
	// The modes of the AI soak come from the server, as the seed does. A build for players refuses them.
	if (const auto refusal = simulation::ApplySoakTools({.allComputerGods = start.allComputerGods, .aiSeed = start.aiSeed});
	    refusal.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The game server asks for a game this build cannot play: {}", *refusal);
		return false;
	}
	// The testbed scenario of the tools comes from the server too. The scenarios' window lays it out before the first turn,
	// as the server does. A build for players has no such window and refuses it.
	if (start.scenario.has_value())
	{
		if (const auto refusal = developer_tools::ScenarioRefusalOf(developer_tools::k_Built); refusal.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The game server asks for a game this build cannot play: {}", *refusal);
			return false;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The network game plays the testbed scenario {}", *start.scenario);
		_scenarioRequest = ScenarioRequest {.id = *start.scenario, .hideWindow = true, .benchmark = false};
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The network game starts: seed {}, {}", start.seed,
	                   pinnedDate.has_value() ? "date pinned" : "date from the clock of the computer");
	_startTestbed = start.testbed;
	_startGodsRingMap = start.godsRingMap;
	// The server chose the land: the testbed of a server is not a developer tool of this computer
	if (start.testbed)
	{
		// A build for players has no testbed
		if (!lands.LoadTestbed())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The game server asks for a game this build cannot play: {}",
			                    "the build has no developer tools, so it has no testbed");
			return false;
		}
	}
	else if (start.godsRingMap)
	{
		// A network client does not make the map for 32 gods: it loads the file of the server, which it got from the
		// lobby (deviation 171)
		const auto* store = Locator::onlineMapStore::has_value() ? &Locator::onlineMapStore::value() : nullptr;
		const auto file = online::ReadStartLand(start.land, store, Locator::filesystem::value());
		if (!file.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "This computer does not have the map of the game server: {}", start.land);
			return false;
		}
		if (!lands.LoadGodsRingMapFile(*file))
		{
			return false;
		}
	}
	else if (!LoadMapWithFreshScripts(*landPath, land_change::Kind::NewGame))
	{
		// The server names a land in the Scripts folder of the game, and never a path out of it
		return false;
	}
	// The story's scripts run the first land, but not the testbed, the map for 32 gods, a playground or a network game
	const bool startStory = !start.testbed && !start.godsRingMap &&
	                        new_game_choice::FirstLandStartsStory(Locator::entitiesRegistry::value().Context().skirmish, true);
	if (!lands.StartChallengeScripts(startStory))
	{
		return false;
	}
	// The world starts as a new world starts to play: everything that the land made goes into the map's cells, and the
	// clock starts from the start of the current turn
	Locator::entitiesMap::value().Sync();
	// Every resource loads before the first turn, as on the game server, so that a thing that waits for its picture or
	// its model starts at the same turn on each computer
	simulation::LoadEveryResource();
	clock.Start();
	clock.ResetTimerToTurn();
	return true;
}

void Game::UpdateTattooMenu(float milliseconds)
{
	if (!Locator::tattooMenuSystem::has_value() || !_interface)
	{
		return;
	}
	auto& tattooMenu = Locator::tattooMenuSystem::value();
	auto& menu = _interface->GetMenu();
	if (menu.IsOpen())
	{
		menu.SetTattooAvailable(tattooMenu.IsButtonShown(menu.IsRestartPending()));
	}
	else if (tattooMenu.IsOpen())
	{
		// The creature goes with the menu
		tattooMenu.Close();
	}
	if (!tattooMenu.IsOpen() || !Locator::windowing::has_value())
	{
		return;
	}
	// The editor's view is the dialogs' space on the window, as the dialog is laid out
	const auto size = Locator::windowing::value().GetSize();
	const auto space = gui::FrontEndSpace(static_cast<int32_t>(size.x), static_cast<int32_t>(size.y));
	const auto origin = space.ToScreen(glm::vec2(0.0f));
	const auto corner = space.ToScreen(glm::vec2(gui::DialogPainter::k_Size));
	const auto pointer = Locator::gameActionSystem::value().GetPointerPosition();
	tattooMenu.Update(milliseconds, pointer, _interface->IsTattooEditorButtonHeld(), _interface->IsTattooEditorLeftHeld(),
	                  {.origin = origin, .size = corner - origin});
}

void Game::ShowTattooMenuCreature() const
{
	if (!Locator::tattooMenuSystem::has_value() || !Locator::rendererInterface::has_value())
	{
		return;
	}
	if (const auto shown = Locator::tattooMenuSystem::value().GetShown(); shown.has_value())
	{
		Locator::rendererInterface::value().ShowMenuCreature({.copy = shown->copy,
		                                                      .view = shown->view,
		                                                      .projection = shown->projection,
		                                                      .viewOrigin = shown->viewport.origin,
		                                                      .viewSize = shown->viewport.size});
	}
}

void Game::CloseMenuForNewLand()
{
	// The trip to the Gods' Playground closes no box. An open menu keeps its pause, and the land information box stays,
	// while the land of the Playground loads. The cinema bars of the first lesson close them.
	if (std::exchange(_boxesStayForTrip, false))
	{
		return;
	}
	// Each other change of land comes only when the menu is closed. The menu closes before the choices in it that change
	// the land. This also closes the menu for a land that the debug tools load.
	if (_interface && _interface->GetMenu().IsOpen())
	{
		_interface->GetMenu().Close();
	}
	// The end of a game closes every box of the game. The script of the next land shows the land information box
	// again when the land has one. This also closes the box for a land that the debug tools load.
	if (_interface && _interface->GetLandInfoBox().IsActive())
	{
		_interface->GetLandInfoBox().Close();
	}
	// The game gets the pause that it had before the menu opened, before the next land asks for a pause of its own
	if (const auto pause = _menuPause.Release(); pause.has_value())
	{
		Locator::time::value().SetPlayerPause(*pause);
		_menuClosedAt = std::chrono::steady_clock::now();
	}
}

void Game::RefreshPlayersPage()
{
	if (!_interface || !Locator::playerProfileSystem::has_value())
	{
		return;
	}
	const auto& profiles = Locator::playerProfileSystem::value().GetProfiles();
	std::vector<std::u16string> names;
	names.reserve(profiles.profiles.size());
	std::optional<size_t> current;
	for (const auto& profile : profiles.profiles)
	{
		if (profile.name == profiles.current)
		{
			current = names.size();
		}
		names.push_back(gui::ToUtf16(profile.name));
	}
	_interface->GetMenu().SetPlayers(std::move(names), current);
}

void Game::PlayerProfileChanged()
{
	// The help record of the new player comes from their profile. Without their file, the record stays as it is
	LoadHelpRecord();
	// The pictures of the new player replace those of the designs that they have pictures for
	ReadTattooPictures();
	RefreshPlayersPage();
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value() || !_interface)
	{
		return;
	}
	// The page shows the profile's creature's name, symbol and options, which take effect
	_interface->GetMenu().ShowPlayer(gui::ToUtf16(profile->creatureName), profile->symbol, profile->settings);
	ApplyPlayerSettings(profile->settings);
	// The bindings of the new player come from their profile
	LoadProfileKeys();
	NameCreatureAfterProfile();
}

void Game::ReadTattooPictures()
{
	// The symbols' picture and the tattoo designs change with the pictures. The game loads them one time only
	// (vanilla-bugs 182), and openblack fixes this.
	if (const auto pictures = simulation::ReadTattooPictures(); pictures.has_value() && _interface)
	{
		_interface->SetSymbolPictures(*pictures);
	}
}

void Game::MakePlayersSymbols(std::optional<uint32_t> symbolInPlace)
{
	game_assets::MakePlayersSymbols(symbolInPlace);
}

void Game::NameCreatureAfterProfile()
{
	// In a game alone, the player's creature takes the name its profile gives it
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value() || !Locator::playerSystem::has_value() ||
	    !creature_names::ProfileRenamesCreature(Locator::time::value().IsNetworkGame()))
	{
		return;
	}
	const auto creature = Locator::playerSystem::value().GetPrimaryCreature(PlayerNames::PLAYER_ONE);
	auto& registry = Locator::entitiesRegistry::value();
	if (!creature.has_value() || !registry.Valid(*creature))
	{
		return;
	}
	if (auto* component = registry.TryGet<ecs::components::Creature>(*creature); component != nullptr)
	{
		component->name = gui::ToUtf16(profile->creatureName);
	}
}

void Game::AskForAnotherPlayer(bool canCancel)
{
	auto& profiles = Locator::playerProfileSystem::value();
	SDL_StartTextInput();
	_interface->ShowNewPlayerBoxOverGame(canCancel && profiles.GetProfileCount() > 0,
	                                     [&profiles](std::u16string_view name) { return profiles.CheckNewName(name); });
}

void Game::EndGame(game_session::End end)
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game ends: {}",
	                   end == game_session::End::NewGame    ? "a new game starts"
	                   : end == game_session::End::Online   ? "the player left the network game"
	                   : end == game_session::End::Skirmish ? "the skirmish box shows"
	                                                        : "the game continues as the profile picked");
	SaveAsGameEnds(end, false);
	// The next game has no previous profile until the Players page opens again
	if (_interface)
	{
		_interface->GetMenu().ForgetPreviousProfile();
	}
	_gameEnding = end;
}

void Game::StartNextGameWhenSaved()
{
	// The save on exit takes the picture of the last frame of the game that ended. The next game starts after it.
	if (!_gameEnding.has_value() ||
	    (Locator::saveGameSystem::has_value() && Locator::saveGameSystem::value().AnyPictureWaiting()))
	{
		return;
	}
	const auto end = *std::exchange(_gameEnding, std::nullopt);
	auto& profiles = Locator::playerProfileSystem::value();
	if (const auto picked = std::exchange(_profileAfterSave, std::nullopt); picked.has_value())
	{
		profiles.SelectProfile(*picked);
	}
	const auto profile = profiles.GetCurrentProfile();
	if (profile.has_value() && _interface)
	{
		_interface->SetPlayerProfile(gui::ToUtf16(profile->name), gui::ToUtf16(profile->creatureName), profile->symbol,
		                             profile->settings);
		RefreshPlayersPage();
	}
	switch (game_session::NextStart(end).value_or(game_session::Start::NewGame))
	{
	case game_session::Start::Continue:
		ContinueFromExitSave();
		break;
	case game_session::Start::NewGame:
		if (!StartNewGame(std::nullopt))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The new game could not start");
		}
		break;
	case game_session::Start::Online:
		StartAfterNetworkGame();
		break;
	case game_session::Start::Skirmish:
		// A player who left a network game plays alone again: the skirmish box starts a game of this computer alone
		if (Locator::networkGame::has_value())
		{
			Locator::networkGame::value().EndNetworkGame();
		}
		OpenSkirmishBoxOverGame();
		break;
	case game_session::Start::SavedGame:
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "This start of a game is not available yet");
		break;
	}
}

game_session::ExitSave Game::ExitSaveAsGameEnds(game_session::End end) const
{
	const auto& context = Locator::entitiesRegistry::value().Context();
	game_session::ExitSaveFacts facts {
	    .end = end,
	    .gameOver = context.gameOver,
	    .multiplayer = Locator::time::value().IsNetworkGame(),
	    .skirmish = context.skirmish,
	    .godsPlayground = context.mapScriptGlobals.landNumber == escape_key::k_GodsPlaygroundLand,
	    .previousProfile = std::nullopt,
	    .currentProfile = {},
	    .previousExists = false,
	};
	if (_interface)
	{
		facts.previousProfile = _interface->GetMenu().GetPreviousProfile();
	}
	if (Locator::playerProfileSystem::has_value())
	{
		const auto& profiles = Locator::playerProfileSystem::value();
		if (const auto profile = profiles.GetCurrentProfile(); profile.has_value())
		{
			facts.currentProfile = gui::ToUtf16(profile->name);
		}
		facts.previousExists = facts.previousProfile.has_value() &&
		                       player_profiles::Find(profiles.GetProfiles(), *facts.previousProfile) != nullptr;
	}
	return game_session::ExitSaveFor(facts);
}

bool Game::SaveAsGameEnds(game_session::End end, bool quitting)
{
	if (!Locator::saveGameSystem::has_value())
	{
		return false;
	}
	auto& saves = Locator::saveGameSystem::value();
	switch (ExitSaveAsGameEnds(end))
	{
	case game_session::ExitSave::None:
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game ends with no save on exit");
		return false;
	case game_session::ExitSave::Current:
		return saves.SaveOnExit(quitting);
	case game_session::ExitSave::Previous:
		break;
	}
	// The save goes to the profile that the player played before they picked another one. The game plays as that
	// profile while it saves, and then plays as the profile picked again.
	auto& profiles = Locator::playerProfileSystem::value();
	const auto picked = profiles.GetCurrentProfile();
	const auto previous = _interface->GetMenu().GetPreviousProfile();
	if (!picked.has_value() || !previous.has_value() || !profiles.SelectProfile(*previous))
	{
		return false;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The save on exit goes to the profile played before");
	const bool saved = saves.SaveOnExit(quitting);
	if (quitting)
	{
		profiles.SelectProfile(gui::ToUtf16(picked->name));
	}
	else
	{
		// The picture of the save comes back after the next frame, into the folder of the previous profile
		_profileAfterSave = gui::ToUtf16(picked->name);
	}
	return saved;
}

void Game::TellMenuTheGame()
{
	if (!_interface)
	{
		return;
	}
	auto& menu = _interface->GetMenu();
	menu.SetNetworkGame(InNetworkGame());
	// The land of a network game also plays as a skirmish, but its menu is the menu of a network game
	menu.SetSkirmish(Locator::entitiesRegistry::value().Context().skirmish && !Locator::time::value().IsNetworkGame());
}

bool Game::ContinueGame()
{
	auto& saves = Locator::saveGameSystem::value();
	if (saves.HasExitSave() && saves.Load(gamesave::k_ExitSlot))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game continues from the save on exit");
		return true;
	}
	// With no save on exit, or a load that did not happen, a new game starts on the first land
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "No save on exit loaded: a new game starts");
	if (!StartNewGame(_newGameStart))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The new game could not start");
	}
	return false;
}

bool Game::ContinueFromExitSave() noexcept
{
	if (!ContinueGame())
	{
		return false;
	}
	ShowMenuOnStart();
	return true;
}

void Game::ShowMenuOnStart()
{
	if (!_interface)
	{
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The menu shows over the game that continued");
	TellMenuTheGame();
	_interface->OpenMenu();
	// The menu reads the profile's own pictures again as it opens
	ReadTattooPictures();
	// The menu pauses the game while it is open
	HandleInterfaceAction();
}

void Game::ApplyPlayerSettings(const player_profiles::Settings& settings)
{
	simulation::ApplyPlayerSettings(settings);
	if (_interface)
	{
		constexpr int k_Levels = 4;
		_interface->GetToolTips().SetLevel(static_cast<gui::ToolTipLevel>(std::clamp(settings.toolTips, 0, k_Levels - 1)));
	}
}

void Game::LoadLandscape(const std::filesystem::path& path)
{
	Locator::landSystem::value().LoadLandscape(path);
}

void Game::SetTime(float time) noexcept
{
	Locator::skyClockSystem::value().SetTime(time);
}
