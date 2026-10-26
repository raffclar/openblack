/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarioRunner.h"

#include <cmath>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <numbers>
#include <ranges>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include <LHVM.h>
#include <MindFile.h>
#include <SDL_events.h>
#include <SDL_keycode.h>
#include <SDL_mouse.h>
#include <bgfx/bgfx.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/FlatLand.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ScreenPick.h"
#include "3D/TempleInteriorInterface.h"
#include "Animals/BirdRules.h"
#include "Animals/GrazerRules.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/CustomMusicPlayerInterface.h"
#include "Audio/GameMusic.h"
#include "Audio/HeaderOverrides.h"
#include "Audio/MusicMood.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Camera/DoubleClickFlight.h"
#include "Commands/PlayerCommands.h"
#include "Common/FileDialog.h"
#include "Common/MachineClock.h"
#include "Common/PortableMath.h"
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
#include "Debug/FightAutopilotInterface.h"
#include "Debug/TestbedMultiplayer.h"
#include "Debug/TownMiracles.h"
#include "DebugGuiInterface.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/DeadTreeArchetype.h"
#include "ECS/Archetypes/DrinkWaypointArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/FieldArchetype.h"
#include "ECS/Archetypes/FishFarmArchetype.h"
#include "ECS/Archetypes/FlowersArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLeashRope.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PrayerPower.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
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
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/CreatureReactionNotice.h"
#include "ECS/Dances.h"
#include "ECS/Map.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerBreeding.h"
#include "ECS/Systems/Implementations/VillagerCreatureReactions.h"
#include "ECS/Systems/Implementations/VillagerDance.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/InspectorLoading.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TattooMenuSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/WorldObjects.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Gestures/GesturePaths.h"
#include "Gui/GameInterface.h"
#include "Gui/OnlineMapTexts.h"
#include "Gui/TextDatabase.h"
#include "Help/HelpProfileFile.h"
#include "Help/Spirits.h"
#include "Help/VillageVoices.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Input/InjectedInput.h"
#include "Input/PointerSettle.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/ScriptCast.h"
#include "Magic/SpellRules.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptInterfaceLevels.h"
#include "TestbedDispenserGrid.h"
#include "Windowing/WindowingInterface.h"

#include "../Profiler.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using openblack::ecs::archetypes::CreatureArchetype;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureAnimation;
using openblack::ecs::components::CreatureHandGlows;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureNeeds;
using openblack::ecs::components::Transform;

