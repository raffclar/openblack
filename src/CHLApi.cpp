/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CHLApi.h"

#include <cctype>
#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <chrono>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraEdits.h"
#include "3D/DayNightClock.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ScreenPick.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameMusic.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/ScriptSoundEffect.h"
#include "Audio/Sound.h"
#include "Camera/Camera.h"
#include "Camera/ScriptCameraModel.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/LeashRules.h"
#include "Creature/TemplePen.h"
#include "ECS/Archetypes/BallArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Archetypes/WhaleArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptFlock.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownAggression.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/Whale.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/DanceRules.h"
#include "ECS/Dances.h"
#include "ECS/Map.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFind.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/ScriptPopulate.h"
#include "ECS/ScriptSpotVisuals.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDance.h"
#include "ECS/Systems/Implementations/VillagerScript.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TownDesire.h"
#include "ECS/TownPlaythings.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerScriptRules.h"
#include "ECS/WorldObjects.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Hand/HandClickRules.h"
#include "Help/DialogueText.h"
#include "Help/ScriptSpirits.h"
#include "Help/Spirits.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/ScriptCast.h"
#include "Physics/Body.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptNameLists.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::chlapi
{

using namespace openblack::ecs::archetypes;

PlayerNames ScriptPlayerName(int32_t scriptPlayer);

using openblack::Locator;
using openblack::MobileStaticInfo;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::Transform;
using openblack::ecs::systems::HandSystemInterface;
using openblack::lhvm::DataType;
using openblack::lhvm::VMValue;
using openblack::script::ObjectType;

#define CREATE_FUNCTION_BINDING(NAME, STACKIN, STACKOUT, FUNCTION)       \
	{                                                                    \
		_functionsTable.emplace_back(FUNCTION, STACKIN, STACKOUT, NAME); \
	}

const std::vector<lhvm::NativeFunction>& CHLApi::GetFunctionsTable()
{
	return _functionsTable;
}

std::unordered_set<std::string> GetUniqueWords(const std::string& strings)
{
	std::unordered_set<std::string> result;
	std::istringstream iss(strings);
	std::string word;
	while (std::getline(iss, word, ' '))
	{
		result.insert(word);
	}
	return result;
}

/// The task running the native now, 0 between tasks
uint32_t CurrentTask()
{
	return Locator::vm::value().GetCurrentTaskNumber();
}

/// The kind of script a task runs, none for a task that has gone
lhvm::ScriptType TaskType(uint32_t task)
{
	const auto& tasks = Locator::vm::value().GetTasks();
	const auto found = tasks.find(task);
	return found != tasks.end() ? found->second.type : lhvm::ScriptType::None;
}

/// A script's mistake the game tells of, and carries on
void ScriptMessage(std::string_view message)
{
	SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "{}", message);
}

/// Nothing outside is in view while the player is inside the temple
bool PlayerInsideTemple()
{
	return Locator::temple::has_value() && Locator::temple::value().Active();
}

/// The task whose cinema bars are in, 0 for none
uint32_t WideScreenOwner()
{
	return Locator::cinematicDirectorSystem::value().GetWideScreenOwner();
}

/// The script's camera the camera commands move: with a warning when the script has none
ScriptCameraModel* ScriptCamera()
{
	auto* camera = Locator::scriptControlSystem::value().GetScriptCamera(Locator::camera::value());
	if (camera == nullptr)
	{
		ScriptMessage("We are in the wrong camera mode! - exception happened?");
	}
	return camera;
}

/// A thing a script's camera follows or looks at, as the camera sees it while the thing is there: where it is drawn,
/// raised by half its height, and the way it faces when it walks
ScriptCameraModel::ThingLookup FollowedThing(entt::entity thing)
{
	return [thing]() -> std::optional<script_camera::FollowedThing> {
		const auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(thing))
		{
			return std::nullopt;
		}
		const auto* transform = registry.TryGet<const Transform>(thing);
		if (transform == nullptr)
		{
			return std::nullopt;
		}
		// TODO(opening): a creature's height is its body's, and a flock is followed at its middle and its leader's
		// height
		float height = 0.0f;
		if (const auto* mesh = registry.TryGet<const ecs::components::Mesh>(thing); mesh != nullptr)
		{
			const auto& meshes = Locator::resources::value().GetMeshes();
			if (meshes.Contains(mesh->id))
			{
				height = meshes.Handle(mesh->id)->GetBoundingBox().Size().y * std::abs(transform->scale.y);
			}
		}
		const auto* wallHug = registry.TryGet<const ecs::components::WallHug>(thing);
		return script_camera::FollowedThing {
		    .point = transform->position + glm::vec3(0.0f, height * 0.5f, 0.0f),
		    .gameAngle = wallHug != nullptr ? std::optional(wallHug->gameAngle) : std::nullopt,
		    .height = height,
		};
	};
}

/// The camera commands that move it warn of a script doing so in the temple
ScriptCameraModel* ScriptCameraToMove()
{
	if (PlayerInsideTemple())
	{
		ScriptMessage("Script moving camera in citadel");
	}
	return ScriptCamera();
}

/// Leashes are drawn again once a script gives the camera back, and not while it has it
void DrawLeashes(bool drawn)
{
	if (Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().SetDrawn(drawn);
	}
}

/// The task with the dialogue gives it back: the cinema bars go, and the advisors are sent home
void ReleaseDialogue(uint32_t task)
{
	if (Locator::dialogueControlSystem::value().Release(task, TaskType(task) == lhvm::ScriptType::Help))
	{
		Locator::cinematicDirectorSystem::value().SetWideScreen(false, 0);
	}
}

PlayerNames ScriptPlayerName(int32_t scriptPlayer);

/// The advisors the scripts drive, nothing before they are loaded
help::spirits::AdvisorSpiritController* ScriptAdvisors()
{
	if (!Locator::advisorSystem::has_value() || !Locator::advisorSystem::value().IsLoaded())
	{
		return nullptr;
	}
	return &Locator::advisorSystem::value().GetController();
}

/// The advisor a script's spirit means, 1 the good one or 2 the evil one, by the local player's alignment for those
/// that go by it
int32_t ScriptHelpSpirit(int32_t scriptSpirit)
{
	const auto alignment = Locator::alignmentSystem::has_value()
	                           ? Locator::alignmentSystem::value().GetPlayerAlignment(ScriptPlayerName(0))
	                           : 0.0f;
	return help::script_spirits::HelpSpiritOf(scriptSpirit, help::script_spirits::DiscreteAlignment(alignment),
	                                          []() { return Locator::gameRandom::value().LocalRand(100); });
}

/// The last of the advisors' animations a script can play
constexpr int32_t k_LastSpiritAnim = 80;

/// A script's place on the screen, as fractions across and down: each out of 0 to 1 is reported, down first, and the
/// script goes on
void CheckScreenFractions(float x, float y)
{
	if (!help::script_spirits::IsScreenFraction(y))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid Y");
	}
	if (!help::script_spirits::IsScreenFraction(x))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid X");
	}
}

glm::vec3 PopVec()
{
	auto& lhvm = Locator::vm::value();
	const auto z = lhvm.Popf();
	const auto y = lhvm.Popf();
	const auto x = lhvm.Popf();
	return {x, y, z};
}

void PushVec(const glm::vec3& vec)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushv(vec.x);
	lhvm.Pushv(vec.y);
	lhvm.Pushv(vec.z);
}

std::string PopString()
{
	auto& lhvm = Locator::vm::value();
	return lhvm.GetString(lhvm.Pop().intVal);
}

std::vector<float> PopVarArg(const int32_t argc)
{
	std::vector<float> vals;
	vals.resize(argc);
	auto& lhvm = Locator::vm::value();
	for (int i = argc - 1; i >= 0; i--)
	{
		vals[i] = lhvm.Popf();
	}
	return vals;
}

/// Whether a thing is one of the world's objects, which a miracle can be cast on, rather than something with only a place,
/// such as a town or a miracle: anything with a model but the hand, a creature, or a field
bool IsScriptObject(const ecs::Registry& registry, entt::entity thing)
{
	return (registry.AnyOf<ecs::components::Mesh>(thing) && !registry.AnyOf<ecs::components::Hand>(thing)) ||
	       registry.AnyOf<ecs::components::Creature, ecs::components::Field>(thing);
}

/// A football made by a script goes to the nearest town, however far, for its people to play with; a town that already
/// has a football still about keeps that one
entt::entity CreateScriptBall(const glm::vec3& position)
{
	const auto ball = BallArchetype::Create(position);
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<ecs::town_playthings::Candidate> towns;
	registry.Each<const ecs::components::Town, const ecs::components::Transform>(
	    [&towns](entt::entity entity, const ecs::components::Town& town, const ecs::components::Transform& transform) {
		    towns.push_back({.town = entity, .owner = town.owner, .id = town.id, .position = transform.position});
	    });
	const auto nearest = ecs::town_playthings::Nearest(towns, position);
	if (!nearest.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Ball Created but unable to add it to a town.");
		return ball;
	}
	auto& town = registry.Get<ecs::components::Town>(*nearest);
	ecs::town_playthings::Add(town.playthings, ball, [&registry](entt::entity thing) {
		return registry.Valid(thing) && registry.AllOf<ecs::components::Ball>(thing);
	});
	return ball;
}

/// A villager a script makes: a grown-up just past growing up, or a child of ten, standing waiting for the script
entt::entity CreateScriptVillager(bool child, uint32_t subtype, const glm::vec3& position)
{
	const auto& infos = Locator::infoConstants::value().villager;
	if (subtype >= infos.size())
	{
		ScriptMessage("Thing not created");
		return entt::null;
	}
	constexpr uint32_t k_ChildAge = 10;
	const auto& info = infos.at(subtype);
	const uint32_t age = child ? k_ChildAge : info.grownUpAge + 1;
	// TODO(opening): the game makes one of its special villagers instead, now and then, when one fits
	const auto villager = VillagerArchetype::Create(position, position, static_cast<VillagerInfo>(subtype), age);
	if (Locator::livingActionSystem::has_value())
	{
		Locator::livingActionSystem::value().VillagerSetScriptState(villager, VillagerStates::InScript);
	}
	return villager;
}

entt::entity CreateScriptObject(const ObjectType type, uint32_t subtype, const glm::vec3& position, float altitude,
                                float xAngleRadians, float yAngleRadians, const float zAngleRadians, const float scale)
{
	// TODO(Daniels118): handle all types
	switch (type)
	{
	case ObjectType::MobileStatic:
		return MobileStaticArchetype::Create(position, static_cast<MobileStaticInfo>(subtype), altitude, xAngleRadians,
		                                     yAngleRadians, zAngleRadians, scale);
	case ObjectType::Rock:
	{
		// A rock is a mobile static the scripts know as a rock
		const auto rock = MobileStaticArchetype::Create(position, static_cast<MobileStaticInfo>(subtype), altitude,
		                                                xAngleRadians, yAngleRadians, zAngleRadians, scale);
		if (rock != entt::null)
		{
			Locator::entitiesRegistry::value().Assign<ecs::components::Rock>(rock);
		}
		return rock;
	}
	case ObjectType::Ball:
		return CreateScriptBall(position);
	case ObjectType::Marker:
		return ScriptMarkerArchetype::Create(position);
	case ObjectType::Villager:
	case ObjectType::VillagerChild:
		return CreateScriptVillager(type == ObjectType::VillagerChild, subtype, position);
	case ObjectType::Whale:
		return WhaleArchetype::Create(position, scale);
	case ObjectType::Vortex:
	{
		// A vortex of the three kinds; the game makes nothing for any other
		if (subtype > static_cast<uint32_t>(VortexType::Volcano))
		{
			break;
		}
		const auto vortex = Locator::vortexSystem::value().Create(position, static_cast<VortexType>(subtype), altitude);
		return vortex != entt::null ? vortex : static_cast<entt::entity>(0);
	}
	case ObjectType::Animal:
	case ObjectType::Bird:
		// Made on its own and held still for the script; openblack makes only the land's birds so far
		if (Locator::animalSystem::has_value())
		{
			const auto animal = Locator::animalSystem::value().CreateScriptAnimal(static_cast<AnimalInfo>(subtype),
			                                                                      glm::vec2(position.x, position.z));
			if (animal != entt::null)
			{
				return animal;
			}
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "CreateScriptObject not implemented for animal kind {}", subtype);
		return static_cast<entt::entity>(0);
	default:
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "CreateScriptObject not implemented for type {}", static_cast<int>(type));
	}
	return entt::null;
}

VMValue Pop(DataType& type)
{
	auto& lhvm = Locator::vm::value();
	return lhvm.Pop(type);
}

VMValue Pop()
{
	auto& lhvm = Locator::vm::value();
	return lhvm.Pop();
}

float Popf()
{
	auto& lhvm = Locator::vm::value();
	return lhvm.Popf();
}

/// An object a script made takes a place in the scripts' table as made by a script, so the script controls it from
/// its first reference
void RegisterCreated(entt::entity object)
{
	if (object != entt::null && Locator::entitiesRegistry::value().Valid(object))
	{
		Locator::scriptObjects::value().Register(object, true);
	}
}

/// An object given to a native: a native that takes control of what it is given takes control of it
entt::entity PopObject()
{
	// A script's object 0 is none: the native was given nothing, or a creation that failed
	const auto id = Pop().uintVal;
	if (id == 0)
	{
		return entt::null;
	}
	return Locator::scriptObjects::value().Fetch(static_cast<entt::entity>(id));
}

/// The villager a native is given, if it is one the living actions direct
bool IsDirectableVillager(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<ecs::components::Villager, ecs::components::LivingAction>(object) &&
	       Locator::livingActionSystem::has_value();
}

/// A script effect's seconds as game turns: a whole number of turns a second, as the game's turn length gives it
int SpecialEffectTurns(float seconds)
{
	constexpr auto k_TurnsPerSecond = 1000 / static_cast<int>(ecs::systems::TimeSystemInterface::k_TurnDuration.count());
	return static_cast<int>(seconds * static_cast<float>(k_TurnsPerSecond));
}

/// One of the spot visuals started by a script, at a point or on an object that ends it when it goes, for the player at
/// this computer; numbers past the table's last start nothing
uint32_t StartScriptSpotVisual(int32_t effect, glm::vec3 position, int turns, entt::entity owner)
{
	constexpr int32_t k_LastSpotVisual = 49;
	if (effect < 0 || effect > k_LastSpotVisual || !Locator::particleSystem::has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		return 0;
	}
	auto& particles = Locator::particleSystem::value();
	const auto id = particles.StartSpotVisual(static_cast<SpotVisualType>(effect), position, turns, owner, 1.0f);
	if (id == ecs::systems::ParticleSystemInterface::k_NoEffect)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		return 0;
	}
	if (Locator::playerSystem::has_value())
	{
		particles.SetPlayer(id, static_cast<int>(Locator::playerSystem::value().GetLocalPlayer()));
	}
	return id;
}

/// The thing a script holds for a visual it started, taking its place in the scripts' table as made by a script; none
/// when no visual was started. The script holds the thing, never the visual's own number.
static entt::entity ScriptSpotVisualThing(uint32_t effect, glm::vec3 position)
{
	if (effect == ecs::systems::ParticleSystemInterface::k_NoEffect)
	{
		return entt::null;
	}
	const auto thing = ecs::script_spot_visuals::MakeThing(Locator::entitiesRegistry::value(), effect, position);
	RegisterCreated(thing);
	return thing;
}

void Push(VMValue value, DataType type)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Push(value, type);
}

void Pushf(float value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushf(value);
}

void Pushv(float value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushv(value);
}

void Pushi(int32_t value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushi(value);
}

void Pusho(uint32_t value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pusho(value);
}

void Pushb(bool value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushb(value);
}

void CHLApi::TaskStopped(uint32_t task)
{
	// What the task held goes back: the dialogue, its cinema bars, the camera and the game's speed
	ReleaseDialogue(task);
	auto& director = Locator::cinematicDirectorSystem::value();
	if (director.IsWideScreenOn() && director.GetWideScreenOwner() == task)
	{
		director.SetWideScreen(false, 0);
	}
	const auto released = Locator::scriptControlSystem::value().TaskStopped(Locator::camera::value(), task);
	if (released.camera)
	{
		DrawLeashes(true);
	}
	if (released.gameSpeed)
	{
		Locator::time::value().SetSpeed(1.0f);
	}
}

/// An object handed to a script: none is the script's object 0
void PushObject(entt::entity object)
{
	Pusho(object == entt::null ? 0u : static_cast<uint32_t>(object));
}

CHLApi::CHLApi()
{
	_functionsTable.reserve(464);
	InitFunctionsTable0();
	InitFunctionsTable1();
	InitFunctionsTable2();
	InitFunctionsTable3();
	InitFunctionsTable4();
}

void CHLApi::ResetSwitches()
{
	_gameSoundOn = true;
	_highlightDrawOn = true;
	if (Locator::creatureAudioSystem::has_value())
	{
		Locator::creatureAudioSystem::value().SetOtherVoicesEnabled(true);
	}
}

void CHLApi::NotImplemented(std::optional<int32_t> detail)
{
	if (!_stubCalls.Record(_currentNative, detail))
	{
		return;
	}
	const auto name = _currentNative < _functionsTable.size() ? _functionsTable[_currentNative].name : "?";
	if (detail.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script native {} ({}) not implemented for {}; later calls are counted",
		                    name, _currentNative, *detail);
	}
	else
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script native {} ({}) not implemented; later calls are counted", name,
		                    _currentNative);
	}
}

void CHLApi::LogStubCalls() const
{
	const auto entries = _stubCalls.Entries();
	if (entries.empty())
	{
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "Unimplemented script natives called: {} calls of {} natives",
	                   _stubCalls.Total(), entries.size());
	for (const auto& entry : entries)
	{
		const auto name = entry.native < _functionsTable.size() ? _functionsTable[entry.native].name : "?";
		if (entry.detail.has_value())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "  {} calls of {} ({}) for {}", entry.calls, name, entry.native,
			                   *entry.detail);
		}
		else
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "  {} calls of {} ({})", entry.calls, name, entry.native);
		}
	}
}

namespace
{

/// The native being run isn't written yet, or not for this case of it
void NotImplemented(std::optional<int32_t> detail = std::nullopt)
{
	Locator::chlapi::value().NotImplemented(detail);
}

} // namespace

/// What the player at this computer last clicked, kept on their hand; none without a hand
ecs::components::HandClicked* LocalHandClicked()
{
	if (!Locator::handSystem::has_value())
	{
		return nullptr;
	}
	const auto hand = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	auto& registry = Locator::entitiesRegistry::value();
	if (hand == entt::null || !registry.Valid(hand))
	{
		return nullptr;
	}
	if (auto* clicked = registry.TryGet<ecs::components::HandClicked>(hand))
	{
		return clicked;
	}
	return &registry.Assign<ecs::components::HandClicked>(hand);
}

void None() {} // 000 NONE

void SetCameraPosition() // 001 SET_CAMERA_POSITION
{
	const auto position = PopVec();
	// Without the script's camera the position is quietly dropped
	if (auto* camera = Locator::scriptControlSystem::value().GetScriptCamera(Locator::camera::value()); camera != nullptr)
	{
		camera->SetOrigin(position);
	}
}

void SetCameraFocus() // 002 SET_CAMERA_FOCUS
{
	const auto position = PopVec();
	if (auto* camera = ScriptCameraToMove(); camera != nullptr)
	{
		camera->SetFocus(position);
	}
}

void MoveCameraPosition() // 003 MOVE_CAMERA_POSITION
{
	const auto time = Popf();
	const auto position = PopVec();
	if (auto* camera = ScriptCameraToMove(); camera != nullptr)
	{
		camera->MoveOrigin(position, time);
	}
}

void MoveCameraFocus() // 004 MOVE_CAMERA_FOCUS
{
	const auto time = Popf();
	const auto position = PopVec();
	if (auto* camera = ScriptCameraToMove(); camera != nullptr)
	{
		camera->MoveFocus(position, time);
	}
}

void GetCameraPosition() // 005 GET_CAMERA_POSITION
{
	auto& camera = Locator::camera::value();
	const auto position = camera.GetOrigin();
	PushVec(position);
}

void GetCameraFocus() // 006 GET_CAMERA_FOCUS
{
	auto& camera = Locator::camera::value();
	const auto focus = camera.GetFocus();
	PushVec(focus);
}

void SpiritEject() // 007 SPIRIT_EJECT
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		// A help script's advisor appears where it is rather than flying out
		advisors->SpiritEject(spirit, TaskType(CurrentTask()) == lhvm::ScriptType::Help);
	}
}

void SpiritHome() // 008 SPIRIT_HOME
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		// A help script's advisor vanishes rather than flying home
		advisors->SpiritHome(spirit, TaskType(CurrentTask()) == lhvm::ScriptType::Help);
	}
}

void SpiritPointPos() // 009 SPIRIT_POINT_POS
{
	// Only exactly 1 points in the world
	const auto inWorld = Pop().uintVal == 1;
	const auto position = PopVec();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritPointPosition(spirit, position, inWorld);
	}
}

void SpiritPointGameThing() // 010 SPIRIT_POINT_GAME_THING
{
	// const auto inWorld = static_cast<bool>(Pop().intVal);
	// const auto target = Pop().uintVal;
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

/// The view the scripts ask what is in: the camera as it is now and the window's size; none without a window
static std::optional<screen_pick::View> ScriptScreenView()
{
	if (!Locator::camera::has_value() || !Locator::windowing::has_value())
	{
		return std::nullopt;
	}
	const auto size = Locator::windowing::value().GetSize();
	if (size.x <= 0 || size.y <= 0)
	{
		return std::nullopt;
	}
	const auto& camera = Locator::camera::value();
	return screen_pick::View {
	    .worldToClip = camera.GetViewProjectionMatrix(),
	    .resolution = glm::vec2(size),
	    .near = camera.GetNearClip(),
	    .xScale = camera.GetProjectionMatrix()[0][0],
	    .camera = glm::vec3(glm::inverse(camera.GetViewMatrix(Camera::Interpolation::Current))[3]),
	};
}

/// Whether a thing shows on the screen: a thing with a model by its model's bounding sphere, anything else by the point
/// it stands at
static bool ThingOnScreen(const screen_pick::View& view, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(thing);
	if (transform == nullptr)
	{
		return false;
	}
	if (const auto* mesh = registry.TryGet<const ecs::components::Mesh>(thing); mesh != nullptr)
	{
		if (!Locator::resources::has_value() || !Locator::resources::value().GetMeshes().Contains(mesh->id))
		{
			return false;
		}
		const auto box = Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox();
		// The sphere about the model's box: its centre placed as the thing is, its radius the half diagonal times the
		// thing's size
		const auto centre = transform->position + transform->rotation * (transform->scale * box.Center());
		const float radius = glm::length(box.Size() * 0.5f) * transform->scale.x;
		return screen_pick::SphereOnScreen(view, centre, radius, transform->position);
	}
	// Where it stands, across the land as a map position holds it
	const auto at = glm::vec3(map_coords::Quantise(transform->position.x), transform->position.y,
	                          map_coords::Quantise(transform->position.z));
	return screen_pick::PointOnScreen(view, at);
}

void GameThingFieldOfView() // 011 GAME_THING_FIELD_OF_VIEW
{
	const auto object = PopObject();
	const bool valid = object != entt::null && Locator::entitiesRegistry::value().Valid(object);
	if (!valid)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object no longer valid");
	}
	const auto view = ScriptScreenView();
	Pushb(valid && !PlayerInsideTemple() && view.has_value() && ThingOnScreen(*view, object));
}

void PosFieldOfView() // 012 POS_FIELD_OF_VIEW
{
	const auto position = PopVec();
	const auto view = ScriptScreenView();
	Pushb(!PlayerInsideTemple() && view.has_value() && screen_pick::PointOnScreen(*view, position));
}

void RunText() // 013 RUN_TEXT
{
	const auto withInteraction = Pop().intVal;
	const auto text = Pop().uintVal;
	const auto singleLine = Pop().uintVal != 0;
	if (!Locator::helpTextSystem::value().RunText(singleLine, text, withInteraction))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid text");
	}
}

void TempText() // 014 TEMP_TEXT
{
	// const auto withInteraction = Pop().intVal;
	// const auto string = PopString();
	// const auto singleLine = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void TextRead() // 015 TEXT_READ
{
	Pushb(Locator::helpTextSystem::value().IsTextRead());
}

void GameThingClicked() // 016 GAME_THING_CLICKED
{
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object no longer valid");
		Pushb(false);
		return;
	}
	const auto* clicked = LocalHandClicked();
	// TODO(script-natives): a clicked challenge highlight is saved to the instant save slot first, once highlights and
	// saves exist
	Pushb(clicked != nullptr && hand_click::IsThingClicked(*clicked, object));
}

void SetScriptState() // 017 SET_SCRIPT_STATE
{
	const auto state = Pop().intVal;
	const auto object = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(object))
	{
		ScriptMessage("Object no longer valid");
		return;
	}
	if (IsDirectableVillager(object))
	{
		auto& living = Locator::livingActionSystem::value();
		if (state < 0 || state >= static_cast<int32_t>(VillagerStates::_COUNT) || !living.VillagerCanBeDirected(object))
		{
			ScriptMessage("Object not living for set state");
			return;
		}
		living.VillagerSetScriptState(object, static_cast<VillagerStates>(state));
		return;
	}
	// TODO(opening): creatures, animals and groups of things
	NotImplemented();
}

void SetScriptStatePos() // 018 SET_SCRIPT_STATE_POS
{
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto object = PopObject();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetScriptFloat() // 019 SET_SCRIPT_FLOAT
{
	[[maybe_unused]] const auto value = Popf();
	[[maybe_unused]] const auto object = PopObject();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetScriptUlong() // 020 SET_SCRIPT_ULONG
{
	// A count of -1 plays the clip without end
	const auto plays = Pop().uintVal;
	const auto animation = Pop().intVal;
	const auto object = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(object))
	{
		ScriptMessage("Object no longer valid");
		return;
	}
	if (IsDirectableVillager(object))
	{
		Locator::livingActionSystem::value().VillagerSetScriptAnimation(object, static_cast<AnimId>(animation), plays);
		return;
	}
	// TODO(opening): creatures and groups of things
	NotImplemented();
}

/// Whether something is drowning, as the scripts ask: a villager while it is in its drowning state, anything else
/// while it is in the physics with its body's centre under the sea's level
static bool IsDrowning(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		const auto* action = registry.TryGet<const ecs::components::LivingAction>(object);
		return action != nullptr && Locator::livingActionSystem::has_value() &&
		       Locator::livingActionSystem::value().VillagerGetState(*action, ecs::components::LivingAction::Index::Top) ==
		           VillagerStates::Drowning;
	}
	if (!registry.AllOf<ecs::components::InPhysics>(object) || !Locator::dynamicsSystem::has_value())
	{
		return false;
	}
	const auto* entry = Locator::dynamicsSystem::value().Find(object);
	return entry != nullptr && entry->body != nullptr && entry->body->Centre().y < 0.0f;
}

/// Whether a town is wholly destroyed: none of its buildings stands
static bool TownCompletelyDestroyed(const ecs::components::Town& town)
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::vector<script::property_rules::TownBuilding> buildings;
	buildings.reserve(town.abodes.size());
	for (const auto abode : town.abodes)
	{
		if (!registry.Valid(abode))
		{
			continue;
		}
		const auto* progress = registry.TryGet<const ecs::components::BuildProgress>(abode);
		buildings.push_back({
		    .life = ecs::world_objects::LifeOf(abode),
		    .field = registry.AllOf<ecs::components::Field>(abode),
		    .built = progress != nullptr ? progress->built : 1.0f,
		});
	}
	return script::property_rules::TownCompletelyDestroyed(buildings);
}

/// A creature's body as the scripts read and set it, none for anything else
static creature_physiology::Needs* CreatureNeedsOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<ecs::components::Creature>(object))
	{
		return nullptr;
	}
	auto* needs = registry.TryGet<ecs::components::CreatureNeeds>(object);
	return needs != nullptr ? &needs->needs : nullptr;
}

/// The need of a creature a property is, none for the properties that aren't
static std::optional<script::property_rules::CreatureNeed> NeedOf(script::ObjectPropertyType prop)
{
	using script::ObjectPropertyType;
	using script::property_rules::CreatureNeed;
	switch (prop)
	{
	case ObjectPropertyType::CreatureWarmth:
		return CreatureNeed::Warmth;
	case ObjectPropertyType::CreatureEnergy:
		return CreatureNeed::Energy;
	case ObjectPropertyType::CreatureItchiness:
		return CreatureNeed::Itchiness;
	case ObjectPropertyType::CreatureAmountOfPoo:
		return CreatureNeed::Poo;
	case ObjectPropertyType::CreatureExhaustion:
		return CreatureNeed::Exhaustion;
	case ObjectPropertyType::CreatureDehydration:
		return CreatureNeed::Dehydration;
	default:
		return std::nullopt;
	}
}

static float& NeedValue(creature_physiology::Needs& needs, script::property_rules::CreatureNeed need)
{
	using script::property_rules::CreatureNeed;
	switch (need)
	{
	case CreatureNeed::Warmth:
		return needs.warmth;
	case CreatureNeed::Energy:
		return needs.energy;
	case CreatureNeed::Itchiness:
		return needs.itchiness;
	case CreatureNeed::Poo:
		return needs.poo;
	case CreatureNeed::Exhaustion:
		return needs.exhaustion;
	case CreatureNeed::Dehydration:
	default:
		return needs.dehydration;
	}
}

