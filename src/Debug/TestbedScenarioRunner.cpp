/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarioRunner.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/FlatLand.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/SkyDome.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Commands/PlayerCommands.h"
#include "Commands/TurnCodec.h"
#include "Common/FileDialog.h"
#include "Common/PortableMath.h"
#include "Creature/CaveCreature.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureScriptPlay.h"
#include "Debug/ComputerActions.h"
#include "Debug/FightAutopilotInterface.h"
#include "Debug/TownMiracles.h"
#include "DebugGuiInterface.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/DrinkWaypointArchetype.h"
#include "ECS/Archetypes/FieldArchetype.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/GameStanding.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/Weather.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ComputerPlayerStart.h"
#include "ECS/Dances.h"
#include "ECS/Map.h"
#include "ECS/ObserverInterfaceRules.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerCreatureReactions.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/InspectorLoading.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "FileSystem/FileSystemInterface.h"
#include "Help/Spirits.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/TownBelief.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptInterfaceLevels.h"
#include "Sound/GameMusic.h"
#include "TestbedDispenserGrid.h"
#include "TestbedScenarioRunnerParts.h"
#include "Visual/RendererRequestsInterface.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::runner_parts;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureAnimation;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureNeeds;
using openblack::ecs::components::Transform;

namespace
{

/// The lines of the log of commands kept
constexpr size_t k_LogLines = 8;
/// The least the overview takes in either way of the middle of what it frames
constexpr float k_MinOverviewHalfSize = 25.0f;

/// Where a creature faces on the land; its mesh looks back along +z
glm::vec2 AheadOf(const Transform& transform)
{
	const auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	return {ahead.x, ahead.z};
}

/// Whether a command sends its creature somewhere, or has it face, throw or point somewhere
bool HasPoint(Kind kind)
{
	return kind == Kind::WalkTo || kind == Kind::RunTo || kind == Kind::FleeFrom || kind == Kind::TurnToFace ||
	       kind == Kind::ThrowAt || kind == Kind::PointAt || kind == Kind::TurnToFaceWhenStill;
}

/// The camera's fields of view across and up and down, in radians
glm::vec2 FieldsOfView(const Camera& camera)
{
	return {camera.GetHorizontalFieldOfView(), camera.GetVerticalFieldOfView()};
}

/// The made-up round trip of the ping line in microseconds: 40 ms, then 8 ms more each second to 64 ms, then 40 ms
/// again
[[nodiscard]] uint64_t DemoRoundTrip(float seconds)
{
	constexpr uint64_t k_First = 40'000;
	constexpr uint64_t k_Step = 8'000;
	constexpr uint64_t k_Steps = 4;
	return k_First + (k_Step * (static_cast<uint64_t>(seconds) % k_Steps));
}

} // namespace

namespace openblack::testbed_scenarios::runner_parts
{
/// The debug windows leave the mouse alone while a scenario drives it, so that the real pointer resting on one of them
/// doesn't take the scenario's presses
void KeepDebugWindowsOffTheMouse(bool off)
{
	if (Locator::debugGui::has_value())
	{
		Locator::debugGui::value().KeepOffTheMouse(off);
	}
}

std::string_view Started(bool started)
{
	return started ? "started" : "can't";
}
} // namespace openblack::testbed_scenarios::runner_parts

