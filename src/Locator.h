/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <optional>
#include <string>

#include <entt/locator/locator.hpp>

#include "EngineConfig.h"

namespace openblack
{
struct EngineConfig;
class Camera;
class EventManager;
class LandIslandInterface;
struct LandData;
class OceanInterface;
class Profiler;
class GameRandomInterface;
class RandomNumberManagerInterface;
class TempleInteriorInterface;

namespace player_profiles
{
struct Store;
}

namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;

namespace audio
{
class AudioManagerInterface;
}

namespace chlapi
{
class CHLApi;
}

namespace online
{
class HttpClientInterface;
class WebSocketClientInterface;
} // namespace online

namespace debug::gui
{
class DebugGuiInterface;
}

namespace filesystem
{
class FileSystemInterface;
}

namespace graphics
{
class RendererInterface;
}

namespace input
{
class GameActionInterface;
}

namespace lhvm
{
class LHVM;
}

namespace resources
{
class ResourcesInterface;
}

namespace windowing
{
enum class DisplayMode : std::uint8_t;
class WindowingInterface;
} // namespace windowing

namespace ecs
{
class Registry;
class DrawnRegistry;
class MapInterface;
} // namespace ecs

namespace ecs::systems
{
class CameraBookmarkSystemInterface;
class CameraZoneSystemInterface;
class DynamicsSystemInterface;
class PickingSystemInterface;
class HandSystemInterface;
class HandGrabSystemInterface;
class CameraPathSystemInterface;
class LivingActionSystemInterface;
class MistSystemInterface;
class CloudSystemInterface;
class VillageLightSystemInterface;
class AbodeSoundSystemInterface;
class VillageTotemSystemInterface;
class SpellIconSystemInterface;
class ArtefactSystemInterface;
class HeartbeatSystemInterface;
class GameEndSystemInterface;
class FieldSystemInterface;
class FishFarmSystemInterface;
class AnimalSystemInterface;
class SnowSystemInterface;
class SnowfallSystemInterface;
class WaterRingSystemInterface;
class CreatureAnimationSystemInterface;
class CreatureMindSystemInterface;
class CreatureLocomotionSystemInterface;
class CreatureHairSystemInterface;
class CreatureAudioSystemInterface;
class CreatureObjectActionSystemInterface;
class CreatureHandSystemInterface;
class FootprintSystemInterface;
class FootpathSystemInterface;
class CreatureSkinSystemInterface;
class EditorSystemInterface;
class CreaturePhysiologySystemInterface;
class CreaturePenSystemInterface;
class LeashSystemInterface;
class CreatureFightSystemInterface;
class FightTranscriptSystemInterface;
class CreatureModeSystemInterface;
class CreatureCaveSystemInterface;
class TattooEditorSystemInterface;
class TattooMenuSystemInterface;
class FilmCreatureSystemInterface;
class CinematicDirectorSystemInterface;
class ScriptControlSystemInterface;
class DialogueControlSystemInterface;
class HelpSpeechSystemInterface;
class HighDetailSystemInterface;
class IntroSystemInterface;
class BoatLaunchSystemInterface;
class WalkPathSystemInterface;
class DanceSystemInterface;
class SharkSystemInterface;
class VideoSystemInterface;
class HelpTextSystemInterface;
class HelpProfileSystemInterface;
class ComputerSettingsSystemInterface;
class CreatureHelpSystemInterface;
class GuidanceSystemInterface;
class HelpMessageSystemInterface;
class TipBubbleSystemInterface;
class HandDemoSystemInterface;
class PlayerCommandSystemInterface;
class SyncChecksumSystemInterface;
class AdvisorSystemInterface;
class SoundTagSystemInterface;
class RainSystemInterface;
class ChimneySmokeSystemInterface;
class AbodeKnockSystemInterface;
class InfluenceSystemInterface;
class TownDesireSystemInterface;
class PathfindingSystemInterface;
class AlignmentSystemInterface;
class CameraHelpSystemInterface;
class TutorialSkipSystemInterface;
class PlayerProfileSystemInterface;
class ProfileFolderSystemInterface;
class MusicMoodSystemInterface;
class TempleExteriorSystemInterface;
class TempleDestructionSystemInterface;
class ComputerPlayerSystemInterface;
class PlayerSystemInterface;
class RenderingSystemInterface;
class TownSystemInterface;
class WorshipSiteSystemInterface;
class ResourceStoreSystemInterface;
class AnimatedStaticSystemInterface;
class ObjectDrawListSystemInterface;
class AppleBarrelSystemInterface;
class TimeSystemInterface;
class SimulationSystemInterface;
class VegetationInterface;
class SkySystemInterface;
class WeatherSystemInterface;
class ParticleSystemInterface;
class MagicSystemInterface;
class GestureEventsInterface;
class ReactionSystemInterface;
class TeleportSystemInterface;
class VortexSystemInterface;
class FixedOutlineSystemInterface;
class CreatureFizzSystemInterface;
class LiquidParticleSystemInterface;
class LandShadeSystemInterface;
class CreatureCarryOverSystemInterface;
class SaveGameSystemInterface;
class CreatureWebPageSystemInterface;
class LobbyClientSystemInterface;
class NetworkGameSystemInterface;
class RealWeatherSystemInterface;
class TornadoSystemInterface;
class MagicShieldSystemInterface;
class ForestSystemInterface;
class FireflySystemInterface;
class GestureSystemInterface;
class MiracleFxSystemInterface;
class PauseTextSystemInterface;
class LandSystemInterface;
class FireSystemInterface;
class ExplosionSystemInterface;
class FloatingNumberSystemInterface;
class RewardSystemInterface;
class ScriptObjectsSystemInterface;
class BuildingDamageSystemInterface;
class ScriptHighlightSystemInterface;
class InspectorSystemInterface;
} // namespace ecs::systems

void InitializeWindow(const std::string& title, int width, int height, windowing::DisplayMode displayMode, uint32_t extraFlags,
                      bool background = false);
/// The services that the game and the dedicated server share, first of all: the profiler, the events, the files and the
/// random numbers
void InitializeSimulationCore();
/// The services that show the world: the renderer, the debug GUI and the audio. Call it after the core services.
bool InitializeEngine(GraphicsBackend backend, bool vsync) noexcept;
/// The script machine and the systems of the world, which the game and the dedicated server share. The players' profiles
/// are kept as the store says.
bool InitializeSimulation(const player_profiles::Store& profiles) noexcept;
/// The services of the game on the player's computer: the input, the drawing of the world, the editor, the videos and
/// the online services. Call it after the simulation.
void InitializeClient(const player_profiles::Store& profiles);
/// The simulation, then the services of the game on the player's computer
bool InitializeGame(const player_profiles::Store& profiles) noexcept;
void InitializeLevel(const std::filesystem::path& path);
/// Starts a level on land that is generated rather than read from a file, as the flat testbed is
void InitializeLevel(const LandData& land);
/// The settings of this computer, read from the file when there is one (see computer_settings)
void InitializeComputerSettings(std::optional<std::filesystem::path> file);
void ShutDownServices();
/// Starts the debug inspector's server on a port of 127.0.0.1 (any free one for 0): false, with why in the log, if it
/// can't listen. Only in builds with the inspector.
bool StartInspector(uint16_t port);

struct Locator
{
	using config = entt::locator<EngineConfig>;
	using infoConstants = entt::locator<const InfoConstants>;
	using profiler = entt::locator<Profiler>;
	using events = entt::locator<EventManager>;
	using windowing = entt::locator<windowing::WindowingInterface>;
	using debugGui = entt::locator<debug::gui::DebugGuiInterface>;
	using filesystem = entt::locator<filesystem::FileSystemInterface>;
	using resources = entt::locator<resources::ResourcesInterface>;
	using rng = entt::locator<RandomNumberManagerInterface>;
	using gameRandom = entt::locator<GameRandomInterface>;
	using terrainSystem = entt::locator<LandIslandInterface>;
	using oceanSystem = entt::locator<OceanInterface>;
	using skySystem = entt::locator<ecs::systems::SkySystemInterface>;
	using audio = entt::locator<audio::AudioManagerInterface>;
	using camera = entt::locator<Camera>;
	using gameActionSystem = entt::locator<input::GameActionInterface>;
	using rendereringSystem = entt::locator<ecs::systems::RenderingSystemInterface>;
	using rendererInterface = entt::locator<graphics::RendererInterface>;
	using dynamicsSystem = entt::locator<ecs::systems::DynamicsSystemInterface>;
	using pickingSystem = entt::locator<ecs::systems::PickingSystemInterface>;
	using cameraBookmarkSystem = entt::locator<ecs::systems::CameraBookmarkSystemInterface>;
	using cameraZoneSystem = entt::locator<ecs::systems::CameraZoneSystemInterface>;
	using cameraPathSystem = entt::locator<ecs::systems::CameraPathSystemInterface>;
	using livingActionSystem = entt::locator<ecs::systems::LivingActionSystemInterface>;
	using townSystem = entt::locator<ecs::systems::TownSystemInterface>;
	using worshipSiteSystem = entt::locator<ecs::systems::WorshipSiteSystemInterface>;
	using resourceStoreSystem = entt::locator<ecs::systems::ResourceStoreSystemInterface>;
	using animatedStaticSystem = entt::locator<ecs::systems::AnimatedStaticSystemInterface>;
	using objectDrawListSystem = entt::locator<ecs::systems::ObjectDrawListSystemInterface>;
	using appleBarrelSystem = entt::locator<ecs::systems::AppleBarrelSystemInterface>;
	using weatherSystem = entt::locator<ecs::systems::WeatherSystemInterface>;
	using pathfindingSystem = entt::locator<ecs::systems::PathfindingSystemInterface>;
	using entitiesRegistry = entt::locator<ecs::Registry>;
	/// The things that only the frames make and draw, out of the synced world
	using drawnRegistry = entt::locator<ecs::DrawnRegistry>;
	using entitiesMap = entt::locator<ecs::MapInterface>;
	using playerSystem = entt::locator<ecs::systems::PlayerSystemInterface>;
	using alignmentSystem = entt::locator<ecs::systems::AlignmentSystemInterface>;
	using cameraHelpSystem = entt::locator<ecs::systems::CameraHelpSystemInterface>;
	using tutorialSkipSystem = entt::locator<ecs::systems::TutorialSkipSystemInterface>;
	using playerProfileSystem = entt::locator<ecs::systems::PlayerProfileSystemInterface>;
	using profileFolderSystem = entt::locator<ecs::systems::ProfileFolderSystemInterface>;
	using musicMoodSystem = entt::locator<ecs::systems::MusicMoodSystemInterface>;
	using templeExteriorSystem = entt::locator<ecs::systems::TempleExteriorSystemInterface>;
	using templeDestructionSystem = entt::locator<ecs::systems::TempleDestructionSystemInterface>;
	using computerPlayerSystem = entt::locator<ecs::systems::ComputerPlayerSystemInterface>;
	using handSystem = entt::locator<ecs::systems::HandSystemInterface>;
	using handGrabSystem = entt::locator<ecs::systems::HandGrabSystemInterface>;
	using temple = entt::locator<TempleInteriorInterface>;
	using time = entt::locator<ecs::systems::TimeSystemInterface>;
	using simulation = entt::locator<ecs::systems::SimulationSystemInterface>;
	using vegetation = entt::locator<ecs::systems::VegetationInterface>;
	using mistSystem = entt::locator<ecs::systems::MistSystemInterface>;
	using cloudSystem = entt::locator<ecs::systems::CloudSystemInterface>;
	using villageLightSystem = entt::locator<ecs::systems::VillageLightSystemInterface>;
	using abodeSoundSystem = entt::locator<ecs::systems::AbodeSoundSystemInterface>;
	using villageTotemSystem = entt::locator<ecs::systems::VillageTotemSystemInterface>;
	using spellIconSystem = entt::locator<ecs::systems::SpellIconSystemInterface>;
	using artefactSystem = entt::locator<ecs::systems::ArtefactSystemInterface>;
	using heartbeatSystem = entt::locator<ecs::systems::HeartbeatSystemInterface>;
	using gameEndSystem = entt::locator<ecs::systems::GameEndSystemInterface>;
	using fieldSystem = entt::locator<ecs::systems::FieldSystemInterface>;
	using fishFarmSystem = entt::locator<ecs::systems::FishFarmSystemInterface>;
	using animalSystem = entt::locator<ecs::systems::AnimalSystemInterface>;
	using snowSystem = entt::locator<ecs::systems::SnowSystemInterface>;
	using snowfallSystem = entt::locator<ecs::systems::SnowfallSystemInterface>;
	using waterRingSystem = entt::locator<ecs::systems::WaterRingSystemInterface>;
	using creatureAnimationSystem = entt::locator<ecs::systems::CreatureAnimationSystemInterface>;
	using creatureMindSystem = entt::locator<ecs::systems::CreatureMindSystemInterface>;
	using creatureLocomotionSystem = entt::locator<ecs::systems::CreatureLocomotionSystemInterface>;
	using creatureHairSystem = entt::locator<ecs::systems::CreatureHairSystemInterface>;
	using creatureAudioSystem = entt::locator<ecs::systems::CreatureAudioSystemInterface>;
	using creatureObjectActionSystem = entt::locator<ecs::systems::CreatureObjectActionSystemInterface>;
	using creatureHandSystem = entt::locator<ecs::systems::CreatureHandSystemInterface>;
	using footprintSystem = entt::locator<ecs::systems::FootprintSystemInterface>;
	using footpathSystem = entt::locator<ecs::systems::FootpathSystemInterface>;
	using creatureSkinSystem = entt::locator<ecs::systems::CreatureSkinSystemInterface>;
	using editorSystem = entt::locator<ecs::systems::EditorSystemInterface>;
	using creaturePhysiologySystem = entt::locator<ecs::systems::CreaturePhysiologySystemInterface>;
	using creaturePenSystem = entt::locator<ecs::systems::CreaturePenSystemInterface>;
	using leashSystem = entt::locator<ecs::systems::LeashSystemInterface>;
	using creatureFightSystem = entt::locator<ecs::systems::CreatureFightSystemInterface>;
	/// Only in builds with the inspector, and only once the inspector started
	using fightTranscript = entt::locator<ecs::systems::FightTranscriptSystemInterface>;
	using creatureModeSystem = entt::locator<ecs::systems::CreatureModeSystemInterface>;
	using creatureCaveSystem = entt::locator<ecs::systems::CreatureCaveSystemInterface>;
	using tattooEditorSystem = entt::locator<ecs::systems::TattooEditorSystemInterface>;
	using tattooMenuSystem = entt::locator<ecs::systems::TattooMenuSystemInterface>;
	using filmCreatureSystem = entt::locator<ecs::systems::FilmCreatureSystemInterface>;
	using cinematicDirectorSystem = entt::locator<ecs::systems::CinematicDirectorSystemInterface>;
	using scriptControlSystem = entt::locator<ecs::systems::ScriptControlSystemInterface>;
	using dialogueControlSystem = entt::locator<ecs::systems::DialogueControlSystemInterface>;
	using helpSpeechSystem = entt::locator<ecs::systems::HelpSpeechSystemInterface>;
	using highDetailSystem = entt::locator<ecs::systems::HighDetailSystemInterface>;
	using introSystem = entt::locator<ecs::systems::IntroSystemInterface>;
	using boatLaunchSystem = entt::locator<ecs::systems::BoatLaunchSystemInterface>;
	using walkPathSystem = entt::locator<ecs::systems::WalkPathSystemInterface>;
	using danceSystem = entt::locator<ecs::systems::DanceSystemInterface>;
	using sharkSystem = entt::locator<ecs::systems::SharkSystemInterface>;
	using videoSystem = entt::locator<ecs::systems::VideoSystemInterface>;
	using helpTextSystem = entt::locator<ecs::systems::HelpTextSystemInterface>;
	using helpProfileSystem = entt::locator<ecs::systems::HelpProfileSystemInterface>;
	using computerSettingsSystem = entt::locator<ecs::systems::ComputerSettingsSystemInterface>;
	using creatureHelpSystem = entt::locator<ecs::systems::CreatureHelpSystemInterface>;
	using helpMessageSystem = entt::locator<ecs::systems::HelpMessageSystemInterface>;
	using guidanceSystem = entt::locator<ecs::systems::GuidanceSystemInterface>;
	using tipBubbleSystem = entt::locator<ecs::systems::TipBubbleSystemInterface>;
	using handDemoSystem = entt::locator<ecs::systems::HandDemoSystemInterface>;
	using playerCommandSystem = entt::locator<ecs::systems::PlayerCommandSystemInterface>;
	using syncChecksumSystem = entt::locator<ecs::systems::SyncChecksumSystemInterface>;
	using advisorSystem = entt::locator<ecs::systems::AdvisorSystemInterface>;
	using soundTagSystem = entt::locator<ecs::systems::SoundTagSystemInterface>;
	using rainSystem = entt::locator<ecs::systems::RainSystemInterface>;
	using chimneySmokeSystem = entt::locator<ecs::systems::ChimneySmokeSystemInterface>;
	using abodeKnockSystem = entt::locator<ecs::systems::AbodeKnockSystemInterface>;
	using influenceSystem = entt::locator<ecs::systems::InfluenceSystemInterface>;
	using townDesireSystem = entt::locator<ecs::systems::TownDesireSystemInterface>;
	using particleSystem = entt::locator<ecs::systems::ParticleSystemInterface>;
	using magicSystem = entt::locator<ecs::systems::MagicSystemInterface>;
	using gestureEvents = entt::locator<ecs::systems::GestureEventsInterface>;
	using reactionSystem = entt::locator<ecs::systems::ReactionSystemInterface>;
	using teleportSystem = entt::locator<ecs::systems::TeleportSystemInterface>;
	using vortexSystem = entt::locator<ecs::systems::VortexSystemInterface>;
	using fixedOutlineSystem = entt::locator<ecs::systems::FixedOutlineSystemInterface>;
	using creatureFizzSystem = entt::locator<ecs::systems::CreatureFizzSystemInterface>;
	using liquidParticleSystem = entt::locator<ecs::systems::LiquidParticleSystemInterface>;
	using landShadeSystem = entt::locator<ecs::systems::LandShadeSystemInterface>;
	using creatureCarryOverSystem = entt::locator<ecs::systems::CreatureCarryOverSystemInterface>;
	using saveGameSystem = entt::locator<ecs::systems::SaveGameSystemInterface>;
	using creatureWebPageSystem = entt::locator<ecs::systems::CreatureWebPageSystemInterface>;
	/// The HTTPS client of the web calls of the game
	using httpClient = entt::locator<online::HttpClientInterface>;
	/// The WSS client of the push channel of the lobby and of the sockets of the game servers
	using webSocketClient = entt::locator<online::WebSocketClientInterface>;
	/// The client of the lobby (bwgame-service)
	using lobbyClient = entt::locator<ecs::systems::LobbyClientSystemInterface>;
	/// The network game of this computer: the session with a game server, which gives the turns of the world
	using networkGame = entt::locator<ecs::systems::NetworkGameSystemInterface>;
	/// The real-world weather of a single-player game, off unless the player turns it on
	using realWeatherSystem = entt::locator<ecs::systems::RealWeatherSystemInterface>;
	using tornadoSystem = entt::locator<ecs::systems::TornadoSystemInterface>;
	using magicShieldSystem = entt::locator<ecs::systems::MagicShieldSystemInterface>;
	using forestSystem = entt::locator<ecs::systems::ForestSystemInterface>;
	using fireflySystem = entt::locator<ecs::systems::FireflySystemInterface>;
	using gestureSystem = entt::locator<ecs::systems::GestureSystemInterface>;
	using miracleFxSystem = entt::locator<ecs::systems::MiracleFxSystemInterface>;
	using pauseTextSystem = entt::locator<ecs::systems::PauseTextSystemInterface>;
	using landSystem = entt::locator<ecs::systems::LandSystemInterface>;
	using fireSystem = entt::locator<ecs::systems::FireSystemInterface>;
	using explosionSystem = entt::locator<ecs::systems::ExplosionSystemInterface>;
	using floatingNumberSystem = entt::locator<ecs::systems::FloatingNumberSystemInterface>;
	using rewardSystem = entt::locator<ecs::systems::RewardSystemInterface>;
	using scriptObjects = entt::locator<ecs::systems::ScriptObjectsSystemInterface>;
	using buildingDamageSystem = entt::locator<ecs::systems::BuildingDamageSystemInterface>;
	using scriptHighlightSystem = entt::locator<ecs::systems::ScriptHighlightSystemInterface>;
	/// Only in builds with the inspector, and only once --inspect-port started it
	using inspector = entt::locator<ecs::systems::InspectorSystemInterface>;
	using vm = entt::locator<lhvm::LHVM>;
	using chlapi = entt::locator<chlapi::CHLApi>;
};
} // namespace openblack