/// Whether a thing is one of the world's objects, which have a life, angles and a height; a town isn't
static bool IsWorldObject(entt::entity object)
{
	return !Locator::entitiesRegistry::value().AllOf<ecs::components::Town>(object);
}

/// Whether a living thing: its angle is kept by its walking, not by how it was placed
static bool IsLivingThing(entt::entity object)
{
	return Locator::entitiesRegistry::value()
	    .AnyOf<ecs::components::Villager, ecs::components::Creature, ecs::components::Animal>(object);
}

/// The way a living thing faces, as the scripts take it: radians from +x towards +z
static float LivingAngleOf(entt::entity object, const Transform& transform)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* walk = registry.TryGet<const ecs::components::WallHug>(object); walk != nullptr)
	{
		return walk->yAngle;
	}
	if (const auto* animal = registry.TryGet<const ecs::components::Animal>(object); animal != nullptr)
	{
		return animal->heading;
	}
	if (const auto* locomotion = registry.TryGet<const ecs::components::CreatureLocomotion>(object);
	    locomotion != nullptr && locomotion->started)
	{
		return script::property_rules::CreatureHeadingToLivingAngle(locomotion->heading);
	}
	// Standing still, a living thing is turned as it is drawn: its model faces the other way round, a quarter turn on
	return script::property_rules::CreatureHeadingToLivingAngle(-script::property_rules::PlacedAngles(transform.rotation).y);
}

/// Turns a living thing to face a way, given as the scripts take it: its walk, if it has one, faces that way from now
static void TurnLivingThing(entt::entity object, Transform& transform, float angle)
{
	auto& registry = Locator::entitiesRegistry::value();
	const float heading = script::property_rules::LivingAngleToCreatureHeading(angle);
	if (auto* walk = registry.TryGet<ecs::components::WallHug>(object); walk != nullptr)
	{
		walk->yAngle = angle;
		walk->gameAngle = static_cast<uint16_t>(gutils::ConvertAngle3DToGame(angle));
	}
	if (auto* animal = registry.TryGet<ecs::components::Animal>(object); animal != nullptr)
	{
		animal->heading = angle;
		animal->previousHeading = angle;
	}
	if (auto* locomotion = registry.TryGet<ecs::components::CreatureLocomotion>(object); locomotion != nullptr)
	{
		locomotion->heading = heading;
		locomotion->targetHeading = heading;
		locomotion->fromHeading = heading;
		locomotion->toHeading = heading;
	}
	transform.rotation = glm::mat3(glm::eulerAngleY(heading));
	registry.SetDirty();
}

/// Whether a thing may lean, so that its angles across and forward are its own; the others always stand upright
static bool CanLean(entt::entity object)
{
	return Locator::entitiesRegistry::value()
	    .AnyOf<ecs::components::MobileStatic, ecs::components::MobileObject, ecs::components::Pot>(object);
}

/// Whether a script holds the thing while it owns the widescreen bars
static bool HeldDuringCutscene(entt::entity object)
{
	if (!Locator::entitiesRegistry::value().AllOf<ecs::components::InScript>(object) ||
	    !Locator::cinematicDirectorSystem::has_value())
	{
		return false;
	}
	const auto& director = Locator::cinematicDirectorSystem::value();
	return director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
}

/// How tall an object stands to the scripts: a creature by its size, anything else by its model as it is scaled, none
/// without a model
static float ScriptHeightOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
	{
		return script::property_rules::CreatureHeight(creature->size);
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	const auto* mesh = registry.TryGet<const ecs::components::Mesh>(object);
	if (transform == nullptr || mesh == nullptr || !Locator::resources::has_value())
	{
		return 0.0f;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	return meshes.Handle(mesh->id)->GetBoundingBox().Size().y * transform->scale.y;
}

/// The town a totem stands in: the town of the town centre it stands on
static entt::entity TownOfTotem(entt::entity totem)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* totemComponent = registry.TryGet<const ecs::components::VillageTotem>(totem);
	const auto* abode = totemComponent != nullptr && registry.Valid(totemComponent->townCentre)
	                        ? registry.TryGet<const ecs::components::Abode>(totemComponent->townCentre)
	                        : nullptr;
	if (abode == nullptr)
	{
		return entt::null;
	}
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(abode->townId);
	return found != towns.end() && registry.Valid(found->second) ? found->second : entt::null;
}

/// The script asked for a property the thing doesn't have
static void CannotGetProperty(script::ObjectPropertyType prop)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Cannot Get Property {}", static_cast<int>(prop));
	Pushf(0.0f);
}

static void CannotSetProperty(script::ObjectPropertyType prop)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Cannot Set Property {}", static_cast<int>(prop));
}

void GetProperty() // 021 GET_PROPERTY
{
	const auto object = PopObject();
	const auto prop = static_cast<script::ObjectPropertyType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object no longer valid");
		Pushf(0.0f);
		return;
	}
	if (const auto need = NeedOf(prop); need.has_value())
	{
		auto* needs = CreatureNeedsOf(object);
		if (needs == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			Pushf(0.0f);
			return;
		}
		Pushf(NeedValue(*needs, *need));
		return;
	}
	switch (prop)
	{
	case script::ObjectPropertyType::Flying:
		// In the physics, thrown, dropped or knocked and not yet at rest
		Pushb(registry.AllOf<ecs::components::InPhysics>(object));
		return;
	case script::ObjectPropertyType::Drowning:
		Pushb(IsDrowning(object));
		return;
	case script::ObjectPropertyType::Scale:
		if (const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(object); transform != nullptr)
		{
			Pushf(transform->scale.x);
			return;
		}
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Speed:
		if (const auto* wallHug = Locator::entitiesRegistry::value().TryGet<const ecs::components::WallHug>(object);
		    wallHug != nullptr)
		{
			Pushf(ecs::villager_script_rules::WalkSpeedToScriptSpeed(wallHug->speed));
			return;
		}
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Age:
		// Its age in years; only living things have one
		if (const auto* person = registry.TryGet<const ecs::components::Villager>(object); person != nullptr)
		{
			Pushf(static_cast<float>(ecs::villager_age::AgeNow(*person)));
			return;
		}
		// TODO(opening): the ages of creatures and animals; other things have none ("not used on non living objects")
		NotImplemented(static_cast<int32_t>(prop));
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::InHand:
		// Held in a hand, as a number
		Pushf(registry.AllOf<ecs::components::InHand>(object) ? 1.0f : 0.0f);
		return;
	case script::ObjectPropertyType::Player:
	{
		const auto* town = registry.TryGet<const ecs::components::Town>(object);
		Pushf(script::property_rules::PlayerProperty(ecs::world_objects::PlayerOf(object),
		                                             town != nullptr && TownCompletelyDestroyed(*town)));
		return;
	}
	case script::ObjectPropertyType::BuiltPercentage:
		// How much of it is built, 0 to 1; anything that isn't built is whole
		if (const auto* progress = registry.TryGet<const ecs::components::BuildProgress>(object); progress != nullptr)
		{
			Pushf(progress->built);
			return;
		}
		Pushf(1.0f);
		return;
	case script::ObjectPropertyType::XPos:
	case script::ObjectPropertyType::YPos:
	case script::ObjectPropertyType::ZPos:
	{
		const auto* transform = registry.TryGet<const Transform>(object);
		if (transform == nullptr)
		{
			Pushf(0.0f);
			return;
		}
		const auto& at = transform->position;
		if (prop == script::ObjectPropertyType::YPos)
		{
			// How high above the land it is
			const float land =
			    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(at.x, at.z)) : 0.0f;
			Pushf(at.y - land);
			return;
		}
		// Across the land, as a map position holds it
		Pushf(map_coords::Quantise(prop == script::ObjectPropertyType::XPos ? at.x : at.z));
		return;
	}
	case script::ObjectPropertyType::Health:
		if (!IsWorldObject(object))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not an object");
			CannotGetProperty(prop);
			return;
		}
		Pushf(ecs::world_objects::LifeOf(object));
		return;
	case script::ObjectPropertyType::Moving:
	{
		// A creature moves while it is on its way somewhere, from being given where to go until it stops
		if (const auto* locomotion = registry.TryGet<const ecs::components::CreatureLocomotion>(object); locomotion != nullptr)
		{
			Pushb(locomotion->destination.has_value() && !locomotion->facingOnly &&
			      locomotion->motion != ecs::components::CreatureLocomotion::Motion::Standing);
			return;
		}
		// An animal moved across the land in its last turn
		if (const auto* animal = registry.TryGet<const ecs::components::Animal>(object); animal != nullptr)
		{
			Pushb(script::property_rules::MovedAcross(animal->previousPosition, animal->position));
			return;
		}
		// Things that aren't in the map's cells never move
		if (!IsWorldObject(object))
		{
			Pushb(false);
			return;
		}
		// TODO(script-natives): a villager moved in its last turn, and any other thing has moved from where it was made or
		// came to rest from the physics: openblack keeps neither position
		NotImplemented(static_cast<int32_t>(prop));
		Pushb(false);
		return;
	}
	case script::ObjectPropertyType::Death:
	{
		// What a dead villager died of; every other thing (and a living villager) answers none
		const auto* death = registry.TryGet<const ecs::components::VillagerDeath>(object);
		Pushf(death != nullptr && death->reason.has_value() ? static_cast<float>(*death->reason) : 0.0f);
		return;
	}
	case script::ObjectPropertyType::Angle:
	case script::ObjectPropertyType::XAngle:
	case script::ObjectPropertyType::ZAngle:
	{
		const auto* transform = registry.TryGet<const Transform>(object);
		if (!IsWorldObject(object) || transform == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not an object");
			CannotGetProperty(prop);
			return;
		}
		if (prop != script::ObjectPropertyType::Angle)
		{
			// Only the things that may lean have a lean; everything else stands upright
			const auto angles = script::property_rules::PlacedAngles(transform->rotation);
			const float lean = !CanLean(object) ? 0.0f : prop == script::ObjectPropertyType::XAngle ? angles.x : angles.z;
			Pushf(script::property_rules::AngleToScript(lean));
			return;
		}
		if (IsLivingThing(object))
		{
			Pushf(script::property_rules::AngleToScript(LivingAngleOf(object, *transform)));
			return;
		}
		Pushf(script::property_rules::AngleToScript(script::property_rules::PlacedAngles(transform->rotation).y));
		return;
	}
	case script::ObjectPropertyType::Strength:
	{
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(creature->strength);
			return;
		}
		// A miracle's globe or seed: what scales the prayer power and the time of what it casts
		if (const auto* globe = registry.TryGet<const ecs::components::OneOffSpellSeed>(object); globe != nullptr)
		{
			Pushf(globe->multiplier);
			return;
		}
		if (const auto* seed = registry.TryGet<const ecs::components::SpellSeed>(object); seed != nullptr)
		{
			Pushf(seed->castMultiplier);
			return;
		}
		// Anything else answers its life
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script-Did you want the strength of this?");
		Pushf(ecs::world_objects::LifeOf(object));
		return;
	}
	case script::ObjectPropertyType::Alignment:
	{
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(creature->alignment);
			return;
		}
		// Anything else its player's, none without a player
		const auto player = ecs::world_objects::PlayerOf(object);
		Pushf(player.has_value() && Locator::alignmentSystem::has_value()
		          ? Locator::alignmentSystem::value().GetPlayerAlignment(*player)
		          : 0.0f);
		return;
	}
	case script::ObjectPropertyType::Height:
		// A town's totem answers the share of its people it was last set to send to worship
		if (const auto* totem = registry.TryGet<const ecs::components::VillageTotem>(object); totem != nullptr)
		{
			Pushf(totem->ease.target);
			return;
		}
		Pushf(IsWorldObject(object) ? ScriptHeightOf(object) : 0.0f);
		return;
	case script::ObjectPropertyType::MaxHeight:
		// Nothing keeps a greatest height
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::CreatureMinSize:
	case script::ObjectPropertyType::CreatureMaxSize:
	{
		const auto* spells = registry.TryGet<const ecs::components::CreatureSpells>(object);
		const auto* creature = registry.TryGet<const ecs::components::Creature>(object);
		if (spells == nullptr || creature == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			CannotGetProperty(prop);
			return;
		}
		// What the size spells would take it to now: in a fight they go by its size
		const auto inFight =
		    registry.AllOf<ecs::components::CreatureFighting>(object) ? std::optional(creature->size) : std::nullopt;
		Pushf(prop == script::ObjectPropertyType::CreatureMinSize
		          ? creature_spells::SmallestSizeNow(spells->smallestSize, inFight)
		          : creature_spells::LargestSizeNow(spells->largestSize, inFight));
		return;
	}
	case script::ObjectPropertyType::CreatureFatness:
		if (const auto* creature = registry.TryGet<const ecs::components::Creature>(object); creature != nullptr)
		{
			Pushf(creature->fatness);
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotGetProperty(prop);
		return;
	default:
		// TODO(Daniels118): the other properties
		NotImplemented(static_cast<int32_t>(prop));
		Pushf(0.0f);
		return;
	}
}

void SetProperty() // 022 SET_PROPERTY
{
	const auto value = Popf();
	const auto object = PopObject();
	const auto prop = static_cast<script::ObjectPropertyType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object no longer valid");
		return;
	}
	if (const auto need = NeedOf(prop); need.has_value())
	{
		auto* needs = CreatureNeedsOf(object);
		if (needs == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
			return;
		}
		NeedValue(*needs, *need) = script::property_rules::SetNeed(*need, value);
		return;
	}
	if (prop == script::ObjectPropertyType::YPos)
	{
		// A highlight keeps the height it is given, whatever stands under it
		if (registry.AllOf<ecs::components::ScriptHighlight>(object))
		{
			Locator::scriptHighlightSystem::value().SetDrawHeight(object, value);
		}
		// How high above the land it is: it is drawn there at once
		if (auto* transform = registry.TryGet<Transform>(object); transform != nullptr)
		{
			const auto& at = transform->position;
			const float land =
			    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(at.x, at.z)) : 0.0f;
			transform->position.y = land + value;
		}
		return;
	}
	switch (prop)
	{
	case script::ObjectPropertyType::Health:
		if (!IsWorldObject(object))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not an object");
			CannotSetProperty(prop);
			return;
		}
		// A thing a script holds in a cutscene, or made indestructible, can't be brought to nearly no life
		ecs::world_objects::SetLife(
		    object, value,
		    script::property_rules::CanSetLife(value, HeldDuringCutscene(object),
		                                       registry.AllOf<ecs::components::Indestructible>(object)));
		return;
	case script::ObjectPropertyType::Angle:
	case script::ObjectPropertyType::XAngle:
	case script::ObjectPropertyType::ZAngle:
	{
		auto* transform = registry.TryGet<Transform>(object);
		if (!IsWorldObject(object) || transform == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not an object");
			CannotSetProperty(prop);
			return;
		}
		const float radians = script::property_rules::AngleFromScript(value);
		auto angles = script::property_rules::PlacedAngles(transform->rotation);
		if (!CanLean(object))
		{
			// Only its turn is its own: a lean asked for changes nothing
			if (prop != script::ObjectPropertyType::Angle)
			{
				return;
			}
			angles = {.x = 0.0f, .y = angles.y, .z = 0.0f};
		}
		if (IsLivingThing(object))
		{
			// Unless it already faces that way
			if (radians != LivingAngleOf(object, *transform))
			{
				TurnLivingThing(object, *transform, radians);
			}
			return;
		}
		(prop == script::ObjectPropertyType::Angle    ? angles.y
		 : prop == script::ObjectPropertyType::XAngle ? angles.x
		                                              : angles.z) = radians;
		transform->rotation = script::property_rules::PlacedRotation(angles);
		registry.SetDirty();
		return;
	}
	case script::ObjectPropertyType::Strength:
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			// Its body shows it
			creature->strength = value;
			return;
		}
		if (auto* globe = registry.TryGet<ecs::components::OneOffSpellSeed>(object); globe != nullptr)
		{
			globe->multiplier = value;
			return;
		}
		if (auto* seed = registry.TryGet<ecs::components::SpellSeed>(object); seed != nullptr)
		{
			seed->castMultiplier = value;
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Cannot set strength of this");
		CannotSetProperty(prop);
		return;
	case script::ObjectPropertyType::Alignment:
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			// Its body shows it
			creature->alignment = value;
			return;
		}
		// Only a creature (or a reward, which openblack doesn't keep an alignment for) takes one
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "UNEXPECTED Alignment change");
		return;
	case script::ObjectPropertyType::Height:
		if (registry.AllOf<ecs::components::VillageTotem>(object))
		{
			// A totem sets its town's share of people at worship
			if (const auto town = TownOfTotem(object); town != entt::null && Locator::villageTotemSystem::has_value())
			{
				Locator::villageTotemSystem::value().SetTownShare(town, value);
			}
			// TODO(script-natives): a totem with no town takes the share itself
			return;
		}
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			// It grows or shrinks to that height at once
			creature->size = script::property_rules::CreatureSizeForHeight(value);
			if (auto* transform = registry.TryGet<Transform>(object); transform != nullptr)
			{
				transform->scale = glm::vec3(ecs::archetypes::CreatureArchetype::DrawnScale(creature->species, creature->size));
				registry.SetDirty();
			}
		}
		// Nothing else takes a height
		return;
	case script::ObjectPropertyType::MaxHeight:
		// Nothing keeps a greatest height
		return;
	case script::ObjectPropertyType::CreatureMinSize:
	case script::ObjectPropertyType::CreatureMaxSize:
		if (auto* spells = registry.TryGet<ecs::components::CreatureSpells>(object); spells != nullptr)
		{
			// The size spells take it no further
			(prop == script::ObjectPropertyType::CreatureMinSize ? spells->smallestSize : spells->largestSize) = value;
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotSetProperty(prop);
		return;
	case script::ObjectPropertyType::CreatureFatness:
		if (auto* creature = registry.TryGet<ecs::components::Creature>(object); creature != nullptr)
		{
			creature->fatness = value;
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object not a creature");
		CannotSetProperty(prop);
		return;
	case script::ObjectPropertyType::Scale:
		// The thing is drawn at this size, the same along every axis
		if (auto* transform = registry.TryGet<Transform>(object); transform != nullptr)
		{
			transform->scale = glm::vec3(value);
			registry.SetDirty();
		}
		return;
	case script::ObjectPropertyType::Age:
		// Only living things take an age, in whole years
		if (registry.AllOf<ecs::components::Villager>(object))
		{
			Locator::livingActionSystem::value().VillagerSetAge(object, static_cast<uint32_t>(static_cast<int32_t>(value)));
			return;
		}
		// TODO(opening): the ages of creatures and animals; other things take none
		NotImplemented(static_cast<int32_t>(prop));
		return;
	case script::ObjectPropertyType::Speed:
		// Metres a turn
		if (auto* wallHug = registry.TryGet<ecs::components::WallHug>(object); wallHug != nullptr)
		{
			wallHug->speed = ecs::villager_script_rules::ScriptSpeedToWalkSpeed(value);
		}
		return;
	case script::ObjectPropertyType::BuiltPercentage:
		// A building is built that far, finished at all of it
		ecs::construction::SetBuilt(object, value);
		return;
	default:
		// TODO(Daniels118): the other properties
		NotImplemented(static_cast<int32_t>(prop));
		return;
	}
}

void GetPosition() // 023 GET_POSITION
{
	// No object, or one that has gone, finds no transform: it is at the origin
	const auto objId = entt::to_integral(PopObject());

	glm::vec3 position(0.0f);
	if (objId != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(objId));
		const auto object = static_cast<entt::entity>(objId);
		if (ecs::script_flocks::IsFlock(registry, object))
		{
			// Where its leader is, or its own place on the ground when it has nobody
			const auto leader = ecs::script_flocks::Leader(registry, object);
			const auto* leaderTransform = leader != entt::null ? registry.TryGet<Transform>(leader) : nullptr;
			if (leaderTransform != nullptr)
			{
				position = leaderTransform->position;
			}
			else
			{
				const auto& place = registry.Get<const ecs::components::ScriptFlock>(object).place;
				const auto xz = map_coords::ToMetres(place);
				position = {xz.x, Locator::terrainSystem::value().GetHeightAt(xz) + place.altitude, xz.y};
			}
		}
		else if (const auto* whale = registry.TryGet<const ecs::components::Whale>(static_cast<entt::entity>(objId)))
		{
			// Where it is this turn, not where it is drawn on the way there
			position = whale->position;
		}
		else if (transform != nullptr)
		{
			position = transform->position;
		}
	}

	PushVec(position);
}

void SetPosition() // 024 SET_POSITION
{
	auto position = PopVec();
	// No object, or one that has gone, finds no transform: nothing moves
	const auto objId = entt::to_integral(PopObject());

	if (objId != 0)
	{
		const auto& island = Locator::terrainSystem::value();
		position.y = island.GetHeightAt(glm::vec2(position.x, position.z));
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(objId));
		if (transform != nullptr)
		{
			transform->position = position;
		}
	}
}

void GetDistance() // 025 GET_DISTANCE
{
	const auto p1 = PopVec();
	const auto p0 = PopVec();
	Pushf(ecs::villager_script_rules::ScriptDistance(p0, p1));
}

/// What a "get ... at" (no reach) or "get ... at ... radius" (a reach) asks for
struct ScriptFindRequest
{
	ObjectType type;
	uint32_t subtype;
	map_coords::MapCoords at;
	std::optional<float> radius;
	bool excludingScripted;
};

/// Where a thing is measured from by the scripts' searches
static map_coords::MapCoords ScriptFindPosition(const ecs::Registry& registry, entt::entity entity)
{
	const auto& position = registry.Get<const Transform>(entity).position;
	return map_coords::FromMetres({position.x, position.z});
}

/// The things in a cell of the map that a search asks for
static std::vector<ecs::script_find::Candidate> ScriptFindCandidates(const ScriptFindRequest& request, glm::ivec2 cell)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const std::vector<entt::entity> things = Locator::entitiesMap::value().GetAllInCell(cell);
	std::vector<ecs::script_find::Candidate> found;
	for (const auto thing : things)
	{
		if (!registry.Valid(thing) || !registry.AllOf<Transform>(thing))
		{
			continue;
		}
		// The excluding variants pass over what a script holds already
		if (request.excludingScripted && registry.AllOf<ecs::components::InScript>(thing))
		{
			continue;
		}
		const auto kind = ecs::script_find::KindOf(registry, thing);
		if (!kind.has_value() || !ecs::script_find::Matches(*kind, request.type, request.subtype))
		{
			continue;
		}
		const auto at = ScriptFindPosition(registry, thing);
		if (request.radius.has_value() && gutils::GetDistanceInMetres(request.at, at) > *request.radius)
		{
			continue;
		}
		found.push_back({.entity = thing, .at = at});
	}
	return found;
}

/// The town nearest a place strictly within the reach, each player's towns in turn and the neutral ones last
static entt::entity FindScriptTown(const map_coords::MapCoords& at, float reach)
{
	const auto& registry = Locator::entitiesRegistry::value();
	struct Entry
	{
		PlayerNames owner;
		uint32_t id;
		ecs::script_find::Candidate candidate;
	};
	std::vector<Entry> towns;
	registry.Each<const ecs::components::Town, const Transform>(
	    [&towns, &registry](entt::entity entity, const ecs::components::Town& town, const Transform&) {
		    towns.push_back({.owner = town.owner,
		                     .id = town.id,
		                     .candidate = {.entity = entity, .at = ScriptFindPosition(registry, entity)}});
	    });
	std::ranges::sort(towns,
	                  [](const Entry& a, const Entry& b) { return a.owner != b.owner ? a.owner < b.owner : a.id < b.id; });
	std::vector<ecs::script_find::Candidate> ordered;
	ordered.reserve(towns.size());
	std::ranges::transform(towns, std::back_inserter(ordered), &Entry::candidate);
	return ecs::script_find::FindNearestTown(at, reach, ordered);
}

/// The thing a script's search finds, taken into the scripts' table, or none. Types with no search (markers, dances,
/// flocks and two unused ones) and types out of range are an error; a creature's search isn't written yet
static entt::entity FindForScript(int32_t type, uint32_t subtype, const glm::vec3& position, std::optional<float> radius,
                                  bool excludingScripted)
{
	constexpr int32_t k_LastFindType = 41;
	if (type < 1 || type > k_LastFindType)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script object type {} out of range", type);
		return entt::null;
	}
	const auto objectType = static_cast<ObjectType>(type);
	switch (objectType)
	{
	case ObjectType::Marker:
	case ObjectType::Dance:
	case ObjectType::Flock:
	case ObjectType::InfluenceRing:
	case ObjectType::WeatherThing:
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "No search for script object type {}", type);
		return entt::null;
	case ObjectType::Creature:
		// TODO(script-natives): the creatures' own searches (see docs scripts/calls.md)
		NotImplemented(type);
		return entt::null;
	default:
		break;
	}
	const auto at = map_coords::FromMetres({position.x, position.z});
	entt::entity found = entt::null;
	if (objectType == ObjectType::Town)
	{
		found = FindScriptTown(at, radius.value_or(ecs::script_find::k_TownAtReach));
	}
	else
	{
		const ScriptFindRequest request {
		    .type = objectType, .subtype = subtype, .at = at, .radius = radius, .excludingScripted = excludingScripted};
		found = ecs::script_find::FindNearest(at, radius.value_or(ecs::script_find::k_AtReach),
		                                      [&request](glm::ivec2 cell) { return ScriptFindCandidates(request, cell); });
	}
	if (found == entt::null)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "Thing not found");
		return entt::null;
	}
	Locator::scriptObjects::value().Register(found, false);
	return found;
}

void Call() // 026 CALL
{
	// The nearest thing of the type and subtype in the cells within a metre of the place
	const auto excludingScripted = Pop().intVal != 0;
	const auto position = PopVec();
	const auto subtype = Pop().uintVal;
	const auto type = Pop().intVal;
	const auto found = FindForScript(type, subtype, position, std::nullopt, excludingScripted);
	Pusho(found == entt::null ? 0u : static_cast<uint32_t>(found));
}

void Create() // 027 CREATE
{
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = static_cast<ObjectType>(Pop().intVal);

	const auto object = CreateScriptObject(type, subtype, position, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	RegisterCreated(object);

	PushObject(object);
}

void Random() // 028 RANDOM
{
	const auto max = Popf();
	const auto min = Popf();
	const float random = min + (max - min) * static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	Pushf(random);
}

void DllGettime() // 029 DLL_GETTIME
{
	// The game leaves this one to the script machine: its own clock
	Locator::vm::value().PushElaspedTime();
}

void StartCameraControl() // 030 START_CAMERA_CONTROL
{
	const auto task = CurrentTask();
	const bool templeScript = TaskType(task) & (lhvm::ScriptType::TempleHelp | lhvm::ScriptType::TempleSpecial);
	const bool insideTemple = PlayerInsideTemple();
	// Following a creature is left for the script's camera, which takes over from where the camera is
	if (!insideTemple && Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive())
	{
		Locator::creatureModeSystem::value().Leave();
	}
	const bool taken = Locator::scriptControlSystem::value().StartCameraControl(
	    Locator::camera::value(), {.task = task, .templeScript = templeScript, .insideTemple = insideTemple},
	    [](float x, float z) { return Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)); });
	// Out in the world the leashes aren't drawn during the script's shots
	if (taken && !insideTemple)
	{
		DrawLeashes(false);
	}
	Pushb(taken);
}

void EndCameraControl() // 031 END_CAMERA_CONTROL
{
	if (Locator::scriptControlSystem::value().EndCameraControl(Locator::camera::value(), CurrentTask()))
	{
		DrawLeashes(true);
	}
}

void SetWidescreen() // 032 SET_WIDESCREEN
{
	const auto enabled = static_cast<bool>(Pop().intVal);
	// Only the task that brought the bars in may change them, or any while none has
	auto& director = Locator::cinematicDirectorSystem::value();
	const auto owner = director.GetWideScreenOwner();
	const auto task = Locator::vm::value().GetCurrentTaskNumber();
	if (enabled && owner == task)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script asking for widescreen it has control of");
	}
	if (owner == 0 || owner == task)
	{
		director.SetWideScreen(enabled, task);
	}
}

/// The villagers of a town in the order the scripts look through them: each building's people, the newest building
/// first and each building's newest person first, then the homeless, the newest first
static std::vector<entt::entity> TownVillagersInOrder(const ecs::Registry& registry, entt::entity town)
{
	std::vector<entt::entity> villagers;
	const auto* data = registry.TryGet<const ecs::components::Town>(town);
	if (data == nullptr)
	{
		return villagers;
	}
	for (const auto abode : data->abodes)
	{
		if (const auto* building = registry.Valid(abode) ? registry.TryGet<const ecs::components::Abode>(abode) : nullptr)
		{
			villagers.insert(villagers.end(), building->inhabitants.begin(), building->inhabitants.end());
		}
	}
	villagers.insert(villagers.end(), data->homelessVillagers.begin(), data->homelessVillagers.end());
	return villagers;
}

/// Whether a container a script can look in: a town, a flock or a dance
static bool IsScriptContainer(const ecs::Registry& registry, entt::entity thing)
{
	return registry.Valid(thing) && (registry.AllOf<ecs::components::Town>(thing) ||
	                                 ecs::script_flocks::IsFlock(registry, thing) || ecs::dances::IsDance(registry, thing));
}

/// The things in a script container, in the order the scripts look through them
static std::vector<entt::entity> ContainerMembers(const ecs::Registry& registry, entt::entity container)
{
	if (ecs::script_flocks::IsFlock(registry, container))
	{
		return registry.Get<const ecs::components::ScriptFlock>(container).members;
	}
	if (registry.AllOf<ecs::components::Town>(container))
	{
		return TownVillagersInOrder(registry, container);
	}
	if (ecs::dances::IsDance(registry, container))
	{
		return ecs::dances::Dancers(registry, container);
	}
	return {};
}

