/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed runner: it prepares the world of a scenario (the date, the weather, the objects, the birds and the
// creatures) and keeps its villagers on their walks.

#include <cmath>
#include <ctime>

#include <algorithm>
#include <limits>
#include <numbers>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Animals/BirdRules.h"
#include "Animals/GrazerRules.h"
#include "Animals/HunterRules.h"
#include "Camera/Camera.h"
#include "Commands/PlayerCommands.h"
#include "Common/PortableMath.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureRig.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/DeadTreeArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/FieldArchetype.h"
#include "ECS/Archetypes/FishFarmArchetype.h"
#include "ECS/Archetypes/FlowersArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/BuildingReplan.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLeashRope.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PrayerPower.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/Weather.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerBreeding.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/ScriptCast.h"
#include "Magic/SpellRules.h"
#include "Magic/TownBelief.h"
#include "Sound/GameMusic.h"
#include "TestbedScenarioRunner.h"
#include "TestbedScenarioRunnerParts.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::runner_parts;
using openblack::ecs::archetypes::CreatureArchetype;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::Transform;

namespace
{
/// Forced weather lasts as long as anyone watches
constexpr float k_WeatherSeconds = 3600.0f;

/// Enough building to finish a worship site at once
constexpr float k_BuildWholeSite = 1.0e6f;

/// The prayer power the testbed's player has when a scenario doesn't say, as the testbed starts with
constexpr float k_PlentyOfPrayer = 1.0e6f;

/// The town of an object of a scenario: the land's own town that the object names, or else the scenario's town
uint32_t ObjectTown(const ObjectSetup& object, glm::vec2 middle)
{
	return object.landTown.has_value() ? *object.landTown : ScenarioTown(middle);
}

/// The weather of each kind laid over the island, as the weather window's presets have it
ecs::components::WeatherInfo WeatherOf(Weather weather)
{
	switch (weather)
	{
	case Weather::Rain:
		return {.temperature = 12, .rain = 80, .overcast = 90, .windZ = 15};
	case Weather::Thunderstorm:
		return {.temperature = 20, .rain = 100, .overcast = 100, .windZ = 30};
	case Weather::Snow:
		return {.temperature = -5, .snow = 70, .overcast = 80, .windZ = 5};
	case Weather::Blizzard:
		return {.temperature = -15, .snow = 100, .overcast = 100, .windZ = 60};
	case Weather::Clear:
	default:
		// The testbed has no climate of its own, which would leave it at freezing; a clear day is mild
		return {.temperature = 18};
	}
}

/// An animal as the scenario gives it: a living one as a land's script makes it, or one that stays at its position
entt::entity PlaceAnimal(AnimalInfo type, const ObjectSetup& object, glm::vec3 position, float yaw)
{
	if (!Locator::animalSystem::has_value())
	{
		return ecs::archetypes::AnimalArchetype::Create(type, position, yaw, object.scale, PlayerNames::NEUTRAL);
	}
	auto& animalSystem = Locator::animalSystem::value();
	const glm::vec2 point {position.x, position.z};
	entt::entity animal = entt::null;
	// A script's animal: made and held still as a land's script makes one, and under the script's control from then on
	if (object.inScript)
	{
		animal = animalSystem.CreateScriptAnimal(type, point);
		if (animal != entt::null)
		{
			Locator::entitiesRegistry::value().AssignOrReplace<ecs::components::ScriptControlled>(animal);
			return animal;
		}
	}
	if (object.living && animals::grazers::IsGrazer(type))
	{
		animal = animalSystem.CreateGrazer(type, point, 0, entt::null, entt::null);
	}
	else if (object.living && animals::birds::IsLandBird(type))
	{
		animal = animalSystem.CreateBird(type, point, 0, entt::null);
	}
	else if (object.living && animals::hunters::IsWildHunter(type))
	{
		animal = animalSystem.CreateHunter(type, point, 0, entt::null);
		// A hungry hunter starts with its need to eat full
		auto& registry = Locator::entitiesRegistry::value();
		if (auto* hunter = animal != entt::null ? registry.TryGet<ecs::components::Hunter>(animal) : nullptr;
		    hunter != nullptr && object.hungry)
		{
			hunter->needs.hunger =
			    static_cast<int16_t>(Locator::infoConstants::value().animal.at(static_cast<size_t>(type)).hunger);
		}
	}
	if (animal != entt::null)
	{
		return animal;
	}
	if (object.living)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Testbed: animal kind {} cannot live yet, it stays where it is put",
		                   static_cast<int>(type));
	}
	return animalSystem.CreateStillAnimal(type, position, yaw, object.scale);
}

/// A villager's or animal's life and poison as the scenario gives them
/// Make the crop of a testbed field ripe: fully sown, at the ripe age of its type, with all the food of its type
void RipenCrop(entt::entity field, FieldTypeInfo type, bool ripe)
{
	auto* state =
	    ripe && field != entt::null ? Locator::entitiesRegistry::value().TryGet<ecs::components::Field>(field) : nullptr;
	const auto& types = Locator::infoConstants::value().fieldType;
	const auto row = static_cast<size_t>(type);
	if (state == nullptr || row >= types.size())
	{
		return;
	}
	const auto& info = types.at(row);
	state->crop = {.timesSown = static_cast<uint8_t>(info.timesToSow), .age = info.ageRecolt, .food = info.totalFoodInField};
}

