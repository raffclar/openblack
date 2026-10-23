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
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LNDFile.h>
#include <MorphFile.h>
#include <PackFile.h>
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
#include "3D/GripLandscapeEffect.h"
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
#include "3D/PlayersSymbols.h"
#include "3D/SeaTexture.h"
#include "3D/SkyDome.h"
#include "3D/SnowCover.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleRoomMeshes.h"
#include "3D/WaterRings.h"
#include "Audio/AtmosAudio.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/ClipSounds.h"
#include "Audio/GameMusic.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/HelpSpeech.h"
#include "Audio/ScriptSoundEffect.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/DoubleClickFlight.h"
#include "Camera/FightCameraModel.h"
#include "Camera/NearClipping.h"
#include "Camera/ScriptCameraModel.h"
#include "Common/BufferedLogSink.h"
#include "Common/BuildVersion.h"
#include "Common/EventManager.h"
#include "Common/FatalMessage.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Common/RandomNumberManager.h"
#include "Common/StringUtils.h"
#include "Common/UserData.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureFightHud.h"
#include "Creature/CreatureHandRules.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"
#include "Creature/LeashOrders.h"
#include "Creature/TattooPictures.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/FightAutopilotInterface.h"
#include "Debug/FrameStatsLog.h"
#include "Debug/TestbedDispenserGrid.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Archetypes/SkyArchetype.h"
#include "ECS/ChallengePictures.h"
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
#include "ECS/Components/HandMorph.h"
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
#include "ECS/FloatingNumber.h"
#include "ECS/LearningMarker.h"
#include "ECS/Map.h"
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
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
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
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
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
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/PauseTextSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/RainSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RealWeatherSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SharkSystemInterface.h"
#include "ECS/Systems/SimulationSystemInterface.h"
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
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TownDesireFlags.h"
#include "ECS/TreeRoots.h"
#include "ECS/VillageTotem.h"
#include "ECS/VillagerLabelSource.h"
#include "ECS/WorldObjects.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gestures/GestureTrailBuilder.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/TexelDepth.h"
#include "Gui/Canvas.h"
#include "Gui/DialogPainter.h"
#include "Gui/EscapeKeyRules.h"
#include "Gui/GameInterface.h"
#include "Gui/HandToolTips.h"
#include "Gui/LoadingScreen.h"
#include "Gui/MultiplayerFrontEnd.h"
#include "Gui/OnlineMapTexts.h"
#include "Gui/ScreenFit.h"
#include "Gui/StartupRules.h"
#include "Gui/WorldToolTips.h"
#include "Hand/HandBlend.h"
#include "Hand/HandFeel.h"
#include "Hand/HandGrabRules.h"
#include "Hand/HandReach.h"
#include "Hand/HandTrace.h"
#include "Hand/HandVisibility.h"
#include "Hand/ScriptHandState.h"
#include "Help/HelpProfile.h"
#include "Help/SingAlong.h"
#include "Help/Spirits.h"
#include "Help/TurnCheer.h"
#include "Input/GameActionMapInterface.h"
#include "Input/KeyRebinding.h"
#include "Input/PlatformKeys.h"
#include "LHScriptX/MapScript.h"
#include "LHScriptX/Script.h"
#include "LivingTurnOrder.h"
#include "Locator.h"
#include "Online/HttpClientInterface.h"
#include "Online/LanDiscoveryInterface.h"
#include "Online/OpenPage.h"
#include "Online/WebSocketInterface.h"
#include "Parsers/InfoFile.h"
#include "Particles/LiquidParticles.h"
#include "Physics/Materials.h"
#include "Profiler.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptInterfaceLevels.h"
#include "Serializer/FotFile.h"
#include "Simulation/CommandLog.h"
#include "Simulation/FrameBody.h"
#include "Simulation/ProfileMenu.h"
#include "Simulation/TurnBody.h"
#include "Simulation/WorldStart.h"
#include "Temple/TempleHelp.h"
#include "Video/VideoRules.h"

#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif

using namespace openblack;
using namespace openblack::lhscriptx;
using namespace std::chrono_literals;

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

/// Notes in the hand's debugging trace that a part of the frame put the hand at a position (see hand_trace::HandTrace)
void TraceHand(std::string_view writer, const glm::vec3& position)
{
	if (auto* trace = Locator::handSystem::value().GetTrace(); trace != nullptr)
	{
		trace->Note(writer, position);
	}
}
/// The hand's splash, its sound and the scare it gives the fish are this high over the sea
constexpr float k_HandSplashHeight = 0.2f;

/// The meshes the hand is pulled towards as its player turns evil or good, the first two of its morph file's variants,
/// and the files of the base and those two for their skins. Black & White would also blend the hand's bones and
/// animations towards theirs, but its own meshes share the base's bones and the morph file has no animations for
/// them, so that changes nothing; a warning says when other data would.
void LoadHandLooks(const morph::MorphFile& morphFile)
{
	using ecs::components::HandMorph;
	auto& fileSystem = Locator::filesystem::value();
	auto& resources = Locator::resources::value();
	auto& meshes = resources.GetMeshes();
	auto& files = resources.GetL3DFiles();
	const auto& header = morphFile.GetHeader();
	const auto pathOf = [&fileSystem](const std::array<char, 0x20>& name) {
		return fileSystem.GetPath<filesystem::Path::CreatureMesh>() / (std::string(name.data()) + ".l3d");
	};
	const auto load = [&files](entt::id_type id, const std::filesystem::path& path) {
		try
		{
			files.Load(id, resources::L3DFileLoader::FromDiskTag {}, path);
		}
		catch (std::exception& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't read the hand's skins from {}: {}", path.string(), err.what());
		}
	};
	load(HandMorph::k_SkinFileIds[0], pathOf(header.baseMeshName));
	const auto base = meshes.Handle(ecs::components::Hand::k_MeshId);
	for (size_t look = 0; look < HandMorph::k_LookMeshIds.size(); ++look)
	{
		const auto& name = header.variantMeshNames.at(look);
		if (name[0] == '\0')
		{
			continue;
		}
		const auto path = pathOf(name);
		try
		{
			meshes.Load(HandMorph::k_LookMeshIds.at(look), resources::L3DLoader::FromDiskTag {}, path);
		}
		catch (std::exception& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't load the hand mesh {}: {}", path.string(), err.what());
			continue;
		}
		load(HandMorph::k_SkinFileIds.at(look + 1), path);
		const auto variant = meshes.Handle(HandMorph::k_LookMeshIds.at(look));
		if (base && variant->GetBoneMatrices() != base->GetBoneMatrices())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The bones of {} differ from the hand's: they are not blended",
			                   path.string());
		}
		if (!morphFile.GetVariantAnimationSet(static_cast<uint32_t>(look)).empty())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The animations of {} in the hand's morph file are not blended",
			                   path.string());
		}
	}
}

/// Whether the hand turns to the face of a model it rests on: not a tree's, a dead tree's, a totem statue's, a seed's or
/// a shield's, whose faces leave the hand turned to the land
bool FeelsModel(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<ecs::components::Tree, ecs::components::DeadTree, ecs::components::SpellSeed,
	                   ecs::components::MagicShield>(object))
	{
		return false;
	}
	const auto* info = ecs::world_objects::InfoOf(object);
	return info == nullptr || info->type != ObjectType::TotemStatue;
}

/// The creature a script's camera is about, any player's: the thing it looks at, else the one it follows, when that is
/// a creature
std::optional<entt::entity> ScriptCameraCreature()
{
	if (!Locator::scriptControlSystem::has_value() || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto* camera = Locator::scriptControlSystem::value().GetScriptCamera(Locator::camera::value());
	const auto thing = camera != nullptr ? camera->GetFocusedThing() : std::nullopt;
	if (!thing.has_value())
	{
		return std::nullopt;
	}
	const auto entity = static_cast<entt::entity>(*thing);
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) && registry.AllOf<ecs::components::Creature>(entity) ? std::optional(entity) : std::nullopt;
}

/// Whether the hand is within the local player's influence, as what it offers and does asks it: at the point picked,
/// which is the position of the thing under the cursor, or the land under it with nothing there (anywhere, with neither).
/// While the hand keeps some of the player's influence past the border it counts as inside however far out it is.
bool HandInInfluence()
{
	// One answer for what the hand offers and what it does
	if (Locator::handGrabSystem::has_value())
	{
		return Locator::handGrabSystem::value().IsInInfluence();
	}
	const auto point = Locator::pickingSystem::has_value() ? Locator::pickingSystem::value().GetPick().point : std::nullopt;
	return !point.has_value() || Locator::influenceSystem::value().IsHandInInfluence(PlayerNames::PLAYER_ONE, *point);
}

/// Whether the hand points at the water where the fish of a fish farm show, with the hand in the local player's
/// influence: the hand can take fish there, though the farm has no model to point at
bool HandOverFish()
{
	const auto point = Locator::pickingSystem::has_value() ? Locator::pickingSystem::value().GetPick().point : std::nullopt;
	if (!point.has_value() || !Locator::fishFarmSystem::has_value() || !HandInInfluence())
	{
		return false;
	}
	// Water is a cell of the land with water in it, or a place off the land
	const auto cell = map_coords::CellOf(*point);
	if (map_coords::InBounds(cell) && Locator::terrainSystem::has_value())
	{
		const auto* land = Locator::terrainSystem::value().FindCell(glm::u16vec2(cell));
		if (land != nullptr && land->properties.hasWater == 0)
		{
			return false;
		}
	}
	return Locator::fishFarmSystem::value().FarmWithFishAt({point->x, point->z}).has_value();
}

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

/// How the game draws its scene into the window, from its camera
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

} // namespace

const std::string k_WindowTitle = "openblack";

Game* Game::sInstance = nullptr;

/// The state that the frames of the game loop keep from one frame to the next
struct Game::FrameLoop
{
	std::chrono::high_resolution_clock::time_point lastTime;
	std::optional<debug::FrameStatsLog> frameStats;
	std::chrono::steady_clock::time_point frameStart;
};

Game::Game(Arguments&& args) noexcept
    : _gamePath(args.gamePath)
    , _startMap(args.startLevel)
    , _playVideo(args.playVideo)
    , _preIntro(args.preIntro || args.launchSwitches.preIntroVideo)
    , _skipLogos(args.skipLogos)
    , _logoEffectAsked(args.logoEffect)
    , _newGameStart(args.newGameStart)
    , _newGameAsked(args.newGame || launch_switches::FirstGameOf(args.launchSwitches) == launch_switches::FirstGame::NewGame)
    , _continueAsked(args.continueGame)
    , _launchSwitches(args.launchSwitches)
    , _startTestbed(args.startTestbed || args.scenario.has_value())
    , _scenarioRequest(args.scenario)
    , _inspectPort(args.inspectPort)
    , _fightAutopilot(args.fightAutopilot)
    , _fightTranscriptFolder(args.fightTranscriptFolder)
    , _simulation(args.simulation)
    , _screenshotRoot(args.screenshotRoot)
    , _seed(args.seed)
    , _networkGame(args.networkGame)
    , _inspectInputLock(args.inspectInputLock)
    , _testbedWindow(!args.scenario.has_value() || !args.scenario->hideWindow)
    , _requestScreenshot(args.requestScreenshot)
    , _profileFolder(args.profileFolder)
    // Any of these is a developer's or a tool's run; the game started plainly, or with only its window's size, mode,
    // backend, detail and logging chosen, is the player's
    , _automatedRun(args.startLevel != k_DefaultStartLevel || args.startTestbed || args.scenario.has_value() ||
                    args.newGameStart.has_value() || args.skipLogos || args.inspectPort.has_value() || args.seed.has_value() ||
                    !args.playVideo.empty() || args.numFramesToSimulate > 0 || args.requestScreenshot.has_value())
{
	Locator::camera::emplace(glm::zero<glm::vec3>());
	// The log of each part of the game
	simulation::StartLogs(args.logFile, args.logLevels);
	sInstance = this;

	auto& config = Locator::config::emplace();
	config.numFramesToSimulate = args.numFramesToSimulate;
	config.frameStatsInterval = args.frameStatsInterval;
	config.frameStatsViews = args.frameStatsViews;
	config.frameStatsOut = args.frameStatsOut;
	config.warmUpFirstFrame = args.warmUpFirstFrame;
	config.lobbyOrigin = args.lobbyOrigin;
	config.useInternetWeather = args.useInternetWeather;
	config.dynamicTreeShadows = args.dynamicTreeShadows;
	config.unlockedBonusCreatures = args.unlockedBonusCreatures;
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
	if (Locator::saveGameSystem::has_value())
	{
		Locator::saveGameSystem::value().SetShowBox({});
		Locator::saveGameSystem::value().SetReportProblem({});
	}
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
	ShutDownServices();
	SDL_Quit(); // todo: move to GameWindow
	spdlog::shutdown();
}