void Runner::Start(const Scenario& scenario)
{
	// Loading until its things are laid out on the fresh testbed, so that the inspector's tools never see the land
	// without them
	const ecs::systems::InspectorLoading loading("scenario " + std::string(scenario.id));
	Stop();
	// The camera is the player's again before the scenario frames it, and the cave closes
	if (Locator::creatureModeSystem::has_value())
	{
		Locator::creatureModeSystem::value().Leave();
	}
	if (Locator::creatureCaveSystem::has_value() && Locator::creatureCaveSystem::value().IsOpen())
	{
		Locator::creatureCaveSystem::value().Close();
	}
	_scenario = &scenario;
	_running = true;
	_seconds = 0.0f;
	_lastNativeResult.reset();
	_fireBallTakeFrames.reset();
	_nextMiracleLog = 0.0f;
	_timeline = {};
	_creatures.clear();
	_slotCreatures.clear();
	_objects.clear();
	_birdFlocks.clear();
	_scriptFlock = entt::null;
	_keptByScript.clear();
	_madeByNatives.clear();
	_globes.clear();
	_dispensers.clear();
	_temples.clear();
	_throwsDue.clear();
	_turnsAsked.clear();
	_deviceRestoreAsked.reset();
	for (const auto& setup : scenario.throws)
	{
		_throwsDue.emplace_back(setup.delaySeconds);
	}
	_walks.clear();
	_particles.clear();
	_miracles.clear();
	_started.clear();
	_log.clear();
	_checks.Start(scenario.checks);
	_checksReported = false;
	_shot.reset();
	_crowdCreatures.clear();
	_village = {};
	_crowdNext = 0;
	_crowdAbodes.clear();
	_crowdEntities.clear();
	_crowdProgress = {};
	_settledFrames = 0;
	_recorder.reset();
	_liveResults = {};
	_framesSinceSummary = 0;
	_saved = false;
	if (scenario.crowd.has_value())
	{
		const auto& crowd = *scenario.crowd;
		if (crowd.kind == Crowd::Kind::Creatures)
		{
			// The towns among the creatures first, so that the creatures keep clear of their homes
			if (crowd.villagers > 0)
			{
				_village = LayOutVillagers(crowd.villagers, crowd.seed);
			}
			_crowdCreatures = LayOutCreatures(crowd.count, crowd.seed, TownCentres(_village), k_TownClearRadius);
		}
		else
		{
			_village = LayOutVillagers(crowd.count, crowd.seed);
		}
		_crowdProgress.total =
		    _village.towns.size() + _village.abodes.size() + _village.villagers.size() + _crowdCreatures.size();
		_recorder = std::make_unique<benchmark::Recorder>(BenchmarkStages(), _benchmark.frames);
	}

	// A fresh testbed: the last one's creatures, objects, footprints, weather and scripts all go with it
	if (Locator::landSystem::has_value())
	{
		auto& land = Locator::landSystem::value();
		if (scenario.environment.land == Land::GodsRingMap)
		{
			if (!land.LoadGodsRingMap())
			{
				_running = false;
				return;
			}
		}
		else if (scenario.environment.land == Land::Story)
		{
			// The land plays as the story plays it, with its scripts from a fresh start
			const auto path =
			    Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / std::string(scenario.environment.storyLand);
			if (!land.LoadMapWithFreshScripts(path, land_change::Kind::NewGame))
			{
				Log(fmt::format("The story land {} did not load", scenario.environment.storyLand));
				_running = false;
				return;
			}
		}
		else
		{
			land.LoadTestbed();
		}
	}
	if (!Locator::terrainSystem::has_value())
	{
		_running = false;
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	_middle = (land.GetExtent().minimum + land.GetExtent().maximum) * 0.5f;

	SetUpEnvironment(scenario.environment);
	// The testbed's lake has a water point at the middle of its south edge, as a land's script marks its places to
	// drink. The creature drinks there.
	if (scenario.environment.land == Land::Testbed)
	{
		const glm::vec2 drinkAt {flat_land::k_LakeCentre.x,
		                         flat_land::k_LakeCentre.y - flat_land::k_LakeHalfExtent.y + (flat_land::k_CellSize * 0.5f)};
		ecs::archetypes::DrinkWaypointArchetype::Create({drinkAt.x, land.GetHeightAt(drinkAt), drinkAt.y});
	}
	if (const auto& temple = scenario.environment.temple; temple.has_value())
	{
		const auto at = MapPoint(_middle, *temple);
		ecs::archetypes::CitadelArchetype::Create({at.x, land.GetHeightAt(at), at.y}, PlayerNames::PLAYER_ONE, 0.0f,
		                                          glm::vec3(1.0f));
	}
	if (!scenario.objectsAfterLandScript)
	{
		PlaceObjects(scenario, _middle);
	}
	for (const auto& offset : scenario.sharks)
	{
		const auto at = MapPoint(_middle, offset);
		ecs::archetypes::SharkArchetype::Create({at.x, 0.0f, at.y}, 1.0f);
	}
	PlaceHighlights(scenario, _middle);
	// The testbed's grid of dispensers goes before the land's script, so that it is not in the way of a building that
	// the script makes
	if (!KeepsDispenserGrid(scenario))
	{
		testbed_dispensers::RemoveGrid();
	}
	// As on a land, the things are on the map before the footpaths come, so no footpath goes round them
	if (Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().Sync();
	}
	if (Locator::footpathSystem::has_value())
	{
		Locator::footpathSystem::value().ProcessObstacleChanges();
	}
	PlaceFootpaths(scenario, _middle);
	if (!scenario.landScript.empty())
	{
		lhscriptx::Script landScript({_middle.x, 0.0f, _middle.y}, map_coords::FromMetres(_middle));
		landScript.Load(std::string(scenario.landScript));
	}
	// The land's start makes the sheet of the players' symbols after the land's script, which switches on the computer
	// players
	if (scenario.environment.symbolsAfterLandScript)
	{
		if (_stage != nullptr)
		{
			_stage->MakePlayersSymbols(scenario.environment.profileSymbol);
		}
	}
	if (scenario.objectsAfterLandScript)
	{
		PlaceObjects(scenario, _middle);
	}
	PlaceBirds(scenario, _middle);
	PlaceCreatures(scenario, _middle);
	PlaceDispensers(scenario);
	// A network game fills its empty slots after the land script. The temples of the scenario must stand first.
	if (scenario.fillEmptySlots && Locator::computerPlayerSystem::has_value())
	{
		constexpr std::array k_Humans {PlayerNames::PLAYER_ONE};
		Locator::computerPlayerSystem::value().FillEmptySlots(computer_player::k_SlotGods, k_Humans);
		// The commands can name the creatures of the filled slots after the creatures of the scenario
		std::vector<entt::entity> filled;
		Locator::entitiesRegistry::value().Each<const Creature>([this, &filled](entt::entity entity, const Creature&) {
			if (std::ranges::find(_creatures, entity) == _creatures.end())
			{
				filled.push_back(entity);
			}
		});
		std::ranges::sort(filled);
		_slotCreatures = std::move(filled);
	}
	if (Locator::playerSystem::has_value())
	{
		for (const auto& ally : scenario.allies)
		{
			Locator::playerSystem::value().SetAlly(ally.player, ally.other, ally.strength);
		}
	}
	// With the scenario's things laid out on the land, each town of a player with a temple is given its worship site, as
	// a land's are once it is laid out
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().LandLaidOut();
	}
	if (!scenario.townMiracles.empty())
	{
		const auto& towns = Locator::entitiesRegistry::value().Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end())
		{
			for (const auto type : scenario.townMiracles)
			{
				debug::town_miracles::Apply({.town = town->second, .type = type});
			}
		}
	}
	if (scenario.worshipShare.has_value())
	{
		SetUpWorship(*scenario.worshipShare);
	}
	_startTurn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	_computerActionsDone.assign(scenario.computerActions.size(), false);
	UpdateComputerActions();
	if (!scenario.debugWindow.empty() && Locator::debugGui::has_value())
	{
		Locator::debugGui::value().OpenWindow(scenario.debugWindow);
	}
	SetUpStatistics(scenario);
	if (Locator::fireflySystem::has_value())
	{
		for (const auto& [name, weight] : scenario.fireflyRewards)
		{
			Locator::fireflySystem::value().SetRewardWeight(name, weight);
		}
	}
	if (scenario.townBeliefInPlayer.has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			if (auto* impression = registry.TryGet<ecs::components::TownImpression>(town->second))
			{
				magic::town_belief::SetInPlayer(impression->belief, PlayerNames::PLAYER_ONE, *scenario.townBeliefInPlayer);
			}
		}
	}
	if (scenario.townBeliefInRival.has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			if (auto* impression = registry.TryGet<ecs::components::TownImpression>(town->second))
			{
				magic::town_belief::SetInPlayer(impression->belief, PlayerNames::PLAYER_TWO, *scenario.townBeliefInRival);
			}
		}
	}
	if (!scenario.townBeliefInPlayers.empty())
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			if (auto* impression = registry.TryGet<ecs::components::TownImpression>(town->second))
			{
				for (const auto& [player, belief] : scenario.townBeliefInPlayers)
				{
					magic::town_belief::SetInPlayer(impression->belief, player, belief);
				}
			}
		}
	}
	if (scenario.townOwner.has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			auto& owned = registry.Get<ecs::components::Town>(town->second);
			if (owned.owner != *scenario.townOwner)
			{
				// The town goes last in the list of its new player
				owned.owner = *scenario.townOwner;
				owned.gained = registry.Context().nextTownGained++;
			}
		}
	}
	if (scenario.townFeelingAboutCreatures.has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
			for (const auto creature : _creatures)
			{
				ecs::villager_creature::SetFeeling(town->second, creature, *scenario.townFeelingAboutCreatures, turn);
			}
		}
	}
	if (scenario.tribalPower.has_value() && Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().SetTribalPower(PlayerNames::PLAYER_ONE, scenario.tribalPower->first,
		                                             scenario.tribalPower->second);
	}
	// The hand held still over the land, looked down on from above it
	if (scenario.hand.has_value() && Locator::magicSystem::has_value())
	{
		const auto at = MapPoint(_middle, scenario.hand->offset);
		const glm::vec3 point {at.x, land.GetHeightAt(at), at.y};
		const glm::vec3 hand = point + glm::vec3(0.0f, scenario.hand->height, 0.0f);
		_heldHand = ecs::systems::MagicSystemInterface::HandFrame {
		    .handPosition = hand,
		    .point = point,
		    .rayOrigin = hand + glm::vec3(0.0f, k_HandEyeHeight, 0.0f),
		    .rayDirection = glm::vec3(0.0f, -1.0f, 0.0f),
		    .cameraForward = glm::vec3(0.0f, -1.0f, 0.0f),
		    .overWorld = true,
		};
		_heldHandHeight = scenario.hand->height;
		Locator::magicSystem::value().DriveHand(*_heldHand);
	}
	for (const auto& miracle : scenario.miracles)
	{
		_miracles.push_back({.nextAt = miracle.delaySeconds, .letGoAt = std::nullopt, .spell = entt::null});
	}
	// The storms a land's setup script places, in the island's global climate
	if (Locator::weatherSystem::has_value())
	{
		for (const auto& storm : scenario.landStorms)
		{
			const auto at = MapPoint(_middle, storm.offset);
			const auto to = MapPoint(_middle, storm.destination);
			lhscriptx::FeatureScriptCommands::CreateWeatherStorm(
			    0, {at.x, land.GetHeightAt(at), at.y}, storm.age, storm.cloudCount, std::string(storm.shape),
			    std::string(storm.clouds), std::string(storm.weather), storm.speed, {to.x, land.GetHeightAt(to), to.y});
		}
	}
	for (size_t i = 0; i < scenario.particles.size(); ++i)
	{
		_particles.push_back({StartParticle(i), 0.0f});
	}
	_vortices.assign(scenario.vortices.size(), {});
	_rewards.assign(scenario.rewards.size(), entt::null);
	Frame(scenario.framing.shot, scenario.framing.creature, scenario.framing.distance);
	// In a network game the commands step by the turns from this turn
	_layoutTurn = Locator::time::has_value() ? std::optional(Locator::time::value().GetTurn()) : std::nullopt;
	if (_layoutTurn.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The testbed scenario {} is laid out at turn {}", scenario.id, *_layoutTurn);
	}
	Log(fmt::format("Started {}", scenario.name));
}