void SetLifeAndPoison(entt::entity entity, const ObjectSetup& object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (entity == entt::null || !registry.Valid(entity))
	{
		return;
	}
	if (object.life.has_value())
	{
		if (auto* villager = registry.TryGet<ecs::components::Villager>(entity))
		{
			villager->life = std::clamp(*object.life, 0.0f, 1.0f);
		}
		// A street lantern starts with that much life
		if (registry.AllOf<ecs::components::StreetLantern>(entity))
		{
			ecs::world_objects::SetLife(entity, std::clamp(*object.life, 0.0f, 1.0f));
		}
		// A building is hurt down to it, as anything that hurts it does, which opens a site for its repair
		if (registry.AllOf<ecs::components::Abode>(entity))
		{
			static_cast<void>(ecs::world_objects::ReduceLife(entity, 1.0f - std::clamp(*object.life, 0.0f, 1.0f)));
		}
		if (auto* animal = registry.TryGet<ecs::components::Animal>(entity))
		{
			animal->life = std::clamp(*object.life, 0.0f, 1.0f);
			// One given no life is killed: it falls dead and lies its time
			if (animal->Dead() && Locator::animalSystem::has_value())
			{
				Locator::animalSystem::value().SetDying(entity);
			}
		}
	}
	if (auto* villager = registry.TryGet<ecs::components::Villager>(entity);
	    villager != nullptr && (object.woodHeld != 0 || object.foodHeld != 0))
	{
		villager->woodHeld = object.woodHeld;
		villager->foodHeld = object.foodHeld;
	}
	if (object.poisoned)
	{
		if (auto* pot = registry.TryGet<ecs::components::Pot>(entity); pot != nullptr)
		{
			pot->poisoned = true;
		}
		else
		{
			registry.AssignOrReplace<ecs::components::Poisoned>(entity);
		}
	}
	if (object.indestructible)
	{
		registry.AssignOrReplace<ecs::components::Indestructible>(entity);
	}
	if (object.cannotBePickedUp)
	{
		registry.AssignOrReplace<ecs::components::CannotBePickedUp>(entity);
	}
	// A villager given a row of the table of named people takes that name in place of any name that it got when made
	if (object.nameRow.has_value() && registry.AllOf<ecs::components::Villager>(entity))
	{
		auto& used = registry.Context().namedVillagersUsed;
		if (const auto* named = registry.TryGet<ecs::components::NamedVillager>(entity))
		{
			used.at(named->row) = false;
		}
		used.at(*object.nameRow) = true;
		registry.AssignOrReplace<ecs::components::NamedVillager>(entity, *object.nameRow);
	}
	if (object.immovable)
	{
		registry.AssignOrReplace<ecs::components::Immovable>(entity);
	}
	// A building is held by a script as a script holds a thing that one of its variables names
	if (object.inScript && registry.AnyOf<ecs::components::Abode, ecs::components::SpellDispenser>(entity))
	{
		registry.AssignOrReplace<ecs::components::InScript>(entity);
	}
	if (object.inScript && registry.AllOf<ecs::components::Villager>(entity) && Locator::livingActionSystem::has_value())
	{
		Locator::livingActionSystem::value().VillagerSetScriptState(entity, VillagerStates::InScript);
	}
	if (object.scriptControlled && registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(entity))
	{
		registry.AssignOrReplace<ecs::components::ScriptControlled>(entity);
	}
	if (object.disciple != VillagerDisciple::None && registry.AllOf<ecs::components::Villager>(entity))
	{
		ecs::villager_disciple::SetType(entity, object.disciple);
	}
}
} // namespace

namespace openblack::testbed_scenarios::runner_parts
{
/// The scenario's town, a Celtic town of the player's in the middle of the map, made when first needed
uint32_t ScenarioTown(glm::vec2 middle)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& towns = registry.Context().towns;
	if (const auto found = towns.find(k_ScenarioTownId); found != towns.end())
	{
		if (registry.Valid(found->second))
		{
			return k_ScenarioTownId;
		}
		towns.erase(found);
	}
	const glm::vec3 position {middle.x, Locator::terrainSystem::value().GetHeightAt(middle), middle.y};
	ecs::archetypes::TownArchetype::Create(static_cast<int>(k_ScenarioTownId), position, PlayerNames::PLAYER_ONE,
	                                       Tribe::CELTIC);
	return k_ScenarioTownId;
}
} // namespace openblack::testbed_scenarios::runner_parts

void Runner::SetUpDate(const std::optional<RealDate>& date)
{
	if (!Locator::time::has_value())
	{
		return;
	}
	auto& time = Locator::time::value();
	if (!date.has_value())
	{
		if (_pinnedDateBefore.has_value())
		{
			time.PinDate(*_pinnedDateBefore);
			_pinnedDateBefore.reset();
		}
		return;
	}
	if (!_pinnedDateBefore.has_value())
	{
		_pinnedDateBefore = time.GetPinnedDate();
	}
	std::tm local {};
	local.tm_year = date->year - 1900;
	local.tm_mon = date->month - 1;
	local.tm_mday = date->day;
	local.tm_hour = date->hour;
	local.tm_min = date->minute;
	// The computer's time zone tells whether summer time applies on that date
	local.tm_isdst = -1;
	const auto unixTime = static_cast<int64_t>(std::mktime(&local));
	time.PinDate(ecs::systems::PinForDateNow(unixTime, time.GetTicks()));
	Log(fmt::format("Date set to {:04}-{:02}-{:02} {:02}:{:02}", date->year, date->month, date->day, date->hour, date->minute));
}