void MoveGameThing() // 033 MOVE_GAME_THING
{
	// How near a creature has to come; others go to the point itself
	[[maybe_unused]] const auto radius = Popf();
	const auto position = PopVec();
	const auto object = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(object))
	{
		ScriptMessage("Object no longer valid");
		return;
	}
	if (IsDirectableVillager(object))
	{
		// One in a hand, in the air or drowning stays where it is
		auto& living = Locator::livingActionSystem::value();
		if (living.VillagerCanBeDirected(object))
		{
			living.VillagerScriptMoveTo(object, glm::vec2(position.x, position.z));
		}
		return;
	}
	if (ecs::script_flocks::IsFlock(Locator::entitiesRegistry::value(), object))
	{
		// Its place moves, and the goal of its leader, who isn't sent there
		ecs::script_flocks::MoveTo(Locator::entitiesRegistry::value(), object,
		                           map_coords::FromMetres({position.x, position.z}));
		return;
	}
	// TODO(opening): creatures, the weather, computer players and other things
	NotImplemented();
}

void SetFocus() // 034 SET_FOCUS
{
	const auto position = PopVec();
	const auto object = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(object))
	{
		ScriptMessage("Object no longer valid");
		return;
	}
	if (IsDirectableVillager(object))
	{
		// It turns at once to face the point
		Locator::livingActionSystem::value().VillagerFace(object, glm::vec2(position.x, position.z));
		return;
	}
	if (IsScriptContainer(Locator::entitiesRegistry::value(), object))
	{
		// Each of its villagers turns to face the point
		for (const auto member : ContainerMembers(Locator::entitiesRegistry::value(), object))
		{
			if (IsDirectableVillager(member))
			{
				Locator::livingActionSystem::value().VillagerFace(member, glm::vec2(position.x, position.z));
			}
		}
		return;
	}
	// TODO(opening): other objects and creatures
	NotImplemented();
}

void HasCameraArrived() // 035 HAS_CAMERA_ARRIVED
{
	if (PlayerInsideTemple())
	{
		ScriptMessage("Script camera in citadel");
	}
	auto& camera = Locator::camera::value();
	if (const auto* script = Locator::scriptControlSystem::value().GetScriptCamera(camera); script != nullptr)
	{
		Pushb(script->Arrived());
		return;
	}
	// The player's camera has arrived once where it is and what it looks at are where it is going
	const auto distanceSquared = [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a - b, a - b); };
	Pushb(distanceSquared(camera.GetOrigin(Camera::Interpolation::Target), camera.GetOrigin()) <
	          script_camera::k_ArrivedDistanceSquared &&
	      distanceSquared(camera.GetFocus(Camera::Interpolation::Target), camera.GetFocus()) <
	          script_camera::k_ArrivedDistanceSquared);
}

/// A villager's town: it leaves its old town's homeless, then joins the new town
static void MoveVillagerToTown(entt::entity villager, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& person = registry.Get<ecs::components::Villager>(villager);
	if (auto* old = registry.Valid(person.town) ? registry.TryGet<ecs::components::Town>(person.town) : nullptr)
	{
		std::erase(old->homelessVillagers, villager);
	}
	static_cast<void>(Locator::townSystem::value().AddVillagerToTown(town, villager));
}

/// A living a script puts in a flock keeps up with it from then on
static void SetFlockState(entt::entity living)
{
	if (IsDirectableVillager(living))
	{
		Locator::livingActionSystem::value().VillagerSetScriptState(living, VillagerStates::MoveInFlock);
	}
}

/// Whether a thing is a living a flock can take
static bool IsFlockLiving(const ecs::Registry& registry, entt::entity thing)
{
	return registry.Valid(thing) && registry.AnyOf<ecs::components::Villager>(thing);
}

void FlockCreate() // 036 FLOCK_CREATE
{
	const auto position = PopVec();
	auto& registry = Locator::entitiesRegistry::value();
	// Nobody owns a script's flock: it belongs to the neutral player
	const auto flock = ecs::script_flocks::Create(registry, map_coords::FromMetres({position.x, position.z}));
	RegisterCreated(flock);
	Pusho(static_cast<uint32_t>(flock));
}

/// A living or a flock put in a flock, or two livings made a flock together
static void FlockAttachToFlock(entt::entity object, entt::entity target, bool asLeader)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& scripts = Locator::scriptObjects::value();
	const bool objectFlock = ecs::script_flocks::IsFlock(registry, object);
	const bool targetFlock = ecs::script_flocks::IsFlock(registry, target);
	if (objectFlock && targetFlock)
	{
		// The flock takes in every member of the other, which goes once it is empty
		while (ecs::script_flocks::Size(registry, object) > 0)
		{
			ecs::script_flocks::AddLiving(registry, target,
			                              registry.Get<const ecs::components::ScriptFlock>(object).members.front());
		}
		Pusho(static_cast<uint32_t>(target));
		return;
	}
	if (objectFlock || targetFlock)
	{
		const auto flock = targetFlock ? target : object;
		const auto living = targetFlock ? object : target;
		if (!IsFlockLiving(registry, living))
		{
			ScriptMessage("JONTY-Trying to add non living to flock");
			ScriptMessage("Thing not added to Flock");
			Pusho(0);
			return;
		}
		if (asLeader)
		{
			ecs::script_flocks::AddLeader(registry, flock, living);
		}
		else if (!ecs::script_flocks::AddLiving(registry, flock, living))
		{
			ScriptMessage("Living already in Flock");
		}
		SetFlockState(living);
		Pusho(static_cast<uint32_t>(flock));
		scripts.AddReference(living);
		return;
	}
	if (!IsFlockLiving(registry, object) || !IsFlockLiving(registry, target))
	{
		ScriptMessage("JONTY- Something is seriously wrong with my code");
		ScriptMessage("Thing not added to Flock");
		Pusho(0);
		return;
	}
	// Two livings make a new flock where the second stands
	const auto flock = ecs::script_flocks::Create(registry, ecs::script_flocks::PositionOf(registry, target));
	if (!scripts.Register(flock, true))
	{
		ScriptMessage("Flock ID failed");
		ecs::script_flocks::Destroy(registry, flock);
		ScriptMessage("Thing not added to Flock");
		Pusho(0);
		return;
	}
	scripts.AddReference(target);
	Pusho(static_cast<uint32_t>(flock));
	// The leader keeps what it is doing; the others keep up with the flock
	if (asLeader)
	{
		ecs::script_flocks::AddLeader(registry, flock, object);
		scripts.AddReference(object);
	}
	else if (ecs::script_flocks::AddLiving(registry, flock, object))
	{
		SetFlockState(object);
		scripts.AddReference(object);
	}
	else
	{
		ScriptMessage("Living already in Flock");
	}
	if (!ecs::script_flocks::AddLiving(registry, flock, target))
	{
		ScriptMessage("Living already in Flock");
		return;
	}
	SetFlockState(target);
	scripts.AddReference(target);
}

/// A villager, or each villager of a container, joins a town
static void FlockAttachToTown(entt::entity object, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (IsScriptContainer(registry, object))
	{
		for (const auto member : ContainerMembers(registry, object))
		{
			if (registry.Valid(member) && registry.AllOf<ecs::components::Villager>(member))
			{
				MoveVillagerToTown(member, town);
			}
		}
		Pusho(static_cast<uint32_t>(town));
		return;
	}
	if (!registry.AllOf<ecs::components::Villager>(object))
	{
		// TODO(opening): a spell dispenser given to a town is the town's from then on
		ScriptMessage("JONTY-Trying to add non villager to town");
		Pusho(0);
		return;
	}
	// It stays in any flock it is in, doing what it was doing
	MoveVillagerToTown(object, town);
	Pusho(static_cast<uint32_t>(town));
}

/// A living joins a dance, dancing in the group whose turn it is that takes it, or standing in the dance in none
static void JoinDance(entt::entity living, entt::entity dance, bool asCentre)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (asCentre)
	{
		registry.Get<ecs::components::Dance>(dance).owner = living;
	}
	// One already dancing leaves its dance first
	ecs::dances::RemoveDancer(registry, living);
	const auto sex = registry.AllOf<ecs::components::Villager>(living) ? ecs::villager_dance::DanceSex(living)
	                                                                   : ecs::components::DanceGroup::k_AnySex;
	static_cast<void>(ecs::dances::AddDancer(registry, dance, living, sex));
	if (IsDirectableVillager(living))
	{
		Locator::livingActionSystem::value().VillagerSetScriptState(living, VillagerStates::InDance);
	}
}

/// A living, or each villager of a container, joins a dance
static void FlockAttachToDance(entt::entity object, entt::entity dance, bool asCentre)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& scripts = Locator::scriptObjects::value();
	if (IsScriptContainer(registry, object))
	{
		for (const auto member : ContainerMembers(registry, object))
		{
			if (IsFlockLiving(registry, member))
			{
				registry.AssignOrReplace<ecs::components::ScriptControlled>(member);
				JoinDance(member, dance, asCentre);
				scripts.AddReference(member);
			}
		}
		Pusho(static_cast<uint32_t>(dance));
		return;
	}
	if (!IsFlockLiving(registry, object))
	{
		ScriptMessage("JONTY-Trying to add non living to dance");
		Pusho(0);
		return;
	}
	JoinDance(object, dance, asCentre);
	scripts.AddReference(object);
	Pusho(static_cast<uint32_t>(dance));
}

void FlockAttach() // 037 FLOCK_ATTACH
{
	const auto asLeader = Pop().intVal != 0;
	const auto target = PopObject();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		ScriptMessage("Id deleted to attach");
		Pusho(0);
		return;
	}
	if (target == entt::null || !registry.Valid(target))
	{
		ScriptMessage("Id deleted to attach to");
		Pusho(0);
		return;
	}
	if (ecs::script_flocks::IsFlock(registry, target) || ecs::script_flocks::IsFlock(registry, object))
	{
		FlockAttachToFlock(object, target, asLeader);
		return;
	}
	if (ecs::dances::IsDance(registry, target))
	{
		registry.AssignOrReplace<ecs::components::ScriptControlled>(target);
		FlockAttachToDance(object, target, asLeader);
		return;
	}
	if (registry.AllOf<ecs::components::Town>(target))
	{
		FlockAttachToTown(object, target);
		return;
	}
	ScriptMessage("Thing not added to id");
	Pusho(0);
}

/// A living leaves a flock, keeping its state, the flock staying when it is left empty
static void DetachFromFlock(uint32_t objectId, entt::entity object, entt::entity flock)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& scripts = Locator::scriptObjects::value();
	if (objectId != 0)
	{
		if (!IsFlockLiving(registry, object))
		{
			ScriptMessage("Trying to remove non living from flock");
			ScriptMessage("Thing not removed from Flock");
			Pusho(0);
			return;
		}
		// TODO(opening): an animal is split off into a flock of its own
		ecs::script_flocks::Remove(registry, object, false);
		scripts.RemoveReference(object);
		Pusho(objectId);
		return;
	}
	// A member at random, not the leader when there are others
	const auto member = ecs::script_flocks::RandomMember(registry, flock, ecs::script_flocks::Leader(registry, flock),
	                                                     [](uint32_t n) { return Locator::gameRandom::value().GameRand(n); });
	if (member == entt::null)
	{
		ScriptMessage("Thing not found from Flock");
		ScriptMessage("Thing not removed from Flock");
		Pusho(0);
		return;
	}
	ecs::script_flocks::Remove(registry, member, false);
	scripts.Register(member, false);
	scripts.RemoveReference(member);
	Pusho(static_cast<uint32_t>(member));
}

/// A dancer leaves a dance: the one given, or with none given the first of its groups', passing over what the dance is
/// danced about while others dance
static void DetachFromDance(uint32_t objectId, entt::entity object, entt::entity dance)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& scripts = Locator::scriptObjects::value();
	if (objectId != 0)
	{
		if (!IsFlockLiving(registry, object))
		{
			ScriptMessage("Trying to remove non living from dance");
			ScriptMessage("Thing not removed from Dance");
			Pusho(0);
			return;
		}
		ecs::dances::RemoveDancer(registry, object);
		scripts.RemoveReference(object);
		Pusho(objectId);
		return;
	}
	const auto& data = registry.Get<const ecs::components::Dance>(dance);
	const auto exclude = data.dancers == 1 ? entt::null : data.owner;
	const auto dancer = ecs::dances::FirstDancer(registry, dance, exclude);
	if (dancer == entt::null)
	{
		ScriptMessage("Thing not removed from Dance");
		Pusho(0);
		return;
	}
	ecs::dances::RemoveDancer(registry, dancer);
	scripts.Register(dancer, false);
	Pusho(static_cast<uint32_t>(dancer));
	scripts.RemoveReference(dancer);
}

void FlockDetach() // 038 FLOCK_DETACH
{
	const auto container = PopObject();
	const auto objectId = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !registry.Valid(container))
	{
		// The game pushes nothing here
		ScriptMessage("From thing dead!");
		return;
	}
	const auto object = objectId != 0 ? Locator::scriptObjects::value().Fetch(static_cast<entt::entity>(objectId)) : entt::null;
	if (ecs::script_flocks::IsFlock(registry, container))
	{
		DetachFromFlock(objectId, object, container);
		return;
	}
	if (registry.AllOf<ecs::components::Town>(container))
	{
		const auto* person = registry.Valid(object) ? registry.TryGet<const ecs::components::Villager>(object) : nullptr;
		if (person != nullptr && person->town == container)
		{
			// TODO(opening): the villager leaves the town altogether
			NotImplemented();
			Pusho(0);
			return;
		}
		if (person != nullptr)
		{
			ScriptMessage("Wrong Town");
		}
		ScriptMessage("Thing not removed from Town");
		Pusho(0);
		return;
	}
	if (IsFlockLiving(registry, container))
	{
		const auto flock = ecs::script_flocks::FlockOf(registry, container);
		if (flock == entt::null)
		{
			ScriptMessage("No flock for living so what the...");
			Pusho(0);
			return;
		}
		DetachFromFlock(objectId, object, flock);
		return;
	}
	if (ecs::dances::IsDance(registry, container))
	{
		DetachFromDance(objectId, object, container);
		return;
	}
	ScriptMessage("Not living - confused");
	Pusho(0);
}

void FlockDisband() // 039 FLOCK_DISBAND
{
	const auto container = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !registry.Valid(container))
	{
		ScriptMessage("Disbanding a NULL object");
		ScriptMessage("Bad Id for Disband");
		return;
	}
	// Every member of a flock or dance leaves it, and one the script controls waits for it; the flock or dance stays,
	// empty. A town or an abode keeps its own
	if (Locator::scriptObjects::value().Disband(container) || registry.AllOf<ecs::components::Abode>(container))
	{
		return;
	}
	ScriptMessage("Bad Id for Disband");
}

void IdSize() // 040 ID_SIZE
{
	const auto container = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (container != entt::null && ecs::script_flocks::IsFlock(registry, container))
	{
		Pushf(static_cast<float>(ecs::script_flocks::Size(registry, container)));
		return;
	}
	if (container != entt::null && registry.Valid(container) && registry.AllOf<ecs::components::Town>(container))
	{
		// Its grown-ups and its children
		uint32_t people = 0;
		registry.Each<const ecs::components::Villager>(
		    [&people, container](const ecs::components::Villager& person) { people += person.town == container ? 1 : 0; });
		Pushf(static_cast<float>(people));
		return;
	}
	if (container != entt::null && ecs::dances::IsDance(registry, container))
	{
		Pushf(static_cast<float>(ecs::dances::Size(registry, container)));
		return;
	}
	// TODO(opening): footballs
	ScriptMessage("Cannot Find Flock/Dance/Town Size");
	Pushf(0.0f);
}

void FlockMember() // 041 FLOCK_MEMBER
{
	const auto flock = PopObject();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::script_flocks::IsFlock(registry, flock) || !IsFlockLiving(registry, object))
	{
		ScriptMessage("Invalid Flock or Thing");
		Pushb(false);
		return;
	}
	Pushb(ecs::script_flocks::IsMember(registry, flock, object));
}

void GetHandPosition() // 042 GET_HAND_POSITION
{
	const auto handEntity = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	auto& handTransform = Locator::entitiesRegistry::value().Get<Transform>(handEntity);

	PushVec(handTransform.position);
}

/// The loaded sound group of a bank's file, whatever the case of the file's name on disk
static std::optional<std::string> LoadedSoundGroup(std::string_view file)
{
	const auto sameName = [file](const std::string& name) {
		return std::ranges::equal(name, file, [](char a, char b) {
			return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		});
	};
	for (const auto& name : Locator::audio::value().GetSoundGroups() | std::views::keys)
	{
		if (sameName(name))
		{
			return name;
		}
	}
	return std::nullopt;
}

void CHLApi::PlayBankSoundEffect(int32_t bank, int32_t sample, std::optional<glm::vec3> position) const
{
	// A number naming no bank, a bank the game doesn't ship and a sample the bank doesn't have all play nothing
	const auto file = audio::ScriptSoundBankFile(bank);
	if (!file.has_value() || !Locator::audio::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	const auto group = LoadedSoundGroup(*file);
	if (!group.has_value())
	{
		return;
	}
	const auto id = entt::hashed_string(fmt::format("{}/{}", *group, sample).c_str()).value();
	auto& sounds = Locator::resources::value().GetSounds();
	if (!sounds.Contains(id))
	{
		return;
	}

	const auto conditions = audio::CurrentSoundEffectConditions();
	if (!audio::SoundEffectHeard(conditions, static_cast<audio::ScriptSoundBank>(bank), sounds.Handle(id)->userParam))
	{
		return;
	}
	Locator::audio::value().PlaySoundEffect(id, position);
}

void PlaySoundEffect() // 043 PLAY_SOUND_EFFECT
{
	const auto withPosition = Pop().intVal != 0;
	const auto position = PopVec();
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	Locator::chlapi::value().PlayBankSoundEffect(bank, sample, withPosition ? std::optional(position) : std::nullopt);
}

void StartMusic() // 044 START_MUSIC
{
	const auto music = Pop().intVal;
	if (music < 0 || music >= static_cast<int32_t>(audio::MusicType::_COUNT))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "START_MUSIC: no music type {}", music);
		return;
	}
	if (auto* gameMusic = Game::Instance()->GetGameMusic())
	{
		gameMusic->StartScriptMusic(static_cast<audio::MusicType>(music));
	}
}

void StopMusic() // 045 STOP_MUSIC
{
	if (auto* gameMusic = Game::Instance()->GetGameMusic())
	{
		gameMusic->StartScriptMusic(audio::MusicType::None);
	}
}

void AttachMusic() // 046 ATTACH_MUSIC
{
	// const auto target = Pop().uintVal;
	// const auto music = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void DetachMusic() // 047 DETACH_MUSIC
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

/// A thing a script deletes goes at once: a creature leaves the game, a dance's dancers go back to deciding what to do,
/// a flock's members are in no flock any more
static void DeleteNow(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<ecs::components::Creature>(object))
	{
		static_cast<void>(ecs::creature_removal::RemoveFromGame(object));
	}
	else if (ecs::dances::IsDance(registry, object))
	{
		Locator::danceSystem::value().Destroy(object);
	}
	else if (ecs::script_flocks::IsFlock(registry, object))
	{
		ecs::script_flocks::Destroy(registry, object);
	}
	else
	{
		ecs::world_objects::Remove(object);
	}
}

void ObjectDelete() // 048 OBJECT_DELETE
{
	const auto mode = Pop().intVal;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	// TODO(opening): a puzzle game is only let go of by the script
	switch (mode)
	{
	case 0:
		// It goes at once
		DeleteNow(object);
		return;
	case 1:
		// Only one of the world's objects goes this way
		if (!IsScriptObject(registry, object))
		{
			return;
		}
		if (registry.AllOf<ecs::components::Creature>(object))
		{
			// A creature fizzes right out over two seconds and goes for good
			Locator::creatureFizzSystem::value().SetFizz(object, 1.0f, 2.0f, true);
			return;
		}
		// A ghost of it flickers out where it stood
		ecs::world_objects::LeaveGhost(object);
		DeleteNow(object);
		return;
	case 2:
		if (!IsScriptObject(registry, object))
		{
			return;
		}
		// TODO(opening): its model breaks up where it stood (15 and 3 to the 3D engine's break-up)
		DeleteNow(object);
		return;
	case 3:
		// A temple starts its destruction
		// TODO(opening): a heart already flagged breaks up instead (80 and 3 to the 3D engine's break-up), and is let go
		if (registry.AllOf<ecs::components::Temple>(object) && Locator::templeDestructionSystem::has_value())
		{
			Locator::templeDestructionSystem::value().Start(object);
		}
		return;
	default:
		// Any other mode only lets it go
		return;
	}
}

void FocusFollow() // 049 FOCUS_FOLLOW
{
	// const auto target = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void PositionFollow() // 050 POSITION_FOLLOW
{
	// const auto target = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CallNear() // 051 CALL_NEAR
{
	// The nearest thing of the type and subtype within the radius of the place
	const auto excludingScripted = Pop().intVal != 0;
	const auto radius = Popf();
	const auto position = PopVec();
	const auto subtype = Pop().uintVal;
	const auto type = Pop().intVal;
	const auto found = FindForScript(type, subtype, position, radius, excludingScripted);
	Pusho(found == entt::null ? 0u : static_cast<uint32_t>(found));
}

void SpecialEffectPosition() // 052 SPECIAL_EFFECT_POSITION
{
	const auto turns = SpecialEffectTurns(Popf());
	const auto position = PopVec();
	const auto effect = Pop().intVal;
	PushObject(ScriptSpotVisualThing(StartScriptSpotVisual(effect, position, turns, entt::null), position));
}

void SpecialEffectObject() // 053 SPECIAL_EFFECT_OBJECT
{
	const auto turns = SpecialEffectTurns(Popf());
	const auto object = PopObject();
	const auto effect = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(object) ? registry.TryGet<Transform>(object) : nullptr;
	if (transform == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SPECIAL_EFFECT_OBJECT: thing invalid, spell not created");
		PushObject(entt::null);
		return;
	}
	// The effect stands on the object and ends when it goes
	const auto position = transform->position;
	PushObject(ScriptSpotVisualThing(StartScriptSpotVisual(effect, position, turns, object), position));
}

void DanceCreate() // 054 DANCE_CREATE
{
	const auto duration = Popf();
	const auto position = PopVec();
	const auto type = Pop().intVal;
	const auto centre = PopObject();
	// What it is danced about may be nothing
	const auto dance = Locator::danceSystem::value().Create(static_cast<DanceInfo>(type), position, centre,
	                                                        static_cast<uint32_t>(map_coords::FtoL(duration)), true);
	if (dance == entt::null)
	{
		ScriptMessage("Dance not created");
		Pusho(0);
		return;
	}
	RegisterCreated(dance);
	Pusho(static_cast<uint32_t>(dance));
}

void CallIn() // 055 CALL_IN
{
	const auto excludingScripted = Pop().intVal != 0;
	const auto container = PopObject();
	const auto subtype = Pop().uintVal;
	const auto type = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	if (container == entt::null || !registry.Valid(container))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Cannot find in");
		Pusho(0);
		return;
	}
	if (!IsScriptContainer(registry, container))
	{
		ScriptMessage("Cannot look in object");
		ScriptMessage("Cannot find in");
		Pusho(0);
		return;
	}
	// TODO(opening): a town's animals and its storage pit
	const auto objectType = static_cast<ObjectType>(type);
	if (registry.AllOf<ecs::components::Town>(container) && objectType != ObjectType::Villager &&
	    objectType != ObjectType::VillagerChild)
	{
		ScriptMessage("Looking for strange type in Town");
		ScriptMessage("Cannot find in");
		Pusho(0);
		return;
	}
	// The first of its things of the type and subtype, passing over those a script holds when asked to
	for (const auto member : ContainerMembers(registry, container))
	{
		if (!registry.Valid(member) || (excludingScripted && registry.AllOf<ecs::components::InScript>(member)))
		{
			continue;
		}
		const auto kind = ecs::script_find::KindOf(registry, member);
		if (kind.has_value() && ecs::script_find::Matches(*kind, objectType, subtype))
		{
			Locator::scriptObjects::value().Register(member, false);
			Pusho(static_cast<uint32_t>(member));
			return;
		}
	}
	ScriptMessage("Cannot find in");
	Pusho(0);
}

void ChangeInnerOuterProperties() // 056 CHANGE_INNER_OUTER_PROPERTIES
{
	const auto calm = Popf();
	const auto outer = Popf();
	const auto inner = Popf();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object != entt::null && ecs::script_flocks::IsFlock(registry, object))
	{
		// A nought leaves that distance as it was; the calm is kept whatever it is
		auto& flock = registry.Get<ecs::components::ScriptFlock>(object);
		if (outer != 0.0f)
		{
			flock.domainRadius = static_cast<uint16_t>(map_coords::FtoL(outer));
		}
		if (inner != 0.0f)
		{
			flock.flockDistance = static_cast<uint16_t>(map_coords::FtoL(inner));
		}
		flock.calm = map_coords::FtoL(calm);
		return;
	}
	// TODO(opening): the weather's own inner and outer sizes
	ScriptMessage("Invalid thing for Changing Variables");
}

void Snapshot() // 057 SNAPSHOT
{
	// const auto challengeId = Pop().intVal;
	// const auto argc = Pop().intVal;
	// const auto argv = PopVarArg(argc);
	// const auto reminderScript = PopString();
	// const auto titleStrID = Pop().intVal;
	// const auto alignment = Popf();
	// const auto success = Popf();
	// const auto focus = PopVec();
	// const auto position = PopVec();
	// const auto quest = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetAlignment() // 058 GET_ALIGNMENT
{
	// const auto zero = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SetAlignment() // 059 SET_ALIGNMENT
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

/// An influence a script makes for a player, at a place or going about with an object: within it the player has
/// influence, or none at all for an anti-influence. It takes its place in the scripts' table as made by a script; none for
/// a player there isn't.
static entt::entity CreateScriptInfluence(uint32_t player, float radius, bool anti, glm::vec3 position, entt::entity follows)
{
	if (player >= static_cast<uint32_t>(PlayerNames::_COUNT))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Could not make influence ring!");
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto ring = registry.Create();
	registry.Assign<Transform>(ring, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::InfluenceSource>(ring, static_cast<PlayerNames>(player), radius, anti, follows);
	RegisterCreated(ring);
	return ring;
}

void InfluenceObject() // 060 INFLUENCE_OBJECT
{
	const auto anti = Pop().intVal != 0;
	// The game's own player number, not the scripts'
	const auto player = Pop().uintVal;
	const auto radius = Popf();
	const auto target = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = target != entt::null && registry.Valid(target) ? registry.TryGet<Transform>(target) : nullptr;
	if (transform == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "thing not valid");
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Could not make influence ring!");
		PushObject(entt::null);
		return;
	}
	PushObject(CreateScriptInfluence(player, radius, anti, transform->position, target));
}

void InfluencePosition() // 061 INFLUENCE_POSITION
{
	const auto anti = Pop().intVal != 0;
	const auto player = Pop().uintVal;
	const auto radius = Popf();
	const auto position = PopVec();
	PushObject(CreateScriptInfluence(player, radius, anti, position, entt::null));
}

void GetInfluence() // 062 GET_INFLUENCE
{
	// const auto position = PopVec();
	// const auto raw = static_cast<bool>(Pop().intVal);
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SetInterfaceInteraction() // 063 SET_INTERFACE_INTERACTION
{
	const auto level = Pop().intVal;
	// What the camera lets the player do at this level of the interface, as the tutorials step through them
	if (!Locator::cameraHelpSystem::value().Get().SetInterfaceLevel(level))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Unexpected interaction : {}", level);
		return;
	}
	// TODO(raffclar): the level also sets two flags of the player's interface. One makes the tooltip of an object held in
	// the hand come from another source; openblack has no objects held in the hand yet. The other flag's reader wasn't
	// found in the game, so what it does isn't known.
}

void Played() // 064 PLAYED
{
	const auto object = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(object))
	{
		// Something gone has played whatever it was asked to
		ScriptMessage("Object no longer valid");
		Pushb(true);
		return;
	}
	if (IsDirectableVillager(object))
	{
		Pushb(Locator::livingActionSystem::value().VillagerHasPlayedScriptAnimation(object));
		return;
	}
	// TODO(opening): creatures' plans, other living things, the weather and dances
	NotImplemented();
	Pushb(true);
}

void RandomUlong() // 065 RANDOM_ULONG
{
	// const auto max = Pop().intVal;
	// const auto min = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void SetGamespeed() // 066 SET_GAMESPEED
{
	const auto speed = Popf();
	if (Locator::scriptControlSystem::value().MaySetGameSpeed(CurrentTask()))
	{
		Locator::time::value().SetSpeed(speed);
	}
}

void CallInNear() // 067 CALL_IN_NEAR
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto radius = Popf();
	// const auto pos = PopVec();
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void OverrideStateAnimation() // 068 OVERRIDE_STATE_ANIMATION
{
	const auto clip = Pop().intVal;
	const auto object = PopObject();
	// The game has clips 1 to 440; it complains of any other but plays it all the same
	constexpr int32_t k_LastClip = 440;
	if (clip < 1 || clip > k_LastClip)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid animation forced");
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		return;
	}
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		Locator::livingActionSystem::value().VillagerOverrideAnimation(object, clip);
		return;
	}
	// TODO(opening): the clips of creatures and animals; anything else isn't living ("thing must be living")
	NotImplemented();
}