void Runner::Stop()
{
	if (!_running)
	{
		return;
	}
	_running = false;
	// The ping line belongs to the network game again
	if (_demoPingSince.has_value() && _stage != nullptr)
	{
		_stage->SetDemoRoundTrip(std::nullopt);
	}
	_demoPingSince.reset();
	// The debug option's bonus creatures are as they were
	if (_unlockedBefore.has_value() && Locator::config::has_value())
	{
		Locator::config::value().unlockedBonusCreatures = *_unlockedBefore;
	}
	_unlockedBefore.reset();
	// The land's number is as it was
	if (_landNumberBefore.has_value() && Locator::entitiesRegistry::has_value())
	{
		Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber = *_landNumberBefore;
	}
	_landNumberBefore.reset();
	// The background weather is as it was
	if (_backgroundWeatherBefore.has_value() && Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().SetBackgroundWeather(*_backgroundWeatherBefore);
	}
	_backgroundWeatherBefore.reset();
	// The camera zones that the scenario set go
	if (_cameraZoneSet && Locator::cameraZoneSystem::has_value())
	{
		Locator::cameraZoneSystem::value().ClearZones();
	}
	_cameraZoneSet = false;
	if (_skirmishBefore.has_value() && Locator::entitiesRegistry::has_value())
	{
		// The end of the skirmish goes too, so that the world turns again
		auto& registry = Locator::entitiesRegistry::value();
		registry.Context().skirmish = *_skirmishBefore;
		registry.Context().gameEnd = {};
		std::vector<entt::entity> standings;
		registry.Each<const ecs::components::GameStanding>(
		    [&standings](entt::entity entity, const ecs::components::GameStanding&) { standings.push_back(entity); });
		for (const auto entity : standings)
		{
			registry.Remove<ecs::components::GameStanding>(entity);
		}
	}
	_skirmishBefore.reset();
	if (_networkGameBefore.has_value() && Locator::time::has_value())
	{
		Locator::time::value().SetNetworkGame(*_networkGameBefore);
	}
	_networkGameBefore.reset();
	// The players that the scenario added go
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto player : _addedPlayers)
		{
			if (registry.Valid(player))
			{
				registry.Destroy(player);
			}
		}
		if (!_addedPlayers.empty() && Locator::playerSystem::has_value())
		{
			Locator::playerSystem::value().RegisterPlayers();
		}
	}
	_addedPlayers.clear();
	// The mouse is the player's again
	_sweep.reset();
	_heldHand.reset();
	_sweepMovesHeldHand = false;
	if (Locator::gameActionSystem::has_value())
	{
		if (_ownsPointer && Locator::gameActionSystem::value().GetScriptedPointer().has_value())
		{
			Locator::gameActionSystem::value().SetScriptedPointer(std::nullopt);
			KeepDebugWindowsOffTheMouse(false);
		}
		_ownsPointer = false;
	}
	// Its particle effects die away
	if (Locator::particleRuleSystem::has_value())
	{
		for (const auto& particle : _particles)
		{
			Locator::particleRuleSystem::value().CloseDown(particle.effect);
		}
	}
	_particles.clear();
	// Its vortices go, though the ground they levelled stays levelled as the game leaves it
	for (const auto& running : _vortices)
	{
		if (running.vortex != entt::null && Locator::entitiesRegistry::value().Valid(running.vortex))
		{
			Locator::entitiesRegistry::value().Destroy(running.vortex);
		}
	}
	_vortices.clear();
	// Its reward chests go, opened or not
	for (const auto chest : _rewards)
	{
		if (chest != entt::null && Locator::entitiesRegistry::value().Valid(chest))
		{
			Locator::entitiesRegistry::value().Destroy(chest);
		}
	}
	_rewards.clear();
	// Its held miracles stop, and the hand is the mouse's again
	if (Locator::magicSystem::has_value())
	{
		auto& magic = Locator::magicSystem::value();
		if (std::ranges::any_of(_miracles, [](const RunningMiracle& m) { return m.pressAt || m.releaseAt; }))
		{
			magic.ReleaseAction();
			magic.DiscardHeldSeed();
		}
		magic.DriveHand(std::nullopt);
		if (_scenario != nullptr && _scenario->tribalPower.has_value())
		{
			magic.SetTribalPower(PlayerNames::PLAYER_ONE, _scenario->tribalPower->first, 1.0f);
		}
		for (const auto& miracle : _miracles)
		{
			if (miracle.letGoAt.has_value())
			{
				Locator::magicSystem::value().CloseDown(miracle.spell);
			}
		}
	}
	_miracles.clear();
	// The hand lets go of a creature a scenario held it to
	if (Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().IsHeldByCommand())
	{
		Locator::creatureHandSystem::value().Release();
	}
	if (Locator::creaturePhysiologySystem::has_value())
	{
		auto& physiology = Locator::creaturePhysiologySystem::value();
		physiology.SetTimeScale(1.0f);
		physiology.SetFaintingEnabled(true);
	}
	if (Locator::footprintSystem::has_value())
	{
		Locator::footprintSystem::value().SetAprilFoolsOverride(std::nullopt);
	}
	SetUpDate(std::nullopt);
	if (Locator::skyClockSystem::has_value())
	{
		Locator::skyClockSystem::value().GetClock().SetRunning(true);
	}
	Log("Stopped");
}