void Runner::SetUpEnvironment(const Environment& environment)
{
	SetUpDate(environment.realDate);
	if (environment.nightWhisperChecks.has_value() && Locator::guidanceSystem::has_value())
	{
		Locator::guidanceSystem::value().StageNightWhisper(*environment.nightWhisperChecks);
		Log(fmt::format("The night whisper checks the clock next, after {} checks", *environment.nightWhisperChecks));
	}
	// A land of the story keeps its own sky, weather, alignment and prayer power
	const bool storyLand = environment.land == Land::Story;
	if (!storyLand && Locator::skyClockSystem::has_value())
	{
		auto& sky = Locator::skyClockSystem::value();
		sky.SetTime(environment.hour);
		sky.GetClock().SetRunning(environment.clockRuns);
	}
	if (!storyLand && Locator::weatherSystem::has_value())
	{
		// Only the scenario's weather: the climates breed no storms of their own
		auto& weather = Locator::weatherSystem::value();
		weather.ClearStorms();
		weather.SetStormCreationEnabled(false);
		const auto thunder = environment.weather == Weather::Thunderstorm;
		weather.ForceStorm({.effect = WeatherOf(environment.weather),
		                    .seconds = k_WeatherSeconds,
		                    .fadeSeconds = 0.5f,
		                    .thunderWait = thunder ? glm::vec2(3.0f, 15.0f) : glm::vec2(0.0f),
		                    .boltWait = thunder ? environment.boltSeconds.value_or(glm::vec2(2.0f, 8.0f)) : glm::vec2(0.0f)});
	}
	if (environment.snowDepth.has_value() && Locator::snowSystem::has_value())
	{
		Locator::snowSystem::value().Cover(*environment.snowDepth);
	}
	if (environment.backgroundWind.has_value() && Locator::weatherSystem::has_value())
	{
		// The wind of the background weather, as the real-world weather sets it. The next turn gives the ambient wind.
		auto& weather = Locator::weatherSystem::value();
		auto background = weather.GetBackgroundWeather();
		if (!_backgroundWeatherBefore.has_value())
		{
			_backgroundWeatherBefore = background;
		}
		const auto wind = *environment.backgroundWind;
		background.windX = static_cast<int8_t>(wind.x);
		background.windZ = static_cast<int8_t>(wind.y);
		weather.SetBackgroundWeather(background);
		Log(fmt::format("Background wind set to {}, {}", wind.x, wind.y));
	}
	if (Locator::creaturePhysiologySystem::has_value())
	{
		auto& physiology = Locator::creaturePhysiologySystem::value();
		physiology.SetTimeScale(environment.bodyTimeScale);
		physiology.SetFaintingEnabled(environment.fainting);
	}
	if (Locator::footprintSystem::has_value())
	{
		// A scenario starts with no prints from the one before
		Locator::footprintSystem::value().Reset();
		Locator::footprintSystem::value().SetAprilFoolsOverride(environment.aprilFools);
	}
	if (!storyLand && Locator::alignmentSystem::has_value())
	{
		Locator::alignmentSystem::value().SetPlayerAlignment(PlayerNames::PLAYER_ONE,
		                                                     environment.playerAlignment.value_or(0.0f));
	}
	// A scenario starts with the first player as the god of this computer, after a scenario that changed it
	if (std::exchange(_ownGodChanged, false) && Locator::handSystem::has_value())
	{
		Locator::handSystem::value().SetOwnPlayer(PlayerNames::PLAYER_ONE);
	}
	// The player's prayer power: as much as the scenario gives, or plenty
	if (!storyLand && Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		registry.Each<const ecs::components::Player, ecs::components::PrayerPower>(
		    [&environment](const ecs::components::Player& player, ecs::components::PrayerPower& prayer) {
			    if (player.name == PlayerNames::PLAYER_ONE)
			    {
				    prayer.chants = environment.prayer.value_or(k_PlentyOfPrayer);
				    prayer.infinite = false;
			    }
		    });
	}
	// The switch of the Soccer Add-On, which the next land reads again from the settings
	if (environment.football && Locator::footballSystem::has_value())
	{
		Locator::footballSystem::value().StartGame(true, Locator::time::value().IsNetworkGame());
	}
	if (environment.pushScrolling && Locator::config::has_value())
	{
		Locator::config::value().pushScrolling = true;
		Locator::config::value().testbedPushScrollInWindow = true;
	}
	if (environment.profileSymbol.has_value())
	{
		if (_stage != nullptr)
		{
			_stage->MakePlayersSymbols(environment.profileSymbol);
		}
	}
	if (environment.drawFootpaths && Locator::config::has_value())
	{
		Locator::config::value().drawFootpaths = true;
	}
	if (environment.unlockedBonusCreatures.has_value() && Locator::config::has_value())
	{
		auto& unlocked = Locator::config::value().unlockedBonusCreatures;
		if (!_unlockedBefore.has_value())
		{
			_unlockedBefore = unlocked;
		}
		unlocked = *environment.unlockedBonusCreatures;
	}
	if (!environment.cameraZone.empty() && Locator::cameraZoneSystem::has_value())
	{
		_cameraZoneSet = true;
		if (!Locator::cameraZoneSystem::value().SetZones(environment.cameraZone))
		{
			Log(fmt::format("The camera zones of {} did not load", environment.cameraZone));
		}
	}
	if (environment.landNumber.has_value() && Locator::entitiesRegistry::has_value())
	{
		auto& landNumber = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
		if (!_landNumberBefore.has_value())
		{
			_landNumberBefore = landNumber;
		}
		landNumber = *environment.landNumber;
	}
	if (environment.skirmish && Locator::entitiesRegistry::has_value())
	{
		auto& skirmish = Locator::entitiesRegistry::value().Context().skirmish;
		if (!_skirmishBefore.has_value())
		{
			_skirmishBefore = skirmish;
		}
		skirmish = true;
	}
	if (environment.networkGame && Locator::time::has_value())
	{
		auto& clock = Locator::time::value();
		if (!_networkGameBefore.has_value())
		{
			_networkGameBefore = clock.IsNetworkGame();
		}
		clock.SetNetworkGame(true);
	}
	// The other players that the scenario puts in the game
	if (Locator::entitiesRegistry::has_value() && Locator::playerSystem::has_value())
	{
		for (const auto name : environment.players)
		{
			if (!ecs::creature_from_file::PlayerInGame(name))
			{
				const auto player = ecs::archetypes::PlayerArchetype::Create(name);
				Locator::playerSystem::value().AddPlayer(player);
				_addedPlayers.push_back(player);
			}
		}
	}
	if (environment.cursor && Locator::windowing::has_value())
	{
		const auto size = glm::vec2(Locator::windowing::value().GetSize());
		_stage->SetMousePosition(glm::ivec2(*environment.cursor * size));
	}
}