namespace
{
/// The type a script sees for a value the testbed gives it
lhvm::DataType ScriptDataTypeOf(ScriptArgument::Kind kind)
{
	switch (kind)
	{
	case ScriptArgument::Kind::Number:
		return lhvm::DataType::Float;
	case ScriptArgument::Kind::Integer:
		return lhvm::DataType::Int;
	default:
		return lhvm::DataType::Object;
	}
}

using Kind = Command::Kind;
using Pace = ecs::systems::CreatureLocomotionSystemInterface::Pace;

/// The lines of the log of commands kept
constexpr size_t k_LogLines = 8;
/// A creature follows another this far behind, for a creature of size 1
constexpr float k_FollowDistance = 25.0f;
/// Forced weather lasts as long as anyone watches
constexpr float k_WeatherSeconds = 3600.0f;
/// The size of a creature whose scenario doesn't give one
constexpr float k_DefaultSize = 1.0f;
/// The least the overview takes in either way of the middle of what it frames
constexpr float k_MinOverviewHalfSize = 25.0f;
/// The live results of a benchmark are summed up again every so many frames
constexpr uint32_t k_FramesPerSummary = 30;
/// What the store of a crowd's town starts with
constexpr uint32_t k_CrowdFood = 2000;
/// The town a scenario's buildings and fields belong to, its id kept clear of the crowds' towns, and what its buildings
/// hold
constexpr uint32_t k_ScenarioTownId = 900;
constexpr uint32_t k_ScenarioTownFood = 500;
/// How high above its hand another god reports its camera, in metres
constexpr float k_OtherGodCameraAbove = 30.0f;
constexpr uint32_t k_ScenarioTownWood = 500;

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
constexpr uint32_t k_CrowdWood = 2000;
/// Enough building to finish a worship site at once
constexpr float k_BuildWholeSite = 1.0e6f;
/// The prayer power the testbed's player has when a scenario doesn't say, as the testbed starts with
constexpr float k_PlentyOfPrayer = 1.0e6f;

/// The profiler's stages as benchmarks name them: the drawing of the reflection and of the main pass told apart, and
/// everything from drawing the scene on counted as rendering
std::vector<benchmark::StageInfo> BenchmarkStages()
{
	static const std::vector<std::string> k_Names = [] {
		std::vector<std::string> names;
		for (size_t i = 0; i < Profiler::k_StageNames.size(); ++i)
		{
			const auto stage = static_cast<Profiler::Stage>(i);
			std::string name(Profiler::k_StageNames.at(i));
			if (stage > Profiler::Stage::ReflectionPass && stage <= Profiler::Stage::ReflectionDrawParticles)
			{
				name = "Reflection " + name;
			}
			else if (stage > Profiler::Stage::MainPass && stage <= Profiler::Stage::MainPassDrawParticles)
			{
				name = "Main " + name;
			}
			names.push_back(std::move(name));
		}
		return names;
	}();
	std::vector<benchmark::StageInfo> stages;
	for (size_t i = 0; i < k_Names.size(); ++i)
	{
		stages.push_back({.name = k_Names.at(i), .render = static_cast<Profiler::Stage>(i) >= Profiler::Stage::SceneDraw});
	}
	return stages;
}

double Milliseconds(std::chrono::system_clock::duration duration)
{
	return std::chrono::duration<double, std::milli>(duration).count();
}

#ifdef NDEBUG
constexpr std::string_view k_Build = "optimised";
#else
constexpr std::string_view k_Build = "debug";
#endif

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

std::string_view MoveResultName(ecs::systems::CreatureLocomotionSystemInterface::MoveResult result)
{
	using MoveResult = ecs::systems::CreatureLocomotionSystemInterface::MoveResult;
	switch (result)
	{
	case MoveResult::InvalidDestination:
		return "nowhere to stand there";
	case MoveResult::Busy:
		return "it can't walk";
	case MoveResult::Started:
	default:
		return "on its way";
	}
}

/// Where a creature faces on the land; its mesh looks back along +z
glm::vec2 AheadOf(const Transform& transform)
{
	const auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	return {ahead.x, ahead.z};
}

/// The debug windows leave the mouse alone while a scenario drives it, so that the real pointer resting on one of them
/// doesn't take the scenario's presses
void KeepDebugWindowsOffTheMouse(bool off)
{
	if (ImGui::GetCurrentContext() == nullptr)
	{
		return;
	}
	auto& io = ImGui::GetIO();
	io.ConfigFlags = off ? (io.ConfigFlags | ImGuiConfigFlags_NoMouse) : (io.ConfigFlags & ~ImGuiConfigFlags_NoMouse);
}
/// Whether a command sends its creature somewhere, or has it face, throw or point somewhere
bool HasPoint(Kind kind)
{
	return kind == Kind::WalkTo || kind == Kind::RunTo || kind == Kind::FleeFrom || kind == Kind::TurnToFace ||
	       kind == Kind::ThrowAt || kind == Kind::PointAt || kind == Kind::TurnToFaceWhenStill;
}

std::string_view Started(bool started)
{
	return started ? "started" : "can't";
}

/// The camera's fields of view across and up and down, in radians
glm::vec2 FieldsOfView(const Camera& camera)
{
	return {camera.GetHorizontalFieldOfView(), camera.GetVerticalFieldOfView()};
}

/// Why the leash was refused, for the readout
std::string RefusedText(const ecs::systems::LeashSystemInterface& leashes)
{
	const auto refused = leashes.LastRefusal();
	return refused.has_value() ? fmt::format("refused: {}", creature_leash::Describe(refused->why)) : "can't";
}

/// A miracle cast by hand: the button goes down this long after the seed is in the hand, so that the hand is over its
/// point first, and a thrown one is let go this long after
constexpr float k_HandPressWait = 0.2f;
constexpr float k_HandThrowSeconds = 0.4f;
/// The hand looks at its point from this high over itself, and at a creature this high over its feet
constexpr float k_HandEyeHeight = 20.0f;
constexpr float k_HandAimHeight = 2.0f;

std::string_view HandResultName(ecs::systems::MagicSystemInterface::HandResult result)
{
	using HandResult = ecs::systems::MagicSystemInterface::HandResult;
	switch (result)
	{
	case HandResult::None:
		return "nothing";
	case HandResult::TookMiracle:
		return "took a miracle";
	case HandResult::Readied:
		return "armed";
	case HandResult::Cast:
		return "cast";
	case HandResult::CastHeld:
		return "cast, held";
	case HandResult::NotReady:
		return "not ready";
	case HandResult::CantCastThere:
		return "can't cast there";
	case HandResult::Released:
		return "let go";
	case HandResult::Discarded:
		return "dropped";
	case HandResult::NoCircle:
		return "no circle drawn";
	case HandResult::PoweredUp:
		return "powered up";
	case HandResult::Stored:
		return "stored";
	}
	return "";
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
	if (object.inScript && registry.AllOf<ecs::components::Villager>(entity) && Locator::livingActionSystem::has_value())
	{
		Locator::livingActionSystem::value().VillagerSetScriptState(entity, VillagerStates::InScript);
	}
	if (object.scriptControlled && registry.AllOf<ecs::components::Villager>(entity))
	{
		registry.AssignOrReplace<ecs::components::ScriptControlled>(entity);
	}
	if (object.disciple != VillagerDisciple::None && registry.AllOf<ecs::components::Villager>(entity))
	{
		ecs::villager_disciple::SetType(entity, object.disciple);
	}
}

/// The player has grey rings of their own for a design, which the tattoo editor puts in the creature's block of
/// pictures when a tattoo is dropped
std::string GiveOwnTattooPicture(size_t design)
{
	if (!Locator::tattooEditorSystem::has_value() || design >= creature_tattoo::k_DesignCount)
	{
		return "no tattoo editor or no such design";
	}
	creature_tattoo::Picture rings {};
	constexpr int k_Middle = static_cast<int>(creature_tattoo::k_DesignSize) / 2;
	for (int y = 0; y < static_cast<int>(creature_tattoo::k_DesignSize); ++y)
	{
		for (int x = 0; x < static_cast<int>(creature_tattoo::k_DesignSize); ++x)
		{
			const auto distance = static_cast<int>(
			    std::sqrt(static_cast<float>(((x - k_Middle) * (x - k_Middle)) + ((y - k_Middle) * (y - k_Middle)))));
			const auto index = static_cast<size_t>((y * static_cast<int>(creature_tattoo::k_DesignSize)) + x);
			rings.at(index) = distance < k_Middle && (distance / 6) % 2 == 0 ? uint8_t {255} : uint8_t {0};
		}
	}
	auto& editor = Locator::tattooEditorSystem::value();
	auto pictures = editor.GetPictures();
	pictures.at(design) = rings;
	editor.SetPictures(pictures);
	return fmt::format("design {} has the player's own picture", design + 1);
}

/// The game's menu opens on its Players page, and the tattoo editor over it as the page's Edit Tattoo button opens it
std::string OpenTattooMenu()
{
	auto* game = Game::Instance();
	if (game == nullptr || game->GetInterface() == nullptr || !Locator::tattooMenuSystem::has_value())
	{
		return "failed: no menu";
	}
	auto& interface = *game->GetInterface();
	interface.OpenMenu();
	interface.GetMenu().ShowPage(gui::GameMenu::Page::Players);
	Locator::tattooMenuSystem::value().Open();
	return Locator::tattooMenuSystem::value().IsOpen() ? "the tattoo editor shows its creature over the menu"
	                                                   : "failed: the editor did not open";
}

/// The game's menu opens on its Controls page, picks a row and double clicks it, as the player does to bind it anew
std::string OpenControlsBox(size_t row)
{
	auto* game = Game::Instance();
	if (game == nullptr || game->GetInterface() == nullptr)
	{
		return "failed: no menu";
	}
	auto& interface = *game->GetInterface();
	interface.OpenMenu();
	auto& menu = interface.GetMenu();
	menu.ShowPage(gui::GameMenu::Page::Controls);
	const auto& list = menu.GetControls();
	for (int y = list.rect.min.y; y < list.rect.max.y; ++y)
	{
		if (list.ItemAt(y) == row)
		{
			const glm::ivec2 point {list.rect.Centre().x, y + 4};
			menu.MouseDown(point);
			menu.MouseUp(point);
			menu.DoubleClick(point);
			break;
		}
	}
	return menu.IsCapturing() ? fmt::format("the box asks for the new input of row {}", row) : "failed: no box";
}

/// The multiplayer front end opens over the temple's future room with the demonstration lobby, on its way to a stage
std::string OpenMultiplayerBox(size_t stage)
{
	auto* game = Game::Instance();
	if (game == nullptr || game->GetInterface() == nullptr || stage >= static_cast<size_t>(testbed_multiplayer::Stage::_Count))
	{
		return "failed: no interface";
	}
	auto maps = gui::ReadOnlineMapTexts();
	auto frontEnd = testbed_multiplayer::MakeDemoFrontEnd(static_cast<testbed_multiplayer::Stage>(stage), u"Alice",
	                                                      std::move(maps.localMaps), std::move(maps.conditionTemplate));
	if (!game->OpenMultiplayerFrontEnd(std::move(frontEnd)))
	{
		return "failed: no temple";
	}
	return fmt::format("the front end goes to stage {}", stage);
}

/// The game's menu opens on a page. Without sound, the menu shows what a game with no sound shows.
std::string OpenMenuPage(size_t page, bool noSound)
{
	auto* game = Game::Instance();
	if (game == nullptr || game->GetInterface() == nullptr || page >= static_cast<size_t>(gui::GameMenu::Page::_Count))
	{
		return "failed: no menu";
	}
	auto& interface = *game->GetInterface();
	interface.OpenMenu();
	auto& menu = interface.GetMenu();
	if (noSound)
	{
		menu.SetSoundInstalled(false);
	}
	menu.ShowPage(static_cast<gui::GameMenu::Page>(page));
	return fmt::format("the menu shows page {}{}", page, noSound ? " as with no sound" : "");
}

/// A key pressed in the game's menu, as the keyboard gives it
std::string PressMenuKey(size_t key, bool shift)
{
	auto* game = Game::Instance();
	if (game == nullptr || game->GetInterface() == nullptr || !game->GetInterface()->GetMenu().IsOpen())
	{
		return "failed: the menu is not open";
	}
	auto& menu = game->GetInterface()->GetMenu();
	const auto action = static_cast<int>(key) == SDLK_ESCAPE ? menu.Escape() : menu.KeyDown(static_cast<int>(key), shift);
	if (action == gui::GameMenu::Action::Continue)
	{
		menu.Close();
	}
	return fmt::format("key {}{} pressed", key, shift ? " with Shift" : "");
}

/// The game writes the help record of the player to their folder in the profile folder, then reads it again
std::string CheckProfileFolder()
{
	if (!Locator::profileFolderSystem::has_value() || !Locator::helpProfileSystem::has_value() ||
	    !Locator::playerProfileSystem::has_value())
	{
		return "failed: no profile folder";
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value())
	{
		return "failed: no player profile";
	}
	const auto player = gui::ToUtf16(profile->name);
	auto& folder = Locator::profileFolderSystem::value();
	auto& record = Locator::helpProfileSystem::value();
	const auto written = help::profile::ToFileBytes(record.Get());
	if (!record.SaveRecord(folder, player))
	{
		return "failed: the game could not write the help record";
	}
	const auto read = folder.ReadPlayerFile(player, help::profile::k_FileName);
	const auto path = (*folder.GetPlayerFolder(player) / help::profile::k_FileName).generic_string();
	if (!read.has_value() || *read != written)
	{
		return fmt::format("failed: {} does not hold the help record", path);
	}
	record.LoadRecord(folder, player);
	if (help::profile::ToFileBytes(record.Get()) != written)
	{
		return "failed: the help record changed when the game read it again";
	}
	return fmt::format("passed: {} holds the help record ({} bytes) and reads back the same", path, read->size());
}

/// The custom music player plays a track of the custom music folder, or stops; the music moods look at once
std::string PlayCustomMusic(size_t track)
{
	auto* player = Locator::audio::has_value() ? Locator::audio::value().GetCustomMusic() : nullptr;
	if (player == nullptr || !Locator::musicMoodSystem::has_value())
	{
		return "failed: no custom music player";
	}
	auto& moods = Locator::musicMoodSystem::value();
	if (track == k_StopCustomMusic)
	{
		player->Stop();
		moods.UpdateOnGameTurn(true);
		return "the custom music stops";
	}
	moods.LoadFolder();
	if (track >= player->GetTracks().size())
	{
		return fmt::format("failed: the custom music folder has {} tracks", player->GetTracks().size());
	}
	player->SetCurrentTrack(track);
	player->Play();
	const auto id = music_mood::TrackId(player->GetSongName());
	const auto records = moods.GetRecords();
	const auto record = std::ranges::find(records, id, &music_mood::Record::id);
	const auto mood = record != records.end() && !record->emotions.empty() ? record->emotions.front().mood : 0.0f;
	moods.UpdateOnGameTurn(true);
	return fmt::format("plays {} (track {:08x}), the creature's mood for it {:.3f}", player->GetSongName(), id, mood);
}
} // namespace

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
	_fireBallTakeFrames.reset();
	_nextMiracleLog = 0.0f;
	_timeline = {};
	_creatures.clear();
	_objects.clear();
	_globes.clear();
	_temples.clear();
	_throwsDue.clear();
	_turnsAsked.clear();
	for (const auto& setup : scenario.throws)
	{
		_throwsDue.emplace_back(setup.delaySeconds);
	}
	_walks.clear();
	_particles.clear();
	_miracles.clear();
	_started.clear();
	_log.clear();
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
			_crowdCreatures = LayOutCreatures(crowd.count, crowd.seed);
			_crowdProgress.total = _crowdCreatures.size();
		}
		else
		{
			_village = LayOutVillagers(crowd.count, crowd.seed);
			_crowdProgress.total = _village.towns.size() + _village.abodes.size() + _village.villagers.size();
		}
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
	PlaceObjects(scenario, _middle);
	for (const auto& offset : scenario.sharks)
	{
		const auto at = MapPoint(_middle, offset);
		ecs::archetypes::SharkArchetype::Create({at.x, 0.0f, at.y}, 1.0f);
	}
	PlaceHighlights(scenario, _middle);
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
	PlaceBirds(scenario, _middle);
	PlaceCreatures(scenario, _middle);
	PlaceDispensers(scenario);
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
	if (scenario.townOwner.has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		if (const auto town = towns.find(k_ScenarioTownId); town != towns.end() && registry.Valid(town->second))
		{
			registry.Get<ecs::components::Town>(town->second).owner = *scenario.townOwner;
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
		Locator::magicSystem::value().DriveHand(ecs::systems::MagicSystemInterface::HandFrame {
		    .handPosition = hand,
		    .point = point,
		    .rayOrigin = hand + glm::vec3(0.0f, k_HandEyeHeight, 0.0f),
		    .rayDirection = glm::vec3(0.0f, -1.0f, 0.0f),
		    .cameraForward = glm::vec3(0.0f, -1.0f, 0.0f),
		    .overWorld = true,
		});
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
	Log(fmt::format("Started {}", scenario.name));
}

void Runner::Stop()
{
	if (!_running)
	{
		return;
	}
	_running = false;
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
	if (Locator::particleSystem::has_value())
	{
		for (const auto& particle : _particles)
		{
			Locator::particleSystem::value().CloseDown(particle.effect);
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
	if (Locator::skySystem::has_value())
	{
		Locator::skySystem::value().GetClock().SetRunning(true);
	}
	Log("Stopped");
}

void Runner::SetUpEnvironment(const Environment& environment)
{
	if (Locator::skySystem::has_value())
	{
		auto& sky = Locator::skySystem::value();
		sky.SetTime(environment.hour);
		sky.GetClock().SetRunning(environment.clockRuns);
	}
	if (Locator::weatherSystem::has_value())
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
	if (Locator::alignmentSystem::has_value())
	{
		Locator::alignmentSystem::value().SetPlayerAlignment(PlayerNames::PLAYER_ONE,
		                                                     environment.playerAlignment.value_or(0.0f));
	}
	// The player's prayer power: as much as the scenario gives, or plenty
	if (Locator::entitiesRegistry::has_value())
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
	if (environment.pushScrolling && Locator::config::has_value())
	{
		Locator::config::value().pushScrolling = true;
		Locator::config::value().testbedPushScrollInWindow = true;
	}
	if (environment.profileSymbol.has_value())
	{
		if (auto* game = Game::Instance(); game != nullptr)
		{
			game->MakePlayersSymbols(environment.profileSymbol);
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
	if (environment.landNumber.has_value() && Locator::entitiesRegistry::has_value())
	{
		auto& landNumber = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
		if (!_landNumberBefore.has_value())
		{
			_landNumberBefore = landNumber;
		}
		landNumber = *environment.landNumber;
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
		Game::Instance()->SetMousePosition(glm::ivec2(*environment.cursor * size));
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
				    if (object.joinTown)
				    {
					    auto& registry = Locator::entitiesRegistry::value();
					    registry.Get<ecs::components::Villager>(villager).town =
					        registry.Context().towns.at(ScenarioTown(middle));
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
					    const auto plan = ecs::archetypes::AbodeArchetype::CreatePlan(ScenarioTown(middle), position, type, yaw,
					                                                                  object.scale);
					    if (auto* planned = Locator::entitiesRegistry::value().TryGet<ecs::components::PlannedAbode>(plan))
					    {
						    planned->wasBuilt = object.wasBuilt;
					    }
					    return plan;
				    }
				    const auto abode = ecs::archetypes::AbodeArchetype::Create(
				        ScenarioTown(middle), position, type, yaw, object.scale, object.storedFood.value_or(k_ScenarioTownFood),
				        object.storedWood.value_or(k_ScenarioTownWood));
				    if (object.built.has_value())
				    {
					    ecs::construction::SetBuilt(abode, *object.built);
				    }
				    Locator::entitiesRegistry::value().Get<ecs::components::Abode>(abode).neglect = object.neglect;
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
			    else
			    {
				    return ecs::archetypes::FeatureArchetype::Create(position, type, yaw, object.scale);
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

void Runner::PointAtBody(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const ecs::components::Transform>(creature);
	const auto* mesh = registry.TryGet<const ecs::components::Mesh>(creature);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (transform == nullptr || mesh == nullptr || !meshes.Contains(mesh->id) || !Locator::windowing::has_value())
	{
		return;
	}
	// The middle of the body's bounding box, as it stands now
	const auto model = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	const auto middle = glm::vec3(model * glm::vec4(meshes.Handle(mesh->id)->GetBoundingBox().Center(), 1.0f));
	const auto size = glm::vec2(Locator::windowing::value().GetSize());
	glm::vec3 pixel;
	if (Locator::camera::value().ProjectWorldToScreen(middle, {0.0f, 0.0f, size.x, size.y}, pixel))
	{
		Game::Instance()->SetMousePosition(glm::ivec2(glm::round(glm::vec2(pixel))));
	}
}

std::optional<entt::entity> Runner::CreatureAt(size_t index) const
{
	if (index >= _creatures.size() || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto entity = _creatures[index];
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

std::string Runner::GiveObjectCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		return "no hands";
	}
	auto& hands = Locator::creatureObjectActionSystem::value();
	const auto& land = Locator::terrainSystem::value();
	const auto point = MapPoint(_middle, command.point);
	const glm::vec3 onLand {point.x, land.GetHeightAt(point), point.y};
	const auto object = command.globe.has_value() ? GlobeAt(*command.globe) : ObjectAt(command.object);
	switch (command.kind)
	{
	case Kind::PickUp:
		return object.has_value() ? std::string(Started(hands.PickUp(creature, *object))) : "it is gone";
	case Kind::PutDown:
		return std::string(Started(hands.PutDown(creature)));
	case Kind::Discard:
		return std::string(Started(hands.Discard(creature)));
	case Kind::EatHeld:
		return std::string(Started(hands.EatHeld(creature)));
	case Kind::Examine:
		return std::string(Started(hands.Keep(creature, creature_object_actions::k_FirstKeepAnimation + command.value)));
	case Kind::ThrowAt:
		// At about the height of a creature's middle
		return std::string(Started(hands.Throw(creature, onLand + glm::vec3(0.0f, CreatureHeight(1.0f) * 0.5f, 0.0f))));
	case Kind::KnockDown:
		return object.has_value() ? std::string(Started(hands.Destroy(creature, *object))) : "it is gone";
	case Kind::PointAt:
		return std::string(Started(hands.PointAt(creature, onLand)));
	default:
		return {};
	}
}

std::string Runner::GiveHandCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureHandSystem::has_value())
	{
		return "no hand";
	}
	auto& hand = Locator::creatureHandSystem::value();
	switch (command.kind)
	{
	case Kind::HandStroke:
		return hand.Stroke(creature, static_cast<creature_feedback::BodyPart>(command.bodyPart)) ? "stroked" : "busy";
	case Kind::HandSlap:
		return hand.Slap(creature, command.slapHeight, command.gentle, command.sweepsRight) ? "slapped" : "busy";
	case Kind::HandLetGo:
		if (hand.GetCreature() == creature)
		{
			const auto sum = hand.GetFeedbackSum();
			hand.Release();
			return fmt::format("{:+.2f}", sum);
		}
		return "not held";
	default:
		return {};
	}
}

std::string Runner::GiveLeashCommand(entt::entity creature, const Command& command)
{
	if (!Locator::leashSystem::has_value())
	{
		return "no leashes";
	}
	auto& leashes = Locator::leashSystem::value();
	switch (command.kind)
	{
	case Kind::PutOnLeash:
		// It must know the learning leash before any, and the leash itself
		leashes.SetKnown(creature, LeashType::Rope, true);
		leashes.SetKnown(creature, command.leash, true);
		if (leashes.IsLeashed(creature))
		{
			return leashes.ChangeType(creature, command.leash) ? "changed" : "can't";
		}
		return leashes.PutOn(creature, command.leash) ? "on" : RefusedText(leashes);
	case Kind::MakeLeashable:
		return leashes.SetLeashable(creature, true) ? "leashable" : RefusedText(leashes);
	case Kind::HandTapLeash:
		// As the player's Action button tapping it does
		return leashes.TapCreature(command.player, creature) ? "on" : RefusedText(leashes);
	case Kind::LeashShake:
		// The game shakes a leash off with a scribble drawn with the empty hand
		return DrawGesture(GestureType::Scribble);
	case Kind::LeashKey:
	{
		// As the player pressing the key does, through the same call the controls make
		constexpr std::array k_Keys {creature_leash::LeashKey::Leash, creature_leash::LeashKey::PreviousLeash,
		                             creature_leash::LeashKey::NextLeash};
		const auto before = leashes.TypeOf(creature);
		if (!leashes.PressKey(command.player, k_Keys.at(command.value)))
		{
			return RefusedText(leashes);
		}
		const auto after = leashes.TypeOf(creature);
		return after == LeashType::None ? "off" : before == after ? "same" : creature_leash::Name(after);
	}
	case Kind::TieLeash:
		if (const auto object = ObjectAt(command.object))
		{
			return leashes.TieTo(creature, *object) ? "tied" : "can't";
		}
		return "it is gone";
	case Kind::LeashOrderAt:
	{
		const auto point = MapPoint(_middle, command.point);
		const glm::vec3 onLand {point.x, Locator::terrainSystem::value().GetHeightAt(point), point.y};
		return leashes.OrderAt(command.player, onLand) ? "taken" : "refused";
	}
	case Kind::LeashOrderOn:
		if (const auto object = ObjectAt(command.object))
		{
			return leashes.OrderOn(command.player, *object) ? "taken" : "refused";
		}
		return "it is gone";
	case Kind::HangLeashPosts:
	{
		constexpr float k_Apart = 6.0f;
		constexpr float k_Up = 5.0f;
		const auto middle = MapPoint(_middle, command.point);
		std::array<glm::vec3, 3> points {};
		for (size_t i = 0; i < points.size(); ++i)
		{
			const glm::vec2 at {middle.x + ((static_cast<float>(i) - 1.0f) * k_Apart), middle.y};
			points.at(i) = {at.x, Locator::terrainSystem::value().GetHeightAt(at) + k_Up, at.y};
		}
		leashes.PlacePosts(command.player, points);
		return "hung";
	}
	case Kind::TapLeashPost:
	{
		std::optional<entt::entity> post;
		Locator::entitiesRegistry::value().Each<const ecs::components::LeashPost>(
		    [&post, &command](entt::entity entity, const ecs::components::LeashPost& at) {
			    if (at.owner == command.player && at.type == creature_leash::k_Types.at(command.value))
			    {
				    post = entity;
			    }
		    });
		return post.has_value() && leashes.TapPost(*post) ? "tapped" : "no such leash";
	}
	case Kind::LeashTapBuilding:
	{
		const auto object = ObjectAt(command.object);
		if (!object.has_value())
		{
			return "it is gone";
		}
		if (leashes.TapThing(command.player, *object))
		{
			return "the leash takes the tap";
		}
		if (!Locator::abodeKnockSystem::has_value())
		{
			return "no knock";
		}
		const auto& at = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(*object).position;
		return Locator::abodeKnockSystem::value().Tap(*object, at, true) ? "knocked" : "not knocked";
	}
	case Kind::UntieLeash:
		leashes.UntieToHand(creature);
		return {};
	case Kind::TakeOffLeash:
		leashes.TakeOff(creature);
		return {};
	case Kind::ConfineToHome:
		leashes.ConfineToHome(creature, command.radius);
		return {};
	default:
		return {};
	}
}

std::string Runner::GivePlayerCommand(const Command& command)
{
	switch (command.kind)
	{
	case Kind::HoldSeed:
	{
		if (!Locator::magicSystem::has_value())
		{
			return "no miracles";
		}
		auto& magic = Locator::magicSystem::value();
		magic.DiscardHeldSeed();
		const auto seed = static_cast<SpellSeedType>(command.value);
		const auto given = magic.GiveSeedToHand(command.player, seed, magic::k_BasePowerUpLevel, 1.0f);
		return given != entt::null ? fmt::format("seed {} in the hand", command.value) : "the hand isn't free";
	}
	case Kind::DrawGesture:
		return DrawGesture(static_cast<GestureType>(command.value));
	case Kind::PressKey:
		if (!Locator::gameActionSystem::has_value())
		{
			return "no keys";
		}
		Locator::gameActionSystem::value().QueuePress(static_cast<input::BindableActionMap>(command.value));
		return {};
	case Kind::SummonSeed:
	{
		if (!Locator::magicSystem::has_value())
		{
			return "no miracles";
		}
		auto& magic = Locator::magicSystem::value();
		magic.DiscardHeldSeed();
		const auto seed = static_cast<SpellSeedType>(command.value);
		const auto given = magic.SummonSeed(command.player, seed, magic::k_BasePowerUpLevel);
		return given != entt::null ? fmt::format("seed {} summoned to the hand", command.value) : "the hand isn't free";
	}
	case Kind::HandTakeFireBall:
		return HandTakeFireBall();
	case Kind::HandTapObject:
		return HandTapObject(command.object);
	case Kind::ScriptRelease:
	{
		const auto object = ObjectAt(command.object);
		if (!object.has_value() || !Locator::scriptObjects::has_value())
		{
			return "nothing to release";
		}
		Locator::scriptObjects::value().ReleaseFromScript(*object);
		return Locator::entitiesRegistry::value().AllOf<ecs::components::ScriptControlled>(*object) ? "still controlled"
		                                                                                            : "released";
	}
	case Kind::PauseGame:
		Locator::time::value().SetPaused(command.value != 0);
		return command.value != 0 ? "paused" : "running";
	case Kind::SetGameSpeed:
		Locator::time::value().SetSpeed(command.amount);
		return fmt::format("speed {}", command.amount);
	case Kind::FinishBuilding:
	{
		const auto object = ObjectAt(command.object);
		if (!object.has_value())
		{
			return "nothing to finish";
		}
		ecs::construction::SetBuilt(*object, 1.0f);
		return "finished";
	}
	case Kind::RemoveObject:
	{
		const auto object = ObjectAt(command.object);
		if (!object.has_value())
		{
			return "nothing to remove";
		}
		ecs::world_objects::Remove(*object);
		return "removed";
	}
	case Kind::OtherGodHand:
	{
		if (!Locator::handSystem::has_value() || !Locator::terrainSystem::has_value())
		{
			return "no hands";
		}
		auto& hands = Locator::handSystem::value();
		if (command.player == hands.GetOwnPlayer())
		{
			return "the player is the god of this computer";
		}
		std::ignore = hands.MakeHandsOf(command.player);
		// The hand takes the thing before the first report, which asks the influence at no point
		std::string held = "the hand holds nothing";
		if (const auto object = ObjectAt(command.object); object.has_value() && Locator::handGrabSystem::has_value())
		{
			held = Locator::handGrabSystem::value().TakeIntoHandOf(command.player, *object) ? "the hand holds the thing"
			                                                                                : "the hand can't take the thing";
		}
		const auto at = MapPoint(_middle, command.point);
		const glm::vec3 ground {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
		const auto hand = ground + glm::vec3(0.0f, command.radius, 0.0f);
		hands.Report(command.player, {.collidePoint = ground,
		                              .handPoint = hand,
		                              .camera = hand + glm::vec3(0.0f, k_OtherGodCameraAbove, 0.0f),
		                              .focus = ground});
		return held;
	}
	case Kind::RemoveTeleportStone:
	{
		if (!Locator::teleportSystem::has_value())
		{
			return "no teleport";
		}
		auto& teleport = Locator::teleportSystem::value();
		const auto& registry = Locator::entitiesRegistry::value();
		const auto at = MapPoint(_middle, command.point);
		const auto stones = teleport.GetStones(PlayerNames::PLAYER_ONE);
		const auto nearest = std::ranges::min_element(stones, {}, [&registry, &at](entt::entity stone) {
			const auto& position = registry.Get<const ecs::components::Transform>(stone).position;
			return glm::distance(glm::vec2(position.x, position.z), at);
		});
		if (nearest == stones.end())
		{
			return "no stone";
		}
		teleport.RemoveStone(*nearest);
		return "removed";
	}
	case Kind::MakeDance:
	{
		const auto centre = ObjectAt(command.object);
		if (!centre.has_value() || !Locator::danceSystem::has_value())
		{
			return "no place";
		}
		auto& registry = Locator::entitiesRegistry::value();
		const auto& place = registry.Get<const ecs::components::Transform>(*centre).position;
		const auto dance =
		    Locator::danceSystem::value().Create(static_cast<DanceInfo>(command.value), place, *centre, 0, true, entt::null);
		if (dance == entt::null)
		{
			return "no dance";
		}
		uint32_t dancers = 0;
		for (const auto object : _objects)
		{
			if (object == *centre || !registry.Valid(object) || !registry.AllOf<ecs::components::Villager>(object))
			{
				continue;
			}
			// The villager joins the dance as a script's attach does
			if (ecs::dances::AddDancer(registry, dance, object, ecs::villager_dance::DanceSex(object)))
			{
				Locator::livingActionSystem::value().VillagerSetScriptState(object, VillagerStates::InDance);
				++dancers;
			}
		}
		if (command.ctrl)
		{
			registry.Get<ecs::components::Dance>(dance).cameraWanted = true;
		}
		return fmt::format("{} dancers{}", dancers, command.ctrl ? ", the dance wants its camera" : "");
	}
	case Kind::HandGripTotem:
	{
		const auto centre = ObjectAt(command.object);
		if (!centre.has_value() || !Locator::villageTotemSystem::has_value())
		{
			return "no totem";
		}
		std::optional<entt::entity> totem;
		Locator::entitiesRegistry::value().Each<const ecs::components::VillageTotem>(
		    [&totem, &centre](entt::entity entity, const ecs::components::VillageTotem& component) {
			    if (component.townCentre == *centre)
			    {
				    totem = entity;
			    }
		    });
		if (!totem.has_value())
		{
			return "no totem";
		}
		// The scenario does not wait for the villagers to build the worship site: it stands built at once
		{
			auto& registry = Locator::entitiesRegistry::value();
			const auto* abode = registry.TryGet<const ecs::components::Abode>(*centre);
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
		auto& totems = Locator::villageTotemSystem::value();
		if (!totems.Grip(*totem, command.player))
		{
			return fmt::format("the hand can't take hold of the totem: {}", totems.WhyNotGrip(*totem, command.player));
		}
		totems.LetGo();
		return "gripped and let go";
	}
	case Kind::HandTake:
	{
		const auto object = command.globe.has_value() ? GlobeAt(*command.globe) : ObjectAt(command.object);
		if (!object.has_value() || !Locator::handGrabSystem::has_value())
		{
			return "nothing to take";
		}
		auto& hand = Locator::handGrabSystem::value();
		const auto why = hand.WhyNotTake(*object);
		return hand.TakeIntoHand(*object) ? std::string("taken into the hand") : fmt::format("the hand can't take it: {}", why);
	}
	case Kind::AttachObjectScriptSound:
	{
		const auto object = ObjectAt(command.object);
		if (!object.has_value() || !Locator::soundTagSystem::has_value())
		{
			return "nothing to attach a sound to";
		}
		const auto sound = static_cast<entt::id_type>(command.value);
		const auto tag = Locator::soundTagSystem::value().AttachObjectSound(*object, sound, false);
		return fmt::format("sound {} attached to the object as tag {}", sound, entt::to_integral(tag));
	}
	case Kind::HandPour:
	{
		if (!Locator::resourceStoreSystem::has_value())
		{
			return "no stores";
		}
		const auto at = MapPoint(_middle, command.point);
		const glm::vec3 point {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
		const auto amount = static_cast<uint32_t>(command.value);
		// What a store or pile didn't take is left as a new loose pile
		auto& registry = Locator::entitiesRegistry::value();
		const auto loose = [&registry] { return registry.Size<ecs::components::MagicPile>(); };
		const auto before = loose();
		Locator::resourceStoreSystem::value().PourAt(ResourceType::Wood, point, amount, false, command.player, false);
		return fmt::format("{} wood poured at ({:.1f}, {:.1f}): {}", amount, at.x, at.y,
		                   loose() > before ? "left on the land as a new pile" : "taken in");
	}
	case Kind::SayLine:
	{
		if (!Locator::helpSpeechSystem::has_value())
		{
			return "no speech";
		}
		const auto text = static_cast<uint32_t>(command.value);
		if (!command.ctrl)
		{
			Locator::helpSpeechSystem::value().Say(text, audio::SpeechVoice::First, std::nullopt);
			return fmt::format("line of text {} said, heard everywhere", text);
		}
		const auto at = MapPoint(_middle, command.point);
		const glm::vec3 point {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
		Locator::helpSpeechSystem::value().Say(text, audio::SpeechVoice::First, point);
		return fmt::format("line of text {} said at ({:.1f}, {:.1f})", text, at.x, at.y);
	}
	case Kind::PlayGuidanceVoice:
	{
		if (!Locator::audio::has_value())
		{
			return "no audio";
		}
		const auto sample = fmt::format("Guidance.sad/{}", command.value);
		audio::SoundEffectOptions options {.pitchPercent = guidance::k_VoicePitch,
		                                   .volume = guidance::k_VoiceVolume,
		                                   .voicePlayType = pack::AudioBankLoop::Once};
		if (command.ctrl)
		{
			const auto at = MapPoint(_middle, command.point);
			options.position = glm::vec3 {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
			options.minDistance = command.radius * guidance::k_VoiceNearShare;
			options.maxDistance = command.radius;
			options.callerWins = audio::k_CallerDistancesWin;
		}
		const auto started = Locator::audio::value().StartSoundEffect(resources::HashIdentifier(sample), options);
		return fmt::format("{} asked for at volume {} and pitch {}%{}{}", sample, guidance::k_VoiceVolume,
		                   guidance::k_VoicePitch, command.ctrl ? ", placed" : ", heard everywhere",
		                   started == entt::null ? ", not started" : "");
	}
	case Kind::FillSiteIcons:
	{
		if (!Locator::infoConstants::has_value())
		{
			return "no info";
		}
		const auto& info = Locator::infoConstants::value();
		auto& registry = Locator::entitiesRegistry::value();
		size_t filled = 0;
		registry.Each<ecs::components::WorshipIcon>([&](ecs::components::WorshipIcon& icon) {
			const auto* site =
			    registry.Valid(icon.site) ? registry.TryGet<const ecs::components::WorshipSite>(icon.site) : nullptr;
			if (site == nullptr || site->player != command.player || icon.seed == SpellSeedType::None)
			{
				return;
			}
			const auto type = magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, icon.seed), icon.powerUp);
			icon.store = magic::GetChantsRequiredToCreate(info, type) * command.amount;
			++filled;
		});
		return fmt::format("{} icons filled to {:.0f}%", filled, command.amount * 100.0f);
	}
	case Kind::ChargeIconWithSeedHeld:
		return ChargeIconWithSeedHeld(command);
	case Kind::BuildWorshipSites:
	{
		if (!Locator::worshipSiteSystem::has_value())
		{
			return "no worship sites";
		}
		auto& sites = Locator::worshipSiteSystem::value();
		auto& registry = Locator::entitiesRegistry::value();
		std::vector<entt::entity> unbuilt;
		registry.Each<const ecs::components::WorshipSite>(
		    [&](entt::entity site, const ecs::components::WorshipSite& component) {
			    if (component.player == command.player && !sites.IsBuilt(site))
			    {
				    unbuilt.push_back(site);
			    }
		    });
		for (const auto site : unbuilt)
		{
			sites.BuildBy(site, 1.0f);
		}
		return fmt::format("{} worship sites built", unbuilt.size());
	}
	case Kind::HandRelease:
		if (!Locator::handGrabSystem::has_value() || !Locator::handGrabSystem::value().LetGoAt(command.velocity))
		{
			return "the hand holds nothing";
		}
		return command.velocity == glm::vec3(0.0f) ? "put down" : "thrown";
	default:
		return {};
	}
}

std::string Runner::StopScript(const Command& command)
{
	if (!Locator::vm::has_value())
	{
		return "no script machine";
	}
	auto& vm = Locator::vm::value();
	const auto before = vm.GetTasks().size();
	vm.StopScripts([&command](const std::string& name, const std::string& /*filename*/) { return name == command.script; });
	const auto stopped = before - vm.GetTasks().size();
	const auto object = ObjectAt(0);
	const auto inScript = object.has_value() && Locator::entitiesRegistry::value().AllOf<ecs::components::InScript>(*object);
	return fmt::format("stopped {} tasks of {}; the object is {}in a script", stopped, command.script, inScript ? "" : "not ");
}

std::optional<lhvm::VMValue> Runner::ScriptValueOf(const ScriptArgument& argument)
{
	// A number, or an object that the scripts' table knows, or a marker made for it
	std::optional<entt::entity> object;
	switch (argument.kind)
	{
	case ScriptArgument::Kind::Number:
		return lhvm::VMValue(argument.number);
	case ScriptArgument::Kind::Integer:
		return lhvm::VMValue(static_cast<int32_t>(argument.number));
	case ScriptArgument::Kind::Marker:
	{
		const auto& land = Locator::terrainSystem::value();
		const auto point = MapPoint(_middle, argument.point);
		const auto marker =
		    ecs::archetypes::ScriptMarkerArchetype::Create({point.x, land.GetHeightAt(point) + argument.height, point.y});
		Locator::scriptObjects::value().Register(marker, true);
		return lhvm::VMValue(static_cast<uint32_t>(marker));
	}
	case ScriptArgument::Kind::Object:
		object = ObjectAt(argument.index);
		break;
	case ScriptArgument::Kind::Creature:
		object = CreatureAt(argument.index);
		break;
	case ScriptArgument::Kind::Position:
		// A position is three values, which only a native call gives
		return std::nullopt;
	}
	if (!object.has_value())
	{
		return std::nullopt;
	}
	Locator::scriptObjects::value().Register(*object, false);
	return lhvm::VMValue(static_cast<uint32_t>(*object));
}

std::string Runner::CallNative(const Command& command)
{
	if (!Locator::vm::has_value() || !Locator::scriptObjects::has_value())
	{
		return "no script machine";
	}
	auto& vm = Locator::vm::value();
	const auto* functions = vm.GetFunctions();
	if (functions == nullptr)
	{
		return "no natives";
	}
	const auto found =
	    std::ranges::find_if(*functions, [&command](const auto& function) { return function.name == command.script; });
	if (found == functions->end())
	{
		return fmt::format("no native {}", command.script);
	}
	std::vector<lhvm::NativeArgument> arguments;
	for (const auto& argument : command.scriptArguments)
	{
		if (argument.kind == ScriptArgument::Kind::Position)
		{
			const auto point = MapPoint(_middle, argument.point);
			const float height = Locator::terrainSystem::value().GetHeightAt(point) + argument.height;
			for (const float part : {point.x, height, point.y})
			{
				arguments.emplace_back(std::pair(lhvm::VMValue(part), lhvm::DataType::Float));
			}
			continue;
		}
		const auto given = ScriptValueOf(argument);
		if (!given.has_value())
		{
			return "an argument names nothing of the scenario's";
		}
		const auto type = ScriptDataTypeOf(argument.kind);
		arguments.emplace_back(std::pair(*given, type));
	}
	std::vector<std::pair<lhvm::VMValue, lhvm::DataType>> results;
	const auto id = static_cast<uint32_t>(std::distance(functions->begin(), found));
	if (!vm.CallNative(id, arguments, results))
	{
		return fmt::format("{} could not run now", command.script);
	}
	return fmt::format("{} ran and gave back {} values", command.script, results.size());
}

std::string Runner::RunScript(const Command& command)
{
	if (!Locator::vm::has_value() || !Locator::scriptObjects::has_value())
	{
		return "no script machine";
	}
	auto& vm = Locator::vm::value();
	if (std::ranges::none_of(vm.GetScripts(), [&command](const auto& script) { return script.name == command.script; }))
	{
		return fmt::format("no script {}", command.script);
	}
	const auto value = [this](const ScriptArgument& argument) { return ScriptValueOf(argument); };
	const auto type = [](const ScriptArgument& argument) { return ScriptDataTypeOf(argument.kind); };
	const auto& globals = vm.GetVariables();
	for (const auto& [name, argument] : command.scriptGlobals)
	{
		const auto found = std::ranges::find_if(globals, [name](const auto& global) { return global.name == name; });
		const auto given = value(argument);
		if (found == globals.end() || !given.has_value())
		{
			return fmt::format("no global {} or nothing to set it to", name);
		}
		vm.SetVariable(static_cast<uint32_t>(std::distance(globals.begin(), found)), *given);
	}
	for (const auto& argument : command.scriptArguments)
	{
		const auto given = value(argument);
		if (!given.has_value())
		{
			return "an argument names nothing of the scenario's";
		}
		vm.Push(*given, type(argument));
	}
	const auto task = vm.StartScript(std::string(command.script), lhvm::ScriptType::All);
	return task != 0 ? fmt::format("{} runs as task {}", command.script, task) : fmt::format("{} didn't start", command.script);
}

std::string Runner::HandTapObject(size_t index)
{
	const auto object = ObjectAt(index);
	if (!object.has_value() || !Locator::handSystem::has_value() || !Locator::abodeKnockSystem::has_value())
	{
		return "nothing to tap";
	}
	// A knock on a building by this computer's hand, from where the hand is, as the Action button pressed on it makes
	auto& registry = Locator::entitiesRegistry::value();
	const auto hand =
	    Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	const auto* transform = registry.TryGet<const Transform>(hand);
	// A rock tall enough breaks under the tap of this computer's player's hand
	if (ecs::object_physics::CanTapRock(*object) && Locator::dynamicsSystem::has_value())
	{
		const auto player =
		    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
		ecs::object_physics::TapRock(Locator::dynamicsSystem::value(), *object,
		                             transform != nullptr ? transform->position : glm::vec3(0.0f), player);
		return "broke the rock";
	}
	const bool took =
	    Locator::abodeKnockSystem::value().Tap(*object, transform != nullptr ? transform->position : glm::vec3(0.0f), true);
	return took ? "knocked" : "it takes no knock";
}

std::string Runner::ChargeIconWithSeedHeld(const Command& command)
{
	if (!Locator::infoConstants::has_value() || !Locator::spellIconSystem::has_value() || !Locator::magicSystem::has_value() ||
	    !Locator::worshipSiteSystem::has_value())
	{
		return "no miracles";
	}
	const auto& info = Locator::infoConstants::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto seedType = static_cast<SpellSeedType>(command.value);
	std::optional<entt::entity> found;
	registry.Each<const ecs::components::WorshipIcon>([&](entt::entity entity, const ecs::components::WorshipIcon& icon) {
		const auto* site = registry.Valid(icon.site) ? registry.TryGet<const ecs::components::WorshipSite>(icon.site) : nullptr;
		if (!found.has_value() && site != nullptr && site->player == command.player && icon.seed == seedType)
		{
			found = entity;
		}
	});
	if (!found.has_value())
	{
		return "no such icon";
	}
	auto& icon = registry.Get<ecs::components::WorshipIcon>(*found);
	const auto type = magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, icon.seed), magic::k_BasePowerUpLevel);
	const float cost = magic::GetChantsRequiredToCreate(info, type);
	// The icon is charged full and a tap puts its miracle in the hand
	icon.charging = false;
	icon.store = cost;
	Locator::spellIconSystem::value().Tap(*found, command.player);
	const auto held = Locator::magicSystem::value().GetHeldSeed();
	if (!held.has_value() || std::ranges::find(icon.seedsOut, *held) == icon.seedsOut.end())
	{
		return "the hand did not take the seed";
	}
	// The seed keeps only a part of its charge, and the icon charges again for the hand that holds the seed
	auto& seed = registry.Get<ecs::components::SpellSeed>(*held);
	seed.chantStore = cost * command.amount;
	icon.charging = true;
	icon.charger = command.player;
	icon.powerUp = magic::k_BasePowerUpLevel;
	icon.store = 0.0f;
	Locator::worshipSiteSystem::value().ReturnChants(icon.site, cost);
	return fmt::format("the seed in the hand holds {:.0f} of {:.0f}, and its icon charges", seed.chantStore, cost);
}

std::string Runner::HandTakeFireBall()
{
	if (!Locator::magicSystem::has_value())
	{
		return "no miracles";
	}
	// The first fireball the hand may hold now, the hand straight over it looking down
	std::optional<glm::vec3> target;
	Locator::entitiesRegistry::value().Each<const ecs::components::MagicFireBall>(
	    [this, &target](entt::entity entity, const ecs::components::MagicFireBall& ball) {
		    if (!target.has_value() && ball.handTarget.has_value())
		    {
			    target = ball.handTarget;
			    _fireBallTaken = entity;
		    }
	    });
	if (!target.has_value())
	{
		return "no fireball to take hold of";
	}
	constexpr float k_HandAbove = 10.0f;
	const glm::vec3 hand = *target + glm::vec3(0.0f, k_HandAbove, 0.0f);
	Locator::magicSystem::value().DriveHand(ecs::systems::MagicSystemInterface::HandFrame {.handPosition = hand,
	                                                                                       .point = *target,
	                                                                                       .rayOrigin = hand,
	                                                                                       .rayDirection = {0.0f, -1.0f, 0.0f},
	                                                                                       .cameraForward = {0.0f, -1.0f, 0.0f},
	                                                                                       .overWorld = true});
	// The hand takes its place over the next frames, then taps or presses
	_fireBallTakeFrames = 0;
	return fmt::format("the hand goes over it at ({:.1f}, {:.1f}, {:.1f})", target->x, target->y, target->z);
}

void Runner::FinishTakingFireBall()
{
	constexpr int k_FramesToReach = 2;
	if (!_fireBallTakeFrames.has_value() || ++*_fireBallTakeFrames < k_FramesToReach || !Locator::magicSystem::has_value())
	{
		return;
	}
	_fireBallTakeFrames.reset();
	auto& magic = Locator::magicSystem::value();
	std::string result;
	if (magic.IsHandBusy())
	{
		const bool taken = magic.PressAction();
		magic.ReleaseAction();
		result = taken ? "the press was taken" : "the press wasn't taken";
	}
	else
	{
		result = magic.TapFireBall(_fireBallTaken) ? "caught it" : "it slipped through the hand";
	}
	magic.DriveHand(std::nullopt);
	Log(fmt::format("{:.1f}s: hand on the fireball: {}", _seconds, result));
}

std::string Runner::DrawGesture(GestureType gesture)
{
	if (!Locator::gestureSystem::has_value())
	{
		return "no gestures";
	}
	auto& gestures = Locator::gestureSystem::value();
	// Across the middle of the screen, a little under half its height across
	const auto aspect = gestures.GetScreenAspect();
	const glm::vec2 middle {gesture::k_ReferenceHeight * aspect * 0.5f, gesture::k_ReferenceHeight * 0.5f};
	constexpr float k_Size = 320.0f;
	auto path = gesture::TraceGesture(gestures.GetTemplates(), gesture, middle, k_Size, aspect);
	if (!path.has_value())
	{
		// Without the game's templates, the circle and the scribble are drawn as a hand would draw them
		if (gesture == GestureType::Circle)
		{
			path = gesture::TraceCircle(middle, k_Size * 0.5f, true);
		}
		else if (gesture == GestureType::Scribble)
		{
			path = gesture::TraceScribble(middle, k_Size, 5);
		}
		else
		{
			return "no template to draw it from";
		}
	}
	// A circle sizes a storm or a shield readied by holding the Action button
	const bool holdingAction = gesture == GestureType::Circle;
	gestures.DrawPath(std::move(*path), holdingAction);
	// With a miracle in the hand the button goes down on it too, as the player's does, readying it to be cast
	_drawingPressedAction = holdingAction && Locator::magicSystem::has_value() && Locator::magicSystem::value().IsHandBusy() &&
	                        Locator::magicSystem::value().PressAction();
	if (_drawingPressedAction)
	{
		Log(fmt::format("{:.1f}s: pressed for the circle: {}", _seconds,
		                HandResultName(Locator::magicSystem::value().GetLastHandResult())));
	}
	_drawing = gesture;
	const auto last = gestures.GetLastRecognised();
	_recognisedBefore = last.has_value() ? last->number : 0;
	return fmt::format("drawing gesture {}", static_cast<int>(gesture));
}

void Runner::WatchGesture()
{
	if (!_drawing.has_value() || !Locator::gestureSystem::has_value())
	{
		return;
	}
	const auto& gestures = Locator::gestureSystem::value();
	const auto recognised = gestures.GetLastRecognised();
	// Recognised, or drawn to the end without being recognised
	const bool done = recognised.has_value() && recognised->number != _recognisedBefore;
	// A button held for the gesture stays down until the path is drawn to its end
	if ((!done || _drawingPressedAction) && gestures.IsDrawingPath())
	{
		return;
	}
	if (_drawingPressedAction && Locator::magicSystem::has_value())
	{
		auto& magic = Locator::magicSystem::value();
		magic.ReleaseAction();
		Log(fmt::format("{:.1f}s: let go after the circle: {}", _seconds, HandResultName(magic.GetLastHandResult())));
		_drawingPressedAction = false;
	}
	if (!done)
	{
		Log(fmt::format("{:.1f}s: gesture {} not recognised", _seconds, static_cast<int>(*_drawing)));
		_drawing.reset();
		return;
	}
	std::string what;
	if (const auto& event = recognised->event; event.has_value())
	{
		using EventKind = ecs::systems::GestureEvent::Kind;
		if (event->kind == EventKind::Circle)
		{
			what =
			    fmt::format(", a circle at {:.0f}, {:.0f} of radius {:.1f}", event->centre.x, event->centre.z, event->radius);
		}
		else if (event->kind == EventKind::PowerUp)
		{
			what = fmt::format(", power up to level {}", event->powerUpLevel);
		}
		else
		{
			what = ", the held seed is dropped";
		}
	}
	const auto creature = CreatureAt(0);
	const auto leash = Locator::leashSystem::has_value() && creature.has_value()
	                       ? Locator::leashSystem::value().TypeOf(*creature)
	                       : LeashType::None;
	Log(fmt::format("{:.1f}s: gesture {} recognised ({}){}; leash {}, picker {}", _seconds,
	                static_cast<int>(recognised->request.gesture), gesture::Name(recognised->request.purpose), what,
	                leash == LeashType::None ? "off" : creature_leash::Name(leash),
	                gestures.IsLeashPickerOpen() ? "open" : "closed"));
	_drawing.reset();
}

std::string Runner::GiveFightCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureFightSystem::has_value())
	{
		return "no fights";
	}
	auto& fights = Locator::creatureFightSystem::value();
	switch (command.kind)
	{
	case Kind::StartFight:
		if (const auto opponent = CreatureAt(command.value))
		{
			constexpr std::array<std::string_view, 6> k_Results {"started",  "no opponent", "busy",
			                                                     "too weak", "no arena",    "refused"};
			return std::string(k_Results.at(static_cast<size_t>(fights.StartFight(creature, *opponent))));
		}
		return "it is gone";
	case Kind::FightBlow:
	{
		constexpr std::array k_Bands {creature_fight::Band::High, creature_fight::Band::Mid, creature_fight::Band::Low};
		// As a click held for the charge, then let go
		const bool queued = fights.QueueMove(creature, creature_fight::AttackMove(k_Bands.at(command.value)), true);
		fights.ReleaseCharge(creature, command.chargeMs);
		return queued ? fmt::format("{:.0f} ms", command.chargeMs) : "not fighting";
	}
	case Kind::FightBlock:
		return fights.QueueMove(creature, creature_fight::BlockMove(), true) ? "" : "not fighting";
	case Kind::FightStep:
		return fights.QueueMove(creature, creature_fight::StepMove(static_cast<creature_fight::Step>(command.value)), true)
		           ? ""
		           : "not fighting";
	case Kind::FightSpecial:
		return fights.QueueMove(creature, {.kind = creature_fight::Move::Kind::Special}, true) ? "" : "not fighting";
	case Kind::FightAuto:
		fights.SetAutoFighting(creature, command.value != 0);
		return fights.IsFighting(creature) ? "" : "not fighting";
	case Kind::FightSpell:
		return fights.QueueMove(
		           creature, {.kind = creature_fight::Move::Kind::Spell, .value = static_cast<uint32_t>(command.value)}, false)
		           ? ""
		           : "not fighting";
	case Kind::FightView:
		fights.FlewTo(Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(creature).position);
		return {};
	case Kind::FightAutopilot:
	{
		auto* autopilot = Locator::inspector::has_value() ? Locator::inspector::value().GetFightAutopilot() : nullptr;
		if (autopilot == nullptr)
		{
			return "no fight autopilot: start the game with the inspector (--inspect-port 0)";
		}
		return autopilot->Start({.policy = static_cast<fight_autopilot::Policy>(command.value),
		                         .seed = 1,
		                         .stage = std::nullopt,
		                         .sequence = {},
		                         .enterView = true});
	}
	case Kind::FightLessonCheck:
	{
		if (!Locator::fightTranscript::has_value())
		{
			return "no fight transcript: start the game with the inspector (--inspect-port 0)";
		}
		const auto& recorder = Locator::fightTranscript::value().GetRecorder();
		const auto fight = recorder.FightOf(entt::to_integral(creature));
		if (!fight.has_value())
		{
			return "failed: no fight is recorded for the creature";
		}
		const auto check = fight_autopilot::CheckLesson(*recorder.Find(*fight), entt::to_integral(creature));
		const auto at = [](const std::optional<uint64_t>& index) {
			return index.has_value() ? fmt::format("{}", *index) : std::string("none");
		};
		return fmt::format("{}: hit at record {}, hits at {}, block at {}, step at {} of fight {}",
		                   check.InOrder() ? "passed" : "failed", at(check.hit), at(check.hits), at(check.block),
		                   at(check.step), *fight);
	}
	case Kind::KnockOut:
		fights.KnockOut(creature);
		return {};
	case Kind::BringRound:
		fights.Resurrect(creature);
		return {};
	case Kind::FightAbort:
		fights.AbortFight(creature);
		return {};
	case Kind::TieLeashToCreature:
		if (const auto other = CreatureAt(command.value); other.has_value() && Locator::leashSystem::has_value())
		{
			return Locator::leashSystem::value().TieTo(creature, *other) ? "tied" : "can't";
		}
		return "it is gone";
	default:
		return {};
	}
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
	    command.kind == Kind::HandTake || command.kind == Kind::HandPour || command.kind == Kind::HandRelease ||
	    command.kind == Kind::RemoveObject || command.kind == Kind::HandGripTotem ||
	    command.kind == Kind::AttachObjectScriptSound || command.kind == Kind::SayLine ||
	    command.kind == Kind::PlayGuidanceVoice || command.kind == Kind::ScriptRelease || command.kind == Kind::FillSiteIcons ||
	    command.kind == Kind::BuildWorshipSites || command.kind == Kind::ChargeIconWithSeedHeld ||
	    command.kind == Kind::FinishBuilding || command.kind == Kind::PauseGame || command.kind == Kind::SetGameSpeed ||
	    command.kind == Kind::RemoveTeleportStone || command.kind == Kind::MakeDance || command.kind == Kind::OtherGodHand)
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
	if (command.kind == Kind::OpenTattooMenu)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), OpenTattooMenu()));
		return;
	}
	if (command.kind == Kind::OpenControlsBox)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), OpenControlsBox(command.value)));
		return;
	}
	if (command.kind == Kind::OpenMultiplayerBox)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), OpenMultiplayerBox(command.value)));
		return;
	}
	if (command.kind == Kind::OpenMenuPage)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), OpenMenuPage(command.value, command.ctrl)));
		return;
	}
	if (command.kind == Kind::PressMenuKey)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), PressMenuKey(command.value, command.ctrl)));
		return;
	}
	if (command.kind == Kind::ShowCustomMusicControls)
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr)
		{
			Log(fmt::format("{:.1f}s: {}: failed: no interface", _seconds, Name(command.kind)));
			return;
		}
		auto& controls = game->GetInterface()->GetMusicControls();
		controls.ShowControls(command.value >= 1, command.value >= 2);
		Log(fmt::format("{:.1f}s: {}: buttons {}, play list {}, the controls {}", _seconds, Name(command.kind),
		                controls.GetPanel().controlsShown ? "shown" : "hidden",
		                controls.GetPanel().playListShown ? "shown" : "hidden", controls.IsActive() ? "show" : "hide now"));
		return;
	}
	if (command.kind == Kind::StartScriptMusic)
	{
		if (command.value >= static_cast<size_t>(audio::MusicType::_COUNT))
		{
			Log(fmt::format("{:.1f}s: {}: no music type {}", _seconds, Name(command.kind), command.value));
			return;
		}
		const auto type = static_cast<audio::MusicType>(command.value);
		if (auto* game = Game::Instance(); game != nullptr && game->GetGameMusic() != nullptr)
		{
			game->GetGameMusic()->ResetMusicLyrics();
			game->GetGameMusic()->StartScriptMusic(type);
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), audio::GetMusicTypeName(type)));
		return;
	}
	if (command.kind == Kind::PlayCustomMusic)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), PlayCustomMusic(command.value)));
		return;
	}
	if (command.kind == Kind::OwnTattooPicture)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), GiveOwnTattooPicture(command.value)));
		return;
	}
	if (command.kind == Kind::ProfileFolderCheck)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), CheckProfileFolder()));
		return;
	}
	if (command.kind == Kind::PlayBoatFilm)
	{
		Log(fmt::format("{:.1f}s: {}", _seconds, PlayBoatFilm(command)));
		return;
	}
	if (command.kind == Kind::EnterTemple)
	{
		const auto room = static_cast<TempleRoom>(command.value);
		if (Locator::temple::has_value())
		{
			auto& temple = Locator::temple::value();
			// In the temple, no room is a walk through the main room's way out
			if (temple.Active() && room == TempleRoom::Unknown)
			{
				temple.EnterRoom(room);
			}
			else if (temple.Active())
			{
				temple.GoToRoom(room);
			}
			else
			{
				temple.ActivateAfterFrame(room);
			}
		}
		Log(fmt::format("{:.1f}s: the player goes into temple room {}", _seconds, command.value));
		return;
	}
	if (command.kind == Kind::Reframe)
	{
		Frame(_scenario->framing.shot, _scenario->framing.creature, _scenario->framing.distance);
		Log(fmt::format("{:.1f}s: the camera is put back", _seconds));
		return;
	}
	if (command.kind == Kind::SetHour)
	{
		if (Locator::skySystem::has_value())
		{
			Locator::skySystem::value().SetTime(command.hour);
		}
		Log(fmt::format("{:.1f}s: the hour is {:.1f}", _seconds, command.hour));
		return;
	}
	if (command.kind == Kind::Advisor)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), GiveAdvisorCommand(command)));
		return;
	}
	if (command.kind == Kind::SetOpenClose || command.kind == Kind::LayGateStone)
	{
		GiveSceneryCommand(command);
		return;
	}
	if (command.kind == Kind::SetAlignment)
	{
		if (Locator::alignmentSystem::has_value())
		{
			Locator::alignmentSystem::value().SetPlayerAlignment(command.player, command.alignment);
		}
		Log(fmt::format("{:.1f}s: the alignment is {:.2f}", _seconds, command.alignment));
		return;
	}
	if (command.kind == Kind::SaveGame || command.kind == Kind::LoadGame)
	{
		const auto slot = static_cast<uint32_t>(command.value);
		const bool saving = command.kind == Kind::SaveGame;
		bool done = false;
		if (Locator::saveGameSystem::has_value())
		{
			auto& saves = Locator::saveGameSystem::value();
			done = saving ? saves.Save(slot, u"Testbed") : saves.Load(slot);
		}
		Log(fmt::format("{:.1f}s: {} slot {}: {}", _seconds, saving ? "save into" : "load from", slot,
		                done ? "done" : "failed"));
		return;
	}
	if (command.kind == Kind::PlayLogoEffect)
	{
		if (auto* game = Game::Instance(); game != nullptr)
		{
			game->PlayLogoEffect();
		}
		Log(fmt::format("{:.1f}s: the logo effect played", _seconds));
		return;
	}
	if (command.kind == Kind::ContinueGame)
	{
		auto* game = Game::Instance();
		const bool loaded = game != nullptr && game->ContinueFromExitSave();
		Log(fmt::format("{:.1f}s: continue from the save on exit: {}", _seconds,
		                loaded ? "loaded, the menu shows" : "a new game"));
		return;
	}
	if (command.kind == Kind::QuickSave)
	{
		const bool done = Locator::saveGameSystem::has_value() && Locator::saveGameSystem::value().QuickSave();
		Log(fmt::format("{:.1f}s: quick save: {}", _seconds, done ? "done" : "failed"));
		return;
	}
	if (command.kind == Kind::EndGuidanceWaits)
	{
		if (Locator::guidanceSystem::has_value())
		{
			Locator::guidanceSystem::value().EndWaits();
		}
		Log(fmt::format("{:.1f}s: the guidance's waits are over", _seconds));
		return;
	}
	if (command.kind == Kind::RunHelpMessage)
	{
		const bool started =
		    Locator::helpMessageSystem::has_value() && Locator::helpMessageSystem::value().RunMessageSet(command.value);
		Log(fmt::format("{:.1f}s: the help message set {} {}", _seconds, command.value, started ? "runs" : "doesn't run"));
		return;
	}
	if (command.kind == Kind::DestroyTemple)
	{
		const bool known = command.value < _temples.size();
		if (known && Locator::templeDestructionSystem::has_value())
		{
			Locator::templeDestructionSystem::value().Start(_temples.at(command.value));
		}
		Log(fmt::format("{:.1f}s: temple {} {}", _seconds, command.value, known ? "starts being destroyed" : "isn't there"));
		return;
	}
	if (command.kind == Kind::LookAtTempleScroll)
	{
		if (Locator::temple::has_value())
		{
			Locator::temple::value().LookAtScroll(command.value);
		}
		Log(fmt::format("{:.1f}s: the temple's camera looks at scroll {}", _seconds, command.value));
		return;
	}
	if (command.kind == Kind::ReadTip)
	{
		// A sign far under the land, tapped once by the player
		const auto text = static_cast<uint32_t>(command.value);
		const auto category = static_cast<uint32_t>(std::lround(command.amount));
		const bool read = Locator::scriptHighlightSystem::has_value();
		if (read)
		{
			auto& highlights = Locator::scriptHighlightSystem::value();
			const auto sign =
			    highlights.Create(static_cast<uint32_t>(HighlightInfo::DidYouKnowSign), {0.0f, -1000.0f, 0.0f}, 0);
			highlights.SetProperties(sign, text, category);
			highlights.Tap(sign, true);
		}
		Log(fmt::format("{:.1f}s: tip {} of category {} {}", _seconds, text, category, read ? "read" : "not read"));
		return;
	}
	if (command.kind == Kind::RunText)
	{
		// Not alone on its line and not waiting for a click, as the lessons' texts are shown
		const bool shown = Locator::helpTextSystem::has_value() && Locator::helpTextSystem::value().IsStarted() &&
		                   Locator::helpTextSystem::value().RunText(false, static_cast<uint32_t>(command.value), 0);
		Log(fmt::format("{:.1f}s: help text {} {}", _seconds, command.value, shown ? "shown" : "not shown"));
		return;
	}
	if (command.kind == Kind::PlayHandDemo)
	{
		if (Locator::handDemoSystem::has_value())
		{
			Locator::handDemoSystem::value().Play(command.handDemo, 0, false, false);
		}
		Log(fmt::format("{:.1f}s: the hand demonstration {} plays", _seconds, command.handDemo));
		return;
	}
	if (command.kind == Kind::FlyToBookmark)
	{
		const auto at = MapPoint(_middle, command.point);
		const glm::vec3 focus {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
		const glm::vec3 origin = focus + glm::vec3(0.0f, 50.0f, -80.0f);
		if (Locator::cameraBookmarkSystem::has_value())
		{
			Locator::cameraBookmarkSystem::value().SetBookmark(static_cast<size_t>(command.value), focus, std::nullopt, origin);
		}
		Locator::camera::value().GetModel().SetFlight(origin, focus, camera_fly::k_BookmarkRise);
		Log(fmt::format("{:.1f}s: the camera flies to bookmark {} at ({:.0f}, {:.0f}, {:.0f})", _seconds, command.value + 1,
		                focus.x, focus.y, focus.z));
		return;
	}
	if (command.kind == Kind::TabToTown)
	{
		if (Locator::cameraBookmarkSystem::has_value())
		{
			Locator::cameraBookmarkSystem::value().FlyToNextTown(std::nullopt);
		}
		Log(fmt::format("{:.1f}s: the player presses the Tab key", _seconds));
		return;
	}
	if (command.kind == Kind::SetInterfaceLevel)
	{
		if (Locator::cameraHelpSystem::has_value())
		{
			auto& help = Locator::cameraHelpSystem::value().Get();
			const bool known = help.SetInterfaceLevel(static_cast<int32_t>(command.value));
			if (known && Locator::playerSystem::has_value())
			{
				script::interface_levels::Apply(Locator::entitiesRegistry::value(),
				                                Locator::playerSystem::value().GetPlayer(PlayerNames::PLAYER_ONE),
				                                static_cast<int32_t>(command.value));
			}
			Log(fmt::format("{:.1f}s: the interface level is {}{}, the hand reaches {:.0f}", _seconds, command.value,
			                known ? "" : " (a level the game does not know)", help.handReach));
		}
		return;
	}
	if (command.kind == Kind::PlayIntroFilm)
	{
		if (Locator::videoSystem::has_value())
		{
			auto& videos = Locator::videoSystem::value();
			videos.Play("Data/INTRO.bik");
			videos.ScheduleIntro();
		}
		Log(fmt::format("{:.1f}s: the intro film plays", _seconds));
		return;
	}
	if (command.kind == Kind::PlayFallingSpellFilm)
	{
		// The script's film starts and the script's fade goes at once
		if (Locator::videoSystem::has_value())
		{
			Locator::videoSystem::value().StartFallingSpell();
		}
		if (Locator::cinematicDirectorSystem::has_value())
		{
			Locator::cinematicDirectorSystem::value().FadeBackToNormal(0);
		}
		Log(fmt::format("{:.1f}s: the falling spell's film starts", _seconds));
		return;
	}
	if (command.kind == Kind::WideScreen)
	{
		// Held by a script, as the land's scripts hold them, under a number no script task has: a task of that number
		// stopping would take the bars away with it
		constexpr uint32_t k_ScriptOwner = std::numeric_limits<uint32_t>::max();
		if (Locator::cinematicDirectorSystem::has_value())
		{
			Locator::cinematicDirectorSystem::value().SetWideScreen(command.value != 0, k_ScriptOwner);
		}
		Log(fmt::format("{:.1f}s: the cinema bars slide {}", _seconds, command.value != 0 ? "in" : "out"));
		return;
	}
	// With a value of 1, the scenario's object catches fire, with or without a creature. With a value of 2, it catches
	// at the most heat of its fire, so that it burns for a time.
	if (command.kind == Kind::SetAlight && (command.value == 1 || command.value == 2))
	{
		const auto burning = ObjectAt(command.object);
		if (Locator::fireSystem::has_value() && burning.has_value())
		{
			Locator::fireSystem::value().SetOnFire(*burning, command.value == 2 ? 1.0f : 0.0f);
		}
		Log(fmt::format("{:.1f}s: object {} {}", _seconds, command.object,
		                Locator::fireSystem::has_value() && burning.has_value() ? "alight" : "no fire"));
		return;
	}
	const auto entity = CreatureAt(command.creature);
	if (!entity.has_value() || !Locator::creatureLocomotionSystem::has_value() || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	const auto& setup = _scenario->creatures[command.creature];
	const auto who = setup.label.empty() ? fmt::format("creature {}", command.creature) : std::string(setup.label);
	// Lying out cold, it does nothing it is told until it comes round
	if (const auto* needs = Locator::entitiesRegistry::value().TryGet<const CreatureNeeds>(*entity);
	    needs != nullptr && needs->rest == CreatureNeeds::Rest::Unconscious && command.kind != Kind::BringRound)
	{
		Log(fmt::format("{:.1f}s: {} {}: out cold", _seconds, who, Name(command.kind)));
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	auto& minds = Locator::creatureMindSystem::value();
	const auto point = MapPoint(_middle, command.point);
	std::string result;
	switch (command.kind)
	{
	case Kind::WalkTo:
	case Kind::RunTo:
		result =
		    MoveResultName(locomotion.MoveTo(*entity, point, command.kind == Kind::RunTo ? Pace::Run : Pace::Walk, 0.0f, 1.0f));
		break;
	case Kind::ScriptPlay:
		result = minds.CarryOutForScript(
		             *entity, creature_script_play::Agenda({.animation = static_cast<int32_t>(command.value),
		                                                    .plays = static_cast<uint32_t>(std::lround(command.amount))}))
		             ? "playing"
		             : "can't";
		break;
	case Kind::AnimalFlees:
	{
		const auto animal = ObjectAt(command.object);
		result = animal.has_value() && Locator::animalSystem::has_value() &&
		                 Locator::animalSystem::value().SetupFleeFromObject(*animal, *entity, Reaction::FleeFromObject)
		             ? "flees"
		             : "can't";
		break;
	}
	case Kind::DoAction:
	{
		const auto* tables = minds.GetTables();
		const auto action = tables != nullptr ? creature_mind_tables::FindAction(*tables, command.action) : std::nullopt;
		const auto target = command.atCreature.has_value()                 ? CreatureAt(*command.atCreature)
		                    : command.onTemple && !_temples.empty()        ? std::optional(_temples.front())
		                    : command.onGlobe && command.globe.has_value() ? GlobeAt(*command.globe)
		                    : command.atCreature.has_value()               ? CreatureAt(*command.atCreature)
		                                                                   : ObjectAt(command.object);
		const auto with = command.globe.has_value() && !command.onGlobe ? GlobeAt(*command.globe)
		                  : command.usesCreature.has_value()            ? CreatureAt(*command.usesCreature)
		                  : command.with.has_value()                    ? ObjectAt(*command.with)
		                                                                : std::nullopt;
		result = !action.has_value()                                     ? "no such action"
		         : !target.has_value()                                   ? "no such object"
		         : minds.ScriptDoAction(*entity, *action, *target, with) ? fmt::format("doing {}", command.action)
		                                                                 : "can't";
		break;
	}
	case Kind::MakeFriends:
		if (const auto other = CreatureAt(command.value))
		{
			auto& registry = Locator::entitiesRegistry::value();
			for (const auto each : {*entity, *other})
			{
				if (!registry.AllOf<ecs::components::CreatureFriends>(each))
				{
					registry.Assign<ecs::components::CreatureFriends>(each);
				}
			}
			creature_activity::MakeFriends(
			    registry.Get<ecs::components::CreatureFriends>(*entity).friends, entt::to_integral(*entity),
			    registry.Get<ecs::components::CreatureFriends>(*other).friends, entt::to_integral(*other));
			result = "friends";
		}
		else
		{
			result = "no such creature";
		}
		break;
	case Kind::OnlyDesire:
		if (command.value < creature_desires::k_DesireCount)
		{
			// For the seconds given (amount), as a script gives them
			minds.SetOnlyDesire(*entity, static_cast<creature_desires::Desire>(command.value), command.amount);
			result = fmt::format("only {}", creature_desires::Name(static_cast<creature_desires::Desire>(command.value)));
		}
		else
		{
			result = "no such desire";
		}
		break;
	case Kind::ScriptReaction:
		ecs::creature_reaction_notice::SetTakesNotice(Locator::entitiesRegistry::value(), *entity, command.value != 0);
		result = command.value != 0 ? "takes notice" : "takes no notice";
		break;
	case Kind::SwapMinds:
		if (const auto other = CreatureAt(command.value))
		{
			result = minds.SwapMinds(*entity, *other) ? "swapped" : "can't";
		}
		else
		{
			result = "no such creature";
		}
		break;
	case Kind::WalkHome:
		if (const auto* leash = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureLeash>(*entity);
		    leash != nullptr && leash->home.has_value())
		{
			result =
			    MoveResultName(locomotion.MoveTo(*entity, glm::vec2(leash->home->x, leash->home->z), Pace::Walk, 0.0f, 1.0f));
		}
		else
		{
			result = "it has no home";
		}
		break;
	case Kind::Follow:
		if (const auto leader = CreatureAt(command.value))
		{
			const auto size = ShownSize(Locator::entitiesRegistry::value().Get<Creature>(*entity));
			result = MoveResultName(locomotion.Follow(*entity, *leader, k_FollowDistance * std::max(size, 0.5f), Pace::Walk));
		}
		break;
	case Kind::FleeFrom:
		result = MoveResultName(locomotion.FleeFrom(*entity, point, Pace::Run));
		break;
	case Kind::TurnToFace:
		result = locomotion.TurnToFace(*entity, point) ? "turning" : "can't";
		break;
	case Kind::FaceCamera:
		result = locomotion.TurnToFace(*entity, glm::xz(Locator::camera::value().GetOrigin())) ? "turning" : "can't";
		break;
	case Kind::Stop:
		locomotion.Stop(*entity);
		break;
	case Kind::StopAtOnce:
		locomotion.StopAtOnce(*entity);
		break;
	case Kind::HoldRouteSearch:
		if (auto* walk = Locator::entitiesRegistry::value().TryGet<ecs::components::CreatureLocomotion>(*entity))
		{
			walk->searchHeld = command.value != 0;
		}
		break;
	case Kind::TurnToFaceWhenStill:
		if (locomotion.TurnToFace(*entity, point))
		{
			result = "turning";
		}
		else
		{
			_turnsAsked.emplace_back(*entity, point);
			result = "waits until it stands";
		}
		break;
	case Kind::ScriptPlace:
		locomotion.PlaceWithoutWalking(*entity, {point.x, Locator::terrainSystem::value().GetHeightAt(point), point.y});
		break;
	case Kind::PlayAction:
		result = minds.PlayAction(*entity, command.value) ? "playing" : "busy";
		break;
	case Kind::PlayGesture:
		result = minds.PlayGesture(*entity, command.value) ? "playing" : "busy";
		break;
	case Kind::PullFace:
		minds.PullFace(*entity, command.value);
		break;
	case Kind::CreatureDrawGesture:
		result = minds.DrawGesture(*entity, static_cast<uint32_t>(command.value)) ? "drawing" : "busy";
		break;
	case Kind::ShowFeeling:
		if (const auto face = minds.ShowFeeling(*entity, static_cast<creature_face::Cue>(command.value)))
		{
			result = creature_face::Name(face->face);
		}
		break;
	case Kind::SitDown:
		result = minds.SitDown(*entity) ? "sitting" : "busy";
		break;
	case Kind::StandUp:
		minds.StandUp(*entity);
		break;
	case Kind::Sleep:
		result = minds.Sleep(*entity) ? "started" : "can't";
		break;
	case Kind::Wake:
		minds.Wake(*entity);
		break;
	case Kind::Eat:
		result = minds.Eat(*entity, std::nullopt) ? "started" : "nothing to eat";
		break;
	case Kind::Drink:
		result = minds.Drink(*entity) ? "started" : "no water near";
		break;
	case Kind::Poo:
		result = minds.Poo(*entity) ? "started" : "can't";
		break;
	case Kind::ForceAction:
		result =
		    minds.ForcePlan(
		        *entity,
		        {.desire = creature_desires::Desire::ObeyPlayer, .action = command.action, .object = *entity, .ordered = true})
		        ? fmt::format("forced {}", command.action)
		        : "can't";
		break;
	case Kind::Fart:
		// A fart is always a plan for the desire to poo
		result = minds.ForcePlan(*entity, {.desire = creature_desires::Desire::Poo, .action = "Fart"}) ? "started" : "can't";
		break;
	case Kind::Puke:
		result = minds.Puke(*entity) ? "started" : "can't";
		break;
	case Kind::Faint:
		result = minds.Faint(*entity) ? "started" : "can't";
		break;
	case Kind::Stroke:
	case Kind::Slap:
		// As a whole session of the hand on the creature would, a full reward or punishment as the hand lets go
		minds.ReceiveFeedback(*entity, command.kind == Kind::Stroke ? 1.0f : -1.0f);
		break;
	case Kind::PickUp:
	case Kind::PutDown:
	case Kind::Discard:
	case Kind::EatHeld:
	case Kind::Examine:
	case Kind::ThrowAt:
	case Kind::KnockDown:
	case Kind::PointAt:
		result = GiveObjectCommand(*entity, command);
		break;
	case Kind::HandStroke:
	case Kind::HandSlap:
	case Kind::HandLetGo:
		result = GiveHandCommand(*entity, command);
		break;
	case Kind::PutOnLeash:
	case Kind::TieLeash:
	case Kind::UntieLeash:
	case Kind::TakeOffLeash:
	case Kind::ConfineToHome:
	case Kind::MakeLeashable:
	case Kind::HandTapLeash:
	case Kind::LeashKey:
	case Kind::LeashShake:
	case Kind::LeashOrderAt:
	case Kind::LeashOrderOn:
	case Kind::HangLeashPosts:
	case Kind::TapLeashPost:
	case Kind::LeashTapBuilding:
		result = GiveLeashCommand(*entity, command);
		break;
	case Kind::StartFight:
	case Kind::FightBlow:
	case Kind::FightBlock:
	case Kind::FightStep:
	case Kind::FightSpecial:
	case Kind::FightAuto:
	case Kind::FightSpell:
	case Kind::FightView:
	case Kind::FightAutopilot:
	case Kind::FightLessonCheck:
	case Kind::KnockOut:
	case Kind::BringRound:
	case Kind::FightAbort:
	case Kind::TieLeashToCreature:
		result = GiveFightCommand(*entity, command);
		break;
	case Kind::CreatureKey:
	case Kind::DoubleClick:
	case Kind::CameraKeys:
	case Kind::ClearCameraView:
	case Kind::LeaveCreatureMode:
	case Kind::OpenCreatureCave:
	case Kind::ApplyTattoo:
	case Kind::RemoveTattoo:
	case Kind::OpenTattooEditor:
		result = GiveCreatureModeCommand(*entity, command);
		break;
	case Kind::GiveHandGlows:
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto& glows = registry.AllOf<CreatureHandGlows>(*entity) ? registry.Get<CreatureHandGlows>(*entity)
		                                                         : registry.Assign<CreatureHandGlows>(*entity);
		for (size_t place = 0; place < creature_hand_glow::k_PlaceCount; ++place)
		{
			creature_hand_glow::Set(glows.glows, static_cast<creature_hand_glow::Place>(place), 1.0f,
			                        static_cast<float>(command.value));
		}
		result = fmt::format("four glows of power {}", command.value);
		break;
	}
	case Kind::GiveCreedPower:
	{
		// Each shipped script that gives a creed power gives this scale
		constexpr float k_ScriptScale = 5.0f;
		const auto place = creature_hand_glow::PlaceOf(static_cast<int32_t>(command.value));
		if (!place.has_value() || !Locator::creatureAnimationSystem::has_value())
		{
			result = "no such place";
			break;
		}
		Locator::creatureAnimationSystem::value().SetCreedPower(*entity, *place, k_ScriptScale, command.amount);
		result = fmt::format("a creed power of {} at place {}", command.amount, command.value);
		break;
	}
	case Kind::AttachScriptSound:
	{
		if (!Locator::soundTagSystem::has_value())
		{
			result = "no sound tags";
			break;
		}
		const auto sound = static_cast<entt::id_type>(command.value);
		const auto tag = Locator::soundTagSystem::value().AttachObjectSound(*entity, sound, false);
		result = fmt::format("sound {} attached as tag {}", sound, entt::to_integral(tag));
		break;
	}
	case Kind::WriteWebPage:
		if (Locator::creatureWebPageSystem::has_value())
		{
			Locator::creatureWebPageSystem::value().WritePage(*entity);
			Locator::creatureWebPageSystem::value().FlushWrites();
			result = "written";
		}
		else
		{
			result = "no web page system";
		}
		break;
	case Kind::BookmarkCreature:
		// The player keeps the creature in a bookmark, the hand over it, and the camera follows the bookmark
		if (Locator::cameraBookmarkSystem::has_value() && Locator::camera::has_value())
		{
			auto& bookmarks = Locator::cameraBookmarkSystem::value();
			const auto& camera = Locator::camera::value();
			bookmarks.SetBookmark(command.value, camera.GetFocus(), *entity, camera.GetOrigin());
			bookmarks.ToggleFollow(command.value);
			result = fmt::format("bookmark {} on the creature, followed", command.value + 1);
		}
		else
		{
			result = "no bookmarks";
		}
		break;
	case Kind::HandGive:
		result = Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GiveHeldTo(*entity)
		             ? "given"
		             : "the hand gives nothing";
		break;
	case Kind::SetHour:
	case Kind::HoldSeed:
	case Kind::DrawGesture:
	case Kind::SummonSeed:
	case Kind::PressKey:
	case Kind::HandTakeFireBall:
	case Kind::HandTapObject:
	case Kind::HandTake:
	case Kind::HandPour:
	case Kind::HandRelease:
	case Kind::AttachObjectScriptSound:
	case Kind::SayLine:
	case Kind::PlayGuidanceVoice:
	case Kind::ScriptRelease:
	case Kind::FillSiteIcons:
	case Kind::BuildWorshipSites:
	case Kind::ChargeIconWithSeedHeld:
	case Kind::RemoveObject:
	case Kind::FinishBuilding:
	case Kind::PauseGame:
	case Kind::SetGameSpeed:
	case Kind::RemoveTeleportStone:
	case Kind::MakeDance:
	case Kind::OpenMultiplayerBox:
	case Kind::OtherGodHand:
	case Kind::HandGripTotem:
	case Kind::SetAlignment:
	case Kind::Advisor:
	case Kind::DestroyTemple:
	case Kind::SetOpenClose:
	case Kind::LayGateStone:
	case Kind::WideScreen:
	case Kind::SetInterfaceLevel:
	case Kind::FlyToBookmark:
	case Kind::TabToTown:
	case Kind::PlayFallingSpellFilm:
	case Kind::PlayIntroFilm:
	case Kind::RunHelpMessage:
	case Kind::EndGuidanceWaits:
	case Kind::ProfileFolderCheck:
	case Kind::OwnTattooPicture:
	case Kind::OpenTattooMenu:
	case Kind::OpenControlsBox:
	case Kind::OpenMenuPage:
	case Kind::PressMenuKey:
	case Kind::PlayCustomMusic:
	case Kind::ShowCustomMusicControls:
	case Kind::SaveGame:
	case Kind::LoadGame:
	case Kind::QuickSave:
	case Kind::ContinueGame:
	case Kind::PlayLogoEffect:
	case Kind::StopScript:
	case Kind::StartScriptMusic:
	case Kind::RunText:
	case Kind::ReadTip:
	case Kind::LookAtTempleScroll:
	case Kind::PlayHandDemo:
	case Kind::Reframe:
	case Kind::RecordChallenge:
	case Kind::EnterTemple:
	case Kind::PlayBoatFilm:
	// The mouse commands are given before a creature is looked for
	case Kind::PointerTo:
	case Kind::PointerPress:
	case Kind::PointerRelease:
	case Kind::PointerSweep:
	case Kind::WheelTurn:
	case Kind::PointerOnOpponent:
	case Kind::PointerOnCaveCreature:
	case Kind::PointerOnCreature:
	case Kind::PointerOnAltar:
	case Kind::PointerOnRope:
	case Kind::PointerOnObject:
		break;
	case Kind::SetDesire:
	case Kind::SetPhase:
	case Kind::RewardIf:
	case Kind::SetMiracleSightings:
		result = TeachMind(*entity, command);
		break;
	case Kind::SetAlight:
	{
		// With a value of 1, the scenario's object catches fire in place of the creature
		const auto burning = command.value == 1 ? ObjectAt(command.object) : std::optional(*entity);
		if (Locator::fireSystem::has_value() && burning.has_value())
		{
			Locator::fireSystem::value().SetOnFire(*burning, 0.0f);
		}
		result = Locator::fireSystem::has_value() && burning.has_value() ? "alight" : "no fire";
		break;
	}
	case Kind::SetFizz:
		if (Locator::creatureFizzSystem::has_value())
		{
			Locator::creatureFizzSystem::value().SetFizz(*entity, command.amount, static_cast<float>(command.value) / 1000.0f,
			                                             false);
		}
		result = Locator::creatureFizzSystem::has_value() ? fmt::format("fizzes to {:.2f}", command.amount) : "no fizz";
		break;
	case Kind::SetFightLean:
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* found = registry.TryGet<ecs::components::CreatureFightRecord>(*entity);
		auto& record = found != nullptr ? *found : registry.Assign<ecs::components::CreatureFightRecord>(*entity);
		record.tendency = command.amount;
		result = fmt::format("leans {:+.2f}", record.tendency);
		break;
	}
	case Kind::SeeSkill:
		minds.SeeSkill(Locator::entitiesRegistry::value().Get<Transform>(*entity).position, command.value);
		break;
	case Kind::SeeMiracle:
		minds.SeeMiracle(Locator::entitiesRegistry::value().Get<Transform>(*entity).position, command.value);
		break;
	case Kind::KnowMiracle:
		minds.KnowMiracle(*entity, command.value);
		break;
	case Kind::CastMiracle:
	{
		const auto target = command.atCreature.has_value() ? CreatureAt(*command.atCreature) : ObjectAt(command.object);
		result = !target.has_value()                                                       ? "nothing to cast at"
		         : minds.TellCast(*entity, static_cast<MagicType>(command.value), *target) ? ""
		                                                                                   : "can't";
		break;
	}
	case Kind::PlayerDid:
	{
		const auto& land = Locator::terrainSystem::value();
		minds.PlayerDid(command.value, glm::vec3(point.x, land.GetHeightAt(point), point.y), std::nullopt, std::nullopt);
		break;
	}
	case Kind::PlayerDidOn:
	{
		const auto object = ObjectAt(command.object);
		const auto* at = object.has_value() ? Locator::entitiesRegistry::value().TryGet<const Transform>(*object) : nullptr;
		if (at == nullptr)
		{
			result = "no such object";
			break;
		}
		minds.PlayerDid(command.value, at->position, object, std::nullopt);
		break;
	}
	}
	Log(fmt::format("{:.1f}s: {} {}{}{}", _seconds, who, Name(command.kind), result.empty() ? "" : ": ", result));
}