std::optional<entt::entity> Runner::CreatureAt(size_t index) const
{
	// After the creatures of the scenario come the creatures of the filled slots
	const auto total = _creatures.size() + _slotCreatures.size();
	if (index >= total || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto entity = index < _creatures.size() ? _creatures[index] : _slotCreatures[index - _creatures.size()];
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Creature>(entity))
	{
		return std::nullopt;
	}
	return entity;
}

void Runner::ApplyStates()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (size_t i = 0; i < _creatures.size(); ++i)
	{
		const auto entity = CreatureAt(i);
		if (!entity.has_value())
		{
			continue;
		}
		const auto& setup = _scenario->creatures[i];
		auto* needs = registry.TryGet<CreatureNeeds>(*entity);
		auto* mind = registry.TryGet<CreatureMindState>(*entity);
		const auto* creature = registry.TryGet<const Creature>(*entity);
		// The body and the mind start from the species' tables on their first turn, over anything set before
		if (needs == nullptr || mind == nullptr || creature == nullptr || !needs->started || !mind->desires.has_value())
		{
			continue;
		}
		if (setup.pauseMind && setup.thinksAfterSeconds.has_value() && _seconds >= *setup.thinksAfterSeconds)
		{
			mind->paused = false;
		}
		if (_started[i] && !setup.hold)
		{
			continue;
		}
		auto overrides = setup.needs;
		if (_started[i])
		{
			// Its age is only set as it starts, so it still grows older
			overrides.age.reset();
		}
		Apply(overrides, needs->needs, creature->size);
		Apply(setup.desires, *mind->desires);
		if (!_started[i] && !setup.desires.empty())
		{
			// Its first thought was had with the species' desires; it thinks again with the scenario's
			mind->idle = {};
		}
		_started[i] = true;
	}
}

