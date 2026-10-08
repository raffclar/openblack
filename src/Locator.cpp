/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Locator.h"

#include <cstdlib>
#include <ctime>

#include <algorithm>

#define LOCATOR_IMPLEMENTATIONS

#include <spdlog/spdlog.h>

#include "3D/Clouds.h"
#include "3D/Implementations/LandIsland.h"
#include "3D/Implementations/Ocean.h"
#include "3D/Implementations/Sky.h"
#include "3D/Implementations/TempleInterior.h"
#include "3D/Implementations/UnloadedIsland.h"
#include "Audio/AudioManager.h"
#include "Audio/Device/Device.h"
#include "CHLApi.h"
#include "Common/EventManager.h"
#include "Common/GameRandomProduction.h"
#include "Common/RandomNumberManagerProduction.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/CreatureMimic.h"
#include "ECS/CreaturePhysics.h"
#include "ECS/MapProduction.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AlignmentSystem.h"
#include "ECS/Systems/Implementations/AnimalAISystem.h"
#include "ECS/Systems/Implementations/AudioState.h"
#include "ECS/Systems/Implementations/CameraBookmarkSystem.h"
#include "ECS/Systems/Implementations/CameraPathSystem.h"
#include "ECS/Systems/Implementations/CreatureAnimationSystem.h"
#include "ECS/Systems/Implementations/CreatureAudioSystem.h"
#include "ECS/Systems/Implementations/CreatureCaveSystem.h"
#include "ECS/Systems/Implementations/CreatureFightSystem.h"
#include "ECS/Systems/Implementations/CreatureHairSystem.h"
#include "ECS/Systems/Implementations/CreatureHandSystem.h"
#include "ECS/Systems/Implementations/CreatureLocomotionSystem.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "ECS/Systems/Implementations/CreatureModeSystem.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "ECS/Systems/Implementations/CreaturePhysiologySystem.h"
#include "ECS/Systems/Implementations/CreatureSkinSystem.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/Implementations/DebugHooks.h"
#include "ECS/Systems/Implementations/EditorSystem.h"
#include "ECS/Systems/Implementations/FallingSpellSystem.h"
#include "ECS/Systems/Implementations/FireEffectSystem.h"
#include "ECS/Systems/Implementations/FireGraphicSystem.h"
#include "ECS/Systems/Implementations/FireSoundSystem.h"
#include "ECS/Systems/Implementations/FootprintSystem.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "ECS/Systems/Implementations/GameStatsSystem.h"
#include "ECS/Systems/Implementations/HandMagicState.h"
#include "ECS/Systems/Implementations/HandSystem.h"
#include "ECS/Systems/Implementations/HandTapRegistry.h"
#include "ECS/Systems/Implementations/InputState.h"
#include "ECS/Systems/Implementations/LandAvoidSystem.h"
#include "ECS/Systems/Implementations/LandBalanceSystem.h"
#include "ECS/Systems/Implementations/LandPickSystem.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "ECS/Systems/Implementations/LivingActionSystem.h"
#include "ECS/Systems/Implementations/MagicObjectsSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/MapScriptSystem.h"
#include "ECS/Systems/Implementations/MapShapeProvider.h"
#include "ECS/Systems/Implementations/MeshBoxProvider.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "ECS/Systems/Implementations/ParticleSystem.h"
#include "ECS/Systems/Implementations/PathfindingSystem.h"
#include "ECS/Systems/Implementations/PhysicsObjectsSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/Implementations/ReactionsSystem.h"
#include "ECS/Systems/Implementations/RenderFrameSystem.h"
#include "ECS/Systems/Implementations/RenderingSystem.h"
#include "ECS/Systems/Implementations/RoutePlanStateSystem.h"
#include "ECS/Systems/Implementations/ScreenFadeSystem.h"
#include "ECS/Systems/Implementations/ScreenshotRequestSystem.h"
#include "ECS/Systems/Implementations/ScriptState.h"
#include "ECS/Systems/Implementations/SkyFrameSystem.h"
#include "ECS/Systems/Implementations/SpellSystem.h"
#include "ECS/Systems/Implementations/TempleExteriorSystem.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "ECS/Systems/Implementations/ToBeDeletedSystem.h"
#include "ECS/Systems/Implementations/TownCellObjects.h"
#include "ECS/Systems/Implementations/TownStateSystem.h"
#include "ECS/Systems/Implementations/TownSystem.h"
#include "ECS/Systems/Implementations/TreeSystem.h"
#include "ECS/Systems/Implementations/VideoSystem.h"
#include "ECS/Systems/Implementations/VillageLightSystem.h"
#include "ECS/Systems/Implementations/VillagerBuildingSites.h"
#include "ECS/Systems/Implementations/VillagerChildFactory.h"
#include "ECS/Systems/Implementations/VillagerDiscipleJobs.h"
#include "ECS/Systems/Implementations/VillagerFields.h"
#include "ECS/Systems/Implementations/VillagerFishFarms.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/Implementations/VillagerRules.h"
#include "ECS/Systems/Implementations/VillagerStateSystem.h"
#include "ECS/Systems/Implementations/VillagerStores.h"
#include "ECS/Systems/Implementations/VillagerTentQueries.h"
#include "ECS/Systems/Implementations/VillagerWorldQueries.h"
#include "ECS/Systems/Implementations/VillagerWorshipCheck.h"
#include "ECS/Systems/Implementations/WeatherSystem.h"
#include "ECS/Systems/Implementations/WorldEffects.h"
#include "ECS/Systems/Implementations/WorshipState.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Villager/VillagerDeath.h"
#include "GameClock.h"
#include "Graphics/RendererInterface.h"
#include "Input/GameActionMap.h"
#include "LHVM.h"
#include "Magic/Objects/MagicTree.h"
#include "Magic/Spells/SpellClasses.h"
#include "ModLoaderStatus.h"
#include "Profiler.h"
#include "Resources/Resources.h"
#include "Windowing/Sdl2WindowingSystem.h"
#if __ANDROID__
#include "FileSystem/AndroidFileSystem.h"
#else
#include "FileSystem/DefaultFileSystem.h"
#endif