void Runner::PlaceHighlights(const Scenario& scenario, glm::vec2 middle)
{
	if (!Locator::scriptHighlightSystem::has_value())
	{
		return;
	}
	auto& highlights = Locator::scriptHighlightSystem::value();
	const auto& land = Locator::terrainSystem::value();
	for (const auto& setup : scenario.highlights)
	{
		const auto point = MapPoint(middle, setup.offset);
		const auto highlight = highlights.Create(setup.kind, {point.x, land.GetHeightAt(point), point.y}, setup.challenge);
		if (highlight != entt::null)
		{
			if (setup.tip.has_value())
			{
				highlights.SetProperties(highlight, setup.tip->first, setup.tip->second);
			}
			if (setup.height.has_value())
			{
				highlights.SetDrawHeight(highlight, *setup.height);
			}
			if (setup.active)
			{
				highlights.SetActive(highlight, true);
			}
		}
		_objects.push_back(highlight);
	}
}

void Runner::PlaceFootpaths(const Scenario& scenario, glm::vec2 middle)
{
	using ecs::components::Footpath;
	using ecs::components::FootpathLink;
	auto& registry = Locator::entitiesRegistry::value();
	const auto& land = Locator::terrainSystem::value();
	// Where an object stands on the map, in whole map units: a fish farm on its map cell's middle
	const auto placeOf = [&registry](entt::entity object) {
		if (const auto* farm = registry.TryGet<const ecs::components::FishFarm>(object))
		{
			return farm->place;
		}
		const auto& position = registry.Get<const ecs::components::Transform>(object).position;
		return glm::ivec2(map_coords::ToFixed(position.x), map_coords::ToFixed(position.z));
	};
	const auto nodeAt = [&land](glm::ivec2 place) {
		const glm::vec2 ground {map_coords::ToMetres(place.x), map_coords::ToMetres(place.y)};
		return Footpath::Node {.position = {ground.x, land.GetHeightAt(ground), ground.y}, .place = place};
	};
	for (const auto& setup : scenario.footpaths)
	{
		const auto entity = registry.Create();
		auto& footpath = registry.Assign<Footpath>(entity);
		if (setup.startsAt.has_value() && *setup.startsAt < _objects.size() && registry.Valid(_objects[*setup.startsAt]))
		{
			footpath.nodes.push_back(nodeAt(placeOf(_objects[*setup.startsAt])));
		}
		for (const auto offset : setup.nodes)
		{
			const auto point = MapPoint(middle, offset);
			footpath.nodes.push_back(nodeAt({map_coords::ToFixed(point.x), map_coords::ToFixed(point.y)}));
		}
		for (const auto index : setup.leadsFrom)
		{
			if (index >= _objects.size() || !registry.Valid(_objects[index]))
			{
				continue;
			}
			const auto object = _objects[index];
			if (auto* link = registry.TryGet<FootpathLink>(object))
			{
				link->footpaths.push_back(static_cast<Footpath::Id>(entity));
			}
			else
			{
				const auto place = placeOf(object);
				registry.Assign<FootpathLink>(object, nodeAt(place).position,
				                              std::vector<Footpath::Id> {static_cast<Footpath::Id>(entity)});
			}
		}
	}
}