std::optional<entt::entity> Runner::GlobeAt(size_t index) const
{
	if (index >= _globes.size() || !Locator::entitiesRegistry::has_value() ||
	    !Locator::entitiesRegistry::value().Valid(_globes[index]))
	{
		return std::nullopt;
	}
	return _globes[index];
}

std::optional<entt::entity> Runner::ObjectAt(size_t index) const
{
	if (index >= _objects.size() || !Locator::entitiesRegistry::has_value() ||
	    !Locator::entitiesRegistry::value().Valid(_objects[index]))
	{
		return std::nullopt;
	}
	return _objects[index];
}

std::optional<entt::entity> Runner::TotemAt(size_t index) const
{
	const auto centre = ObjectAt(index);
	if (!centre.has_value())
	{
		return std::nullopt;
	}
	std::optional<entt::entity> totem;
	Locator::entitiesRegistry::value().Each<const ecs::components::VillageTotem>(
	    [&totem, &centre](entt::entity entity, const ecs::components::VillageTotem& component) {
		    if (component.townCentre == *centre)
		    {
			    totem = entity;
		    }
	    });
	return totem;
}

bool Runner::IsFree(size_t creature) const
{
	const auto entity = CreatureAt(creature);
	if (!entity.has_value())
	{
		// A creature that is gone holds nothing up
		return true;
	}
	if (Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(*entity))
	{
		return false;
	}
	const auto* animation = Locator::entitiesRegistry::value().TryGet<const CreatureAnimation>(*entity);
	return animation == nullptr || !creature_layers::IsPlaying(animation->body);
}

void Runner::Give(const Command& command)
{
	if (command.kind == Kind::HoldSeed || command.kind == Kind::DrawGesture || command.kind == Kind::SummonSeed ||
	    command.kind == Kind::PressKey || command.kind == Kind::HandTakeFireBall || command.kind == Kind::HandTapObject ||
	    command.kind == Kind::HandTake || command.kind == Kind::HandApply || command.kind == Kind::HandTakeShownSeed ||
	    command.kind == Kind::HandPour || command.kind == Kind::HandRelease || command.kind == Kind::SetReportLatency ||
	    command.kind == Kind::RemoveObject || command.kind == Kind::GiveMagicTreesToStore ||
	    command.kind == Kind::HandCastAtPoint || command.kind == Kind::HandGripTotem ||
	    command.kind == Kind::AttachObjectScriptSound || command.kind == Kind::SayLine ||
	    command.kind == Kind::PlayGuidanceVoice || command.kind == Kind::ScriptRelease || command.kind == Kind::FillSiteIcons ||
	    command.kind == Kind::BuildWorshipSites || command.kind == Kind::ScriptBuildsWorshipSites ||
	    command.kind == Kind::PressPlainKey || command.kind == Kind::PlainKeyDown || command.kind == Kind::PlainKeyRepeat ||
	    command.kind == Kind::PlainKeyUp || command.kind == Kind::ChargeIconWithSeedHeld ||
	    command.kind == Kind::TakeIconSeed || command.kind == Kind::TapSiteIcon || command.kind == Kind::TapTempleEntrance ||
	    command.kind == Kind::FinishBuilding || command.kind == Kind::ScaffoldStartsSite ||
	    command.kind == Kind::DestroyScaffoldSite || command.kind == Kind::ScaffoldClearsGround ||
	    command.kind == Kind::FillBuiltAmount || command.kind == Kind::PauseGame || command.kind == Kind::SetGameSpeed ||
	    command.kind == Kind::RemoveTeleportStone || command.kind == Kind::MakeDance || command.kind == Kind::OtherGodHand ||
	    command.kind == Kind::ObserverCamera || command.kind == Kind::OwnGod || command.kind == Kind::BringCreature ||
	    command.kind == Kind::ComputerPersonality || command.kind == Kind::ComputerUntaughtSends ||
	    command.kind == Kind::HandCastOnDispenser || command.kind == Kind::HandCastOnCreature ||
	    command.kind == Kind::CloseHeldMiracle || command.kind == Kind::JoinFootball)
	{
		const auto result = GivePlayerCommand(command);
		Log(fmt::format("{:.1f}s: {}{}{}", _seconds, Name(command.kind), result.empty() ? "" : ": ", result));
		return;
	}
	if (IsPointerCommand(command.kind))
	{
		const auto result = GivePointerCommand(command);
		const auto line = fmt::format("{:.2f}s: {}: {}", _seconds, Name(command.kind), result);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
		Log(line);
		return;
	}
	if (command.kind == Kind::CameraLooksAt)
	{
		const auto look = MapPoint(_middle, command.point);
		// The eye is 15 m south of the point and 15 m above it, so that a script that waits for the camera within 30 m
		// of the point goes on
		const auto& land = Locator::terrainSystem::value();
		const auto height = land.GetHeightAt(look);
		Locator::camera::value().SetOrigin({look.x, height + 15.0f, look.y - 15.0f}).SetFocus({look.x, height, look.y});
		Log(fmt::format("{:.1f}s: {}", _seconds, Name(command.kind)));
		return;
	}
	if (command.kind == Kind::ClickScroll)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), ClickScroll(command)));
		return;
	}
	if (command.kind == Kind::ClickCreatureAt)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), ClickCreatureAt(command)));
		return;
	}
	if (command.kind == Kind::TapRewardNear)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), TapRewardNear(command)));
		return;
	}
	if (command.kind == Kind::ClickCreatureNear)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), ClickCreatureNear(command)));
		return;
	}
	if (command.kind == Kind::HandTakeNear)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), HandTakeNear(command)));
		return;
	}
	if (command.kind == Kind::RunScript)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), RunScript(command)));
		return;
	}
	if (command.kind == Kind::CallNative)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), CallNative(command)));
		return;
	}
	if (command.kind == Kind::StopScript)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), StopScript(command)));
		return;
	}
	if (command.kind == Kind::RecordChallenge)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), RecordChallenge(command)));
		return;
	}
	if (GiveFrontEndCommand(command))
	{
		return;
	}
	if (GiveStoryCommand(command))
	{
		return;
	}
	GiveCreatureCommand(command);
}