bool Game::ProcessEvents(const SDL_Event& event) noexcept
{
	static bool leftMouseButton = false;
	static bool middleMouseButton = false;
	static bool rightMouseButton = false;

	// Pressed and let go, or as the mouse moving finds them: a press or a let go the menu or the debug windows took would
	// otherwise leave the hand gripping, as on leaving the temple
	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_LEFT)
	{
		leftMouseButton = event.type == SDL_MOUSEBUTTONDOWN;
	}
	// A press of the left button may click the dialogue on, whatever else it does
	if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
	    !Locator::debugGui::value().IsMouseOverWindow())
	{
		_dialogueClick = true;
	}
	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_MIDDLE)
	{
		middleMouseButton = event.type == SDL_MOUSEBUTTONDOWN;
	}
	const bool rightLetGo =
	    rightMouseButton && ((event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) ||
	                         (event.type == SDL_MOUSEMOTION && (event.motion.state & SDL_BUTTON_RMASK) == 0));
	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_RIGHT)
	{
		rightMouseButton = event.type == SDL_MOUSEBUTTONDOWN;
	}
	if (event.type == SDL_MOUSEMOTION)
	{
		leftMouseButton = (event.motion.state & SDL_BUTTON_LMASK) != 0;
		middleMouseButton = (event.motion.state & SDL_BUTTON_MMASK) != 0;
		rightMouseButton = (event.motion.state & SDL_BUTTON_RMASK) != 0;
	}

	// The hand grips the land, which the temple has none of: its camera takes the clicks
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	// While a script's cinema bars have taken the interface, the buttons' presses don't reach the hand
	const bool handTakesPresses = hand_visibility::TakesPresses(Locator::cinematicDirectorSystem::value().IsInterfaceActive());
	// Holding the right button on a creature holds the hand to it to stroke or slap it; clicking it puts the leash on.
	// While the player's creature fights, clicks on the creatures and the arena's ground direct the fight instead.
	auto& creatureHand = Locator::creatureHandSystem::value();
	auto& fights = Locator::creatureFightSystem::value();
	auto& magic = Locator::magicSystem::value();
	// The hand holds one thing at a time: a thing it picked up, or a miracle
	auto* handGrab = Locator::handGrabSystem::has_value() ? &Locator::handGrabSystem::value() : nullptr;
	const bool handHoldsThing = handGrab != nullptr && handGrab->IsBusy();
	// The interface level of a lesson can stop the Action button from taking hold of creatures and totems. It can also
	// stop an empty hand from acting on anything but a highlight that a script made.
	ecs::components::InterfaceLimits interfaceLimits {};
	if (Locator::playerSystem::has_value())
	{
		const auto player = Locator::playerSystem::value().GetPlayer(PlayerNames::PLAYER_ONE);
		const auto& registry = Locator::entitiesRegistry::value();
		if (const auto* limits =
		        registry.Valid(player) ? registry.TryGet<const ecs::components::InterfaceLimits>(player) : nullptr;
		    limits != nullptr)
		{
			interfaceLimits = *limits;
		}
	}
	const bool mayHold = script::interface_levels::MayHold(interfaceLimits);
	// Letting go of the Action button lets go of the tip bubble
	if (Locator::tipBubbleSystem::has_value() && Locator::tipBubbleSystem::value().IsGripped() &&
	    (rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)))
	{
		Locator::tipBubbleSystem::value().LetGo();
	}
	// Letting go of the Action button lets go of a town's totem, leaving it where it was slid
	if (Locator::villageTotemSystem::has_value() && Locator::villageTotemSystem::value().GetGripped().has_value() &&
	    (rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)))
	{
		Locator::villageTotemSystem::value().LetGo();
		_totemPointer.reset();
	}
	// Letting go of the Action button lets go of what the hand was taking, or puts down or throws what it holds
	if (handGrab != nullptr && (rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)))
	{
		// TODO(hand): a press too short to take the thing taps it
		[[maybe_unused]] const auto tapped = handGrab->Release(machine_clock::Ticks(), Locator::time::value().GetTurn());
		// Let go with an empty hand that did nothing with the press, it clicks the thing or the place under it, for the
		// scripts; not while the game is paused
		if (handTakesPresses && !_actionPressTaken && !handHoldsThing && !magic.IsHandBusy() && !inTemple && !IsPaused())
		{
			handGrab->ClickReleased(Locator::time::value().GetTurn());
		}
	}
	// The action button (the right) casts the miracle in the hand, which comes before the creatures: pressed, it arms,
	// locks on or casts it, and let go it throws an armed one or lets a locked one go. Without a miracle it takes hold
	// of the creature under the hand; with the leash on, pressed on another creature it ties the leash to that one
	// instead (see the leash's input).
	if (rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT))
	{
		// Letting go of the rope of the tied leash: the leash's length becomes how far the hand is from what it is tied to,
		// and the creature goes to the place under the hand
		if (Locator::leashSystem::value().RopeHand(PlayerNames::PLAYER_ONE) == leash_snap::Hand::Holding &&
		    Locator::pickingSystem::has_value())
		{
			Locator::leashSystem::value().LetGoOfRope(PlayerNames::PLAYER_ONE, Locator::pickingSystem::value().GetPick().point);
		}
		magic.ReleaseAction();
		// Let go quickly on a fireball, it is tapped
		magic.ReleaseFireBall(machine_clock::Ticks(), Locator::time::value().GetTurn());
	}
	if (handTakesPresses && !inTemple && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT &&
	    !Locator::debugGui::value().IsMouseOverWindow())
	{
		_actionPressTaken = magic.IsHandBusy() && magic.PressAction();
		// While the camera shows a duel of the player's creature and the hand is empty, every Action press goes to the
		// fight and to nothing else
		const bool fightTakesPress = fights.DirectsFight();
		// Holding a thing, the hand makes ready to throw it
		if (!_actionPressTaken && handHoldsThing)
		{
			_actionPressTaken = handGrab->Press(machine_clock::Ticks(), Locator::time::value().GetTurn());
		}
		// With an empty hand and the leash held, a quick second press near the first is a double tap, which ties the leash
		// to the thing under the cursor or unties it, before anything else takes it
		if (!_actionPressTaken && !handHoldsThing && !magic.IsHandBusy())
		{
			const auto screenSize = Locator::windowing::value().GetSize();
			const auto cursor = glm::vec2(event.button.x, event.button.y);
			glm::vec3 rayOrigin;
			glm::vec3 rayDirection;
			Locator::camera::value().DeprojectScreenToWorld(
			    cursor / static_cast<glm::vec2>(glm::max(screenSize, glm::ivec2(1))), rayOrigin, rayDirection);
			_actionPressTaken = Locator::leashSystem::value().PressAction(PlayerNames::PLAYER_ONE, rayOrigin, rayDirection,
			                                                              cursor, machine_clock::Ticks());
		}
		// With an empty hand on the tip bubble, the hand grips it and the words follow the hand until the button is let go
		if (!_actionPressTaken && !fightTakesPress && !handHoldsThing && !magic.IsHandBusy() &&
		    Locator::tipBubbleSystem::has_value() && Locator::tipBubbleSystem::value().IsUnderHand())
		{
			Locator::tipBubbleSystem::value().Grip();
			_actionPressTaken = true;
		}
		// With an empty hand over the rope of the tied leash, the hand takes hold of the rope
		if (!_actionPressTaken && !fightTakesPress && !handHoldsThing && !magic.IsHandBusy() &&
		    Locator::leashSystem::value().PressRope(PlayerNames::PLAYER_ONE))
		{
			_actionPressTaken = true;
		}
		// A lesson can let an empty hand act only on a highlight that a script made. This limit also stops a press on a
		// fireball, which is not a highlight.
		const auto picked =
		    Locator::pickingSystem::has_value() ? Locator::pickingSystem::value().GetPick().object : std::nullopt;
		const bool highlightUnderHand = picked.has_value() && Locator::entitiesRegistry::value().Valid(*picked) &&
		                                Locator::entitiesRegistry::value().AllOf<ecs::components::ScriptHighlight>(*picked);
		const bool mayPress = handHoldsThing || script::interface_levels::MayPress(interfaceLimits, highlightUnderHand);
		// With an empty hand, a fireball under it takes the press. A one-shot globe is taken hold of as any thing is.
		if (!_actionPressTaken && !fightTakesPress && !handHoldsThing && !magic.IsHandBusy() && mayPress)
		{
			_actionPressTaken = magic.PressFireBall(machine_clock::Ticks(), Locator::time::value().GetTurn());
		}
		if (!_actionPressTaken)
		{
			const auto screenSize = Locator::windowing::value().GetSize();
			glm::vec3 rayOrigin;
			glm::vec3 rayDirection;
			Locator::camera::value().DeprojectScreenToWorld(glm::vec2(event.button.x, event.button.y) /
			                                                    static_cast<glm::vec2>(glm::max(screenSize, glm::ivec2(1))),
			                                                rayOrigin, rayDirection);
			// While the player's creature duels, the Action button adds a move to its queue
			if (fights.Press(rayOrigin, rayDirection, creature_fight::Button::Action, machine_clock::Ticks(),
			                 Locator::time::value().GetTurn()))
			{
				_actionPressTaken = true;
				_fightButton = creature_fight::Button::Action;
			}
			// A press that the fight refuses does nothing: the hand does not take hold of a creature or a thing then
			else if (fightTakesPress)
			{
				_actionPressTaken = true;
			}
			const auto& leashes = Locator::leashSystem::value();
			const auto own = leashes.PlayersCreature(PlayerNames::PLAYER_ONE);
			const auto under = creatureHand.CreatureUnderCursor();
			const bool tying =
			    under.has_value() && own.has_value() &&
			    creature_hand::PressOn(*under == *own, leashes.IsLeashed(*own)) == creature_hand::CreaturePress::Leash;
			if (!_actionPressTaken && under.has_value() && !tying && mayHold)
			{
				_actionPressTaken = true;
				if (handGrab != nullptr)
				{
					handGrab->ClickThing(*under, Locator::time::value().GetTurn());
				}
				if (!creatureHand.Grab())
				{
					// A creature the hand may not hold: a click on it still asks the leash, which says why not
					Locator::leashSystem::value().TapCreature(PlayerNames::PLAYER_ONE, *under);
				}
			}
		}
		// A town's totem under the hand is taken hold of, to slide it up and down
		if (!_actionPressTaken && !magic.IsHandBusy() && !handHoldsThing && mayHold &&
		    Locator::villageTotemSystem::has_value() && Locator::pickingSystem::has_value())
		{
			auto& totems = Locator::villageTotemSystem::value();
			if (picked.has_value())
			{
				if (const auto totem = totems.TotemOf(*picked);
				    totem.has_value() && totems.Grip(*totem, PlayerNames::PLAYER_ONE))
				{
					_actionPressTaken = true;
					_totemPointer = _mousePosition;
				}
			}
		}
		// Otherwise the hand takes hold of a thing under it, if the lesson lets it
		if (!_actionPressTaken && !magic.IsHandBusy() && handGrab != nullptr && mayPress)
		{
			_actionPressTaken = handGrab->Press(machine_clock::Ticks(), Locator::time::value().GetTurn());
		}
		// A press that the lesson refuses does nothing more: the leash gives no order and taps no post
		if (!_actionPressTaken && !mayPress)
		{
			_actionPressTaken = true;
		}
	}
	if (handTakesPresses && !magic.IsHandBusy() && !inTemple && event.type == SDL_MOUSEBUTTONDOWN &&
	    event.button.button == SDL_BUTTON_LEFT && !middleMouseButton)
	{
		const auto screenSize = Locator::windowing::value().GetSize();
		glm::vec3 rayOrigin;
		glm::vec3 rayDirection;
		Locator::camera::value().DeprojectScreenToWorld(glm::vec2(event.button.x, event.button.y) /
		                                                    static_cast<glm::vec2>(glm::max(screenSize, glm::ivec2(1))),
		                                                rayOrigin, rayDirection);
		// The second press of a double click on a creature, anyone's, locks the camera onto it
		const bool doubleClicked =
		    Locator::creatureModeSystem::has_value() &&
		    Locator::creatureModeSystem::value().Press({.milliseconds = event.button.timestamp,
		                                                .screen = glm::vec2(event.button.x, event.button.y),
		                                                .creature = creatureHand.CreatureUnderCursor()});
		// While the player's creature duels, the Move button makes a move at once in place of those queued
		if (!doubleClicked && fights.Press(rayOrigin, rayDirection, creature_fight::Button::Move, machine_clock::Ticks(),
		                                   Locator::time::value().GetTurn()))
		{
			_fightButton = creature_fight::Button::Move;
		}
		else if (!doubleClicked)
		{
			PlayHandGrabSound();
		}
	}
	// Letting go of the button that pressed charges the blow it queued
	if (_fightButton.has_value() && fights.IsPressed() &&
	    ((*_fightButton == creature_fight::Button::Move && !leftMouseButton) ||
	     (*_fightButton == creature_fight::Button::Action && !rightMouseButton)))
	{
		fights.Release(machine_clock::Ticks(), Locator::time::value().GetTurn());
		_fightButton.reset();
	}
	// Letting go of the right button lets go of the creature, also of a hold that a command started. Let go quickly,
	// having neither stroked nor slapped it, the press was a click, which puts the leash on the player's creature.
	if (creature_hand::LetsGoOfCreature(rightLetGo,
	                                    event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) &&
	    creatureHand.GetCreature().has_value())
	{
		const auto creature = *creatureHand.GetCreature();
		// The click puts the leash on, and then the creature feels how the hand treated it. Both are commands of the
		// player, which the world does at the start of the next turn.
		if (creatureHand.IsClick())
		{
			Locator::playerCommandSystem::value().Submit(player_commands::LeashTap {.creature = creature});
		}
		creatureHand.Release();
	}
	const bool onCreature = creatureHand.GetCreature().has_value();

	// A miracle in the hand doesn't stop the hand moving the land; the middle button, or both buttons, turn the camera
	_handGripping = !inTemple && (middleMouseButton || (leftMouseButton && !onCreature && !fights.IsPressed()));
	_handRotating = !inTemple && (middleMouseButton || (leftMouseButton && rightMouseButton));
	_handSelectGrip = !inTemple && leftMouseButton && !onCreature && !fights.IsPressed();
	_handOtherButton = rightMouseButton;

	auto& window = Locator::windowing::value();
	auto& camera = Locator::camera::value();

	switch (event.type)
	{
	case SDL_QUIT:
		_closeAsked = true;
		break;
	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == window.GetID())
		{
			_closeAsked = true;
		}
		// A window resized, maximised or restored is followed by the next frame drawn (FollowWindowSize)
		break;
	case SDL_KEYDOWN:
		switch (event.key.keysym.sym)
		{
		case SDLK_ESCAPE:
			// Not while a script has the cinema bars in for its scene
			if (!Locator::cinematicDirectorSystem::value().IsInterfaceActive())
			{
				break;
			}
			return false;
		case SDLK_f:
			if (Locator::config::value().background)
			{
				break;
			}
			window.SetDisplayMode(windowing::DisplayMode::Fullscreen);
			break;
		case SDLK_p:
			Locator::time::value().SetPlayerPause(!IsPaused());
			break;
		default:
		{
			namespace keys = input::platform_keys;
			const auto key = event.key.keysym.sym;
			const auto mod = event.key.keysym.mod;
			const keys::Modifiers held {
			    .shift = (mod & KMOD_SHIFT) != 0,
			    .ctrl = (mod & KMOD_CTRL) != 0,
			    .alt = (mod & KMOD_ALT) != 0,
			};
			// F1 is the game's Help key, so the renderer's statistics are on another key
			if (key == keys::RendererStatisticsKey(k_Platform))
			{
				Locator::rendererInterface::value().SetDebug(!Locator::rendererInterface::value().GetDebug());
				break;
			}
			// The slower key halves the game's speed, to half of normal at the least. The faster key doubles it, to
			// twice normal at the most.
			if (const auto speed = keys::SpeedKeyFor(k_Platform, key, held); speed != keys::SpeedKey::None)
			{
				auto& clock = Locator::time::value();
				clock.SetSpeed(speed == keys::SpeedKey::Slower ? std::max(clock.GetSpeed() * 0.5f, 0.5f)
				                                               : std::min(clock.GetSpeed() + clock.GetSpeed(), 2.0f));
				break;
			}
			if (key >= SDLK_1 && key <= SDLK_7)
			{
				if (!BookmarkKeysAllowed() || !Locator::cameraBookmarkSystem::value().IsEnabled())
				{
					break;
				}
				auto& bookmarks = Locator::cameraBookmarkSystem::value();
				const auto slot = static_cast<size_t>(key - SDLK_1);
				switch (keys::BookmarkKeyFor(k_Platform, held))
				{
				case keys::BookmarkKey::Clear:
					if (bookmarks.IsSet(slot))
					{
						bookmarks.ClearBookmark(slot);
					}
					break;
				case keys::BookmarkKey::Set:
				{
					// The bookmark keeps the place of the hand, or the thing under the hand
					const auto left = Locator::handSystem::value().GetDrawnHandPosition();
					const auto right =
					    Locator::handSystem::value()
					        .GetPlayerHandPositions()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)];
					if (left.has_value() || right.has_value())
					{
						std::optional<entt::entity> thing;
						if (Locator::pickingSystem::has_value())
						{
							thing = Locator::pickingSystem::value().GetPick().object;
						}
						bookmarks.SetBookmark(slot, left.value_or(right.value_or(glm::zero<glm::vec3>())), thing,
						                      camera.GetOrigin());
					}
					break;
				}
				case keys::BookmarkKey::ToggleFollow:
					bookmarks.ToggleFollow(slot);
					break;
				case keys::BookmarkKey::Jump:
					bookmarks.JumpTo(slot);
					break;
				case keys::BookmarkKey::None:
					break;
				}
				break;
			}
			const auto move = keys::BookmarkMoveFor(k_Platform, key, held);
			if (move == keys::BookmarkMove::None || !BookmarkKeysAllowed())
			{
				break;
			}
			auto& bookmarks = Locator::cameraBookmarkSystem::value();
			switch (move)
			{
			case keys::BookmarkMove::Back:
				bookmarks.JumpBack();
				break;
			case keys::BookmarkMove::Previous:
				bookmarks.JumpToPrevious();
				break;
			case keys::BookmarkMove::Next:
				bookmarks.JumpToNext();
				break;
			case keys::BookmarkMove::NextTown:
			{
				std::optional<entt::entity> thing;
				if (Locator::pickingSystem::has_value())
				{
					thing = Locator::pickingSystem::value().GetPick().object;
				}
				bookmarks.FlyToNextTown(thing);
				break;
			}
			case keys::BookmarkMove::None:
				break;
			}
			break;
		}
		}
		break;
	case SDL_MOUSEMOTION:
	{
		// While the mouse turns the camera the cursor is held where it was, and the pointer isn't followed
		if (!Locator::gameActionSystem::value().IsCursorFrozen())
		{
			_mousePosition = {event.motion.x, event.motion.y};
			_handPointer.Moved();
		}
		break;
	}
	}

	return true;
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

void Game::WarmUpFirstFrame()
{
	const auto& config = Locator::config::value();
	if (!config.warmUpFirstFrame || !Locator::rendererInterface::has_value() || !Locator::rendereringSystem::has_value() ||
	    !Locator::camera::has_value())
	{
		return;
	}
	auto& renderer = Locator::rendererInterface::value();
	auto& rendering = Locator::rendereringSystem::value();
	// The draw lists of the land's things, as the first frame makes them
	if (config.drawEntities)
	{
		rendering.PrepareDraw(config.drawBoundingBoxes, config.drawFootpaths, config.drawStreams);
	}
	const auto start = std::chrono::steady_clock::now();
	// Some renderers divide the warm-up between some frames, each with a part of the draws
	bool shown = false;
	bool partsLeft = true;
	for (uint32_t part = 0; partsLeft; ++part)
	{
		partsLeft = false;
		shown = RenderLoadingFrame(true, [&renderer, &config, &partsLeft, part, this]() {
			partsLeft = renderer.WarmUpScene(SceneDrawDesc(config, 0, IsHandDrawn()), part);
			// The interface draws over the scene too, and the cinema bars are its first draw
			gui::Canvas::WarmUp(graphics::RenderPass::CinemaBars);
			// The villagers' labels and the tip bubble test the depth of the scene, sorted among its see-through things
			// outside the temple
			gui::Canvas::WarmUp(graphics::RenderPass::WorldLabels, gui::Canvas::DepthTest::NearerIsGreater);
			gui::Canvas::WarmUp(graphics::RenderPass::Translucent, gui::Canvas::DepthTest::NearerIsGreater, true);
			// The interface that the game draws over the scene, which the loading screen does not blend in each way
			gui::Canvas::WarmUp(graphics::RenderPass::Interface);
		});
	}
	// The first frame makes the draw lists again from what the game's first update changed
	if (config.drawEntities)
	{
		rendering.SetLayoutDirty();
	}
	if (shown)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: the first frame drawn behind the loading screen in {:.1f} ms",
		                   std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
	}
}

bool Game::IsHandDrawn() const
{
	return hand_visibility::IsShown(_interface && _interface->IsDialogOpen(),
	                                Locator::cinematicDirectorSystem::value().IsInterfaceActive(),
	                                Locator::temple::has_value() && Locator::temple::value().Active());
}

bool Game::FrontEndBoxShows() const
{
	return _interface && (_interface->IsDialogOpen() || _interface->GetTattooEditor().IsOpen());
}

std::optional<help::profile::WorldCameraView> Game::HelpWorldCameraView()
{
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto& model = Locator::camera::value().GetModel();
	if (dynamic_cast<const DefaultWorldCameraModel*>(&model) == nullptr &&
	    dynamic_cast<const FightCameraModel*>(&model) == nullptr)
	{
		return std::nullopt;
	}
	const auto camera = gui::hand_tooltip::CameraNow();
	return help::profile::WorldCameraView {.centreDistance = camera.centreDistance, .pitch = camera.pitch};
}

void Game::UpdateGestures(const Camera& camera, glm::ivec2 screenSize, float deltaSeconds)
{
	if (!Locator::gestureSystem::has_value() || screenSize.x <= 0 || screenSize.y <= 0)
	{
		return;
	}
	const auto size = static_cast<glm::vec2>(screenSize);
	const auto rayAt = [&camera, size](glm::vec2 pixel) {
		glm::vec3 origin;
		glm::vec3 direction;
		camera.DeprojectScreenToWorld(pixel / size, origin, direction);
		return std::pair {origin, direction};
	};
	ecs::systems::GestureSystemInterface::Frame frame {
	    .seconds = deltaSeconds,
	    .cursor = static_cast<glm::vec2>(_mousePosition),
	    // Not over a debug window, nor while a cut scene has the screen
	    .overWorld = !Locator::debugGui::value().StealsFocus() && !Locator::debugGui::value().IsMouseOverWindow() &&
	                 Locator::cinematicDirectorSystem::value().IsInterfaceActive(),
	    .actionHeld = Locator::gameActionSystem::value().Get(input::BindableActionMap::ACTION),
	    .player = PlayerNames::PLAYER_ONE,
	    .view = {.screenSize = size,
	             .cameraRight = camera.GetRight(),
	             .cameraEye = camera.GetOrigin(),
	             .cameraForward = camera.GetForward()},
	    .cameraShaking = Locator::explosionSystem::has_value() && Locator::explosionSystem::value().IsShaking(),
	};
	// The fight view that shows a fight in its arena, and the camera of Creature Mode, let the hand draw on while they
	// move
	const auto& model = camera.GetModel();
	const bool fights = Locator::creatureFightSystem::has_value();
	frame.cameraLetsHandDraw = (dynamic_cast<const FightCameraModel*>(&model) != nullptr && fights &&
	                            Locator::creatureFightSystem::value().IsCameraOnFightInArena()) ||
	                           dynamic_cast<const CreatureCameraModel*>(&model) != nullptr;
	frame.moveHeldInFight = fights && Locator::creatureFightSystem::value().DirectsFight() &&
	                        Locator::gameActionSystem::value().Get(input::BindableActionMap::MOVE);
	// The land or the sea under a point of the screen, as the interface takes it to be under the cursor
	frame.view.landAt = [&camera, size](glm::vec2 pixel) -> std::optional<glm::vec3> {
		if (!Locator::pickingSystem::has_value())
		{
			return std::nullopt;
		}
		const auto [eye, nearPoint] = camera.OnNearPlane(pixel / size);
		return Locator::pickingSystem::value().LandUnderPixel(eye, nearPoint,
		                                                      ecs::systems::PickingSystemInterface::Sea::Anywhere);
	};
	// A point of the screen as far in front of the camera as another point
	frame.view.atDepthOf = [rayAt, forward = camera.GetForward()](glm::vec2 pixel, glm::vec3 sameDepthAs) {
		const auto [origin, direction] = rayAt(pixel);
		float distance = 0.0f;
		if (glm::intersectRayPlane(origin, direction, sameDepthAs, -forward, distance))
		{
			return origin + (direction * distance);
		}
		return sameDepthAs;
	};
	// Where a recognised gesture's trail is laid under a pixel: where the line from the camera through it meets the land,
	// or the sea's level no further than 7500 units from the camera across the land, at the land's height there;
	// otherwise 400 units from the camera towards the pixel
	frame.view.trailPointUnder = [&camera, rayAt, size, eye = camera.GetOrigin()](glm::ivec2 pixel) {
		const auto direction = rayAt(glm::vec2(pixel)).second;
		const auto onNear = camera.OnNearPlane(glm::vec2(pixel) / size);
		if (Locator::pickingSystem::has_value())
		{
			if (const auto hit = Locator::pickingSystem::value().LandOrSeaAlong(onNear.eye, onNear.point, onNear.eye))
			{
				return *hit;
			}
		}
		return eye + (direction * gesture::k_TrailSkyDistance);
	};
	Locator::gestureSystem::value().Update(frame);
	// A path drawn for the testbed is drawn by the hand, which follows it
	if (const auto drawing = Locator::gestureSystem::value().GetDrawingPoint())
	{
		_mousePosition = glm::ivec2(*drawing * (size.y / gesture::k_ReferenceHeight));
		_handPointer.Moved();
	}
}

bool Game::BookmarkKeysAllowed()
{
	if (!Locator::cinematicDirectorSystem::value().IsInterfaceActive())
	{
		return false;
	}
	if (!Locator::cameraHelpSystem::has_value())
	{
		return true;
	}
	const auto& help = Locator::cameraHelpSystem::value().Get();
	return help.cameraKeys && help.placeKeys;
}

bool Game::HandOverThing() const
{
	// In the temple the hand only ever wiggles
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		return false;
	}
	if (!Locator::pickingSystem::has_value())
	{
		return false;
	}
	// Only a thing with a model of its own (a bubble's is drawn by the miracles): a fish farm, found through its fish,
	// has none
	const auto picked = Locator::pickingSystem::value().GetPick().object;
	return picked.has_value() && Locator::entitiesRegistry::value().Valid(*picked) &&
	       Locator::entitiesRegistry::value().AnyOf<ecs::components::Mesh, ecs::components::OneOffSpellSeed>(*picked);
}

void Game::ResolveScreenShapes()
{
	// The pick's last step, once the interface is drawn: the help's text bubble and the tied leash's rope under the cursor
	// are weighed against what the frame picked, and the bubble and the rope learn whether they have the cursor
	if (!Locator::pickingSystem::has_value() || !Locator::camera::has_value() || !Locator::windowing::has_value())
	{
		return;
	}
	ecs::systems::PickingSystemInterface::ScreenShapes shapes;
	if (Locator::tipBubbleSystem::has_value())
	{
		const auto& bubble = Locator::tipBubbleSystem::value();
		shapes.textBubble = bubble.GetUnderPointer();
		shapes.textBubblePoint = bubble.GetAnchor().value_or(glm::vec3(0.0f));
	}
	if (Locator::leashSystem::has_value())
	{
		shapes.rope = Locator::leashSystem::value().RopeUnderCursor(
		    PlayerNames::PLAYER_ONE, {.viewProjection = Locator::camera::value().GetViewProjectionMatrix(),
		                              .screenSize = static_cast<glm::vec2>(Locator::windowing::value().GetSize()),
		                              .cursor = Locator::gameActionSystem::value().GetPointerPosition()});
	}
	auto& picking = Locator::pickingSystem::value();
	picking.ResolveScreenShapes(shapes);
	const auto& pick = picking.GetPick();
	if (Locator::tipBubbleSystem::has_value())
	{
		Locator::tipBubbleSystem::value().SetUnderHand(pick.textBubble);
	}
	if (Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().SetRopeHovered(PlayerNames::PLAYER_ONE, pick.rope);
	}
}

void Game::PickUnderCursor(float seconds)
{
	if (!Locator::pickingSystem::has_value() || !Locator::windowing::has_value())
	{
		return;
	}
	const auto screenSize = Locator::windowing::value().GetSize();
	if (screenSize.x <= 0 || screenSize.y <= 0)
	{
		return;
	}
	const auto& camera = Locator::camera::value();
	const auto resolution = static_cast<glm::vec2>(screenSize);
	const auto cursor = static_cast<glm::vec2>(_mousePosition);
	const auto [eye, nearPoint] = camera.OnNearPlane(cursor / resolution);
	Locator::pickingSystem::value().PickUnderCursor({
	    .view =
	        {
	            .worldToClip = camera.GetViewProjectionMatrix(),
	            .resolution = resolution,
	            .near = camera.GetNearClip(),
	            .xScale = camera.GetProjectionMatrix()[0][0],
	            .camera = eye,
	            .cursor = cursor,
	            .up = camera.GetUp(),
	        },
	    .nearPoint = nearPoint,
	    .seconds = seconds,
	    // Gripping the land, the interface holds what it picked
	    .locked = _handCameraState && _handPose == hand_navigation_pose::Pose::Grip,
	});
}

void Game::UpdateHandInterface()
{
	if (!_interface)
	{
		return;
	}
	_interface->SetCreaturePanel(std::nullopt);
	// The temple places the hand's tooltip itself
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (inTemple)
	{
		return;
	}
	// The creature the hand is held to, or else the one it is over, any player's
	const auto& creatureHand = Locator::creatureHandSystem::value();
	auto creature = creatureHand.GetCreature();
	const bool rightButtonHeld = (Locator::gameActionSystem::value().GetPointerButtons() & SDL_BUTTON_RMASK) != 0;
	if (!creature.has_value() && creature_panel::Triggered(rightButtonHeld))
	{
		creature = _creatureUnderHand;
	}
	// Without one, the creature Creature Mode follows shows near the top of the screen, without the reward
	std::optional<float> reward = creatureHand.GetLastFeedbackSum();
	if (!creature.has_value() && Locator::creatureModeSystem::has_value())
	{
		creature = Locator::creatureModeSystem::value().GetCreature();
		reward.reset();
	}
	// or the creature a script's camera follows or looks at, any player's
	if (!creature.has_value())
	{
		creature = ScriptCameraCreature();
		reward.reset();
	}
	if (!creature.has_value())
	{
		return;
	}
	const auto* needs = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureNeeds>(*creature);
	if (needs != nullptr)
	{
		// The reward is the hand's, which it keeps showing after letting go until it takes hold again
		_interface->SetCreaturePanel(creature_panel::FromNeeds(needs->needs, reward));
	}
}