void Runner::PlaceObjects(const Scenario& scenario, glm::vec2 middle)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	// A forest the trees are given that the land doesn't have yet is made where its first tree stands, as the land's
	// script makes its forests before planting them
	if (Locator::forestSystem::has_value())
	{
		auto& forests = Locator::forestSystem::value();
		for (const auto& object : scenario.objects)
		{
			if (std::holds_alternative<TreeInfo>(object.type) && object.forest != 0 && !forests.LandForestOf(object.forest))
			{
				const auto point = MapPoint(middle, object.offset);
				forests.MakeLandForest(object.forest, {point.x, land.GetHeightAt(point), point.y}, entt::null, false);
			}
		}
	}
	for (const auto& object : scenario.objects)
	{
		const auto point = MapPoint(middle, object.offset);
		const glm::vec3 position {point.x, land.GetHeightAt(point), point.y};
		const auto yaw = glm::radians(object.yawDegrees);
		_objects.push_back(std::visit(
		    [&]<typename T>(T type) {
			    if constexpr (std::is_same_v<T, MobileObjectInfo>)
			    {
				    return ecs::archetypes::MobileObjectArchetype::Create(position, type, yaw, object.scale);
			    }
			    else if constexpr (std::is_same_v<T, TreeInfo>)
			    {
				    if (object.dead)
				    {
					    return ecs::archetypes::DeadTreeArchetype::Create(position, type, glm::radians(object.leanDegrees.x),
					                                                      yaw, glm::radians(object.leanDegrees.y),
					                                                      object.scale);
				    }
				    return ecs::archetypes::TreeArchetype::Create(object.forest, position, type, true, yaw,
				                                                  object.fullSize.value_or(object.scale), object.scale);
			    }
			    else if constexpr (std::is_same_v<T, VillagerInfo>)
			    {
				    constexpr uint32_t k_AdultAge = 30;
				    const auto villager = ecs::archetypes::VillagerArchetype::Create(position, position, type,
				                                                                     object.villagerAge.value_or(k_AdultAge));
				    if (object.joinTown || object.landTown.has_value())
				    {
					    auto& registry = Locator::entitiesRegistry::value();
					    registry.Get<ecs::components::Villager>(villager).town =
					        registry.Context().towns.at(ObjectTown(object, middle));
				    }
				    return villager;
			    }
			    else if constexpr (std::is_same_v<T, PotInfo>)
			    {
				    return ecs::archetypes::PotArchetype::Create(position, yaw, type, object.amount);
			    }
			    else if constexpr (std::is_same_v<T, AbodeInfo>)
			    {
				    if (object.planned)
				    {
					    const auto plan = ecs::archetypes::AbodeArchetype::CreatePlan(ObjectTown(object, middle), position,
					                                                                  type, yaw, object.scale);
					    if (auto* planned = Locator::entitiesRegistry::value().TryGet<ecs::components::PlannedAbode>(plan))
					    {
						    planned->wasBuilt = object.wasBuilt;
					    }
					    return plan;
				    }
				    // A miracle dispenser of the town, as a script makes it: it holds no miracle and is off
				    const auto& rows = Locator::infoConstants::value().abode;
				    if (static_cast<size_t>(type) < rows.size() &&
				        ecs::building_replan::MadeAsDispenser(rows.at(static_cast<size_t>(type)).abodeType) &&
				        Locator::magicSystem::has_value())
				    {
					    auto& registry = Locator::entitiesRegistry::value();
					    const auto dispenser = Locator::magicSystem::value().CreateScriptDispenser(
					        position, type, yaw, object.scale, registry.Context().towns.at(ObjectTown(object, middle)));
					    if (auto* data = registry.TryGet<ecs::components::SpellDispenser>(dispenser))
					    {
						    data->notPlannedAgain = object.notPlannedAgain;
					    }
					    return dispenser;
				    }
				    const auto abode = ecs::archetypes::AbodeArchetype::Create(
				        ObjectTown(object, middle), position, type, yaw, object.scale,
				        object.storedFood.value_or(k_ScenarioTownFood), object.storedWood.value_or(k_ScenarioTownWood));
				    if (object.built.has_value())
				    {
					    ecs::construction::SetBuilt(abode, *object.built);
				    }
				    auto& building = Locator::entitiesRegistry::value().Get<ecs::components::Abode>(abode);
				    building.neglect = object.neglect;
				    building.notPlannedAgain = object.notPlannedAgain;
				    return abode;
			    }
			    else if constexpr (std::is_same_v<T, FieldTypeInfo>)
			    {
				    const auto field =
				        ecs::archetypes::FieldArchetype::Create(static_cast<int>(ScenarioTown(middle)), position, type, yaw);
				    RipenCrop(field, type, object.ripeCrop);
				    return field;
			    }
			    else if constexpr (std::is_same_v<T, AnimalInfo>)
			    {
				    return PlaceAnimal(type, object, position, yaw);
			    }
			    else if constexpr (std::is_same_v<T, MobileStaticInfo>)
			    {
				    // A lantern is made as the land scripts make it, with its light
				    if (type == MobileStaticInfo::StreetLantern || type == MobileStaticInfo::CountryLantern)
				    {
					    return ecs::archetypes::StreetLanternArchetype::Create(position, type);
				    }
				    return ecs::archetypes::MobileStaticArchetype::Create(position, type, 0.0f,
				                                                          glm::radians(object.leanDegrees.x), yaw,
				                                                          glm::radians(object.leanDegrees.y), object.scale);
			    }
			    else if constexpr (std::is_same_v<T, AnimatedStaticInfo>)
			    {
				    return ecs::archetypes::AnimatedStaticArchetype::Create(position, type, yaw, object.scale);
			    }
			    else if constexpr (std::is_same_v<T, FishFarmInfo>)
			    {
				    return ecs::archetypes::FishFarmArchetype::Create(position);
			    }
			    else if constexpr (std::is_same_v<T, FlowersInfo>)
			    {
				    return ecs::archetypes::FlowersArchetype::Create(position, type, yaw, object.scale);
			    }
			    else if constexpr (std::is_same_v<T, ScaffoldInfo>)
			    {
				    if (!Locator::workshopSystem::has_value())
				    {
					    return static_cast<entt::entity>(entt::null);
				    }
				    const auto town = object.joinTown || object.landTown.has_value()
				                          ? Locator::entitiesRegistry::value().Context().towns.at(ObjectTown(object, middle))
				                          : entt::null;
				    const auto scaffold =
				        Locator::workshopSystem::value().CreateScaffold(position, town, entt::null, yaw, object.scale);
				    if (auto* data = Locator::entitiesRegistry::value().TryGet<ecs::components::Scaffold>(scaffold))
				    {
					    data->destroysThingsInWay = object.clearsGround;
				    }
				    return scaffold;
			    }
			    else
			    {
				    const auto feature = ecs::archetypes::FeatureArchetype::Create(position, type, yaw, object.scale);
				    // A feature that is only part built has a site of the scenario's town, which builds it up
				    if (object.built.has_value())
				    {
					    ecs::construction::SetBuilt(feature, *object.built);
					    static_cast<void>(ecs::construction::OpenSiteForTown(
					        feature, Locator::entitiesRegistry::value().Context().towns.at(ScenarioTown(middle))));
				    }
				    return feature;
			    }
		    },
		    object.type));
		SetLifeAndPoison(_objects.back(), object);
		if (object.homeIn.has_value() && *object.homeIn + 1 < _objects.size() && Locator::townSystem::has_value())
		{
			// The villager lives in the building as one of its people
			Locator::townSystem::value().AddVillagerToAbode(_objects.at(*object.homeIn), _objects.back());
		}
		if (object.firefly && Locator::fireflySystem::has_value())
		{
			const auto& placed = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(_objects.back());
			Locator::fireflySystem::value().Create(map_coords::FromWorld(land, placed.position));
		}
		if (object.alight && Locator::fireSystem::has_value())
		{
			Locator::fireSystem::value().SetOnFire(_objects.back(), 0.0f);
		}
		if (object.reaction.has_value() && Locator::reactionSystem::has_value())
		{
			const auto& placed = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(_objects.back());
			Locator::reactionSystem::value().Create({.initiator = _objects.back(),
			                                         .type = *object.reaction,
			                                         .player = PlayerNames::PLAYER_ONE,
			                                         .position = placed.position});
		}
	}
	// A villager of the town moves into its building, as one born there lives in it
	for (size_t i = 0; i < scenario.objects.size(); ++i)
	{
		const auto& home = scenario.objects[i].livesIn;
		if (home.has_value() && *home < _objects.size() && Locator::townSystem::has_value())
		{
			Locator::townSystem::value().MoveInAnyway(_objects.at(*home), _objects.at(i));
		}
	}
}