void Runner::Frame(Shot shot, size_t creature, float distance)
{
	_shot = shot;
	_shotCreature = creature;
	_shotDistance = distance;
	UpdateCamera();
}

void Runner::UpdateCamera()
{
	if (!_shot.has_value() || _scenario == nullptr || !Locator::terrainSystem::has_value())
	{
		return;
	}
	// Creature Mode takes the camera from the scenario's shot for good
	if (Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive())
	{
		_shot.reset();
		return;
	}
	auto& camera = Locator::camera::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& land = Locator::terrainSystem::value();
	std::optional<CameraPlacement> placement;
	switch (*_shot)
	{
	case Shot::Testbed:
		// As the testbed leaves it: south of the middle and up, looking north just east of the middle
		placement = CameraPlacement {
		    .origin = {_middle.x + flat_land::k_CameraAside, land.GetHeightAt(_middle) + flat_land::k_CameraHeight,
		               _middle.y - flat_land::k_CameraBack},
		    .focus = {_middle.x + flat_land::k_CameraAside, land.GetHeightAt(_middle), _middle.y},
		};
		break;
	case Shot::Overview:
	{
		std::vector<glm::vec2> points;
		for (size_t i = 0; i < _creatures.size(); ++i)
		{
			if (const auto entity = CreatureAt(i))
			{
				points.push_back(glm::xz(registry.Get<Transform>(*entity).position));
			}
		}
		for (const auto& object : _scenario->objects)
		{
			points.push_back(MapPoint(_middle, object.offset));
		}
		for (const auto& particle : _scenario->particles)
		{
			points.push_back(MapPoint(_middle, particle.offset));
		}
		for (const auto& extra : _scenario->framing.include)
		{
			points.push_back(MapPoint(_middle, extra));
		}
		// And everywhere the creatures are sent
		for (const auto& command : _scenario->commands)
		{
			if (HasPoint(command.kind))
			{
				points.push_back(MapPoint(_middle, command.point));
			}
		}
		const auto bounds = BoundsOf(points, glm::vec2(k_MinOverviewHalfSize));
		placement = Overview({bounds.centre.x, land.GetHeightAt(bounds.centre), bounds.centre.y}, bounds.halfSize,
		                     FieldsOfView(camera), _shotDistance);
		break;
	}
	case Shot::Placed:
	{
		const auto eye = MapPoint(_middle, {_scenario->framing.eye.x, _scenario->framing.eye.z});
		const auto look = MapPoint(_middle, {_scenario->framing.look.x, _scenario->framing.look.z});
		placement = CameraPlacement {
		    .origin = {eye.x, land.GetHeightAt(eye) + _scenario->framing.eye.y, eye.y},
		    .focus = {look.x, land.GetHeightAt(look) + _scenario->framing.look.y, look.y},
		};
		break;
	}
	case Shot::Follow:
	case Shot::Head:
		if (const auto entity = CreatureAt(_shotCreature))
		{
			// The camera keeps up with the body where the frame draws it
			const auto transform =
			    DrawnTransform(registry.Get<Transform>(*entity), registry.TryGet<CreatureAnimation>(*entity));
			const auto height = CreatureHeight(ShownSize(registry.Get<Creature>(*entity)));
			placement = *_shot == Shot::Follow ? Follow(transform.position, height, _shotDistance)
			                                   : Head(transform.position, AheadOf(transform), height, _shotDistance);
		}
		break;
	}
	if (placement.has_value())
	{
		camera.SetOrigin(placement->origin).SetFocus(placement->focus);
	}
	// A shot of everything is taken once; following and close ups keep up with the creature
	if (*_shot == Shot::Overview || *_shot == Shot::Testbed || *_shot == Shot::Placed)
	{
		_shot.reset();
	}
}