using namespace openblack::audio;
using namespace openblack::filesystem;
using openblack::GameRandomProduction;
using openblack::LandIsland;
using openblack::RandomNumberManagerProduction;
using openblack::TempleInterior;
using openblack::TimeSystem;
using openblack::UnloadedIsland;
using openblack::chlapi::CHLApi;
using openblack::debug::gui::DebugGuiInterface;
using openblack::ecs::MapProduction;
using openblack::ecs::Registry;
using openblack::ecs::systems::AlignmentSystem;
using openblack::ecs::systems::AnimalAISystem;
using openblack::ecs::systems::CameraBookmarkSystem;
using openblack::ecs::systems::CameraPathSystem;
using openblack::ecs::systems::CreatureAnimationSystem;
using openblack::ecs::systems::CreatureAudioSystem;
using openblack::ecs::systems::CreatureCaveSystem;
using openblack::ecs::systems::CreatureFightSystem;
using openblack::ecs::systems::CreatureHairSystem;
using openblack::ecs::systems::CreatureHandSystem;
using openblack::ecs::systems::CreatureLocomotionSystem;
using openblack::ecs::systems::CreatureMindSystem;
using openblack::ecs::systems::CreatureModeSystem;
using openblack::ecs::systems::CreatureObjectActionSystem;
using openblack::ecs::systems::CreaturePhysiologySystem;
using openblack::ecs::systems::CreatureSkinSystem;
using openblack::ecs::systems::DayNightClockSystem;
using openblack::ecs::systems::EditorSystem;
using openblack::ecs::systems::FallingSpellSystem;
using openblack::ecs::systems::FireEffectSystem;
using openblack::ecs::systems::FireGraphicSystem;
using openblack::ecs::systems::FireSoundSystem;
using openblack::ecs::systems::FootprintSystem;
using openblack::ecs::systems::ForestSystem;
using openblack::ecs::systems::GameStatsSystem;
using openblack::ecs::systems::HandMagicState;
using openblack::ecs::systems::HandSystem;
using openblack::ecs::systems::InputState;
using openblack::ecs::systems::LandBalanceSystem;
using openblack::ecs::systems::LandPickSystem;
using openblack::ecs::systems::LeashSystem;
using openblack::ecs::systems::LivingActionSystem;
using openblack::ecs::systems::MagicObjectsSystem;
using openblack::ecs::systems::MapCellsSystem;
using openblack::ecs::systems::MapScriptSystem;
using openblack::ecs::systems::MapShapeProvider;
using openblack::ecs::systems::MeshBoxProvider;
using openblack::ecs::systems::ObjectCreationIndexSystem;
using openblack::ecs::systems::PathfindingSystem;
using openblack::ecs::systems::PlayerSystem;
using openblack::ecs::systems::ReactionsSystem;
using openblack::ecs::systems::RenderingSystem;
using openblack::ecs::systems::RoutePlanStateSystem;
using openblack::ecs::systems::ScreenFadeSystem;
using openblack::ecs::systems::ScreenshotRequestSystem;
using openblack::ecs::systems::SpellSystem;
using openblack::ecs::systems::ToBeDeletedSystem;
using openblack::ecs::systems::TownCellObjects;
using openblack::ecs::systems::TownStateSystem;
using openblack::ecs::systems::TownSystem;
using openblack::ecs::systems::TreeSystem;
using openblack::ecs::systems::VillagerBuildingSites;
using openblack::ecs::systems::VillagerChildFactory;
using openblack::ecs::systems::VillagerDiscipleJobs;
using openblack::ecs::systems::VillagerFields;
using openblack::ecs::systems::VillagerFishFarms;
using openblack::ecs::systems::VillagerRules;
using openblack::ecs::systems::VillagerStateSystem;
using openblack::ecs::systems::VillagerStores;
using openblack::ecs::systems::VillagerTentQueries;
using openblack::ecs::systems::VillagerWorldQueries;
using openblack::ecs::systems::VillagerWorshipCheck;
using openblack::ecs::systems::WorshipState;
using openblack::graphics::RendererInterface;
using openblack::input::GameActionMap;
using openblack::lhvm::LHVM;
using openblack::resources::Resources;
using openblack::windowing::DisplayMode;
using openblack::windowing::Sdl2WindowingSystem;

