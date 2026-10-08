/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TestServices.h"

#include <memory>

#include <gtest/gtest.h>

#include "Audio/AudioManager.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Systems/Implementations/AudioState.h"
#include "ECS/Systems/Implementations/DebugHooks.h"
#include "ECS/Systems/Implementations/FallingSpellSystem.h"
#include "ECS/Systems/Implementations/GameStatsSystem.h"
#include "ECS/Systems/Implementations/HandMagicState.h"
#include "ECS/Systems/Implementations/HandTapRegistry.h"
#include "ECS/Systems/Implementations/InputState.h"
#include "ECS/Systems/Implementations/LandAvoidSystem.h"
#include "ECS/Systems/Implementations/MagicObjectsSystem.h"
#include "ECS/Systems/Implementations/MapScriptSystem.h"
#include "ECS/Systems/Implementations/MapShapeProvider.h"
#include "ECS/Systems/Implementations/MeshBoxProvider.h"
#include "ECS/Systems/Implementations/ParticleSystem.h"
#include "ECS/Systems/Implementations/PhysicsObjectsSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/Implementations/RenderFrameSystem.h"
#include "ECS/Systems/Implementations/RoutePlanStateSystem.h"
#include "ECS/Systems/Implementations/ScriptState.h"
#include "ECS/Systems/Implementations/SkyFrameSystem.h"
#include "ECS/Systems/Implementations/SpellSystem.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "ECS/Systems/Implementations/TownCellObjects.h"
#include "ECS/Systems/Implementations/TownStateSystem.h"
#include "ECS/Systems/Implementations/VillagerBuildingSites.h"
#include "ECS/Systems/Implementations/VillagerFields.h"
#include "ECS/Systems/Implementations/VillagerFishFarms.h"
#include "ECS/Systems/Implementations/VillagerStateSystem.h"
#include "ECS/Systems/Implementations/VillagerStores.h"
#include "ECS/Systems/Implementations/VillagerTentQueries.h"
#include "ECS/Systems/Implementations/WeatherSystem.h"
#include "ECS/Systems/Implementations/WorldEffects.h"
#include "ECS/Systems/Implementations/WorshipState.h"
#include "FileSystem/DefaultFileSystem.h"
#include "Locator.h"

void openblack::test::EmplaceTestServices()
{
	using namespace openblack::ecs::systems;
	// The game's hidden state, as the program starts it. A service belongs here only when (1) it replaces global state
	// that always existed without anyone setting it up, (2) it is set up empty, exactly as production starts it, and (3)
	// it has no production behaviour a test should fake. Defaults of production behaviour do not belong here.
	// The random streams (both seeds 0, the C runtime seed 1, no hooks) and the clock (reset)
	Locator::gameRandom::emplace<GameRandomTesting>();
	Locator::time::emplace<TimeSystem>();
	// the particle system's state, empty
	Locator::particleSystem::emplace<ParticleSystem>();
	// the RenderFrame state, empty
	Locator::renderFrameSystem::emplace<RenderFrameSystem>();
	// the SkyFrame state, empty
	Locator::skyFrameSystem::emplace<SkyFrameSystem>();
	// the LandAvoid state, empty
	Locator::landAvoidSystem::emplace<LandAvoidSystem>();
	// the debug hooks' state, empty
	Locator::debugHooks::emplace<DebugHooks>();
	// the script side's state, empty
	Locator::scriptState::emplace<ScriptState>();
	// the hand-tap handlers, none yet
	Locator::handTapRegistry::emplace<HandTapRegistry>();
	// the world effects modules' state, empty
	Locator::worldEffects::emplace<WorldEffects>();
	// the audio modules' state, empty
	Locator::audio::emplace<audio::AudioManager>();
	Locator::audioState::emplace<AudioState>();
	// the weather's state, empty
	Locator::weatherSystem::emplace<WeatherSystem>();
	// the physics objects' state, empty (the base game's constants)
	Locator::physicsObjectsSystem::emplace<PhysicsObjectsSystem>();
	// the players' alignment and the magic of the players without an entity, at their new game values
	Locator::playerSystem::emplace<PlayerSystem>();
	// the spells and their sinks, empty
	Locator::spellSystem::emplace<SpellSystem>();
	// the map shields and the fireballs, empty
	Locator::magicObjectsSystem::emplace<MagicObjectsSystem>();
	// the towns' and the villagers' shared lists, empty
	Locator::townStateSystem::emplace<TownStateSystem>();
	Locator::villagerStateSystem::emplace<VillagerStateSystem>();
	// the hand's magic modules' state, empty
	Locator::handMagicState::emplace<HandMagicState>();
	// the route planner's callbacks, hook and holder pool, empty
	Locator::routePlanStateSystem::emplace<RoutePlanStateSystem>();
	// no falling spell yet
	Locator::fallingSpellSystem::emplace<FallingSpellSystem>();
	// the worship modules' state, empty
	Locator::worshipState::emplace<WorshipState>();
	// the input modules' state, empty (no packet handlers)
	Locator::inputState::emplace<InputState>();
	// every player's GameStats and the shared statics, at their start values
	Locator::gameStatsSystem::emplace<GameStatsSystem>();
	// the map script's globals at their start values, where the tests without a Game had a static copy (a Game made by
	// a test puts in its own). They hold the land balance's values too, so it is emplaced before any land balance
	// (WorldSystems.h) and reset after it
	Locator::mapScriptSystem::emplace<MapScriptSystem>();
}