void Runner::SetUpStatistics(const Scenario& scenario)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (scenario.playerStatistics)
	{
		registry.Each<const ecs::components::Player, ecs::components::PlayerStatistics>(
		    [&scenario](const ecs::components::Player& player, ecs::components::PlayerStatistics& statistics) {
			    if (player.name == PlayerNames::PLAYER_ONE)
			    {
				    scenario.playerStatistics(statistics);
			    }
		    });
	}
	if (scenario.creatureTallies.has_value() && Locator::playerSystem::has_value())
	{
		const auto creature = Locator::playerSystem::value().GetPrimaryCreature(PlayerNames::PLAYER_ONE);
		if (creature.has_value() && registry.Valid(*creature))
		{
			const auto& wanted = *scenario.creatureTallies;
			auto& tallies = registry.AssignOrReplace<ecs::components::CreatureTallies>(*creature);
			tallies.meals.peopleKilled = wanted.peopleKilled;
			tallies.meals.animalsKilled = wanted.animalsKilled;
			tallies.meals.creaturesKilled = wanted.creaturesKilled;
			tallies.poos = wanted.poos;
			tallies.mushroomsEaten = wanted.mushroomsEaten;
		}
	}
}

void Runner::SetUpWorship(float share)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(k_ScenarioTownId);
	if (found == towns.end() || !registry.Valid(found->second) || !Locator::worshipSiteSystem::has_value())
	{
		return;
	}
	const auto town = found->second;
	const auto site = registry.Get<const ecs::components::Town>(town).worshipSite;
	if (!registry.Valid(site))
	{
		return;
	}
	// Put up at once, as a land's script puts down a site already built, then the totem raised to the share
	Locator::worshipSiteSystem::value().BuildBy(site, 1.0f);
	if (Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().SetTownShare(town, share);
	}
}

