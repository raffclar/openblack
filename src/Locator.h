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
class MapInterface;
} // namespace ecs

namespace ecs::systems
{
class CameraBookmarkSystemInterface;
class DynamicsSystemInterface;
class PickingSystemInterface;
class HandSystemInterface;
class HandGrabSystemInterface;
class CameraPathSystemInterface;
class LivingActionSystemInterface;
class MistSystemInterface;
class CloudSystemInterface;
class VillageLightSystemInterface;
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
class CreatureSkinSystemInterface;
class EditorSystemInterface;
class CreaturePhysiologySystemInterface;
class LeashSystemInterface;
class CreatureFightSystemInterface;
class CreatureModeSystemInterface;
class CreatureCaveSystemInterface;
class TattooEditorSystemInterface;
class CinematicDirectorSystemInterface;
class ScriptControlSystemInterface;
class DialogueControlSystemInterface;
class HelpSpeechSystemInterface;
class HighDetailSystemInterface;
class WalkPathSystemInterface;
class DanceSystemInterface;
class WhaleSystemInterface;
class VideoSystemInterface;
class HelpTextSystemInterface;
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
class TempleExteriorSystemInterface;
class TempleDestructionSystemInterface;
class PlayerSystemInterface;
class RenderingSystemInterface;
class TownSystemInterface;
class ResourceStoreSystemInterface;
class TimeSystemInterface;
class VegetationInterface;
class SkySystemInterface;
class WeatherSystemInterface;
class ParticleSystemInterface;
class MagicSystemInterface;
class GestureEventsInterface;
class ReactionSystemInterface;
class TeleportSystemInterface;
class VortexSystemInterface;
class CreatureFizzSystemInterface;
class CreatureCarryOverSystemInterface;
class TornadoSystemInterface;
class MagicShieldSystemInterface;
class ForestSystemInterface;
class FireflySystemInterface;
class GestureSystemInterface;
class MiracleFxSystemInterface;
class FireSystemInterface;
class ExplosionSystemInterface;
class RewardSystemInterface;
class ScriptObjectsSystemInterface;
class BuildingDamageSystemInterface;
class InspectorSystemInterface;
} // namespace ecs::systems