namespace
{
/// Today's date by the local clock, which only the creatures' footprints read (once per land)
openblack::ecs::systems::CalendarDate LocalDate()
{
	const auto now = std::time(nullptr);
	std::tm local {};
#if defined(_WIN32)
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	return {.month = local.tm_mon + 1, .day = local.tm_mday};
}
} // namespace

void openblack::InitializeWindow(const std::string& title, int width, int height, DisplayMode displayMode, uint32_t extraFlags)
{
	Locator::windowing::emplace<Sdl2WindowingSystem>(title, width, height, displayMode, extraFlags);
}

void openblack::InitializeClock()
{
	// A clock set up before the game (a test's fixed step) is kept, as the old global clock was
	if (!Locator::time::has_value())
	{
		Locator::time::emplace<TimeSystem>();
	}
}

void openblack::InitializeGameState()
{
	// Always new, as each Game started with its own (a test's is replaced too); in the order the Game made them
	Locator::screenFade::emplace<ScreenFadeSystem>();
	Locator::dayNightClock::emplace<DayNightClockSystem>();
	Locator::mapScriptSystem::emplace<MapScriptSystem>();
	// not in the tests' services: without a Game the test hooks request no screenshot
	Locator::screenshotRequest::emplace<ScreenshotRequestSystem>();
}