void openblack::test::ResetTestServices()
{
	Locator::mapScriptSystem::reset();
	Locator::gameStatsSystem::reset();
	Locator::inputState::reset();
	Locator::worshipState::reset();
	Locator::fallingSpellSystem::reset();
	Locator::routePlanStateSystem::reset();
	Locator::handMagicState::reset();
	Locator::villagerStateSystem::reset();
	Locator::townStateSystem::reset();
	Locator::magicObjectsSystem::reset();
	Locator::spellSystem::reset();
	Locator::playerSystem::reset();
	Locator::physicsObjectsSystem::reset();
	Locator::weatherSystem::reset();
	Locator::audio::reset();
	Locator::audioState::reset();
	Locator::worldEffects::reset();
	Locator::handTapRegistry::reset();
	Locator::scriptState::reset();
	Locator::debugHooks::reset();
	Locator::landAvoidSystem::reset();
	Locator::skyFrameSystem::reset();
	Locator::renderFrameSystem::reset();
	Locator::particleSystem::reset();
	Locator::time::reset();
	Locator::gameRandom::reset();
}

void openblack::test::EmplaceMapAndVillagerDefaults()
{
	using namespace openblack::ecs::systems;
	Locator::villagerFields::emplace<VillagerFields>();
	Locator::villagerFishFarms::emplace<VillagerFishFarms>();
	Locator::villagerBuildingSites::emplace<VillagerBuildingSites>();
	Locator::villagerStores::emplace<VillagerStores>();
	Locator::villagerTentQueries::emplace<VillagerTentQueries>();
	Locator::townCellObjects::emplace<TownCellObjects>();
	Locator::mapShapeProvider::emplace<MapShapeProvider>();
	Locator::meshBoxProvider::emplace<MeshBoxProvider>();
}

void openblack::test::ResetMapAndVillagerDefaults()
{
	Locator::meshBoxProvider::reset();
	Locator::mapShapeProvider::reset();
	Locator::townCellObjects::reset();
	Locator::villagerTentQueries::reset();
	Locator::villagerStores::reset();
	Locator::villagerBuildingSites::reset();
	Locator::villagerFishFarms::reset();
	Locator::villagerFields::reset();
}

openblack::test::ScopedDefaultFileSystem::ScopedDefaultFileSystem()
    : _previous(Locator::filesystem::handle())
{
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
}

openblack::test::ScopedDefaultFileSystem::~ScopedDefaultFileSystem()
{
	Locator::filesystem::reset(_previous);
}

namespace
{
/// Every test starts with the same services, whether it runs alone or with the rest of its executable. Only the
/// hidden-state services above are set here; a test injects any other dependency itself
class TestServices final: public ::testing::EmptyTestEventListener
{
	void OnTestStart(const ::testing::TestInfo& /*info*/) override { openblack::test::EmplaceTestServices(); }
	void OnTestEnd(const ::testing::TestInfo& /*info*/) override { openblack::test::ResetTestServices(); }
};

/// Registered before main() runs the tests (gtest_main's main calls InitGoogleTest, which keeps the listeners). gtest
/// takes ownership of the listener
const bool k_Registered = [] {
	::testing::UnitTest::GetInstance()->listeners().Append(std::make_unique<TestServices>().release());
	return true;
}();
} // namespace