void CreatureCreateRelativeToCreature() // 069 CREATURE_CREATE_RELATIVE_TO_CREATURE
{
	// const auto type = Pop().intVal;
	// const auto position = PopVec();
	// const auto scale = Popf();
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void CreatureLearnEverything() // 070 CREATURE_LEARN_EVERYTHING
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureSetKnowsAction() // 071 CREATURE_SET_KNOWS_ACTION
{
	const auto knows = Pop().intVal != 0;
	const auto action = Pop().uintVal;
	const auto typeOfAction = Pop().uintVal;
	const auto creature = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || !registry.Valid(creature))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "No creature for script");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		// The game goes on to teach nothing in particular and fails
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "No script for creature");
		return;
	}
	Locator::creatureMindSystem::value().SetKnowsAction(creature, static_cast<CreatureActionLearningType>(typeOfAction), action,
	                                                    knows);
}

void CreatureSetAgendaPriority() // 072 CREATURE_SET_AGENDA_PRIORITY
{
	// const auto priority = Popf();
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureTurnOffAllDesires() // 073 CREATURE_TURN_OFF_ALL_DESIRES
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureLearnDistinctionAboutActivityObject() // 074 CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureDoAction() // 075 CREATURE_DO_ACTION
{
	// const auto withObject = Pop().uintVal;
	// const auto target = Pop().uintVal;
	// const auto unk1 = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void InCreatureHand() // 076 IN_CREATURE_HAND
{
	const auto creature = PopObject();
	const auto thing = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	if (creature == entt::null || thing == entt::null || !registry.Valid(creature) || !registry.Valid(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid creature or thing");
		Pushb(false);
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Not a creature");
		Pushb(false);
		return;
	}
	const auto* held = registry.TryGet<const ecs::components::CreatureHeldObject>(creature);
	const auto* action = registry.TryGet<const ecs::components::CreatureObjectAction>(creature);
	const auto eating = action != nullptr && action->kind == creature_object_actions::Kind::Eat ? action->target : std::nullopt;
	Pushb(script::property_rules::InCreatureHand(thing, held != nullptr ? held->object : entt::null, eating));
}

void CreatureSetDesireValue() // 077 CREATURE_SET_DESIRE_VALUE
{
	// const auto value = Popf();
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureSetDesireActivated78() // 078 CREATURE_SET_DESIRE_ACTIVATED
{
	// const auto active = Pop().intVal;
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureSetDesireActivated79() // 079 CREATURE_SET_DESIRE_ACTIVATED
{
	// const auto active = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureSetDesireMaximum() // 080 CREATURE_SET_DESIRE_MAXIMUM
{
	// const auto value = Popf();
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ConvertCameraPosition() // 081 CONVERT_CAMERA_POSITION
{
	// Where one of the camera editor's numbered cameras is. The game pushes whatever its stack held for a number the file
	// doesn't have; here that is nowhere
	const auto camera = camera_edits::FindCamera(Pop().intVal);
	const auto position = camera.has_value() ? camera->position : std::array<float, 3> {};
	Pushv(position[0]);
	Pushv(position[1]);
	Pushv(position[2]);
}

void ConvertCameraFocus() // 082 CONVERT_CAMERA_FOCUS
{
	// What one of the camera editor's numbered cameras looks at, nowhere for a number the file doesn't have
	const auto camera = camera_edits::FindCamera(Pop().intVal);
	const auto focus = camera.has_value() ? camera->focus : std::array<float, 3> {};
	Pushv(focus[0]);
	Pushv(focus[1]);
	Pushv(focus[2]);
}

void CreatureSetPlayer() // 083 CREATURE_SET_PLAYER
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StartCountdownTimer() // 084 START_COUNTDOWN_TIMER
{
	// const auto timeout = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureInitialiseNumTimesPerformedAction() // 085 CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureGetNumTimesActionPerformed() // 086 CREATURE_GET_NUM_TIMES_ACTION_PERFORMED
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void RemoveCountdownTimer() // 087 REMOVE_COUNTDOWN_TIMER
{
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetObjectDropped() // 088 GET_OBJECT_DROPPED
{
	// The last thing a creature let go of, none once it has gone; asked only of creatures
	const auto creature = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* dropped = registry.Valid(creature) && registry.AllOf<ecs::components::Creature>(creature)
	                          ? registry.TryGet<const ecs::components::CreatureDroppedObject>(creature)
	                          : nullptr;
	if (registry.Valid(creature) && !registry.AllOf<ecs::components::Creature>(creature))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_OBJECT_DROPPED: only creatures drop things");
	}
	const bool exists = dropped != nullptr && dropped->object != entt::null && registry.Valid(dropped->object);
	Pusho(exists ? static_cast<uint32_t>(dropped->object) : 0);
}

void ClearDroppedByObject() // 089 CLEAR_DROPPED_BY_OBJECT
{
	// The creature forgets what it last let go of
	const auto creature = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(creature) && registry.AllOf<ecs::components::CreatureDroppedObject>(creature))
	{
		registry.Remove<ecs::components::CreatureDroppedObject>(creature);
	}
}

void CreateReaction() // 090 CREATE_REACTION
{
	// const auto reaction = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void RemoveReaction() // 091 REMOVE_REACTION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetCountdownTimer() // 092 GET_COUNTDOWN_TIMER
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void StartDualCamera() // 093 START_DUAL_CAMERA
{
	// const auto obj2 = Pop().uintVal;
	// const auto obj1 = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void UpdateDualCamera() // 094 UPDATE_DUAL_CAMERA
{
	// const auto obj2 = Pop().uintVal;
	// const auto obj1 = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ReleaseDualCamera() // 095 RELEASE_DUAL_CAMERA
{
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetCreatureHelp() // 096 SET_CREATURE_HELP
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetTargetObject() // 097 GET_TARGET_OBJECT
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void CreatureDesireIs() // 098 CREATURE_DESIRE_IS
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void CountdownTimerExists() // 099 COUNTDOWN_TIMER_EXISTS
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void LookGameThing() // 100 LOOK_GAME_THING
{
	// const auto target = Pop().uintVal;
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetObjectDestination() // 101 GET_OBJECT_DESTINATION
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void CreatureForceFinish() // 102 CREATURE_FORCE_FINISH
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void HideCountdownTimer() // 103 HIDE_COUNTDOWN_TIMER
{
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetActionTextForObject() // 104 GET_ACTION_TEXT_FOR_OBJECT
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void CreateDualCameraWithPoint() // 105 CREATE_DUAL_CAMERA_WITH_POINT
{
	// const auto position = PopVec();
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetCameraToFaceObject() // 106 SET_CAMERA_TO_FACE_OBJECT
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void MoveCameraToFaceObject() // 107 MOVE_CAMERA_TO_FACE_OBJECT
{
	// const auto time = Popf();
	// const auto distance = Popf();
	// const auto target = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetMoonPercentage() // 108 GET_MOON_PERCENTAGE
{
	// How full the moon was the last time it showed: 0 at the full moon, 1 at a new moon
	const auto* moon = Locator::entitiesRegistry::value().TryGet<ecs::components::Moon>(Locator::skySystem::value().GetMoon());
	Pushf(graphics::moon::ScriptPercentage(moon != nullptr ? moon->phase : 0.0f));
}

void PopulateContainer() // 109 POPULATE_CONTAINER
{
	namespace populate = ecs::script_populate;
	const auto subtype = Pop().intVal;
	const auto type = Pop().intVal;
	const auto count = populate::CountOf(Popf());
	const auto container = PopObject();
	if (!populate::IsValidType(type))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid type={}", type);
		// The game leaves a value on the stack here although the statement gives none back
		Pusho(0);
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(container) || container == static_cast<entt::entity>(0))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Container not valid");
		return;
	}
	// About the container's place: openblack's containers that can be filled are flocks, at their home on the land
	glm::vec3 centre(0.0f);
	if (const auto* flock = registry.TryGet<const ecs::components::Flock>(container))
	{
		centre = {flock->centre.x, Locator::terrainSystem::value().GetHeightAt(flock->centre), flock->centre.y};
	}
	else if (const auto* transform = registry.TryGet<const Transform>(container))
	{
		centre = transform->position;
	}
	const float spread = populate::SpreadOf(count);
	const auto floatRand = [](float x) { return Locator::gameRandom::value().GameFloatRand(x); };
	auto& scriptObjects = Locator::scriptObjects::value();
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto place = populate::PlaceOf(centre, spread, floatRand);
		const auto thing = CreateScriptObject(static_cast<ObjectType>(type), static_cast<uint32_t>(subtype), place, 0.0f, 0.0f,
		                                      0.0f, 0.0f, 1.0f);
		if (thing == static_cast<entt::entity>(0) || !registry.Valid(thing))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Could not create thing for populate");
			return;
		}
		if (!registry.AnyOf<ecs::components::Animal, ecs::components::Villager, ecs::components::Creature>(thing))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Created Thing Not Living");
			registry.Destroy(thing);
			return;
		}
		RegisterCreated(thing);
		// A flock takes it as its newest member and it goes about with it
		if (!registry.AllOf<ecs::components::Flock>(container) || !registry.AllOf<ecs::components::Animal>(thing))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Not implemented - Thing not added to id");
			return;
		}
		auto& animals = Locator::animalSystem::value();
		animals.JoinFlock(thing, container);
		animals.SetScriptState(thing, LivingStates::LivingMoveInFlock);
		scriptObjects.AddReference(thing);
	}
}

void AddReference() // 110 ADD_REFERENCE
{
	// A script takes hold of an object. The game's table says one value comes back, but the game's function pushes none
	// A variable that holds no object references nothing
	if (const auto id = Pop().uintVal; id != 0)
	{
		Locator::scriptObjects::value().AddReference(static_cast<entt::entity>(id));
	}
}

void RemoveReference() // 111 REMOVE_REFERENCE
{
	if (const auto id = Pop().uintVal; id != 0)
	{
		Locator::scriptObjects::value().RemoveReference(static_cast<entt::entity>(id));
	}
}

void SetGameTime() // 112 SET_GAME_TIME
{
	const auto time = Popf();
	Locator::skySystem::value().SetTime(time);
}

void GetGameTime() // 113 GET_GAME_TIME
{
	Pushf(Locator::skySystem::value().GetClock().GetScriptTime());
}

void GetRealTime() // 114 GET_REAL_TIME
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetRealDay115() // 115 GET_REAL_DAY
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetRealDay116() // 116 GET_REAL_DAY
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetRealMonth() // 117 GET_REAL_MONTH
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetRealYear() // 118 GET_REAL_YEAR
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void RunCameraPath() // 119 RUN_CAMERA_PATH
{
	// const auto cameraEnum = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StartDialogue() // 120 START_DIALOGUE
{
	auto& dialogue = Locator::dialogueControlSystem::value();
	const auto task = CurrentTask();
	auto owner = dialogue.GetOwner();
	if (owner == 0)
	{
		dialogue.SendSpiritsHome(false);
	}
	else
	{
		if (owner == task)
		{
			ScriptMessage("Script Asking For Dialogue Control It already has! - Dangerous");
			Pushb(true);
			return;
		}
		// A story script takes the dialogue from a help script, whose tasks are stopped and so give it back
		if (TaskType(owner) == lhvm::ScriptType::Help && TaskType(task) == lhvm::ScriptType::Script)
		{
			Locator::vm::value().StopTasksOfType(lhvm::ScriptType::Help | lhvm::ScriptType::TempleHelp |
			                                     lhvm::ScriptType::MultiplayerHelp);
			owner = dialogue.GetOwner();
		}
		if (owner != 0)
		{
			Pushb(false);
			return;
		}
	}
	// The script carries on even when another task's cinema bars keep the dialogue from it
	dialogue.Request(task, WideScreenOwner());
	Pushb(true);
}

void EndDialogue() // 121 END_DIALOGUE
{
	auto& dialogue = Locator::dialogueControlSystem::value();
	const auto task = CurrentTask();
	if (dialogue.GetOwner() != task)
	{
		return;
	}
	dialogue.SendSpiritsHome(TaskType(task) == lhvm::ScriptType::Help);
	ReleaseDialogue(task);
}

void IsDialogueReady() // 122 IS_DIALOGUE_READY
{
	Pushb(!Locator::dialogueControlSystem::value().IsControlled(WideScreenOwner()));
}

void ChangeWeatherProperties() // 123 CHANGE_WEATHER_PROPERTIES
{
	// const auto fallspeed = Popf();
	// const auto overcast = Popf();
	// const auto snowfall = Popf();
	// const auto rainfall = Popf();
	// const auto temperature = Popf();
	// const auto storm = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ChangeLightningProperties() // 124 CHANGE_LIGHTNING_PROPERTIES
{
	// const auto forkmax = Popf();
	// const auto forkmin = Popf();
	// const auto sheetmax = Popf();
	// const auto sheetmin = Popf();
	// const auto storm = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ChangeTimeFadeProperties() // 125 CHANGE_TIME_FADE_PROPERTIES
{
	// const auto fadeTime = Popf();
	// const auto duration = Popf();
	// const auto storm = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ChangeCloudProperties() // 126 CHANGE_CLOUD_PROPERTIES
{
	// const auto elevation = Popf();
	// const auto blackness = Popf();
	// const auto numClouds = Popf();
	// const auto storm = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetHeadingAndSpeed() // 127 SET_HEADING_AND_SPEED
{
	// const auto speed = Popf();
	// const auto position = PopVec();
	// const auto unk0 = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StartGameSpeed() // 128 START_GAME_SPEED
{
	Locator::scriptControlSystem::value().StartGameSpeed(CurrentTask());
}

void EndGameSpeed() // 129 END_GAME_SPEED
{
	if (Locator::scriptControlSystem::value().EndGameSpeed(CurrentTask()))
	{
		Locator::time::value().SetSpeed(1.0f);
	}
}

void BuildBuilding() // 130 BUILD_BUILDING
{
	const auto desire = Popf();
	const auto position = PopVec();
	// The towns start what they planned there: a planned temple goes up
	// TODO(villager-life): the towns' other planned buildings
	if (!ecs::construction::StartPlannedAt(position, desire).has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "BUILD_BUILDING: no planned temple at ({}, {})", position.x, position.z);
	}
}

void SetAffectedByWind() // 131 SET_AFFECTED_BY_WIND
{
	// const auto object = Pop().uintVal;
	// const auto enabled = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void WidescreenTransistionFinished() // 132 WIDESCREEN_TRANSISTION_FINISHED
{
	Pushb(Locator::cinematicDirectorSystem::value().IsWideScreenTransitionFinished());
}

void GetResource() // 133 GET_RESOURCE
{
	// const auto container = Pop().uintVal;
	// const auto resource = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void AddResource() // 134 ADD_RESOURCE
{
	// const auto container = Pop().uintVal;
	// const auto quantity = Popf();
	// const auto resource = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void RemoveResource() // 135 REMOVE_RESOURCE
{
	// const auto container = Pop().uintVal;
	// const auto quantity = Popf();
	// const auto resource = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetTargetRelativePos() // 136 GET_TARGET_RELATIVE_POS
{
	// const auto angle = Popf();
	// const auto distance = Popf();
	// const auto to = PopVec();
	// const auto from = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void StopPointing() // 137 STOP_POINTING
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritStopPointing(spirit);
	}
}

void StopLooking() // 138 STOP_LOOKING
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritStopLooking(spirit);
	}
}

void LookAtPosition() // 139 LOOK_AT_POSITION
{
	const auto position = PopVec();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritLookAtPosition(spirit, position);
	}
}

void PlaySpiritAnim() // 140 PLAY_SPIRIT_ANIM
{
	const auto time = Popf();
	const auto anim = Pop().intVal;
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	// None of the checks stops it
	if (anim < 0 || anim > k_LastSpiritAnim)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid enum");
	}
	CheckScreenFractions(x, y);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritPlayAnim(spirit, x, y, static_cast<uint32_t>(anim), time);
	}
}

void CallInNotNear() // 141 CALL_IN_NOT_NEAR
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto radius = Popf();
	// const auto pos = PopVec();
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SetCameraZone() // 142 SET_CAMERA_ZONE
{
	// const auto filename = PopString();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetObjectState() // 143 GET_OBJECT_STATE
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void RevealCountdownTimer() // 144 REVEAL_COUNTDOWN_TIMER
{
	// TODO(Daniels118): implement this
	NotImplemented();
}

/// The game's turn now, by which the scripts' timers count
static uint32_t TimerTurn()
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

/// The timer a script names, none for anything else (which the script is told of)
static ecs::components::ScriptTimer* ScriptTimerOf(entt::entity object)
{
	auto* timer = Locator::entitiesRegistry::value().TryGet<ecs::components::ScriptTimer>(object);
	if (timer == nullptr && !Locator::entitiesRegistry::value().AllOf<ecs::components::SpellDispenser>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid script thing");
	}
	return timer;
}

void SetTimerTime() // 145 SET_TIMER_TIME
{
	const auto seconds = Popf();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object no longer valid");
		return;
	}
	if (auto* timer = ScriptTimerOf(object); timer != nullptr)
	{
		// It starts again from now
		timer->timer = script::timers::Set(TimerTurn(), seconds);
		return;
	}
	if (registry.AllOf<ecs::components::SpellDispenser>(object))
	{
		// The turns from its bubble being taken to its next, counted as a timer's are; none leaves it as it was, and a
		// time before now is so many turns it never makes another
		const auto turns = script::timers::TurnsFor(seconds);
		if (turns != 0)
		{
			Locator::magicSystem::value().SetDispenserTurns(object, static_cast<uint32_t>(turns));
		}
		return;
	}
}

void CreateTimer() // 146 CREATE_TIMER
{
	const auto seconds = Popf();
	auto& registry = Locator::entitiesRegistry::value();
	const auto timer = registry.Create();
	registry.Assign<ecs::components::ScriptTimer>(timer, script::timers::Set(TimerTurn(), seconds));
	// The script controls it from its first reference
	RegisterCreated(timer);
	PushObject(timer);
}

void GetTimerTimeRemaining() // 147 GET_TIMER_TIME_REMAINING
{
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object no longer valid");
		Pushf(0.0f);
		return;
	}
	const auto* timer = ScriptTimerOf(object);
	Pushf(timer != nullptr ? script::timers::SecondsRemaining(timer->timer, TimerTurn()) : 0.0f);
}

void GetTimerTimeSinceSet() // 148 GET_TIMER_TIME_SINCE_SET
{
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object no longer valid");
		Pushf(std::numeric_limits<float>::max());
		return;
	}
	// Anything but a timer has been set for ever
	const auto* timer = ScriptTimerOf(object);
	Pushf(timer != nullptr ? script::timers::SecondsSinceSet(timer->timer, TimerTurn()) : std::numeric_limits<float>::max());
}

void MoveMusic() // 149 MOVE_MUSIC
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetInclusionDistance() // 150 GET_INCLUSION_DISTANCE
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetLandHeight() // 151 GET_LAND_HEIGHT
{
	const auto position = PopVec();

	const auto& island = Locator::terrainSystem::value();
	const auto elevation = island.GetHeightAt(glm::vec2(position.x, position.z));

	Pushf(elevation);
}

void LoadMap() // 152 LOAD_MAP
{
	// The story moves on to its next land: the land is laid out afresh at once, while the scripts go on running, the
	// one that asked included
	const auto path = PopString();
	const auto& fileSystem = Locator::filesystem::value();
	try
	{
		Game::Instance()->LoadMap(fileSystem.FindPath(filesystem::FileSystemInterface::FixPath(path)),
		                          loading::LoadingClock::Mode::PleaseWait);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Could not load the map {}: {}", path, e.what());
	}
}

void StopAllScriptsExcluding() // 153 STOP_ALL_SCRIPTS_EXCLUDING
{
	const auto scriptNames = PopString();

	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&scriptNames](const std::string& name, [[maybe_unused]] const std::string& filename) -> bool {
		return !script::name_lists::HoldsScript(scriptNames, name);
	});
}

void StopAllScriptsInFilesExcluding() // 154 STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING
{
	const auto sourceFilenames = PopString();

	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&sourceFilenames]([[maybe_unused]] const std::string& name, const std::string& filename) -> bool {
		return !script::name_lists::HoldsFile(sourceFilenames, filename);
	});
}

void StopScript() // 155 STOP_SCRIPT
{
	const auto scriptName = PopString();
	auto& lhvm = Locator::vm::value();
	// It may name several, as "ScriptA, ScriptB"
	lhvm.StopScripts([&scriptName](const std::string& name, [[maybe_unused]] const std::string& filename) -> bool {
		return script::name_lists::HoldsScript(scriptName, name);
	});
}

void ClearClickedObject() // 156 CLEAR_CLICKED_OBJECT
{
	if (auto* clicked = LocalHandClicked())
	{
		hand_click::ClearThing(*clicked);
	}
}

void ClearClickedPosition() // 157 CLEAR_CLICKED_POSITION
{
	if (auto* clicked = LocalHandClicked())
	{
		hand_click::ClearPlace(*clicked);
	}
}

void PositionClicked() // 158 POSITION_CLICKED
{
	const auto radius = Popf();
	const auto position = PopVec();
	const auto* clicked = LocalHandClicked();
	// Along the ground: the position's height doesn't count
	const map_coords::MapCoords at {.x = map_coords::ToFixed(position.x), .z = map_coords::ToFixed(position.z)};
	Pushb(clicked != nullptr && hand_click::IsPlaceClicked(*clicked, at, radius));
}

void ReleaseFromScript() // 159 RELEASE_FROM_SCRIPT
{
	Locator::scriptObjects::value().ReleaseFromScript(PopObject());
}

void GetObjectHandIsOver() // 160 GET_OBJECT_HAND_IS_OVER
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void IdPoisonedSize() // 161 ID_POISONED_SIZE
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void IsPoisoned() // 162 IS_POISONED
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void CallPoisonedIn() // 163 CALL_POISONED_IN
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void CallNotPoisonedIn() // 164 CALL_NOT_POISONED_IN
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SpiritPlayed() // 165 SPIRIT_PLAYED
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	const auto* advisors = ScriptAdvisors();
	Pushb(advisors == nullptr || !advisors->SpiritPlayingAnim(spirit));
}

void ClingSpirit() // 166 CLING_SPIRIT
{
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	CheckScreenFractions(x, y);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritCling(spirit, x, y);
	}
}

void FlySpirit() // 167 FLY_SPIRIT
{
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	CheckScreenFractions(x, y);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritFly(spirit, x, y);
	}
}

void SetIdMoveable() // 168 SET_ID_MOVEABLE
{
	const auto object = PopObject();
	const auto moveable = static_cast<bool>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_ID_MOVEABLE: thing not valid");
		return;
	}
	// A thing made unmoveable is held where it is: the physics never takes it
	if (moveable)
	{
		registry.Remove<ecs::components::Immovable>(object);
	}
	else
	{
		registry.AssignOrReplace<ecs::components::Immovable>(object);
	}
}

void SetIdPickupable() // 169 SET_ID_PICKUPABLE
{
	const auto object = PopObject();
	const auto pickupable = Pop().uintVal != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_ID_PICKUPABLE: thing not valid");
		return;
	}
	// A thing a script says can't be picked up is left by the hand
	if (pickupable)
	{
		registry.Remove<ecs::components::CannotBePickedUp>(object);
	}
	else
	{
		registry.AssignOrReplace<ecs::components::CannotBePickedUp>(object);
	}
}

void IsOnFire() // 170 IS_ON_FIRE
{
	const auto object = PopObject();
	Pushb(Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(object));
}

void IsFireNear() // 171 IS_FIRE_NEAR
{
	const auto radius = Popf();
	const auto position = PopVec();
	Pushb(Locator::fireSystem::has_value() && Locator::fireSystem::value().IsFireNear(position, radius));
}

void StopScriptsInFiles() // 172 STOP_SCRIPTS_IN_FILES
{
	const auto sourceFilenames = PopString();

	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&sourceFilenames]([[maybe_unused]] const std::string& name, const std::string& filename) -> bool {
		return script::name_lists::HoldsFile(sourceFilenames, filename);
	});
}

void SetPoisoned() // 173 SET_POISONED
{
	// const auto obj = Pop().uintVal;
	// const auto poisoned = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetTemperature() // 174 SET_TEMPERATURE
{
	const auto temperature = Popf();
	const auto object = PopObject();
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().SetTemperature(object, temperature, entt::null);
	}
}

void SetOnFire() // 175 SET_ON_FIRE
{
	const auto burnSpeed = Popf();
	const auto object = PopObject();
	const auto enable = static_cast<bool>(Pop().intVal);
	if (!Locator::fireSystem::has_value())
	{
		return;
	}
	// Set alight at the speed, or brought back to the air's temperature
	if (enable)
	{
		Locator::fireSystem::value().SetOnFire(object, burnSpeed);
	}
	else
	{
		Locator::fireSystem::value().PutOut(object);
	}
}

void SetTarget() // 176 SET_TARGET
{
	// const auto time = Popf();
	// const auto position = PopVec();
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void WalkPath() // 177 WALK_PATH
{
	const auto to = Popf();
	const auto from = Popf();
	const auto path = Pop().intVal;
	const auto forward = Pop().intVal != 0;
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ScriptMessage("Thing not valid");
		return;
	}
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		// A villager walks the track at its own speed, then waits for the script
		if (!ecs::villager_script::StartPathWalk(object, path, forward, from, to))
		{
			SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Cannot load track No {}", path);
		}
		return;
	}
	if (registry.AnyOf<ecs::components::Creature, ecs::components::Animal>(object))
	{
		// TODO(opening): creatures and animals walk the track at their own speed too
		NotImplemented();
		return;
	}
	// The things that move as the game's mobile objects do walk the track, anything else can't
	if (!registry.AnyOf<ecs::components::Whale, ecs::components::MobileObject>(object))
	{
		ScriptMessage("Thing is invalid for move path");
		return;
	}
	static_cast<void>(Locator::walkPathSystem::value().Start(object, path, forward, from, to));
}

void FocusAndPositionFollow() // 178 FOCUS_AND_POSITION_FOLLOW
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetWalkPathPercentage() // 179 GET_WALK_PATH_PERCENTAGE
{
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		ScriptMessage("Thing not valid");
		Pushf(1.0f);
		return;
	}
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		// How much of its track it has walked
		// TODO(opening): the game reads the walk of a villager given none to walk; here it has walked the whole way
		Pushf(ecs::villager_script::PathWalkPercentage(object).value_or(1.0f));
		return;
	}
	if (registry.AnyOf<ecs::components::Creature, ecs::components::Animal>(object))
	{
		// TODO(opening): how much of its track a creature or an animal has walked
		NotImplemented();
		Pushf(1.0f);
		return;
	}
	// Anything else, the things walking tracks too, has always walked the whole way
	Pushf(1.0f);
}

void CameraProperties() // 180 CAMERA_PROPERTIES
{
	const auto relative = Pop().intVal != 0;
	// Degrees to radians by the game's own float
	constexpr float k_DegreesToRadians = 0.017453292f;
	const auto angle = Popf() * k_DegreesToRadians;
	const auto glideScale = Popf();
	const auto distance = Popf();
	auto* camera = Locator::scriptControlSystem::value().GetScriptCamera(Locator::camera::value());
	if (camera == nullptr)
	{
		ScriptMessage("Script camera has been removed! - Exception happened?");
		return;
	}
	// How the script's camera follows a thing: from how far, how quickly, at what heading, and whether the heading turns
	// with the thing
	camera->SetFollowSettings(distance, glideScale, angle, relative);
}