bool openblack::InitializeEngine(GraphicsBackend backend, bool vsync) noexcept
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "EnTT version: {}", ENTT_VERSION);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), GLM_VERSION_COMPLETE);

	Locator::profiler::emplace();
	// OPENBLACK_PROFILE=<seconds>: log each stage's average / worst time per frame every <seconds>.
	if (const char* profile = std::getenv("OPENBLACK_PROFILE"); profile != nullptr)
	{
		Locator::profiler::value().SetSummaryInterval(std::max(0.5f, static_cast<float>(std::atof(profile))));
	}

	Locator::rendererInterface::reset(RendererInterface::Create(backend, vsync).release());
	if (!Locator::rendererInterface::has_value())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Failed to create renderer");
		return false;
	}
	// Opening the renderer starts the engine timer (the frame delta, EngineMs)
	game_clock::StartEngineTimer();
	Locator::debugGui::reset(DebugGuiInterface::Create(graphics::RenderPass::ImGui).release());
	Locator::events::emplace<EventManager>();

#if __ANDROID__
	Locator::filesystem::emplace<AndroidFileSystem>();
#else
	Locator::filesystem::emplace<DefaultFileSystem>();
#endif
	Locator::rng::emplace<RandomNumberManagerProduction>();
	Locator::gameRandom::emplace<GameRandomProduction>();
	Locator::particleSystem::emplace<ecs::systems::ParticleSystem>();
	Locator::villageLightSystem::emplace<ecs::systems::VillageLightSystem>();
	Locator::renderFrameSystem::emplace<ecs::systems::RenderFrameSystem>();
	Locator::skyFrameSystem::emplace<ecs::systems::SkyFrameSystem>();
	Locator::landAvoidSystem::emplace<ecs::systems::LandAvoidSystem>();
	Locator::debugHooks::emplace<ecs::systems::DebugHooks>();
	Locator::scriptState::emplace<ecs::systems::ScriptState>();
	Locator::handTapRegistry::emplace<ecs::systems::HandTapRegistry>();
	Locator::videoSystem::emplace<ecs::systems::VideoSystem>();
	Locator::worldEffects::emplace<ecs::systems::WorldEffects>();
	// a test's fake audio service stays
	if (!Locator::audio::has_value())
	{
		Locator::audio::emplace<audio::AudioManager>();
	}
	Locator::audioState::emplace<ecs::systems::AudioState>();
	Locator::physicsObjectsSystem::emplace<ecs::systems::PhysicsObjectsSystem>();
	Locator::weatherSystem::emplace<ecs::systems::WeatherSystem>();
	// The audio system's wave device: without one nothing plays
	audio::device::Open();

	Locator::chlapi::emplace<CHLApi>();
	Locator::vm::emplace<LHVM>();
	return true;
}