void Runner::UpdateDemoPing() const
{
	if (_demoPingSince.has_value() && _stage != nullptr)
	{
		_stage->SetDemoRoundTrip(DemoRoundTrip(_seconds - *_demoPingSince));
	}
}

void Runner::Update(float seconds)
{
	if (_scenario == nullptr)
	{
		return;
	}
	UpdateCamera();
	if (!_running)
	{
		return;
	}
	UpdateChecks();
	// Another land was loaded over the testbed
	const auto gone = [](entt::entity entity) { return !Locator::entitiesRegistry::value().Valid(entity); };
	if ((!_creatures.empty() && std::ranges::none_of(std::views::iota(size_t {0}, _creatures.size()),
	                                                 [this](size_t i) { return CreatureAt(i).has_value(); })) ||
	    (!_crowdEntities.empty() && gone(_crowdEntities.front())))
	{
		_running = false;
		_shot.reset();
		Log("Its creatures are gone");
		return;
	}
	_seconds += seconds;
	LogMiracles();
	Measure();
	SpawnCrowd();
	UpdateParticles(seconds);
	UpdateVortices();
	UpdateRewards();
	UpdateMiracles(seconds);
	UpdateVillagerWalks();
	UpdateTurnsAsked();
	UpdateDeviceRestore();
	UpdateThrows();
	UpdateComputerActions();
	WatchGesture();
	FinishTakingFireBall();
	ApplyStates();
	UpdatePointer(seconds);
	UpdateDemoPing();
	auto due = Advance(_timeline, _scenario->commands, _scenario->repeatFrom, seconds, [this](const Command& command) {
		const bool toHand =
		    command.kind == Kind::HoldSeed || command.kind == Kind::DrawGesture || command.kind == Kind::SummonSeed;
		if (toHand)
		{
			return !Locator::gestureSystem::has_value() || !Locator::gestureSystem::value().IsDrawingPath();
		}
		// The temple takes no press while a script has the cinema bars in, as when the advisors introduce a room
		const bool toTemple = command.kind == Kind::LookAtTempleScroll || command.kind == Kind::LookAtSaveGamePicture;
		if (toTemple)
		{
			return !Locator::cinematicDirectorSystem::has_value() ||
			       !Locator::cinematicDirectorSystem::value().IsWideScreenOn();
		}
		return IsFree(command.creature);
	});
	const auto turn = Locator::time::has_value() ? std::optional(Locator::time::value().GetTurn()) : std::nullopt;
	if (_followedGives.has_value() && turn.has_value())
	{
		due = GivesAt(*_followedGives, *turn, _scenario->commands.size());
	}
	for (const auto index : due)
	{
		if (_giveObserver && turn.has_value())
		{
			_giveObserver(*turn, index);
		}
		Give(_scenario->commands[index]);
	}
}

void Runner::BeforeTurn()
{
	if (!Locator::time::has_value())
	{
		return;
	}
	if (TurnStepOf(_layoutTurn, Locator::time::value().GetTurn()) == TurnStep::Update)
	{
		Update(std::chrono::duration<float>(ecs::systems::TimeSystemInterface::k_TurnDuration).count());
	}
}