void Runner::GiveSceneryCommand(const Command& command)
{
	const auto object = ObjectAt(command.object);
	if (!object.has_value() || !Locator::animatedStaticSystem::has_value())
	{
		Log(fmt::format("{:.1f}s: no such object", _seconds));
		return;
	}
	auto& scenery = Locator::animatedStaticSystem::value();
	if (command.kind == Kind::SetOpenClose)
	{
		const bool done = scenery.SetOpenState(*object, static_cast<int32_t>(command.value));
		Log(fmt::format("{:.1f}s: {} object {}{}", _seconds, command.value == 1 ? "opened" : "closed", command.object,
		                done ? "" : ", which isn't animated scenery"));
		return;
	}
	// As the hand gives it: laid in the plinth, the stone is used up, leaving its ghost
	const auto plinth = ObjectAt(command.value);
	if (plinth.has_value() && scenery.LayGateStone(*plinth, *object))
	{
		ecs::world_objects::LeaveGhost(*object);
		ecs::world_objects::Remove(*object);
		Log(fmt::format("{:.1f}s: object {} laid in the plinth, now worth {}", _seconds, command.object,
		                scenery.GateStoneValue(*plinth).value_or(0)));
		return;
	}
	Log(fmt::format("{:.1f}s: object {} isn't taken by object {}", _seconds, command.object, command.value));
}