void InitializeWindow(const std::string& title, int width, int height, windowing::DisplayMode displayMode, uint32_t extraFlags);
bool InitializeEngine(GraphicsBackend backend, bool vsync) noexcept;
bool InitializeGame() noexcept;
void InitializeLevel(const std::filesystem::path& path);
/// Starts a level on land that is generated rather than read from a file, as the flat testbed is
void InitializeLevel(const LandData& land);
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
	using cameraPathSystem = entt::locator<ecs::systems::CameraPathSystemInterface>;
	using livingActionSystem = entt::locator<ecs::systems::LivingActionSystemInterface>;
	using townSystem = entt::locator<ecs::systems::TownSystemInterface>;
	using resourceStoreSystem = entt::locator<ecs::systems::ResourceStoreSystemInterface>;
	using weatherSystem = entt::locator<ecs::systems::WeatherSystemInterface>;
	using pathfindingSystem = entt::locator<ecs::systems::PathfindingSystemInterface>;
	using entitiesRegistry = entt::locator<ecs::Registry>;
	using entitiesMap = entt::locator<ecs::MapInterface>;
	using playerSystem = entt::locator<ecs::systems::PlayerSystemInterface>;
	using alignmentSystem = entt::locator<ecs::systems::AlignmentSystemInterface>;
	using cameraHelpSystem = entt::locator<ecs::systems::CameraHelpSystemInterface>;
	using tutorialSkipSystem = entt::locator<ecs::systems::TutorialSkipSystemInterface>;
	using playerProfileSystem = entt::locator<ecs::systems::PlayerProfileSystemInterface>;
	using templeExteriorSystem = entt::locator<ecs::systems::TempleExteriorSystemInterface>;
	using templeDestructionSystem = entt::locator<ecs::systems::TempleDestructionSystemInterface>;
	using handSystem = entt::locator<ecs::systems::HandSystemInterface>;
	using handGrabSystem = entt::locator<ecs::systems::HandGrabSystemInterface>;
	using temple = entt::locator<TempleInteriorInterface>;
	using time = entt::locator<ecs::systems::TimeSystemInterface>;
	using vegetation = entt::locator<ecs::systems::VegetationInterface>;
	using mistSystem = entt::locator<ecs::systems::MistSystemInterface>;
	using cloudSystem = entt::locator<ecs::systems::CloudSystemInterface>;
	using villageLightSystem = entt::locator<ecs::systems::VillageLightSystemInterface>;
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
	using creatureSkinSystem = entt::locator<ecs::systems::CreatureSkinSystemInterface>;
	using editorSystem = entt::locator<ecs::systems::EditorSystemInterface>;
	using creaturePhysiologySystem = entt::locator<ecs::systems::CreaturePhysiologySystemInterface>;
	using leashSystem = entt::locator<ecs::systems::LeashSystemInterface>;
	using creatureFightSystem = entt::locator<ecs::systems::CreatureFightSystemInterface>;
	using creatureModeSystem = entt::locator<ecs::systems::CreatureModeSystemInterface>;
	using creatureCaveSystem = entt::locator<ecs::systems::CreatureCaveSystemInterface>;
	using tattooEditorSystem = entt::locator<ecs::systems::TattooEditorSystemInterface>;
	using cinematicDirectorSystem = entt::locator<ecs::systems::CinematicDirectorSystemInterface>;
	using scriptControlSystem = entt::locator<ecs::systems::ScriptControlSystemInterface>;
	using dialogueControlSystem = entt::locator<ecs::systems::DialogueControlSystemInterface>;
	using helpSpeechSystem = entt::locator<ecs::systems::HelpSpeechSystemInterface>;
	using highDetailSystem = entt::locator<ecs::systems::HighDetailSystemInterface>;
	using walkPathSystem = entt::locator<ecs::systems::WalkPathSystemInterface>;
	using danceSystem = entt::locator<ecs::systems::DanceSystemInterface>;
	using whaleSystem = entt::locator<ecs::systems::WhaleSystemInterface>;
	using videoSystem = entt::locator<ecs::systems::VideoSystemInterface>;
	using helpTextSystem = entt::locator<ecs::systems::HelpTextSystemInterface>;
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
	using creatureFizzSystem = entt::locator<ecs::systems::CreatureFizzSystemInterface>;
	using creatureCarryOverSystem = entt::locator<ecs::systems::CreatureCarryOverSystemInterface>;
	using tornadoSystem = entt::locator<ecs::systems::TornadoSystemInterface>;
	using magicShieldSystem = entt::locator<ecs::systems::MagicShieldSystemInterface>;
	using forestSystem = entt::locator<ecs::systems::ForestSystemInterface>;
	using fireflySystem = entt::locator<ecs::systems::FireflySystemInterface>;
	using gestureSystem = entt::locator<ecs::systems::GestureSystemInterface>;
	using miracleFxSystem = entt::locator<ecs::systems::MiracleFxSystemInterface>;
	using fireSystem = entt::locator<ecs::systems::FireSystemInterface>;
	using explosionSystem = entt::locator<ecs::systems::ExplosionSystemInterface>;
	using rewardSystem = entt::locator<ecs::systems::RewardSystemInterface>;
	using scriptObjects = entt::locator<ecs::systems::ScriptObjectsSystemInterface>;
	using buildingDamageSystem = entt::locator<ecs::systems::BuildingDamageSystemInterface>;
	/// Only in builds with the inspector, and only once --inspect-port started it
	using inspector = entt::locator<ecs::systems::InspectorSystemInterface>;
	using vm = entt::locator<lhvm::LHVM>;
	using chlapi = entt::locator<chlapi::CHLApi>;
};
} // namespace openblack