std::optional<double> Runner::ReadCheck(const checks::Check& check) const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto creatureIndex = [&check]() { return check.index; };
	switch (check.quantity)
	{
	case checks::Quantity::CreaturesAlive:
	{
		size_t alive = 0;
		for (size_t i = 0; i < _creatures.size() + _slotCreatures.size(); ++i)
		{
			alive += CreatureAt(i).has_value() ? 1 : 0;
		}
		return static_cast<double>(alive);
	}
	case checks::Quantity::ObjectsAlive:
	{
		size_t alive = 0;
		for (size_t i = 0; i < _objects.size(); ++i)
		{
			alive += ObjectAt(i).has_value() ? 1 : 0;
		}
		return static_cast<double>(alive);
	}
	case checks::Quantity::Entities:
		for (const auto& [name, count] : EntityCounts())
		{
			if (name == check.kind)
			{
				return static_cast<double>(count);
			}
		}
		return std::nullopt;
	case checks::Quantity::CreatureEnergy:
	case checks::Quantity::CreatureThirst:
	case checks::Quantity::CreatureExhaustion:
	case checks::Quantity::CreaturePoo:
	case checks::Quantity::CreatureWarmth:
	{
		const auto creature = CreatureAt(creatureIndex());
		const auto* needs = creature.has_value() ? registry.TryGet<const ecs::components::CreatureNeeds>(*creature) : nullptr;
		if (needs == nullptr)
		{
			return std::nullopt;
		}
		const auto& body = needs->needs;
		switch (check.quantity)
		{
		case checks::Quantity::CreatureEnergy:
			return static_cast<double>(body.energy);
		case checks::Quantity::CreatureThirst:
			return static_cast<double>(body.dehydration);
		case checks::Quantity::CreatureExhaustion:
			return static_cast<double>(body.exhaustion);
		case checks::Quantity::CreaturePoo:
			return static_cast<double>(body.poo);
		default:
			return static_cast<double>(body.warmth);
		}
	}
	case checks::Quantity::CreatureSize:
	{
		const auto creature = CreatureAt(creatureIndex());
		const auto* body = creature.has_value() ? registry.TryGet<const Creature>(*creature) : nullptr;
		return body != nullptr ? std::optional(static_cast<double>(body->size)) : std::nullopt;
	}
	case checks::Quantity::CreatureLife:
	{
		const auto creature = CreatureAt(creatureIndex());
		return creature.has_value() ? std::optional(static_cast<double>(ecs::world_objects::LifeOf(*creature))) : std::nullopt;
	}
	case checks::Quantity::CreatureDistance:
	{
		const auto creature = CreatureAt(creatureIndex());
		const auto* transform = creature.has_value() ? registry.TryGet<const Transform>(*creature) : nullptr;
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		const auto goal = MapPoint(_middle, check.point);
		return static_cast<double>(glm::distance(glm::vec2(transform->position.x, transform->position.z), goal));
	}
	case checks::Quantity::ObjectResource:
	{
		const auto object = ObjectAt(check.index);
		if (!object.has_value() || !Locator::resourceStoreSystem::has_value())
		{
			return std::nullopt;
		}
		const auto& stores = Locator::resourceStoreSystem::value();
		return static_cast<double>(stores.GetResource(*object, ResourceType::Food)) +
		       static_cast<double>(stores.GetResource(*object, ResourceType::Wood));
	}
	case checks::Quantity::ObjectDistance:
	{
		const auto object = ObjectAt(check.index);
		const auto* transform = object.has_value() ? registry.TryGet<const Transform>(*object) : nullptr;
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		const auto goal = MapPoint(_middle, check.point);
		return static_cast<double>(glm::distance(glm::vec2(transform->position.x, transform->position.z), goal));
	}
	case checks::Quantity::ObjectTemperature:
	{
		const auto object = ObjectAt(check.index);
		if (!object.has_value() || !Locator::fireSystem::has_value())
		{
			return std::nullopt;
		}
		return static_cast<double>(Locator::fireSystem::value().GetTemperature(*object));
	}
	case checks::Quantity::FireNear:
	{
		if (!Locator::fireSystem::has_value())
		{
			return std::nullopt;
		}
		const auto point = MapPoint(_middle, check.point);
		return Locator::fireSystem::value().IsFireNear({point.x, 0.0f, point.y}, check.radius) ? 1.0 : 0.0;
	}
	case checks::Quantity::RewardChestsInATown:
	{
		size_t inATown = 0;
		registry.Each<const ecs::components::Reward>([&registry, &inATown](const ecs::components::Reward& chest) {
			inATown += registry.Valid(chest.town) && registry.AllOf<ecs::components::Town>(chest.town) ? 1 : 0;
		});
		return static_cast<double>(inATown);
	}
	}
	return std::nullopt;
}

void Runner::UpdateChecks()
{
	if (_scenario == nullptr || _checksReported || !Locator::time::has_value())
	{
		return;
	}
	// A scenario with no checks says nothing, unless the command line asked for its verdict
	if (_scenario->checks.empty() && !_benchmark.reportChecks)
	{
		return;
	}
	if (!_checks.Done())
	{
		const auto turns = Locator::time::value().GetTurn() - _startTurn;
		_checks.Update(turns, [this](const checks::Check& check) { return ReadCheck(check); });
	}
	const auto verdict = _checks.GetVerdict();
	if (verdict == checks::Verdict::Waiting)
	{
		return;
	}
	_checksReported = true;
	auto lines = _checks.Report(_scenario->id);
	for (const auto& line : lines)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Scenario check: {}", line);
	}
	if (_stage != nullptr)
	{
		_stage->ReportChecks({.scenario = std::string(_scenario->id), .verdict = verdict, .lines = std::move(lines)},
		                     _benchmark.reportChecks);
	}
}

void Runner::UpdateTurnsAsked()
{
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		_turnsAsked.clear();
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	const auto& registry = Locator::entitiesRegistry::value();
	// A turn asked for again starts once the creature stands. A creature that is gone asks no more.
	std::erase_if(_turnsAsked, [&](const std::pair<entt::entity, glm::vec2>& asked) {
		return !registry.Valid(asked.first) || locomotion.TurnToFace(asked.first, asked.second);
	});
}

void Runner::UpdateDeviceRestore()
{
	if (!_deviceRestoreAsked.has_value() || !Locator::rendererRequests::has_value() || !Locator::skyClockSystem::has_value())
	{
		return;
	}
	const auto* dome = Locator::entitiesRegistry::value().TryGet<const ecs::components::SkyDome>(
	    Locator::skyClockSystem::value().GetEntities().dome);
	if (dome == nullptr)
	{
		return;
	}
	// Part way: the follow blended its first band and has more rows to blend
	const auto rowsDone = dome->follow.RowsDone();
	if (rowsDone == 0 || rowsDone >= sky_dome::k_Rows)
	{
		return;
	}
	Locator::rendererRequests::value().RequestDeviceRestore(*_deviceRestoreAsked);
	_deviceRestoreAsked.reset();
	Log(fmt::format("{:.1f}s: the graphics device is made again with {} rows of the dome blended for the sky type {:.3f}",
	                _seconds, rowsDone, dome->follow.Following()));
}

void Runner::Log(std::string line)
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Testbed: {}", line);
	_log.push_back(std::move(line));
	while (_log.size() > k_LogLines)
	{
		_log.pop_front();
	}
}