std::string Runner::GiveCreatureModeCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureModeSystem::has_value() || !Locator::creatureCaveSystem::has_value())
	{
		return "no creature mode";
	}
	auto& mode = Locator::creatureModeSystem::value();
	auto& cave = Locator::creatureCaveSystem::value();
	const auto following = [&mode] {
		const auto followed = mode.GetCreature();
		return followed.has_value() ? fmt::format("following entity {}", static_cast<uint32_t>(*followed))
		                            : std::string("the camera is the player's");
	};
	switch (command.kind)
	{
	case Kind::CreatureKey:
		mode.PressCreatureKey();
		return following();
	case Kind::DoubleClick:
	{
		// Two presses of the left button on the creature, a fifth of a second apart
		const auto now = static_cast<uint32_t>(_seconds * 1000.0f) + 1;
		mode.Press({.milliseconds = now, .creature = creature});
		mode.Press({.milliseconds = now + 200, .creature = creature});
		return following();
	}
	case Kind::CameraKeys:
	{
		constexpr std::array<glm::ivec2, 4> k_Directions {{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};
		const auto direction = k_Directions.at(std::min<size_t>(command.value, k_Directions.size() - 1));
		mode.HoldKeys(direction.x, direction.y, !command.ctrl, command.ctrl, command.amount);
		return mode.IsActive() ? "" : "not in Creature Mode";
	}
	case Kind::ClearCameraView:
		mode.ClearView();
		if (const auto view = mode.GetView(); view.has_value())
		{
			return fmt::format("heading {:.2f}, pitch {:.2f}", view->yaw, view->pitch);
		}
		return "not in Creature Mode";
	case Kind::LeaveCreatureMode:
		mode.Leave();
		return following();
	case Kind::OpenCreatureCave:
		cave.Open();
		cave.GetScreen().page = static_cast<creature_cave::Page>(command.value);
		cave.GetScreen().pageRequested = true;
		return cave.GetCreature().has_value() ? "" : "the player has no creature";
	case Kind::ApplyTattoo:
		return cave.ApplyTattoo(static_cast<uint8_t>(command.bodyPart), static_cast<uint8_t>(command.value),
		                        glm::u8vec3(180, 30, 30))
		           ? ""
		           : "can't";
	case Kind::RemoveTattoo:
		return cave.RemoveTattoo(static_cast<uint8_t>(command.bodyPart)) ? "" : "nothing there";
	case Kind::OpenTattooEditor:
	{
		const auto caveCreature = cave.GetCreature();
		if (!caveCreature.has_value() || !Locator::tattooEditorSystem::has_value())
		{
			return "the player has no creature";
		}
		const auto& transform = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(*caveCreature);
		Locator::tattooEditorSystem::value().Open(*caveCreature,
		                                          creature_tattoo_editor::CaveView(transform.position, transform.scale.x));
		return "";
	}
	default:
		return {};
	}
}