void Game::UpdateHandToolTipAnchor()
{
	// The temple places the hand's tooltip itself
	if (!_interface || !Locator::windowing::has_value() || (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	// Not while the pointer is over a debug window
	if (Locator::debugGui::value().IsMouseOverWindow())
	{
		_interface->SetHandOnScreen(std::nullopt);
		return;
	}
	const auto handEntity =
	    Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	const auto* handTransform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(handEntity);
	if (handTransform == nullptr)
	{
		_interface->SetHandOnScreen(std::nullopt);
		return;
	}
	// At the hand as drawn this frame, wherever the pointer is
	const auto& camera = Locator::camera::value();
	const auto hand = gui::world_tooltip::HandOnScreen(camera.GetViewProjectionMatrix(), camera.GetNearClip(),
	                                                   handTransform->position, Locator::windowing::value().GetSize());
	_interface->SetHandOnScreen(hand.has_value() ? std::optional(glm::vec2(*hand)) : std::nullopt);
}

std::optional<glm::vec2> Game::OnScreen(glm::vec3 point) const
{
	if (!Locator::windowing::has_value())
	{
		return std::nullopt;
	}
	const auto size = Locator::windowing::value().GetSize();
	glm::vec3 screen;
	if (!Locator::camera::value().ProjectWorldToScreen(point, glm::vec4(0.0f, 0.0f, glm::vec2(size)), screen))
	{
		return std::nullopt;
	}
	// The game takes the whole pixel it falls in, which must be on the screen
	const glm::ivec2 pixel {static_cast<int>(screen.x), static_cast<int>(screen.y)};
	if (pixel.x < 0 || pixel.y < 0 || pixel.x >= size.x || pixel.y >= size.y)
	{
		return std::nullopt;
	}
	return glm::vec2(pixel);
}

void Game::UpdateFloatingNumbers(float seconds)
{
	ecs::floating_number::StepAll(Locator::entitiesRegistry::value(), seconds);
}

void Game::UpdateLearningMarks(float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	ecs::learning_marker::StepAll(registry, milliseconds);
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	// The bulbs and glows are drawn with the world's effects
	std::vector<particles::draw::LearningMark> marks;
	const auto bulb = resources::HashIdentifier(MeshId::ObjectLightBulb);
	registry.Each<const ecs::components::LearningMarker>(
	    [&marks, bulb](entt::entity /*unused*/, const ecs::components::LearningMarker& marker) {
		    const auto look = ecs::learning_marker::LookOf(marker);
		    marks.push_back({.glow = look.glow,
		                     .glowHalfSize = look.glowHalfSize,
		                     .glowColour = look.glowColour,
		                     .bulb = look.bulb,
		                     .bulbScale = look.bulbScale,
		                     .bulbColour = look.bulbColour,
		                     .bulbMesh = bulb});
	    });
	Locator::particleSystem::value().SetLearningMarks(std::move(marks));
}

void Game::PlaceLearningTexts()
{
	if (!_interface)
	{
		return;
	}
	std::vector<gui::GameInterface::LearningText> texts;
	Locator::entitiesRegistry::value().Each<const ecs::components::LearningMarker>(
	    [this, &texts](entt::entity /*unused*/, const ecs::components::LearningMarker& marker) {
		    const auto look = ecs::learning_marker::LookOf(marker);
		    if (marker.text.empty())
		    {
			    return;
		    }
		    if (const auto screen = OnScreen(look.textPoint); screen.has_value())
		    {
			    texts.push_back({.screen = *screen, .text = gui::ToUtf16(marker.text), .alpha = look.textAlpha});
		    }
	    });
	_interface->SetLearningTexts(std::move(texts));
}

void Game::PlaceScreenLabels()
{
	if (!_interface)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// Those on the screen, the furthest first so the nearer are drawn over them
	struct Placed
	{
		float distance;
		gui::GameInterface::FloatingNumber number;
	};
	std::vector<Placed> placed;
	const auto eye = Locator::camera::value().GetOrigin();
	registry.Each<const ecs::components::FloatingNumber>(
	    [this, &placed, eye](entt::entity /*unused*/, const ecs::components::FloatingNumber& number) {
		    const auto alpha = ecs::floating_number::Alpha(number.life);
		    const auto screen = OnScreen(number.position);
		    if (!alpha.has_value() || !screen.has_value())
		    {
			    return;
		    }
		    placed.push_back(
		        {.distance = glm::distance(eye, number.position),
		         .number = {.screen = *screen, .text = gui::ToUtf16(number.text), .colour = number.colour, .alpha = *alpha}});
	    });
	std::ranges::sort(placed, std::ranges::greater {}, &Placed::distance);
	std::vector<gui::GameInterface::FloatingNumber> numbers;
	numbers.reserve(placed.size());
	for (auto& each : placed)
	{
		numbers.push_back(std::move(each.number));
	}
	_interface->SetFloatingNumbers(std::move(numbers));

	if (!Locator::windowing::has_value())
	{
		return;
	}
	// Inside the temple the land's villagers aren't drawn, nor their labels
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		_interface->SetVillagerLabels({});
		return;
	}
	_interface->SetVillagerLabels(ecs::villager_label_source::Gather({
	    .screen = Locator::windowing::value().GetSize(),
	    .nameFade = _villagerNameFadeDrawn,
	    .texts = &_interface->GetTexts(),
	}));
}

void Game::UpdateVillagerLabels(float milliseconds)
{
	// The names display changes size only on a frame that draws the land
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool landHidden = Locator::videoSystem::has_value() &&
	                        (Locator::videoSystem::value().CoversScreen() || Locator::videoSystem::value().HidesWorld());
	const auto frame = gui::villager_labels::StepNameFadeFrame(_villagerNameFade,
	                                                           Locator::entitiesRegistry::value().Context().villagerNamesShown,
	                                                           milliseconds, !inTemple && !landHidden);
	_villagerNameFadeDrawn = frame.drawn;
	_villagerNameFade = frame.next;
}

void Game::ProcessHandToolTipTurn()
{
	if (!_interface || (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	auto& toolTips = _interface->GetToolTips();
	// The shown tooltip's display time runs down only while the player may turn the camera
	const bool displayTimeRuns = !Locator::cameraHelpSystem::has_value() ||
	                             (Locator::cameraHelpSystem::value().Get().features & camera_help::feature::k_Rotate) != 0;
	// When the hand takes a miracle, or the miracle in the hand gets a new power-up level, the hand shows its name at once
	if (const auto text =
	        Locator::magicSystem::has_value() ? Locator::magicSystem::value().TakeHeldMiracleToolTip() : std::nullopt;
	    text.has_value())
	{
		if (const auto index = gui::world_tooltip::ToolTipOfText(*text); index.has_value())
		{
			toolTips.Force(*index, 0.0f);
		}
	}
	// As the player's own totem moves on the screen, the hand shows the share it is held at, over anything else
	if (const auto tip = Locator::villageTotemSystem::has_value()
	                         ? Locator::villageTotemSystem::value().TakeShareToolTip(PlayerNames::PLAYER_ONE)
	                         : std::nullopt;
	    tip.has_value() && Locator::entitiesRegistry::value().Valid(tip->totem) &&
	    OnScreen(Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(tip->totem).position).has_value())
	{
		toolTips.Force(ecs::village_totem::k_ShareToolTip, tip->percent);
	}
	// Let go of the creature having told it next to nothing, the hand offers to focus on it, over anything else
	if (Locator::creatureHandSystem::value().TakeFocusTip())
	{
		toolTips.Force(creature_leash_orders::k_FocusCreatureTip, 1.0f);
	}
	// As the hand takes food or wood from a pile, it shows the amount it holds, over anything else
	if (Locator::handGrabSystem::has_value())
	{
		if (const auto amount = Locator::handGrabSystem::value().TakeAmountToolTip(); amount.has_value())
		{
			const auto held = gui::world_tooltip::HeldAmount(*amount);
			toolTips.SubmitNumbered(held.index, gui::ToolTipAction::None, held.first, std::nullopt, true);
		}
	}
	const auto pointed = Locator::pickingSystem::has_value() ? Locator::pickingSystem::value().GetPick().object : std::nullopt;
	const bool shown = !_interface->IsDialogOpen() && Locator::cinematicDirectorSystem::value().IsInterfaceActive();
	if (shown)
	{
		SubmitHandToolTips(toolTips, pointed);
	}
	toolTips.ProcessTurn(displayTimeRuns);
}

void Game::SubmitHandToolTips(gui::ToolTips& toolTips, std::optional<entt::entity> pointed)
{
	namespace tips = gui::hand_tooltip;
	const auto submitAll = [&toolTips](const std::vector<tips::Offer>& offers) {
		for (const auto& offer : offers)
		{
			toolTips.Submit(offer.index, offer.action, offer.arrows);
		}
	};
	// Over the land, or over a thing that the hand can do nothing with, the hand shows how a drag moves the camera. When
	// the middle of the screen shows the land, the help counts it.
	const auto overNothing = [&submitAll]() {
		const auto camera = tips::CameraNow();
		const bool overFish = HandOverFish();
		if (const auto event = tips::OverNothingHelpEvent(camera, overFish);
		    event.has_value() && Locator::helpProfileSystem::has_value())
		{
			Locator::helpProfileSystem::value().Trigger(*event);
		}
		submitAll({tips::OverNothing(camera, overFish)});
	};
	// What a tap does with the leash. Each time the hand offers to focus on the creature, the help counts it.
	const auto leashTip = [pointed]() {
		const auto tip = Locator::leashSystem::value().ToolTip(PlayerNames::PLAYER_ONE, pointed);
		if (tip == creature_leash_orders::k_FocusCreatureTip && Locator::helpProfileSystem::has_value())
		{
			Locator::helpProfileSystem::value().Trigger(help::profile::k_FocusCreatureTip);
		}
		return tip;
	};
	// While the hand grips the land, it shows "Zoom" when the Action button is held too. In other conditions it shows
	// what a tap does with the leash, or how the drag moves the camera. The grip starts with a drag of the land and
	// continues until the Move button is let go. Both buttons together stop the drag but not the grip.
	const bool bothButtons = Locator::gameActionSystem::has_value() &&
	                         Locator::gameActionSystem::value().Get(input::UnbindableActionMap::TWO_BUTTON_CLICK);
	const bool moveHeld = bothButtons || (Locator::gameActionSystem::has_value() &&
	                                      Locator::gameActionSystem::value().Get(input::BindableActionMap::MOVE));
	if (Locator::camera::has_value() && Locator::camera::value().GetModel().GetHandCues().dragging)
	{
		_tipsLandGrip = true;
	}
	else if (!moveHeld)
	{
		_tipsLandGrip = false;
	}
	if (_tipsLandGrip)
	{
		// The Action button counts as held from its press until it is let go, by itself or with the Move button
		const bool actionHeld = bothButtons || (Locator::gameActionSystem::has_value() &&
		                                        Locator::gameActionSystem::value().Get(input::BindableActionMap::ACTION));
		const auto camera = tips::CameraNow();
		if (const auto zoom = tips::GrippingLand(actionHeld, camera); zoom.index == tips::k_Zoom)
		{
			submitAll({zoom});
			return;
		}
		if (const auto tip = leashTip())
		{
			toolTips.Submit(*tip, gui::ToolTipAction::Apply, gui::ToolTipArrows::k_None);
			return;
		}
		overNothing();
		return;
	}
	// With a miracle in the hand, poured or not, the hand offers to cast it where it may be cast: on the thing under the
	// pointer, or on the land where there is none. In other places it shows what it shows over the land.
	const auto held = Locator::magicSystem::has_value() ? Locator::magicSystem::value().HeldCastAt(pointed) : std::nullopt;
	if (held.has_value())
	{
		if (const auto tip = gui::world_tooltip::HeldMiracle(magic::OfferFor(*held)); tip.has_value())
		{
			toolTips.Submit(*tip, gui::ToolTipAction::Apply, gui::ToolTipArrows::k_None);
			return;
		}
	}
	// With the leash held, the hand says what a tap of the Action button does with it, before anything else
	if (const auto tip = leashTip())
	{
		toolTips.Submit(*tip, gui::ToolTipAction::Apply, gui::ToolTipArrows::k_None);
		return;
	}
	if (held.has_value())
	{
		overNothing();
		return;
	}
	// While the player's creature duels, the hand offers to block over it, to attack over its opponent, and to manoeuvre
	// anywhere else, each by the Action button
	if (const auto tip = Locator::creatureFightSystem::value().HandTip(_creatureUnderHand))
	{
		toolTips.Submit(creature_fight::ToolTipIndex(*tip), gui::ToolTipAction::Apply, gui::ToolTipArrows::k_None);
		return;
	}
	// While the hand holds a thing, it offers to give it, to put it into a store, or to drop and throw it. A hand that
	// holds the button down to throw offers nothing.
	if (Locator::handGrabSystem::has_value())
	{
		const auto& grab = Locator::handGrabSystem::value();
		if (const auto thing = grab.GetHeld(); thing.has_value())
		{
			if (grab.IsAboutToThrow())
			{
				return;
			}
			const bool tree = Locator::entitiesRegistry::value().AllOf<ecs::components::Tree>(*thing);
			if (const auto offers = tips::Holding(HandInInfluence(), tips::UseAt(*thing, pointed), tree); offers.has_value())
			{
				submitAll(*offers);
				return;
			}
			overNothing();
			return;
		}
	}
	// Over the player's own creature, the hand shows that it can take hold of it to stroke or slap it. It can hold other
	// players' creatures too, but the game only offers it for the player's own.
	const auto over = _creatureUnderHand.has_value() ? _creatureUnderHand : Locator::creatureHandSystem::value().GetCreature();
	if (over.has_value())
	{
		// Within the influence, over a creature that takes hold, the mouse shows the Action button; in other places the
		// tooltip has no mouse
		const auto tip = Locator::creatureHandSystem::has_value()
		                     ? Locator::creatureHandSystem::value().InteractTipOver(*over, HandInInfluence())
		                     : creature_hand::InteractTip::None;
		if (tip != creature_hand::InteractTip::None)
		{
			toolTips.Submit(creature_panel::k_InteractToolTip,
			                tip == creature_hand::InteractTip::WithAction ? gui::ToolTipAction::Apply
			                                                              : gui::ToolTipAction::None,
			                gui::ToolTipArrows::k_None);
			return;
		}
		overNothing();
		return;
	}
	// While the hand grips the tip bubble of a sign, or the pointer is on the bubble and on nothing else, the hand offers
	// to scroll its words
	if (Locator::tipBubbleSystem::has_value())
	{
		const auto& bubble = Locator::tipBubbleSystem::value();
		if (bubble.IsGripped() || (!pointed.has_value() && bubble.IsUnderHand()))
		{
			submitAll({tips::BubbleOffer()});
			return;
		}
	}
	if (!pointed.has_value() || !tips::IsInteractable(*pointed))
	{
		overNothing();
		return;
	}
	// Over a building that is not fully built or not fully repaired, the wood that it wants and how far it is built take
	// turns, wherever the hand is
	if (const auto building = tips::BuildingOf(*pointed); building.has_value())
	{
		if (building->wood.words == tips::k_WoodRequired)
		{
			toolTips.SubmitNumbered(tips::k_WoodRequired, gui::ToolTipAction::None, building->wood.number, std::nullopt);
		}
		else
		{
			toolTips.SubmitInWords(tips::k_WoodRequired, building->wood.words, static_cast<float>(building->wood.number));
		}
		toolTips.SubmitNumbered(tips::k_BuildingCompleted, gui::ToolTipAction::None, building->completed, std::nullopt);
		return;
	}
	const bool inInfluence = HandInInfluence();
	if (const auto flagTip = Locator::townDesireSystem::value().GetFlagToolTip(*pointed);
	    flagTip.has_value() && gui::world_tooltip::Shows(gui::world_tooltip::Subject::DesireFlag, inInfluence))
	{
		// Over a flag of a town's store, the hand shows how much the town wants its thing, in the flag's own words
		toolTips.SubmitInWords(ecs::town_desire_flags::k_NeedToolTip, flagTip->words, flagTip->percent);
		return;
	}
	// Over a temple, a village centre, a store, an altar or a pile, the hand says what it holds or can do
	if (const auto submissions = gui::world_tooltip::Over(*pointed, inInfluence); !submissions.empty())
	{
		for (const auto& submission : submissions)
		{
			if (submission.wordsOnly)
			{
				toolTips.Submit(submission.numbers.index, submission.action, gui::ToolTipArrows::k_None, submission.force);
			}
			else
			{
				toolTips.SubmitNumbered(submission.numbers.index, submission.action, submission.numbers.first,
				                        submission.numbers.second, submission.force);
			}
		}
		return;
	}
	// Within the influence, the way into a temple that is not the player's offers nothing
	if (inInfluence && Locator::entitiesRegistry::value().AllOf<ecs::components::TempleEntrance>(*pointed))
	{
		return;
	}
	// A scroll that a script put in the world offers to start it, or to remind the player of it, anywhere
	if (const auto scroll = tips::ScrollOf(*pointed); scroll.has_value())
	{
		submitAll({*scroll});
		return;
	}
	// The rest needs the hand in the player's influence
	if (!inInfluence)
	{
		overNothing();
		return;
	}
	// Over a miracle icon, whoever's it is, the hand names its miracle at once. A worship site's icon also shows how full
	// its miracle is.
	if (const auto icon = tips::SpellIconOf(*pointed); icon.has_value())
	{
		const auto charge =
		    Locator::spellIconSystem::has_value() ? Locator::spellIconSystem::value().ShownCharge(*pointed) : std::nullopt;
		toolTips.ForceWithCharge(icon->index, charge);
		return;
	}
	// Over a one-shot globe at rest, the hand names its miracle at once. A globe in flight is caught as any thing is.
	if (const auto globe = tips::GlobeOf(*pointed); globe.has_value())
	{
		if (tips::IsMoving(*pointed))
		{
			submitAll(tips::OverTakeable(true, std::nullopt));
		}
		else
		{
			toolTips.ForceWithCharge(globe->index, std::nullopt);
		}
		return;
	}
	// Over a thing that the hand can take: "Catch" while it moves, else "Pick Up" and what a tap does to it
	if (Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().WhyNotTake(*pointed).empty())
	{
		submitAll(tips::OverTakeable(tips::IsMoving(*pointed), tips::TakeableTapOf(*pointed)));
		return;
	}
	if (const auto tap = tips::TapOf(*pointed); tap.has_value())
	{
		submitAll({*tap});
		return;
	}
	overNothing();
}

void Game::ProcessTurnCheering()
{
	// While the turning lesson asks for it, the advisors cheer the player's turning of the camera on
	if (!Locator::cameraHelpSystem::has_value() || !Locator::gameRandom::has_value() || !Locator::chlapi::has_value())
	{
		return;
	}
	auto& help = Locator::cameraHelpSystem::value().Get();
	auto& random = Locator::gameRandom::value();
	const help::turn_cheer::Random draws {
	    .upTo = [&random](float x) { return random.LocalFloatRand(x); },
	    .below = [&random](int32_t n) { return random.LocalRand(n); },
	};
	if (const auto line = help::turn_cheer::ProcessTurn(help.turnCheering, help.turnSpeed, GetTurn(), draws))
	{
		Locator::chlapi::value().PlayBankSoundEffect(static_cast<int32_t>(audio::ScriptSoundBank::HelpSprites), *line, {});
	}
}

void Game::PlayViewStep(game_turn::Step step, glm::vec3 cameraPosition)
{
	using game_turn::Step;
	auto& clock = Locator::time::value();
	switch (step)
	{
	case Step::TurnCheering:
		ProcessTurnCheering();
		break;
	case Step::HandToolTips:
		ProcessHandToolTipTurn();
		break;
	case Step::HelpProfile:
		// The help's count of what the player did moves on a turn
		if (Locator::helpProfileSystem::has_value())
		{
			Locator::helpProfileSystem::value().ProcessTurn(FrontEndBoxShows(), HelpWorldCameraView());
		}
		break;
	case Step::Ambience:
		// The ambience follows the weather at the camera
		if (_atmosAudio)
		{
			ecs::components::WeatherInfo weather {};
			if (Locator::weatherSystem::has_value())
			{
				weather = Locator::weatherSystem::value().GetWeatherSmooth(cameraPosition);
			}
			_atmosAudio->SetAlignment(Locator::alignmentSystem::value().GetCameraAlignment());
			_atmosAudio->EndTurn({
			    .camera = cameraPosition,
			    .weather =
			        {
			            .rain = weather.rain,
			            .snow = weather.snow,
			            .windX = weather.windX,
			            .windZ = weather.windZ,
			        },
			    .paused = false,
			    .turn = clock.GetTurn(),
			    .inCitadel = false,
			    .videoPlaying = Locator::videoSystem::value().IsPlaying(),
			});
		}
		break;
	case Step::Music:
		ProcessMusicTurn(cameraPosition, false);
		break;
	default:
		// The world does the other steps (see simulation::PlayTurnStep)
		break;
	}
}

void Game::UpdatePauseText()
{
	if (!Locator::pauseTextSystem::has_value() || !_interface)
	{
		return;
	}
	const auto& clock = Locator::time::value();
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool scriptHasSpeed =
	    Locator::scriptControlSystem::has_value() && Locator::scriptControlSystem::value().GetGameSpeedOwner() != 0;
	Locator::pauseTextSystem::value().Update(
	    {
	        .paused = clock.IsPaused(),
	        .speed = clock.GetSpeed(),
	        .started = true,
	        .insideTemple = inTemple,
	        .scriptHasSpeed = scriptHasSpeed,
	        .editing = false,
	        .videoPlaying = Locator::videoSystem::value().IsPlaying(),
	        .boxWithBackground = _interface->IsBoxWithBackgroundActive(),
	    },
	    static_cast<uint32_t>(clock.GetFrameRealTime().count()));
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
	    .playerAlignment = alignment.GetPlayerAlignment(Locator::playerSystem::value().GetLocalPlayer()),
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
	music.creatureDance = ecs::creature_dance_music::Heard(cameraPosition);
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

void Game::ProcessTempleRoomKeys()
{
	if (!Locator::temple::has_value())
	{
		return;
	}
	// The keys for the temple's rooms take the player into the temple at that room's path in, and inside the temple
	// they cut to the room
	constexpr std::array<std::pair<input::BindableActionMap, TempleRoom>, 6> k_RoomKeys {{
	    {input::BindableActionMap::ZOOM_TO_INSIDE_TEMPLE, TempleRoom::Main},
	    {input::BindableActionMap::ZOOM_TO_CREATURE_ROOM, TempleRoom::CreatureCave},
	    {input::BindableActionMap::ZOOM_TO_CHALLENGE_ROOM, TempleRoom::Challenge},
	    {input::BindableActionMap::ZOOM_TO_SAVE_GAME_ROOM, TempleRoom::SaveGame},
	    {input::BindableActionMap::ZOOM_TO_OPTIONS_ROOM, TempleRoom::Options},
	    {input::BindableActionMap::ZOOM_TO_LIBRARY, TempleRoom::Credits},
	}};
	const auto& actions = Locator::gameActionSystem::value();
	auto& temple = Locator::temple::value();
	for (const auto& [action, room] : k_RoomKeys)
	{
		if (!actions.Get(action) || !actions.GetChanged(action))
		{
			continue;
		}
		if (temple.Active())
		{
			temple.GoToRoom(room);
		}
		else
		{
			temple.ActivateAfterFrame(room);
		}
		return;
	}
}

void Game::ProcessTempleAudioTurn()
{
	// The temple's turns play the citadel's music, while the land's ambience fades out
	ProcessMusicTurn(Locator::camera::value().GetOrigin(), true);
	if (_atmosAudio)
	{
		_atmosAudio->ContinueTurn(
		    {.paused = false, .turn = GetTurn(), .inCitadel = true, .videoPlaying = Locator::videoSystem::value().IsPlaying()});
	}
}

bool Game::Update() noexcept
{
	// The frame's steps go in the game's order, because many systems depend on it (see game_frame::k_Frame)
	FrameState frame;
	if (!game_frame::PlayFrame([this, &frame](game_frame::Step step) { return PlayFrameStep(step, frame); }))
	{
		return false;
	}
	if (sim_run::Finished(Locator::simulation::value().GetSettings(), Locator::time::value().GetTurn()))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The simulation run is at its last turn, {}", Locator::time::value().GetTurn());
		return false;
	}
	const auto& config = Locator::config::value();
	return config.numFramesToSimulate == 0 || _frameCount < config.numFramesToSimulate;
}

game_frame::GraphicsWindow Game::FrameGraphicsWindow() const
{
	game_frame::GraphicsWindow window {
	    .minimised = Locator::windowing::has_value() && Locator::windowing::value().IsMinimised(),
	    .fixedFrameTime = Locator::time::has_value() && Locator::time::value().GetFixedFrameTime().has_value(),
	};
	if (window.minimised)
	{
		// The pictures that the debug inspector, a challenge, the creature's page or a saved game wait for
		window.pictureWanted =
		    (_requestScreenshot.has_value() && _requestScreenshot->first == _frameCount) ||
		    ecs::challenge_pictures::AnyWaiting(Locator::entitiesRegistry::value()) ||
		    (Locator::creatureWebPageSystem::has_value() && Locator::creatureWebPageSystem::value().AnyPictureWaiting()) ||
		    (Locator::saveGameSystem::has_value() && Locator::saveGameSystem::value().AnyPictureWaiting());
	}
	return window;
}

bool Game::DoesFrameGraphics() const
{
	const auto window = FrameGraphicsWindow();
	return game_frame::DoesGraphics(window.minimised, window.pictureWanted);
}

bool Game::PlayFrameStep(game_frame::Step step, FrameState& frame)
{
	using game_frame::Step;
	// While the window is minimised the turns, the keys and the film's clock continue, but the camera, the start of the draw,
	// the hand and the world's draw stop
	if (!game_frame::DoesStep(step, FrameGraphicsWindow()))
	{
		return true;
	}
	switch (step)
	{
	case Step::FrameClock:
		StartFrame(frame);
		break;
	case Step::Input:
		UpdateInput();
		break;
	case Step::KeyCommands:
		SubmitKeyCommands();
		break;
	case Step::ScreenMarks:
		UpdateScreenMarks();
		break;
	case Step::DebugGui:
		return UpdateDebugGui();
	case Step::Interface:
		UpdateInterfaceFrame(frame.deltaTime);
		break;
	case Step::GripEffect:
		GripLandscapeEffect::Update(std::chrono::duration_cast<std::chrono::duration<float>>(frame.deltaTime).count());
		break;
	case Step::Turns:
		return PlayTurns();
	case Step::SendCommands:
		ReportHand();
		Locator::playerCommandSystem::value().SendOpenBlock();
		break;
	case Step::FrameTime:
		// The frame's game time: none while paused, quicker or slower with the game speed
		Locator::time::value().UpdateFrame();
		break;
	case Step::FilmClock:
		// The clock of a full-screen video counts the frames that are due by the real clock
		Locator::videoSystem::value().CountDueFrames(std::chrono::steady_clock::now());
		break;
	case Step::DrawClock:
		// The real time since the last draw, which the steps of the draw go by
		Locator::time::value().StartDraw();
		break;
	case Step::Video:
		// A full-screen video shows the frames that are due, fades, and ends
		Locator::videoSystem::value().Update(std::chrono::steady_clock::now());
		break;
	case Step::PauseText:
		// The words of the pause and the speed, as the draw finishes
		UpdatePauseText();
		break;
	case Step::ScreenFade:
		// The colour over the screen fades by the real time of the draw
		if (_interface)
		{
			const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
			_interface->GetScreenFade().Update(static_cast<uint32_t>(Locator::time::value().GetFrameRealTime().count()),
			                                   inTemple);
		}
		break;
	case Step::CameraMouse:
		// The camera reads the movement of the mouse and of the hand of this frame as it moves, after the turns
		if (!Locator::cameraPathSystem::value().HoldsCamera())
		{
			Locator::camera::value().HandleActions(frame.deltaTime);
		}
		break;
	case Step::CameraPaths:
		Locator::cameraPathSystem::value().Update(frame.deltaTime);
		break;
	case Step::CameraControllers:
		UpdateCameraControllers(frame.deltaTime);
		break;
	case Step::Camera:
		UpdateCamera();
		break;
	case Step::CameraShake:
	{
		// The blasts' rubble lies and fades, their dust flies and the camera shakes
		auto explosions = Locator::profiler::value().BeginScoped(Profiler::Stage::ExplosionUpdate);
		Locator::explosionSystem::value().Update(
		    std::chrono::duration<float, std::milli>(Locator::time::value().GetFrameGameTime()).count());
		// The camera keeps this view, with the shake of this frame, for its rays to the land on the next frame. A command
		// of this frame that puts the camera does not change the view of these rays.
		Locator::camera::value().KeepDrawnView();
		break;
	}
	case Step::Temple:
		UpdateTemple(frame.deltaTime);
		break;
	case Step::Bookmarks:
	{
		// The markers of the bookmarks throb on, only on a frame that draws the land
		const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
		const bool landHidden = Locator::videoSystem::has_value() &&
		                        (Locator::videoSystem::value().CoversScreen() || Locator::videoSystem::value().HidesWorld());
		Locator::cameraBookmarkSystem::value().Update(Locator::time::value().GetFrameGameTime(), !inTemple && !landHidden);
		break;
	}
	case Step::HighDetail:
		// The villagers a script draws in high detail are drawn as usual once its cinema bars are gone
		Locator::highDetailSystem::value().Update();
		break;
	case Step::CreaturePoses:
		UpdateCreaturePoses();
		break;
	case Step::FrameEffects:
		UpdateFrameEffects();
		break;
	case Step::Hand:
		UpdateHandFrame(frame.deltaTime);
		break;
	case Step::Entities:
		UpdateEntitiesFrame(frame.deltaTime);
		break;
	case Step::Audio:
		Locator::audio::value().Update();
		break;
	case Step::Online:
		UpdateOnline();
		break;
	case Step::CameraKeys:
		UpdateCameraKeys(frame.deltaTime);
		break;
	case Step::ProfileAlignment:
		Locator::landSystem::value().KeepAlignmentInProfile();
		break;
	case Step::_Count:
		break;
	}
	return true;
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
	if (Locator::lobbyClient::has_value())
	{
		Locator::lobbyClient::value().Update();
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
	Locator::time::value().Update();
	// The debug inspector answers what was asked since the last frame, and holds or releases the game for stepping,
	// before the frame's input and turn
	if (Locator::inspector::has_value())
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

void Game::UpdateInput()
{
	auto& profiler = Locator::profiler::value();
	auto& camera = Locator::camera::value();
	// Input events
	{
		auto sdlInput = profiler.BeginScoped(Profiler::Stage::SdlInput);
		// The debug windows' presses of the options screen's actions are made by the action map's frame
		auto& actions = Locator::gameActionSystem::value();
		// The world's camera is the one turned with the mouse, which holds the cursor and the hand still meanwhile
		actions.AllowCursorFreeze(
		    !(Locator::temple::has_value() && Locator::temple::value().Active()) &&
		    dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()) != nullptr &&
		    (Locator::cameraHelpSystem::value().Get().features & camera_help::feature::k_RotateAroundMouse) != 0);
		// The land's scripts may take the camera's keys away, as the tutorials do
		actions.SetBlockedActions(Locator::cameraHelpSystem::value().Get().BlockedActions());
		if (!Locator::debugGui::value().StealsFocus() || actions.HasQueuedPresses())
		{
			actions.Frame();
		}
		// A hand demonstration plays its records due now through the hand's own input
		if (Locator::handDemoSystem::has_value())
		{
			Locator::handDemoSystem::value().Update();
		}
		// The scripted mouse's moves and buttons take effect as they are pumped, as a real mouse's do in SDL: after the
		// action map's frame has read the pointer
		actions.PumpScriptedPointer();
		SDL_Event e;
		while (SDL_PollEvent(&e) != 0)
		{
			// While an agent drives the game, the player's mouse and keyboard are kept out of it
			if (!actions.AdmitEvent(e))
			{
				continue;
			}
			actions.PointPlayerEvent(e);
			Locator::events::value().Create<SDL_Event>(e);
		}
		FollowWindowSize();
		if (actions.IsCursorFrozen())
		{
			_mousePosition = glm::ivec2(actions.GetMousePosition());
		}
		if (const auto warp = actions.GetCursorWarp(); warp.has_value())
		{
			_mousePosition = *warp;
			_handPointer.Moved();
		}
		// The hand is put under the pointer every frame, moving or not: until the mouse first moves, under the pointer
		// the game keeps, so from a land's first frame it is where the cursor is, or the top left corner while the
		// cursor has never been over the window
		_mousePosition = _handPointer.Frame(
		    _mousePosition, actions.IsPointerKnown() ? std::optional(actions.GetPointerPosition()) : std::nullopt);
		ProcessTempleRoomKeys();
		_shortcutKeys.Update();
	}
}

void Game::SubmitKeyCommands()
{
	// The keys do nothing in the temple. While the debug windows have the keyboard, the controls do not change, so a key
	// that the player pressed would read as a new press on each frame.
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (inTemple || Locator::debugGui::value().StealsFocus() || !Locator::gameActionSystem::has_value())
	{
		return;
	}
	using input::BindableActionMap;
	const auto& actions = Locator::gameActionSystem::value();
	auto& commands = Locator::playerCommandSystem::value();
	// The leash shortcuts
	for (const auto action :
	     {BindableActionMap::LEASH_UNLEASH_CREATURE, BindableActionMap::PREVIOUS_LEASH, BindableActionMap::NEXT_LEASH})
	{
		if (const auto key = creature_leash::KeyFor(action);
		    key.has_value() && actions.GetChanged(action) && actions.Get(action))
		{
			commands.Submit(player_commands::LeashKey {.key = *key});
		}
	}
}

bool Game::UpdateDebugGui()
{
	auto& profiler = Locator::profiler::value();
	const auto& config = Locator::config::value();
	if (!config.running || _quitRequested)
	{
		return false;
	}
	// ImGui events + prepare
	{
		auto guiLoop = profiler.BeginScoped(Profiler::Stage::GuiLoop);
		// The debug windows keep off the mouse while the player's input is locked out, and show a notice
		Locator::debugGui::value().SetInputLock(Locator::gameActionSystem::value().IsPlayerInputBlocked(),
		                                        Locator::gameActionSystem::value().GetScriptedPointer().has_value());
		// The debug menu bar comes up with the game's menu, or always without it
		Locator::debugGui::value().SetMenuBarVisible(!_interface || _interface->GetMenu().IsOpen());
		if (Locator::debugGui::value().Loop())
		{
			return false; // Quit event
		}
	}
	return true;
}

void Game::UpdateCameraControllers(std::chrono::microseconds deltaTime)
{
	auto& profiler = Locator::profiler::value();
	// The in-game editor keeps its camera on what it has picked
	if (Locator::editorSystem::has_value())
	{
		auto editor = profiler.BeginScoped(Profiler::Stage::EditorUpdate);
		Locator::editorSystem::value().Update(deltaTime);
	}

	// Creature Mode keeps the camera on its creature, and the cave follows the temple's creature room
	if (Locator::creatureModeSystem::has_value())
	{
		auto creatureMode = profiler.BeginScoped(Profiler::Stage::CreatureModeUpdate);
		Locator::creatureModeSystem::value().Update(deltaTime, {.handGripping = _handGripping});
	}
	if (Locator::creatureCaveSystem::has_value())
	{
		auto creatureCave = profiler.BeginScoped(Profiler::Stage::CreatureCaveUpdate);
		Locator::creatureCaveSystem::value().Update(std::chrono::duration<float, std::milli>(deltaTime));
	}
	if (Locator::tattooEditorSystem::has_value())
	{
		Locator::tattooEditorSystem::value().Update(std::chrono::duration<float, std::milli>(deltaTime).count());
	}
}

void Game::UpdateCamera()
{
	auto& camera = Locator::camera::value();
	// While a miracle's camera path has the camera, the player's camera doesn't move it
	// A hand demonstration has the camera exactly where its recording had it, and the camera's own moves wait
	const auto demoCamera = Locator::handDemoSystem::has_value() ? Locator::handDemoSystem::value().GetCamera() : std::nullopt;
	if (!Locator::cameraPathSystem::value().HoldsCamera() && !demoCamera.has_value())
	{
		// A script's camera track runs on the game's time; the camera itself steps as the hand does, so that a cut scene
		// holds while the game is paused and keeps pace with the game's speed
		if (auto* scriptCamera = Locator::scriptControlSystem::value().GetScriptCamera(camera); scriptCamera != nullptr)
		{
			scriptCamera->PassGameTime(Locator::time::value().GetFrameGameTime());
		}
		camera.Update(std::chrono::duration_cast<std::chrono::microseconds>(CameraStepTime()));
	}
	// The lens glides on the game's time, and holds while the player is in the temple, whose own lens it has
	if (!(Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		camera.UpdateLens(std::chrono::duration<float>(Locator::time::value().GetFrameGameTime()).count());
	}
	if (demoCamera.has_value())
	{
		camera.SetOrigin(demoCamera->origin).SetFocus(demoCamera->focus);
		if (auto* script = dynamic_cast<ScriptCameraModel*>(&camera.GetModel()); script != nullptr)
		{
			script->SetOrigin(demoCamera->origin);
			script->SetFocus(demoCamera->focus);
		}
	}
	// While the opening's camera chases the falling light, it is drawn from behind the light as the last frame left it,
	// whatever the script's camera does underneath
	if (const auto chase = Locator::introSystem::value().GetCameraView(); chase.has_value())
	{
		camera.SetOrigin(chase->origin).SetFocus(chase->focus);
	}
	FitNearClip();
}

void Game::UpdateTemple(std::chrono::microseconds deltaTime)
{
	// The temple's camera may have taken the player out of the temple
	if (Locator::temple::has_value())
	{
		Locator::temple::value().Update(deltaTime);
		// When the player leaves the save game room after the game is over, the game stops. The program closes. It
		// does not go back to a front end. Because the game is over, it does not write an autosave and does not draw
		// black frames before it closes.
		if (Locator::temple::value().TakeGameEnded())
		{
			RequestQuit();
		}
	}
}

void Game::UpdateInterfaceFrame(std::chrono::microseconds deltaTime)
{
	// The interface reads the places of the pointer that came in, and the help counts each movement of the mouse
	if (Locator::gameActionSystem::has_value())
	{
		const auto moves = Locator::gameActionSystem::value().TakeMouseMoves();
		for (uint32_t i = 0; i < moves && Locator::helpProfileSystem::has_value(); ++i)
		{
			Locator::helpProfileSystem::value().Trigger(help::profile::k_MouseMoved);
		}
	}
	if (_interface)
	{
		// The fighters' health and stamina while a fight is on, wanted afresh every frame whether or not the hand is out
		const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
		const bool scriptHasCamera =
		    Locator::scriptControlSystem::has_value() && Locator::scriptControlSystem::value().GetCameraOwner() != 0;
		_interface->SetFightPanel(creature_fight_hud::PanelAllowed(inTemple, scriptHasCamera)
		                              ? Locator::creatureFightSystem::value().GetPanel()
		                              : std::nullopt);
		_interface->Update(std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
		HandleInterfaceAction();
		UpdateTattooMenu(std::chrono::duration<float, std::milli>(deltaTime).count());
		UpdateMultiplayerFrontEnd();
		if (const auto end = std::exchange(_gameEndAsked, std::nullopt); end.has_value())
		{
			EndGame(*end);
		}
		StartNextGameWhenSaved();
	}
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

void Game::ReportHand()
{
	// The report goes once a turn, after the turns of a frame that played one, and only when something in it changed
	const auto turn = Locator::time::value().GetTurn();
	if (_handReportTurn == turn)
	{
		return;
	}
	_handReportTurn = turn;
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand =
	    Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	const auto* transform = registry.Valid(hand) ? registry.TryGet<const ecs::components::Transform>(hand) : nullptr;
	if (transform == nullptr)
	{
		return;
	}
	// A new hand, as on a new land, has no report yet
	if (hand != _reportedHand)
	{
		_reportedHand = hand;
		_lastHandReport = {};
	}
	const auto& camera = Locator::camera::value();
	auto [collidePoint, handPoint, cameraPoint, focus] = _lastHandReport;
	handPoint = transform->position;
	// The point that the interface picks under the cursor stays as it was while the cursor picks nothing
	if (Locator::pickingSystem::has_value())
	{
		collidePoint = Locator::pickingSystem::value().GetPick().point.value_or(collidePoint);
	}
	cameraPoint = camera.GetOrigin();
	focus = camera.GetFocus();
	const std::array report {collidePoint, handPoint, cameraPoint, focus};
	if (report == _lastHandReport)
	{
		return;
	}
	_lastHandReport = report;
	Locator::playerCommandSystem::value().Submit(player_commands::HandReport {
	    .collidePoint = collidePoint,
	    .handPoint = handPoint,
	    .camera = cameraPoint,
	    .focus = focus,
	});
}

bool Game::PlayTurns()
{
	// The game is the view of the turns
	Locator::landSystem::value().PlayTurns(*this);
	return true;
}

void Game::UpdateCreaturePoses()
{
	simulation::PlayCreaturePoses();
}

void Game::UpdateScreenMarks()
{
	// The numbers and the marks rise and fade only on a frame that draws the land, by the game time of the frame. They
	// stand still while the game pauses.
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool landHidden = Locator::videoSystem::has_value() &&
	                        (Locator::videoSystem::value().CoversScreen() || Locator::videoSystem::value().HidesWorld());
	if (inTemple || landHidden)
	{
		return;
	}
	const auto milliseconds = static_cast<float>(Locator::time::value().GetFrameGameTime().count());
	// The marks over creatures that learn rise, blink and fade
	UpdateLearningMarks(milliseconds);
	// The numbers floating up from things rise and fade
	UpdateFloatingNumbers(milliseconds * 0.001f);
}

void Game::UpdateFrameEffects()
{
	// The game is the view of the effects of the frame
	simulation::PlayFrameEffects(*this);
}

void Game::UpdateAdvisors()
{
	if (!Locator::windowing::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	auto& clock = Locator::time::value();
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const auto realMs = static_cast<int32_t>(clock.GetFrameRealTime().count());
	const auto gameMs = static_cast<int32_t>(clock.GetFrameGameTime().count());
	const auto screenSize = glm::max(Locator::windowing::value().GetSize(), glm::ivec2(1));
	Locator::advisorSystem::value().Update({
	    .camera = &camera,
	    .screen = static_cast<glm::u16vec2>(screenSize),
	    .mouse = _mousePosition,
	    .frameMs = static_cast<uint32_t>(std::max(realMs, 1)),
	    .stepMs = std::clamp(inTemple ? realMs : gameMs, 0, 500),
	    .tickMs = machine_clock::Ticks(),
	    .wideScreen = Locator::cinematicDirectorSystem::value().IsWideScreenOn(),
	});
}

bool Game::TakeDialogueClick()
{
	return std::exchange(_dialogueClick, false);
}

bool Game::DialogueSkipKeyDown() const
{
	// The key comes from the player's keyboard, which the input lock keeps out also when the window has the keys
	const bool playerKeys =
	    !Locator::gameActionSystem::has_value() || !Locator::gameActionSystem::value().IsPlayerInputBlocked();
	const auto* keys = playerKeys ? SDL_GetKeyboardState(nullptr) : nullptr;
	return keys != nullptr && keys[SDL_SCANCODE_KP_ENTER] != 0;
}

void Game::UpdateSingAlong(bool inTemple)
{
	if (!_interface)
	{
		return;
	}
	const auto& clock = Locator::time::value();
	std::optional<int32_t> word;
	if (_gameMusic && help::sing_along::IsSungVerse(static_cast<int32_t>(_gameMusic->GetScriptType())))
	{
		word = _gameMusic->GetMusicLyrics().word;
	}
	_interface->SetSingAlong(word, help::sing_along::StepMilliseconds(inTemple,
	                                                                  static_cast<int32_t>(clock.GetFrameRealTime().count()),
	                                                                  static_cast<int32_t>(clock.GetFrameGameTime().count())));
}

bool Game::DialogueWordsDrawn() const
{
	return _interface && _interface->WereDialogueWordsDrawn();
}

void Game::CloseMenuForCinema()
{
	if (_interface && _interface->GetMenu().IsOpen())
	{
		_interface->GetMenu().Close();
	}
}

void Game::UpdateHandFrame(std::chrono::microseconds deltaTime)
{
	auto& profiler = Locator::profiler::value();
	auto& camera = Locator::camera::value();
	const auto& config = Locator::config::value();
	auto& clock = Locator::time::value();
	const auto gameTime = std::chrono::duration<float, std::milli>(clock.GetFrameGameTime());
	// Update Uniforms
	{
		auto profilerScopedUpdateUniforms = profiler.BeginScoped(Profiler::Stage::UpdateUniforms);

		// The hand follows the pointer through the view that is seen: an inspector's override while one is shown
		ShowInspectorOverride(true);
		// Update Hand and intersection point
		// Upright where nothing is under the cursor
		ecs::components::Transform intersectionTransform {
		    .position = glm::vec3(0.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)};
		// The point the interface picks under the cursor, which the hand's influence is tested at
		std::optional<map_coords::MapCoords> handPick;
		{
			const auto screenSize =
			    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
			const auto scale = glm::vec3(50.0f, 50.0f, 50.0f);
			if (screenSize.x > 0 && screenSize.y > 0)
			{
				glm::vec3 rayOrigin;
				glm::vec3 rayDirection;
				camera.DeprojectScreenToWorld(static_cast<glm::vec2>(_mousePosition) / static_cast<glm::vec2>(screenSize),
				                              rayOrigin, rayDirection);

				_cursorWorldPosition.reset();
				_cursorLand.reset();
				_handRestOnThing.reset();
				if (Locator::temple::has_value() && Locator::temple::value().Active())
				{
					// In the temple, the hand goes where the cursor meets the room, turning to its surface
					if (const auto hit = Locator::temple::value().GetCursorHit())
					{
						const auto seconds = std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count();
						_handTempleNormal.SetDestination(hit->normal, k_HandTempleTurnTime);
						_handTempleNormal.Update(seconds);
						const auto normal = _handTempleNormal.GetValue();
						_cursorWorldPosition = hit->point;
						intersectionTransform.position = hit->point;
						intersectionTransform.rotation =
						    glm::length(normal) > 0.0f
						        ? glm::mat3_cast(glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), glm::normalize(normal)))
						        : glm::mat3(1.0f);
					}
				}
				else if (!glm::any(glm::isnan(rayOrigin) || glm::isnan(rayDirection)))
				{
					// The leash keys, and the Action button tapping leash posts, and with the leash held giving orders and
					// tying the leash to things.
					// Not while the debug windows have the keyboard or mouse, as when typing in a text field: the controls
					// aren't updated then, so a key just pressed would read as pressed again every frame.
					if (!Locator::debugGui::value().StealsFocus())
					{
						auto& leashes = Locator::leashSystem::value();
						leashes.HandleInput(rayOrigin, rayDirection, _actionPressTaken);
						_actionPressTaken = false;
					}
					// The gestures drawn with the hand: circles and power-ups for the miracles, the leash's gestures, and
					// the scribble that shakes off what the hand holds
					{
						auto gestures = profiler.BeginScoped(Profiler::Stage::GestureUpdate);
						UpdateGestures(camera, screenSize,
						               std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
					}
					// What the interface picked under the cursor as the last frame was drawn: the object nearest the camera, or
					// else the land or the sea. The hand's influence is tested at that point.
					const auto& picking = Locator::pickingSystem::value();
					const auto& pick = picking.GetPick();
					_cursorOnObject = pick.object.has_value();
					if (pick.point.has_value())
					{
						handPick = map_coords::FromMetres({pick.point->x, pick.point->z});
					}
					// The hand rests over what was picked: before a villager or an animal, on a model it feels the triangles
					// of, or else on the land, turned to its slope
					const auto [eye, nearPoint] =
					    camera.OnNearPlane(static_cast<glm::vec2>(_mousePosition) / static_cast<glm::vec2>(screenSize));
					const auto cursorDirection =
					    hand_feel::CursorDirection(eye, nearPoint, hand_feel::HoverLimit(HandHoverInputs(eye)));
					_handSurfaceUp.reset();
					_handOverObject = false;
					_handRestNearer = 0.0f;
					_handOverSpellIcon = false;
					// Over the help's text bubble or the leash's rope, the hand stands straight up; over the bubble it
					// eases to the bubble's distance. While the hand holds the rope, the rope counts as under the cursor.
					const bool rope = pick.rope || Locator::leashSystem::value().RopeHand(PlayerNames::PLAYER_ONE) ==
					                                   leash_snap::Hand::Holding;
					_handStandsStraight = !pick.object.has_value() && (pick.textBubble || rope);
					_handBubbleDistance =
					    !pick.object.has_value() && pick.textBubble && !rope ? std::optional(pick.distance) : std::nullopt;
					// Only the empty hand offers to turn the camera
					_handMayOfferTurn = !pick.object.has_value() && !pick.textBubble && !rope &&
					                    !Locator::handGrabSystem::value().GetHeld().has_value();
					std::optional<glm::vec3> rest;
					if (pick.object.has_value())
					{
						const auto& registry = Locator::entitiesRegistry::value();
						const auto object = *pick.object;
						const auto& grab = Locator::handGrabSystem::value();
						const auto held = grab.GetHeld();
						const auto* mesh = registry.TryGet<const ecs::components::Mesh>(object);
						auto& meshes = Locator::resources::value().GetMeshes();
						const bool posed = mesh != nullptr && meshes.Contains(mesh->id) && meshes.Handle(mesh->id)->IsBoned();
						const auto feel = hand_feel::FeelOf({
						    .living =
						        registry.AnyOf<ecs::components::Villager, ecs::components::Animal, ecs::components::Creature>(
						            object),
						    .creature = registry.AllOf<ecs::components::Creature>(object),
						    .posedModel = posed,
						    .animatedStatic = registry.AllOf<ecs::components::AnimatedStatic>(object),
						    .ignored = registry.AllOf<ecs::components::CarriedByTornado>(object) || held == object,
						    .spellIcon = registry.AllOf<ecs::components::IconSeed>(object),
						});
						// Holding something, the hand feels along the line to the cursor's point on the land, raised by
						// its lift for what it holds
						std::optional<hand_feel::RaisedGround> raisedGround;
						if (held.has_value())
						{
							raisedGround = hand_feel::RaisedGround {.ground = pick.land.value_or(_handPosition),
							                                        .lift = grab.GetCursorRaise()};
						}
						const auto feltDirection = hand_feel::FeelDirection(eye, cursorDirection, raisedGround);
						if (feel == hand_feel::Feel::AtIcon)
						{
							// The hand rests nearer than a miracle's icon by its own height, turned to the land
							rest = registry.Get<const ecs::components::Transform>(object).position;
							_handRestNearer = hand_feel::k_IconGap;
							_handOverSpellIcon = true;
						}
						else if (feel == hand_feel::Feel::AtPosition)
						{
							rest = hand_feel::RestAtPosition(eye, nearPoint, feltDirection,
							                                 registry.Get<const ecs::components::Transform>(object).position,
							                                 ecs::systems::object_measures::TwoDRadius(registry, object));
							_handOverObject = true;
						}
						else if (feel == hand_feel::Feel::OnModel)
						{
							_handOverObject = true;
							if (const auto felt = picking.FeelModel(object, eye, feltDirection))
							{
								std::optional<hand_feel::Holding> holding;
								if (held.has_value())
								{
									holding = hand_feel::Holding {
									    .radius = ecs::systems::object_measures::TwoDRadius(registry, *held),
									    .creature = std::nullopt,
									};
									if (registry.AllOf<ecs::components::Creature>(object))
									{
										const auto& at = registry.Get<const ecs::components::Transform>(object).position;
										holding->creature = hand_feel::CreatureReach {
										    .centre = {at.x, at.z},
										    .radius = ecs::systems::object_measures::TwoDRadius(registry, object),
										};
									}
								}
								const auto onModel = hand_feel::RestOnModel(eye, cursorDirection, feltDirection, felt->point,
								                                            felt->normal, FeelsModel(object), holding);
								rest = onModel.point;
								_handSurfaceUp = onModel.up;
							}
						}
					}
					_cursorLand = pick.land;
					_handRestOnThing = rest;
					// The cursor's point at the furthest the hand may be this frame, which it stands towards the camera's
					// focus from
					_handLimitPoint = hand_feel::PointAlongCursor(eye, nearPoint, hand_feel::HoverLimit(HandHoverInputs(eye)));
					const auto restPoint = rest.has_value() ? rest : pick.point;
					if (restPoint.has_value())
					{
						const auto up = _handSurfaceUp.has_value()
						                    ? glm::normalize(*_handSurfaceUp)
						                    : Locator::terrainSystem::value().GetNormalAt({restPoint->x, restPoint->z});
						intersectionTransform.position = *restPoint;
						intersectionTransform.rotation = glm::mat3_cast(glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), up));
						_cursorWorldPosition = *restPoint;
					}
				}
				intersectionTransform.scale = scale;
			}
		}

		// The hand's debugging trace records this frame, starting from where the hand was left before it
		if (auto* trace = Locator::handSystem::value().GetTrace(); trace != nullptr && trace->Enabled())
		{
			trace->BeginFrame(_frameCount, std::chrono::duration<float>(deltaTime).count());
			const auto handEntity = Locator::handSystem::value()
			                            .GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
			if (const auto* transform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(handEntity))
			{
				trace->Note("frame start", transform->position);
			}
		}

		// Update Hand
		{
			// Put away, it stays where it was and does nothing until it comes back. The creature's panel is wanted afresh
			// every frame, so with the hand away (a cut scene, a menu) it isn't left up from before
			if (!IsHandDrawn() && _interface)
			{
				_interface->SetCreaturePanel(std::nullopt);
			}
			if (IsHandDrawn())
			{
				const glm::mat4 modelRotationCorrection = glm::eulerAngleX(glm::radians(90.0f));

				const auto handEntity =
				    Locator::handSystem::value()
				        .GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
				auto& handTransform = Locator::entitiesRegistry::value().Get<ecs::components::Transform>(handEntity);
				if (!_stillHandAt.has_value())
				{
					_stillHandAt = handTransform.position;
				}
				UpdateHandNavigation(handTransform);
				UpdateHandKnock(handTransform);
				if (Locator::temple::has_value() && Locator::temple::value().Active())
				{
					if (!_handGripping)
					{
						handTransform.rotation = glm::eulerAngleY(camera.GetRotation().y) * modelRotationCorrection;
						handTransform.rotation = intersectionTransform.rotation * handTransform.rotation;
					}
				}
				else
				{
					OrientHand(handTransform, glm::mat3(glm::eulerAngleY(camera.GetRotation().y) * modelRotationCorrection),
					           _handSurfaceUp.value_or(glm::vec3(0.0f, 1.0f, 0.0f)),
					           std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
				}
				PlaceHand(handTransform, std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
				TraceHand("navigation", handTransform.position);
				// A tool's run can keep the hand where it started, so that the hand gives no input
				if (Locator::simulation::value().GetSettings().stillHand)
				{
					handTransform.position = _stillHandAt.value_or(handTransform.position);
				}
				// Held to a creature, the hand rests on its body under the cursor, stroking and slapping it
				{
					auto creatureHand = profiler.BeginScoped(Profiler::Stage::CreatureHandUpdate);
					const auto screenSize =
					    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
					_handOnCreature.reset();
					_creatureUnderHand.reset();
					if (screenSize.x > 0 && screenSize.y > 0)
					{
						auto& hands = Locator::creatureHandSystem::value();
						glm::vec3 rayOrigin;
						glm::vec3 rayDirection;
						camera.DeprojectScreenToWorld(static_cast<glm::vec2>(_mousePosition) /
						                                  static_cast<glm::vec2>(screenSize),
						                              rayOrigin, rayDirection);
						// The hand isn't over the world while it is over a debug window, or in the temple
						const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
						if (!inTemple && !Locator::debugGui::value().IsMouseOverWindow())
						{
							_creatureUnderHand = hands.CreatureUnderCursor();
						}
						if (hands.GetCreature().has_value())
						{
							_handOnCreature = hands.Update(rayOrigin, rayDirection, static_cast<glm::vec2>(_mousePosition),
							                               static_cast<float>(screenSize.x), HandStepSeconds());
						}
						if (_handOnCreature.has_value())
						{
							handTransform.position = _handOnCreature->position;
							// Its back turns to the body it rests on, or stands it upright beside it
							handTransform.rotation =
							    hand_orientation::StandOnSlope(_handOnLevelLand, _handHeading, _handOnCreature->up);
							TraceHand("on creature", handTransform.position);
						}
					}
					UpdateHandInterface();
				}
				{
					auto magic = profiler.BeginScoped(Profiler::Stage::MagicUpdate);
					// Food and wood pouring from the hand lift it and tip it forward, and what it pours comes from there
					const auto pour = Locator::magicSystem::value().GetHandPour(Locator::time::value().GetTurnFraction());
					// The hand is drawn where a scenario puts it, and stays where a pour that holds it began
					const auto driven = Locator::magicSystem::value().GetDrivenHand();
					if (driven.has_value())
					{
						handTransform.position = driven->handPosition;
					}
					handTransform.position = pour.pinned.value_or(handTransform.position);
					handTransform.position.y += pour.raise;
					TraceHand("scenario or pour", handTransform.position);
					// A seed in the hand lifts it, by how it is held; the pour tips it as the hand is posed
					// (magic::HandHoldPoser)
					if (const auto held = magic::HandHoldPoser::Find())
					{
						// Measured to the land under the cursor, or under the hand a scenario puts
						const auto land = driven.has_value() ? driven->point : _cursorWorldPosition;
						handTransform.position.y += magic::HandHoldPoser::Lift(
						    *held,
						    x87::Distance<x87::SumOrder::Zyx>(camera.GetOrigin(), land.value_or(handTransform.position)));
						TraceHand("seed lift", handTransform.position);
					}
					else if (const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize()
					                                                                 : glm::zero<glm::ivec2>();
					         Locator::handGrabSystem::has_value() && screenSize.x > 0 && screenSize.y > 0)
					{
						// A thing the hand takes or holds lifts it, and makes ready to throw, its spring drags it
						glm::vec3 rayOrigin;
						glm::vec3 rayDirection;
						camera.DeprojectScreenToWorld(static_cast<glm::vec2>(_mousePosition) /
						                                  static_cast<glm::vec2>(screenSize),
						                              rayOrigin, rayDirection);
						const auto land = _cursorWorldPosition.value_or(handTransform.position);
						handTransform.position = Locator::handGrabSystem::value().UpdateFrame({
						    .target = handTransform.position,
						    .rayOrigin = rayOrigin,
						    .rayDirection = rayDirection,
						    .camera = camera.GetOrigin(),
						    .cursorGround = _cursorWorldPosition,
						    .handSize =
						        HandAnimation::SizeAtDistance(x87::Distance<x87::SumOrder::Zyx>(camera.GetOrigin(), land)),
						    .seconds = std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count(),
						    .gameMs = static_cast<uint32_t>(std::lround(gameTime.count())),
						    .nowMs = machine_clock::Ticks(),
						    .turn = Locator::time::value().GetTurn(),
						});
						TraceHand("grab", handTransform.position);
					}
					UpdateMagicHand(handTransform.position,
					                std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
				}
				// Holding a town's totem, the mouse slides it up and down and the hand stays on its icon
				if (Locator::villageTotemSystem::has_value() && Locator::villageTotemSystem::value().GetGripped().has_value())
				{
					auto& totems = Locator::villageTotemSystem::value();
					const auto screenSize =
					    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(0);
					// The pointer is put back on the hand every frame, so how far it went from there is how far the mouse
					// moved
					const int up = _totemPointer.has_value() ? _totemPointer->y - _mousePosition.y : 0;
					totems.Slide(static_cast<float>(up), static_cast<float>(screenSize.y));
					if (const auto hold = totems.GetHandHold())
					{
						handTransform.position = hold->position;
						TraceHand("totem", handTransform.position);
						// It faces the way it always does, tipped forwards over the icon
						handTransform.rotation =
						    hand_orientation::TippedHand(hand_orientation::TippedRows(_handHeadingAngle, hold->tilt));
						// The pointer goes where the hand is on the screen, while it is on it
						glm::vec3 screen;
						if (camera.ProjectWorldToScreen(hold->position, glm::vec4(0.0f, 0.0f, glm::vec2(screenSize)), screen))
						{
							const glm::ivec2 at {std::lrint(screen.x), std::lrint(screen.y)};
							if (at.x >= 0 && at.y >= 0 && at.x < screenSize.x && at.y < screenSize.y)
							{
								// Only put when it isn't there already
								if (at != _mousePosition)
								{
									Locator::gameActionSystem::value().WarpCursor(at);
									_mousePosition = at;
									_handPointer.Moved();
								}
								_totemPointer = at;
							}
						}
					}
				}
				UpdateMagicHand(handTransform.position,
				                std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
			}
			// A fireball pressed long enough is taken, which catches it
			Locator::magicSystem::value().HoldFireBall(machine_clock::Ticks(), Locator::time::value().GetTurn());
			{
				// The hand and the piles show their miracles
				auto miracleFx = profiler.BeginScoped(Profiler::Stage::MiracleFxUpdate);
				Locator::miracleFxSystem::value().Update(
				    std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count(),
				    std::chrono::duration<float>(gameTime).count());
			}
			Locator::entitiesRegistry::value().SetDirty();
		}

		// The leashes' ropes swing from where the hand now is
		{
			auto creatureLeash = profiler.BeginScoped(Profiler::Stage::CreatureLeashUpdate);
			Locator::leashSystem::value().Update(std::chrono::duration<float>(gameTime).count());
		}

		// Holding a miracle's seed, the hand takes the still pose of its hold, sways with the seed and tips with a pour. A
		// thing it picked up it holds the same way.
		const auto heldSeed = magic::HandHoldPoser::Find();
		auto heldThing = heldSeed;
		if (!heldThing.has_value() && Locator::handGrabSystem::has_value())
		{
			if (const auto held = Locator::handGrabSystem::value().GetHeldPose())
			{
				heldThing = magic::HandHoldPoser::HeldSeed {
				    .entity = held->object,
				    .hold = held->hold,
				    .hang = held->hang,
				    .reach = held->reach,
				    .yRotate = 0.0f,
				    .effectInFingers = false,
				};
			}
		}
		bool holdingSeed = false;
		{
			const auto handEntity = Locator::handSystem::value()
			                            .GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
			if (auto* handTransform = Locator::entitiesRegistry::value().TryGet<ecs::components::Transform>(handEntity))
			{
				const magic::HandHoldPoser::Frame holdFrame {
				    .dt = deltaTime,
				    .cursor = _mousePosition,
				    .camera = camera.GetOrigin(),
				    .heading = _handHeadingAngle,
				    .modelCorrection = glm::mat3(glm::eulerAngleX(glm::radians(90.0f))),
				    // A pour tips the hand; offering what it holds to a creature turns it over too
				    .tilt = Locator::magicSystem::value().GetHandPour(Locator::time::value().GetTurnFraction()).tilt +
				            (heldSeed.has_value() || !Locator::handGrabSystem::has_value()
				                 ? 0.0f
				                 : Locator::handGrabSystem::value().GetOfferTurn()),
				    // A thing the hand picked up isn't turned about for a right hand as a seed is
				    .handScale = HandDrawnScale(camera.GetOrigin(), handTransform->position, heldThing.has_value()),
				    .rightHanded = heldSeed.has_value() && config.rightHandedHand,
				};
				holdingSeed = _handHold.Pose(heldThing, holdFrame, _handAnimation.get(), *handTransform);
			}
		}
		{
			// The globes show their miracles where they are now, a globe in the hand where the hand holds it, each turned
			// to face the camera as it is drawn
			auto miracleGlobes = profiler.BeginScoped(Profiler::Stage::MiracleGlobesUpdate);
			Locator::miracleFxSystem::value().UpdateGlobes(std::chrono::duration<float>(gameTime).count());
		}

		// The animation of the hand in this frame, and what is below the hand, for the blend of the drawn hand
		auto handCycle = HandAnimation::Cycle::Wiggle;
		bool nothingUnderHand = true;
		bool gripsTotem = false;
		bool pulls = false;
		// Animate the hand: gripping while it drags the land, otherwise its normal pose. Turning the camera with the
		// middle button leaves the hand idle.
		if (_handAnimation)
		{
			using HandState = HandAnimation::State;
			using HandCycle = HandAnimation::Cycle;
			using hand_navigation_pose::Pose;
			// Held to a creature the hand is in its own state, which plays its cycle without leaning
			const auto state = _handOnCreature.has_value() ? HandState::Creature
			                   : _handCameraState          ? HandState::Camera
			                                               : HandState::Normal;
			// Over the rope of the tied leash the fingers open to pinch it, and holding it they pinch it
			const auto ropeHand = Locator::leashSystem::value().RopeHand(PlayerNames::PLAYER_ONE);
			const auto cycle = hand_cycle::Choose({
			    .pose = _handPose,
			    .cameraState = _handCameraState,
			    .overThing = HandOverThing(),
			    .overSpellIcon = _handOverSpellIcon,
			    .rope = ropeHand,
			    .creature = _handOnCreature.has_value() ? std::optional(_handOnCreature->cycle) : std::nullopt,
			});
			// Pulling a thing free, the hand takes the pose the thing is held with
			std::optional<ecs::systems::HandGrabSystemInterface::PullPose> pull;
			if (!holdingSeed && Locator::handGrabSystem::has_value())
			{
				pull = Locator::handGrabSystem::value().GetPullPose();
			}
			const auto pullCycle = pull.has_value() ? magic::hand_hold::HoldCycle(pull->hold) : std::nullopt;
			const auto* pullClip =
			    pullCycle.has_value() ? _handAnimation->GetAnimation(static_cast<size_t>(*pullCycle)) : nullptr;
			// Holding a town's totem, the hand takes its side hold on the icon, closed by how wide the icon is
			const auto totemHold =
			    Locator::villageTotemSystem::has_value() ? Locator::villageTotemSystem::value().GetHandHold() : std::nullopt;
			handCycle = cycle;
			gripsTotem = totemHold.has_value();
			pulls = pull.has_value();
			nothingUnderHand =
			    ropeHand == leash_snap::Hand::Away &&
			    !(Locator::magicSystem::has_value() && Locator::magicSystem::value().OrbUnderHand().has_value()) &&
			    !(Locator::pickingSystem::has_value() && Locator::pickingSystem::value().GetPick().object.has_value());
			const auto* sideHold =
			    totemHold.has_value() ? _handAnimation->GetAnimation(static_cast<size_t>(HandCycle::HoldSide)) : nullptr;
			if (sideHold != nullptr)
			{
				_handAnimation->UpdateHeld(deltaTime, HandCycle::HoldSide,
				                           ecs::village_totem::GripTimeMs(totemHold->closure, sideHold->duration),
				                           _mousePosition);
			}
			else if (pullClip != nullptr)
			{
				const float handSize = HandAnimation::SizeAtDistance(
				    magic::hand_hold::DistanceFromView(camera.GetOrigin(), _handPosition, heldThing.has_value()));
				_handAnimation->UpdateHeld(deltaTime, *pullCycle,
				                           magic::hand_hold::TugTimeMs(pull->hold, pullClip->duration, pull->reach, handSize),
				                           _mousePosition);
			}
			else if (_handKnocking)
			{
				// Knocking on a house, the hand plays its tap once through, without leaning
				const auto* tap = _handAnimation->GetAnimation(static_cast<size_t>(HandCycle::TapHouse));
				const auto timeMs = _handKnock.Play(deltaTime, tap != nullptr ? tap->duration : 0);
				_handAnimation->UpdateHeld(deltaTime, HandCycle::TapHouse, timeMs, _mousePosition);
				_handAnimation->SettleCursor(_mousePosition);
			}
			else if (!holdingSeed)
			{
				_handAnimation->Update(deltaTime, state, cycle, _mousePosition, leash_snap::PinchShare(ropeHand));
			}

			const auto handEntity = Locator::handSystem::value()
			                            .GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
			auto& registry = Locator::entitiesRegistry::value();
			auto* hand = registry.TryGet<ecs::components::Hand>(handEntity);
			if (hand != nullptr)
			{
				hand->boneMatrices = _handAnimation->GetBoneMatrices();
			}
			// The hand keeps about the same size on screen however far away it is
			if (auto* handTransform = registry.TryGet<ecs::components::Transform>(handEntity))
			{
				// The game mirrors the mesh's left hand along its x axis to make a right hand
				const auto scale = HandDrawnScale(camera.GetOrigin(), handTransform->position, heldThing.has_value());
				handTransform->scale = glm::vec3(config.rightHandedHand ? -scale : scale, scale, scale);
				// Showing it turns the camera as it drags the land, the hand is drawn a third of its height higher, the
				// place it holds staying where it is
				if (hand != nullptr && _handCameraState && _handPose == Pose::Rotate &&
				    glm::determinant(handTransform->rotation) != 0.0f && scale != 0.0f)
				{
					const auto lift =
					    hand_navigation_pose::k_RotateLiftShare * k_HandHeight * HandAnimation::SizeAtDistance(_handDistance);
					const auto toModel =
					    glm::inverse(handTransform->rotation * glm::mat3(glm::scale(glm::mat4(1.0f), handTransform->scale)));
					const auto raise = glm::translate(glm::mat4(1.0f), toModel * glm::vec3(0.0f, lift, 0.0f));
					for (auto& bone : hand->boneMatrices)
					{
						bone = raise * bone;
					}
				}
			}
		}
		// What the hand does in this frame, which starts its blends and decides whether the camera's icons show
		const auto mode = hand_blend::ModeOf({
		    .shown = IsHandDrawn(),
		    .insideTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
		    .holds = heldThing.has_value(),
		    .holdsSeed = heldSeed.has_value(),
		    .pulls = pulls,
		    .knocks = _handKnocking,
		    .gripsTotem = gripsTotem,
		    .touchesCreature = _handOnCreature.has_value(),
		    .movesCamera = _handCameraState,
		});
		// The camera's helper icons by the hand, where it is drawn this frame, now that it is posed
		UpdateHandTricons(IsHandDrawn(), std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count(), mode);
		// The tooltip by the drawn hand too, which stays with it while it is held still and the pointer moves on
		UpdateHandToolTipAnchor();
		// Taking or letting go of a seed cross-fades the drawn hand, and the seed's in-hand effect sits in its fingers
		{
			const auto handEntity = Locator::handSystem::value()
			                            .GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
			if (auto* handTransform = Locator::entitiesRegistry::value().TryGet<ecs::components::Transform>(handEntity))
			{
				const float handSize = HandAnimation::SizeAtDistance(
				    magic::hand_hold::DistanceFromView(camera.GetOrigin(), handTransform->position, heldThing.has_value()));
				// Each change of what the hand does starts a blend of the drawn hand from how it looked before
				const auto* grab = Locator::handGrabSystem::has_value() ? &Locator::handGrabSystem::value() : nullptr;
				const auto changes = _handBlend.Step({
				    .mode = mode,
				    .cycle = handCycle,
				    .held = heldThing.has_value() ? std::optional(heldThing->entity) : std::nullopt,
				    .nothingUnderHand = nothingUnderHand,
				});
				const auto placed = handTransform->position;
				if (auto* hand = Locator::entitiesRegistry::value().TryGet<ecs::components::Hand>(handEntity))
				{
					// Only a thing in the hand hangs from the blended place of the hand, not a seed
					_handHold.Fade(changes, mode != hand_blend::HandMode::Hidden, mode == hand_blend::HandMode::Holding,
					               heldThing.has_value() ? heldThing->entity : entt::null, HandStepSeconds(), *handTransform,
					               hand->boneMatrices);
				}
				TraceHand("stage fade", handTransform->position);
				EndHandTrace(placed, *handTransform, mode, grab);
				// The land behind the hand's middle, as posed for this frame, which the hand reaches about when pressed
				// with nothing under the cursor
				if (Locator::pickingSystem::has_value())
				{
					std::optional<glm::vec3> middle;
					if (const auto* hand = Locator::entitiesRegistry::value().TryGet<ecs::components::Hand>(handEntity))
					{
						const auto model = glm::translate(glm::mat4(1.0f), handTransform->position) *
						                   glm::mat4(handTransform->rotation) *
						                   glm::scale(glm::mat4(1.0f), handTransform->scale);
						std::vector<glm::vec3> bonePlaces;
						bonePlaces.reserve(hand->boneMatrices.size());
						for (const auto& bone : hand->boneMatrices)
						{
							bonePlaces.emplace_back(model * bone[3]);
						}
						middle = hand_reach::Middle(bonePlaces);
					}
					Locator::pickingSystem::value().FindBehindHand(camera.GetOrigin(), middle);
				}
				_handHold.PlaceHandEffect(heldSeed, _handAnimation.get(), *handTransform, handSize);
				// The trails' sheets of light and the chain behind the gesturing hand move on every frame
				Locator::particleSystem::value().UpdateFrame(
				    std::chrono::duration<float>(gameTime).count(),
				    {.position = handTransform->position,
				     .size = handSize,
				     .cameraPosition = camera.GetOrigin(),
				     .gesturing = Locator::gestureSystem::has_value() && Locator::gestureSystem::value().IsGesturing(),
				     .selecting = Locator::gestureSystem::has_value() && Locator::gestureSystem::value().IsSelecting()});
			}
			// The temples' leashes, the picked one round the wrist of the hand as it is drawn, after the blend of its pose
			Locator::leashSystem::value().ShowPosts(IsHandDrawn());
			// A tribe's power behind a miracle spins its name round the hand, or up from where it was cast
			Locator::miracleFxSystem::value().UpdateTribalPower(std::chrono::duration<float>(gameTime).count());
		}

		// The hand shows its player's alignment, catching up with it each frame once it has moved far enough. Its skin is
		// blended again as it goes into or out of the player's influence, tested where the cursor picks the land. The
		// point is held while the hand grips the land, and in the temple.
		// TODO(raffclar): what the game picks in the temple, which needs it run under a debugger
		{
			const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
			Locator::handSystem::value().UpdateAlignmentMorph(handPick, _handGripping || inTemple);
		}

		// The trees bend away from where the hand now is, and rustle
		{
			auto actions = profiler.BeginScoped(Profiler::Stage::VegetationUpdate);
			auto& vegetation = Locator::vegetation::value();
			vegetation.UpdateBendPoints(IsHandDrawn());
			vegetation.Rustle(gameTime);
		}

		ShowInspectorOverride(false);
	}
}

void Game::UpdateEntitiesFrame(std::chrono::microseconds deltaTime)
{
	auto& profiler = Locator::profiler::value();
	auto& config = Locator::config::value();
	auto& clock = Locator::time::value();
	const auto gameTime = std::chrono::duration<float, std::milli>(clock.GetFrameGameTime());
	{
		// Update Entities
		{
			auto updateEntities = profiler.BeginScoped(Profiler::Stage::UpdateEntities);
			// The scrolls and signs spin, scale and glow towards the camera, and their glints play. The gates and the other
			// scenery that the scripts open and close play on, and the stones of the plinths sit or sink. Both use the draw
			// list of this frame.
			const auto updateDrawnScenery = [&clock, &gameTime]() {
				if (Locator::scriptHighlightSystem::has_value())
				{
					Locator::scriptHighlightSystem::value().UpdateFrame(
					    std::chrono::duration<float, std::milli>(gameTime).count(), clock.GetTurnFraction(),
					    Locator::camera::value().GetOrigin());
				}
				Locator::animatedStaticSystem::value().Update(clock.GetFrameGameTime());
			};
			if (config.drawEntities)
			{
				// The villagers in view are posed for the camera the frame is drawn from
				ShowInspectorCamera(true);
				Locator::livingActionSystem::value().PoseVillagersInView(Locator::camera::value().GetViewProjectionMatrix());
				// The eyes of the villagers in high detail blink and turn by the game time of the frame, once each frame
				Locator::highDetailSystem::value().PlaceEyes(static_cast<uint32_t>(clock.GetFrameGameTime().count()),
				                                             Locator::camera::value().GetViewProjectionMatrix());
				// The villagers that swim in high detail leave rings on the water, on a frame that draws the land
				if (!Locator::temple::has_value() || !Locator::temple::value().Active())
				{
					Locator::highDetailSystem::value().LeaveSwimmingRings(
					    static_cast<uint32_t>(clock.GetFrameGameTime().count()));
				}
				// The trees out of the land are drawn with their roots, as each now is
				ecs::tree_roots::Show(Locator::entitiesRegistry::value());
				// The land's draw list, from the camera the frame is drawn from, is made again when it is due: what stands
				// on the blocks beyond it isn't drawn
				if (Locator::objectDrawListSystem::has_value())
				{
					Locator::objectDrawListSystem::value().Update(clock.GetTurn());
					// The things that the list draws show a little more or less of the snow where they stand
					if (Locator::snowSystem::has_value())
					{
						Locator::snowSystem::value().StepObjects(Locator::entitiesRegistry::value(),
						                                         Locator::objectDrawListSystem::value(),
						                                         clock.GetFrameRealTime(), clock.GetFrameGameTime());
					}
					// The towns' buildings that the list draws can make their sounds
					if (Locator::abodeSoundSystem::has_value())
					{
						Locator::abodeSoundSystem::value().Update(
						    std::chrono::duration<float, std::milli>(clock.GetFrameGameTime()));
					}
				}
				updateDrawnScenery();
				// The static of each fizzing creature slides only when the frame draws its body on the screen
				Locator::creatureFizzSystem::value().SlideDrawnStatic(
				    std::chrono::duration<float>(clock.GetFrameGameTime()).count());
				// The fast simulation mode makes every frame ready, also the ones it does not draw: the game's things go
				// by what the frames make ready, so a frame left out would change the run
				Locator::rendereringSystem::value().PrepareDraw(config.drawBoundingBoxes, config.drawFootpaths,
				                                                config.drawStreams);
				ShowInspectorCamera(false);
				// The interface picks what is under the cursor as the frame is drawn, for the next frame to go by: through
				// an inspector's override while one is shown, as the frame is seen
				ShowInspectorOverride(true);
				PickUnderCursor(std::chrono::duration<float>(deltaTime).count());
				ShowInspectorOverride(false);
			}
			else
			{
				updateDrawnScenery();
			}
		}
	}
}

void Game::UpdateCameraKeys(std::chrono::microseconds deltaTime)
{
	// In the temple, without the interface or with a box on the screen, the camera does not read its keys
	const bool interfaceActive =
	    !Locator::cinematicDirectorSystem::has_value() || Locator::cinematicDirectorSystem::value().IsInterfaceActive();
	if (!game_frame::CameraReadsKeys({
	        .inTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
	        .interfaceActive = interfaceActive,
	        .boxShows = FrontEndBoxShows(),
	    }))
	{
		return;
	}
	auto& camera = Locator::camera::value();
	// The keys of the camera act after the camera moved, so that their steps move it in the next frame
	auto& actions = Locator::gameActionSystem::value();
	// A miracle's camera path keeps the camera until the player moves it with the movement keys or grips the land,
	// which hand it straight back; meanwhile the player's other camera controls do nothing
	auto& cameraPaths = Locator::cameraPathSystem::value();
	const bool pathHeld = cameraPaths.HoldsCamera();
	cameraPaths.HandlePlayerControl(
	    {.movementKey = actions.GetAny(input::BindableActionMap::MOVE_LEFT, input::BindableActionMap::MOVE_RIGHT,
	                                   input::BindableActionMap::MOVE_FORWARDS, input::BindableActionMap::MOVE_BACKWARDS),
	     .frameMilliseconds = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(deltaTime).count()),
	     .grippingLand = _handGripping && !_handRotating});
	// The camera that the path gives back reads the keys from the next frame
	if (!pathHeld)
	{
		camera.HandleKeys(deltaTime);
	}
	// A grip of the land ends the camera of a dance. A zoom with the land held does not end it.
	hand::script_state::Facts grip {.gripsLand = _handSelectGrip, .otherButton = _handOtherButton};
	if (Locator::cameraHelpSystem::has_value())
	{
		grip.cameraFeatures = Locator::cameraHelpSystem::value().Get().features;
	}
	if (Locator::danceSystem::has_value())
	{
		Locator::danceSystem::value().ProcessCameraKeys(hand::script_state::StateOf(grip) ==
		                                                hand::script_state::State::GripLandscape);
	}
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
	auto& events = Locator::events::value();

	events.AddHandler(std::function([this, &config](const SDL_Event& event) {
		// If gui captures this input, do not propagate
		if (!Locator::debugGui::value().ProcessEvents(event))
		{
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
			// Escape in play, with no dialog open
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && _interface && !_interface->IsDialogOpen() &&
			    !_interface->GetTattooEditor().IsOpen())
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
		if (_simulation->frameTime.has_value())
		{
			Locator::time::value().SetFixedFrameTime(*_simulation->frameTime);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Simulation run: fast {}, frame time {} us, draw every {}, until turn {}",
		                   _simulation->settings.fast, _simulation->frameTime.value_or(std::chrono::microseconds {0}).count(),
		                   _simulation->settings.drawEvery, _simulation->settings.untilTurn.value_or(0));
	}
	if (_networkGame)
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
		if (StartInspector(*_inspectPort))
		{
			loadingData.emplace("game data");
			Locator::gameActionSystem::value().SetInputLockMode(_inspectInputLock);
			StartFightTools();
		}
	}

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
		// The Game Statistics box takes the players' figures as it opens
		_interface->GetMenu().SetStatisticsSource([] {
			const auto local =
			    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
			return std::pair(player_statistics::Box(), local);
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
		                                                                  _simulation->limitsPath, _simulation->hashParts);
		    error.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The simulation record cannot start: {}", *error);
		}
	}
	// A tool's run can give the hand recorded input from the first frame of the land
	if (_simulation.has_value() && _simulation->inputPath.has_value() && Locator::inspector::has_value())
	{
		if (const auto error = Locator::inspector::value().ReplayOnLoad(*_simulation->inputPath); error.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The recorded input cannot start: {}", *error);
		}
	}
	// A tool's run can write the commands of each turn, for a dedicated server that does the same commands
	if (_simulation.has_value() && _simulation->commandsPath.has_value())
	{
		_commandLog = std::make_unique<command_log::Writer>(*_simulation->commandsPath);
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

	// The game has no skirmish box yet, so that switch starts the game as with no switch. The -MULTIPLAYER switch opens
	// the multiplayer front end before the first frame, in the temple of the land that starts, when the player has a
	// profile.
	switch (launch_switches::FirstGameOf(_launchSwitches))
	{
	case launch_switches::FirstGame::Skirmish:
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The -SKIRMISH switch: the skirmish box is not available yet");
		break;
	case launch_switches::FirstGame::Multiplayer:
	case launch_switches::FirstGame::Plain:
	case launch_switches::FirstGame::NewGame:
		break;
	}

	auto start = game_session::Start::NewGame;
	if (_startTestbed)
	{
		LoadTestbed();
	}
	else
	{
		// A first start: the new player is asked their name before the first land; closing the window meanwhile ends
		// the game at its first frame. A first start always starts a new game.
		const bool hadProfile = HasPlayerProfiles();
		// With no player profile, the -MULTIPLAYER switch starts a new game as with no switch
		_frontEndOnStart =
		    hadProfile && launch_switches::FirstGameOf(_launchSwitches) == launch_switches::FirstGame::Multiplayer;
		AskForNewPlayer();
		// A plain start continues from the save on exit of the player. The continue loads the save once the scripts are
		// ready, because the save keeps the state of the scripts.
		start = game_session::FirstStart({.hadProfile = hadProfile,
		                                  .developerRun = _automatedRun,
		                                  .newGameAsked = _newGameAsked,
		                                  .continueAsked = _continueAsked});
		start = game_session::Resolve(start,
		                              Locator::saveGameSystem::has_value() && Locator::saveGameSystem::value().HasExitSave());
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
	// start-of-game question isn't asked
	auto& lands = Locator::landSystem::value();
	const bool startStory = !_startTestbed && start == game_session::Start::NewGame &&
	                        new_game_choice::FirstLandStartsStory(Locator::entitiesRegistry::value().Context().skirmish);
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
	simulation::BeginPlay(continued, _simulation.has_value() && _simulation->frameTime.has_value(), *this);
	_frameCount = 0;
	_frameLoop = std::make_unique<FrameLoop>();
	_frameLoop->lastTime = std::chrono::high_resolution_clock::now();
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
	if (_playVideo == "intro")
	{
		Locator::videoSystem::value().Play("Data/INTRO.bik");
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
	// The first game of the program starts with the logo effect and its sound, before the menu shows
	if (startup::PlaysLogoEffect(_automatedRun, _logoEffectAsked, false))
	{
		PlayLogoEffect();
	}
	// The -MULTIPLAYER switch: the multiplayer front end opens in place of the menu
	if (std::exchange(_frontEndOnStart, false))
	{
		if (OpenMultiplayerFrontEnd(MakeMultiplayerFrontEnd()))
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The -MULTIPLAYER switch: the multiplayer front end opens");
			return;
		}
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The multiplayer front end needs the temple and the interface");
	}
	// A game that continued from the save on exit starts under the menu
	if (game_session::ShowsMenuOnStart(game_session::Start::Continue, continued))
	{
		ShowMenuOnStart();
	}
}

bool Game::RunFrame() noexcept
{
	if (!Update())
	{
		return false;
	}
	auto& config = Locator::config::value();
	auto& profiler = Locator::profiler::value();
	const auto lastTime = _frameLoop->lastTime;
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

		auto duration = std::chrono::high_resolution_clock::now() - lastTime;
		auto milliseconds = std::chrono::duration_cast<std::chrono::duration<uint32_t, std::milli>>(duration);
		// The frame is drawn from where the inspector shows the camera (an override, a picture's), while everything the
		// game did this frame went by its own camera, which it gets back once the frame is drawn
		ShowInspectorCamera(true);
		// The game's interface over the scene, its labels over things placed from the camera the scene is drawn from. It
		// is laid out before the scene, so that the creatures it shows over itself are drawn in the same frame as it.
		PlaceScreenLabels();
		PlaceLearningTexts();
		// In the fast simulation mode the game draws only some frames, or none
		const bool drawsFrame =
		    sim_run::DrawsFrame(Locator::simulation::value().GetSettings(), _frameCount) && DoesFrameGraphics();
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
			// TODO(#481): get actual time
			const auto drawDesc = SceneDrawDesc(config, milliseconds.count(), IsHandDrawn());
			// The sun, the moon and the dome's blend of this frame, for drawing it
			Locator::skySystem::value().UpdateFrame(config.drawSky);
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
				Locator::rendererInterface::value().DrawScene(drawDesc);
			}
		}

		ResolveScreenShapes();

		{
			auto section = profiler.BeginScoped(Profiler::Stage::GuiDraw);
			const bool screenshotThisFrame = _requestScreenshot.has_value() && _requestScreenshot->first == _frameCount;
			if (screenshotThisFrame)
			{
				Locator::rendererInterface::value().RequestScreenshot(_requestScreenshot->second);
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

		{
			auto section = profiler.BeginScoped(Profiler::Stage::RendererFrame);
			Locator::rendererInterface::value().Frame();
		}
		// The challenges' pictures whose screens are back
		auto& renderer = Locator::rendererInterface::value();
		ecs::challenge_pictures::Collect(
		    Locator::entitiesRegistry::value(),
		    {.take = [&renderer](const std::string& name) { return renderer.TakeScreenPicture(name); },
		     .failed = [&renderer](const std::string& name) { return renderer.TakeScreenshotFailure(name).has_value(); }});
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
		ShowInspectorCamera(false);

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
		_statisticsFrameRate.Frame(machine_clock::Ticks(), _frameCount);
		// The game is playable from its first frame drawn
		if (_startupTimer.has_value())
		{
			_startupTimer->Step("first frame");
			_startupTimer.reset();
		}

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
			frameStats->Frame(std::chrono::duration<float, std::milli>(frameEnd - frameStart).count(), profiler,
			                  Locator::rendererInterface::value().GetPipelinesMade(), Locator::time::value().GetTurn());
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
	Locator::landSystem::value().LoadTestbed();
}

bool Game::LoadGodsRingMap() noexcept
{
	return Locator::landSystem::value().LoadGodsRingMap();
}

bool Game::LoadSavedLand(const std::filesystem::path& landPath, const std::filesystem::path& landscape,
                         const std::function<bool()>& restore) noexcept
{
	return Locator::landSystem::value().LoadSavedLand(landPath, landscape, restore);
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
	if (Locator::debugGui::has_value() && _testbedWindow)
	{
		Locator::debugGui::value().OpenWindow(debug::gui::k_TestbedScenariosWindow);
	}
}

bool Game::AskStartQuestion(bool hasCreature)
{
	if (!_interface)
	{
		return false;
	}
	// The game waits, paused, for the answer
	Locator::time::value().SetPlayerPause(true);
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

escape_key::Situation Game::EscapeSituation(uint16_t keyModifiers) const
{
	const auto& director = Locator::cinematicDirectorSystem::value();
	const auto fade = director.GetFadeColour();
	escape_key::Situation situation {
	    .shiftOrCtrl = (keyModifiers & (KMOD_SHIFT | KMOD_CTRL)) != 0,
	    .scriptHoldsCinemaBars = director.IsWideScreenOn() && director.GetWideScreenOwner() != 0,
	    .helpScriptHoldsDialogue = Locator::vm::has_value() && chlapi::CHLApi::HelpScriptHoldsDialogue(),
	    // openblack's games are all single player
	    .inGodsPlayground =
	        Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber == escape_key::k_GodsPlaygroundLand,
	    .screenClear = director.IsFadeFinished() && (fade >> 24u) == 0,
	    // The lessons run only after going to the Playground from another land, which openblack can't do yet
	    .playgroundLessonRunning = false,
	    .sinceMenuClosed = std::nullopt,
	};
	if (_menuClosedAt.has_value())
	{
		situation.sinceMenuClosed =
		    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - *_menuClosedAt);
	}
	return situation;
}

void Game::HandleEscapeInPlay(uint16_t keyModifiers)
{
	auto& director = Locator::cinematicDirectorSystem::value();
	const auto response = escape_key::Respond(EscapeSituation(keyModifiers));
	if (response.clearScreenFade)
	{
		director.FadeBackToNormal(0);
	}
	if (response.stopHelpScripts)
	{
		chlapi::CHLApi::StopHelpScriptsByEscape();
	}
	switch (response.outcome)
	{
	case escape_key::Outcome::Nothing:
		break;
	case escape_key::Outcome::OpenMenu:
		// The game keeps the help record of the player when Escape opens the menu
		SaveHelpRecord();
		_interface->OpenMenu();
		// The menu reads the profile's own pictures again as it opens
		ReadTattooPictures();
		HandleInterfaceAction();
		break;
	case escape_key::Outcome::AskLeavePlayground:
	case escape_key::Outcome::EndPlaygroundLesson:
		// Both need the player to have come from another land, the game saved as they left it, which openblack can't do
		// yet
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Escape on the Gods' Playground is not available yet");
		break;
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
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "This part of the menu is not available yet");
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
		_lanDiscovery = std::make_unique<online::NoLanDiscovery>();
	}
	const auto start = std::chrono::steady_clock::now();
	gui::MultiplayerFrontEnd::Services services {
	    .lobby = Locator::lobbyClient::has_value() ? &Locator::lobbyClient::value() : nullptr,
	    .sockets = Locator::webSocketClient::has_value() ? &Locator::webSocketClient::value() : nullptr,
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
	    .clock =
	        [start] {
		        const auto since = std::chrono::steady_clock::now() - start;
		        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(since).count());
	        },
	    .owner = {},
	    .poll = {},
	};
	gui::MultiplayerFrontEnd::Settings settings;
	// The registration page is on the lobby's site: Register on the site, then log in in the game
	const auto origin = Locator::config::value().lobbyOrigin.value_or(std::string());
#if defined(__EMSCRIPTEN__)
	settings.browser = true;
	settings.registerPage = "/register/";
#else
	settings.registerPage = origin.empty() ? std::string() : origin + "/register/";
#endif
	// The internet connection's name in the list is the lobby's host
	const auto host = origin.find("://");
	settings.internetName = gui::ToUtf16(host == std::string::npos ? origin : origin.substr(host + 3));
	if (const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile(); profile.has_value())
	{
		settings.profileName = gui::ToUtf16(profile->name);
	}
	// A game server of another build shows "(Wrong Version)" in the game list
	settings.gameVersion = std::string(build_version::Text());
	auto maps = gui::ReadOnlineMapTexts();
	settings.localMaps = std::move(maps.localMaps);
	settings.conditionTemplate = std::move(maps.conditionTemplate);
	return std::make_unique<gui::MultiplayerFrontEnd>(std::move(services), std::move(settings));
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
	auto& boxes = _interface->GetMultiplayerBoxes();
	if (!boxes.IsActive())
	{
		return;
	}
	const bool templeGone = !Locator::temple::has_value() || !Locator::temple::value().IsMultiplayerFrontEnd();
	if (boxes.TakeLeft() || templeGone)
	{
		boxes.Close();
		if (Locator::temple::has_value())
		{
			Locator::temple::value().LeaveMultiplayerFrontEnd();
		}
	}
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
	// In the game, a land changes only when the menu is closed. The menu closes before the choices in it that change
	// the land. This also closes the menu for a land that the debug tools load.
	if (_interface && _interface->GetMenu().IsOpen())
	{
		_interface->GetMenu().Close();
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

void Game::LoadProfileKeys()
{
	// A profile that keeps no bindings has the game's defaults
	input::KeyBindingTable bindings = input::DefaultKeyBindings();
	if (Locator::playerProfileSystem::has_value())
	{
		if (const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile(); profile.has_value())
		{
			bindings = input::rebinding::FromKept(profile->keys);
		}
	}
	if (Locator::gameActionSystem::has_value())
	{
		Locator::gameActionSystem::value().SetKeyBindings(bindings);
	}
	if (_interface)
	{
		_interface->GetMenu().SetControls(bindings);
	}
}

void Game::KeepProfileKeys()
{
	if (!Locator::playerProfileSystem::has_value() || !Locator::gameActionSystem::has_value())
	{
		return;
	}
	const auto bindings = Locator::gameActionSystem::value().GetKeyBindings();
	Locator::playerProfileSystem::value().SetCurrentKeys(input::rebinding::ToKept(bindings));
}

void Game::ReadTattooPictures()
{
	// The symbols' picture and the tattoo designs change with the pictures. The game loads them one time only
	// (vanilla-bugs 380), and openblack fixes this.
	if (const auto pictures = simulation::ReadTattooPictures(); pictures.has_value() && _interface)
	{
		_interface->SetSymbolPictures(*pictures);
	}
}

void Game::MakePlayersSymbols(std::optional<uint32_t> symbolInPlace)
{
	using filesystem::Path;
	if (!Locator::filesystem::has_value() || !Locator::playerProfileSystem::has_value())
	{
		return;
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto textures = fileSystem.GetPath<Path::Textures>();
	const auto read = [&fileSystem](const std::filesystem::path& path) -> std::vector<uint8_t> {
		try
		{
			return fileSystem.ReadAll(path);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			return {};
		}
	};
	players_symbols::Sheet file {.colours = read(textures / "PlayersSymbols.raw"),
	                             .alpha = read(textures / "PlayersSymbolsa.raw")};
	const players_symbols::Sheet designs {.colours = read(textures / "OriginalChooseSymbol.raw"),
	                                      .alpha = read(textures / "OriginalChooseSymbola.raw")};
	if (!file.Whole() || !designs.Whole())
	{
		return;
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	const auto symbol =
	    players_symbols::ChosenSymbol(profile.has_value() ? std::optional(profile->symbol) : std::nullopt, symbolInPlace);
	std::optional<std::span<const uint8_t>> ownPicture;
	std::optional<creature_tattoo::Picture> picture;
	if (symbol.has_value() && Locator::tattooEditorSystem::has_value() && *symbol < creature_tattoo::k_DesignCount)
	{
		picture = Locator::tattooEditorSystem::value().GetPictures().at(*symbol);
		if (picture.has_value())
		{
			ownPicture = std::span<const uint8_t>(*picture);
		}
	}
	const auto localPlayer =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	auto players = players_symbols::MakeSheet(std::move(file), designs, symbol, static_cast<uint32_t>(localPlayer), ownPicture);
	// The sheet takes the place of the file's, kept as the game keeps a texture with an alpha
	auto& textureManager = Locator::resources::value().GetTextures();
	const auto replace = [&textureManager](const std::string& name, std::vector<uint8_t> bytes) {
		textureManager.Erase(name);
		try
		{
			textureManager.Load(name, resources::Texture2DLoader::FromBufferTag {}, name, std::move(bytes),
			                    graphics::texel_depth::Depth::FourBits);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	};
	replace("raw/PlayersSymbols", std::move(players.colours));
	replace("raw/PlayersSymbolsa", std::move(players.alpha));
}

void Game::NameCreatureAfterProfile()
{
	// The player's creature takes the name its profile gives it
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value() || !Locator::playerSystem::has_value())
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
	                   end == game_session::End::NewGame ? "a new game starts" : "the game continues as the profile picked");
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
	case game_session::Start::SavedGame:
	case game_session::Start::Online:
	case game_session::Start::Skirmish:
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
	    // openblack's games are all single player
	    .multiplayer = false,
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
	Locator::skySystem::value().SetTime(time);
}

void Game::SizeSceneToWindow()
{
	if (!Locator::rendererInterface::has_value() || !Locator::oceanSystem::has_value())
	{
		return;
	}
	if (Locator::windowing::has_value())
	{
		const auto size = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
		Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Main, size, 0x274659ff);
		Locator::oceanSystem::value().ResizeReflectionFramebuffer(size);
	}
	uint16_t width = 0;
	uint16_t height = 0;
	Locator::oceanSystem::value().GetReflectionFramebuffer().GetSize(width, height);
	Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Reflection, {width, height}, 0x274659ff);
}

void Game::FollowWindowSize()
{
	if (!Locator::windowing::has_value() || !Locator::rendererInterface::has_value())
	{
		return;
	}
	const auto size = _windowSize.Follow(Locator::windowing::value().GetSize());
	if (!size.has_value())
	{
		return;
	}
	Locator::rendererInterface::value().Reset(*size);
	if (Locator::oceanSystem::has_value())
	{
		Locator::oceanSystem::value().ResizeReflectionFramebuffer(*size);
	}
	// The camera keeps its lens (the world's, the temple's) and its clip planes, only its shape changes
	if (Locator::camera::has_value())
	{
		Locator::camera::value().SetAspect(_windowSize.Aspect());
	}
}

void Game::FitNearClip()
{
	// Outside a camera with a lens of its own, the near plane follows the camera's height over the land, but for close
	// shots: a script's, and a miracle's camera path
	auto& camera = Locator::camera::value();
	if (camera.GetModel().GetLens().has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto origin = camera.GetOrigin();
	const float height = origin.y - Locator::terrainSystem::value().GetHeightAt(glm::vec2(origin.x, origin.z));
	const float nearClip = near_clipping::NearPlane(height, Locator::cinematicDirectorSystem::value().IsCloseClipping() ||
	                                                            Locator::cameraPathSystem::value().HoldsCamera());
	if (nearClip != camera.GetNearClip())
	{
		camera.SetNearClip(nearClip);
	}
}

void Game::ShowInspectorCamera(bool shown)
{
	if (!Locator::inspector::has_value())
	{
		return;
	}
	auto& inspector = Locator::inspector::value();
	if (shown)
	{
		inspector.PlaceCamera();
	}
	else
	{
		inspector.GiveCameraBack();
	}
	FitNearClip();
}

void Game::ShowInspectorOverride(bool shown)
{
	if (!Locator::inspector::has_value())
	{
		return;
	}
	auto& inspector = Locator::inspector::value();
	if (shown)
	{
		inspector.ShowOverrideToPointer();
	}
	else
	{
		inspector.GiveCameraBack();
	}
	FitNearClip();
}

void Game::RequestScreenshot(const std::filesystem::path& path, bool hideDebugGui) noexcept
{
	_requestScreenshot = std::make_pair(_frameCount, path);
	_screenshotHidesDebugGui = hideDebugGui;
}

void Game::LoadHandAnimation()
{
	auto& fileSystem = Locator::filesystem::value();
	const auto path = fileSystem.GetPath<filesystem::Path::Data>() / "CTR" / "hh.hbn";
	const auto specPath = fileSystem.GetPath<filesystem::Path::Data>() / "hndspec5.txt";
	if (!fileSystem.Exists(path) || !fileSystem.Exists(specPath))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The hand is not animated: {} or {} is missing", path.string(),
		                   specPath.string());
		return;
	}

	pack::PackFile pack;
	const auto packResult = pack.ReadFile(*fileSystem.GetData(path));
	if (packResult != pack::PackResult::Success || !pack.HasBlock("Hand"))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the Hand block of {}: {}", path.string(),
		                    pack::ResultToStr(packResult));
		return;
	}
	morph::MorphFile morphFile;
	const auto morphResult = morphFile.Open(pack.GetBlock("Hand"), fileSystem.FindPath(specPath).parent_path());
	if (morphResult != morph::MorphResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the hand animations of {}: {}", path.string(),
		                    morph::ResultToStr(morphResult));
		return;
	}

	LoadHandLooks(morphFile);

	const auto mesh = Locator::resources::value().GetMeshes().Handle(entt::hashed_string("hand"));
	auto animation = std::make_unique<HandAnimation>();
	if (!mesh || !animation->Load(morphFile, mesh->GetBoneParents(), mesh->GetBoneMatrices()))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The hand animations of {} do not fit the hand mesh", path.string());
		return;
	}
	_handAnimation = std::move(animation);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loaded the hand animations of {}", path.string());
}

void Game::PlaceHand(ecs::components::Transform& handTransform, float deltaSeconds)
{
	const auto& camera = Locator::camera::value();
	const auto eye = camera.GetOrigin();
	// How far the hand reaches from the camera, as the land's scripts allow
	const auto handReach = Locator::cameraHelpSystem::value().Get().handReach;

	// In the temple the hand hangs on the line of sight through the cursor, a little short of where it
	// meets the room, slowly away from the camera and quickly towards it
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		if (_cursorWorldPosition)
		{
			const auto toRoom = *_cursorWorldPosition - eye;
			const auto roomDistance = glm::length(toRoom);
			if (roomDistance > 0.0f)
			{
				_handRayDirection = toRoom / roomDistance;
			}
			const auto target =
			    glm::clamp(glm::max(roomDistance - k_HandTempleGap, 1.0f), k_HandTempleMinDistance, k_HandTempleMaxDistance);
			const auto easeTime = _handHoverZoomer.GetDestination() <= target ? 0.4f : 0.2f;
			_handHoverZoomer.SetDestination(target, easeTime);
			_handHoverZoomer.Update(deltaSeconds);
			// The hand's distance from the camera
			_handDistance = _handHoverZoomer.GetValue();
		}
		handTransform.position = eye + _handRayDirection * _handDistance;
		return;
	}

	// In the clear view, the hand stays on the land at the thing that the camera turns round. The camera puts it there
	// after the turn moved the hand, so this comes first.
	if (const auto* world = dynamic_cast<const DefaultWorldCameraModel*>(&camera.GetModel()); world != nullptr)
	{
		if (const auto hand = world->GetClearViewHand(); hand.has_value())
		{
			_handPosition = *hand;
			_handCrossFade.Update(deltaSeconds);
			handTransform.position = _handCrossFade.Apply(_handPosition);
			return;
		}
	}

	// Tapping a house it knocked on, the hand stays where it knocked
	if (_handKnocking)
	{
		_handPosition = _handKnock.Point().value_or(_handPosition);
		_handCrossFade.Update(deltaSeconds);
		handTransform.position = _handCrossFade.Apply(_handPosition);
		return;
	}

	// Gripping the land, the camera keeps the land the hand gripped under the cursor, and the hand stays on the land it
	// gripped, so it moves with it. Its hover carries on from how far the gripped land was from the camera.
	const bool gripsLand = _handCameraState && _handPose == hand_navigation_pose::Pose::Grip;
	if (gripsLand)
	{
		if (!_handWasGripping)
		{
			_handGripPoint = _cursorWorldPosition.value_or(_handPosition);
		}
		_handPosition = _handGripPoint;
		_handDistance = glm::clamp(glm::distance(eye, _handGripPoint), k_HandMinDistance, handReach);
		_handHoverZoomer.Reset(_handDistance);
		_handHoldZoomer.Reset(_handDistance);
		_handWasGripping = true;
		_handCrossFade.Update(deltaSeconds);
		handTransform.position = _handCrossFade.Apply(_handPosition);
		return;
	}
	_handWasGripping = false;

	// Dragging by the edge of the screen, before the drag is decided or as it turns or tilts the camera, the hand keeps
	// on the line of sight through the cursor, holding how far it is from the camera over 0.4 seconds but never past
	// the land
	if (_handCameraState)
	{
		const auto screenSize =
		    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
		if (screenSize.x > 0 && screenSize.y > 0)
		{
			glm::vec3 rayOrigin;
			glm::vec3 rayDirection;
			camera.DeprojectScreenToWorld(static_cast<glm::vec2>(_mousePosition) / static_cast<glm::vec2>(screenSize),
			                              rayOrigin, rayDirection);
			if (!glm::any(glm::isnan(rayDirection)))
			{
				_handRayDirection = glm::normalize(rayDirection);
			}
		}
		_handHoldZoomer.SetDestination(glm::distance(eye, _handPosition), k_HandHoldTime);
		_handHoldZoomer.Update(deltaSeconds);
		const auto landDistance = _cursorWorldPosition.has_value() ? glm::distance(eye, *_cursorWorldPosition) : handReach;
		_handDistance = glm::clamp(glm::min(_handHoldZoomer.GetValue(), landDistance), k_HandMinDistance, handReach);
		_handHoverZoomer.Reset(_handDistance);
		_handPosition = eye + _handRayDirection * _handDistance;
		_handCrossFade.Update(deltaSeconds);
		handTransform.position = _handCrossFade.Apply(_handPosition);
		return;
	}
	_handHoldZoomer.Reset(_handDistance);

	// While a press on a body is held in a fight, the hand goes to a sparkle on the opponent's body that the last
	// drawing found. That point takes the place of the cursor's point.
	const auto sparkle =
	    Locator::creatureFightSystem::has_value() ? Locator::creatureFightSystem::value().TakeHandSparkle() : std::nullopt;

	// The cursor's point on the near plane, which the hand's line of sight goes through
	std::optional<glm::vec3> cursorNearPoint;
	auto lineEye = eye;
	if (Locator::windowing::has_value())
	{
		const auto size = Locator::windowing::value().GetSize();
		if (size.x > 0 && size.y > 0)
		{
			const auto line = camera.OnNearPlane(static_cast<glm::vec2>(_mousePosition) / static_cast<glm::vec2>(size));
			lineEye = line.eye;
			cursorNearPoint = line.point;
		}
	}

	// The game puts the origin of the hand, by its fingertips, on the line of sight through the cursor, so the hand is
	// always under the cursor on screen. How far along it depends on the land the cursor is over. Turning the camera
	// with the mouse holds the cursor still, so the hand stays where it is on screen and eases to the land coming under
	// it.
	if (_cursorLand.has_value() || _handRestOnThing.has_value() || _handBubbleDistance.has_value())
	{
		// Over the land the hand points at it; over a thing, at where it rests on the thing
		// Over the help's text bubble it eases to the bubble, even with the sky behind it
		const auto toward = _handRestOnThing.value_or(_cursorLand.value_or(eye)) - eye;
		if (const auto length = glm::length(toward); length > 0.0f)
		{
			_handRayDirection = toward / length;
		}

		// The furthest the hand may be is the land less its height, so its fingers hang down to the land, or the
		// water over the sea; holding something, the land itself. Over a thing it eases to where it rests on the thing,
		// touching it.
		auto inputs = HandHoverInputs(lineEye);
		inputs.restOnThing = _handRestOnThing;
		inputs.restNearer = _handRestNearer;
		inputs.bubbleDistance = _handBubbleDistance;
		if (sparkle.has_value())
		{
			inputs.cursorPoint = *sparkle;
		}
		else if (cursorNearPoint.has_value())
		{
			inputs.cursorPoint = hand_feel::PointAlongCursor(lineEye, *cursorNearPoint, hand_feel::HoverLimit(inputs));
		}
		const auto hover = hand_feel::HoverDistancesOf(inputs);

		// The hand eases out slowly away from the camera and quickly towards it, but not while it pulls a thing out of
		// the ground: the pull places it then. While the camera watches a fight or the hand offers what it holds to a
		// creature, it eases out more slowly.
		const bool pulling = Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().IsPulling();
		const bool watchesFight = dynamic_cast<const FightCameraModel*>(&camera.GetModel()) != nullptr;
		const bool offers = Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().OffersToCreature();
		hand_feel::EaseHover(_handHoverZoomer, hover.target, deltaSeconds, pulling, watchesFight || offers);
		_handDistance = glm::clamp(glm::min(_handHoverZoomer.GetValue(), hover.limit), k_HandMinDistance, handReach);
	}
	// The hand is on the line of sight through the cursor at its distance, or on the fight's sparkle
	_handPosition = cursorNearPoint.has_value() ? hand_feel::PointAlongCursor(lineEye, *cursorNearPoint, _handDistance)
	                                            : eye + _handRayDirection * _handDistance;
	_handPosition = sparkle.value_or(_handPosition);
	_handCrossFade.Update(deltaSeconds);
	handTransform.position = _handCrossFade.Apply(_handPosition);
}

float Game::HandDrawnScale(glm::vec3 eye, glm::vec3 handPosition, bool holding) const
{
	const auto distance = magic::hand_hold::DistanceFromView(eye, handPosition, holding);
	// Held to a creature, its size is multiplied by the creature's scale before the size becomes the drawn scale
	const float size = HandAnimation::SizeAtDistance(distance);
	return _handAnimation->DrawnScale(_handOnCreature.has_value() ? _handOnCreature->scale * size : size);
}

hand_feel::Hover Game::HandHoverInputs(glm::vec3 eye) const
{
	std::optional<HoldType> held;
	bool offers = false;
	if (Locator::handGrabSystem::has_value())
	{
		const auto& grab = Locator::handGrabSystem::value();
		if (const auto pose = grab.GetHeldPose())
		{
			held = pose->hold;
		}
		offers = grab.OffersToCreature();
	}
	const bool holding = hand_feel::HoldReachesLand(held, offers);
	const auto overSea =
	    _cursorLand.has_value() && Locator::terrainSystem::value().GetHeightAt(glm::xz(*_cursorLand)) < k_HandSeaAltitude;
	return {
	    .camera = eye,
	    .land = _cursorLand,
	    .landIsSea = overSea,
	    .holding = holding,
	    .restOnThing = std::nullopt,
	    .cursorPoint = std::nullopt,
	    .handHeight = k_HandHeight * HandAnimation::SizeAtDistance(_handDistance),
	    .currentDistance = _handDistance,
	    .minDistance = k_HandMinDistance,
	    .reach = Locator::cameraHelpSystem::value().Get().handReach,
	};
}

void Game::UpdateHandKnock(const ecs::components::Transform& handTransform)
{
	if (!Locator::abodeKnockSystem::has_value())
	{
		return;
	}
	auto& knocks = Locator::abodeKnockSystem::value();
	// The houses' read-out of their people runs by the frame's game time, so it stops while the game pauses and
	// follows the game speed
	knocks.Update(Locator::time::value().GetFrameGameTime());
	// A knock while the tap plays doesn't start it over, the hand already being where it knocked
	if (knocks.TakeHandKnock())
	{
		_handKnock.Knock(_handPosition);
	}
	// Holding something comes first; the tap starts over once the hand lets go
	bool holds = magic::HandHoldPoser::Find().has_value();
	if (!holds && Locator::handGrabSystem::has_value())
	{
		holds = Locator::handGrabSystem::value().GetHeldPose().has_value();
	}
	const bool knocking = _handKnock.Hold(holds);
	// Starting and ending the tap, the hand fades from where it was drawn
	if (knocking != _handKnocking)
	{
		_handCrossFade.Start(handTransform.position);
	}
	_handKnocking = knocking;
}

void Game::UpdateHandNavigation(const ecs::components::Transform& handTransform)
{
	using namespace hand_navigation_pose;
	auto cues = Locator::camera::value().GetModel().GetHandCues();
	// A hand demonstration shows the camera hints its recording had
	if (Locator::handDemoSystem::has_value() && Locator::handDemoSystem::value().IsPlaying(0))
	{
		cues.tricons = Locator::handDemoSystem::value().GetHints();
	}
	// Dragging the land is the hand's camera state; turning the camera with the middle button or both buttons isn't
	const bool cameraState = _handGripping && !_handRotating;
	auto pose = Pose::Idle;
	if (cameraState)
	{
		// Other cameras than the world's don't sort their drags: the hand grips the land at once
		const bool gripsLand = !cues.dragging || cues.dragMode == camera_drag::DragMode::Pan || cues.clearViewGrip;
		pose = WhileDragging(gripsLand, cues.tricons);
	}
	else if (!_cursorOnObject && !_creatureUnderHand.has_value() && !_handOnCreature.has_value())
	{
		pose = WhileHovering(cues.tricons).value_or(Pose::Idle);
	}
	// Changing state or pose, the hand fades from where it was drawn to its new place
	if (cameraState != _handCameraState || pose != _handPose)
	{
		_handCrossFade.Start(handTransform.position);
	}
	_handCameraState = cameraState;
	_handPose = pose;
}

void Game::UpdateHandTricons(bool handDrawn, float seconds, hand_blend::HandMode mode)
{
	if (!_interface)
	{
		return;
	}
	_interface->SetHandTricons(std::nullopt);
	// Shown from the world camera, or from a tutorial demonstration's recording in any camera, out of the temple
	const auto& camera = Locator::camera::value();
	const auto cues = camera.GetModel().GetHandCues();
	const bool demonstration = Locator::handDemoSystem::has_value() && Locator::handDemoSystem::value().IsPlaying(0);
	if (!handDrawn || (!demonstration && !cues.icons.has_value()) || !Locator::windowing::has_value() ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	const auto& actions = Locator::gameActionSystem::value();
	gui::GameInterface::HandTricons tricons {
	    .handStateShowsIcons = true,
	    .cinemaBars = Locator::cinematicDirectorSystem::value().IsWideScreenOn(),
	    .seconds = seconds,
	};
	if (demonstration)
	{
		const auto& demo = Locator::handDemoSystem::value();
		const auto held = demo.GetHeldButtons();
		_triconAngle = demo.GetHintAngle();
		tricons.icons = demo.GetHints();
		tricons.demonstration = true;
		tricons.moveHeld = held.move;
		tricons.actionHeld = held.action;
	}
	else
	{
		auto frame = *cues.icons;
		frame.handInInfluence = HandInInfluence();
		// A box of the menu on the screen stops the camera input, so no icon shows
		frame.inputOff = FrontEndBoxShows();
		frame.tickMs = machine_clock::Ticks();
		const auto icons = hand_tricons::WorldCamera(frame, _triconAngle);
		_triconAngle = icons.rotateAngle;
		tricons.icons = icons.icons;
		tricons.leans = cues.clearView <= hand_tricons::k_ClearViewLeans;
		tricons.cameraBusy =
		    frame.gripping || frame.rotatingAroundMouse || frame.turn != 0.0f || frame.tilt != 0.0f || frame.zoom != 0.0f;
		// The cross brightens with a thing under the hand, and shows at once with the action button
		if (actions.Get(input::BindableActionMap::ACTION))
		{
			tricons.crossNudge = hand_tricons::CrossNudge::Action;
		}
		else if (_cursorOnObject)
		{
			tricons.crossNudge = hand_tricons::CrossNudge::OverThing;
		}
	}
	tricons.rotateAngle = _triconAngle;

	const auto screen = glm::vec2(Locator::windowing::value().GetSize());
	// Where the posed hand is drawn: by its first bone, at the wrist
	const auto handEntity =
	    Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	auto& registry = Locator::entitiesRegistry::value();
	const auto* handTransform = registry.TryGet<ecs::components::Transform>(handEntity);
	if (handTransform == nullptr)
	{
		return;
	}
	const auto* hand = registry.TryGet<ecs::components::Hand>(handEntity);
	const auto anchor = hand_tricons::AnchorPoint(handTransform->position, handTransform->rotation, handTransform->scale,
	                                              hand != nullptr ? std::span<const glm::mat4>(hand->boneMatrices)
	                                                              : std::span<const glm::mat4>());
	const auto clip = camera.GetViewProjectionMatrix() * glm::vec4(anchor, 1.0f);
	// Not with the hand behind the camera
	if (clip.w < camera.GetNearClip())
	{
		return;
	}
	tricons.hand = {static_cast<int>((clip.x / clip.w + 1.0f) * screen.x * 0.5f),
	                static_cast<int>((1.0f - clip.y / clip.w) * screen.y * 0.5f)};
	// They lean towards the cursor the hand follows this frame, after any warp onto the edge's turning ring
	tricons.cursor = _mousePosition;
	// The same size on the screen at any distance: a part of the focal length across
	tricons.halfSize = hand_tricons::IconHalfSize(camera.GetProjectionMatrix()[0][0], glm::ivec2(screen));
	// The mode of the hand decides whether they show
	const bool scoopsLockedThing =
	    Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GetLockedOn().has_value();
	tricons.handStateShowsIcons = hand_blend::AllowsCameraIcons(mode, scoopsLockedThing);
	_interface->SetHandTricons(tricons);
}

void Game::OrientHand(ecs::components::Transform& handTransform, const glm::mat3& facingCamera, glm::vec3 surfaceUp,
                      float deltaSeconds)
{
	using namespace hand_orientation;
	const auto& camera = Locator::camera::value();
	const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();

	// The hand faces along the line of sight through the cursor, laid flat
	if (screenSize.x > 0 && screenSize.y > 0)
	{
		glm::vec3 rayOrigin;
		glm::vec3 rayDirection;
		camera.DeprojectScreenToWorld(static_cast<glm::vec2>(_mousePosition) / static_cast<glm::vec2>(screenSize), rayOrigin,
		                              rayDirection);
		if (!glm::any(glm::isnan(rayDirection)))
		{
			// Both forms of the heading change together, only when the line of sight leans across the land
			if (LeansAcrossLand(rayDirection))
			{
				_handHeadingAngle = HeadingAngle(rayDirection);
			}
			_handHeading = HeadingAlongRay(rayDirection, _handHeading);
		}
	}
	const auto cameraHeading = HeadingAlongRay(camera.GetForward(), _handHeading);

	// Its up eases over 0.4 seconds to the slope of the land under it, or to the face of what the cursor is on, given
	// afresh only as the cursor moves across the screen. Offering to turn the camera at the edge of the screen it
	// stands up towards where the camera looks instead. Dragging the land it holds its up, or stands straight towards
	// the camera's focus showing it turns the camera; letting go it stands up straight and eases from there.
	using hand_navigation_pose::Pose;
	const bool cursorMovedAcross = _mousePosition.x != _handLastCursorX;
	_handLastCursorX = _mousePosition.x;
	// A hand that holds a thing keeps the rules of the hand that holds, also while it drags the land
	const bool holdsSeed = magic::HandHoldPoser::Find().has_value();
	const bool holds =
	    holdsSeed || (Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GetHeld().has_value());
	const bool cameraUp = _handCameraState && !holds;
	// A pour tips the hand that holds the miracle. Its up is then given afresh each frame.
	const float pourTilt = holdsSeed && Locator::magicSystem::has_value()
	                           ? Locator::magicSystem::value().GetHandPour(Locator::time::value().GetTurnFraction()).tilt
	                           : 0.0f;
	// While the camera turns by the ring around the middle of the screen, the hand that holds stands towards the focus
	const auto cues = camera.GetModel().GetHandCues();
	const bool ringTurn = holds && cues.dragging && cues.dragMode == camera_drag::DragMode::EdgeRotate;
	if (cameraUp)
	{
		if (_handPose == Pose::Rotate)
		{
			_handUp.Reset(hand_navigation_pose::UpTowardsFocus(_handPosition, camera.GetFocus()));
		}
	}
	else
	{
		if (_handUpWasHeld)
		{
			_handUp.Reset(glm::vec3(0.0f, 1.0f, 0.0f));
		}
		if (cursorMovedAcross || pourTilt != 0.0f)
		{
			// The face of a model the hand rests on, unless it is lower over the thing than half its own height; else the
			// land where the hand is
			const float handHeight = k_HandHeight * HandAnimation::SizeAtDistance(_handDistance);
			// The land's height under the hand is taken at the hand's map position
			const auto handPoint = map_coords::FromMetres(glm::xz(handTransform.position));
			const float aboveLand =
			    Locator::terrainSystem::has_value()
			        ? handTransform.position.y - Locator::terrainSystem::value().GetAltitude(handPoint.x, handPoint.z)
			        : 0.0f;
			auto up = !hand_feel::TurnsToLand(_handSurfaceUp.has_value(), _handOverObject, aboveLand, handHeight)
			              ? surfaceUp
			              : (Locator::terrainSystem::has_value()
			                     ? Locator::terrainSystem::value().GetNormalAt(glm::xz(handTransform.position))
			                     : glm::vec3(0.0f, 1.0f, 0.0f));
			// Over the help's text bubble or the leash's rope it stands straight up. Offering to turn the camera over the
			// land with nothing in it, it stands towards the camera's focus from the cursor's point at its furthest.
			if (_handStandsStraight)
			{
				up = glm::vec3(0.0f, 1.0f, 0.0f);
			}
			else if (_handPose == Pose::Rotate && _handMayOfferTurn)
			{
				up = hand_navigation_pose::UpTowardsFocus(_handLimitPoint.value_or(_handPosition), camera.GetFocus());
			}
			if (ringTurn && !_handStandsStraight)
			{
				up = hand_navigation_pose::UpTowardsFocusOnRing(_handLimitPoint.value_or(_handPosition), camera.GetFocus());
			}
			// A pour turns straight up by its tilt about the line from the hand to the camera, over all else
			if (pourTilt != 0.0f)
			{
				up = hand_feel::PourUp(camera.GetOrigin(), handTransform.position, pourTilt);
			}
			_handUp.SetDestination(hand_feel::UnitUp(up), k_UpEaseSeconds);
		}
		_handUp.Update(deltaSeconds);
	}
	_handUpWasHeld = cameraUp;

	// Tapping a house, the hand stands straight up
	if (_handKnocking)
	{
		_handUp.Reset(glm::vec3(0.0f, 1.0f, 0.0f));
	}
	const auto up = hand_feel::UnitUp(_handUp.GetValue());
	const auto onLevelLand = TurnToHeading(facingCamera, cameraHeading, _handHeading);
	_handOnLevelLand = onLevelLand;
	handTransform.rotation = glm::length(up) > 0.0f ? StandOnSlope(onLevelLand, _handHeading, up) : onLevelLand;
}

std::chrono::milliseconds Game::CameraStepTime() const
{
	const auto& time = Locator::time::value();
	const bool scripted = Locator::cinematicDirectorSystem::has_value() &&
	                      Locator::cinematicDirectorSystem::value().IsWideScreenOn() &&
	                      Locator::cinematicDirectorSystem::value().GetWideScreenOwner() != 0;
	const bool insideTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	return ecs::systems::CameraStep(time.GetFrameRealTime(), time.GetFrameGameTime(), scripted, insideTemple);
}

float Game::HandStepSeconds() const
{
	return static_cast<float>(static_cast<int32_t>(CameraStepTime().count())) * 0.001f;
}

void Game::EndHandTrace(const glm::vec3& placed, const ecs::components::Transform& hand, hand_blend::HandMode mode,
                        const ecs::systems::HandGrabSystemInterface* grab)
{
	auto* trace = Locator::handSystem::value().GetTrace();
	auto* frame = trace != nullptr ? trace->Current() : nullptr;
	if (frame == nullptr)
	{
		return;
	}
	frame->cursor = glm::vec2(_mousePosition);
	frame->placed = placed;
	frame->drawn = hand.position;
	// The hand shows where its fingertips are as posed, which its pose can carry away from its origin
	frame->visible = hand.position;
	if (_handAnimation && _handAnimation->IsLoaded())
	{
		const auto model =
		    glm::translate(glm::mat4(1.0f), hand.position) * glm::mat4(hand.rotation) * glm::scale(glm::mat4(1.0f), hand.scale);
		frame->visible = glm::vec3(model * glm::vec4(_handAnimation->LeafBoneCentre(), 1.0f));
		// The wrist as it is drawn: after the blend of the drawn pose, which the animation's own bones do not have
		const auto handEntity =
		    Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
		const auto* drawn = Locator::entitiesRegistry::value().TryGet<const ecs::components::Hand>(handEntity);
		const auto& bones = drawn != nullptr ? drawn->boneMatrices : _handAnimation->GetBoneMatrices();
		if (!bones.empty())
		{
			frame->wrist = glm::vec3(model * bones.front()[3]);
		}
	}
	const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
	glm::vec3 screen;
	if (screenSize.x > 0 && screenSize.y > 0 &&
	    Locator::camera::value().ProjectWorldToScreen(frame->visible, glm::vec4(0.0f, 0.0f, glm::vec2(screenSize)), screen))
	{
		frame->screen = glm::vec2(screen);
	}
	if (screenSize.x > 0 && screenSize.y > 0 &&
	    Locator::camera::value().ProjectWorldToScreen(frame->wrist, glm::vec4(0.0f, 0.0f, glm::vec2(screenSize)), screen))
	{
		frame->wristScreen = glm::vec2(screen);
	}
	frame->stage = static_cast<uint8_t>(mode);
	if (const auto picked = Locator::pickingSystem::value().GetPick().object)
	{
		frame->picked = static_cast<uint32_t>(entt::to_integral(*picked));
	}
	frame->cursorLand = _cursorLand;
	frame->restOnThing = _handRestOnThing;
	if (grab != nullptr)
	{
		frame->busy = grab->IsBusy();
		if (const auto held = grab->GetHeld())
		{
			frame->held = static_cast<uint32_t>(entt::to_integral(*held));
			if (const auto* transform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(*held))
			{
				frame->heldAt = transform->position;
			}
		}
	}
	trace->EndFrame();
}

void Game::UpdateMagicHand(const glm::vec3& handPosition, float deltaSeconds)
{
	const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
	ecs::systems::MagicSystemInterface::HandFrame frame {.handPosition = handPosition, .point = _cursorWorldPosition};
	if (screenSize.x > 0 && screenSize.y > 0)
	{
		Locator::camera::value().DeprojectScreenToWorld(
		    static_cast<glm::vec2>(_mousePosition) / static_cast<glm::vec2>(screenSize), frame.rayOrigin, frame.rayDirection);
	}
	frame.cameraForward = Locator::camera::value().GetForward();
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	frame.overWorld = !inTemple && !Locator::debugGui::value().IsMouseOverWindow();
	auto& magic = Locator::magicSystem::value();
	// The hand's movement, and the spin it gives a miracle, are measured by what the hand steps by
	magic.UpdateHand(frame, HandStepSeconds());
	magic.Update(deltaSeconds);
	Locator::magicShieldSystem::value().Update(deltaSeconds);
	// The seeds above the stone hands and worship icons spin as the globes' do
	if (Locator::spellIconSystem::has_value())
	{
		Locator::spellIconSystem::value().Update(deltaSeconds);
	}
}

void Game::PlayHandGrabSound()
{
	if (!_cursorWorldPosition || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto position = *_cursorWorldPosition;

	// Land is a cell of the landscape without water
	bool isLand = false;
	const auto cell = glm::floor(glm::vec2(position.x, position.z) / 10.0f);
	if (cell.x >= 0.0f && cell.y >= 0.0f && cell.x < 512.0f && cell.y < 512.0f)
	{
		const auto* landCell = Locator::terrainSystem::value().FindCell(glm::u16vec2(cell));
		isLand = landCell != nullptr && landCell->properties.hasWater == 0;
	}

	const auto feedback = hand_grab::GripFeedbackOf(isLand, !Locator::cinematicDirectorSystem::value().IsInterfaceActive());
	if (feedback == hand_grab::GripFeedback::None)
	{
		return;
	}
	if (feedback == hand_grab::GripFeedback::Dust)
	{
		// The game throws up a spot visual where the hand grips the land, as it plays the sound
		GripLandscapeEffect::Spawn(
		    glm::vec3(position.x, Locator::terrainSystem::value().GetHeightAt(glm::xz(position)), position.z));
		// One of G_HandGrabLand_01 to _06, centred on the listener
		const auto sample = 4 + Locator::rng::value().NextValue(0, 5);
		const auto id = fmt::format("InGame.sad/{}", sample);
		// The grip is a sound of its own to the voice rules
		audio::StartGameSoundEffect(entt::hashed_string(id.c_str()), {.voiceOwner = audio::VoiceOwner::OwnSound});
	}
	else
	{
		// While the game runs, the hand splashes where it goes in: a ring on the water, in the land's brightest light
		if (!Locator::time::value().IsPaused())
		{
			const auto angle = Locator::gameRandom::value().CrtRandom(0.0f, glm::two_pi<float>());
			Locator::waterRingSystem::value().Add(
			    water_rings::HandSplash(glm::vec2(position.x, position.z), angle, FrameLandLight(255)));
			// The fish near where the hand went in dart away
			Locator::fishFarmSystem::value().Scare({position.x, k_HandSplashHeight, position.z});
		}
		// G_HandInWater_01 to _10 in turn, on the water's surface where the hand went in
		const auto id = fmt::format("InGame.sad/{}", 99 + _handInWaterSample);
		_handInWaterSample = (_handInWaterSample + 1) % 10;
		audio::PlayGameSoundEffect(entt::hashed_string(id.c_str()), glm::vec3(position.x, k_HandSplashHeight, position.z));
	}
}
