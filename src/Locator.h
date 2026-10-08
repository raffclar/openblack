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

namespace openblack
{
enum class GraphicsBackend : uint8_t;
struct EngineConfig;
class Camera;
class EventManager;
class TimeSystemInterface;
class GameRandomInterface;
class LandIslandInterface;
class OceanInterface;
struct ModLoaderStatus;
class Profiler;
class RandomNumberManagerInterface;
class SkyInterface;
class TempleInteriorInterface;

namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;

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
class AlignmentSystemInterface;
class AnimalAISystemInterface;
class CameraBookmarkSystemInterface;
class CameraPathSystemInterface;
class DayNightClockSystemInterface;
class ScreenFadeSystemInterface;
class ScreenshotRequestSystemInterface;
class ParticleSystemInterface;
class VillageLightSystemInterface;
class RenderFrameSystemInterface;
class SkyFrameSystemInterface;
class LandAvoidSystemInterface;
class AudioStateInterface;
class WorldEffectsInterface;
class ScriptStateInterface;
class DebugHooksInterface;
class HandTapRegistryInterface;
class VideoSystemInterface;
class PhysicsObjectsSystemInterface;
class WeatherSystemInterface;
class CreatureModeSystemInterface;
class CreatureCaveSystemInterface;
class DynamicsSystemInterface;
class EditorSystemInterface;
class FallingSpellSystemInterface;
class FireEffectSystemInterface;
class FireGraphicSystemInterface;
class FireSoundSystemInterface;
class ForestSystemInterface;
class GameStatsSystemInterface;
class HandMagicStateInterface;
class HandSystemInterface;
class InputStateInterface;
class LandBalanceSystemInterface;
class LivingActionSystemInterface;
class MagicObjectsSystemInterface;
class MapCellsSystemInterface;
class MapScriptSystemInterface;
class MapShapeProviderInterface;
class MeshBoxProviderInterface;
class ObjectCreationIndexSystemInterface;
class PathfindingSystemInterface;
class PlayerSystemInterface;
class ReactionsSystemInterface;
class RenderingSystemInterface;
class RoutePlanStateSystemInterface;
class SpellSystemInterface;
class TempleExteriorSystemInterface;
class ToBeDeletedSystemInterface;
class TownStateSystemInterface;
class TownCellObjectsInterface;
class TownSystemInterface;
class TreeSystemInterface;
class VillagerBuildingSitesInterface;
class VillagerChildFactoryInterface;
class VillagerDiscipleJobsInterface;
class VillagerFieldsInterface;
class VillagerFishFarmsInterface;
class VillagerRulesInterface;
class VillagerStateSystemInterface;
class VillagerStoresInterface;
class VillagerTentQueriesInterface;
class VillagerWorldQueriesInterface;
class VillagerWorshipCheckInterface;
class WorshipStateInterface;
class CreatureAnimationSystemInterface;
class CreatureMindSystemInterface;
class CreatureLocomotionSystemInterface;
class CreatureHairSystemInterface;
class CreatureAudioSystemInterface;
class CreatureObjectActionSystemInterface;
class CreatureHandSystemInterface;
class FootprintSystemInterface;
class CreatureSkinSystemInterface;
class CreaturePhysiologySystemInterface;
class LeashSystemInterface;
class CreatureFightSystemInterface;
} // namespace ecs::systems

void InitializeWindow(const std::string& title, int width, int height, windowing::DisplayMode displayMode, uint32_t extraFlags);
/// The game clock, first of all: the engine timer, the audio device and the game read it
void InitializeClock();
/// The state the game keeps for its whole life, made fresh when the Game is made (the script fade, the day / night
/// clock, the map script's globals and the frame count with its screenshot request)
void InitializeGameState();
bool InitializeEngine(GraphicsBackend backend, bool vsync) noexcept;
bool InitializeGame() noexcept;
void InitializeLevel(const std::filesystem::path& path);
void ShutDownServices();