bool openblack::InitializeGame() noexcept
{
	Locator::terrainSystem::emplace<UnloadedIsland>();
	Locator::resources::emplace<Resources>();
	// The players' alignment lasts the whole game, across lands: an existing player system (a test's, or one a
	// previous game left, as it is not reset at shutdown) is kept
	if (!Locator::playerSystem::has_value())
	{
		Locator::playerSystem::emplace<PlayerSystem>();
	}
	// The players', the camera's and the sky's alignment as one service, over the player system's
	Locator::alignmentSystem::emplace<AlignmentSystem>();
	// The input modules' state (the game packets and their handlers, the hand demo, the interface's flags), kept for
	// the whole game. Made before the hand system, which registers its packet handlers when it is made
	if (!Locator::inputState::has_value())
	{
		Locator::inputState::emplace<InputState>();
	}
	// The worship modules' state, kept for the whole game; each module clears its own part on a land load as before
	if (!Locator::worshipState::has_value())
	{
		Locator::worshipState::emplace<WorshipState>();
	}
	// Every player's GameStats and the shared statics, kept for the whole game (the game never clears them)
	if (!Locator::gameStatsSystem::has_value())
	{
		Locator::gameStatsSystem::emplace<GameStatsSystem>();
	}
	Locator::gameActionSystem::emplace<GameActionMap>();
	Locator::rendereringSystem::emplace<RenderingSystem>();
	Locator::entitiesRegistry::emplace<Registry>();
	Locator::handSystem::emplace<HandSystem>();
	Locator::temple::emplace<TempleInterior>();
	Locator::oceanSystem::emplace<Ocean>();
	Locator::skySystem::emplace<Sky>();
	// The in-game editor (F2), kept for the whole game: while it drives the camera it holds the player's camera model
	Locator::editorSystem::emplace<EditorSystem>();
	// What the villagers ask of the fields, fish farms, building sites, stores and tent spots. A test may have put
	// its fakes in before the game: they are kept
	if (!Locator::villagerFields::has_value())
	{
		Locator::villagerFields::emplace<VillagerFields>();
	}
	if (!Locator::villagerFishFarms::has_value())
	{
		Locator::villagerFishFarms::emplace<VillagerFishFarms>();
	}
	if (!Locator::villagerBuildingSites::has_value())
	{
		Locator::villagerBuildingSites::emplace<VillagerBuildingSites>();
	}
	if (!Locator::villagerStores::has_value())
	{
		Locator::villagerStores::emplace<VillagerStores>();
	}
	if (!Locator::villagerTentQueries::has_value())
	{
		Locator::villagerTentQueries::emplace<VillagerTentQueries>();
	}
	// The villagers' rules, world queries, worship check, child maker and disciple jobs. A test's fakes are kept the
	// same way
	if (!Locator::villagerRules::has_value())
	{
		Locator::villagerRules::emplace<VillagerRules>();
	}
	if (!Locator::villagerWorldQueries::has_value())
	{
		Locator::villagerWorldQueries::emplace<VillagerWorldQueries>();
	}
	if (!Locator::villagerWorshipCheck::has_value())
	{
		Locator::villagerWorshipCheck::emplace<VillagerWorshipCheck>();
	}
	if (!Locator::villagerChildFactory::has_value())
	{
		Locator::villagerChildFactory::emplace<VillagerChildFactory>();
	}
	if (!Locator::villagerDiscipleJobs::has_value())
	{
		Locator::villagerDiscipleJobs::emplace<VillagerDiscipleJobs>();
	}
	// What the map cells and the town read of the meshes and the cells' objects. A test's fakes are kept the same way
	if (!Locator::townCellObjects::has_value())
	{
		Locator::townCellObjects::emplace<TownCellObjects>();
	}
	if (!Locator::mapShapeProvider::has_value())
	{
		Locator::mapShapeProvider::emplace<MapShapeProvider>();
	}
	if (!Locator::meshBoxProvider::has_value())
	{
		Locator::meshBoxProvider::emplace<MeshBoxProvider>();
	}
	// The game's world lists, kept for the whole game: the dead list, the object creation counter and the map cells
	// (each land starts the counter again through object_index::OnLoadMap and empties the cells through
	// magic::OnLoadMap). A test's own are kept the same way
	if (!Locator::toBeDeletedSystem::has_value())
	{
		Locator::toBeDeletedSystem::emplace<ToBeDeletedSystem>();
	}
	if (!Locator::objectCreationIndexSystem::has_value())
	{
		Locator::objectCreationIndexSystem::emplace<ObjectCreationIndexSystem>();
	}
	if (!Locator::mapCellsSystem::has_value())
	{
		Locator::mapCellsSystem::emplace<MapCellsSystem>();
	}
	// The game's reactions, kept for the whole game (each land empties them through magic::OnLoadMap). The animals'
	// and the villagers' handlers are set as soon as they exist, before any map load
	if (!Locator::reactionsSystem::has_value())
	{
		Locator::reactionsSystem::emplace<ReactionsSystem>();
		openblack::ecs::animal_ai::RegisterReactionHandler();
		openblack::ecs::villager_reactions::RegisterHandlers();
	}
	// The game's fires, their graphics and their crackle, kept for the whole game (each land empties them through
	// magic::OnLoadMap: fire::Clear, then fire::graphic::Clear)
	if (!Locator::fireEffectSystem::has_value())
	{
		Locator::fireEffectSystem::emplace<FireEffectSystem>();
	}
	if (!Locator::fireGraphicSystem::has_value())
	{
		Locator::fireGraphicSystem::emplace<FireGraphicSystem>();
	}
	if (!Locator::fireSoundSystem::has_value())
	{
		Locator::fireSoundSystem::emplace<FireSoundSystem>();
	}
	// The game's forests (each land empties them: Game::LoadMap, ecs::ClearForests), what every tree shares (with the
	// magic trees' deletion listener, before any tree), the land balance (reset by Game::LoadMap) and what the animals
	// share; all kept for the whole game
	if (!Locator::forestSystem::has_value())
	{
		Locator::forestSystem::emplace<ForestSystem>();
	}
	if (!Locator::treeSystem::has_value())
	{
		Locator::treeSystem::emplace<TreeSystem>();
		openblack::magic::magic_tree::RegisterTreeListener();
	}
	if (!Locator::landBalanceSystem::has_value())
	{
		Locator::landBalanceSystem::emplace<LandBalanceSystem>();
	}
	if (!Locator::animalAISystem::has_value())
	{
		// with the flock miracles' species dying: the spell classes register it only once per spell system
		Locator::animalAISystem::emplace<AnimalAISystem>();
		openblack::magic::RegisterFlockSpeciesDying();
	}
	// The game's spells and their sinks, kept for the whole game (each land empties them through magic::OnLoadMap)
	if (!Locator::spellSystem::has_value())
	{
		Locator::spellSystem::emplace<SpellSystem>();
	}
	// The map shields and the fireballs, kept for the whole game (each land empties them through magic::OnLoadMap)
	if (!Locator::magicObjectsSystem::has_value())
	{
		Locator::magicObjectsSystem::emplace<MagicObjectsSystem>();
	}
	// The towns' and the villagers' shared lists, kept for the whole game (the vagrants are emptied by a script reboot,
	// the mourning takers by magic::OnLoadMap; the belief sprites are only ever popped)
	if (!Locator::townStateSystem::has_value())
	{
		Locator::townStateSystem::emplace<TownStateSystem>();
	}
	if (!Locator::villagerStateSystem::has_value())
	{
		Locator::villagerStateSystem::emplace<VillagerStateSystem>();
	}
	// The hand's magic modules' state (gestures, casting, mouse sampling, power-up bands, grain sprinkle), kept for the
	// whole game; each module clears its own part on a land load as before
	if (!Locator::handMagicState::has_value())
	{
		Locator::handMagicState::emplace<HandMagicState>();
	}
	// The route planner's callbacks and obstacle hook, and the footpaths' holder pool, kept for the whole game (the
	// callbacks are installed again at every land load; the pool never shrinks)
	if (!Locator::routePlanStateSystem::has_value())
	{
		Locator::routePlanStateSystem::emplace<RoutePlanStateSystem>();
	}
	// The falling spell (made on its first use) and its sparks flag, kept for the whole game
	if (!Locator::fallingSpellSystem::has_value())
	{
		Locator::fallingSpellSystem::emplace<FallingSpellSystem>();
	}
	// The temples' outsides, their meshes' files read on the first blend and kept for the whole game
	if (!Locator::templeExteriorSystem::has_value())
	{
		Locator::templeExteriorSystem::emplace<openblack::ecs::systems::TempleExteriorSystem>();
	}
	// The creatures' systems (called from ECS/CreatureLoop): each made new with the game, none reaches the registry
	// when made
	Locator::creatureAnimationSystem::emplace<CreatureAnimationSystem>();
	Locator::creatureMindSystem::emplace<CreatureMindSystem>();
	Locator::creaturePhysiologySystem::emplace<CreaturePhysiologySystem>();
	Locator::creatureHairSystem::emplace<CreatureHairSystem>();
	Locator::creatureAudioSystem::emplace<CreatureAudioSystem>();
	Locator::creatureObjectActionSystem::emplace<CreatureObjectActionSystem>();
	Locator::creatureHandSystem::emplace<CreatureHandSystem>();
	Locator::footprintSystem::emplace<FootprintSystem>(&LocalDate);
	Locator::creatureSkinSystem::emplace<CreatureSkinSystem>();
	Locator::leashSystem::emplace<LeashSystem>();
	// Creature Mode, kept for the whole game: while it follows a creature it holds the player's camera model. The
	// player's creature is the one the leash service knows
	Locator::creatureModeSystem::emplace<CreatureModeSystem>([](PlayerNames player) -> std::optional<entt::entity> {
		return Locator::leashSystem::has_value() ? Locator::leashSystem::value().PlayersCreature(player) : std::nullopt;
	});
	// The Creature Cave, which the temple's creature room shows: of the player's creature, through Creature Mode. The
	// mind's tables and the tattoos come from the mind and skin services
	Locator::creatureCaveSystem::emplace<CreatureCaveSystem>(CreatureCaveSystem::Services {
	    .mindTables = []() -> const creature_mind_tables::Tables* {
		    return Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
	    },
	    .setTattoo =
	        [](entt::entity creature, size_t slot, const creature_tattoo::Slot& tattoo) {
		        if (Locator::creatureSkinSystem::has_value())
		        {
			        Locator::creatureSkinSystem::value().SetTattoo(creature, slot, tattoo);
		        }
	        },
	});
	Locator::creatureFightSystem::emplace<CreatureFightSystem>();
	// the creature's physics class (thrown at, mass 1000), as the living things register theirs
	openblack::ecs::creature_physics::RegisterPhysicsHandlers();
	// The game's handlers of what the villagers' deaths and the towns' belief and desires report: help sprites,
	// tooltips, the smoke and the souls; and the players' deeds their creatures watch
	if (Locator::events::has_value())
	{
		auto& events = Locator::events::value();
		openblack::ecs::villager::AddDeathEventHandlers(events);
		openblack::ecs::town_belief::AddBeliefEventHandlers(events);
		openblack::ecs::town_desire::AddDesireEventHandlers(events);
		openblack::ecs::creature_mimic::AddMimicEventHandlers(events);
	}

	return true;
}