void EnableDisableMusic() // 181 ENABLE_DISABLE_MUSIC
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetMusicObjDistance() // 182 GET_MUSIC_OBJ_DISTANCE
{
	// const auto source = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetMusicEnumDistance() // 183 GET_MUSIC_ENUM_DISTANCE
{
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SetMusicPlayPosition() // 184 SET_MUSIC_PLAY_POSITION
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void AttachObjectLeashToObject() // 185 ATTACH_OBJECT_LEASH_TO_OBJECT
{
	const auto object = PopObject();
	const auto creature = PopObject();
	if (object == entt::null || creature == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing for leash not found");
		return;
	}
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	leashes.TieTo(creature, object);
}

void AttachObjectLeashToHand() // 186 ATTACH_OBJECT_LEASH_TO_HAND
{
	const auto creature = PopObject();
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	if (leashes.TiedTo(creature).has_value())
	{
		leashes.UntieToHand(creature);
	}
	else if (!leashes.IsLeashed(creature))
	{
		leashes.Toggle(creature);
	}
}

void DetachObjectLeash() // 187 DETACH_OBJECT_LEASH
{
	const auto creature = PopObject();
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	leashes.TakeOff(creature);
}

void SetCreatureOnlyDesire() // 188 SET_CREATURE_ONLY_DESIRE
{
	// const auto value = Popf();
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetCreatureOnlyDesireOff() // 189 SET_CREATURE_ONLY_DESIRE_OFF
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void RestartMusic() // 190 RESTART_MUSIC
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void MusicPlayed191() // 191 MUSIC_PLAYED
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void IsOfType() // 192 IS_OF_TYPE
{
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void ClearHitObject() // 193 CLEAR_HIT_OBJECT
{
	// The physics forgets the last thing hit and what hit it
	Locator::dynamicsSystem::value().RecordHit(entt::null, entt::null);
}

void GameThingHit() // 194 GAME_THING_HIT
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void SpellAtThing() // 195 SPELL_AT_THING
{
	const auto curl = Popf();
	const auto duration = Popf();
	const auto radius = Popf();
	const auto from = PopVec();
	const auto target = PopObject();
	const auto spell = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(target) ? registry.TryGet<const ecs::components::Transform>(target) : nullptr;
	if (transform == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid thing");
		Pusho(0);
		return;
	}
	if (!magic::IsScriptMagicType(spell) || !Locator::magicSystem::has_value() || !Locator::infoConstants::has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid magic");
		Pusho(0);
		return;
	}
	// The neutral player's miracle, spun by the script's curl: on an object as its seed casts, else at the thing's place
	const auto type = static_cast<MagicType>(spell);
	const auto& info = Locator::infoConstants::value();
	const auto cast = magic::MakeScriptCast(magic::GetMagicEffectInfo(info, type).initialChants, transform->position, from,
	                                        radius, duration, curl);
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, type);
	const bool object = IsScriptObject(registry, target);
	const bool onObject = object && seed.has_value() && magic::GetSpellSeedInfo(info, *seed).castOnObject != 0;
	auto& magicSystem = Locator::magicSystem::value();
	const auto entity = onObject ? magicSystem.CastOnObject(type, PlayerNames::NEUTRAL, target, cast.cast, cast.info)
	                             : magicSystem.CastAtPoint(type, PlayerNames::NEUTRAL, cast.point, cast.cast, cast.info);
	if (entity == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		Pusho(0);
		return;
	}
	// The miracle is the script's own making, which it controls from its first reference
	RegisterCreated(entity);
	Pusho(static_cast<uint32_t>(entity));
}

void SpellAtPos() // 196 SPELL_AT_POS
{
	const auto curl = Popf();
	const auto duration = Popf();
	const auto radius = Popf();
	const auto from = PopVec();
	const auto target = PopVec();
	const auto spell = Pop().intVal;
	if (!magic::IsScriptMagicType(spell) || !Locator::magicSystem::has_value() || !Locator::infoConstants::has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid magic");
		// The script still takes a result off the stack
		Pusho(0);
		return;
	}
	// The neutral player's miracle, spun by the script's curl
	const auto type = static_cast<MagicType>(spell);
	const auto cast = magic::MakeScriptCast(magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).initialChants,
	                                        target, from, radius, duration, curl);
	const auto entity = Locator::magicSystem::value().CastAtPoint(type, PlayerNames::NEUTRAL, cast.point, cast.cast, cast.info);
	if (entity == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Spell not created");
		Pusho(0);
		return;
	}
	// The miracle is the script's own making, which it controls from its first reference
	RegisterCreated(entity);
	Pusho(static_cast<uint32_t>(entity));
}

void CallPlayerCreature() // 197 CALL_PLAYER_CREATURE
{
	// The player's creature is their primary one: the first they got that is still theirs
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto creature =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetPrimaryCreature(player) : std::nullopt;
	if (!creature.has_value() || !Locator::entitiesRegistry::value().Valid(*creature))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Player {} has no creature", static_cast<int>(player));
		PushObject(entt::null);
		return;
	}
	// It takes a place in the scripts' table
	Locator::scriptObjects::value().Register(*creature, false);
	PushObject(*creature);
}

void GetSlowestSpeed() // 198 GET_SLOWEST_SPEED
{
	// const auto flock = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetObjectHeld199() // 199 GET_OBJECT_HELD
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void HelpSystemOn() // 200 HELP_SYSTEM_ON
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void ShakeCamera() // 201 SHAKE_CAMERA
{
	const auto duration = Popf();
	const auto amplitude = Popf();
	const auto radius = Popf();
	const auto position = PopVec();
	// The shake lasts the seconds given rounded to whole milliseconds, and shakes every way, not only up and down
	const auto milliseconds = std::round(duration * 1000.0f);
	Locator::explosionSystem::value().AddShake(position, radius, amplitude, milliseconds / 1000.0f, false);
}

void SetAnimationModify() // 202 SET_ANIMATION_MODIFY
{
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetAviSequence() // 203 SET_AVI_SEQUENCE
{
	// Sequence 1 is the story's intro, 2 the falling spell's film
	const auto sequence = Pop().intVal;
	const auto enable = static_cast<bool>(Pop().intVal);
	auto& videos = Locator::videoSystem::value();
	auto& director = Locator::cinematicDirectorSystem::value();
	if (!enable)
	{
		if (sequence == 2)
		{
			videos.EndFallingSpell();
		}
		return;
	}
	if (sequence == 1)
	{
		// The intro is cut short: it fades from 58 s and ends at 60 s. The script's fade is lifted at once under it
		videos.Play("Data/INTRO.bik");
		videos.ScheduleIntro();
		director.FadeBackToNormal(0);
	}
	else if (sequence == 2)
	{
		videos.StartFallingSpell();
		director.FadeBackToNormal(0);
	}
}

void PlayGesture() // 204 PLAY_GESTURE
{
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void DevFunction() // 205 DEV_FUNCTION
{
	const auto func = Pop().intVal;
	// The functions the tutorials use on the local player's creature: starting it growing up again kept at home, and
	// granting it the learning leash, then the aggression and compassion leashes
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	const auto creature = leashes.PlayersCreature(PlayerNames::PLAYER_ONE);
	if (!creature.has_value())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "DEV_FUNCTION({}): the player has no creature they can lead", func);
		return;
	}
	switch (func)
	{
	case 1:
		if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(*creature))
		{
			mind->developmentPhase = 0;
		}
		leashes.ConfineToHome(*creature, creature_leash::k_HomeConfinement);
		break;
	case 2:
		leashes.SetKnown(*creature, LeashType::Rope, true);
		break;
	case 3:
		leashes.SetKnown(*creature, LeashType::Good, true);
		leashes.SetKnown(*creature, LeashType::Evil, true);
		break;
	case 8:
	case 9:
		// Whether the creature can die: a miracle that takes the last of its life knocks it out, or gives it all back
		if (auto* body = Locator::entitiesRegistry::value().TryGet<ecs::components::Creature>(*creature))
		{
			body->canDie = func == 8;
		}
		break;
	default:
		// TODO(Daniels118): implement the other functions
		NotImplemented(func);
		break;
	}
}

void HasMouseWheel() // 206 HAS_MOUSE_WHEEL
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void NumMouseButtons() // 207 NUM_MOUSE_BUTTONS
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SetCreatureDevStage() // 208 SET_CREATURE_DEV_STAGE
{
	// const auto stage = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetFixedCamRotation() // 209 SET_FIXED_CAM_ROTATION
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SwapCreature() // 210 SWAP_CREATURE
{
	[[maybe_unused]] const auto toCreature = PopObject();
	[[maybe_unused]] const auto fromCreature = PopObject();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetArena() // 211 GET_ARENA
{
	// The nearest arena closer than the distance to the point, or else a new one there for the two creatures, as a fight
	// makes one: sized for the bigger of them, with room clear for the first
	const auto second = PopObject();
	const auto first = PopObject();
	const auto distance = Popf();
	const auto position = PopVec();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(first) || !registry.Valid(second))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_ARENA: creature for arena not found!");
	}
	else if (!registry.AllOf<ecs::components::Creature>(first) || !registry.AllOf<ecs::components::Creature>(second))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_ARENA: thing not creature");
	}
	else if (Locator::creatureFightSystem::has_value())
	{
		if (const auto found = Locator::creatureFightSystem::value().FindOrMakeArena(position, first, second, distance))
		{
			if (Locator::scriptObjects::has_value())
			{
				Locator::scriptObjects::value().Register(found->arena, found->made);
			}
			PushObject(found->arena);
			return;
		}
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_ARENA: Arena not found or created!");
	Pusho(0);
}

void GetFootballPitch() // 212 GET_FOOTBALL_PITCH
{
	// const auto town = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void StopAllGames() // 213 STOP_ALL_GAMES
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void AttachToGame() // 214 ATTACH_TO_GAME
{
	// The team, the football pitch and the villager joining it; the native takes control of both objects
	[[maybe_unused]] const auto team = Pop().intVal;
	[[maybe_unused]] const auto pitch = PopObject();
	[[maybe_unused]] const auto villager = PopObject();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void DetachFromGame() // 215 DETACH_FROM_GAME
{
	// A value the game ignores, the football pitch and the villager leaving it; the native takes control of both objects
	[[maybe_unused]] const auto unused = Pop().intVal;
	[[maybe_unused]] const auto pitch = PopObject();
	[[maybe_unused]] const auto villager = PopObject();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void DetachUndefinedFromGame() // 216 DETACH_UNDEFINED_FROM_GAME
{
	// A value and the football pitch, of which the native takes control
	[[maybe_unused]] const auto value = Pop().intVal;
	[[maybe_unused]] const auto pitch = PopObject();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetOnlyForScripts() // 217 SET_ONLY_FOR_SCRIPTS
{
	// The football pitch, of which the native takes control, then whether only the scripts play on it
	[[maybe_unused]] const auto pitch = PopObject();
	[[maybe_unused]] const auto onlyForScripts = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StartMatchWithReferee() // 218 START_MATCH_WITH_REFEREE
{
	// The villager refereeing and the football pitch; the native takes control of both
	[[maybe_unused]] const auto referee = PopObject();
	[[maybe_unused]] const auto pitch = PopObject();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GameTeamSize() // 219 GAME_TEAM_SIZE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GameType() // 220 GAME_TYPE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void GameSubType() // 221 GAME_SUB_TYPE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void IsLeashed() // 222 IS_LEASHED
{
	const auto object = PopObject();
	Pushb(Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(object));
}

void SetCreatureHome() // 223 SET_CREATURE_HOME
{
	// The creature's home becomes the point, on the ground and kept as precisely as a map position. While its player's
	// temple stands the temple's pen is its home again from the next game turn.
	const auto position = PopVec();
	const auto creature = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_CREATURE_HOME: thing not found");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(creature))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_CREATURE_HOME: thing not creature");
		return;
	}
	const auto place = temple_pen::MapPlace(position);
	const auto ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(place) : 0.0f;
	auto* leash = registry.TryGet<ecs::components::CreatureLeash>(creature);
	if (leash == nullptr)
	{
		leash = &registry.Assign<ecs::components::CreatureLeash>(creature);
	}
	leash->home = glm::vec3(place.x, ground, place.y);
}

void GetHitObject() // 224 GET_HIT_OBJECT
{
	// The last thing a body in the physics hit, none once it has gone
	const auto hit = Locator::dynamicsSystem::value().GetHitObject();
	Pusho(hit == entt::null ? 0 : static_cast<uint32_t>(hit));
}

void GetObjectWhichHit() // 225 GET_OBJECT_WHICH_HIT
{
	// What hit it, none once it has gone
	const auto hitter = Locator::dynamicsSystem::value().GetObjectWhichHit();
	Pusho(hitter == entt::null ? 0 : static_cast<uint32_t>(hitter));
}

void GetNearestTownOfPlayer() // 226 GET_NEAREST_TOWN_OF_PLAYER
{
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SpellAtPoint() // 227 SPELL_AT_POINT
{
	// Finds a miracle already at a point, casting nothing: for either shield the first shield standing over the point,
	// else the newest miracle of the kind whose last event was strictly within the radius
	const auto radius = Popf();
	const auto position = PopVec();
	const auto spell = Pop().intVal;
	std::optional<entt::entity> found;
	if (spell == static_cast<int32_t>(MagicType::Shield) || spell == static_cast<int32_t>(MagicType::PhysicalShield))
	{
		if (Locator::magicShieldSystem::has_value())
		{
			found = Locator::magicShieldSystem::value().ShieldAt(position);
		}
	}
	else if (Locator::magicSystem::has_value())
	{
		found = Locator::magicSystem::value().SpellAt(static_cast<MagicType>(spell), position, radius);
	}
	Pusho(found.has_value() ? static_cast<uint32_t>(*found) : 0);
}

void SetAttackOwnTown() // 228 SET_ATTACK_OWN_TOWN
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

/// The creature a fight native is given, or none (logged) when it is not a creature
std::optional<entt::entity> FightCreature(const char* native, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	if (!registry.AllOf<ecs::components::Creature>(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: thing not creature", native);
		return std::nullopt;
	}
	if (!Locator::creatureFightSystem::has_value())
	{
		return std::nullopt;
	}
	return thing;
}

void IsFighting() // 229 IS_FIGHTING
{
	const auto object = PopObject();
	if (!Locator::entitiesRegistry::value().Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "IS_FIGHTING: thing not found");
	}
	const auto creature = FightCreature("IS_FIGHTING", object);
	Pushb(creature.has_value() && Locator::creatureFightSystem::value().IsFighting(*creature));
}

void SetMagicRadius() // 230 SET_MAGIC_RADIUS
{
	// const auto radius = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void TempTextWithNumber() // 231 TEMP_TEXT_WITH_NUMBER
{
	// const auto withInteraction = Pop().intVal;
	// const auto value = Popf();
	// const auto format = PopString();
	// const auto singleLine = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void RunTextWithNumber() // 232 RUN_TEXT_WITH_NUMBER
{
	// const auto withInteraction = Pop().intVal;
	// const auto number = Popf();
	// const auto string = Pop().intVal;
	// const auto singleLine = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureSpellReversion() // 233 CREATURE_SPELL_REVERSION
{
	// Whether the spells on a creature put it back as it was once they wear off. The creature is on top of the stack, the
	// flag under it.
	const auto object = PopObject();
	const auto enable = Pop().intVal != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not found");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not creature");
		return;
	}
	auto* spells = registry.TryGet<ecs::components::CreatureSpells>(object);
	if (spells == nullptr)
	{
		spells = &registry.Assign<ecs::components::CreatureSpells>(object);
	}
	spells->spells.reversion = enable;
}

void GetDesire() // 234 GET_DESIRE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetEventsPerSecond() // 235 GET_EVENTS_PER_SECOND
{
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetTimeSince() // 236 GET_TIME_SINCE
{
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetTotalEvents() // 237 GET_TOTAL_EVENTS
{
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void UpdateSnapshot() // 238 UPDATE_SNAPSHOT
{
	// const auto challengeId = Pop().intVal;
	// const auto argc = Pop().intVal;
	// const auto argv = PopVarArg(argc);
	// const auto reminderScript = PopString();
	// const auto titleStrID = Pop().intVal;
	// const auto alignment = Popf();
	// const auto success = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

/// The kinds of reward a script can give are numbered 1 to 60
constexpr int32_t k_LastRewardType = 60;

/// A reward chest of a kind for the player at this computer, given to a town or none: on the land, or from the sky
uint32_t GiveReward(int32_t type, glm::vec3 position, entt::entity town, bool fromSky)
{
	if (type < 1 || type > k_LastRewardType)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATE_REWARD: invalid type {}", type);
		return 0;
	}
	if (!Locator::rewardSystem::has_value())
	{
		return 0;
	}
	const auto player =
	    Locator::playerSystem::has_value() ? std::optional(Locator::playerSystem::value().GetLocalPlayer()) : std::nullopt;
	const auto chest =
	    Locator::rewardSystem::value().Create(position, static_cast<RewardObjectInfo>(type), player, town, fromSky);
	RegisterCreated(chest);
	return static_cast<uint32_t>(chest);
}

void CreateReward() // 239 CREATE_REWARD
{
	const auto fromSky = Pop().intVal != 0;
	const auto position = PopVec();
	const auto type = Pop().intVal;
	Pusho(GiveReward(type, position, entt::null, fromSky));
}

void CreateRewardInTown() // 240 CREATE_REWARD_IN_TOWN
{
	const auto fromSky = Pop().intVal != 0;
	const auto position = PopVec();
	const auto town = PopObject();
	const auto type = Pop().intVal;
	// The town must be one
	if (!Locator::entitiesRegistry::value().Valid(town) ||
	    !Locator::entitiesRegistry::value().AllOf<ecs::components::Town>(town))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATE_REWARD_IN_TOWN: not a town");
		Pusho(0);
		return;
	}
	Pusho(GiveReward(type, position, town, fromSky));
}

void SetFade() // 241 SET_FADE
{
	// The game takes each as a whole number, the colour's as bytes and the seconds as a small signed one
	const auto time = static_cast<int8_t>(static_cast<int32_t>(Popf()));
	const auto blue = static_cast<uint8_t>(static_cast<int32_t>(Popf()));
	const auto green = static_cast<uint8_t>(static_cast<int32_t>(Popf()));
	const auto red = static_cast<uint8_t>(static_cast<int32_t>(Popf()));
	Locator::cinematicDirectorSystem::value().FadeTo(red, green, blue, time);
}

void SetFadeIn() // 242 SET_FADE_IN
{
	const auto duration = static_cast<int8_t>(static_cast<int32_t>(Popf()));
	Locator::cinematicDirectorSystem::value().FadeBackToNormal(duration);
}

void FadeFinished() // 243 FADE_FINISHED
{
	Pushb(Locator::cinematicDirectorSystem::value().IsFadeFinished());
}

void SetPlayerMagic() // 244 SET_PLAYER_MAGIC
{
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void HasPlayerMagic() // 245 HAS_PLAYER_MAGIC
{
	// const auto player = Popf();
	// const auto spell = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void SpiritSpeaks() // 246 SPIRIT_SPEAKS
{
	const auto text = Pop().uintVal;
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (text >= help::k_HelpTextCount)
	{
		Pushb(false);
		return;
	}
	const auto narrator = Locator::helpTextSystem::value().GetNarrator(text);
	Pushb(help::script_spirits::SpiritWhoTalks(narrator) == spirit);
}

void BeliefForPlayer() // 247 BELIEF_FOR_PLAYER
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object) || player >= PlayerNames::_COUNT)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object or player no longer valid");
		Pushf(0.0f);
		return;
	}
	const auto* town = registry.TryGet<const ecs::components::Town>(object);
	std::optional<float> townBelief;
	if (town != nullptr)
	{
		const auto belief = town->beliefs.find(std::string(k_PlayerNamesStrs.at(static_cast<size_t>(player))));
		if (belief != town->beliefs.end())
		{
			townBelief = belief->second;
		}
	}
	Pushf(script::property_rules::BeliefForPlayer(town != nullptr, townBelief, ecs::world_objects::PlayerOf(object), player));
}

void GetHelp() // 248 GET_HELP
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SetLeashWorks() // 249 SET_LEASH_WORKS
{
	const auto creature = PopObject();
	const auto enable = Pop().intVal != 0;
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	leashes.SetWorks(creature, enable);
}

void LoadMyCreature() // 250 LOAD_MY_CREATURE
{
	// The player's creature, as kept when a land was last cleared, comes out at the place, unless they have one already
	const auto position = PopVec();
	Locator::creatureCarryOverSystem::value().LoadPlayersCreature({position.x, position.z});
}

void ObjectRelativeBelief() // 251 OBJECT_RELATIVE_BELIEF
{
	// const auto belief = Popf();
	// const auto player = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreateWithAngleAndScale() // 252 CREATE_WITH_ANGLE_AND_SCALE
{
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = static_cast<ObjectType>(Pop().intVal);
	const auto scale = Popf();
	const auto angle = Popf();

	const entt::entity object = CreateScriptObject(type, subtype, position, 0.0f, 0.0f, angle, 0.0f, scale);
	RegisterCreated(object);

	PushObject(object);
}

void SetHelpSystem() // 253 SET_HELP_SYSTEM
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetVirtualInfluence() // 254 SET_VIRTUAL_INFLUENCE
{
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	const auto enable = Pop().intVal != 0;
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "The hand's virtual influence for player {} is turned {}",
	                    static_cast<int>(player), enable ? "on" : "off");
	// Turned off, the player's hand keeps nothing of their influence past the border, and loses what it had
	if (!Locator::playerSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = Locator::playerSystem::value().GetPlayer(player);
	if (!registry.Valid(entity))
	{
		return;
	}
	auto& state =
	    (registry.AnyOf<ecs::components::VirtualInfluence>(entity) ? registry.Get<ecs::components::VirtualInfluence>(entity)
	                                                               : registry.Assign<ecs::components::VirtualInfluence>(entity))
	        .state;
	state.disabled = !enable;
	if (state.disabled)
	{
		state.fraction = 0.0f;
	}
}

void SetActive() // 255 SET_ACTIVE
{
	const auto object = PopObject();
	const auto active = Pop().intVal != 0;
	if (object == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object no longer valid");
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<ecs::components::ScriptHighlight>(object))
	{
		Locator::scriptHighlightSystem::value().SetActive(object, active);
		return;
	}
	// A dispenser set active floats a new bubble at once
	if (registry.AllOf<ecs::components::SpellDispenser>(object))
	{
		Locator::magicSystem::value().SetDispenserActive(object, active);
		return;
	}
	// TODO(script-natives): a scaffold set active is built at once, for the script's player; openblack has no scaffolds
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid object type");
}

void ThingValid() // 256 THING_VALID
{
	// No object, one that has gone or one no longer to be dealt with is not valid; a town is valid while any of it stands
	const auto object = PopObject();
	const auto* town =
	    object != entt::null ? Locator::entitiesRegistry::value().TryGet<const ecs::components::Town>(object) : nullptr;
	Pushb(object != entt::null && (town == nullptr || !TownCompletelyDestroyed(*town)));
}

void VortexFadeOut() // 257 VORTEX_FADE_OUT
{
	// The vortex starts to fade out, whatever it was doing. Nothing given at all is an error; a thing that isn't a vortex
	// is one too.
	DataType type {};
	const auto value = Pop(type);
	// A script's object 0 is none, as PopObject reads it
	const auto vortex = value.uintVal == 0 ? entt::entity {entt::null}
	                                       : Locator::scriptObjects::value().Fetch(static_cast<entt::entity>(value.uintVal));
	if (vortex == entt::null || !Locator::entitiesRegistry::value().Valid(vortex))
	{
		if (type == DataType::None)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "vortex fade out failed");
		}
		return;
	}
	if (!Locator::vortexSystem::value().StartFadeOut(vortex))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not vortex");
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "vortex fade out failed");
	}
}

void RemoveReactionOfType() // 258 REMOVE_REACTION_OF_TYPE
{
	// const auto reaction = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureLearnEverythingExcluding() // 259 CREATURE_LEARN_EVERYTHING_EXCLUDING
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void PlayedPercentage() // 260 PLAYED_PERCENTAGE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void ObjectCastByObject() // 261 OBJECT_CAST_BY_OBJECT
{
	// const auto caster = Pop().uintVal;
	// const auto spellInstance = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void IsWindMagicAtPos() // 262 IS_WIND_MAGIC_AT_POS
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void CreateMist() // 263 CREATE_MIST
{
	// const auto heightRatio = Popf();
	// const auto transparency = Popf();
	// const auto b = Popf();
	// const auto g = Popf();
	// const auto r = Popf();
	// const auto scale = Popf();
	// const auto pos = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SetMistFade() // 264 SET_MIST_FADE
{
	// const auto duration = Popf();
	// const auto endTransparency = Popf();
	// const auto startTransparency = Popf();
	// const auto endScale = Popf();
	// const auto startScale = Popf();
	// const auto mist = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetObjectFade() // 265 GET_OBJECT_FADE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void PlayHandDemo() // 266 PLAY_HAND_DEMO
{
	// const auto withoutHandModify = static_cast<bool>(Pop().intVal);
	// const auto withPause = static_cast<bool>(Pop().intVal);
	// const auto string = PopString();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void IsPlayingHandDemo() // 267 IS_PLAYING_HAND_DEMO
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void GetArsePosition() // 268 GET_ARSE_POSITION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void IsLeashedToObject() // 269 IS_LEASHED_TO_OBJECT
{
	const auto target = PopObject();
	const auto object = PopObject();
	Pushb(Locator::leashSystem::has_value() && Locator::leashSystem::value().TiedTo(object) == target);
}

void GetInteractionMagnitude() // 270 GET_INTERACTION_MAGNITUDE
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void IsCreatureAvailable() // 271 IS_CREATURE_AVAILABLE
{
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void CreateHighlight() // 272 CREATE_HIGHLIGHT
{
	const auto challenge = Pop().uintVal;
	const auto position = PopVec();
	const auto kind = Pop().uintVal;
	// A kind past the info table makes none (the game reads past its table)
	const auto highlight = Locator::scriptHighlightSystem::value().Create(kind, position, challenge);
	if (highlight == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Highlight not created");
		PushObject(entt::null);
		return;
	}
	RegisterCreated(highlight);
	PushObject(highlight);
}

void GetObjectHeld273() // 273 GET_OBJECT_HELD
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void GetActionCount() // 274 GET_ACTION_COUNT
{
	// const auto creature = Pop().uintVal;
	// const auto action = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetObjectLeashType() // 275 GET_OBJECT_LEASH_TYPE
{
	const auto object = PopObject();
	// The scripts count no leash as 0
	const auto type = Locator::leashSystem::has_value() ? Locator::leashSystem::value().TypeOf(object) : LeashType::None;
	Pushi(type == LeashType::None ? 0 : static_cast<int32_t>(type));
}

/// The thing a following command is given, and the script's camera, each complained of when missing
std::pair<entt::entity, ScriptCameraModel*> FollowCommand(entt::entity thing)
{
	auto* camera = Locator::scriptControlSystem::value().GetScriptCamera(Locator::camera::value());
	if (camera == nullptr)
	{
		ScriptMessage("Script camera has been removed! - Exception happened?");
	}
	const bool valid = thing != entt::null && Locator::entitiesRegistry::value().Valid(thing);
	if (!valid)
	{
		ScriptMessage("Object no longer valid");
	}
	return {valid ? thing : entt::null, camera};
}

void SetFocusFollow() // 276 SET_FOCUS_FOLLOW
{
	const auto [thing, camera] = FollowCommand(PopObject());
	if (thing != entt::null && camera != nullptr)
	{
		// The script's camera keeps looking at the thing
		camera->LookAt(FollowedThing(thing));
	}
}

void SetPositionFollow() // 277 SET_POSITION_FOLLOW
{
	const auto [thing, camera] = FollowCommand(PopObject());
	if (thing != entt::null && camera != nullptr)
	{
		// The script's camera is put behind the thing, as far as eight times its height, and follows it
		camera->Follow(FollowedThing(thing));
	}
}

void SetFocusAndPositionFollow() // 278 SET_FOCUS_AND_POSITION_FOLLOW
{
	const auto distance = Popf();
	const auto [thing, camera] = FollowCommand(PopObject());
	if (thing != entt::null && camera != nullptr)
	{
		// The script's camera is put at a distance from the thing, follows it and looks at it
		camera->FollowAndLookAt(FollowedThing(thing), distance);
	}
}

void SetCameraLens() // 279 SET_CAMERA_LENS
{
	// const auto lens = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void MoveCameraLens() // 280 MOVE_CAMERA_LENS
{
	// const auto time = Popf();
	// const auto lens = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureReaction() // 281 CREATURE_REACTION
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureInDevScript() // 282 CREATURE_IN_DEV_SCRIPT
{
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StoreCameraDetails() // 283 STORE_CAMERA_DETAILS
{
	// TODO(Daniels118): implement this
	NotImplemented();
}

void RestoreCameraDetails() // 284 RESTORE_CAMERA_DETAILS
{
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StartAngleSound285() // 285 START_ANGLE_SOUND
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetCameraPosFocLens() // 286 SET_CAMERA_POS_FOC_LENS
{
	// const auto unk6 = Pop().intVal;
	// const auto unk5 = Pop().intVal;
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void MoveCameraPosFocLens() // 287 MOVE_CAMERA_POS_FOC_LENS
{
	// const auto unk7 = Pop().intVal;
	// const auto unk6 = Pop().intVal;
	// const auto unk5 = Pop().intVal;
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GameTimeOnOff() // 288 GAME_TIME_ON_OFF
{
	const auto enable = Pop().intVal != 0;
	Locator::skySystem::value().GetClock().SetRunning(enable);
}

void MoveGameTime() // 289 MOVE_GAME_TIME
{
	const auto duration = Popf();
	const auto hourOfTheDay = Popf();
	Locator::skySystem::value().GetClock().MoveScriptTime(hourOfTheDay, duration);
}

void SetHighGraphicsDetail() // 290 SET_HIGH_GRAPHICS_DETAIL
{
	const auto object = PopObject();
	const auto enable = Pop().intVal != 0;
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not found");
		return;
	}
	// The thing is drawn in high detail for the script's cinema, or as usual again
	// TODO(opening): the high-detail drawing itself: the eyes and their blinking, the eased turning and the blending
	// between clips
	auto& highDetail = Locator::highDetailSystem::value();
	if (enable)
	{
		highDetail.Make(object);
	}
	else
	{
		highDetail.Release(object);
	}
}

void SetSkeleton() // 291 SET_SKELETON
{
	// const auto object = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void IsSkeleton() // 292 IS_SKELETON
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void PlayerSpellCastTime() // 293 PLAYER_SPELL_CAST_TIME
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void PlayerSpellLastCast() // 294 PLAYER_SPELL_LAST_CAST
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void GetLastSpellCastPos() // 295 GET_LAST_SPELL_CAST_POS
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void AddSpotVisualTargetPos() // 296 ADD_SPOT_VISUAL_TARGET_POS
{
	// const auto position = PopVec();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void AddSpotVisualTargetObject() // 297 ADD_SPOT_VISUAL_TARGET_OBJECT
{
	// const auto target = Pop().uintVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetIndestructable() // 298 SET_INDESTRUCTABLE
{
	const auto object = PopObject();
	const bool indestructible = (Pop().uintVal & 1u) != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_INDESTRUCTABLE: thing not valid");
		return;
	}
	const auto mark = [&registry, indestructible](entt::entity thing) {
		if (indestructible)
		{
			registry.AssignOrReplace<ecs::components::Indestructible>(thing);
		}
		else
		{
			registry.Remove<ecs::components::Indestructible>(thing);
		}
	};
	// A town is marked through every villager of it; anything else itself
	if (registry.AllOf<ecs::components::Town>(object))
	{
		registry.Each<const ecs::components::Villager>(
		    [&mark, object](entt::entity villager, const ecs::components::Villager& person) {
			    if (person.town == object)
			    {
				    mark(villager);
			    }
		    });
		return;
	}
	mark(object);
}

void SetGraphicsClipping() // 299 SET_GRAPHICS_CLIPPING
{
	// Only the first argument counts: whether the camera clips close
	Pop();
	const auto close = Pop().uintVal != 0;
	Locator::cinematicDirectorSystem::value().SetCloseClipping(close);
}

void SpiritAppear() // 300 SPIRIT_APPEAR
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritEject(spirit, true);
	}
}

void SpiritDisappear() // 301 SPIRIT_DISAPPEAR
{
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		advisors->SpiritHome(spirit, true);
	}
}

void SetFocusOnObject() // 302 SET_FOCUS_ON_OBJECT
{
	// const auto target = Pop().uintVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ReleaseObjectFocus() // 303 RELEASE_OBJECT_FOCUS
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ImmersionExists() // 304 IMMERSION_EXISTS
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void SetDrawLeash() // 305 SET_DRAW_LEASH
{
	const auto enable = Pop().intVal != 0;
	if (Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().SetDrawn(enable);
	}
}

void SetDrawHighlight() // 306 SET_DRAW_HIGHLIGHT
{
	// The challenge scrolls show or hide; the signs always show
	Locator::chlapi::value().SetHighlightDrawOn(Pop().intVal != 0);
}

void SetOpenClose() // 307 SET_OPEN_CLOSE
{
	const auto object = PopObject();
	// The script's word is kept as it is: 1 opens, 0 closes
	const auto open = static_cast<int32_t>(Pop().uintVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_OPEN_CLOSE: thing not found");
		return;
	}
	if (!Locator::animatedStaticSystem::has_value() || !Locator::animatedStaticSystem::value().SetOpenState(object, open))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_OPEN_CLOSE: thing must be an animated static");
	}
}

void SetIntroBuilding() // 308 SET_INTRO_BUILDING
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureForceFriends() // 309 CREATURE_FORCE_FRIENDS
{
	// const auto targetCreature = Pop().uintVal;
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void MoveComputerPlayerPosition() // 310 MOVE_COMPUTER_PLAYER_POSITION
{
	// const auto withFixedHeight = static_cast<bool>(Pop().intVal);
	// const auto speed = Popf();
	// const auto position = PopVec();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void EnableDisableComputerPlayer311() // 311 ENABLE_DISABLE_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetComputerPlayerPosition() // 312 GET_COMPUTER_PLAYER_POSITION
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SetComputerPlayerPosition() // 313 SET_COMPUTER_PLAYER_POSITION
{
	// const auto withFixedHeight = static_cast<bool>(Pop().intVal);
	// const auto position = PopVec();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetStoredCameraPosition() // 314 GET_STORED_CAMERA_POSITION
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void GetStoredCameraFocus() // 315 GET_STORED_CAMERA_FOCUS
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void CallNearInState() // 316 CALL_NEAR_IN_STATE
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto radius = Popf();
	// const auto position = PopVec();
	// const auto state = Pop().intVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SetCreatureSound() // 317 SET_CREATURE_SOUND
{
	// Off, the creatures of players other than this computer's are not heard in their own voices
	const auto enable = Pop().intVal != 0;
	if (Locator::creatureAudioSystem::has_value())
	{
		Locator::creatureAudioSystem::value().SetOtherVoicesEnabled(enable);
	}
}

void CreatureInteractingWith() // 318 CREATURE_INTERACTING_WITH
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void SetSunDraw() // 319 SET_SUN_DRAW
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ObjectInfoBits() // 320 OBJECT_INFO_BITS
{
	// What the gate stones laid in a plinth are worth: ape 1, tiger 2, cow 4. Asked of anything else, the original
	// reports the error and pushes no answer at all, so the script's next pop finds whatever lies below.
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "OBJECT_INFO_BITS: thing not valid");
		return;
	}
	const auto value = Locator::animatedStaticSystem::has_value()
	                       ? Locator::animatedStaticSystem::value().GateStoneValue(object)
	                       : std::nullopt;
	if (!value.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "OBJECT_INFO_BITS: thing must be an animated static");
		return;
	}
	Pushf(static_cast<float>(*value));
}

void SetHurtByFire() // 321 SET_HURT_BY_FIRE
{
	const auto object = PopObject();
	const auto enable = static_cast<bool>(Pop().intVal);
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().SetHurtByFire(object, enable);
	}
}

void ConfinedObject() // 322 CONFINED_OBJECT
{
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ClearConfinedObject() // 323 CLEAR_CONFINED_OBJECT
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetObjectFlock() // 324 GET_OBJECT_FLOCK
{
	// const auto member = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SetPlayerBelief() // 325 SET_PLAYER_BELIEF
{
	// const auto belief = Popf();
	// const auto player = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void PlayJcSpecial() // 326 PLAY_JC_SPECIAL
{
	const auto special = Pop().intVal;
	switch (special)
	{
	case 14:
	case 15:
		// The camera's bookmarks are shown and taken again, or put away
		Locator::cameraBookmarkSystem::value().SetEnabled(special == 14);
		return;
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		// TODO(opening): the opening's light from the sky, its camera, the hand that lifts the boy from the sea and the
		// missionaries' boat
		NotImplemented(special);
		return;
	default:
		// The others do nothing
		return;
	}
}

void IsPlayingJcSpecial() // 327 IS_PLAYING_JC_SPECIAL
{
	// const auto feature = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void VortexParameters() // 328 VORTEX_PARAMETERS
{
	// const auto flock = Pop().uintVal;
	// const auto radius = Popf();
	// const auto distance = Popf();
	// const auto position = PopVec();
	// const auto town = Pop().uintVal;
	// const auto vortex = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void LoadCreature() // 329 LOAD_CREATURE
{
	// const auto position = PopVec();
	// const auto player = Popf();
	// const auto mindFilename = PopString();
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void IsSpellCharging() // 330 IS_SPELL_CHARGING
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void IsThatSpellCharging() // 331 IS_THAT_SPELL_CHARGING
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void OpposingCreature() // 332 OPPOSING_CREATURE
{
	// The species a god's creature is against the player's creature, from the game's table, as the scripts number them
	const auto god = Pop().uintVal;
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	const auto creature =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetPrimaryCreature(player) : std::nullopt;
	const auto* body =
	    creature.has_value() ? Locator::entitiesRegistry::value().TryGet<const ecs::components::Creature>(*creature) : nullptr;
	const auto& table = Locator::infoConstants::value().scriptOpposingCreature;
	const auto row = body != nullptr ? script::property_rules::ScriptCreatureType(body->species) : 0;
	constexpr uint32_t k_Gods = 3;
	if (body == nullptr || row >= table.size() || god >= k_Gods)
	{
		// The game reads its table regardless (and without a creature fails)
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "No opposing creature for god {}", god);
		Pushi(0);
		return;
	}
	const auto& opposing = table.at(row);
	const std::array<uint32_t, k_Gods> types {opposing.field0x0, opposing.field0x4, opposing.field0x8};
	Pushi(static_cast<int32_t>(types.at(god)));
}

void FlockWithinLimits() // 333 FLOCK_WITHIN_LIMITS
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void HighlightProperties() // 334 HIGHLIGHT_PROPERTIES
{
	const auto category = Pop().uintVal;
	const auto text = Pop().uintVal;
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().AllOf<ecs::components::ScriptHighlight>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not valid");
		return;
	}
	Locator::scriptHighlightSystem::value().SetProperties(object, text, category);
}

void LastMusicLine() // 335 LAST_MUSIC_LINE
{
	// const auto line = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void HandDemoTrigger() // 336 HAND_DEMO_TRIGGER
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void GetBellyPosition() // 337 GET_BELLY_POSITION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SetCreatureCreedProperties() // 338 SET_CREATURE_CREED_PROPERTIES
{
	// const auto time = Popf();
	// const auto power = Popf();
	// const auto scale = Popf();
	// const auto handGlow = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GameThingCanViewCamera() // 339 GAME_THING_CAN_VIEW_CAMERA
{
	// const auto degrees = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void GamePlaySaySoundEffect() // 340 GAME_PLAY_SAY_SOUND_EFFECT
{
	// Says a help text's line, on the second voice for "extra", heard from the position when it has one
	const auto withPosition = Pop().intVal != 0;
	const auto position = PopVec();
	const auto text = static_cast<uint32_t>(Pop().intVal);
	const auto extra = Pop().intVal != 0;
	Locator::helpSpeechSystem::value().Say(text, extra ? audio::SpeechVoice::Second : audio::SpeechVoice::First,
	                                       withPosition ? std::optional(position) : std::nullopt);
}

void SetTownDesireBoost() // 341 SET_TOWN_DESIRE_BOOST
{
	// A town wants one of its desires that much more, or less, until a script changes it again; its order of desires is
	// put right at once
	const auto boost = Popf();
	const auto desire = Pop().intVal;
	const auto town = PopObject();
	const auto& registry = Locator::entitiesRegistry::value();
	const bool isTown = town != entt::null && registry.Valid(town) && registry.AllOf<ecs::components::Town>(town);
	if (!isTown)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_TOWN_DESIRE_BOOST: object not a town");
	}
	if (!ecs::town_desire::ValidScriptBoost(desire, boost))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_TOWN_DESIRE_BOOST: desire {} or boost {} out of range", desire,
		                    boost);
		return;
	}
	if (isTown)
	{
		Locator::townDesireSystem::value().SetBoost(town, static_cast<TownDesireInfo>(desire), boost, true);
	}
}

void IsLockedInteraction() // 342 IS_LOCKED_INTERACTION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void SetCreatureName() // 343 SET_CREATURE_NAME
{
	// const auto textID = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ComputerPlayerReady() // 344 COMPUTER_PLAYER_READY
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void EnableDisableComputerPlayer345() // 345 ENABLE_DISABLE_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// const auto pause = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ClearActorMind() // 346 CLEAR_ACTOR_MIND
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void EnterExitCitadel() // 347 ENTER_EXIT_CITADEL
{
	const auto enter = static_cast<bool>(Pop().intVal);
	if (Locator::temple::has_value())
	{
		auto& temple = Locator::temple::value();
		const auto active = temple.Active();
		if (enter != active)
		{
			if (enter)
			{
				temple.Activate();
			}
			else
			{
				temple.Deactivate();
			}
		}
	}
	else
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "No temple");
	}
}

void StartAngleSound348() // 348 START_ANGLE_SOUND
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ThingJcSpecial() // 349 THING_JC_SPECIAL
{
	const auto target = PopObject();
	const auto special = static_cast<ecs::high_detail_rules::ThingSpecial>(Pop().intVal);
	const auto on = Pop().intVal != 0;
	if (target == entt::null || !Locator::entitiesRegistry::value().Valid(target))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object no longer valid");
		return;
	}
	// The orders are for a thing drawn in high detail and are ignored by anything else; releasing the opening's own
	// specials releases nothing while openblack doesn't make them
	Locator::highDetailSystem::value().Order(target, special, on);
	if (special == ecs::high_detail_rules::ThingSpecial::DrawnObjectSpecial)
	{
		// TODO(opening): the drawn object's own special
		NotImplemented(static_cast<int32_t>(special));
	}
}

void MusicPlayed350() // 350 MUSIC_PLAYED
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void UpdateSnapshotPicture() // 351 UPDATE_SNAPSHOT_PICTURE
{
	// const auto challengeID = Pop().intVal;
	// const auto takingPicture = static_cast<bool>(Pop().intVal);
	// const auto titleStrID = Pop().intVal;
	// const auto alignment = Popf();
	// const auto success = Popf();
	// const auto focus = PopVec();
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StopScriptsInFilesExcluding() // 352 STOP_SCRIPTS_IN_FILES_EXCLUDING
{
	const auto scriptNames = PopString();
	const auto sourceFilenames = PopString();

	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&scriptNames, &sourceFilenames](const std::string& name, const std::string& filename) -> bool {
		return script::name_lists::HoldsFile(sourceFilenames, filename) && !script::name_lists::HoldsScript(scriptNames, name);
	});
}

void CreateRandomVillagerOfTribe() // 353 CREATE_RANDOM_VILLAGER_OF_TRIBE
{
	// const auto position = PopVec();
	// const auto tribe = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void ToggleLeash() // 354 TOGGLE_LEASH
{
	const auto player = static_cast<PlayerNames>(Pop().intVal);
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	// As the player's leash key does, refused and logged when they have no creature to lead
	Locator::leashSystem::value().PressKey(player, creature_leash::LeashKey::Leash);
}

void GameSetMana() // 355 GAME_SET_MANA
{
	// const auto mana = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetMagicProperties() // 356 SET_MAGIC_PROPERTIES
{
	// const auto duration = Popf();
	// const auto magicType = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetGameSound() // 357 SET_GAME_SOUND
{
	// Turned off, every sound effect playing stops and from then only the advisors' and villagers' speech plays
	const auto enable = Pop().intVal != 0;
	if (!enable)
	{
		Locator::audio::value().StopAllSoundEffects();
	}
	Locator::chlapi::value().SetGameSoundOn(enable);
}

void SexIsMale() // 358 SEX_IS_MALE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void GetFirstHelp() // 359 GET_FIRST_HELP
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetLastHelp() // 360 GET_LAST_HELP
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void IsActive() // 361 IS_ACTIVE
{
	const auto object = PopObject();
	if (object == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object no longer valid");
		Pushb(false);
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* highlight = registry.TryGet<const ecs::components::ScriptHighlight>(object); highlight != nullptr)
	{
		Pushb(highlight->active);
		return;
	}
	if (const auto* dispenser = registry.TryGet<const ecs::components::SpellDispenser>(object); dispenser != nullptr)
	{
		Pushb(dispenser->timer.active);
		return;
	}
	// A reward is active once the player has tapped it open.
	// TODO(script-natives): openblack's rewards can't be tapped open yet, so none is ever active
	// Nothing else is ever active
	Pushb(false);
}

void SetBookmarkPosition() // 362 SET_BOOKMARK_POSITION
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetScaffoldProperties() // 363 SET_SCAFFOLD_PROPERTIES
{
	// const auto destroy = static_cast<bool>(Pop().intVal);
	// const auto size = Popf();
	// const auto type = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetComputerPlayerPersonality() // 364 SET_COMPUTER_PLAYER_PERSONALITY
{
	// const auto probability = Popf();
	// const auto aspect = PopString();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetComputerPlayerSuppression() // 365 SET_COMPUTER_PLAYER_SUPPRESSION
{
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ForceComputerPlayerAction() // 366 FORCE_COMPUTER_PLAYER_ACTION
{
	// const auto obj2 = Pop().uintVal;
	// const auto obj1 = Pop().uintVal;
	// const auto action = PopString();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void QueueComputerPlayerAction() // 367 QUEUE_COMPUTER_PLAYER_ACTION
{
	// const auto obj2 = Pop().uintVal;
	// const auto obj1 = Pop().uintVal;
	// const auto action = PopString();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetTownWithId() // 368 GET_TOWN_WITH_ID
{
	// const auto id = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SetDisciple() // 369 SET_DISCIPLE
{
	// const auto withSound = static_cast<bool>(Pop().intVal);
	// const auto discipleType = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ReleaseComputerPlayer() // 370 RELEASE_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetComputerPlayerSpeed() // 371 SET_COMPUTER_PLAYER_SPEED
{
	// const auto speed = Popf();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetFocusFollowComputerPlayer() // 372 SET_FOCUS_FOLLOW_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetPositionFollowComputerPlayer() // 373 SET_POSITION_FOLLOW_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CallComputerPlayer() // 374 CALL_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void CallBuildingInTown() // 375 CALL_BUILDING_IN_TOWN
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void SetCanBuildWorshipsite() // 376 SET_CAN_BUILD_WORSHIPSITE
{
	// A town or a temple is let have worship sites made, or stopped
	const auto object = PopObject();
	const auto enable = Pop().intVal != 0;
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().SetCanHaveSites(object, enable);
	}
}

void GetFacingCameraPosition() // 377 GET_FACING_CAMERA_POSITION
{
	// const auto distance = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SetComputerPlayerAttitude() // 378 SET_COMPUTER_PLAYER_ATTITUDE
{
	// const auto attitude = Popf();
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetComputerPlayerAttitude() // 379 GET_COMPUTER_PLAYER_ATTITUDE
{
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void LoadComputerPlayerPersonality() // 380 LOAD_COMPUTER_PLAYER_PERSONALITY
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SaveComputerPlayerPersonality() // 381 SAVE_COMPUTER_PLAYER_PERSONALITY
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetPlayerAlly() // 382 SET_PLAYER_ALLY
{
	// const auto percentage = Popf();
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CallFlying() // 383 CALL_FLYING
{
	// const auto excluding = static_cast<bool>(Pop().intVal);
	// const auto radius = Popf();
	// const auto position = PopVec();
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SetObjectFadeIn() // 384 SET_OBJECT_FADE_IN
{
	// A creature drops out of sight at once (with the energise sound) and fizzes back in over the seconds given. The
	// game fades nothing else in: any other object is only reported.
	const auto seconds = Popf();
	const auto object = PopObject();
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Object dead man!");
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_OBJECT_FADE_IN: only creatures fade in");
		return;
	}
	auto& fizz = Locator::creatureFizzSystem::value();
	fizz.SetFizz(object, 1.0f, 0.0f, false);
	fizz.SetFizz(object, 0.0f, seconds, false);
}

void IsAffectedBySpell() // 385 IS_AFFECTED_BY_SPELL
{
	// const auto spell = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void SetMagicInObject() // 386 SET_MAGIC_IN_OBJECT
{
	// const auto object = Pop().uintVal;
	// const auto MAGIC_TYPE = Pop().intVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void IdAdultSize() // 387 ID_ADULT_SIZE
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void ObjectCapacity() // 388 OBJECT_CAPACITY
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void ObjectAdultCapacity() // 389 OBJECT_ADULT_CAPACITY
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SetCreatureAutoFighting() // 390 SET_CREATURE_AUTO_FIGHTING
{
	// On, the computer fights the creature until the player makes a move for it; off, nobody does and it makes only the
	// moves queued for it
	const auto object = PopObject();
	const bool enable = Pop().intVal != 0;
	if (const auto creature = FightCreature("SET_CREATURE_AUTO_FIGHTING", object))
	{
		Locator::creatureFightSystem::value().SetAutoFighting(*creature, enable);
	}
}

void IsAutoFighting() // 391 IS_AUTO_FIGHTING
{
	const auto creature = FightCreature("IS_AUTO_FIGHTING", PopObject());
	Pushb(creature.has_value() && Locator::creatureFightSystem::value().IsAutoFighting(*creature));
}

/// A move a script adds to the end of a creature's fight queue, numbered as the scripts number them
void QueueScriptFightMove(const char* native, uint32_t value)
{
	const auto creature = FightCreature(native, PopObject());
	const auto move = creature_fight::ScriptMove(value);
	if (creature.has_value() && move.has_value())
	{
		Locator::creatureFightSystem::value().QueueMove(*creature, *move, false);
	}
}

void SetCreatureQueueFightMove() // 392 SET_CREATURE_QUEUE_FIGHT_MOVE
{
	const auto move = static_cast<uint32_t>(Pop().intVal);
	QueueScriptFightMove("SET_CREATURE_QUEUE_FIGHT_MOVE", move);
}

void SetCreatureQueueFightSpell() // 393 SET_CREATURE_QUEUE_FIGHT_SPELL
{
	const auto spell = static_cast<uint32_t>(Pop().intVal);
	QueueScriptFightMove("SET_CREATURE_QUEUE_FIGHT_SPELL", spell | creature_fight::k_ScriptSpellBit);
}

void SetCreatureQueueFightStep() // 394 SET_CREATURE_QUEUE_FIGHT_STEP
{
	const auto step = static_cast<uint32_t>(Pop().intVal);
	QueueScriptFightMove("SET_CREATURE_QUEUE_FIGHT_STEP", step | creature_fight::k_ScriptAnimationBit);
}

void GetCreatureFightAction() // 395 GET_CREATURE_FIGHT_ACTION
{
	const auto creature = FightCreature("GET_CREATURE_FIGHT_ACTION", PopObject());
	Pushi(creature.has_value() ? static_cast<int32_t>(Locator::creatureFightSystem::value().CurrentFightAction(*creature)) : 0);
}

void CreatureFightQueueHits() // 396 CREATURE_FIGHT_QUEUE_HITS
{
	const auto creature = FightCreature("CREATURE_FIGHT_QUEUE_HITS", PopObject());
	Pushf(creature.has_value() ? static_cast<float>(Locator::creatureFightSystem::value().QueuedBlows(*creature)) : 0.0f);
}

void SquareRoot() // 397 SQUARE_ROOT
{
	const auto value = Popf();
	auto root = 0.0f;
	if (value > 0.0f)
	{
		root = std::sqrt(value);
	}
	Pushf(root);
}

void GetPlayerAlly() // 398 GET_PLAYER_ALLY
{
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

/// The player a script names: its 0 is the player at this computer, the others count from 1
PlayerNames ScriptPlayerName(int32_t scriptPlayer)
{
	if (scriptPlayer != 0)
	{
		return static_cast<PlayerNames>(scriptPlayer - 1);
	}
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
}

ecs::components::Player* ScriptPlayer(float number)
{
	const auto name = ScriptPlayerName(static_cast<int32_t>(number));
	if (!Locator::playerSystem::has_value())
	{
		return nullptr;
	}
	const auto entity = Locator::playerSystem::value().GetPlayer(name);
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) ? registry.TryGet<ecs::components::Player>(entity) : nullptr;
}

void SetPlayerWindResistance() // 399 SET_PLAYER_WIND_RESISTANCE
{
	auto* player = ScriptPlayer(Popf());
	const auto resistance = Pop().uintVal;
	// What the player's hand throws flies without the air's drag while it is set
	if (player != nullptr)
	{
		player->windResistance = resistance;
	}
	Pushi(0);
}

void GetPlayerWindResistance() // 400 GET_PLAYER_WIND_RESISTANCE
{
	const auto* player = ScriptPlayer(Popf());
	// Of its two arguments the game reads only the player
	Pop();
	Pushi(player != nullptr ? static_cast<int32_t>(player->windResistance) : 0);
}

void PauseUnpauseClimateSystem() // 401 PAUSE_UNPAUSE_CLIMATE_SYSTEM
{
	const auto enable = Pop().intVal != 0;
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().SetClimateSystemEnabled(enable);
	}
}

void PauseUnpauseStormCreationInClimateSystem() // 402 PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM
{
	const auto enable = Pop().intVal != 0;
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().SetStormCreationEnabled(enable);
	}
}

void GetManaForSpell() // 403 GET_MANA_FOR_SPELL
{
	// const auto spell = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void KillStormsInArea() // 404 KILL_STORMS_IN_AREA
{
	const auto radius = Popf();
	const auto position = PopVec();
	// A miracle's storm comes back over its clouds at the next step, fading in again
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().KillStormsInArea(position, radius);
	}
}

void InsideTemple() // 405 INSIDE_TEMPLE
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void RestartObject() // 406 RESTART_OBJECT
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetGameTimeProperties() // 407 SET_GAME_TIME_PROPERTIES
{
	const auto percentageChange = Popf();
	const auto percentageNight = Popf();
	const auto duration = Popf();
	Locator::skySystem::value().GetClock().SetCycle(duration, percentageNight, percentageChange);
}

void ResetGameTimeProperties() // 408 RESET_GAME_TIME_PROPERTIES
{
	Locator::skySystem::value().GetClock().SetCycle(DayNightClock::k_DefaultDuration, DayNightClock::k_DefaultNight,
	                                                DayNightClock::k_DefaultChange);
}

void SoundExists() // 409 SOUND_EXISTS
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void GetTownWorshipDeaths() // 410 GET_TOWN_WORSHIP_DEATHS
{
	// const auto town = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GameClearDialogue() // 411 GAME_CLEAR_DIALOGUE
{
	Locator::helpTextSystem::value().ClearAllText();
}

void GameCloseDialogue() // 412 GAME_CLOSE_DIALOGUE
{
	Locator::helpTextSystem::value().CloseDialogue();
}

void GetHandState() // 413 GET_HAND_STATE
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void SetInterfaceCitadel() // 414 SET_INTERFACE_CITADEL
{
	// Whether tapping the temple's entrance takes the player inside
	Locator::entitiesRegistry::value().Context().scriptLetsTempleBeEntered = Pop().intVal != 0;
}

void MapScriptFunction() // 415 MAP_SCRIPT_FUNCTION
{
	// const auto command = PopString();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void WithinRotation() // 416 WITHIN_ROTATION
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void GetPlayerTownTotal() // 417 GET_PLAYER_TOWN_TOTAL
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SpiritScreenPoint() // 418 SPIRIT_SCREEN_POINT
{
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	CheckScreenFractions(x, y);
	if (auto* advisors = ScriptAdvisors(); advisors != nullptr)
	{
		const auto& screen = advisors->GetScreen();
		const auto pixel = glm::ivec2(static_cast<int>(static_cast<float>(screen.width) * x),
		                              static_cast<int>(static_cast<float>(screen.height) * y));
		advisors->SpiritScreenPoint(spirit, pixel);
	}
}

void KeyDown() // 419 KEY_DOWN
{
	// const auto key = Pop().intVal;
	// TODO(Daniels118): implement this (translate key to physical key code)
	NotImplemented();
	Pushb(false);
}

void SetFightExit() // 420 SET_FIGHT_EXIT
{
	// Whether the player may zoom out of the fight view, and the view ends by itself after a fight
	const bool allowed = Pop().intVal != 0;
	if (Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().SetFightExit(allowed);
	}
}

void GetObjectClicked() // 421 GET_OBJECT_CLICKED
{
	const auto* clicked = LocalHandClicked();
	const auto object = clicked != nullptr ? clicked->thing : entt::entity {entt::null};
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		Pusho(0);
		return;
	}
	// The thing found takes a place in the scripts' table, the game's own
	Locator::scriptObjects::value().Register(object, false);
	Pusho(static_cast<uint32_t>(object));
}

void GetMana() // 422 GET_MANA
{
	// const auto worshipSite = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void ClearPlayerSpellCharging() // 423 CLEAR_PLAYER_SPELL_CHARGING
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StopSoundEffect() // 424 STOP_SOUND_EFFECT
{
	// const auto soundbank = Pop().intVal;
	// const auto sound = Pop().intVal;
	// const auto alwaysFalse = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetTotemStatue() // 425 GET_TOTEM_STATUE
{
	// const auto town = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SetSetOnFire() // 426 SET_SET_ON_FIRE
{
	const auto object = PopObject();
	const auto enable = static_cast<bool>(Pop().intVal);
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().SetCanBeSetOnFire(object, enable);
	}
}

void SetLandBalance() // 427 SET_LAND_BALANCE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetObjectBeliefScale() // 428 SET_OBJECT_BELIEF_SCALE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StartImmersion() // 429 START_IMMERSION
{
	// const auto effect = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StopImmersion() // 430 STOP_IMMERSION
{
	// const auto effect = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StopAllImmersion() // 431 STOP_ALL_IMMERSION
{
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetCreatureInTemple() // 432 SET_CREATURE_IN_TEMPLE
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GameDrawText() // 433 GAME_DRAW_TEXT
{
	// const auto fade = Popf();
	// const auto size = Popf();
	// const auto height = Popf();
	// const auto width = Popf();
	// const auto down = Popf();
	// const auto across = Popf();
	// const auto textID = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GameDrawTempText() // 434 GAME_DRAW_TEMP_TEXT
{
	// const auto fade = Popf();
	// const auto size = Popf();
	// const auto height = Popf();
	// const auto width = Popf();
	// const auto down = Popf();
	// const auto across = Popf();
	// const auto string = PopString();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void FadeAllDrawText() // 435 FADE_ALL_DRAW_TEXT
{
	// const auto time = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetDrawTextColour() // 436 SET_DRAW_TEXT_COLOUR
{
	// const auto blue = Popf();
	// const auto green = Popf();
	// const auto red = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetClippingWindow() // 437 SET_CLIPPING_WINDOW
{
	// const auto time = Popf();
	// const auto height = Popf();
	// const auto width = Popf();
	// const auto down = Popf();
	// const auto across = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ClearClippingWindow() // 438 CLEAR_CLIPPING_WINDOW
{
	// const auto time = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SaveGameInSlot() // 439 SAVE_GAME_IN_SLOT
{
	// const auto slot = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetObjectCarrying() // 440 SET_OBJECT_CARRYING
{
	// const auto carriedObj = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void PosValidForCreature() // 441 POS_VALID_FOR_CREATURE
{
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void GetTimeSinceObjectAttacked() // 442 GET_TIME_SINCE_OBJECT_ATTACKED
{
	const auto town = PopObject();
	const auto player = ScriptPlayerName(static_cast<int32_t>(Popf()));
	auto& registry = Locator::entitiesRegistry::value();
	const auto* aggression = registry.Valid(town) ? registry.TryGet<const ecs::components::TownAggression>(town) : nullptr;
	const auto index = static_cast<size_t>(player);
	// The seconds since the player last attacked the town, while the town still holds more than a little against them
	constexpr double k_StillHeld = 0.15;
	if (aggression == nullptr || index >= aggression->record.aggression.size() ||
	    !(aggression->record.aggression.at(index) > k_StillHeld) || !Locator::time::has_value())
	{
		Pushf(std::numeric_limits<float>::max());
		return;
	}
	const auto turns = static_cast<int32_t>(Locator::time::value().GetTurn() - aggression->record.lastTurns.at(index));
	const auto msPerTurn =
	    std::chrono::duration_cast<std::chrono::milliseconds>(ecs::systems::TimeSystemInterface::k_TurnDuration);
	Pushf(static_cast<float>(turns * msPerTurn.count()) * 0.001f);
}

void GetTownAndVillagerHealthTotal() // 443 GET_TOWN_AND_VILLAGER_HEALTH_TOTAL
{
	// const auto town = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GameAddForBuilding() // 444 GAME_ADD_FOR_BUILDING
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void EnableDisableAlignmentMusic() // 445 ENABLE_DISABLE_ALIGNMENT_MUSIC
{
	const auto enable = Pop().intVal != 0;
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "The land's music is turned {}", enable ? "on" : "off");
	if (auto* gameMusic = Game::Instance()->GetGameMusic())
	{
		gameMusic->SetAlignmentMusicEnabled(enable);
	}
}

void GetDeadLiving() // 446 GET_DEAD_LIVING
{
	// const auto radius = Popf();
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void AttachSoundTag() // 447 ATTACH_SOUND_TAG
{
	// const auto target = Pop().uintVal;
	// const auto soundbank = Pop().intVal;
	// const auto sound = Pop().intVal;
	// const auto threeD = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void DetachSoundTag() // 448 DETACH_SOUND_TAG
{
	// const auto target = Pop().uintVal;
	// const auto soundbank = Pop().intVal;
	// const auto sound = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetSacrificeTotal() // 449 GET_SACRIFICE_TOTAL
{
	// const auto worshipSite = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GameSoundPlaying() // 450 GAME_SOUND_PLAYING
{
	// const auto soundbank = Pop().intVal;
	// const auto sound = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void GetTemplePosition() // 451 GET_TEMPLE_POSITION
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void CreatureAutoscale() // 452 CREATURE_AUTOSCALE
{
	// const auto size = Popf();
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetSpellIconInTemple() // 453 GET_SPELL_ICON_IN_TEMPLE
{
	// const auto temple = Pop().uintVal;
	// const auto spell = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void GameClearComputerPlayerActions() // 454 GAME_CLEAR_COMPUTER_PLAYER_ACTIONS
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetFirstInContainer() // 455 GET_FIRST_IN_CONTAINER
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void GetNextInContainer() // 456 GET_NEXT_IN_CONTAINER
{
	// const auto after = Pop().uintVal;
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void GetTempleEntrancePosition() // 457 GET_TEMPLE_ENTRANCE_POSITION
{
	// const auto height = Popf();
	// const auto radius = Popf();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SaySoundEffectPlaying() // 458 SAY_SOUND_EFFECT_PLAYING
{
	const auto text = static_cast<uint32_t>(Pop().intVal);
	const auto extra = Pop().intVal != 0;
	Pushb(Locator::helpSpeechSystem::value().IsSaying(text, extra ? audio::SpeechVoice::Second : audio::SpeechVoice::First));
}

void SetHandDemoKeys() // 459 SET_HAND_DEMO_KEYS
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CanSkipTutorial() // 460 CAN_SKIP_TUTORIAL
{
	Pushb(Locator::tutorialSkipSystem::value().Get().skipTutorial);
}

void CanSkipCreatureTraining() // 461 CAN_SKIP_CREATURE_TRAINING
{
	Pushb(Locator::tutorialSkipSystem::value().Get().skipCreatureTraining);
}

void IsKeepingOldCreature() // 462 IS_KEEPING_OLD_CREATURE
{
	Pushb(Locator::tutorialSkipSystem::value().Get().keepOldCreature);
}

void CurrentProfileHasCreature() // 463 CURRENT_PROFILE_HAS_CREATURE
{
	Pushb(Locator::playerProfileSystem::value().CurrentProfileHasCreature());
}

void CHLApi::InitFunctionsTable0()
{
	CREATE_FUNCTION_BINDING("NONE", 0, 0, None);
	CREATE_FUNCTION_BINDING("SET_CAMERA_POSITION", 3, 0, SetCameraPosition);
	CREATE_FUNCTION_BINDING("SET_CAMERA_FOCUS", 3, 0, SetCameraFocus);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_POSITION", 4, 0, MoveCameraPosition);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_FOCUS", 4, 0, MoveCameraFocus);
	CREATE_FUNCTION_BINDING("GET_CAMERA_POSITION", 0, 3, GetCameraPosition);
	CREATE_FUNCTION_BINDING("GET_CAMERA_FOCUS", 0, 3, GetCameraFocus);
	CREATE_FUNCTION_BINDING("SPIRIT_EJECT", 1, 0, SpiritEject);
	CREATE_FUNCTION_BINDING("SPIRIT_HOME", 1, 0, SpiritHome);
	CREATE_FUNCTION_BINDING("SPIRIT_POINT_POS", 5, 0, SpiritPointPos);
	CREATE_FUNCTION_BINDING("SPIRIT_POINT_GAME_THING", 3, 0, SpiritPointGameThing);
	CREATE_FUNCTION_BINDING("GAME_THING_FIELD_OF_VIEW", 1, 1, GameThingFieldOfView);
	CREATE_FUNCTION_BINDING("POS_FIELD_OF_VIEW", 3, 1, PosFieldOfView);
	CREATE_FUNCTION_BINDING("RUN_TEXT", 3, 0, RunText);
	CREATE_FUNCTION_BINDING("TEMP_TEXT", 3, 0, TempText);
	CREATE_FUNCTION_BINDING("TEXT_READ", 0, 1, TextRead);
	CREATE_FUNCTION_BINDING("GAME_THING_CLICKED", 1, 1, GameThingClicked);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_STATE", 2, 0, SetScriptState);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_STATE_POS", 4, 0, SetScriptStatePos);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_FLOAT", 2, 0, SetScriptFloat);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_ULONG", 3, 0, SetScriptUlong);
	CREATE_FUNCTION_BINDING("GET_PROPERTY", 2, 1, GetProperty);
	CREATE_FUNCTION_BINDING("SET_PROPERTY", 3, 0, SetProperty);
	CREATE_FUNCTION_BINDING("GET_POSITION", 1, 3, GetPosition);
	CREATE_FUNCTION_BINDING("SET_POSITION", 4, 0, SetPosition);
	CREATE_FUNCTION_BINDING("GET_DISTANCE", 6, 1, GetDistance);
	CREATE_FUNCTION_BINDING("CALL", 6, 1, Call);
	CREATE_FUNCTION_BINDING("CREATE", 5, 1, Create);
	CREATE_FUNCTION_BINDING("RANDOM", 2, 1, Random);
	CREATE_FUNCTION_BINDING("DLL_GETTIME", 0, 1, DllGettime);
	CREATE_FUNCTION_BINDING("START_CAMERA_CONTROL", 0, 1, StartCameraControl);
	CREATE_FUNCTION_BINDING("END_CAMERA_CONTROL", 0, 0, EndCameraControl);
	CREATE_FUNCTION_BINDING("SET_WIDESCREEN", 1, 0, SetWidescreen);
	CREATE_FUNCTION_BINDING("MOVE_GAME_THING", 5, 0, MoveGameThing);
	CREATE_FUNCTION_BINDING("SET_FOCUS", 4, 0, SetFocus);
	CREATE_FUNCTION_BINDING("HAS_CAMERA_ARRIVED", 0, 1, HasCameraArrived);
	CREATE_FUNCTION_BINDING("FLOCK_CREATE", 3, 1, FlockCreate);
	CREATE_FUNCTION_BINDING("FLOCK_ATTACH", 3, 1, FlockAttach);
	CREATE_FUNCTION_BINDING("FLOCK_DETACH", 2, 1, FlockDetach);
	CREATE_FUNCTION_BINDING("FLOCK_DISBAND", 1, 0, FlockDisband);
	CREATE_FUNCTION_BINDING("ID_SIZE", 1, 1, IdSize);
	CREATE_FUNCTION_BINDING("FLOCK_MEMBER", 2, 1, FlockMember);
	CREATE_FUNCTION_BINDING("GET_HAND_POSITION", 0, 3, GetHandPosition);
	CREATE_FUNCTION_BINDING("PLAY_SOUND_EFFECT", 6, 0, PlaySoundEffect);
	CREATE_FUNCTION_BINDING("START_MUSIC", 1, 0, StartMusic);
	CREATE_FUNCTION_BINDING("STOP_MUSIC", 0, 0, StopMusic);
	CREATE_FUNCTION_BINDING("ATTACH_MUSIC", 2, 0, AttachMusic);
	CREATE_FUNCTION_BINDING("DETACH_MUSIC", 1, 0, DetachMusic);
	CREATE_FUNCTION_BINDING("OBJECT_DELETE", 2, 0, ObjectDelete);
	CREATE_FUNCTION_BINDING("FOCUS_FOLLOW", 1, 0, FocusFollow);
	CREATE_FUNCTION_BINDING("POSITION_FOLLOW", 1, 0, PositionFollow);
	CREATE_FUNCTION_BINDING("CALL_NEAR", 7, 1, CallNear);
	CREATE_FUNCTION_BINDING("SPECIAL_EFFECT_POSITION", 5, 1, SpecialEffectPosition);
	CREATE_FUNCTION_BINDING("SPECIAL_EFFECT_OBJECT", 3, 1, SpecialEffectObject);
	CREATE_FUNCTION_BINDING("DANCE_CREATE", 6, 1, DanceCreate);
	CREATE_FUNCTION_BINDING("CALL_IN", 4, 1, CallIn);
	CREATE_FUNCTION_BINDING("CHANGE_INNER_OUTER_PROPERTIES", 4, 0, ChangeInnerOuterProperties);
	CREATE_FUNCTION_BINDING("SNAPSHOT", -1, 0, Snapshot);
	CREATE_FUNCTION_BINDING("GET_ALIGNMENT", 1, 1, GetAlignment);
	CREATE_FUNCTION_BINDING("SET_ALIGNMENT", 2, 0, SetAlignment);
	CREATE_FUNCTION_BINDING("INFLUENCE_OBJECT", 4, 1, InfluenceObject);
	CREATE_FUNCTION_BINDING("INFLUENCE_POSITION", 6, 1, InfluencePosition);
	CREATE_FUNCTION_BINDING("GET_INFLUENCE", 5, 1, GetInfluence);
	CREATE_FUNCTION_BINDING("SET_INTERFACE_INTERACTION", 1, 0, SetInterfaceInteraction);
	CREATE_FUNCTION_BINDING("PLAYED", 1, 1, Played);
	CREATE_FUNCTION_BINDING("RANDOM_ULONG", 2, 1, RandomUlong);
	CREATE_FUNCTION_BINDING("SET_GAMESPEED", 1, 0, SetGamespeed);
	CREATE_FUNCTION_BINDING("CALL_IN_NEAR", 8, 1, CallInNear);
	CREATE_FUNCTION_BINDING("OVERRIDE_STATE_ANIMATION", 2, 0, OverrideStateAnimation);
	CREATE_FUNCTION_BINDING("CREATURE_CREATE_RELATIVE_TO_CREATURE", 6, 1, CreatureCreateRelativeToCreature);
	CREATE_FUNCTION_BINDING("CREATURE_LEARN_EVERYTHING", 1, 0, CreatureLearnEverything);
	CREATE_FUNCTION_BINDING("CREATURE_SET_KNOWS_ACTION", 4, 0, CreatureSetKnowsAction);
	CREATE_FUNCTION_BINDING("CREATURE_SET_AGENDA_PRIORITY", 2, 0, CreatureSetAgendaPriority);
	CREATE_FUNCTION_BINDING("CREATURE_TURN_OFF_ALL_DESIRES", 1, 0, CreatureTurnOffAllDesires);
	CREATE_FUNCTION_BINDING("CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT", 4, 0,
	                        CreatureLearnDistinctionAboutActivityObject);
	CREATE_FUNCTION_BINDING("CREATURE_DO_ACTION", 4, 0, CreatureDoAction);
	CREATE_FUNCTION_BINDING("IN_CREATURE_HAND", 2, 1, InCreatureHand);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_VALUE", 3, 0, CreatureSetDesireValue);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_ACTIVATED", 3, 0, CreatureSetDesireActivated78);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_ACTIVATED", 2, 0, CreatureSetDesireActivated79);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_MAXIMUM", 3, 0, CreatureSetDesireMaximum);
	CREATE_FUNCTION_BINDING("CONVERT_CAMERA_POSITION", 1, 3, ConvertCameraPosition);
	CREATE_FUNCTION_BINDING("CONVERT_CAMERA_FOCUS", 1, 3, ConvertCameraFocus);
	CREATE_FUNCTION_BINDING("CREATURE_SET_PLAYER", 1, 0, CreatureSetPlayer);
	CREATE_FUNCTION_BINDING("START_COUNTDOWN_TIMER", 1, 0, StartCountdownTimer);
	CREATE_FUNCTION_BINDING("CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION", 2, 0, CreatureInitialiseNumTimesPerformedAction);
	CREATE_FUNCTION_BINDING("CREATURE_GET_NUM_TIMES_ACTION_PERFORMED", 2, 1, CreatureGetNumTimesActionPerformed);
	CREATE_FUNCTION_BINDING("REMOVE_COUNTDOWN_TIMER", 0, 0, RemoveCountdownTimer);
	CREATE_FUNCTION_BINDING("GET_OBJECT_DROPPED", 1, 1, GetObjectDropped);
	CREATE_FUNCTION_BINDING("CLEAR_DROPPED_BY_OBJECT", 1, 0, ClearDroppedByObject);
	CREATE_FUNCTION_BINDING("CREATE_REACTION", 2, 0, CreateReaction);
	CREATE_FUNCTION_BINDING("REMOVE_REACTION", 1, 0, RemoveReaction);
	CREATE_FUNCTION_BINDING("GET_COUNTDOWN_TIMER", 0, 1, GetCountdownTimer);
	CREATE_FUNCTION_BINDING("START_DUAL_CAMERA", 2, 0, StartDualCamera);
	CREATE_FUNCTION_BINDING("UPDATE_DUAL_CAMERA", 2, 0, UpdateDualCamera);
	CREATE_FUNCTION_BINDING("RELEASE_DUAL_CAMERA", 0, 0, ReleaseDualCamera);
	CREATE_FUNCTION_BINDING("SET_CREATURE_HELP", 1, 0, SetCreatureHelp);
	CREATE_FUNCTION_BINDING("GET_TARGET_OBJECT", 1, 1, GetTargetObject);
	CREATE_FUNCTION_BINDING("CREATURE_DESIRE_IS", 2, 1, CreatureDesireIs);
	CREATE_FUNCTION_BINDING("COUNTDOWN_TIMER_EXISTS", 0, 1, CountdownTimerExists);
	CREATE_FUNCTION_BINDING("LOOK_GAME_THING", 2, 0, LookGameThing);
	CREATE_FUNCTION_BINDING("GET_OBJECT_DESTINATION", 1, 3, GetObjectDestination);
	CREATE_FUNCTION_BINDING("CREATURE_FORCE_FINISH", 1, 0, CreatureForceFinish);
	CREATE_FUNCTION_BINDING("HIDE_COUNTDOWN_TIMER", 0, 0, HideCountdownTimer);
	CREATE_FUNCTION_BINDING("GET_ACTION_TEXT_FOR_OBJECT", 1, 1, GetActionTextForObject);
	CREATE_FUNCTION_BINDING("CREATE_DUAL_CAMERA_WITH_POINT", 4, 0, CreateDualCameraWithPoint);
	CREATE_FUNCTION_BINDING("SET_CAMERA_TO_FACE_OBJECT", 2, 0, SetCameraToFaceObject);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_TO_FACE_OBJECT", 3, 0, MoveCameraToFaceObject);
}

void CHLApi::InitFunctionsTable1()
{
	CREATE_FUNCTION_BINDING("GET_MOON_PERCENTAGE", 0, 1, GetMoonPercentage);
	CREATE_FUNCTION_BINDING("POPULATE_CONTAINER", 4, 0, PopulateContainer);
	CREATE_FUNCTION_BINDING("ADD_REFERENCE", 1, 1, AddReference);
	CREATE_FUNCTION_BINDING("REMOVE_REFERENCE", 1, 1, RemoveReference);
	CREATE_FUNCTION_BINDING("SET_GAME_TIME", 1, 0, SetGameTime);
	CREATE_FUNCTION_BINDING("GET_GAME_TIME", 0, 1, GetGameTime);
	CREATE_FUNCTION_BINDING("GET_REAL_TIME", 0, 1, GetRealTime);
	CREATE_FUNCTION_BINDING("GET_REAL_DAY", 0, 1, GetRealDay115);
	CREATE_FUNCTION_BINDING("GET_REAL_DAY", 0, 1, GetRealDay116);
	CREATE_FUNCTION_BINDING("GET_REAL_MONTH", 0, 1, GetRealMonth);
	CREATE_FUNCTION_BINDING("GET_REAL_YEAR", 0, 1, GetRealYear);
	CREATE_FUNCTION_BINDING("RUN_CAMERA_PATH", 1, 0, RunCameraPath);
	CREATE_FUNCTION_BINDING("START_DIALOGUE", 0, 1, StartDialogue);
	CREATE_FUNCTION_BINDING("END_DIALOGUE", 0, 0, EndDialogue);
	CREATE_FUNCTION_BINDING("IS_DIALOGUE_READY", 0, 1, IsDialogueReady);
	CREATE_FUNCTION_BINDING("CHANGE_WEATHER_PROPERTIES", 6, 0, ChangeWeatherProperties);
	CREATE_FUNCTION_BINDING("CHANGE_LIGHTNING_PROPERTIES", 5, 0, ChangeLightningProperties);
	CREATE_FUNCTION_BINDING("CHANGE_TIME_FADE_PROPERTIES", 3, 0, ChangeTimeFadeProperties);
	CREATE_FUNCTION_BINDING("CHANGE_CLOUD_PROPERTIES", 4, 0, ChangeCloudProperties);
	CREATE_FUNCTION_BINDING("SET_HEADING_AND_SPEED", 5, 0, SetHeadingAndSpeed);
	CREATE_FUNCTION_BINDING("START_GAME_SPEED", 0, 0, StartGameSpeed);
	CREATE_FUNCTION_BINDING("END_GAME_SPEED", 0, 0, EndGameSpeed);
	CREATE_FUNCTION_BINDING("BUILD_BUILDING", 4, 0, BuildBuilding);
	CREATE_FUNCTION_BINDING("SET_AFFECTED_BY_WIND", 2, 0, SetAffectedByWind);
	CREATE_FUNCTION_BINDING("WIDESCREEN_TRANSISTION_FINISHED", 0, 1, WidescreenTransistionFinished);
	CREATE_FUNCTION_BINDING("GET_RESOURCE", 2, 1, GetResource);
	CREATE_FUNCTION_BINDING("ADD_RESOURCE", 3, 1, AddResource);
	CREATE_FUNCTION_BINDING("REMOVE_RESOURCE", 3, 1, RemoveResource);
	CREATE_FUNCTION_BINDING("GET_TARGET_RELATIVE_POS", 8, 3, GetTargetRelativePos);
	CREATE_FUNCTION_BINDING("STOP_POINTING", 1, 0, StopPointing);
	CREATE_FUNCTION_BINDING("STOP_LOOKING", 1, 0, StopLooking);
	CREATE_FUNCTION_BINDING("LOOK_AT_POSITION", 4, 0, LookAtPosition);
	CREATE_FUNCTION_BINDING("PLAY_SPIRIT_ANIM", 5, 0, PlaySpiritAnim);
	CREATE_FUNCTION_BINDING("CALL_IN_NOT_NEAR", 8, 1, CallInNotNear);
	CREATE_FUNCTION_BINDING("SET_CAMERA_ZONE", 1, 0, SetCameraZone);
	CREATE_FUNCTION_BINDING("GET_OBJECT_STATE", 1, 1, GetObjectState);
	CREATE_FUNCTION_BINDING("REVEAL_COUNTDOWN_TIMER", 0, 0, RevealCountdownTimer);
	CREATE_FUNCTION_BINDING("SET_TIMER_TIME", 2, 0, SetTimerTime);
	CREATE_FUNCTION_BINDING("CREATE_TIMER", 1, 1, CreateTimer);
	CREATE_FUNCTION_BINDING("GET_TIMER_TIME_REMAINING", 1, 1, GetTimerTimeRemaining);
	CREATE_FUNCTION_BINDING("GET_TIMER_TIME_SINCE_SET", 1, 1, GetTimerTimeSinceSet);
	CREATE_FUNCTION_BINDING("MOVE_MUSIC", 2, 0, MoveMusic);
	CREATE_FUNCTION_BINDING("GET_INCLUSION_DISTANCE", 0, 1, GetInclusionDistance);
	CREATE_FUNCTION_BINDING("GET_LAND_HEIGHT", 3, 1, GetLandHeight);
	CREATE_FUNCTION_BINDING("LOAD_MAP", 1, 0, LoadMap);
	CREATE_FUNCTION_BINDING("STOP_ALL_SCRIPTS_EXCLUDING", 1, 0, StopAllScriptsExcluding);
	CREATE_FUNCTION_BINDING("STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING", 1, 0, StopAllScriptsInFilesExcluding);
	CREATE_FUNCTION_BINDING("STOP_SCRIPT", 1, 0, StopScript);
	CREATE_FUNCTION_BINDING("CLEAR_CLICKED_OBJECT", 0, 0, ClearClickedObject);
	CREATE_FUNCTION_BINDING("CLEAR_CLICKED_POSITION", 0, 0, ClearClickedPosition);
	CREATE_FUNCTION_BINDING("POSITION_CLICKED", 4, 1, PositionClicked);
	CREATE_FUNCTION_BINDING("RELEASE_FROM_SCRIPT", 1, 0, ReleaseFromScript);
	CREATE_FUNCTION_BINDING("GET_OBJECT_HAND_IS_OVER", 0, 1, GetObjectHandIsOver);
	CREATE_FUNCTION_BINDING("ID_POISONED_SIZE", 1, 1, IdPoisonedSize);
	CREATE_FUNCTION_BINDING("IS_POISONED", 1, 1, IsPoisoned);
	CREATE_FUNCTION_BINDING("CALL_POISONED_IN", 4, 1, CallPoisonedIn);
	CREATE_FUNCTION_BINDING("CALL_NOT_POISONED_IN", 4, 1, CallNotPoisonedIn);
	CREATE_FUNCTION_BINDING("SPIRIT_PLAYED", 1, 1, SpiritPlayed);
	CREATE_FUNCTION_BINDING("CLING_SPIRIT", 3, 0, ClingSpirit);
	CREATE_FUNCTION_BINDING("FLY_SPIRIT", 3, 0, FlySpirit);
	CREATE_FUNCTION_BINDING("SET_ID_MOVEABLE", 2, 0, SetIdMoveable);
	CREATE_FUNCTION_BINDING("SET_ID_PICKUPABLE", 2, 0, SetIdPickupable);
	CREATE_FUNCTION_BINDING("IS_ON_FIRE", 1, 1, IsOnFire);
	CREATE_FUNCTION_BINDING("IS_FIRE_NEAR", 4, 1, IsFireNear);
	CREATE_FUNCTION_BINDING("STOP_SCRIPTS_IN_FILES", 1, 0, StopScriptsInFiles);
	CREATE_FUNCTION_BINDING("SET_POISONED", 2, 0, SetPoisoned);
	CREATE_FUNCTION_BINDING("SET_TEMPERATURE", 2, 0, SetTemperature);
	CREATE_FUNCTION_BINDING("SET_ON_FIRE", 3, 0, SetOnFire);
	CREATE_FUNCTION_BINDING("SET_TARGET", 5, 0, SetTarget);
	CREATE_FUNCTION_BINDING("WALK_PATH", 5, 0, WalkPath);
	CREATE_FUNCTION_BINDING("FOCUS_AND_POSITION_FOLLOW", 2, 0, FocusAndPositionFollow);
	CREATE_FUNCTION_BINDING("GET_WALK_PATH_PERCENTAGE", 1, 1, GetWalkPathPercentage);
	CREATE_FUNCTION_BINDING("CAMERA_PROPERTIES", 4, 0, CameraProperties);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_MUSIC", 2, 0, EnableDisableMusic);
	CREATE_FUNCTION_BINDING("GET_MUSIC_OBJ_DISTANCE", 1, 1, GetMusicObjDistance);
	CREATE_FUNCTION_BINDING("GET_MUSIC_ENUM_DISTANCE", 1, 1, GetMusicEnumDistance);
	CREATE_FUNCTION_BINDING("SET_MUSIC_PLAY_POSITION", 4, 0, SetMusicPlayPosition);
	CREATE_FUNCTION_BINDING("ATTACH_OBJECT_LEASH_TO_OBJECT", 2, 0, AttachObjectLeashToObject);
	CREATE_FUNCTION_BINDING("ATTACH_OBJECT_LEASH_TO_HAND", 1, 0, AttachObjectLeashToHand);
	CREATE_FUNCTION_BINDING("DETACH_OBJECT_LEASH", 1, 0, DetachObjectLeash);
	CREATE_FUNCTION_BINDING("SET_CREATURE_ONLY_DESIRE", 3, 0, SetCreatureOnlyDesire);
	CREATE_FUNCTION_BINDING("SET_CREATURE_ONLY_DESIRE_OFF", 1, 0, SetCreatureOnlyDesireOff);
	CREATE_FUNCTION_BINDING("RESTART_MUSIC", 1, 0, RestartMusic);
	CREATE_FUNCTION_BINDING("MUSIC_PLAYED", 1, 1, MusicPlayed191);
	CREATE_FUNCTION_BINDING("IS_OF_TYPE", 3, 1, IsOfType);
	CREATE_FUNCTION_BINDING("CLEAR_HIT_OBJECT", 0, 0, ClearHitObject);
	CREATE_FUNCTION_BINDING("GAME_THING_HIT", 1, 1, GameThingHit);
	CREATE_FUNCTION_BINDING("SPELL_AT_THING", 8, 1, SpellAtThing);
	CREATE_FUNCTION_BINDING("SPELL_AT_POS", 10, 1, SpellAtPos);
	CREATE_FUNCTION_BINDING("CALL_PLAYER_CREATURE", 1, 1, CallPlayerCreature);
	CREATE_FUNCTION_BINDING("GET_SLOWEST_SPEED", 1, 1, GetSlowestSpeed);
	CREATE_FUNCTION_BINDING("GET_OBJECT_HELD", 0, 1, GetObjectHeld199);
}

void CHLApi::InitFunctionsTable2()
{
	CREATE_FUNCTION_BINDING("HELP_SYSTEM_ON", 0, 1, HelpSystemOn);
	CREATE_FUNCTION_BINDING("SHAKE_CAMERA", 6, 0, ShakeCamera);
	CREATE_FUNCTION_BINDING("SET_ANIMATION_MODIFY", 2, 0, SetAnimationModify);
	CREATE_FUNCTION_BINDING("SET_AVI_SEQUENCE", 2, 0, SetAviSequence);
	CREATE_FUNCTION_BINDING("PLAY_GESTURE", 5, 0, PlayGesture);
	CREATE_FUNCTION_BINDING("DEV_FUNCTION", 1, 0, DevFunction);
	CREATE_FUNCTION_BINDING("HAS_MOUSE_WHEEL", 0, 1, HasMouseWheel);
	CREATE_FUNCTION_BINDING("NUM_MOUSE_BUTTONS", 0, 1, NumMouseButtons);
	CREATE_FUNCTION_BINDING("SET_CREATURE_DEV_STAGE", 2, 0, SetCreatureDevStage);
	CREATE_FUNCTION_BINDING("SET_FIXED_CAM_ROTATION", 4, 0, SetFixedCamRotation);
	CREATE_FUNCTION_BINDING("SWAP_CREATURE", 2, 0, SwapCreature);
	CREATE_FUNCTION_BINDING("GET_ARENA", 5, 1, GetArena);
	CREATE_FUNCTION_BINDING("GET_FOOTBALL_PITCH", 1, 1, GetFootballPitch);
	CREATE_FUNCTION_BINDING("STOP_ALL_GAMES", 1, 0, StopAllGames);
	CREATE_FUNCTION_BINDING("ATTACH_TO_GAME", 3, 0, AttachToGame);
	CREATE_FUNCTION_BINDING("DETACH_FROM_GAME", 3, 0, DetachFromGame);
	CREATE_FUNCTION_BINDING("DETACH_UNDEFINED_FROM_GAME", 2, 0, DetachUndefinedFromGame);
	CREATE_FUNCTION_BINDING("SET_ONLY_FOR_SCRIPTS", 2, 0, SetOnlyForScripts);
	CREATE_FUNCTION_BINDING("START_MATCH_WITH_REFEREE", 2, 0, StartMatchWithReferee);
	CREATE_FUNCTION_BINDING("GAME_TEAM_SIZE", 2, 0, GameTeamSize);
	CREATE_FUNCTION_BINDING("GAME_TYPE", 1, 1, GameType);
	CREATE_FUNCTION_BINDING("GAME_SUB_TYPE", 1, 1, GameSubType);
	CREATE_FUNCTION_BINDING("IS_LEASHED", 1, 1, IsLeashed);
	CREATE_FUNCTION_BINDING("SET_CREATURE_HOME", 4, 0, SetCreatureHome);
	CREATE_FUNCTION_BINDING("GET_HIT_OBJECT", 0, 1, GetHitObject);
	CREATE_FUNCTION_BINDING("GET_OBJECT_WHICH_HIT", 0, 1, GetObjectWhichHit);
	CREATE_FUNCTION_BINDING("GET_NEAREST_TOWN_OF_PLAYER", 5, 1, GetNearestTownOfPlayer);
	CREATE_FUNCTION_BINDING("SPELL_AT_POINT", 5, 1, SpellAtPoint);
	CREATE_FUNCTION_BINDING("SET_ATTACK_OWN_TOWN", 2, 0, SetAttackOwnTown);
	CREATE_FUNCTION_BINDING("IS_FIGHTING", 1, 1, IsFighting);
	CREATE_FUNCTION_BINDING("SET_MAGIC_RADIUS", 2, 0, SetMagicRadius);
	CREATE_FUNCTION_BINDING("TEMP_TEXT_WITH_NUMBER", 4, 0, TempTextWithNumber);
	CREATE_FUNCTION_BINDING("RUN_TEXT_WITH_NUMBER", 4, 0, RunTextWithNumber);
	CREATE_FUNCTION_BINDING("CREATURE_SPELL_REVERSION", 2, 0, CreatureSpellReversion);
	CREATE_FUNCTION_BINDING("GET_DESIRE", 2, 1, GetDesire);
	CREATE_FUNCTION_BINDING("GET_EVENTS_PER_SECOND", 1, 1, GetEventsPerSecond);
	CREATE_FUNCTION_BINDING("GET_TIME_SINCE", 1, 1, GetTimeSince);
	CREATE_FUNCTION_BINDING("GET_TOTAL_EVENTS", 1, 1, GetTotalEvents);
	CREATE_FUNCTION_BINDING("UPDATE_SNAPSHOT", -1, 0, UpdateSnapshot);
	CREATE_FUNCTION_BINDING("CREATE_REWARD", 5, 1, CreateReward);
	CREATE_FUNCTION_BINDING("CREATE_REWARD_IN_TOWN", 6, 1, CreateRewardInTown);
	CREATE_FUNCTION_BINDING("SET_FADE", 4, 0, SetFade);
	CREATE_FUNCTION_BINDING("SET_FADE_IN", 1, 0, SetFadeIn);
	CREATE_FUNCTION_BINDING("FADE_FINISHED", 0, 1, FadeFinished);
	CREATE_FUNCTION_BINDING("SET_PLAYER_MAGIC", 3, 0, SetPlayerMagic);
	CREATE_FUNCTION_BINDING("HAS_PLAYER_MAGIC", 2, 1, HasPlayerMagic);
	CREATE_FUNCTION_BINDING("SPIRIT_SPEAKS", 2, 1, SpiritSpeaks);
	CREATE_FUNCTION_BINDING("BELIEF_FOR_PLAYER", 2, 1, BeliefForPlayer);
	CREATE_FUNCTION_BINDING("GET_HELP", 1, 1, GetHelp);
	CREATE_FUNCTION_BINDING("SET_LEASH_WORKS", 2, 0, SetLeashWorks);
	CREATE_FUNCTION_BINDING("LOAD_MY_CREATURE", 3, 0, LoadMyCreature);
	CREATE_FUNCTION_BINDING("OBJECT_RELATIVE_BELIEF", 3, 0, ObjectRelativeBelief);
	CREATE_FUNCTION_BINDING("CREATE_WITH_ANGLE_AND_SCALE", 7, 1, CreateWithAngleAndScale);
	CREATE_FUNCTION_BINDING("SET_HELP_SYSTEM", 1, 0, SetHelpSystem);
	CREATE_FUNCTION_BINDING("SET_VIRTUAL_INFLUENCE", 2, 0, SetVirtualInfluence);
	CREATE_FUNCTION_BINDING("SET_ACTIVE", 2, 0, SetActive);
	CREATE_FUNCTION_BINDING("THING_VALID", 1, 1, ThingValid);
	CREATE_FUNCTION_BINDING("VORTEX_FADE_OUT", 1, 0, VortexFadeOut);
	CREATE_FUNCTION_BINDING("REMOVE_REACTION_OF_TYPE", 2, 0, RemoveReactionOfType);
	CREATE_FUNCTION_BINDING("CREATURE_LEARN_EVERYTHING_EXCLUDING", 2, 0, CreatureLearnEverythingExcluding);
	CREATE_FUNCTION_BINDING("PLAYED_PERCENTAGE", 1, 1, PlayedPercentage);
	CREATE_FUNCTION_BINDING("OBJECT_CAST_BY_OBJECT", 2, 1, ObjectCastByObject);
	CREATE_FUNCTION_BINDING("IS_WIND_MAGIC_AT_POS", 1, 1, IsWindMagicAtPos);
	CREATE_FUNCTION_BINDING("CREATE_MIST", 9, 1, CreateMist);
	CREATE_FUNCTION_BINDING("SET_MIST_FADE", 6, 0, SetMistFade);
	CREATE_FUNCTION_BINDING("GET_OBJECT_FADE", 1, 1, GetObjectFade);
	CREATE_FUNCTION_BINDING("PLAY_HAND_DEMO", 3, 0, PlayHandDemo);
	CREATE_FUNCTION_BINDING("IS_PLAYING_HAND_DEMO", 0, 1, IsPlayingHandDemo);
	CREATE_FUNCTION_BINDING("GET_ARSE_POSITION", 1, 3, GetArsePosition);
	CREATE_FUNCTION_BINDING("IS_LEASHED_TO_OBJECT", 2, 1, IsLeashedToObject);
	CREATE_FUNCTION_BINDING("GET_INTERACTION_MAGNITUDE", 1, 1, GetInteractionMagnitude);
	CREATE_FUNCTION_BINDING("IS_CREATURE_AVAILABLE", 1, 1, IsCreatureAvailable);
	CREATE_FUNCTION_BINDING("CREATE_HIGHLIGHT", 5, 1, CreateHighlight);
	CREATE_FUNCTION_BINDING("GET_OBJECT_HELD", 1, 1, GetObjectHeld273);
	CREATE_FUNCTION_BINDING("GET_ACTION_COUNT", 2, 1, GetActionCount);
	CREATE_FUNCTION_BINDING("GET_OBJECT_LEASH_TYPE", 1, 1, GetObjectLeashType);
	CREATE_FUNCTION_BINDING("SET_FOCUS_FOLLOW", 1, 0, SetFocusFollow);
	CREATE_FUNCTION_BINDING("SET_POSITION_FOLLOW", 1, 0, SetPositionFollow);
	CREATE_FUNCTION_BINDING("SET_FOCUS_AND_POSITION_FOLLOW", 2, 0, SetFocusAndPositionFollow);
	CREATE_FUNCTION_BINDING("SET_CAMERA_LENS", 1, 0, SetCameraLens);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_LENS", 2, 0, MoveCameraLens);
	CREATE_FUNCTION_BINDING("CREATURE_REACTION", 2, 0, CreatureReaction);
	CREATE_FUNCTION_BINDING("CREATURE_IN_DEV_SCRIPT", 2, 0, CreatureInDevScript);
	CREATE_FUNCTION_BINDING("STORE_CAMERA_DETAILS", 0, 0, StoreCameraDetails);
	CREATE_FUNCTION_BINDING("RESTORE_CAMERA_DETAILS", 0, 0, RestoreCameraDetails);
	CREATE_FUNCTION_BINDING("START_ANGLE_SOUND", 1, 0, StartAngleSound285);
	CREATE_FUNCTION_BINDING("SET_CAMERA_POS_FOC_LENS", 7, 0, SetCameraPosFocLens);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_POS_FOC_LENS", 8, 0, MoveCameraPosFocLens);
	CREATE_FUNCTION_BINDING("GAME_TIME_ON_OFF", 1, 0, GameTimeOnOff);
	CREATE_FUNCTION_BINDING("MOVE_GAME_TIME", 2, 0, MoveGameTime);
	CREATE_FUNCTION_BINDING("SET_HIGH_GRAPHICS_DETAIL", 2, 0, SetHighGraphicsDetail);
	CREATE_FUNCTION_BINDING("SET_SKELETON", 2, 0, SetSkeleton);
	CREATE_FUNCTION_BINDING("IS_SKELETON", 1, 1, IsSkeleton);
	CREATE_FUNCTION_BINDING("PLAYER_SPELL_CAST_TIME", 1, 1, PlayerSpellCastTime);
	CREATE_FUNCTION_BINDING("PLAYER_SPELL_LAST_CAST", 1, 1, PlayerSpellLastCast);
	CREATE_FUNCTION_BINDING("GET_LAST_SPELL_CAST_POS", 1, 3, GetLastSpellCastPos);
	CREATE_FUNCTION_BINDING("ADD_SPOT_VISUAL_TARGET_POS", 4, 0, AddSpotVisualTargetPos);
	CREATE_FUNCTION_BINDING("ADD_SPOT_VISUAL_TARGET_OBJECT", 2, 0, AddSpotVisualTargetObject);
	CREATE_FUNCTION_BINDING("SET_INDESTRUCTABLE", 2, 0, SetIndestructable);
	CREATE_FUNCTION_BINDING("SET_GRAPHICS_CLIPPING", 2, 0, SetGraphicsClipping);
}

void CHLApi::InitFunctionsTable3()
{
	CREATE_FUNCTION_BINDING("SPIRIT_APPEAR", 1, 0, SpiritAppear);
	CREATE_FUNCTION_BINDING("SPIRIT_DISAPPEAR", 1, 0, SpiritDisappear);
	CREATE_FUNCTION_BINDING("SET_FOCUS_ON_OBJECT", 2, 0, SetFocusOnObject);
	CREATE_FUNCTION_BINDING("RELEASE_OBJECT_FOCUS", 1, 0, ReleaseObjectFocus);
	CREATE_FUNCTION_BINDING("IMMERSION_EXISTS", 0, 1, ImmersionExists);
	CREATE_FUNCTION_BINDING("SET_DRAW_LEASH", 1, 0, SetDrawLeash);
	CREATE_FUNCTION_BINDING("SET_DRAW_HIGHLIGHT", 1, 0, SetDrawHighlight);
	CREATE_FUNCTION_BINDING("SET_OPEN_CLOSE", 2, 0, SetOpenClose);
	CREATE_FUNCTION_BINDING("SET_INTRO_BUILDING", 1, 0, SetIntroBuilding);
	CREATE_FUNCTION_BINDING("CREATURE_FORCE_FRIENDS", 3, 0, CreatureForceFriends);
	CREATE_FUNCTION_BINDING("MOVE_COMPUTER_PLAYER_POSITION", 6, 0, MoveComputerPlayerPosition);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_COMPUTER_PLAYER", 2, 0, EnableDisableComputerPlayer311);
	CREATE_FUNCTION_BINDING("GET_COMPUTER_PLAYER_POSITION", 1, 3, GetComputerPlayerPosition);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_POSITION", 5, 0, SetComputerPlayerPosition);
	CREATE_FUNCTION_BINDING("GET_STORED_CAMERA_POSITION", 0, 3, GetStoredCameraPosition);
	CREATE_FUNCTION_BINDING("GET_STORED_CAMERA_FOCUS", 0, 3, GetStoredCameraFocus);
	CREATE_FUNCTION_BINDING("CALL_NEAR_IN_STATE", 8, 1, CallNearInState);
	CREATE_FUNCTION_BINDING("SET_CREATURE_SOUND", 1, 0, SetCreatureSound);
	CREATE_FUNCTION_BINDING("CREATURE_INTERACTING_WITH", 2, 1, CreatureInteractingWith);
	CREATE_FUNCTION_BINDING("SET_SUN_DRAW", 1, 0, SetSunDraw);
	CREATE_FUNCTION_BINDING("OBJECT_INFO_BITS", 1, 1, ObjectInfoBits);
	CREATE_FUNCTION_BINDING("SET_HURT_BY_FIRE", 2, 0, SetHurtByFire);
	CREATE_FUNCTION_BINDING("CONFINED_OBJECT", 5, 0, ConfinedObject);
	CREATE_FUNCTION_BINDING("CLEAR_CONFINED_OBJECT", 1, 0, ClearConfinedObject);
	CREATE_FUNCTION_BINDING("GET_OBJECT_FLOCK", 1, 1, GetObjectFlock);
	CREATE_FUNCTION_BINDING("SET_PLAYER_BELIEF", 3, 0, SetPlayerBelief);
	CREATE_FUNCTION_BINDING("PLAY_JC_SPECIAL", 1, 0, PlayJcSpecial);
	CREATE_FUNCTION_BINDING("IS_PLAYING_JC_SPECIAL", 1, 1, IsPlayingJcSpecial);
	CREATE_FUNCTION_BINDING("VORTEX_PARAMETERS", 8, 0, VortexParameters);
	CREATE_FUNCTION_BINDING("LOAD_CREATURE", 6, 0, LoadCreature);
	CREATE_FUNCTION_BINDING("IS_SPELL_CHARGING", 1, 1, IsSpellCharging);
	CREATE_FUNCTION_BINDING("IS_THAT_SPELL_CHARGING", 2, 1, IsThatSpellCharging);
	CREATE_FUNCTION_BINDING("OPPOSING_CREATURE", 1, 1, OpposingCreature);
	CREATE_FUNCTION_BINDING("FLOCK_WITHIN_LIMITS", 1, 1, FlockWithinLimits);
	CREATE_FUNCTION_BINDING("HIGHLIGHT_PROPERTIES", 3, 0, HighlightProperties);
	CREATE_FUNCTION_BINDING("LAST_MUSIC_LINE", 1, 1, LastMusicLine);
	CREATE_FUNCTION_BINDING("HAND_DEMO_TRIGGER", 0, 1, HandDemoTrigger);
	CREATE_FUNCTION_BINDING("GET_BELLY_POSITION", 1, 3, GetBellyPosition);
	CREATE_FUNCTION_BINDING("SET_CREATURE_CREED_PROPERTIES", 5, 0, SetCreatureCreedProperties);
	CREATE_FUNCTION_BINDING("GAME_THING_CAN_VIEW_CAMERA", 2, 1, GameThingCanViewCamera);
	CREATE_FUNCTION_BINDING("GAME_PLAY_SAY_SOUND_EFFECT", 6, 0, GamePlaySaySoundEffect);
	CREATE_FUNCTION_BINDING("SET_TOWN_DESIRE_BOOST", 3, 0, SetTownDesireBoost);
	CREATE_FUNCTION_BINDING("IS_LOCKED_INTERACTION", 1, 1, IsLockedInteraction);
	CREATE_FUNCTION_BINDING("SET_CREATURE_NAME", 2, 0, SetCreatureName);
	CREATE_FUNCTION_BINDING("COMPUTER_PLAYER_READY", 1, 1, ComputerPlayerReady);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_COMPUTER_PLAYER", 2, 0, EnableDisableComputerPlayer345);
	CREATE_FUNCTION_BINDING("CLEAR_ACTOR_MIND", 1, 0, ClearActorMind);
	CREATE_FUNCTION_BINDING("ENTER_EXIT_CITADEL", 1, 0, EnterExitCitadel);
	CREATE_FUNCTION_BINDING("START_ANGLE_SOUND", 1, 0, StartAngleSound348);
	CREATE_FUNCTION_BINDING("THING_JC_SPECIAL", 3, 0, ThingJcSpecial);
	CREATE_FUNCTION_BINDING("MUSIC_PLAYED", 1, 1, MusicPlayed350);
	CREATE_FUNCTION_BINDING("UPDATE_SNAPSHOT_PICTURE", 11, 0, UpdateSnapshotPicture);
	CREATE_FUNCTION_BINDING("STOP_SCRIPTS_IN_FILES_EXCLUDING", 2, 0, StopScriptsInFilesExcluding);
	CREATE_FUNCTION_BINDING("CREATE_RANDOM_VILLAGER_OF_TRIBE", 4, 1, CreateRandomVillagerOfTribe);
	CREATE_FUNCTION_BINDING("TOGGLE_LEASH", 1, 0, ToggleLeash);
	CREATE_FUNCTION_BINDING("GAME_SET_MANA", 2, 0, GameSetMana);
	CREATE_FUNCTION_BINDING("SET_MAGIC_PROPERTIES", 3, 0, SetMagicProperties);
	CREATE_FUNCTION_BINDING("SET_GAME_SOUND", 1, 0, SetGameSound);
	CREATE_FUNCTION_BINDING("SEX_IS_MALE", 1, 1, SexIsMale);
	CREATE_FUNCTION_BINDING("GET_FIRST_HELP", 1, 1, GetFirstHelp);
	CREATE_FUNCTION_BINDING("GET_LAST_HELP", 1, 1, GetLastHelp);
	CREATE_FUNCTION_BINDING("IS_ACTIVE", 1, 1, IsActive);
	CREATE_FUNCTION_BINDING("SET_BOOKMARK_POSITION", 4, 0, SetBookmarkPosition);
	CREATE_FUNCTION_BINDING("SET_SCAFFOLD_PROPERTIES", 4, 0, SetScaffoldProperties);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_PERSONALITY", 3, 0, SetComputerPlayerPersonality);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_SUPPRESSION", 3, 0, SetComputerPlayerSuppression);
	CREATE_FUNCTION_BINDING("FORCE_COMPUTER_PLAYER_ACTION", 4, 0, ForceComputerPlayerAction);
	CREATE_FUNCTION_BINDING("QUEUE_COMPUTER_PLAYER_ACTION", 4, 0, QueueComputerPlayerAction);
	CREATE_FUNCTION_BINDING("GET_TOWN_WITH_ID", 1, 1, GetTownWithId);
	CREATE_FUNCTION_BINDING("SET_DISCIPLE", 3, 0, SetDisciple);
	CREATE_FUNCTION_BINDING("RELEASE_COMPUTER_PLAYER", 1, 0, ReleaseComputerPlayer);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_SPEED", 2, 0, SetComputerPlayerSpeed);
	CREATE_FUNCTION_BINDING("SET_FOCUS_FOLLOW_COMPUTER_PLAYER", 1, 0, SetFocusFollowComputerPlayer);
	CREATE_FUNCTION_BINDING("SET_POSITION_FOLLOW_COMPUTER_PLAYER", 1, 0, SetPositionFollowComputerPlayer);
	CREATE_FUNCTION_BINDING("CALL_COMPUTER_PLAYER", 1, 1, CallComputerPlayer);
	CREATE_FUNCTION_BINDING("CALL_BUILDING_IN_TOWN", 4, 1, CallBuildingInTown);
	CREATE_FUNCTION_BINDING("SET_CAN_BUILD_WORSHIPSITE", 2, 0, SetCanBuildWorshipsite);
	CREATE_FUNCTION_BINDING("GET_FACING_CAMERA_POSITION", 1, 3, GetFacingCameraPosition);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_ATTITUDE", 3, 0, SetComputerPlayerAttitude);
	CREATE_FUNCTION_BINDING("GET_COMPUTER_PLAYER_ATTITUDE", 2, 1, GetComputerPlayerAttitude);
	CREATE_FUNCTION_BINDING("LOAD_COMPUTER_PLAYER_PERSONALITY", 2, 0, LoadComputerPlayerPersonality);
	CREATE_FUNCTION_BINDING("SAVE_COMPUTER_PLAYER_PERSONALITY", 2, 0, SaveComputerPlayerPersonality);
	CREATE_FUNCTION_BINDING("SET_PLAYER_ALLY", 3, 0, SetPlayerAlly);
	CREATE_FUNCTION_BINDING("CALL_FLYING", 7, 1, CallFlying);
	CREATE_FUNCTION_BINDING("SET_OBJECT_FADE_IN", 2, 0, SetObjectFadeIn);
	CREATE_FUNCTION_BINDING("IS_AFFECTED_BY_SPELL", 2, 1, IsAffectedBySpell);
	CREATE_FUNCTION_BINDING("SET_MAGIC_IN_OBJECT", 3, 0, SetMagicInObject);
	CREATE_FUNCTION_BINDING("ID_ADULT_SIZE", 1, 1, IdAdultSize);
	CREATE_FUNCTION_BINDING("OBJECT_CAPACITY", 1, 1, ObjectCapacity);
	CREATE_FUNCTION_BINDING("OBJECT_ADULT_CAPACITY", 1, 1, ObjectAdultCapacity);
	CREATE_FUNCTION_BINDING("SET_CREATURE_AUTO_FIGHTING", 2, 0, SetCreatureAutoFighting);
	CREATE_FUNCTION_BINDING("IS_AUTO_FIGHTING", 1, 1, IsAutoFighting);
	CREATE_FUNCTION_BINDING("SET_CREATURE_QUEUE_FIGHT_MOVE", 2, 0, SetCreatureQueueFightMove);
	CREATE_FUNCTION_BINDING("SET_CREATURE_QUEUE_FIGHT_SPELL", 2, 0, SetCreatureQueueFightSpell);
	CREATE_FUNCTION_BINDING("SET_CREATURE_QUEUE_FIGHT_STEP", 2, 0, SetCreatureQueueFightStep);
	CREATE_FUNCTION_BINDING("GET_CREATURE_FIGHT_ACTION", 1, 1, GetCreatureFightAction);
	CREATE_FUNCTION_BINDING("CREATURE_FIGHT_QUEUE_HITS", 1, 1, CreatureFightQueueHits);
	CREATE_FUNCTION_BINDING("SQUARE_ROOT", 1, 1, SquareRoot);
	CREATE_FUNCTION_BINDING("GET_PLAYER_ALLY", 2, 1, GetPlayerAlly);
	CREATE_FUNCTION_BINDING("SET_PLAYER_WIND_RESISTANCE", 2, 1, SetPlayerWindResistance);
}

void CHLApi::InitFunctionsTable4()
{
	CREATE_FUNCTION_BINDING("GET_PLAYER_WIND_RESISTANCE", 2, 1, GetPlayerWindResistance);
	CREATE_FUNCTION_BINDING("PAUSE_UNPAUSE_CLIMATE_SYSTEM", 1, 0, PauseUnpauseClimateSystem);
	CREATE_FUNCTION_BINDING("PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM", 1, 0, PauseUnpauseStormCreationInClimateSystem);
	CREATE_FUNCTION_BINDING("GET_MANA_FOR_SPELL", 1, 1, GetManaForSpell);
	CREATE_FUNCTION_BINDING("KILL_STORMS_IN_AREA", 4, 0, KillStormsInArea);
	CREATE_FUNCTION_BINDING("INSIDE_TEMPLE", 0, 1, InsideTemple);
	CREATE_FUNCTION_BINDING("RESTART_OBJECT", 1, 0, RestartObject);
	CREATE_FUNCTION_BINDING("SET_GAME_TIME_PROPERTIES", 3, 0, SetGameTimeProperties);
	CREATE_FUNCTION_BINDING("RESET_GAME_TIME_PROPERTIES", 0, 0, ResetGameTimeProperties);
	CREATE_FUNCTION_BINDING("SOUND_EXISTS", 0, 1, SoundExists);
	CREATE_FUNCTION_BINDING("GET_TOWN_WORSHIP_DEATHS", 1, 1, GetTownWorshipDeaths);
	CREATE_FUNCTION_BINDING("GAME_CLEAR_DIALOGUE", 0, 0, GameClearDialogue);
	CREATE_FUNCTION_BINDING("GAME_CLOSE_DIALOGUE", 0, 0, GameCloseDialogue);
	CREATE_FUNCTION_BINDING("GET_HAND_STATE", 0, 1, GetHandState);
	CREATE_FUNCTION_BINDING("SET_INTERFACE_CITADEL", 1, 0, SetInterfaceCitadel);
	CREATE_FUNCTION_BINDING("MAP_SCRIPT_FUNCTION", 1, 0, MapScriptFunction);
	CREATE_FUNCTION_BINDING("WITHIN_ROTATION", 0, 1, WithinRotation);
	CREATE_FUNCTION_BINDING("GET_PLAYER_TOWN_TOTAL", 1, 1, GetPlayerTownTotal);
	CREATE_FUNCTION_BINDING("SPIRIT_SCREEN_POINT", 3, 0, SpiritScreenPoint);
	CREATE_FUNCTION_BINDING("KEY_DOWN", 1, 1, KeyDown);
	CREATE_FUNCTION_BINDING("SET_FIGHT_EXIT", 1, 0, SetFightExit);
	CREATE_FUNCTION_BINDING("GET_OBJECT_CLICKED", 0, 1, GetObjectClicked);
	CREATE_FUNCTION_BINDING("GET_MANA", 1, 1, GetMana);
	CREATE_FUNCTION_BINDING("CLEAR_PLAYER_SPELL_CHARGING", 1, 0, ClearPlayerSpellCharging);
	CREATE_FUNCTION_BINDING("STOP_SOUND_EFFECT", 3, 0, StopSoundEffect);
	CREATE_FUNCTION_BINDING("GET_TOTEM_STATUE", 1, 1, GetTotemStatue);
	CREATE_FUNCTION_BINDING("SET_SET_ON_FIRE", 2, 0, SetSetOnFire);
	CREATE_FUNCTION_BINDING("SET_LAND_BALANCE", 2, 0, SetLandBalance);
	CREATE_FUNCTION_BINDING("SET_OBJECT_BELIEF_SCALE", 2, 0, SetObjectBeliefScale);
	CREATE_FUNCTION_BINDING("START_IMMERSION", 1, 0, StartImmersion);
	CREATE_FUNCTION_BINDING("STOP_IMMERSION", 1, 0, StopImmersion);
	CREATE_FUNCTION_BINDING("STOP_ALL_IMMERSION", 0, 0, StopAllImmersion);
	CREATE_FUNCTION_BINDING("SET_CREATURE_IN_TEMPLE", 1, 0, SetCreatureInTemple);
	CREATE_FUNCTION_BINDING("GAME_DRAW_TEXT", 7, 0, GameDrawText);
	CREATE_FUNCTION_BINDING("GAME_DRAW_TEMP_TEXT", 7, 0, GameDrawTempText);
	CREATE_FUNCTION_BINDING("FADE_ALL_DRAW_TEXT", 1, 0, FadeAllDrawText);
	CREATE_FUNCTION_BINDING("SET_DRAW_TEXT_COLOUR", 3, 0, SetDrawTextColour);
	CREATE_FUNCTION_BINDING("SET_CLIPPING_WINDOW", 5, 0, SetClippingWindow);
	CREATE_FUNCTION_BINDING("CLEAR_CLIPPING_WINDOW", 1, 0, ClearClippingWindow);
	CREATE_FUNCTION_BINDING("SAVE_GAME_IN_SLOT", 1, 0, SaveGameInSlot);
	CREATE_FUNCTION_BINDING("SET_OBJECT_CARRYING", 2, 0, SetObjectCarrying);
	CREATE_FUNCTION_BINDING("POS_VALID_FOR_CREATURE", 3, 1, PosValidForCreature);
	CREATE_FUNCTION_BINDING("GET_TIME_SINCE_OBJECT_ATTACKED", 2, 1, GetTimeSinceObjectAttacked);
	CREATE_FUNCTION_BINDING("GET_TOWN_AND_VILLAGER_HEALTH_TOTAL", 1, 1, GetTownAndVillagerHealthTotal);
	CREATE_FUNCTION_BINDING("GAME_ADD_FOR_BUILDING", 2, 0, GameAddForBuilding);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_ALIGNMENT_MUSIC", 1, 0, EnableDisableAlignmentMusic);
	CREATE_FUNCTION_BINDING("GET_DEAD_LIVING", 4, 1, GetDeadLiving);
	CREATE_FUNCTION_BINDING("ATTACH_SOUND_TAG", 4, 0, AttachSoundTag);
	CREATE_FUNCTION_BINDING("DETACH_SOUND_TAG", 3, 0, DetachSoundTag);
	CREATE_FUNCTION_BINDING("GET_SACRIFICE_TOTAL", 1, 1, GetSacrificeTotal);
	CREATE_FUNCTION_BINDING("GAME_SOUND_PLAYING", 2, 1, GameSoundPlaying);
	CREATE_FUNCTION_BINDING("GET_TEMPLE_POSITION", 1, 3, GetTemplePosition);
	CREATE_FUNCTION_BINDING("CREATURE_AUTOSCALE", 3, 0, CreatureAutoscale);
	CREATE_FUNCTION_BINDING("GET_SPELL_ICON_IN_TEMPLE", 2, 1, GetSpellIconInTemple);
	CREATE_FUNCTION_BINDING("GAME_CLEAR_COMPUTER_PLAYER_ACTIONS", 1, 0, GameClearComputerPlayerActions);
	CREATE_FUNCTION_BINDING("GET_FIRST_IN_CONTAINER", 1, 1, GetFirstInContainer);
	CREATE_FUNCTION_BINDING("GET_NEXT_IN_CONTAINER", 2, 1, GetNextInContainer);
	CREATE_FUNCTION_BINDING("GET_TEMPLE_ENTRANCE_POSITION", 3, 3, GetTempleEntrancePosition);
	CREATE_FUNCTION_BINDING("SAY_SOUND_EFFECT_PLAYING", 2, 1, SaySoundEffectPlaying);
	CREATE_FUNCTION_BINDING("SET_HAND_DEMO_KEYS", 1, 0, SetHandDemoKeys);
	CREATE_FUNCTION_BINDING("CAN_SKIP_TUTORIAL", 0, 1, CanSkipTutorial);
	CREATE_FUNCTION_BINDING("CAN_SKIP_CREATURE_TRAINING", 0, 1, CanSkipCreatureTraining);
	CREATE_FUNCTION_BINDING("IS_KEEPING_OLD_CREATURE", 0, 1, IsKeepingOldCreature);
	CREATE_FUNCTION_BINDING("CURRENT_PROFILE_HAS_CREATURE", 0, 1, CurrentProfileHasCreature);
}

} // namespace openblack::chlapi