namespace audio
{
class AudioManagerInterface;
}

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
	using time = entt::locator<TimeSystemInterface>;
	using particleSystem = entt::locator<ecs::systems::ParticleSystemInterface>;
	using villageLightSystem = entt::locator<ecs::systems::VillageLightSystemInterface>;
	using renderFrameSystem = entt::locator<ecs::systems::RenderFrameSystemInterface>;
	using skyFrameSystem = entt::locator<ecs::systems::SkyFrameSystemInterface>;
	using landAvoidSystem = entt::locator<ecs::systems::LandAvoidSystemInterface>;
	using debugHooks = entt::locator<ecs::systems::DebugHooksInterface>;
	using scriptState = entt::locator<ecs::systems::ScriptStateInterface>;
	using handTapRegistry = entt::locator<ecs::systems::HandTapRegistryInterface>;
	using videoSystem = entt::locator<ecs::systems::VideoSystemInterface>;
	using worldEffects = entt::locator<ecs::systems::WorldEffectsInterface>;
	// the audio service: the functions of Audio.h, over our engine (raffclar's name)
	using audio = entt::locator<::openblack::audio::AudioManagerInterface>;
	using audioState = entt::locator<ecs::systems::AudioStateInterface>;
	using physicsObjectsSystem = entt::locator<ecs::systems::PhysicsObjectsSystemInterface>;
	using weatherSystem = entt::locator<ecs::systems::WeatherSystemInterface>;
	using terrainSystem = entt::locator<LandIslandInterface>;
	using oceanSystem = entt::locator<OceanInterface>;
	using skySystem = entt::locator<SkyInterface>;
	using camera = entt::locator<Camera>;
	using gameActionSystem = entt::locator<input::GameActionInterface>;
	using rendereringSystem = entt::locator<ecs::systems::RenderingSystemInterface>;
	using rendererInterface = entt::locator<graphics::RendererInterface>;
	using dynamicsSystem = entt::locator<ecs::systems::DynamicsSystemInterface>;
	using editorSystem = entt::locator<ecs::systems::EditorSystemInterface>;
	using creatureModeSystem = entt::locator<ecs::systems::CreatureModeSystemInterface>;
	using creatureCaveSystem = entt::locator<ecs::systems::CreatureCaveSystemInterface>;
	using cameraBookmarkSystem = entt::locator<ecs::systems::CameraBookmarkSystemInterface>;
	using cameraPathSystem = entt::locator<ecs::systems::CameraPathSystemInterface>;
	using livingActionSystem = entt::locator<ecs::systems::LivingActionSystemInterface>;
	using townSystem = entt::locator<ecs::systems::TownSystemInterface>;
	using pathfindingSystem = entt::locator<ecs::systems::PathfindingSystemInterface>;
	using entitiesRegistry = entt::locator<ecs::Registry>;
	using entitiesMap = entt::locator<ecs::MapInterface>;
	using playerSystem = entt::locator<ecs::systems::PlayerSystemInterface>;
	using alignmentSystem = entt::locator<ecs::systems::AlignmentSystemInterface>;
	using handSystem = entt::locator<ecs::systems::HandSystemInterface>;
	using temple = entt::locator<TempleInteriorInterface>;
	using vm = entt::locator<lhvm::LHVM>;
	using chlapi = entt::locator<chlapi::CHLApi>;
	using villagerFields = entt::locator<ecs::systems::VillagerFieldsInterface>;
	using villagerFishFarms = entt::locator<ecs::systems::VillagerFishFarmsInterface>;
	using villagerBuildingSites = entt::locator<ecs::systems::VillagerBuildingSitesInterface>;
	using villagerStores = entt::locator<ecs::systems::VillagerStoresInterface>;
	using villagerTentQueries = entt::locator<ecs::systems::VillagerTentQueriesInterface>;
	using villagerRules = entt::locator<ecs::systems::VillagerRulesInterface>;
	using villagerWorldQueries = entt::locator<ecs::systems::VillagerWorldQueriesInterface>;
	using villagerWorshipCheck = entt::locator<ecs::systems::VillagerWorshipCheckInterface>;
	using villagerChildFactory = entt::locator<ecs::systems::VillagerChildFactoryInterface>;
	using villagerDiscipleJobs = entt::locator<ecs::systems::VillagerDiscipleJobsInterface>;
	using townCellObjects = entt::locator<ecs::systems::TownCellObjectsInterface>;
	using mapShapeProvider = entt::locator<ecs::systems::MapShapeProviderInterface>;
	using meshBoxProvider = entt::locator<ecs::systems::MeshBoxProviderInterface>;
	using toBeDeletedSystem = entt::locator<ecs::systems::ToBeDeletedSystemInterface>;
	using objectCreationIndexSystem = entt::locator<ecs::systems::ObjectCreationIndexSystemInterface>;
	using mapCellsSystem = entt::locator<ecs::systems::MapCellsSystemInterface>;
	using reactionsSystem = entt::locator<ecs::systems::ReactionsSystemInterface>;
	using fireEffectSystem = entt::locator<ecs::systems::FireEffectSystemInterface>;
	using fireGraphicSystem = entt::locator<ecs::systems::FireGraphicSystemInterface>;
	using fireSoundSystem = entt::locator<ecs::systems::FireSoundSystemInterface>;
	using forestSystem = entt::locator<ecs::systems::ForestSystemInterface>;
	using treeSystem = entt::locator<ecs::systems::TreeSystemInterface>;
	using landBalanceSystem = entt::locator<ecs::systems::LandBalanceSystemInterface>;
	using animalAISystem = entt::locator<ecs::systems::AnimalAISystemInterface>;
	using spellSystem = entt::locator<ecs::systems::SpellSystemInterface>;
	using magicObjectsSystem = entt::locator<ecs::systems::MagicObjectsSystemInterface>;
	using townStateSystem = entt::locator<ecs::systems::TownStateSystemInterface>;
	using villagerStateSystem = entt::locator<ecs::systems::VillagerStateSystemInterface>;
	using handMagicState = entt::locator<ecs::systems::HandMagicStateInterface>;
	using routePlanStateSystem = entt::locator<ecs::systems::RoutePlanStateSystemInterface>;
	using fallingSpellSystem = entt::locator<ecs::systems::FallingSpellSystemInterface>;
	using templeExteriorSystem = entt::locator<ecs::systems::TempleExteriorSystemInterface>;
	/// The creatures' systems (emplaced, none called yet; the locomotion one is made again with each land)
	using creatureAnimationSystem = entt::locator<ecs::systems::CreatureAnimationSystemInterface>;
	using creatureMindSystem = entt::locator<ecs::systems::CreatureMindSystemInterface>;
	using creatureLocomotionSystem = entt::locator<ecs::systems::CreatureLocomotionSystemInterface>;
	using creatureHairSystem = entt::locator<ecs::systems::CreatureHairSystemInterface>;
	using creatureAudioSystem = entt::locator<ecs::systems::CreatureAudioSystemInterface>;
	using creatureObjectActionSystem = entt::locator<ecs::systems::CreatureObjectActionSystemInterface>;
	using creatureHandSystem = entt::locator<ecs::systems::CreatureHandSystemInterface>;
	using footprintSystem = entt::locator<ecs::systems::FootprintSystemInterface>;
	using creatureSkinSystem = entt::locator<ecs::systems::CreatureSkinSystemInterface>;
	using creaturePhysiologySystem = entt::locator<ecs::systems::CreaturePhysiologySystemInterface>;
	using leashSystem = entt::locator<ecs::systems::LeashSystemInterface>;
	using creatureFightSystem = entt::locator<ecs::systems::CreatureFightSystemInterface>;
	using worshipState = entt::locator<ecs::systems::WorshipStateInterface>;
	using inputState = entt::locator<ecs::systems::InputStateInterface>;
	using gameStatsSystem = entt::locator<ecs::systems::GameStatsSystemInterface>;
	using mapScriptSystem = entt::locator<ecs::systems::MapScriptSystemInterface>;
	using screenFade = entt::locator<ecs::systems::ScreenFadeSystemInterface>;
	using dayNightClock = entt::locator<ecs::systems::DayNightClockSystemInterface>;
	using screenshotRequest = entt::locator<ecs::systems::ScreenshotRequestSystemInterface>;
	/// only when the mod loader library was loaded at start-up
	using modLoader = entt::locator<ModLoaderStatus>;
};
} // namespace openblack