std::string Runner::TeachMind(entt::entity entity, const Command& command)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(entity);
	if (mind == nullptr)
	{
		return "no mind";
	}
	switch (command.kind)
	{
	case Kind::SetDesire:
		if (mind->desires.has_value() && command.value < creature_desires::k_DesireCount)
		{
			auto& state = mind->desires->desires.at(command.value);
			state.activated = true;
			state.suppressedTurns = 0;
			state.value = command.amount * std::max(state.max, 0.0f);
			return fmt::format("{} {:.2f}", creature_desires::Name(static_cast<creature_desires::Desire>(command.value)),
			                   state.value);
		}
		return "no desires yet";
	case Kind::SetPhase:
		mind->developmentPhase = static_cast<uint32_t>(command.value);
		return fmt::format("stage {}", command.value);
	case Kind::SetMiracleSightings:
	{
		if (!mind->learnt.has_value())
		{
			return "nothing learnt yet";
		}
		auto& knowledge = mind->learnt->knowledge;
		if (command.value >= knowledge.miraclesSeen.size() || command.value >= knowledge.miraclesKnown.size())
		{
			return "no such miracle";
		}
		knowledge.miraclesSeen.at(command.value).count = static_cast<uint32_t>(command.amount);
		knowledge.miraclesKnown.at(command.value) = true;
		return fmt::format("miracle {} seen {} times", command.value, knowledge.miraclesSeen.at(command.value).count);
	}
	case Kind::RewardIf:
		// From now on each thing it does to something is judged as soon as it is done
		mind->trainer = static_cast<uint32_t>(command.value);
		return fmt::format("trained: stroked for a {}, slapped for anything else",
		                   creature_tree::BeliefName(static_cast<uint32_t>(command.value)));
	default:
		return {};
	}
}