void openblack::InitializeLevel(const std::filesystem::path& path)
{
	Locator::entitiesMap::emplace<MapProduction>();
	Locator::landPickSystem::emplace<LandPickSystem>();
	Locator::livingActionSystem::emplace<LivingActionSystem>();
	Locator::townSystem::emplace<TownSystem>();
	Locator::pathfindingSystem::emplace<PathfindingSystem>();
	Locator::creatureLocomotionSystem::emplace<CreatureLocomotionSystem>();
	Locator::cameraBookmarkSystem::emplace<CameraBookmarkSystem>();
	Locator::terrainSystem::emplace<LandIsland>(path);
	Locator::cameraPathSystem::emplace<CameraPathSystem>();
	// Opening a landscape opens its sky too: a new sky of clouds for every land
	Clouds::OnLandscapeOpened();
}

void openblack::ShutDownServices()
{
	// the particle effects first, as a map load clears them: their destructors still find the services they use
	if (Locator::particleSystem::has_value())
	{
		psys::manager::Clear();
	}
	// Manually delete the assets here before BGFX renderer clears its buffers resulting in invalid handles in our assets
	if (Locator::resources::has_value())
	{
		auto& resources = Locator::resources::value();
		resources.GetMeshes().Clear();
		resources.GetTextures().Clear();
		resources.GetAnimations().Clear();
		resources.GetSounds().Clear();
		resources.GetFonts().Clear();
		resources.GetBlobs().Clear();
	}

	// The temple interior before the audio device, the renderer and the registry it uses while active; its scrolls and
	// signs have already gone with the interface (Game's shutdown), so what is left is plain state
	Locator::temple::reset();

	// The audio resources have been cleared and all sounds have been stopped (audio::Shutdown): the channels' sources,
	// the wave buffers and the OpenAL context go
	// the films before the device: the player's sound goes with it
	Locator::videoSystem::reset();
	audio::device::Close();

	Locator::rendereringSystem::reset();
	Locator::landPickSystem::reset();
	// the cave asks Creature Mode for the player's creature
	Locator::creatureCaveSystem::reset();
	// before the camera, whose model it may hold
	Locator::creatureModeSystem::reset();
	Locator::editorSystem::reset();
	Locator::cameraBookmarkSystem::reset();
	Locator::cameraPathSystem::reset();
	Locator::livingActionSystem::reset();
	Locator::townSystem::reset();
	Locator::handSystem::reset();
	Locator::pathfindingSystem::reset();
	Locator::creatureLocomotionSystem::reset();
	Locator::terrainSystem::reset();
	Locator::filesystem::reset();
	Locator::gameActionSystem::reset();

	Locator::oceanSystem::reset();
	Locator::skySystem ::reset();
	Locator::debugGui::reset();
	// after the debug GUI, whose menu reads it
	Locator::modLoader::reset();
	// The creatures' systems may go before the registry: none connects a registry signal, and no creature component's
	// destructor reaches a service
	Locator::creatureFightSystem::reset();
	Locator::leashSystem::reset();
	Locator::creatureMindSystem::reset();
	Locator::creaturePhysiologySystem::reset();
	Locator::creatureSkinSystem::reset();
	Locator::creatureHairSystem::reset();
	Locator::footprintSystem::reset();
	Locator::creatureAudioSystem::reset();
	Locator::creatureObjectActionSystem::reset();
	Locator::creatureHandSystem::reset();
	Locator::creatureAnimationSystem::reset();
	Locator::entitiesRegistry::reset();
	// after the registry: an entity's destruction may still close its spot visual
	Locator::particleSystem::reset();
	Locator::villageLightSystem::reset();
	Locator::renderFrameSystem::reset();
	Locator::skyFrameSystem::reset();
	Locator::landAvoidSystem::reset();
	Locator::debugHooks::reset();
	Locator::scriptState::reset();
	Locator::handTapRegistry::reset();
	Locator::worldEffects::reset();
	Locator::audio::reset();
	Locator::audioState::reset();
	Locator::physicsObjectsSystem::reset();
	Locator::weatherSystem::reset();
	// After the registry: anything an entity's destruction asks of these still finds them
	Locator::villagerFields::reset();
	Locator::villagerFishFarms::reset();
	Locator::villagerBuildingSites::reset();
	Locator::villagerStores::reset();
	Locator::villagerTentQueries::reset();
	Locator::villagerRules::reset();
	Locator::villagerWorldQueries::reset();
	Locator::villagerWorshipCheck::reset();
	Locator::villagerChildFactory::reset();
	Locator::villagerDiscipleJobs::reset();
	Locator::townCellObjects::reset();
	Locator::mapShapeProvider::reset();
	Locator::meshBoxProvider::reset();
	Locator::toBeDeletedSystem::reset();
	Locator::objectCreationIndexSystem::reset();
	Locator::mapCellsSystem::reset();
	Locator::reactionsSystem::reset();
	Locator::fireEffectSystem::reset();
	Locator::fireGraphicSystem::reset();
	Locator::fireSoundSystem::reset();
	Locator::forestSystem::reset();
	Locator::treeSystem::reset();
	Locator::landBalanceSystem::reset();
	Locator::animalAISystem::reset();
	// after the registry: a spell's sink may still be asked for while its effect goes
	Locator::spellSystem::reset();
	Locator::magicObjectsSystem::reset();
	Locator::townStateSystem::reset();
	Locator::villagerStateSystem::reset();
	Locator::handMagicState::reset();
	Locator::routePlanStateSystem::reset();
	Locator::fallingSpellSystem::reset();
	Locator::templeExteriorSystem::reset();
	Locator::worshipState::reset();
	Locator::gameStatsSystem::reset();
	Locator::alignmentSystem::reset();
	// after the hand system: its destructor clears its packet handlers
	Locator::inputState::reset();
	Locator::rendererInterface::reset();
	Locator::windowing::reset();
	Locator::events::reset();
	Locator::camera::reset();
	Locator::config::reset();
	Locator::infoConstants::reset();
	Locator::profiler::reset();
	Locator::gameRandom::reset();
	// Last: the audio device, closed above, was the other thread reading the clock
	Locator::time::reset();

	Locator::vm::reset();
	// Last, as the Game's own members went after all of the above (after the renderer), in the reverse order
	Locator::screenshotRequest::reset();
	Locator::mapScriptSystem::reset();
	Locator::dayNightClock::reset();
	Locator::screenFade::reset();
}