void Runner::PlaceBirds(const Scenario& scenario, glm::vec2 middle)
{
	const auto& land = Locator::terrainSystem::value();
	for (const auto& setup : scenario.temples)
	{
		const auto point = MapPoint(middle, setup.offset);
		const auto temple = ecs::archetypes::CitadelArchetype::Create({point.x, land.GetHeightAt(point), point.y}, setup.owner,
		                                                              setup.angle, glm::vec3(1.0f));
		if (setup.built.has_value())
		{
			ecs::construction::SetBuilt(temple, *setup.built);
		}
		if (setup.life.has_value())
		{
			Locator::entitiesRegistry::value().AssignOrReplace<ecs::components::ObjectLife>(temple, *setup.life);
		}
		_temples.push_back(temple);
	}
	if (!Locator::animalSystem::has_value())
	{
		return;
	}
	// As a land script makes them: the flock numbered, then its birds spread about its home
	auto& animals = Locator::animalSystem::value();
	constexpr float k_Spread = 8.0f;
	for (size_t i = 0; i < scenario.birdFlocks.size(); ++i)
	{
		const auto& setup = scenario.birdFlocks[i];
		const auto home = MapPoint(middle, setup.offset);
		const auto flock = animals.CreateScriptFlock(static_cast<int32_t>(i + 1), home, home, setup.reach, setup.flockDistance);
		_birdFlocks.push_back(flock);
		for (uint32_t bird = 0; bird < setup.count; ++bird)
		{
			const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(bird) / static_cast<float>(setup.count);
			const glm::vec2 at = home + glm::vec2(portable_math::Cos(angle), portable_math::Sin(angle)) * k_Spread;
			animals.CreateBird(setup.kind, at, 0, flock);
		}
	}
}

void Runner::PlaceCreatures(const Scenario& scenario, glm::vec2 middle)
{
	const auto& land = Locator::terrainSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& setup : scenario.creatures)
	{
		const auto point = MapPoint(middle, setup.offset);
		const glm::vec3 position {point.x, land.GetHeightAt(point), point.y};
		auto body = CreatureArchetype::StartBody(setup.species);
		body.alignment = setup.alignment.value_or(body.alignment);
		body.fatness = setup.fatness.value_or(body.fatness);
		body.strength = setup.strength.value_or(body.strength);
		const auto size = setup.size.value_or(k_DefaultSize);
		const auto entity =
		    CreatureArchetype::Create(position, setup.owner, setup.species, 0, glm::radians(setup.facingDegrees), size, body);
		if (auto* mind = registry.TryGet<CreatureMindState>(entity))
		{
			mind->paused = setup.pauseMind;
			if (setup.phase.has_value())
			{
				mind->developmentPhase = *setup.phase;
			}
		}
		if (!setup.mindFile.empty())
		{
			LoadMindFile(entity, setup.mindFile);
		}
		// The value of a mind file, as the body takes it when it loads one
		if (setup.environmentMap != 0)
		{
			if (auto* creature = registry.TryGet<ecs::components::Creature>(entity))
			{
				creature->environmentMap = setup.environmentMap;
			}
		}
		if (Locator::creatureMindSystem::has_value())
		{
			for (const auto skill : setup.skills)
			{
				Locator::creatureMindSystem::value().SetKnowsAction(entity, CreatureActionLearningType::Normal,
				                                                    static_cast<uint32_t>(skill), true);
			}
		}
		if (Locator::leashSystem::has_value())
		{
			// Made for trying things out, it knows every leash, as creatures made by the original's debug tools do
			auto& leashes = Locator::leashSystem::value();
			for (const auto type : creature_leash::k_Types)
			{
				leashes.SetKnown(entity, type, true);
			}
			if (setup.leashable.has_value())
			{
				leashes.SetLeashable(entity, *setup.leashable);
			}
		}
		if (Locator::creatureSkinSystem::has_value())
		{
			auto& skins = Locator::creatureSkinSystem::value();
			// The tattoos go on as the end of a drag in the tattoo editor puts them on
			if (!setup.tattoos.empty())
			{
				creature_tattoo::Slots slots {};
				std::copy_n(setup.tattoos.begin(), std::min(setup.tattoos.size(), slots.size()), slots.begin());
				const auto pictures = Locator::tattooEditorSystem::has_value()
				                          ? Locator::tattooEditorSystem::value().GetPictures()
				                          : creature_tattoo::Pictures {};
				skins.PutOnTattoos(entity, slots, pictures);
			}
			for (const auto& wound : setup.wounds)
			{
				skins.AddWound(entity, wound);
			}
			for (const auto& drop : setup.blood)
			{
				skins.AddBlood(entity, drop);
			}
		}
		_creatures.push_back(entity);
		_started.push_back(false);
	}
}

std::optional<entt::entity> Runner::PieceNearest(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* from = registry.TryGet<const ecs::components::Transform>(creature);
	if (from == nullptr)
	{
		return std::nullopt;
	}
	std::optional<entt::entity> nearest;
	float best = std::numeric_limits<float>::max();
	registry.Each<const ecs::components::BuildingPiece, const ecs::components::Transform>(
	    [&](entt::entity piece, const ecs::components::BuildingPiece& /*part*/, const ecs::components::Transform& at) {
		    const float distance = glm::distance(glm::xz(at.position), glm::xz(from->position));
		    if (distance < best)
		    {
			    best = distance;
			    nearest = piece;
		    }
	    });
	return nearest;
}