void Runner::LoadMindFile(entt::entity entity, std::string_view name)
{
	constexpr std::string_view k_Chosen = "chosen:";
	constexpr std::string_view k_Game = "game:";
	std::filesystem::path gameFolder;
	if (Locator::filesystem::has_value())
	{
		gameFolder = Locator::filesystem::value().GetPath<filesystem::Path::CreatureMind>(true);
	}
	std::filesystem::path path;
	if (name.starts_with(k_Chosen))
	{
		// The mind file last opened with the spawner's file dialog, else the game's own mind of that name
		std::error_code error;
		const auto chosen = file_dialog::RememberedPath("creature-mind-file");
		if (chosen.has_value() && std::filesystem::is_regular_file(*chosen, error))
		{
			path = *chosen;
		}
		else if (!gameFolder.empty())
		{
			path = gameFolder / name.substr(k_Chosen.size());
		}
	}
	else if (name.starts_with(k_Game) && !gameFolder.empty())
	{
		path = gameFolder / name.substr(k_Game.size());
	}
	auto data = std::make_shared<creaturemind::MindFileData>();
	const auto result = path.empty() ? creaturemind::MindResult::ErrCantOpen : creaturemind::ReadFile(path, *data);
	if (result == creaturemind::MindResult::Success && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().LoadMind(entity, data);
	}
	Log(fmt::format("mind file {}: {}", path.empty() ? std::string(name) : path.filename().string(),
	                creaturemind::ResultToStr(result)));
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
	UpdateThrows();
	WatchGesture();
	FinishTakingFireBall();
	ApplyStates();
	UpdatePointer(seconds);
	const auto due = Advance(_timeline, _scenario->commands, _scenario->repeatFrom, seconds, [this](const Command& command) {
		const bool toHand =
		    command.kind == Kind::HoldSeed || command.kind == Kind::DrawGesture || command.kind == Kind::SummonSeed;
		if (toHand)
		{
			return !Locator::gestureSystem::has_value() || !Locator::gestureSystem::value().IsDrawingPath();
		}
		return IsFree(command.creature);
	});
	for (const auto index : due)
	{
		Give(_scenario->commands[index]);
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

void Runner::UpdateThrows()
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::entitiesRegistry::has_value() ||
	    !Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& dynamics = Locator::dynamicsSystem::value();
	for (size_t i = 0; i < _throwsDue.size() && i < _scenario->throws.size(); ++i)
	{
		auto& due = _throwsDue.at(i);
		if (!due.has_value() || _seconds < *due)
		{
			continue;
		}
		const auto& setup = _scenario->throws.at(i);
		const auto object = setup.object < _objects.size() ? _objects.at(setup.object) : entt::null;
		// What broke or went is thrown no more
		if (!registry.Valid(object) || !registry.AllOf<ecs::components::Transform>(object))
		{
			due.reset();
			continue;
		}
		// Still flying from the last throw: it is thrown once it lands
		if (registry.AllOf<ecs::components::InPhysics>(object))
		{
			continue;
		}
		auto& transform = registry.Get<ecs::components::Transform>(object);
		const auto point =
		    setup.from.has_value() ? MapPoint(_middle, *setup.from) : glm::vec2(transform.position.x, transform.position.z);
		transform.position = {point.x, Locator::terrainSystem::value().GetHeightAt(point) + setup.height, point.y};
		registry.SetDirty();
		const auto result = dynamics.LetGoFromHand(object, {.velocity = setup.velocity, .player = PlayerNames::PLAYER_ONE});
		if (!result.accepted)
		{
			Log(fmt::format("{:.1f}s: object {} wasn't thrown", _seconds, setup.object));
		}
		due = setup.repeatSeconds.has_value() ? std::optional(*due + *setup.repeatSeconds) : std::nullopt;
	}
}

void Runner::PlaceDispensers(const Scenario& scenario)
{
	if (!KeepsDispenserGrid(scenario))
	{
		testbed_dispensers::RemoveGrid();
	}
	if (!Locator::magicSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& magic = Locator::magicSystem::value();
	for (const auto& dispenser : scenario.dispensers)
	{
		const auto point = MapPoint(_middle, dispenser.offset);
		const float ground = Locator::terrainSystem::value().GetHeightAt(point);
		if (dispenser.bubbleHeight.has_value())
		{
			_globes.push_back(magic.CreateOneOffSeedFor({point.x, ground + *dispenser.bubbleHeight, point.y}, dispenser.type));
		}
		else if (dispenser.reward)
		{
			PlaceRewardDispenser(dispenser, {point.x, ground, point.y});
		}
		else
		{
			const auto made = magic.CreateDispenser({point.x, ground, point.y}, dispenser.type, 0.0f);
			if (dispenser.charged && made != entt::null)
			{
				magic.ChargeDispenser(made);
			}
		}
	}
}

void Runner::PlaceRewardDispenser(const DispenserSetup& setup, glm::vec3 position)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& magic = Locator::magicSystem::value();
	entt::entity town = entt::null;
	if (!setup.noTown)
	{
		const auto id = ScenarioTown(_middle);
		town = registry.Context().towns.at(id);
	}
	// As the reward script does: the dispenser, its miracle with the building's own period, then turned on
	const auto made = magic.CreateScriptDispenser(position, AbodeInfo::NorseSpellDispenser, 0.0f, 1.0f, town);
	if (made == entt::null)
	{
		return;
	}
	if (setup.noTown)
	{
		// The script gives a dispenser the player's first town when it names none, so take it from its town
		auto& dispenser = registry.Get<ecs::components::SpellDispenser>(made);
		if (registry.Valid(dispenser.town))
		{
			std::erase(registry.Get<ecs::components::Town>(dispenser.town).abodes, made);
			Locator::townSystem::value().StructuresChanged(dispenser.town);
		}
		dispenser.town = entt::null;
	}
	const auto& info = Locator::infoConstants::value().abode.at(static_cast<size_t>(AbodeInfo::NorseSpellDispenser));
	magic.SetDispenserMagic(made, setup.type, static_cast<uint32_t>(info.timeEachMobileObjectTakesToProduce));
	Log(fmt::format("dispenser: period {} turns, wood value {}, builders {}, desire to be repaired {}",
	                info.timeEachMobileObjectTakesToProduce, info.woodValue, info.maxVillagerNeededToBuild,
	                info.desireToBeRepaired));
	magic.SetDispenserActive(made, true);
	if (setup.globeTaken)
	{
		magic.Remove(registry.Get<const ecs::components::SpellDispenser>(made).orb);
	}
	if (setup.life < 1.0f)
	{
		ecs::world_objects::ReduceLife(made, 1.0f - setup.life, std::nullopt);
	}
}

void Runner::UpdateMiracles(float seconds)
{
	if (!Locator::magicSystem::has_value())
	{
		return;
	}
	auto& magic = Locator::magicSystem::value();
	for (size_t i = 0; i < _miracles.size(); ++i)
	{
		auto& miracle = _miracles.at(i);
		const auto& setup = _scenario->miracles.at(i);
		if (setup.byHand)
		{
			// A cast on a creature takes the creature under the pointer, so the pointer stays on the creature's body
			if (setup.target == MiracleCast::Target::Creature && (miracle.pressAt.has_value() || miracle.releaseAt.has_value()))
			{
				if (const auto creature = CreatureAt(setup.creature))
				{
					PointAtBody(*creature);
				}
			}
			// The button goes down once the hand is over the point, and comes up after the hold, the hand moving meanwhile
			if (miracle.pressAt.has_value() && _seconds >= *miracle.pressAt)
			{
				magic.PressAction();
				Log(fmt::format("Pressed: {}", HandResultName(magic.GetLastHandResult())));
				miracle.pressAt.reset();
				miracle.releaseAt = _seconds + setup.holdSeconds.value_or(k_HandThrowSeconds);
			}
			if (miracle.releaseAt.has_value())
			{
				miracle.hand += setup.throwVelocity * seconds;
				magic.DriveHand(HandFrameFor(i, miracle.hand));
				if (_seconds >= *miracle.releaseAt)
				{
					magic.ReleaseAction();
					Log(fmt::format("Let go: {}", HandResultName(magic.GetLastHandResult())));
					miracle.releaseAt.reset();
					if (setup.sweepSeconds.has_value())
					{
						miracle.sweepUntil = _seconds + *setup.sweepSeconds;
					}
					else
					{
						magic.DriveHand(std::nullopt);
					}
					// A seed kept in the hand is dropped, for the next go
					if (magic.IsHandBusy())
					{
						magic.DiscardHeldSeed();
					}
				}
			}
			if (miracle.sweepUntil.has_value())
			{
				// The hand sweeps on over the land as the flock pours out of it
				miracle.hand += setup.throwVelocity * seconds;
				magic.DriveHand(HandFrameFor(i, miracle.hand));
				if (_seconds >= *miracle.sweepUntil)
				{
					miracle.sweepUntil.reset();
					magic.DriveHand(std::nullopt);
				}
			}
			if (miracle.nextAt.has_value() && _seconds >= *miracle.nextAt)
			{
				StartHandCast(i);
				miracle.nextAt =
				    setup.repeatSeconds.has_value() ? std::optional(*miracle.nextAt + *setup.repeatSeconds) : std::nullopt;
			}
			continue;
		}
		if (miracle.letGoAt.has_value() && _seconds >= *miracle.letGoAt)
		{
			Locator::magicSystem::value().CloseDown(miracle.spell);
			miracle.letGoAt.reset();
		}
		if (miracle.breakAt.has_value() && _seconds >= *miracle.breakAt)
		{
			miracle.breakAt.reset();
			if (Locator::magicShieldSystem::has_value() && Locator::magicShieldSystem::value().HasObject(miracle.spell))
			{
				Locator::magicShieldSystem::value().ShieldStruck(miracle.spell, true);
				Locator::magicSystem::value().CloseDown(miracle.spell);
				Log("Shield broken");
			}
		}
		if (!miracle.nextAt.has_value() || _seconds < *miracle.nextAt)
		{
			continue;
		}
		miracle.spell = CastMiracle(i);
		Log(fmt::format("Cast {}{}", static_cast<int>(setup.type), miracle.spell == entt::null ? ", which failed" : ""));
		if (setup.holdSeconds.has_value() && miracle.spell != entt::null)
		{
			miracle.letGoAt = _seconds + *setup.holdSeconds;
		}
		if (setup.breakSeconds.has_value() && miracle.spell != entt::null)
		{
			miracle.breakAt = _seconds + *setup.breakSeconds;
		}
		miracle.nextAt = setup.repeatSeconds.has_value() ? std::optional(*miracle.nextAt + *setup.repeatSeconds) : std::nullopt;
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

entt::entity Runner::CastMiracle(size_t index)
{
	if (!Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	const auto& setup = _scenario->miracles.at(index);
	const auto& info = Locator::infoConstants::value();
	const auto& land = Locator::terrainSystem::value();
	auto& magic = Locator::magicSystem::value();
	// What a seed of it in the hand would cast: a sizing miracle at its usual size
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, setup.type).value_or(SpellSeedType::None);
	float size = 1.0f;
	if (const auto* radius = magic::GetMagicInfoAs<GMagicRadiusSpellInfo>(info, setup.type))
	{
		size = radius->radiusForNormalCost;
	}
	const auto cast = magic::SeedCastData(info, setup.type, seed, 1.0f, size);
	std::optional<entt::entity> target;
	auto point2 = MapPoint(_middle, setup.point);
	glm::vec3 point {point2.x, land.GetHeightAt(point2), point2.y};
	if (setup.target == MiracleCast::Target::Creature)
	{
		target = CreatureAt(setup.creature);
		if (!target.has_value())
		{
			return entt::null;
		}
		point = Locator::entitiesRegistry::value().Get<Transform>(*target).position;
	}
	const auto hand2 = MapPoint(_middle, setup.handOffset);
	const glm::vec3 hand {hand2.x, land.GetHeightAt(hand2) + setup.handHeight, hand2.y};
	// As a script casts it: the neutral player's, from the hand's place, on the creature when its seed is cast on things
	if (setup.script.has_value())
	{
		const auto scripted = magic::MakeScriptCast(magic::GetMagicEffectInfo(info, setup.type).initialChants, point, hand,
		                                            setup.script->radius, setup.script->seconds, setup.script->curl);
		const bool onObject =
		    target.has_value() && seed != SpellSeedType::None && magic::GetSpellSeedInfo(info, seed).castOnObject != 0;
		return onObject ? magic.CastOnObject(setup.type, PlayerNames::NEUTRAL, *target, scripted.cast, scripted.info)
		                : magic.CastAtPoint(setup.type, PlayerNames::NEUTRAL, scripted.point, scripted.cast, scripted.info);
	}
	const auto toward = point - hand;
	const particles::ProcessInfo process {
	    .handPosition = hand,
	    .cameraForward = glm::length(toward) > 0.0f ? glm::normalize(toward) : glm::vec3(0.0f, 0.0f, 1.0f),
	    .direction = setup.throwVelocity,
	};
	return target.has_value() ? magic.CastOnObject(setup.type, setup.player, *target, cast, process)
	                          : magic.CastAtPoint(setup.type, setup.player, point, cast, process);
}

std::optional<ecs::systems::MagicSystemInterface::HandFrame> Runner::HandFrameFor(size_t index, glm::vec3 hand) const
{
	const auto& setup = _scenario->miracles.at(index);
	const auto& land = Locator::terrainSystem::value();
	const auto point2 = MapPoint(_middle, setup.point);
	glm::vec3 point {point2.x, land.GetHeightAt(point2), point2.y};
	glm::vec3 aim = point;
	if (setup.target == MiracleCast::Target::Creature)
	{
		const auto creature = CreatureAt(setup.creature);
		if (!creature.has_value())
		{
			return std::nullopt;
		}
		point = Locator::entitiesRegistry::value().Get<Transform>(*creature).position;
		// At the middle of its body
		aim = point + glm::vec3(0.0f, k_HandAimHeight, 0.0f);
	}
	// Looking down at it from over the hand
	const auto eye = hand + glm::vec3(0.0f, k_HandEyeHeight, 0.0f);
	const auto toward = aim - eye;
	const auto direction = glm::length(toward) > 0.0f ? glm::normalize(toward) : glm::vec3(0.0f, -1.0f, 0.0f);
	return ecs::systems::MagicSystemInterface::HandFrame {
	    .handPosition = hand,
	    .point = point,
	    .rayOrigin = eye,
	    .rayDirection = direction,
	    .cameraForward = direction,
	    .overWorld = true,
	};
}

void Runner::StartHandCast(size_t index)
{
	if (!Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& setup = _scenario->miracles.at(index);
	auto& miracle = _miracles.at(index);
	const auto& info = Locator::infoConstants::value();
	auto& magic = Locator::magicSystem::value();
	if (magic.IsHandBusy())
	{
		magic.DiscardHeldSeed();
	}
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, setup.type);
	if (!seed.has_value())
	{
		return;
	}
	if (setup.casterAlignment.has_value() && Locator::alignmentSystem::has_value())
	{
		Locator::alignmentSystem::value().SetPlayerAlignment(PlayerNames::PLAYER_ONE, *setup.casterAlignment);
	}
	const auto level = magic::GetPowerUpGesture(magic::GetSpellSeedInfo(info, *seed), setup.type).level;
	const auto held = setup.fromWorship ? magic.SummonSeed(PlayerNames::PLAYER_ONE, *seed, level)
	                                    : magic.GiveSeedToHand(PlayerNames::PLAYER_ONE, *seed, level, 1.0f);
	if (held == entt::null)
	{
		Log("The hand couldn't take the seed");
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	const auto hand2 = MapPoint(_middle, setup.handOffset);
	miracle.hand = {hand2.x, land.GetHeightAt(hand2) + setup.handHeight, hand2.y};
	magic.DriveHand(HandFrameFor(index, miracle.hand));
	if (setup.circleRadius.has_value() && Locator::gestureEvents::has_value())
	{
		const auto point2 = MapPoint(_middle, setup.point);
		Locator::gestureEvents::value().Inject({.kind = ecs::systems::GestureEvent::Kind::Circle,
		                                        .gesture = GestureType::Circle,
		                                        .centre = {point2.x, land.GetHeightAt(point2), point2.y},
		                                        .radius = *setup.circleRadius,
		                                        .powerUpLevel = -1});
	}
	// A seed from worship waits until it is ready
	const float wait = setup.holdBeforePress.has_value() ? *setup.holdBeforePress
	                   : setup.fromWorship               ? info.spellSystem.delayBeforeSeedActive + k_HandPressWait
	                                                     : k_HandPressWait;
	miracle.pressAt = _seconds + wait;
	Log(fmt::format("Seed of {} in the hand{}", static_cast<int>(setup.type), setup.fromWorship ? ", from worship" : ""));
}

uint32_t Runner::StartParticle(size_t index) const
{
	if (!Locator::particleSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return ecs::systems::ParticleSystemInterface::k_NoEffect;
	}
	const auto& particle = _scenario->particles.at(index);
	const auto point = MapPoint(_middle, particle.offset);
	const glm::vec3 position {point.x, Locator::terrainSystem::value().GetHeightAt(point) + particle.height, point.y};
	auto& particles = Locator::particleSystem::value();
	const auto effect = particle.file.empty() ? particles.Start(particle.type, position, particle.magnitude)
	                                          : particles.Start(particle.file, position, particle.magnitude);
	particles.SetPlayer(effect, particle.player);
	particles.SetDrawPath(effect, particle.path);
	if (particle.targetsCreatures)
	{
		for (const auto creature : _creatures)
		{
			particles.AddTarget(effect, creature);
		}
	}
	return effect;
}

void Runner::WriteCrossingFile(std::span<const std::string_view> lines)
{
	// Where the profile keeps it, as a vortex out of the last land writes it
	if (!Locator::playerProfileSystem::has_value())
	{
		return;
	}
	const auto folder = Locator::playerProfileSystem::value().SavedGamesFolder();
	if (!folder.has_value())
	{
		return;
	}
	std::error_code error;
	std::filesystem::create_directories(*folder, error);
	std::ofstream file(*folder / "vortex.txt", std::ios::out | std::ios::trunc | std::ios::binary);
	for (const auto line : lines)
	{
		file << line << '\n';
	}
	Log(fmt::format("Wrote {} lines to the crossing file in {}", lines.size(), folder->string()));
}

void Runner::UpdateVortices()
{
	if (!Locator::vortexSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& vortices = Locator::vortexSystem::value();
	for (size_t i = 0; i < _vortices.size(); ++i)
	{
		const auto& setup = _scenario->vortices.at(i);
		auto& running = _vortices.at(i);
		if (running.vortex == entt::null && _seconds >= setup.delaySeconds)
		{
			const auto point = MapPoint(_middle, setup.offset);
			if (!setup.crossingLines.empty())
			{
				WriteCrossingFile(setup.crossingLines);
			}
			running.vortex = vortices.Create({point.x, 0.0f, point.y}, setup.type, 0.0f);
			if (setup.gatheringOffset.has_value())
			{
				const auto gathering = MapPoint(_middle, *setup.gatheringOffset);
				vortices.SetArrival(running.vortex, {.town = entt::null,
				                                     .gatheringPlace = {gathering.x, 0.0f, gathering.y},
				                                     .flockDistance = setup.flockDistance,
				                                     .flockRadius = setup.flockRadius,
				                                     .flock = entt::null});
			}
			Log(fmt::format("Vortex {} opened at {:.0f}, {:.0f}", i, point.x, point.y));
		}
		if (running.vortex != entt::null && !running.fading && setup.fadeOutAfterSeconds.has_value() &&
		    _seconds >= setup.delaySeconds + *setup.fadeOutAfterSeconds)
		{
			running.fading = vortices.StartFadeOut(running.vortex);
			Log(fmt::format("Vortex {} fading out", i));
		}
	}
}

void Runner::UpdateRewards()
{
	if (!Locator::rewardSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	for (size_t i = 0; i < _rewards.size(); ++i)
	{
		const auto& setup = _scenario->rewards.at(i);
		if (_rewards.at(i) != entt::null || _seconds < setup.delaySeconds)
		{
			continue;
		}
		const auto point = MapPoint(_middle, setup.offset);
		const glm::vec3 at {point.x, Locator::terrainSystem::value().GetHeightAt(point), point.y};
		const auto town =
		    setup.inTown ? Locator::entitiesRegistry::value().Context().towns.at(ScenarioTown(_middle)) : entt::null;
		_rewards.at(i) = Locator::rewardSystem::value().Create(at, setup.type, player, town, setup.fromSky);
		if (setup.opened && !setup.fromSky)
		{
			Locator::rewardSystem::value().Tap(_rewards.at(i), player);
		}
		Log(fmt::format("Reward chest {} (kind {}) given at {:.0f}, {:.0f}", i, static_cast<int>(setup.type), point.x,
		                point.y));
	}
}

void Runner::UpdateParticles(float seconds)
{
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	auto& particles = Locator::particleSystem::value();
	for (size_t i = 0; i < _particles.size(); ++i)
	{
		auto& running = _particles.at(i);
		const auto restart = _scenario->particles.at(i).restartSeconds;
		running.seconds += seconds;
		// An effect that has ended, or whose time is up, starts again
		if (!particles.IsRunning(running.effect) || (restart > 0.0f && running.seconds >= restart))
		{
			particles.CloseDown(running.effect);
			running = {StartParticle(i), 0.0f};
		}
	}
}

void Runner::LogMiracles()
{
	if (!_scenario->logMiraclesEvery.has_value() || _seconds < _nextMiracleLog || !Locator::terrainSystem::has_value())
	{
		return;
	}
	_nextMiracleLog = _seconds + *_scenario->logMiraclesEvery;
	const auto& land = Locator::terrainSystem::value();
	Locator::entitiesRegistry::value().Each<const ecs::components::Spell>(
	    [&](entt::entity, const ecs::components::Spell& spell) {
		    const glm::vec2 xz(spell.position.x, spell.position.z);
		    Log(fmt::format("{:.1f} s: miracle {} at ({:.2f}, {:.2f}), land {:.2f}", _seconds,
		                    static_cast<int>(spell.magicType), xz.x - _middle.x, xz.y - _middle.y, land.GetHeightAt(xz)));
	    });
}

namespace
{
glm::vec2 WindowSize()
{
	return Locator::windowing::has_value() ? glm::vec2(Locator::windowing::value().GetSize()) : glm::vec2(1.0f);
}

/// Sends a move of the pointer to where it is, by so much, with the buttons held, as the mouse does
void PushMotion(const input::GameActionInterface::ScriptedPointer& pointer, glm::ivec2 moved)
{
	SDL_Event event {};
	event.type = SDL_MOUSEMOTION;
	event.motion.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
	event.motion.state = pointer.buttons;
	event.motion.x = pointer.position.x;
	event.motion.y = pointer.position.y;
	event.motion.xrel = moved.x;
	event.motion.yrel = moved.y;
	input::MarkInjected(event);
	SDL_PushEvent(&event);
}
/// What the player's hand holds, for the log
std::string HeldByHand()
{
	if (!Locator::handGrabSystem::has_value())
	{
		return {};
	}
	const auto held = Locator::handGrabSystem::value().GetHeld();
	return held.has_value() ? fmt::format(", holding {}", static_cast<uint32_t>(*held)) : std::string {};
}
} // namespace

std::optional<glm::ivec2> Runner::OpponentOnScreen(size_t creature, glm::vec2 size) const
{
	const auto self = CreatureAt(creature);
	if (!self.has_value() || !Locator::creatureFightSystem::has_value() || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto opponent = Locator::creatureFightSystem::value().OpponentOf(*self);
	if (!opponent.has_value())
	{
		return std::nullopt;
	}
	return BodyOnScreen(*opponent, size);
}

std::optional<glm::ivec2> Runner::BodyOnScreen(entt::entity creature, glm::vec2 size)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::camera::has_value() || !registry.AllOf<ecs::components::Transform, ecs::components::Creature>(creature))
	{
		return std::nullopt;
	}
	// The middle of the body is half the height of the creature above its feet
	const auto& feet = registry.Get<const ecs::components::Transform>(creature).position;
	const auto height = creature_feedback::k_HeightAtSizeOne *
	                    ecs::components::ShownSize(registry.Get<const ecs::components::Creature>(creature));
	glm::vec3 screen;
	if (!Locator::camera::value().ProjectWorldToScreen(feet + glm::vec3(0.0f, height * 0.5f, 0.0f),
	                                                   {0.0f, 0.0f, size.x, size.y}, screen))
	{
		return std::nullopt;
	}
	return glm::ivec2(glm::round(glm::vec2(screen)));
}

std::optional<glm::ivec2> Runner::AltarOnScreen(glm::vec2 size) const
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(k_ScenarioTownId);
	if (found == towns.end() || !registry.Valid(found->second) || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto site = registry.Get<const ecs::components::Town>(found->second).worshipSite;
	const auto* worship = registry.Valid(site) ? registry.TryGet<const ecs::components::WorshipSite>(site) : nullptr;
	if (worship == nullptr || !registry.Valid(worship->altar))
	{
		return std::nullopt;
	}
	// The middle of the altar's foot: the line from the camera to it meets the altar's low top first
	const auto& foot = registry.Get<const ecs::components::Transform>(worship->altar).position;
	glm::vec3 screen;
	if (!Locator::camera::value().ProjectWorldToScreen(foot, {0.0f, 0.0f, size.x, size.y}, screen))
	{
		return std::nullopt;
	}
	return glm::ivec2(glm::round(glm::vec2(screen)));
}

std::optional<glm::ivec2> Runner::ObjectOnScreen(size_t object, float height, glm::vec2 size) const
{
	const auto entity = ObjectAt(object);
	if (!entity.has_value() || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(*entity);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	glm::vec3 screen;
	if (!Locator::camera::value().ProjectWorldToScreen(transform->position + glm::vec3(0.0f, height, 0.0f),
	                                                   {0.0f, 0.0f, size.x, size.y}, screen))
	{
		return std::nullopt;
	}
	return glm::ivec2(glm::round(glm::vec2(screen)));
}

std::optional<glm::ivec2> Runner::RopeOnScreen(size_t creature, glm::vec2 size) const
{
	const auto entity = CreatureAt(creature);
	if (!entity.has_value() || !Locator::camera::has_value() || !Locator::leashSystem::has_value())
	{
		return std::nullopt;
	}
	const auto* leash = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureLeash>(*entity);
	const auto* drawn = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureLeashRope>(*entity);
	if (leash == nullptr || !leash->worn.has_value() || drawn == nullptr)
	{
		return std::nullopt;
	}
	// The rope runs from the collar to the thing it is tied to, and it sags below the straight line between them. The
	// points of the straight line are tried from its middle out, away from the collar and the thing. Below each point
	// on the screen, the first pixel that touches the rope is taken.
	const auto& camera = Locator::camera::value();
	const auto& rope = drawn->rope;
	auto& leashes = Locator::leashSystem::value();
	constexpr int k_Steps = 16;
	constexpr int k_MostPixelsBelow = 200;
	for (int tried = 0; tried < k_Steps - 1; ++tried)
	{
		const int step = (k_Steps / 2) + ((tried % 2 == 0) ? tried / 2 : -((tried + 1) / 2));
		const auto point = rope.end + (rope.start - rope.end) * (static_cast<float>(step) / static_cast<float>(k_Steps));
		glm::vec3 screen;
		if (!camera.ProjectWorldToScreen(point, {0.0f, 0.0f, size.x, size.y}, screen))
		{
			continue;
		}
		const auto pixel = glm::ivec2(glm::round(glm::vec2(screen)));
		for (int below = 0; below < k_MostPixelsBelow; ++below)
		{
			const ecs::systems::LeashSystemInterface::RopeFrame frame {
			    .viewProjection = camera.GetViewProjectionMatrix(), .screenSize = size, .cursor = pixel + glm::ivec2(0, below)};
			if (leashes.RopeUnderCursor(PlayerNames::PLAYER_ONE, frame).has_value())
			{
				return frame.cursor;
			}
		}
	}
	return std::nullopt;
}

std::optional<glm::ivec2> Runner::CaveCreatureOnScreen(glm::vec2 size)
{
	if (!Locator::creatureCaveSystem::has_value() || !Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto& cave = Locator::creatureCaveSystem::value();
	const auto copy = cave.GetCaveCreature();
	if (!copy.has_value() || !Locator::temple::has_value())
	{
		return std::nullopt;
	}
	// The copy is a thing of the temple's rooms, and its creature is a thing of the land
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& copies = Locator::temple::value().GetRooms();
	if (!copies.Valid(*copy))
	{
		return std::nullopt;
	}
	const auto* transform = copies.TryGet<const ecs::components::Transform>(*copy);
	const auto* original = copies.TryGet<const ecs::components::CreatureCopy>(*copy);
	const auto* mesh = original != nullptr && registry.Valid(original->creature)
	                       ? registry.TryGet<const ecs::components::Mesh>(original->creature)
	                       : nullptr;
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (transform == nullptr || mesh == nullptr || !meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	// From the creature's feet up to the middle of its body's bounding box, the first pixel where the cave finds the
	// creature under the pointer, as it is posed now
	const auto& camera = Locator::camera::value();
	const auto model = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	const auto middle = glm::vec3(model * glm::vec4(meshes.Handle(mesh->id)->GetBoundingBox().Center(), 1.0f));
	glm::vec3 feet;
	glm::vec3 top;
	const glm::vec4 viewport {0.0f, 0.0f, size.x, size.y};
	if (!camera.ProjectWorldToScreen(transform->position, viewport, feet) ||
	    !camera.ProjectWorldToScreen(middle, viewport, top))
	{
		return std::nullopt;
	}
	constexpr int k_Steps = 64;
	for (int step = 0; step <= k_Steps; ++step)
	{
		const auto pixel = glm::round(glm::mix(glm::vec2(feet), glm::vec2(top), static_cast<float>(step) / k_Steps));
		const auto [eye, near] = camera.OnNearPlane(pixel / size);
		if (cave.CursorOverCaveCreature({
		        .worldToClip = camera.GetViewProjectionMatrix(),
		        .resolution = size,
		        .near = camera.GetNearClip(),
		        .xScale = camera.GetProjectionMatrix()[0][0],
		        .camera = eye,
		        .cursor = pixel,
		    }))
		{
			return glm::ivec2(pixel);
		}
	}
	return std::nullopt;
}

std::string Runner::GivePointerCommand(const Command& command)
{
	if (!Locator::gameActionSystem::has_value())
	{
		return "no controls";
	}
	auto& actions = Locator::gameActionSystem::value();
	const auto size = WindowSize();
	auto pointer = actions.GetScriptedPointer().value_or(input::GameActionInterface::ScriptedPointer {
	    .position = glm::ivec2(actions.GetMousePosition()),
	});
	KeepDebugWindowsOffTheMouse(true);
	_ownsPointer = true;
	switch (command.kind)
	{
	case Kind::PointerOnOpponent:
	{
		const auto to = OpponentOnScreen(command.creature, size);
		if (!to.has_value())
		{
			return "no opponent on the screen";
		}
		const auto moved = *to - pointer.position;
		pointer.position = *to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerOnCreature:
	{
		const auto self = CreatureAt(command.creature);
		const auto to = self.has_value() ? BodyOnScreen(*self, size) : std::nullopt;
		if (!to.has_value())
		{
			return "no creature on the screen";
		}
		const auto moved = *to - pointer.position;
		pointer.position = *to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerOnCaveCreature:
	{
		const auto to = CaveCreatureOnScreen(size);
		if (!to.has_value())
		{
			return "no creature in the cave on the screen";
		}
		const auto moved = *to - pointer.position;
		pointer.position = *to;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerOnRope:
	{
		const auto to = RopeOnScreen(command.creature, size);
		if (!to.has_value())
		{
			return "no tied rope on the screen";
		}
		const auto moved = *to - pointer.position;
		pointer.position = *to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerOnAltar:
	{
		const auto to = AltarOnScreen(size);
		if (!to.has_value())
		{
			return "no altar on the screen";
		}
		const auto moved = *to - pointer.position;
		pointer.position = *to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerOnObject:
	{
		const auto to = ObjectOnScreen(command.object, command.amount, size);
		if (!to.has_value())
		{
			return "no object on the screen";
		}
		const auto moved = *to - pointer.position;
		pointer.position = *to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerTo:
	{
		const auto to = glm::ivec2(glm::round(command.point * size));
		const auto moved = to - pointer.position;
		pointer.position = to;
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		break;
	}
	case Kind::PointerPress:
	case Kind::PointerRelease:
	{
		const bool press = command.kind == Kind::PointerPress;
		// A press waits for the game to know what the pointer is over, so it acts as a press of a hand at rest
		if (press && !input::pointer_settle::PressMayStart(_framesSincePointerMoved))
		{
			_pressWaiting = command;
			break;
		}
		const auto button = static_cast<uint8_t>(command.value);
		pointer.buttons = press ? (pointer.buttons | SDL_BUTTON(button)) : (pointer.buttons & ~SDL_BUTTON(button));
		actions.SetScriptedPointer(pointer);
		SDL_Event event {};
		event.type = press ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
		event.button.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
		event.button.button = button;
		event.button.state = press ? SDL_PRESSED : SDL_RELEASED;
		event.button.clicks = 1;
		event.button.x = pointer.position.x;
		event.button.y = pointer.position.y;
		input::MarkInjected(event);
		SDL_PushEvent(&event);
		_handWatchSeconds = 0.5f;
		break;
	}
	case Kind::PointerSweep:
		_framesSincePointerMoved = 0;
		actions.SetScriptedPointer(pointer);
		_sweep = PointerSweep {.pixelsPerSecond = command.point * size / command.amount, .secondsLeft = command.amount};
		break;
	case Kind::WheelTurn:
	{
		actions.SetScriptedPointer(pointer);
		SDL_Event event {};
		event.type = SDL_MOUSEWHEEL;
		event.wheel.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
		event.wheel.y = static_cast<int32_t>(command.value) * (command.ctrl ? -1 : 1);
		event.wheel.preciseY = static_cast<float>(event.wheel.y);
		input::MarkInjected(event);
		SDL_PushEvent(&event);
		break;
	}
	default:
		break;
	}
	return HandOnScreen();
}

void Runner::UpdatePointer(float seconds)
{
	++_framesSincePointerMoved;
	if (_pressWaiting.has_value() && input::pointer_settle::PressMayStart(_framesSincePointerMoved))
	{
		const auto waiting = std::exchange(_pressWaiting, std::nullopt);
		static_cast<void>(GivePointerCommand(*waiting));
	}
	if (!Locator::gameActionSystem::has_value())
	{
		return;
	}
	auto& actions = Locator::gameActionSystem::value();
	// Once the commands are done and the buttons let go, the mouse is the player's again
	if (const auto pointer = actions.GetScriptedPointer(); _ownsPointer && pointer.has_value() && pointer->buttons == 0 &&
	                                                       !_sweep.has_value() && _timeline.done && _handWatchSeconds <= 0.0f)
	{
		actions.SetScriptedPointer(std::nullopt);
		KeepDebugWindowsOffTheMouse(false);
		_ownsPointer = false;
	}
	if (_handWatchSeconds > 0.0f || _sweep.has_value())
	{
		_handWatchSeconds -= seconds;
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Testbed {:.3f}s: {}", _seconds, HandOnScreen());
	}
	if (_sweep.has_value())
	{
		_framesSincePointerMoved = 0;
		auto pointer = actions.GetScriptedPointer().value_or(input::GameActionInterface::ScriptedPointer {});
		const auto step = std::min(seconds, _sweep->secondsLeft);
		const auto exact = _sweep->pixelsPerSecond * step + _sweep->remainder;
		const auto moved = glm::ivec2(exact);
		_sweep->remainder = exact - glm::vec2(moved);
		// While the camera turns with the mouse its pointer is held, and only the movement comes through
		if (!actions.IsCursorFrozen())
		{
			pointer.position = glm::clamp(pointer.position + moved, glm::ivec2(0), glm::ivec2(WindowSize()) - 1);
		}
		actions.SetScriptedPointer(pointer);
		PushMotion(pointer, moved);
		_sweep->secondsLeft -= step;
		if (_sweep->secondsLeft <= 0.0f)
		{
			_sweep.reset();
			const auto line = fmt::format("{:.2f}s: moved: {}", _seconds, HandOnScreen());
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
			Log(line);
		}
	}
}

std::string Runner::HandOnScreen() const
{
	if (!Locator::gameActionSystem::has_value() || !Locator::handSystem::has_value() || !Locator::camera::has_value())
	{
		return {};
	}
	const auto cursor = Locator::gameActionSystem::value().GetMousePosition();
	const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
	const auto& transform = Locator::entitiesRegistry::value().Get<Transform>(hand);
	const auto& position = transform.position;
	const auto size = WindowSize();
	glm::vec3 screen {0.0f};
	Locator::camera::value().ProjectWorldToScreen(position, {0.0f, 0.0f, size.x, size.y}, screen);
	const auto& camera = Locator::camera::value();
	const auto cues = camera.GetModel().GetHandCues();
	constexpr std::array<std::string_view, 4> k_DragModes {"pan", "edge rotate", "pitch", "pitch from the top"};
	const auto drag = !cues.dragging              ? std::string_view("none")
	                  : cues.dragMode.has_value() ? k_DragModes.at(static_cast<size_t>(*cues.dragMode))
	                                              : std::string_view("undecided");
	return fmt::format("cursor ({}, {}), hand ({:.0f}, {:.0f}) at ({:.1f}, {:.1f}, {:.1f}), {:.1f} from the camera, scale "
	                   "{:.3f}, hints {:#x}, drag {}, camera heading {:.3f} pitch {:.3f}{}{}",
	                   cursor.x, cursor.y, screen.x, screen.y, position.x, position.y, position.z,
	                   glm::distance(position, camera.GetOrigin()), transform.scale.y, cues.tricons, drag,
	                   camera.GetRotation().y, camera.GetRotation().x,
	                   Locator::gameActionSystem::value().IsCursorFrozen() ? ", held" : "", HeldByHand());
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

std::optional<CrowdProgress> Runner::GetCrowdProgress() const
{
	if (_scenario == nullptr || !_scenario->crowd.has_value())
	{
		return std::nullopt;
	}
	return _crowdProgress;
}

std::span<const benchmark::StageInfo> Runner::GetStages() const
{
	if (_recorder == nullptr)
	{
		return {};
	}
	return _recorder->Stages();
}

uint32_t Runner::GetWarmUpLeft() const
{
	return _settledFrames >= _benchmark.warmUpFrames ? 0 : _benchmark.warmUpFrames - _settledFrames;
}

void Runner::SpawnCrowd()
{
	if (_scenario == nullptr || !_scenario->crowd.has_value() || _crowdProgress.Done() ||
	    !Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto start = std::chrono::steady_clock::now();
	const auto batch = std::min(_scenario->crowd->perFrame, _crowdProgress.total - _crowdNext);
	for (size_t i = 0; i < batch; ++i)
	{
		SpawnCrowdMember(_crowdNext++);
	}
	_crowdProgress.spawned = _crowdNext;
	_crowdProgress.spawnMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	++_crowdProgress.spawnFrames;
	if (_crowdProgress.Done())
	{
		Log(fmt::format("Spawned {} in {:.0f} ms over {} frames", _crowdProgress.total, _crowdProgress.spawnMs,
		                _crowdProgress.spawnFrames));
	}
}

void Runner::SpawnCrowdMember(size_t index)
{
	const auto& land = Locator::terrainSystem::value();
	const auto onLand = [&land, this](glm::vec2 offset) {
		const auto point = MapPoint(_middle, offset);
		return glm::vec3(point.x, land.GetHeightAt(point), point.y);
	};
	if (!_crowdCreatures.empty())
	{
		const auto& member = _crowdCreatures.at(index);
		_crowdEntities.push_back(CreatureArchetype::Create(onLand(member.offset), member.owner, member.species, 0,
		                                                   glm::radians(member.facingDegrees), k_DefaultSize,
		                                                   CreatureArchetype::StartBody(member.species)));
		return;
	}
	// The towns first, then their homes and stores, then the villagers who live in them
	const auto towns = _village.towns.size();
	const auto abodes = _village.abodes.size();
	if (index < towns)
	{
		const auto& town = _village.towns.at(index);
		_crowdEntities.push_back(
		    ecs::archetypes::TownArchetype::Create(static_cast<int>(index), onLand(town.offset), town.owner, town.tribe));
		return;
	}
	if (index < towns + abodes)
	{
		const auto& abode = _village.abodes.at(index - towns);
		_crowdAbodes.push_back(ecs::archetypes::AbodeArchetype::Create(static_cast<uint32_t>(abode.town), onLand(abode.offset),
		                                                               abode.type, glm::radians(abode.yawDegrees), 1.0f,
		                                                               k_CrowdFood, k_CrowdWood));
		return;
	}
	const auto& member = _village.villagers.at(index - towns - abodes);
	const auto& home = _village.abodes.at(member.abode);
	const auto entity =
	    ecs::archetypes::VillagerArchetype::Create(onLand(home.offset), onLand(member.offset), member.type, member.age);
	_crowdEntities.push_back(entity);
	// It lives in the home laid out for it, rather than the first in its town with room
	auto& registry = Locator::entitiesRegistry::value();
	const auto abode = _crowdAbodes.at(member.abode);
	const auto& townIds = registry.Context().towns;
	const auto town = townIds.find(static_cast<uint32_t>(home.town));
	if (abode != entt::null && registry.Valid(abode) && town != townIds.end())
	{
		Locator::townSystem::value().AddVillagerToAbode(abode, entity);
		registry.Get<ecs::components::Villager>(entity).town = town->second;
	}
}

void Runner::Measure()
{
	if (_recorder == nullptr || !_crowdProgress.Done() || !Locator::profiler::has_value())
	{
		return;
	}
	if (_settledFrames < _benchmark.warmUpFrames)
	{
		++_settledFrames;
		return;
	}
	// The last frame, whole: it ended as this one started
	const auto& profiler = Locator::profiler::value();
	const auto& entry = profiler.GetEntries().at(profiler.GetEntryIndex(-1));
	const auto frame = entry.frameEnd - entry.frameStart;
	const auto& draw = entry.stages.at(static_cast<size_t>(Profiler::Stage::SceneDraw));
	const bool drawn = draw.finalized && draw.start >= entry.frameStart && draw.start <= entry.frameEnd;
	const auto update = drawn ? draw.start - entry.frameStart : frame;
	std::array<float, static_cast<size_t>(Profiler::Stage::_count)> stages {};
	for (size_t i = 0; i < stages.size(); ++i)
	{
		stages.at(i) = static_cast<float>(Milliseconds(entry.stages.at(i).total));
	}
	// What the renderer last drew, the frame before
	const auto* renderStats = bgfx::getStats();
	_recorder->Add(static_cast<float>(Milliseconds(frame)), static_cast<float>(Milliseconds(update)),
	               static_cast<float>(Milliseconds(frame - update)), stages,
	               renderStats != nullptr ? static_cast<float>(renderStats->numDraw) : 0.0f);
	if (++_framesSinceSummary >= k_FramesPerSummary || _recorder->Count() == _recorder->Capacity())
	{
		_framesSinceSummary = 0;
		_liveResults = _recorder->Summarise();
	}
	if (_benchmark.autoSave.has_value() && !_saved && _recorder->Count() >= _recorder->Capacity())
	{
		_saved = true;
		_liveResults = _recorder->Summarise();
		const auto written = SaveResults(_benchmark.autoSave);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Benchmark {}: frame mean {:.2f} ms p95 {:.2f} max {:.2f}; results {}",
		                   _scenario->id, _liveResults.frame.average, _liveResults.frame.p95, _liveResults.frame.max,
		                   written.has_value() ? written->generic_string() : "not written");
		if (auto* game = Game::Instance(); game != nullptr)
		{
			game->RequestQuit();
		}
	}
}

std::vector<std::pair<std::string, size_t>> Runner::EntityCounts() const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return {};
	}
	auto& registry = Locator::entitiesRegistry::value();
	return {
	    {"placed", registry.Size<Transform>()},
	    {"creatures", registry.Size<Creature>()},
	    {"villagers", registry.Size<ecs::components::Villager>()},
	    {"abodes", registry.Size<ecs::components::Abode>()},
	    {"towns", registry.Size<ecs::components::Town>()},
	};
}

std::optional<std::filesystem::path> Runner::SaveResults(std::optional<std::filesystem::path> base)
{
	if (_scenario == nullptr || _recorder == nullptr || _recorder->Count() == 0)
	{
		return std::nullopt;
	}
	if (!base.has_value())
	{
		const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm local {};
#ifdef _WIN32
		localtime_s(&local, &now);
#else
		localtime_r(&now, &local);
#endif
		base = std::filesystem::path("benchmarks") / fmt::format("{}_{}_{:04}{:02}{:02}_{:02}{:02}{:02}", _scenario->id,
		                                                         k_Build, local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
		                                                         local.tm_hour, local.tm_min, local.tm_sec);
	}
	benchmark::RunInfo run {
	    .scenarioId = std::string(_scenario->id),
	    .scenarioName = std::string(_scenario->name),
	    .build = std::string(k_Build),
	    .crowd = _crowdProgress.total,
	    .spawnMs = _crowdProgress.spawnMs,
	    .spawnFrames = _crowdProgress.spawnFrames,
	    .warmUpFrames = _benchmark.warmUpFrames,
	    .entityCounts = EntityCounts(),
	};
	if (Locator::windowing::has_value())
	{
		const auto size = Locator::windowing::value().GetSize();
		run.width = static_cast<uint32_t>(size.x);
		run.height = static_cast<uint32_t>(size.y);
	}
	std::error_code error;
	if (base->has_parent_path())
	{
		std::filesystem::create_directories(base->parent_path(), error);
	}
	auto jsonPath = *base;
	jsonPath += ".json";
	auto csvPath = *base;
	csvPath += ".csv";
	std::ofstream json(jsonPath, std::ios::binary);
	std::ofstream csv(csvPath, std::ios::binary);
	if (!json || !csv)
	{
		Log(fmt::format("Couldn't write {}", jsonPath.generic_string()));
		return std::nullopt;
	}
	const auto results = _recorder->Summarise();
	json << benchmark::ToJson(run, results, _recorder->Stages());
	csv << benchmark::ToCsv(run, results, _recorder->Stages());
	Log(fmt::format("Saved {}", jsonPath.generic_string()));
	return jsonPath;
}

std::string Runner::PlayBoatFilm(const Command& command) const
{
	if (!Locator::boatLaunchSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return "no boat film";
	}
	const auto dock = MapPoint(_middle, command.point);
	// The sailing clip starts the boat about 67 metres west and 59 south of where it is played from: played from there,
	// the boat sets off from the lake's north-east corner and crosses it to the south west
	constexpr glm::vec2 k_SailingClipStart {-67.3f, -59.4f};
	const auto corner = flat_land::k_LakeCentre + flat_land::k_LakeHalfExtent;
	const auto sailing = corner - k_SailingClipStart;
	const ecs::systems::BoatLaunchSystemInterface::Places places {
	    .dock = {dock.x, Locator::terrainSystem::value().GetHeightAt(dock), dock.y},
	    .sailingStart = {sailing.x, 0.0f, sailing.y},
	};
	Locator::boatLaunchSystem::value().Play(places);
	return fmt::format("the explorers' boat film plays from ({:.0f}, {:.0f})", dock.x, dock.y);
}

std::string Runner::RecordChallenge(const Command& command) const
{
	if (!Locator::terrainSystem::has_value())
	{
		return "no land";
	}
	// The camera is put over the place first, and the picture is of the frame drawn from there
	const auto at = MapPoint(_middle, command.point);
	const auto focus = glm::vec3(at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y);
	const auto position = focus + glm::vec3(0.0f, 50.0f, 70.0f);
	auto& camera = Locator::camera::value();
	camera.SetOrigin(position);
	camera.SetFocus(focus);
	script::challenge_snapshots::Snapshot snapshot;
	snapshot.challenge = static_cast<uint32_t>(command.value);
	snapshot.reminderScript = "";
	snapshot.success = command.amount;
	snapshot.alignment = command.alignment;
	snapshot.position = position;
	snapshot.focus = focus;
	snapshot.quest = command.ctrl;
	auto* game = Game::Instance();
	if (game != nullptr && game->GetInterface() != nullptr)
	{
		const auto names = game->GetInterface()->GetTexts().GetHelpNames();
		if (const auto found = std::ranges::find(names, command.title); found != names.end())
		{
			snapshot.title = static_cast<uint32_t>(std::distance(names.begin(), found));
		}
	}
	chlapi::CHLApi::RecordChallenge(snapshot, script::challenge_snapshots::Call::Start);
	return fmt::format("challenge {} ({}), {:.0f}% done{}", command.value, command.title, command.amount * 100.0f,
	                   command.ctrl ? ", a quest" : "");
}

std::string Runner::GiveAdvisorCommand(const Command& command) const
{
	if (!Locator::advisorSystem::has_value())
	{
		return "no advisors";
	}
	auto& control = Locator::advisorSystem::value().GetController();
	// The scripts name the good advisor 1 and the evil one 2
	const int32_t type = command.value == 0 ? 1 : 2;
	const auto who = command.value == 0 ? "the good advisor" : "the evil advisor";
	using Action = Command::AdvisorAction;
	switch (command.advisor)
	{
	case Action::Out:
		control.SpiritEject(type, false);
		return fmt::format("{} comes out", who);
	case Action::Appear:
		control.SpiritEject(type, true);
		return fmt::format("{} appears", who);
	case Action::Home:
		control.SpiritHome(type, false);
		return fmt::format("{} goes home", who);
	case Action::Vanish:
		control.SpiritHome(type, true);
		return fmt::format("{} vanishes", who);
	case Action::Cling:
		control.SpiritCling(type, command.point.x, command.point.y);
		return fmt::format("{} clings at ({:.2f}, {:.2f})", who, command.point.x, command.point.y);
	case Action::Fly:
		control.SpiritFly(type, command.point.x, command.point.y);
		return fmt::format("{} flies to ({:.2f}, {:.2f})", who, command.point.x, command.point.y);
	case Action::PointOnScreen:
	{
		const auto& screen = control.GetScreen();
		control.SpiritScreenPoint(type, glm::ivec2(static_cast<int32_t>(static_cast<float>(screen.width) * command.point.x),
		                                           static_cast<int32_t>(static_cast<float>(screen.height) * command.point.y)));
		return fmt::format("{} points at ({:.2f}, {:.2f}) on the screen", who, command.point.x, command.point.y);
	}
	case Action::PointAtLand:
	case Action::LookAtLand:
	{
		const auto at = MapPoint(_middle, command.point);
		const float height = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
		const glm::vec3 position(at.x, height, at.y);
		if (command.advisor == Action::LookAtLand)
		{
			control.SpiritLookAtPosition(type, position);
			return fmt::format("{} looks at ({:.0f}, {:.0f})", who, at.x, at.y);
		}
		control.SpiritPointPosition(type, position, command.gentle);
		return fmt::format("{} points at ({:.0f}, {:.0f}){}", who, at.x, at.y, command.gentle ? " out in the world" : "");
	}
	case Action::PlayAnim:
		control.SpiritPlayAnim(type, command.point.x, command.point.y, command.anim, command.amount);
		return fmt::format("{} plays {}", who, help::spirits::AnimName(command.anim));
	case Action::Feel:
		control.Dude(type == 1 ? help::spirits::k_GoodDude : help::spirits::k_EvilDude).SetEmotion(command.anim, 1.0f);
		return fmt::format("{} feels {}", who, help::spirits::EmotionName(command.anim));
	}
	return "";
}