void Runner::BuildWorshipSiteOf(entt::entity centre) const
{
	// The scenario does not wait for the villagers to build the worship site: it stands built at once
	auto& registry = Locator::entitiesRegistry::value();
	const auto* abode = registry.TryGet<const ecs::components::Abode>(centre);
	const auto& towns = registry.Context().towns;
	const auto found = abode != nullptr ? towns.find(abode->townId) : towns.end();
	const auto* town = found != towns.end() && registry.Valid(found->second)
	                       ? registry.TryGet<const ecs::components::Town>(found->second)
	                       : nullptr;
	if (town != nullptr && registry.Valid(town->worshipSite))
	{
		ecs::construction::BuildBy(town->worshipSite, k_BuildWholeSite);
	}
}

void Runner::UpdateVillagerWalks()
{
	auto& registry = Locator::entitiesRegistry::value();
	_walks.resize(_objects.size());
	for (size_t i = 0; i < _objects.size() && i < _scenario->objects.size(); ++i)
	{
		const auto& setup = _scenario->objects.at(i);
		auto& walk = _walks.at(i);
		const auto entity = _objects.at(i);
		if (!registry.Valid(entity) || !registry.AllOf<ecs::components::LivingAction, ecs::components::WallHug>(entity))
		{
			continue;
		}
		if (setup.walkTo.has_value())
		{
			if (!walk.nextAt.has_value() && walk.outwards)
			{
				walk.nextAt = setup.walkAfterSeconds;
			}
			if (walk.nextAt.has_value() && _seconds >= *walk.nextAt)
			{
				const auto goal = MapPoint(_middle, walk.outwards ? *setup.walkTo : setup.offset);
				ecs::villager_home::SetupMoveTo(registry.Get<ecs::components::LivingAction>(entity), goal, setup.walkFinal);
				Log(fmt::format("Villager {} walks {}", i, walk.outwards ? "out" : "back"));
				walk.goal = goal;
				walk.setOffTurn = Locator::time::has_value() ? std::optional(Locator::time::value().GetTurn()) : std::nullopt;
				walk.outwards = !walk.outwards;
				walk.nextAt =
				    setup.walkRepeatSeconds.has_value() ? std::optional(*walk.nextAt + *setup.walkRepeatSeconds) : std::nullopt;
				if (!setup.walkRepeatSeconds.has_value())
				{
					walk.outwards = false;
				}
			}
		}
		// How long each walk took, in game turns, for comparing walks round things in the way
		if (walk.setOffTurn.has_value() && Locator::time::has_value() && ecs::villager_home::WalkIsOverAt(entity, walk.goal))
		{
			Log(fmt::format("Villager {} got there in {} turns", i, Locator::time::value().GetTurn() - *walk.setOffTurn));
			walk.setOffTurn.reset();
		}
		if (setup.mateWith.has_value() && !walk.dropped && _seconds >= setup.walkAfterSeconds &&
		    *setup.mateWith < _objects.size() && registry.Valid(_objects.at(*setup.mateWith)))
		{
			walk.dropped = true;
			ecs::villager_breeding::GoAndHaveSexWith(entity, _objects.at(*setup.mateWith));
			Log(fmt::format("Villager {} goes to make love with object {}", i, *setup.mateWith));
		}
		if (setup.worshipAt.has_value() && !walk.dropped && _seconds >= setup.walkAfterSeconds &&
		    Locator::teleportSystem::has_value() && Locator::terrainSystem::has_value())
		{
			walk.dropped = true;
			// A villager goes through its own player's stones only: one without a town is given one of the player's
			auto& villager = registry.Get<ecs::components::Villager>(entity);
			if (villager.town == entt::null || !registry.Valid(villager.town))
			{
				villager.town = registry.Context().towns.at(ScenarioTown(_middle));
			}
			const auto site = MapPoint(_middle, *setup.worshipAt);
			const glm::vec3 point {site.x, Locator::terrainSystem::value().GetHeightAt(site), site.y};
			const bool routed = Locator::teleportSystem::value().RouteWorshipper(entity, point);
			Log(fmt::format("Villager {} sets off to worship: {}", i, routed ? "through a stone" : "no stone"));
		}
		if (setup.dropOnStoneSeconds.has_value() && !walk.dropped && _seconds >= *setup.dropOnStoneSeconds &&
		    Locator::teleportSystem::has_value())
		{
			walk.dropped = true;
			// The stone nearest the villager
			const auto here = registry.Get<const ecs::components::Transform>(entity).position;
			// A villager jumps from its own player's stones only: one without a town is given one of the player's
			auto& villager = registry.Get<ecs::components::Villager>(entity);
			if (villager.town == entt::null || !registry.Valid(villager.town))
			{
				villager.town = registry.Context().towns.at(ScenarioTown(_middle));
			}
			std::optional<entt::entity> nearest;
			float best = std::numeric_limits<float>::max();
			registry.Each<const ecs::components::TeleportStone, const ecs::components::Transform>(
			    [&](entt::entity stone, const auto&, const ecs::components::Transform& transform) {
				    const float d = glm::distance(transform.position, here);
				    if (d < best)
				    {
					    best = d;
					    nearest = stone;
				    }
			    });
			// The testbed's hand is the first player's
			const bool jumped =
			    nearest.has_value() && Locator::teleportSystem::value().DropOnStone(entity, *nearest, PlayerNames::PLAYER_ONE);
			Log(fmt::format("Villager {} dropped on a stone: {}", i, jumped ? "jumped" : "stayed"));
		}
	}
}
