/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CHLApi.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <array>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraTracks.h"
#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "3D/ScreenFade.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/Audio.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/Services/Confirmation.h"
#include "Audio/Services/GameMusic.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Audio/Services/ScriptSound.h"
#include "Camera/Camera.h"
#include "Camera/CameraShake.h"
#include "Camera/FieldOfView.h"
#include "Camera/PlayerCameraScript.h"
#include "Camera/ScriptCamera.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/LeashScript.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/BonfireArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/MarkerArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/FeatureBuild.h"
#include "ECS/Flocks.h"
#include "ECS/IntroSpecial.h"
#include "ECS/LivingAngle.h"
#include "ECS/LivingWalkPath.h"
#include "ECS/MapCells.h"
#include "ECS/MissionaryBoat.h"
#include "ECS/MobileDrawing.h"
#include "ECS/MobileWalkPaths.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/PlayerCreature.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptContainers.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/ScriptHitObject.h"
#include "ECS/ScriptTimer.h"
#include "ECS/ScriptTypes.h"
#include "ECS/SeaCells.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/ScreenFadeSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerDrowning.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/Vortex.h"
#include "EngineConfig.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Help/HelpProfile.h"
#include "Help/HelpSystem.h"
#include "Help/InterfaceInteraction.h"
#include "Help/ScriptControl.h"
#include "Help/SpiritsRuntime.h"
#include "InfoConstants.h"
#include "Input/HandDemo.h"
#include "Locator.h"
#include "Magic/Script/CHLFire.h"
#include "Magic/Script/CHLInfluence.h"
#include "Magic/Script/CHLSpells.h"
#include "Magic/Script/CHLWeather.h"
#include "Magic/Script/CHLWorship.h"
#include "Magic/Script/ScriptPlayer.h"
#include "Particles/PSysManager.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "Video/FallingSpellVideo.h"
#include "Video/VideoPlayer.h"
#include "Worship/Citadel.h"

namespace openblack::chlapi
{

using namespace openblack::ecs::archetypes;

using openblack::Locator;
using openblack::MobileStaticInfo;
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

/// The scripts call some unimplemented functions every frame (GAME_THING_CLICKED: 37k lines): logged once per function
/// OPENBLACK_SCRIPT_THING_TRACE=1 (openblack only): MOVE_GAME_THING, SET_SCRIPT_STATE, SET_SCRIPT_ULONG and a PLAYED
/// that is true write a line, to follow a script that drives its things (Land 1's FollowUs)
bool ScriptThingTrace()
{
	static const bool on = [] {
		const char* env = std::getenv("OPENBLACK_SCRIPT_THING_TRACE");
		return env != nullptr && std::string_view(env) != "" && std::string_view(env) != "0";
	}();
	return on;
}

void NotImplemented(const char* function)
{
	static std::mutex mutex;
	static std::unordered_set<std::string> warned;
	const std::lock_guard lock(mutex);
	if (warned.insert(function).second)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CHLApi Function {}() not implemented (logged once).", function);
	}
}

/// The script VM as the game's script control asks it (task number, current task's script type, a task's script type,
/// stop tasks of a type)
help::script_control::Vm ScriptVm()
{
	help::script_control::Vm vm;
	vm.taskNumber = []() { return Locator::vm::value().GetCurrentTaskNumber(); };
	vm.currentTaskType = []() { return static_cast<uint32_t>(Locator::vm::value().GetCurrentTaskScriptType()); };
	vm.taskType = [](uint32_t task) { return static_cast<uint32_t>(Locator::vm::value().GetTaskScriptType(task)); };
	vm.stopTasksOfType = [](uint32_t mask) { Locator::vm::value().StopTasksOfType(static_cast<lhvm::ScriptType>(mask)); };
	vm.pushFloat = [](float value) { Locator::vm::value().Pushf(value); };
	vm.startScript = [](std::string_view name, uint32_t mask) {
		Locator::vm::value().StartScript(std::string(name), static_cast<lhvm::ScriptType>(mask));
	};
	return vm;
}

/// The check of the script camera opcodes: no camera mode -> "Script camera has been removed!"; a mode other than the
/// script camera -> "We are in the wrong camera mode!" (SET_CAMERA_POSITION says nothing). Either way the opcode does
/// nothing. openblack always has the player's mode, so only the second can happen. The "Script moving camera in
/// citadel" note of 003/004/287 is not ported: the citadel never has a script mode
bool ScriptCameraMode(const char* opcode)
{
	// A dual camera on top of the script mode (START_DUAL_CAMERA) is the current mode: the opcode does nothing
	if (script_camera::ScriptModeCurrent())
	{
		return true;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "{}: We are in the wrong camera mode!", opcode);
	return false;
}

/// The script's thing for an object id: the thing, or nullopt with the opcode's own message (the callers print
/// different strings: "Thing no longer valid", "Thing not found!", "Object no longer valid").
/// (approximate) As MusicThing: 0 is null and a valid entity stands for a live thing (the original looks the id up in
/// its script table)
std::optional<entt::entity> ScriptThing(uint32_t object, const char* opcode, const char* message)
{
	const auto entity = static_cast<entt::entity>(object);
	if (object != 0 && Locator::entitiesRegistry::value().Valid(entity))
	{
		return entity;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: {}", opcode, message);
	return std::nullopt;
}

/// ScriptThing for the camera opcodes: "Thing no longer valid"
std::optional<entt::entity> CameraThing(uint32_t object, const char* opcode)
{
	return ScriptThing(object, opcode, "Thing no longer valid");
}

/// The thing a leash opcode names: none for 0 or a thing no longer there. The original's messages for it are empty in
/// the shipped game, so nothing is logged
std::optional<entt::entity> LeashThing(uint32_t object)
{
	const auto entity = static_cast<entt::entity>(object);
	if (object != 0 && Locator::entitiesRegistry::value().Valid(entity))
	{
		return entity;
	}
	return std::nullopt;
}

/// Whether a thing is a creature, looked up without making any storage
bool IsLeashCreature(entt::entity entity)
{
	return Locator::entitiesRegistry::value().AllOf<ecs::components::Creature>(entity);
}

/// The creature a leash opcode names: none when the thing is missing or not a creature
std::optional<entt::entity> LeashCreature(uint32_t object)
{
	const auto thing = LeashThing(object);
	return thing.has_value() && IsLeashCreature(*thing) ? thing : std::nullopt;
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

/// The ground position of a script vector: the map coordinates keep y relative to the land, and after the creation
/// the physics editor step resets it to 0, so everything but a marker stands on the land (the CHL vectors come with
/// y = 0)
glm::vec3 OnGround(glm::vec3 position)
{
	if (Locator::terrainSystem::has_value())
	{
		position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	}
	return position;
}

/// The script's create switch, from CREATE 027 (angle 0, scale 1) and CREATE_WITH_ANGLE_AND_SCALE 252. The common step
/// after it rebuilds the 3D object's matrix at the altitude of pos + relY with only its own Y angle and scale (no X/Z
/// tilt), which the archetypes below already give.
/// @return entt::null when nothing is created (the script then gets 0)
entt::entity CreateScriptObject(const ObjectType type, uint32_t subtype, const glm::vec3& position, float yAngleRadians,
                                float scale)
{
	const auto& info = Locator::infoConstants::value();
	// sub_type 5000 is only for the types that ignore it
	if (subtype == 5000 && type != ObjectType::Timer && type != ObjectType::SpellDispenser && type != ObjectType::Whale &&
	    type != ObjectType::Ark && type != ObjectType::Marker && type != ObjectType::Ball && type != ObjectType::Poo &&
	    type != ObjectType::Scaffold)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CHL CREATE: trying to create an invalid sub_type");
		return entt::null;
	}
	const auto ground = OnGround(position);
	switch (type)
	{
	case ObjectType::Marker: // a script marker at the vector as given
		return MarkerArchetype::Create(position);
	case ObjectType::Feature: // (pos, feature info, angle, scale)
		if (subtype < info.feature.size())
		{
			return FeatureArchetype::Create(ground, static_cast<FeatureInfo>(subtype), yAngleRadians, scale);
		}
		break;
	case ObjectType::Villager:      // (pos, info, info.grownUpAge + 1): an adult
	case ObjectType::VillagerChild: // (pos, info, 10): a child
		if (subtype < info.villager.size())
		{
			const auto age = type == ObjectType::Villager ? info.villager.at(subtype).grownUpAge + 1 : 10u;
			return VillagerArchetype::Create(ground, ground, static_cast<VillagerInfo>(subtype), age, false);
		}
		break;
	case ObjectType::Animal: // (pos, animal info, 0, 0), birds too
	case ObjectType::Bird:
		if (subtype < info.animal.size())
		{
			return AnimalArchetype::Create(ground, static_cast<AnimalInfo>(subtype), 0, 0);
		}
		break;
	case ObjectType::MobileStatic:
	case ObjectType::Rock:
		if (subtype >= info.mobileStatic.size())
		{
			break;
		}
		if (subtype == static_cast<uint32_t>(MobileStaticInfo::SingingStoneBase))
		{
			// a base only, without angle or scale
			return MobileStaticArchetype::Create(ground, MobileStaticInfo::SingingStoneBase, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		}
		if (subtype == static_cast<uint32_t>(MobileStaticInfo::StreetLantern) || subtype == 59)
		{
			// a street lantern, the same as the map script's CREATE_STREET_LANTERN (59 is a country lantern)
			return StreetLanternArchetype::Create(ground, static_cast<MobileStaticInfo>(subtype));
		}
		// (pos, info, 0, 0, angle, scale): a rock, a mobile static, or for info 8 a bonfire
		return MobileStaticArchetype::CreateFromInfo(ground, static_cast<MobileStaticInfo>(subtype), 0.0f, yAngleRadians,
		                                             scale);
	case ObjectType::MobileObject: // (pos, mobile object info, 0, angle, scale)
		if (subtype < info.mobileObject.size())
		{
			return MobileObjectArchetype::Create(ground, static_cast<MobileObjectInfo>(subtype), yAngleRadians, scale);
		}
		break;
	case ObjectType::Poo: // mobile object info 5
		return MobileObjectArchetype::Create(ground, MobileObjectInfo::LumpOfPoo, yAngleRadians, scale);
	case ObjectType::Ark: // mobile object info 23
		return MobileObjectArchetype::Create(ground, MobileObjectInfo::Ark, yAngleRadians, scale);
	case ObjectType::Tree: // (pos, info, no forest, scale, angle, scale)
		if (subtype < info.tree.size())
		{
			return TreeArchetype::Create(0, ground, static_cast<TreeInfo>(subtype), true, yAngleRadians, scale, scale);
		}
		break;
	case ObjectType::AnimatedStatic: // (pos, animated static info, angle, scale)
		if (subtype < info.animatedStatic.size())
		{
			return AnimatedStaticArchetype::Create(ground, static_cast<AnimatedStaticInfo>(subtype), yAngleRadians, scale);
		}
		break;
	case ObjectType::Whale: // mobile object info 24, the shark
		return SharkArchetype::Create(ground, yAngleRadians, scale);
	case ObjectType::PuzzleGame: // (pos, sub_type, angle in 2048ths of a turn, scale)
		return openblack::ecs::CreatePuzzleGame(position, static_cast<script::PuzzleGameType>(subtype), yAngleRadians, scale);
	case ObjectType::Abode: // "Invalid create type" in the original too
	case ObjectType::Town:
	case ObjectType::Dance:
	case ObjectType::Flock:
	case ObjectType::InfluenceRing:
	case ObjectType::Citadel:
	case ObjectType::WorshipSite:
	case ObjectType::SpellSeed:
	case ObjectType::Mist:
	case ObjectType::Field:
	case ObjectType::ComputerPlayer:
	case ObjectType::TotemStatue:
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CHL CREATE: invalid create type {}", static_cast<int>(type));
		return entt::null;
	case ObjectType::WeatherThing:
		return magic::script::CreateWeatherThing(subtype, position); // Magic/Script/CHLWeather.cpp
	// the miracle cases (Magic/Script/CHLWorship.cpp)
	case ObjectType::OneShotSpell:
		return magic::script::CreateOneShotSpell(subtype, position);
	case ObjectType::OneShotSpellInHand:
		return magic::script::CreateOneShotSpellInHand(subtype);
	case ObjectType::SpellDispenser:
		return magic::script::CreateSpellDispenser(subtype, position, yAngleRadians, scale);
	case ObjectType::Scaffold: // (pos, scaffold info, angle, scale)
		// no town, no owner (inferred); the sub-type is not read (5000 is accepted above). Its value comes from the scale
		// (1.0 -> 3)
		return ecs::scaffolds::Create(map_coords::FromWorld(ground), entt::null, entt::null, yAngleRadians, scale);
	case ObjectType::Timer: // the sub-type is the seconds
		return ecs::script_timer::Create(static_cast<float>(subtype));
	case ObjectType::Vortex: // (pos, sub_type, 50.0): In / Out / Volcano
		return ecs::vortex::Create(position, static_cast<VortexType>(subtype));
	default:
		// TODO: Reward, Creature, DeadTree, Store, Ball, Totem, Highlight
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "CreateScriptObject not implemented for type {}", static_cast<int>(type));
		return entt::null;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CHL CREATE: invalid sub_type {} for type {}", subtype,
	                    static_cast<int>(type));
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

CHLApi::CHLApi()
{
	_functionsTable.reserve(464);
	InitFunctionsTable0();
	InitFunctionsTable1();
	InitFunctionsTable2();
	InitFunctionsTable3();
	InitFunctionsTable4();
}

void None() {} // 000 NONE

void SetCameraPosition() // 001 SET_CAMERA_POSITION
{
	const auto position = PopVec();
	if (ScriptCameraMode(__func__))
	{
		script_camera::SetPosition(position);
	}
}

void SetCameraFocus() // 002 SET_CAMERA_FOCUS
{
	const auto position = PopVec();
	if (ScriptCameraMode(__func__))
	{
		script_camera::SetFocus(position);
	}
}

void MoveCameraPosition() // 003 MOVE_CAMERA_POSITION
{
	// in seconds of the wall clock
	const auto time = Popf();
	const auto position = PopVec();
	if (ScriptCameraMode(__func__))
	{
		script_camera::MovePosition(position, time);
	}
}

void MoveCameraFocus() // 004 MOVE_CAMERA_FOCUS
{
	const auto time = Popf();
	const auto position = PopVec();
	if (ScriptCameraMode(__func__))
	{
		script_camera::MoveFocus(position, time);
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

/// The help spirit of a popped SCRIPT_SPIRIT_TYPE (the local player's alignment: inferred, openblack's local player is
/// PLAYER_ONE; the random draw uses game_random's local stream)
int32_t ScriptSpirit(int32_t type)
{
	const int discrete = audio::DiscreteAlignment(ecs::effects::alignment::Get(PlayerNames::PLAYER_ONE));
	return help::ResolveScriptAdvisor(type, discrete, []() { return audio::tags::RandomSample(0, 100); });
}

/// The help system's AdvisorSpiritController (Help/SpiritsRuntime.h); nullptr before help::spirits::Start (openblack only)
help::spirits::AdvisorSpiritController* SpiritControl()
{
	auto* runtime = help::spirits::Get();
	return runtime != nullptr ? &runtime->Control() : nullptr;
}

/// The running task is a help script
bool IsHelpTask()
{
	return static_cast<uint32_t>(Locator::vm::value().GetCurrentTaskScriptType()) ==
	       static_cast<uint32_t>(lhvm::ScriptType::Help);
}

/// "Invalid Y" then "Invalid X" for a value below 0 (NaN too) or above 1; the opcode goes on
void CheckScreenXY(const char* opcode, float x, float y)
{
	const auto invalid = [](float v) { return !(v >= 0.0f) || v > 1.0f; };
	if (invalid(y))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Invalid Y", opcode);
	}
	if (invalid(x))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Invalid X", opcode);
	}
}

/// ScriptThing for the spirit opcodes: the object, or 0 with "Object no longer valid" (approximate, as MusicThing: 0
/// is null and a valid entity stands for a live thing)
uint32_t SpiritThing(uint32_t object, const char* opcode)
{
	if (object != 0 && Locator::entitiesRegistry::value().Valid(static_cast<entt::entity>(object)))
	{
		return object;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Object no longer valid", opcode);
	return 0;
}

void SpiritEject() // 007 SPIRIT_EJECT
{
	// POP the spirit -> AdvisorSpiritController::SpiritEject(t, help script): a help script's spirit appears, any other is
	// ejected
	const auto spirit = ScriptSpirit(Pop().intVal);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritEject(spirit, IsHelpTask());
	}
}

void SpiritHome() // 008 SPIRIT_HOME
{
	// POP the spirit -> HelpSystem::SpiritHome(t, help script): a help script's spirit vanishes; any other flies home
	const auto spirit = ScriptSpirit(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->SpiritHome(spirit, IsHelpTask() ? 1 : 0);
	}
}

void SpiritPointPos() // 009 SPIRIT_POINT_POS
{
	// POP in_world (the raw dword), the position, the spirit -> the spirit points there (eject, then point mode 1 with
	// 8.0 / 5.0)
	const auto inWorld = Pop().intVal != 0;
	const auto position = PopVec();
	const auto spirit = ScriptSpirit(Pop().intVal);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritPointPosition(spirit, position, inWorld);
	}
}

void SpiritPointGameThing() // 010 SPIRIT_POINT_GAME_THING
{
	// POP in_world, the object, the spirit (converted before the test); no object -> "Object no longer valid"; else the
	// spirit points at it (re-sent every turn)
	const auto inWorld = Pop().intVal != 0;
	const auto target = Pop().uintVal;
	const auto spirit = ScriptSpirit(Pop().intVal);
	const auto object = SpiritThing(target, "SPIRIT_POINT_GAME_THING");
	if (auto* control = SpiritControl(); control != nullptr && object != 0)
	{
		control->SpiritPointObject(spirit, object, inWorld);
	}
}

void GameThingFieldOfView() // 011 GAME_THING_FIELD_OF_VIEW
{
	// A multiplayer game pushes 1 without popping (not in openblack). POP the thing (none -> "Object no longer valid",
	// 0); the drawn camera's screen test of its bounding sphere (an object) or of its point (Camera/FieldOfView.h)
	const auto object = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GAME_THING_FIELD_OF_VIEW: Object no longer valid");
		Pushb(false);
		return;
	}
	Pushb(field_of_view::ThingInView(entity));
}

void PosFieldOfView() // 012 POS_FIELD_OF_VIEW
{
	// POP the vector; inside the temple 0, else whether the point is on the screen of the drawn camera
	// (Camera/FieldOfView.h)
	const auto position = PopVec();
	Pushb(field_of_view::PosInView(position));
}

// A script string widened as the original's ANSI code page conversion (up to 2047 characters).
// (approximate) byte by byte, the same as code page 1252 except for 0x80..0x9F, and without the length limit
std::u16string WidenScriptString(const std::string& text)
{
	std::u16string wide;
	wide.reserve(text.size());
	for (const char c : text)
	{
		wide.push_back(static_cast<char16_t>(static_cast<unsigned char>(c)));
	}
	return wide;
}

void RunText() // 013 RUN_TEXT
{
	const auto withInteraction = Pop().intVal;
	const auto textID = static_cast<uint32_t>(Pop().intVal);
	const auto singleLine = static_cast<bool>(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->RunText(singleLine, textID, withInteraction);
	}
}

void TempText() // 014 TEMP_TEXT
{
	const auto withInteraction = Pop().intVal;
	const auto string = PopString();
	const auto singleLine = static_cast<bool>(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->TempText(singleLine, WidenScriptString(string), withInteraction);
	}
}

void TextRead() // 015 TEXT_READ
{
	// whether the text was read, pushed as a bool (type 6)
	const auto* helpSystem = help::Get();
	Pushb(helpSystem != nullptr && helpSystem->IsTextRead());
}

void GameThingClicked() // 016 GAME_THING_CLICKED
{
	// (pending) a multiplayer game: "This is not multiplayer friendly yet!" and true, without the pop: openblack has no
	// multiplayer game
	const auto object = Pop().uintVal;
	const auto thing = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GAME_THING_CLICKED: Object no longer valid");
		Pushb(false);
		return;
	}
	auto& hand = Locator::handSystem::value();
	// the last object tapped with the action button == the thing
	const bool clicked = hand.GetClickedObject() == thing;
	// a scroll (not a "did you know") of the challenges 0x38, 0x3B, 0x3C, 0x3D: the click is cleared, the game saved
	// at once (pending: no save rooms), then it is tapped again
	if (clicked && ecs::script_highlight::IsHighlight(thing) && !ecs::script_highlight::IsDidYouKnow(thing) &&
	    ecs::script_highlight::SavesGameWhenClicked(ecs::script_highlight::ScriptIdOf(thing)))
	{
		hand.ClearClicked();
		hand.RememberTapped(thing);
	}
	Pushb(clicked); // pushed as a bool (type 6)
}

void SetScriptState() // 017 SET_SCRIPT_STATE
{
	// the state (first pop), then the object
	const auto state = Pop().intVal;
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	// none -> "Object no longer valid"
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_SCRIPT_STATE: Object no longer valid");
		return;
	}
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_SCRIPT_STATE {} {}", object, state);
	}
	// a script container: the state and each living member's script state, no drowning test
	if (ecs::script_containers::IsContainer(entity))
	{
		ecs::script_containers::ForEachMember(entity, [state](entt::entity member) {
			ecs::script_held::SetLivingScriptState(member, static_cast<uint32_t>(state));
			return false;
		});
		return;
	}
	// a living thing that is not drowning -> its script state; else "Object not living for set state"
	if (registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(entity) && !ecs::IsDrowning(entity))
	{
		ecs::script_held::SetLivingScriptState(entity, static_cast<uint32_t>(state));
		return;
	}
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		// a creature's (with its script clip). TODO: creatures
		NotImplemented("SetScriptState (creature)");
		return;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_SCRIPT_STATE: Object not living for set state");
}

void SetScriptStatePos() // 018 SET_SCRIPT_STATE_POS
{
	// const auto position = PopVec();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetScriptFloat() // 019 SET_SCRIPT_FLOAT
{
	// const auto value = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetScriptUlong() // 020 SET_SCRIPT_ULONG
{
	// the times (first pop), the clip, then the object
	const auto loop = static_cast<uint32_t>(Pop().intVal);
	const auto animation = static_cast<uint32_t>(Pop().intVal);
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	// none -> "Object no longer valid"
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_SCRIPT_ULONG: Object no longer valid");
		return;
	}
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_SCRIPT_ULONG {} clip {} times {}", object, animation, loop);
	}
	// a script container: the clip and times on each villager member
	if (ecs::script_containers::IsContainer(entity))
	{
		ecs::script_containers::ForEachMember(entity, [&registry, animation, loop](entt::entity member) {
			if (registry.AllOf<ecs::components::Villager>(member))
			{
				ecs::villager::SetScriptAnimation(member, animation, loop);
			}
			return false;
		});
		return;
	}
	// a villager -> its script clip and times
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		ecs::villager::SetScriptAnimation(entity, animation, loop);
		return;
	}
	// a creature -> its script clip and times. TODO: creatures
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		NotImplemented("SetScriptUlong (creature)");
		return;
	}
	// anything else: "setting the state of something neither a creature nor a villager"
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"),
	                    "SET_SCRIPT_ULONG: setting the state of something neither a creature nor a villager");
}

void GetProperty() // 021 GET_PROPERTY
{
	// the object, then the property
	const auto object = Pop().uintVal;
	const auto prop = static_cast<script::ObjectPropertyType>(Pop().intVal);
	const auto entity = static_cast<entt::entity>(object);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "GET_PROPERTY: Thing no longer valid");
		Pushf(0.0f);
		return;
	}
	switch (prop)
	{
	case script::ObjectPropertyType::Scale:
		// the object's scale: the Transform's uniform scale. (pending) a villager or an animal only here
		if (registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(entity))
		{
			Pushf(registry.Get<const Transform>(entity).scale.x);
			return;
		}
		NotImplemented("GetProperty (Scale)");
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Speed:
		// ecs::living::SpeedProperty (the physics, the dead living things)
		if (const auto speed = ecs::living::SpeedProperty(entity); speed.has_value())
		{
			Pushf(*speed);
			return;
		}
		NotImplemented("GetProperty (Speed)");
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Age:
		// a living thing: its age as a float; else "Not used on non living objects" and 0
		if (const auto age = ecs::living::AgeProperty(entity); age.has_value())
		{
			Pushf(*age);
			return;
		}
		if (registry.AllOf<ecs::components::Creature>(entity))
		{
			NotImplemented("GetProperty (Age, creature)"); // TODO: creatures
			Pushf(0.0f);
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_PROPERTY: Not used on non living objects");
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Flying:
		// whether the object has a physics object (the same flag the original's in-the-air test reads), asleep resting
		// proxies included
		Pushb(openblack::ecs::physics::PhysicsObjects::Find(entity) != nullptr);
		return;
	case script::ObjectPropertyType::Drowning: // whether it is drowning
		Pushb(openblack::ecs::IsDrowning(entity));
		return;
	case script::ObjectPropertyType::YPos:
		// the altitude of any thing, as a float. (pending) only the highlights here
		if (ecs::script_highlight::IsHighlight(entity))
		{
			Pushf(ecs::script_highlight::GetYPos(entity));
			return;
		}
		NotImplemented(__func__);
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::BuiltPercentage: // a building's built percentage, else 1
		if (const auto percent = openblack::ecs::abodes::GetBuiltPercentage(entity); percent.has_value())
		{
			Pushf(*percent);
			return;
		}
		if (const auto percent = openblack::ecs::feature_build::GetBuiltPercentage(entity); percent.has_value())
		{
			Pushf(*percent);
			return;
		}
		NotImplemented(__func__);
		Pushf(1.0f);
		return;
	default:
		// TODO(Daniels118): implement the other properties
		NotImplemented(__func__);
		Pushi(0);
		return;
	}
}

void SetProperty() // 022 SET_PROPERTY
{
	// the value, the object, the property
	const auto val = Popf();
	const auto object = Pop().uintVal;
	const auto prop = static_cast<script::ObjectPropertyType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	// a villager or an animal (the original's living cast; the script holds them as Villager / Animal entities)
	const bool living =
	    object != 0 && registry.Valid(entity) && registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(entity);
	switch (prop)
	{
	case script::ObjectPropertyType::Scale:
		// the object's scale (and its matrix when it changes): the Transform's uniform scale. (pending) a villager or an
		// animal only here
		if (living)
		{
			registry.Get<Transform>(entity).scale = glm::vec3(val);
			return;
		}
		NotImplemented("SetProperty (Scale)");
		return;
	case script::ObjectPropertyType::Speed:
		// the speed in metres: a villager's (no per-villager factor; it stays, the state speed skips a villager controlled by
		// a script), the wall hug's for an animal
		if (living && registry.AllOf<ecs::components::Villager>(entity))
		{
			ecs::SetVillagerSpeedInMetres(entity, val);
			return;
		}
		if (living)
		{
			ecs::animal_ai::SetSpeedInMetres(entity, val);
			return;
		}
		NotImplemented("SetProperty (Speed)");
		return;
	case script::ObjectPropertyType::Age:
		// a living thing -> its age (truncated): a villager's (the meshes when grownUpAge is crossed, the scale for the age),
		// an animal's; else "Cannot Set Property"
		if (living && registry.AllOf<ecs::components::Villager>(entity))
		{
			ecs::villager::SetAgeAndScale(entity, static_cast<uint32_t>(openblack::map_coords::FtoL(val)));
			return;
		}
		if (living)
		{
			ecs::animal_ai::SetAge(entity, static_cast<uint32_t>(openblack::map_coords::FtoL(val)));
			return;
		}
		if (object != 0 && registry.Valid(entity) && registry.AllOf<ecs::components::Creature>(entity))
		{
			NotImplemented("SetProperty (Age, creature)"); // TODO: creatures
			return;
		}
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "SET_PROPERTY: Cannot Set Property {}", static_cast<int>(prop));
		return;
	case script::ObjectPropertyType::Flying:
	case script::ObjectPropertyType::Drowning:
	case script::ObjectPropertyType::Moving:
		// "Cannot Set Property %d", nothing changes
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "SET_PROPERTY: Cannot Set Property {}", static_cast<int>(prop));
		return;
	case script::ObjectPropertyType::YPos:
		// a script highlight -> its draw height; then, for any thing, its altitude = val and its 3D object moved there.
		// (pending) only the highlights here
		if (object != 0 && ecs::script_highlight::IsHighlight(static_cast<entt::entity>(object)))
		{
			ecs::script_highlight::SetYPos(static_cast<entt::entity>(object), val);
			return;
		}
		NotImplemented(__func__);
		return;
	case script::ObjectPropertyType::BuiltPercentage:
		// a building -> its built percentage (the features here); anything else -> "Cannot Set Property"
		if (object == 0 || !(openblack::ecs::abodes::SetBuiltPercentage(static_cast<entt::entity>(object), val) ||
		                     openblack::ecs::feature_build::SetBuiltPercentage(static_cast<entt::entity>(object), val)))
		{
			NotImplemented(__func__);
		}
		return;
	default:
		// TODO(Daniels118): implement the other properties
		NotImplemented(__func__);
		return;
	}
}

void GetPosition() // 023 GET_POSITION
{
	const auto objId = Pop().uintVal;

	// the altitude of pos + relY, the object's world position; (0, 0, 0) for a lost object
	glm::vec3 position(0.0f);
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(objId);
	if (objId != 0 && registry.Valid(entity))
	{
		const auto* transform = registry.TryGet<const Transform>(entity);
		if (const auto* flock = registry.TryGet<const ecs::components::Flock>(entity); flock != nullptr)
		{
			// a flock: its first member's position, else the flock's own domain centre
			position = flock->domainCentre;
			if (!flock->members.empty() && registry.Valid(flock->members.front()))
			{
				if (const auto* leader = registry.TryGet<const Transform>(flock->members.front()); leader != nullptr)
				{
					position = leader->position;
				}
			}
		}
		else if (registry.AllOf<ecs::components::Villager>(entity) && ecs::villager::AreWeThereAtDestination(entity, 0.0f))
		{
			// a wall hugger that is not a creature and is already there: its destination instead of its position. Its height
			// is the land's + the destination's (approximate: the land under it, openblack's WallHug keeps only x / z;
			// GET_DISTANCE ignores y). TODO: the animals' (AnimalBrain goal)
			const auto dest = *ecs::villager::GetDestPos(entity);
			position = OnGround(glm::vec3(dest.x, 0.0f, dest.y));
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
	const auto objId = Pop().uintVal;

	if (objId != 0)
	{
		const auto& island = Locator::terrainSystem::value();
		position.y = island.GetHeightAt(glm::vec2(position.x, position.z));
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = static_cast<entt::entity>(objId);
		auto* transform = registry.TryGet<Transform>(entity);
		if (transform != nullptr)
		{
			transform->position = position;
			// the position at the start of the turn becomes the new one: drawn there from the next frame, no slide
			ecs::NotifyTeleported(entity);
		}
		// a living thing controlled by the script -> script state 4
		if (registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(entity) &&
		    ecs::script_held::IsControlledByScript(entity))
		{
			ecs::script_held::SetLivingScriptState(entity, 4); // IN_SCRIPT
		}
	}
}

void GetDistance() // 025 GET_DISTANCE
{
	// the two vectors to gutils::GetDistance = hypotenuse(dx, dz): x and z only, y is ignored; under 0.5 it is 0
	const auto p1 = PopVec();
	const auto p0 = PopVec();
	const float distance = gutils::GetDistance(p0, p1);
	Pushf(distance < 0.5f ? 0.0f : distance);
}

namespace
{
/// CALL / CALL_NEAR for creatures: the first creature with GetDistanceInMetres(point, its map coords) <= 1.0 (CALL: no
/// type or sub-type test) / <= r and its sub-type == subtype (CALL_NEAR). (approximate) openblack has no creature
/// list: the registry's order. (pending, creature) the sub-type is taken as the creature's species
/// (ecs::script_type::SubtypeOf)
entt::entity FindCreatureForScript(const map_coords::MapCoords& coords, uint32_t subtype, std::optional<float> radius)
{
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity found = entt::null;
	registry.Each<const ecs::components::Creature>([&](entt::entity creature, const ecs::components::Creature& data) {
		if (found != entt::null || (radius.has_value() && static_cast<uint32_t>(data.species) != subtype))
		{
			return;
		}
		if (gutils::GetDistanceInMetres(coords, ecs::object::MapCoordsOf(creature)) <= radius.value_or(1.0f))
		{
			found = creature;
		}
	});
	return found;
}

/// CALL / CALL_NEAR after their pops (radius: CALL_NEAR's, nullopt for CALL):
/// - type 1..41, else "Invalid type=%d" and 0;
/// - the point as map coords to the type's find function (one for CALL, one for CALL_NEAR); none for NONE, MARKER,
///   DANCE, FLOCK, INFLUENCE_RING and WEATHER_THING: "No find function for type=%d" and 0;
/// - found: added as a script thing; else the warning "Thing not found" and 0.
/// The original's script error and warning messages are empty in the shipped game: it prints none of these. openblack
/// keeps them as its own diagnostics, "Thing not found" at debug level, since the scripts' waiting loops
/// (FollowUs L53071..53087) ask every turn until the thing exists
entt::entity FindForScript(int32_t type, uint32_t subtype, const glm::vec3& position, std::optional<float> radius,
                           bool excludingScripted)
{
	if (type <= 0 || type >= 0x2A)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid type={}", type);
		return entt::null;
	}
	const auto scriptType = static_cast<ObjectType>(type);
	const auto coords = map_coords::FromWorld(position);
	entt::entity found = entt::null;
	switch (scriptType)
	{
	case ObjectType::Marker:
	case ObjectType::Dance:
	case ObjectType::Flock:
	case ObjectType::InfluenceRing:
	case ObjectType::WeatherThing:
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "No find function for type={}", type);
		return entt::null;
	case ObjectType::Town:
		// the nearest town within r (10.0 for CALL). The type, the sub-type, the filter and excludingScripted are not used
		found = ecs::map_cells::GetNearestTown(coords, radius.value_or(10.0f));
		break;
	case ObjectType::Creature:
		found = FindCreatureForScript(coords, subtype, radius); // excludingScripted is not used either
		break;
	default:
	{
		// the nearest accepted thing of the map cells of the square, not cut at r. The filter: the type and sub-type for
		// CALL; for CALL_NEAR also GetDistanceInMetres(point, the thing or its totem) <= r; things in a script are rejected
		// when excludingScripted
		const float searchRadius = radius.value_or(1.0f);
		found = ecs::map_cells::FindNearForScript(
		    coords,
		    [&](entt::entity thing) {
			    if (excludingScripted && ecs::script_held::IsInScript(thing))
			    {
				    return false;
			    }
			    if (!ecs::script_type::Matches(thing, scriptType, subtype))
			    {
				    return false;
			    }
			    return !radius.has_value() ||
			           gutils::GetDistanceInMetres(coords, ecs::map_cells::ScriptDistancePoint(thing)) <= searchRadius;
		    },
		    searchRadius);
		break;
	}
	}
	if (found == entt::null)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "Thing not found");
		return entt::null;
	}
	ecs::script_held::AddScriptThing(found, false);
	return found;
}
} // namespace

void Call() // 026 CALL
{
	// pops excludingScripted, the point (z, y, x), the sub-type, then the type
	const auto excludingScripted = Pop().intVal != 0;
	const auto position = PopVec();
	const auto subtype = static_cast<uint32_t>(Pop().intVal);
	const auto type = Pop().intVal;
	const auto thing = FindForScript(type, subtype, position, std::nullopt, excludingScripted);
	// pushed as an object. (known limitation) CHLApi's entity-as-id convention: entity 0 would read as "not found"
	// (THING_VALID tests objId != 0), as everywhere else in CHLApi
	Pusho(thing == entt::null ? 0 : static_cast<uint32_t>(thing));
}

void Create() // 027 CREATE
{
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = static_cast<ObjectType>(Pop().intVal);

	// types 1..41 only, angle 0, scale 1; 0 when nothing is made
	const auto object = type > ObjectType::None && type <= ObjectType::AnimatedStatic
	                        ? CreateScriptObject(type, subtype, position, 0.0f, 1.0f)
	                        : entt::null;
	// added as a thing the script created
	ecs::script_held::AddScriptThing(object, true);

	Pusho(object == entt::null ? 0 : static_cast<uint32_t>(object));
}

void Random() // 028 RANDOM
{
	// the first POP is max, the second min; ((max - min) + 1) as a float, a synced GameFloatRand of it, + min, truncated
	// towards 0 and pushed back as a float: a whole number in [min, max + 1)
	const auto max = Popf();
	const auto min = Popf();
	const float span = max - min;
	const float range = span + 1.0f;
	const float drawn = game_random::GameFloatRand(range);
	const float sum = drawn + min;
	Pushf(static_cast<float>(static_cast<int32_t>(sum)));
}

void DllGettime() // 029 DLL_GETTIME
{
	// The game's own table has no handler here; the script library DLL puts its own in, LHVM::PushElaspedTime (the VM
	// tick count, one a game turn outside the citadel, x 0.1f)
	Locator::vm::value().PushElaspedTime();
}

void StartCameraControl() // 030 START_CAMERA_CONTROL
{
	// Help/ScriptControl.cpp. Inside the citadel (game_clock::IsInsideCitadel) no camera mode. Outside, the script
	// camera mode is created unless the current mode cannot be left (Camera/ScriptCamera.h)
	const bool insideCitadel = openblack::game_clock::IsInsideCitadel();
	auto& cameraControl = help::script_control::GetCameraControl();
	bool cameraTaken = false;
	if (!insideCitadel)
	{
		const auto& camera = Locator::camera::value();
		cameraTaken = script_camera::BeginFrom(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	}
	const bool granted = help::script_control::StartCameraControl(cameraControl, ScriptVm(), insideCitadel, cameraTaken);
	Pushb(granted);
}

void EndCameraControl() // 031 END_CAMERA_CONTROL
{
	// Help/ScriptControl.cpp: the camera is given back when this task has it; its camera part (the script mode deleted,
	// the player's mode from where the camera is, the FOV back to 70 degrees in 0.5 s) is script_camera::End
	if (help::script_control::EndCameraControl(help::script_control::GetCameraControl(), audio::GetScriptAudioState(),
	                                           ScriptVm()))
	{
		script_camera::End();
	}
}

void SetWidescreen() // 032 SET_WIDESCREEN
{
	// The help system's wide screen; the bars slide in HelpSystemInfo.wideScreenTime seconds (2.0). Only the task that
	// holds it or any when none does (Help/ScriptControl.cpp); the help system's hook moves the bars (Game.cpp)
	const auto on = static_cast<int32_t>(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		help::script_control::SetWideScreen(*helpSystem, on, ScriptVm());
	}
	else
	{
		// openblack only (the original always has a help system; here a VM without a HelpSystem, e.g. tools): the bars move
		// without any owner. 2.0 is the default of ScreenFade::_wideTime (no source)
		const float time =
		    Locator::infoConstants::has_value() ? Locator::infoConstants::value().helpSystem.wideScreenTime : 2.0f;
		Locator::screenFade::value().Fade().SetWideScreen(on != 0, time);
		audio::SetScriptWideScreen(on != 0);
	}
}

void MoveGameThing() // 033 MOVE_GAME_THING
{
	// The pops: the radius (only the creature's), z, y, x, the object
	const auto radius = Popf();
	const auto position = PopVec();
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	// none -> "Thing no longer valid"
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "MOVE_GAME_THING: Thing no longer valid");
		return;
	}
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "MOVE_GAME_THING {} to ({:.2f}, {:.2f}, {:.2f}) radius {:.2f}", object,
		                   position.x, position.y, position.z, radius);
	}
	// the map coords of pos: x / z of the point, y kept above the land (the walks use x / z)
	const glm::vec2 goal(position.x, position.z);
	// a creature
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		// a creature in the map (else "no creature for script") is prepared for a scripted action and given the sub-actions
		// to walk there. TODO: openblack has no creature AI
		NotImplemented("MoveGameThing (creature)");
		return;
	}
	// a living thing: only when it is in the map and not drowning, else nothing
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		if (!ecs::villager::IsObjectInMap(entity) || ecs::IsDrowning(entity))
		{
			return;
		}
		// not there yet -> SetupMoveToPos(coords, IN_SCRIPT 4); else SetScriptState(IN_SCRIPT 4)
		if (!ecs::villager::AreWeThere(entity, goal, 0.0f))
		{
			ecs::villager::SetupMoveToPos(entity, goal, VillagerStates::InScript);
		}
		else
		{
			ecs::villager::SetScriptState(entity, VillagerStates::InScript);
		}
		return;
	}
	if (registry.AllOf<ecs::components::Animal>(entity))
	{
		// The same living branch for an animal: there already -> SetScriptState(IN_SCRIPT 4), else SetupMoveToPos(pos,
		// IN_SCRIPT 4) (animal_ai::ScriptMoveTo). (approximate) "in the map" as "not in the hand" (IN_HAND)
		const auto* brain = registry.TryGet<const ecs::components::AnimalBrain>(entity);
		if (brain == nullptr || ecs::IsDrowning(entity) ||
		    static_cast<ecs::animal_ai::AnimalState>(brain->topState) == ecs::animal_ai::AnimalState::InHand)
		{
			return;
		}
		ecs::animal_ai::ScriptMoveTo(entity, goal);
		return;
	}
	// a flock -> Flock::SetDomainCentrePos(coords): its tail's destination (a villager's or an animal's) and the flock's
	// domain centre (ECS/Flocks.h)
	if (registry.AllOf<ecs::components::Flock>(entity))
	{
		ecs::flocks::SetDomainCentrePos(entity, position);
		return;
	}
	// a weather thing or a computer player would move too (the latter with 60.0); openblack has neither kind of thing.
	// anything else: "Jonty - Thing must be living to move it!", then its position = coords, so its world point is the
	// vector itself. (approximate) only the Transform moves: openblack's derived data (static meshes, physics) is not told
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "MOVE_GAME_THING: Jonty - Thing must be living to move it!");
	if (auto* transform = registry.TryGet<Transform>(entity); transform != nullptr)
	{
		transform->position = position;
		ecs::NotifyTeleported(entity);
	}
}

void SetFocus() // 034 SET_FOCUS
{
	// z, y, x, then the thing
	const auto position = PopVec();
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "SET_FOCUS: Thing no longer valid");
		return;
	}
	// a script container: the focus on each member, without taking it under script control
	if (ecs::script_containers::IsContainer(entity))
	{
		ecs::script_containers::ForEachMember(entity, [position](entt::entity member) {
			if (!ecs::living::SetFocus(member, position))
			{
				NotImplemented("SetFocus (container member that is not a villager or an animal)");
			}
			return false;
		});
		return;
	}
	// a creature is put under script control first. TODO: creatures
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		NotImplemented("SetFocus (creature)");
		return;
	}
	// an instant snap of the yaw, nothing keeps the focus (villagers and animals: living::SetFocus)
	if (ecs::living::SetFocus(entity, position))
	{
		if (ScriptThingTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_FOCUS {} ({}, {}, {})", object, position.x, position.y,
			                   position.z);
		}
		return;
	}
	// (pending) another object: its Y angle set the same way (its x / z angles kept)
	NotImplemented("SetFocus (object)");
}

void HasCameraArrived() // 035 HAS_CAMERA_ARRIVED
{
	// (1 in a network game, not ported) the current camera mode's arrived test: the script mode's, or the player's with
	// the same squared distance 0.001 to the destinations: here the player's Camera zoomers (Zoomer3; (inferred) the
	// original has one camera for both modes)
	if (script_camera::HasMode()) // the script mode, or a dual camera
	{
		Pushb(script_camera::ScriptArrived());
		return;
	}
	const auto& camera = Locator::camera::value();
	const auto dp = camera.GetOrigin() - camera.GetOrigin(Camera::Interpolation::Target);
	const auto df = camera.GetFocus() - camera.GetFocus(Camera::Interpolation::Target);
	Pushb(glm::dot(dp, dp) < script_camera::k_ArrivedDistanceSquared &&
	      glm::dot(df, df) < script_camera::k_ArrivedDistanceSquared);
}

void FlockCreate() // 036 FLOCK_CREATE
{
	// the position (z, y, x), a flock added as a thing the script created (ECS/ScriptContainers.h). (pending) the
	// script name of the create
	const auto position = PopVec();
	const auto flock = ecs::script_containers::CreateFlock(position);
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "FLOCK_CREATE {} ({}, {}, {})", static_cast<uint32_t>(flock), position.x,
		                   position.y, position.z);
	}
	Pusho(static_cast<uint32_t>(flock));
}

void FlockAttach() // 037 FLOCK_ATTACH
{
	// the leader flag, the target, then the obj (ECS/ScriptContainers.h)
	const auto asLeader = Pop().intVal != 0;
	const auto target = Pop().uintVal;
	const auto object = Pop().uintVal;
	const auto pushed = ecs::script_containers::Attach(object, target, asLeader);
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "FLOCK_ATTACH {} to {} leader {} -> {}", object, target, asLeader, pushed);
	}
	Pusho(pushed);
}

void FlockDetach() // 038 FLOCK_DETACH
{
	// the container (first pop), then the obj id (ECS/ScriptContainers.h)
	const auto container = Pop().uintVal;
	const auto object = Pop().uintVal;
	const auto pushed = ecs::script_containers::Detach(container, object);
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "FLOCK_DETACH {} from {} -> {}", object, container, pushed);
	}
	Pusho(pushed);
}

void FlockDisband() // 039 FLOCK_DISBAND
{
	// ECS/ScriptContainers.h
	ecs::script_containers::Disband(Pop().uintVal);
}

void IdSize() // 040 ID_SIZE
{
	// the members as a float; 0 with "Cannot Find Flock/Dance/Town Size"
	Pushf(ecs::script_containers::Size(Pop().uintVal).value_or(0.0f));
}

void FlockMember() // 041 FLOCK_MEMBER
{
	// const auto flock = Pop().uintVal;
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GetHandPosition() // 042 GET_HAND_POSITION
{
	const auto handEntity = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	auto& handTransform = Locator::entitiesRegistry::value().Get<Transform>(handEntity);

	PushVec(handTransform.position);
}

void PlaySoundEffect() // 043 PLAY_SOUND_EFFECT
{
	// six POPs (withPos, z, y, x, bank, sample), then audio::script_sound
	const auto withPosition = Pop().intVal != 0;
	const auto position = PopVec();
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	audio::script_sound::PlaySoundEffect(sample, bank, position, withPosition);
}

/// The script's thing for the music functions: the object, or nullopt with the original's "Thing no longer valid"
/// (approximate: the original looks the id up in its script table, range 1..511; here 0 is null as in Pusho and a
/// valid entity stands for a live thing)
std::optional<audio::ThingId> MusicThing(uint32_t objId)
{
	if (objId != 0 && Locator::entitiesRegistry::value().Valid(static_cast<entt::entity>(objId)))
	{
		return objId;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing no longer valid");
	return std::nullopt;
}

void StartMusic() // 044 START_MUSIC
{
	const auto music = Pop().intVal;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "START_MUSIC({})", music);
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
	{
		gameMusic->ScriptStartMusic(music);
	}
}

void StopMusic() // 045 STOP_MUSIC
{
	// the script music set to none
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "STOP_MUSIC()");
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
	{
		gameMusic->ScriptStopMusic();
	}
}

void AttachMusic() // 046 ATTACH_MUSIC
{
	// the thing is popped first, then the type
	const auto target = MusicThing(Pop().uintVal);
	const auto music = Pop().intVal;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "ATTACH_MUSIC({}, {})", music, target.value_or(0));
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
	{
		gameMusic->ScriptAttachMusic(music, target);
	}
}

void DetachMusic() // 047 DETACH_MUSIC
{
	// the thing's music is removed
	const auto object = MusicThing(Pop().uintVal);
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		gameMusic->RemoveThingMusic(*object);
	}
}

void ObjectDelete() // 048 OBJECT_DELETE
{
	// the mode, then the object; nothing for a thing no longer there
	const auto mode = Pop().intVal;
	const auto object = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(object);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == 0 || !registry.Valid(entity))
	{
		return;
	}
	// the script slots are freed on every path: openblack's go with the entity; a container (town, flock) is disbanded
	// there. (approximate) here before the mode's ToBeDeleted, not after it, because openblack destroys the entity at once
	// (the original only marks it)
	if (ecs::script_containers::IsContainer(entity))
	{
		ecs::script_containers::Disband(object);
	}
	// a puzzle game: removed from the script and reset, whatever the mode; never deleted here
	if (registry.AllOf<ecs::components::PuzzleGame>(entity))
	{
		NotImplemented("ObjectDelete (puzzle game)"); // (pending)
		return;
	}
	const bool creature = registry.AllOf<ecs::components::Creature>(entity);
	switch (mode)
	{
	case 0: // ToBeDeleted (pending: villager::Delete when it exists)
		ecs::ToBeDeleted(entity);
		break;
	case 1: // objects only
		if (creature)
		{
			// the creature fizzles away (1.0, 2.0), not deleted here (pending, creature)
			NotImplemented("ObjectDelete (creature fizz)");
		}
		else
		{
			// a 500 ms ghost of its mesh in the landscape fade list (pending: the ghost is not drawn), then ToBeDeleted
			ecs::ToBeDeleted(entity);
		}
		break;
	case 2: // objects only: the mesh broken up (15.0, 3.0) (pending: not drawn), then ToBeDeleted
		ecs::ToBeDeleted(entity);
		break;
	case 3: // the citadel heart only: broken up (80.0, 3.0) or its destruction sequence
		NotImplemented("ObjectDelete (citadel heart)"); // (pending)
		break;
	default: // past 3: only removed from the script, the thing stays
		break;
	}
}

void FocusFollow() // 049 FOCUS_FOLLOW
{
	// POP the thing (none -> "Thing no longer valid") -> the script camera's focus follows it: the path dropped, and the
	// focus heads for the thing every frame
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::FocusFollow(*thing);
	}
}

void PositionFollow() // 050 POSITION_FOLLOW
{
	// POP the thing -> the camera's position follows it every frame, from the heading and pitch the camera has now, at
	// the thing's viewing distance (its height x 8); with "behind" (on in the script mode) the heading is 0, relative to
	// a wall hugger's angle. The path is kept
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::PositionFollow(*thing);
	}
}

void CallNear() // 051 CALL_NEAR
{
	// pops excludingScripted, the radius, the point (z, y, x), the sub-type, then the type
	const auto excludingScripted = Pop().intVal != 0;
	const auto radius = Popf();
	const auto position = PopVec();
	const auto subtype = static_cast<uint32_t>(Pop().intVal);
	const auto type = Pop().intVal;
	const auto thing = FindForScript(type, subtype, position, radius, excludingScripted);
	Pusho(thing == entt::null ? 0 : static_cast<uint32_t>(thing)); // pushed as an object
}

void SpecialEffectPosition() // 052 SPECIAL_EFFECT_POSITION
{
	// a spot visual with the given duration
	const auto duration = Popf();
	const auto position = PopVec();
	const auto effect = Pop().intVal;
	const auto object = psys::manager::CreateSpotVisual(effect, position, duration, entt::null);
	Pusho(object == entt::null ? 0 : static_cast<uint32_t>(object));
}

void SpecialEffectObject() // 053 SPECIAL_EFFECT_OBJECT
{
	// a spot visual at the object's position when made, with the given duration (it does not follow it); its loss
	// closes the effect
	const auto duration = Popf();
	const auto target = static_cast<entt::entity>(Pop().uintVal);
	const auto effect = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(target) ? registry.TryGet<const Transform>(target) : nullptr;
	entt::entity object = entt::null;
	if (transform != nullptr)
	{
		object = psys::manager::CreateSpotVisual(effect, transform->position, duration, target);
	}
	Pusho(object == entt::null ? 0 : static_cast<uint32_t>(object));
}

void DanceCreate() // 054 DANCE_CREATE
{
	// const auto duration = Popf();
	// const auto position = PopVec();
	// const auto type = Pop().intVal;
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void CallIn() // 055 CALL_IN
{
	// excluding (first pop), the container, the sub-type, the type
	const auto excludingScripted = Pop().intVal != 0;
	const auto container = static_cast<entt::entity>(Pop().uintVal);
	const auto subtype = static_cast<uint32_t>(Pop().intVal);
	const auto type = static_cast<script::ObjectType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity found = entt::null;
	if (container != entt::null && registry.Valid(container))
	{
		found = ecs::script_containers::Find(container, type, subtype, excludingScripted);
	}
	if (found == entt::null)
	{
		// a warning, push 0
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "CALL_IN: Cannot find in");
		Pusho(0);
		return;
	}
	// added as a found script thing
	ecs::script_held::AddScriptThing(found, false);
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "CALL_IN {} in {} -> {}", static_cast<int>(type),
		                   static_cast<uint32_t>(container), static_cast<uint32_t>(found));
	}
	Pusho(static_cast<uint32_t>(found));
}

void ChangeInnerOuterProperties() // 056 CHANGE_INNER_OUTER_PROPERTIES
{
	// calm (first pop), outer, inner, the obj (ECS/ScriptContainers.h)
	const auto calm = Popf();
	const auto outer = Popf();
	const auto inner = Popf();
	const auto object = Pop().uintVal;
	ecs::script_containers::ChangeInnerOuter(object, inner, outer, calm);
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
	NotImplemented(__func__);
}

void GetAlignment() // 058 GET_ALIGNMENT
{
	// the player's alignment value
	const auto player = Pop().intVal;
	Pushf(
	    ecs::effects::alignment::Get(static_cast<PlayerNames>(std::clamp(player, 0, static_cast<int>(PlayerNames::NEUTRAL)))));
}

void SetAlignment() // 059 SET_ALIGNMENT
{
	// the player first, then the value; out of -1..1 it is an error and nothing happens, otherwise AddClamped: the value
	// is ADDED to the player's alignment (clamped), despite the name
	const auto player = Pop().intVal;
	const auto value = Pop().floatVal;
	if (value < -1.0f || value > 1.0f)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_ALIGNMENT: Alignment out of range ({})", value);
		return;
	}
	ecs::effects::alignment::AddClamped(static_cast<PlayerNames>(std::clamp(player, 0, static_cast<int>(PlayerNames::NEUTRAL))),
	                                    value);
}

void InfluenceObject() // 060 INFLUENCE_OBJECT
{
	magic::script::InfluenceObject(); // Magic/Script/CHLInfluence.cpp
}

void InfluencePosition() // 061 INFLUENCE_POSITION
{
	magic::script::InfluencePosition(); // Magic/Script/CHLInfluence.cpp
}

void GetInfluence() // 062 GET_INFLUENCE
{
	magic::script::GetInfluence(); // Magic/Script/CHLInfluence.cpp
}

void SetInterfaceInteraction() // 063 SET_INTERFACE_INTERACTION
{
	// the level (Help/InterfaceInteraction.h)
	help::interface_interaction::Set(Pop().intVal);
}

void Played() // 064 PLAYED
{
	const auto object = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(object);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == 0 || !registry.Valid(entity))
	{
		// "Thing no longer valid" -> 1
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "PLAYED: Thing no longer valid");
		Pushb(true);
		return;
	}
	if (registry.AllOf<openblack::ecs::components::PuzzleGame>(entity))
	{
		// a puzzle game: whether it was played
		Pushb(openblack::ecs::IsPuzzleGamePlayed(entity));
		return;
	}
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		// a villager: whether its script animation is complete
		const bool complete = ecs::villager::IsScriptAnimationComplete(entity);
		if (complete && ScriptThingTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "PLAYED {}: true", object);
		}
		Pushb(complete);
		return;
	}
	if (registry.AnyOf<ecs::components::Animal, ecs::components::Creature>(entity))
	{
		// TODO(Daniels118): a creature -> the creature's plan; any other living thing: its final state == 4 IN_SCRIPT
		NotImplemented(__func__);
		Pushb(false);
		return;
	}
	// a weather thing would answer whether it is done: openblack has no weather things. Anything else: "Thing not
	// living" and 1
	SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "PLAYED: Thing not living");
	Pushb(true);
}

void RandomUlong() // 065 RANDOM_ULONG
{
	// the first POP is max, the second min; a synced GameRand(max - min + 1) (unsigned; 0 for 0) + min, pushed as an int
	const auto max = Pop().uintVal;
	const auto min = Pop().uintVal;
	const uint32_t range = max - min + 1u;
	const uint32_t drawn = game_random::GameRand(range);
	Pushi(static_cast<int32_t>(drawn + min));
}

void SetGamespeed() // 066 SET_GAMESPEED
{
	// Help/ScriptControl.cpp
	const auto speed = Popf();
	help::script_control::SetGameSpeed(help::script_control::GetCameraControl(), ScriptVm(), speed);
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
	NotImplemented(__func__);
	Pusho(0);
}

void OverrideStateAnimation() // 068 OVERRIDE_STATE_ANIMATION
{
	// the clip, then the thing
	const auto clip = Pop().intVal;
	const auto object = Pop().uintVal;
	// clip <= 0 or >= 441: "Invalid animation forced", and it goes on
	if (ecs::living::IsInvalidForcedClip(clip))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "OVERRIDE_STATE_ANIMATION: Invalid animation forced {}", clip);
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !registry.Valid(entity))
	{
		return; // no thing, nothing said
	}
	// the clip's entry in the animation list, entry 0 out of [0, count)
	const int32_t index = ecs::living::ForcedClipIndex(clip);
	// a changed anim is set on the 3D object with its cycle time 0. (not ported) the remind record for the script, read
	// only when a play animation resumes. The clip lasts until the next state animation is set
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		ecs::VillagerSetClip(entity, index, true);
		return;
	}
	if (registry.AllOf<ecs::components::Animal>(entity))
	{
		ecs::SetAnimalAnim(entity, index, true);
		return;
	}
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		NotImplemented("OverrideStateAnimation (creature)"); // TODO: creatures
		return;
	}
	// not a living thing
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "OVERRIDE_STATE_ANIMATION: Thing must be living");
}

void CreatureCreateRelativeToCreature() // 069 CREATURE_CREATE_RELATIVE_TO_CREATURE
{
	// const auto type = Pop().intVal;
	// const auto position = PopVec();
	// const auto scale = Popf();
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void CreatureLearnEverything() // 070 CREATURE_LEARN_EVERYTHING
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureSetKnowsAction() // 071 CREATURE_SET_KNOWS_ACTION
{
	// const auto knows = Pop().intVal;
	// const auto action = Pop().intVal;
	// const auto typeOfAction = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureSetAgendaPriority() // 072 CREATURE_SET_AGENDA_PRIORITY
{
	// const auto priority = Popf();
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureTurnOffAllDesires() // 073 CREATURE_TURN_OFF_ALL_DESIRES
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureLearnDistinctionAboutActivityObject() // 074 CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureDoAction() // 075 CREATURE_DO_ACTION
{
	// const auto withObject = Pop().uintVal;
	// const auto target = Pop().uintVal;
	// const auto unk1 = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void InCreatureHand() // 076 IN_CREATURE_HAND
{
	// const auto creature = Pop().uintVal;
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void CreatureSetDesireValue() // 077 CREATURE_SET_DESIRE_VALUE
{
	// const auto value = Popf();
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureSetDesireActivated78() // 078 CREATURE_SET_DESIRE_ACTIVATED
{
	// const auto active = Pop().intVal;
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureSetDesireActivated79() // 079 CREATURE_SET_DESIRE_ACTIVATED
{
	// const auto active = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureSetDesireMaximum() // 080 CREATURE_SET_DESIRE_MAXIMUM
{
	// const auto value = Popf();
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ConvertCameraPosition() // 081 CONVERT_CAMERA_POSITION
{
	// the camera "Cam%d" of camera.edt, its position
	const auto cameraEnum = Pop().intVal;
	const auto camera = LoadCameraBin(cameraEnum);
	const auto position = camera.has_value() ? camera->position : glm::vec3(0.0f);
	Pushv(position.x);
	Pushv(position.y);
	Pushv(position.z);
}

void ConvertCameraFocus() // 082 CONVERT_CAMERA_FOCUS
{
	// the camera "Cam%d" of camera.edt, its focus
	const auto cameraEnum = Pop().intVal;
	const auto camera = LoadCameraBin(cameraEnum);
	const auto focus = camera.has_value() ? camera->focus : glm::vec3(0.0f);
	Pushv(focus.x);
	Pushv(focus.y);
	Pushv(focus.z);
}

void CreatureSetPlayer() // 083 CREATURE_SET_PLAYER
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StartCountdownTimer() // 084 START_COUNTDOWN_TIMER
{
	// ECS/ScriptTimer.h script_countdown
	const auto timeout = Popf();
	if (!ecs::script_countdown::Start(timeout))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "START_COUNTDOWN_TIMER: Invalid time for timer");
	}
}

void CreatureInitialiseNumTimesPerformedAction() // 085 CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureGetNumTimesActionPerformed() // 086 CREATURE_GET_NUM_TIMES_ACTION_PERFORMED
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void RemoveCountdownTimer() // 087 REMOVE_COUNTDOWN_TIMER
{
	ecs::script_countdown::Remove(); // the countdown timer is cleared
}

void GetObjectDropped() // 088 GET_OBJECT_DROPPED
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void ClearDroppedByObject() // 089 CLEAR_DROPPED_BY_OBJECT
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreateReaction() // 090 CREATE_REACTION
{
	// const auto reaction = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void RemoveReaction() // 091 REMOVE_REACTION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetCountdownTimer() // 092 GET_COUNTDOWN_TIMER
{
	// the remaining whole seconds
	Pushf(ecs::script_countdown::RemainingSeconds());
}

void StartDualCamera() // 093 START_DUAL_CAMERA
{
	// POP b then a; either missing -> "Thing invalid for dual cam" and nothing; else a two-things camera mode on top of
	// whatever mode is current (no mode nor citadel check): the camera looks at the two things' middle from their
	// distance apart (Camera/ScriptCamera.h)
	const auto objectB = Pop().uintVal;
	const auto objectA = Pop().uintVal;
	const auto b = CameraThing(objectB, __func__);
	const auto a = CameraThing(objectA, __func__);
	if (!a.has_value() || !b.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Thing invalid for dual cam", __func__);
		return;
	}
	const auto& camera = Locator::camera::value();
	script_camera::StartDual(*a, *b, camera.GetOriginZoomer(), camera.GetFocusZoomer());
}

void UpdateDualCamera() // 094 UPDATE_DUAL_CAMERA
{
	// POP b then a; either missing -> "Thing invalid for dual cam" and nothing; else when the current mode is a
	// two-things camera its things are set (a point camera becomes a two things one)
	const auto objectB = Pop().uintVal;
	const auto objectA = Pop().uintVal;
	const auto b = CameraThing(objectB, __func__);
	const auto a = CameraThing(objectA, __func__);
	if (!a.has_value() || !b.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Thing invalid for dual cam", __func__);
		return;
	}
	script_camera::UpdateDual(*a, *b);
}

void ReleaseDualCamera() // 095 RELEASE_DUAL_CAMERA
{
	// a current two-things camera mode is deleted and popped: the mode under it (the script mode) moves the camera again,
	// its seconds from 0
	script_camera::ReleaseDual();
}

void SetCreatureHelp() // 096 SET_CREATURE_HELP
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetTargetObject() // 097 GET_TARGET_OBJECT
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void CreatureDesireIs() // 098 CREATURE_DESIRE_IS
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void CountdownTimerExists() // 099 COUNTDOWN_TIMER_EXISTS
{
	Pushb(ecs::script_countdown::Exists()); // whether the countdown timer exists
}

void LookGameThing() // 100 LOOK_GAME_THING
{
	// POP the object, the spirit; no object -> "Object no longer valid"; else the spirit looks at it (the per-turn
	// refresh points, original bug kept)
	const auto target = Pop().uintVal;
	const auto spirit = ScriptSpirit(Pop().intVal);
	const auto object = SpiritThing(target, "LOOK_GAME_THING");
	if (auto* control = SpiritControl(); control != nullptr && object != 0)
	{
		control->SpiritLookObject(spirit, object);
	}
}

void GetObjectDestination() // 101 GET_OBJECT_DESTINATION
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void CreatureForceFinish() // 102 CREATURE_FORCE_FINISH
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void HideCountdownTimer() // 103 HIDE_COUNTDOWN_TIMER
{
	ecs::script_countdown::SetShown(false); // the countdown timer is hidden
}

void GetActionTextForObject() // 104 GET_ACTION_TEXT_FOR_OBJECT
{
	// pops nothing and always pushes the help text 828
	Pushi(828);
}

void CreateDualCameraWithPoint() // 105 CREATE_DUAL_CAMERA_WITH_POINT
{
	// POP the point (z, y, x) then the thing; a thing-and-point camera mode with no check: a thing that is not there
	// leaves a mode that moves nothing (its update would read through null in the original) until the turn's check of
	// the stacked modes drops it
	const auto point = PopVec();
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	const auto& camera = Locator::camera::value();
	script_camera::StartDualWithPoint(thing.value_or(entt::null), point, camera.GetOriginZoomer(), camera.GetFocusZoomer());
}

void SetCameraToFaceObject() // 106 SET_CAMERA_TO_FACE_OBJECT
{
	// POP the distance, then the thing; with the script mode: the focus on the thing (half its height up), the position
	// `distance` away along its facing and 0.1 rad up; then the script camera's position and focus are set (both drop
	// the path and the follows)
	const auto distance = Popf();
	const auto object = Pop().uintVal;
	if (!ScriptCameraMode(__func__))
	{
		return;
	}
	// (inferred) no thing: the original says "no object to face" and then reads through the null pointer; here nothing
	// more is done
	const auto thing = object != 0 ? static_cast<entt::entity>(object) : entt::null;
	if (const auto points = script_camera::FaceObject(thing, distance); points.has_value())
	{
		script_camera::SetPosition(points->position);
		script_camera::SetFocus(points->focus);
	}
}

void MoveCameraToFaceObject() // 107 MOVE_CAMERA_TO_FACE_OBJECT
{
	// POP the time, the distance, then the thing; with the script mode, the same points as 106, the script camera's
	// position and focus moved there in that time (seconds of the wall clock)
	const auto time = Popf();
	const auto distance = Popf();
	const auto object = Pop().uintVal;
	if (!ScriptCameraMode(__func__))
	{
		return;
	}
	const auto thing = object != 0 ? static_cast<entt::entity>(object) : entt::null; // (inferred) as in 106
	if (const auto points = script_camera::FaceObject(thing, distance); points.has_value())
	{
		script_camera::MovePosition(points->position, time);
		script_camera::MoveFocus(points->focus, time);
	}
}

void GetMoonPercentage() // 108 GET_MOON_PERCENTAGE
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void PopulateContainer() // 109 POPULATE_CONTAINER
{
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// const auto quantity = Popf();
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void AddReference() // 110 ADD_REFERENCE
{
	// the script reference is incremented (the original pushes nothing; the binding's one output is kept as the object)
	const auto objId = Pop().uintVal;
	if (objId != 0)
	{
		ecs::script_held::IncrementReference(static_cast<entt::entity>(objId));
	}
	Pusho(objId);
}

void RemoveReference() // 111 REMOVE_REFERENCE
{
	// the script reference is decremented
	const auto objId = Pop().uintVal;
	if (objId != 0)
	{
		ecs::script_held::DecrementReference(static_cast<entt::entity>(objId));
	}
	Pusho(objId);
}

void SetGameTime() // 112 SET_GAME_TIME
{
	// the visual time forced from the script time
	const auto time = Popf();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_GAME_TIME({})", time);
	Game::SetTime(time);
}

void GetGameTime() // 113 GET_GAME_TIME
{
	// the visual time mapped back to script time
	Pushf(Locator::dayNightClock::value().Clock().GetScriptTime());
}

void GetRealTime() // 114 GET_REAL_TIME
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetRealDay115() // 115 GET_REAL_DAY
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetRealDay116() // 116 GET_REAL_DAY
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetRealMonth() // 117 GET_REAL_MONTH
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetRealYear() // 118 GET_REAL_YEAR
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void RunCameraPath() // 119 RUN_CAMERA_PATH
{
	// the script camera runs the path camera.edt "Track%d" (3D/CameraTracks.h)
	const auto path = static_cast<int32_t>(Pop().intVal);
	if (ScriptCameraMode(__func__))
	{
		script_camera::RunPath(path);
	}
}

void StartDialogue() // 120 START_DIALOGUE
{
	// Help/ScriptControl.cpp; the advisors going home are help system hooks (not ported). Without a HelpSystem
	// (openblack only, the original always has one): false
	auto* helpSystem = help::Get();
	Pushb(helpSystem != nullptr && help::script_control::StartDialogue(*helpSystem, ScriptVm()));
}

void EndDialogue() // 121 END_DIALOGUE
{
	// Help/ScriptControl.cpp: only for the task that has the dialogue (nothing without a HelpSystem: openblack only)
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		help::script_control::EndDialogue(*helpSystem, audio::GetScriptAudioState(), ScriptVm());
	}
}

void IsDialogueReady() // 122 IS_DIALOGUE_READY
{
	// whether the dialogue is not controlled, a bool (type 6). Without a HelpSystem (openblack only, no original
	// equivalent): true, nothing controls the dialogue
	const auto* helpSystem = help::Get();
	Pushb(helpSystem == nullptr || help::script_control::IsSpiritReady(*helpSystem));
}

void ChangeWeatherProperties() // 123 CHANGE_WEATHER_PROPERTIES
{
	magic::script::ChangeWeatherProperties(); // Magic/Script/CHLWeather.cpp
}

void ChangeLightningProperties() // 124 CHANGE_LIGHTNING_PROPERTIES
{
	magic::script::ChangeLightningProperties(); // Magic/Script/CHLWeather.cpp
}

void ChangeTimeFadeProperties() // 125 CHANGE_TIME_FADE_PROPERTIES
{
	magic::script::ChangeTimeFadeProperties(); // Magic/Script/CHLWeather.cpp
}

void ChangeCloudProperties() // 126 CHANGE_CLOUD_PROPERTIES
{
	magic::script::ChangeCloudProperties(); // Magic/Script/CHLWeather.cpp
}

void SetHeadingAndSpeed() // 127 SET_HEADING_AND_SPEED
{
	// const auto speed = Popf();
	// const auto position = PopVec();
	// const auto unk0 = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StartGameSpeed() // 128 START_GAME_SPEED
{
	// Help/ScriptControl.cpp
	help::script_control::StartGameSpeed(help::script_control::GetCameraControl(), ScriptVm());
}

void EndGameSpeed() // 129 END_GAME_SPEED
{
	// Help/ScriptControl.cpp
	help::script_control::EndGameSpeed(help::script_control::GetCameraControl(), ScriptVm());
}

void BuildBuilding() // 130 BUILD_BUILDING
{
	// POP desire, then z, y, x (the first popped is the desire); the town forces the building of what it planned at the
	// map coords of the point, with desire x 5.0
	const auto desire = Popf();
	const auto position = PopVec();
	openblack::ecs::building_sites::ForceBuildingOfPlannedAtPos(openblack::map_coords::FromWorld(position), desire * 5.0f);
}

void SetAffectedByWind() // 131 SET_AFFECTED_BY_WIND
{
	// const auto object = Pop().uintVal;
	// const auto enabled = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void WidescreenTransistionFinished() // 132 WIDESCREEN_TRANSISTION_FINISHED
{
	Pushb(Locator::screenFade::value().Fade().IsWideScreenTransitionFinished());
}

void GetResource() // 133 GET_RESOURCE
{
	// const auto container = Pop().uintVal;
	// const auto resource = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void AddResource() // 134 ADD_RESOURCE
{
	// POP the thing, POP the amount (truncated), POP the RESOURCE_TYPE (raw). No thing -> "No thing for resource", not
	// an object -> "Not object for resource", both push 0; else the object's AddResource(type, amount, not IS, not
	// poisoned, no pos) and PUSH what it took as a float. (pending) a villager's own resources in object_resources
	// (Land 1's builders, L52628..52703)
	const auto object = Pop().uintVal;
	const auto amount = static_cast<uint32_t>(openblack::map_coords::FtoL(Popf()));
	const auto type = static_cast<ResourceType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "ADD_RESOURCE: No thing for resource");
		Pushf(0.0f);
		return;
	}
	// (approximate) every entity but a town is an object here (the towns are things with a position, not objects)
	if (registry.AllOf<openblack::ecs::components::Town>(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "ADD_RESOURCE: Not object for resource");
		Pushf(0.0f);
		return;
	}
	const auto added = openblack::ecs::object_resources::AddResource(entity, type, amount);
	Pushf(static_cast<float>(added));
}

void RemoveResource() // 135 REMOVE_RESOURCE
{
	// const auto container = Pop().uintVal;
	// const auto quantity = Popf();
	// const auto resource = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetTargetRelativePos() // 136 GET_TARGET_RELATIVE_POS
{
	// const auto angle = Popf();
	// const auto distance = Popf();
	// const auto to = PopVec();
	// const auto from = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void StopPointing() // 137 STOP_POINTING
{
	// POP the spirit -> it stops pointing
	const auto spirit = ScriptSpirit(Pop().intVal);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritStopPointing(spirit);
	}
}

void StopLooking() // 138 STOP_LOOKING
{
	// POP the spirit -> it stops looking
	const auto spirit = ScriptSpirit(Pop().intVal);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritStopLooking(spirit);
	}
}

void LookAtPosition() // 139 LOOK_AT_POSITION
{
	// POP the position, the spirit -> it looks at the position
	const auto position = PopVec();
	const auto spirit = ScriptSpirit(Pop().intVal);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritLookAtPosition(spirit, position);
	}
}

void PlaySpiritAnim() // 140 PLAY_SPIRIT_ANIM
{
	// POP the time, the anim (raw), y, x, the spirit; anim outside 0..80 -> "Invalid enum", then "Invalid Y" /
	// "Invalid X", none of them stops it; then the spirit plays the anim at (x, y) for that time
	const auto time = Popf();
	const auto anim = Pop().intVal;
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptSpirit(Pop().intVal);
	if (anim < 0 || anim > static_cast<int32_t>(help::spirits::anim::k_Last))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "PLAY_SPIRIT_ANIM: Invalid enum");
	}
	CheckScreenXY("PLAY_SPIRIT_ANIM", x, y);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritPlayAnim(spirit, x, y, static_cast<uint32_t>(anim), time);
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
	NotImplemented(__func__);
	Pusho(0);
}

void SetCameraZone() // 142 SET_CAMERA_ZONE
{
	// POP the string; the camera exclusion file reset, then ".\Data\Zones\%s" loaded and the force field on
	// (Camera/PlayerCameraScript.h). It limits the player's camera, which openblack does not read yet; the script camera
	// is not affected
	const auto zone = PopString();
	player_camera::SetCameraZone(zone);
}

void GetObjectState() // 143 GET_OBJECT_STATE
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void RevealCountdownTimer() // 144 REVEAL_COUNTDOWN_TIMER
{
	ecs::script_countdown::SetShown(true); // the countdown timer is shown
}

void SetTimerTime() // 145 SET_TIMER_TIME
{
	// POP the time, then the thing (none -> "Object no longer valid"). A script timer starts again from this turn; a
	// spell dispenser takes it as its period; anything else is "Invalid script thing"
	const auto time = Popf();
	const auto timer = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(timer);
	if (timer == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_TIMER_TIME: Object no longer valid");
		return;
	}
	if (!ecs::script_timer::SetTime(entity, time) && !magic::script::SetDispenserTimerTime(entity, time))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_TIMER_TIME: Invalid script thing");
	}
}

void CreateTimer() // 146 CREATE_TIMER
{
	// POP the seconds; a script timer added as a thing the script created and PUSHed (0 and "Thing not created" when
	// nothing was made). (not ported) the debug name of the script that created it
	const auto timeout = Popf();
	const auto timer = ecs::script_timer::Create(timeout);
	if (timer == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATE_TIMER: Thing not created");
		Pusho(0);
		return;
	}
	ecs::script_held::AddScriptThing(timer, true);
	Pusho(static_cast<uint32_t>(timer));
}

void GetTimerTimeRemaining() // 147 GET_TIMER_TIME_REMAINING
{
	// none -> "Object no longer valid" and 0.0; not a timer -> "Invalid script thing" and 0.0; else the seconds left of
	// the game turns, 0 once out
	const auto timer = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(timer);
	if (timer == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TIMER_TIME_REMAINING: Object no longer valid");
		Pushf(0.0f);
		return;
	}
	const auto remaining = ecs::script_timer::Remaining(entity);
	if (!remaining)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TIMER_TIME_REMAINING: Invalid script thing");
	}
	Pushf(remaining.value_or(0.0f));
}

void GetTimerTimeSinceSet() // 148 GET_TIMER_TIME_SINCE_SET
{
	// none or not a timer -> the error and FLT_MAX; else the seconds since it was set
	const auto timer = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(timer);
	if (timer == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TIMER_TIME_SINCE_SET: Object no longer valid");
		Pushf(std::numeric_limits<float>::max());
		return;
	}
	const auto since = ecs::script_timer::SinceSet(entity);
	if (!since)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TIMER_TIME_SINCE_SET: Invalid script thing");
	}
	Pushf(since.value_or(std::numeric_limits<float>::max()));
}

void MoveMusic() // 149 MOVE_MUSIC
{
	// "to" is popped first, then "from"; both must be valid
	const auto to = MusicThing(Pop().uintVal);
	const auto from = MusicThing(Pop().uintVal);
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && from && to)
	{
		gameMusic->MoveThingMusic(*from, *to);
	}
}

void GetInclusionDistance() // 150 GET_INCLUSION_DISTANCE
{
	// the camera's inclusion distance as a float. (approximate) the player camera's update, which writes it, is not
	// ported: it stays at its start value FLT_MAX
	Pushf(player_camera::Get().inclusionDistance);
}

void GetLandHeight() // 151 GET_LAND_HEIGHT
{
	const auto position = PopVec();

	// -10 over the sea (altitude 0), off the map or without a block
	const auto& island = Locator::terrainSystem::value();
	Pushf(openblack::ecs::sea_cells::ScriptLandHeight(island, position));
}

void LoadMap() // 152 LOAD_MAP
{
	// const auto path = PopString();

	// auto& fileSystem = Locator::filesystem::value();
	// auto mapPath = fileSystem.GetGamePath() / path;
	// TODO(Daniels118): LoadMap(mapPath);
}

void StopAllScriptsExcluding() // 153 STOP_ALL_SCRIPTS_EXCLUDING
{
	const auto scriptNames = PopString();

	const auto names = GetUniqueWords(scriptNames);
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&names](const std::string& name, [[maybe_unused]] const std::string& filename) -> bool {
		return !names.contains(name);
	});
}

void StopAllScriptsInFilesExcluding() // 154 STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING
{
	const auto sourceFilenames = PopString();

	const auto filenames = GetUniqueWords(sourceFilenames);
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&filenames]([[maybe_unused]] const std::string& name, const std::string& filename) -> bool {
		return !filenames.contains(filename);
	});
}

void StopScript() // 155 STOP_SCRIPT
{
	const auto scriptName = PopString();
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&scriptName](const std::string& name, [[maybe_unused]] const std::string& filename) -> bool {
		return name == scriptName;
	});
}

void ClearClickedObject() // 156 CLEAR_CLICKED_OBJECT
{
	// the clicked object is cleared
	Locator::handSystem::value().ClearClicked();
}

void ClearClickedPosition() // 157 CLEAR_CLICKED_POSITION
{
	// the clicked position is cleared (its turn stays)
	Locator::handSystem::value().ClearClickedPosition();
}

void PositionClicked() // 158 POSITION_CLICKED
{
	// the radius, then the position; (pending) a multiplayer game logs "This is not multiplayer friendly yet!" and
	// pushes true. Then whether the last clicked land point is within the radius
	const auto radius = Popf();
	const auto position = PopVec();
	Pushb(Locator::handSystem::value().PositionClicked(position, radius));
}

void ReleaseFromScript() // 159 RELEASE_FROM_SCRIPT
{
	// none -> "Thing not valid". Controlled by a script -> released into the game: no longer controlled by the script,
	// never deleted, its music removed, then by SCRIPT_OBJECT_TYPE (37: nothing more)
	const auto object = Pop().uintVal;
	const auto thing = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "RELEASE_FROM_SCRIPT: Thing not valid");
		return;
	}
	// (a highlight: DidYouKnow (CHL L2610..) releases its sign at once and it stays when the task ends; the containers'
	// members, the villagers and the animals by their types: ECS/ScriptHeld.h)
	ecs::script_held::ReleaseFromScript(thing);
}

void GetObjectHandIsOver() // 160 GET_OBJECT_HAND_IS_OVER
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void IdPoisonedSize() // 161 ID_POISONED_SIZE
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void IsPoisoned() // 162 IS_POISONED
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void CallPoisonedIn() // 163 CALL_POISONED_IN
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void CallNotPoisonedIn() // 164 CALL_NOT_POISONED_IN
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void SpiritPlayed() // 165 SPIRIT_PLAYED
{
	// POP the spirit; push whether it is not playing an anim, a bool (type 6). Without the control (openblack only):
	// true, nothing plays
	const auto spirit = ScriptSpirit(Pop().intVal);
	const auto* control = SpiritControl();
	Pushb(control == nullptr || !control->SpiritPlayingAnim(spirit));
}

void ClingSpirit() // 166 CLING_SPIRIT
{
	// POP y, x, the spirit; "Invalid Y" / "Invalid X" (not stopping); the spirit clings at (x, y)
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptSpirit(Pop().intVal);
	CheckScreenXY("CLING_SPIRIT", x, y);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritCling(spirit, x, y);
	}
}

void FlySpirit() // 167 FLY_SPIRIT
{
	// POP y, x, the spirit; "Invalid Y" / "Invalid X" (not stopping); the spirit flies to (x, y)
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptSpirit(Pop().intVal);
	CheckScreenXY("FLY_SPIRIT", x, y);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritFly(spirit, x, y);
	}
}

void SetIdMoveable() // 168 SET_ID_MOVEABLE
{
	// the object, then the bool; no object: "Thing not valid"; else its not-moveable flag = !moveable
	const auto object = Pop().uintVal;
	const auto moveable = Pop().intVal != 0;
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_ID_MOVEABLE: Thing not valid");
		return;
	}
	ecs::object_flags::SetMoveable(entity, moveable);
}

void SetIdPickupable() // 169 SET_ID_PICKUPABLE
{
	// as 168, with its not-pickupable flag = !pickupable
	const auto object = Pop().uintVal;
	const auto pickupable = Pop().intVal != 0;
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_ID_PICKUPABLE: Thing not valid");
		return;
	}
	ecs::object_flags::SetPickupable(entity, pickupable);
}

void IsOnFire() // 170 IS_ON_FIRE
{
	magic::script::IsOnFire(); // Magic/Script/CHLFire.cpp
}

void IsFireNear() // 171 IS_FIRE_NEAR
{
	magic::script::IsFireNear(); // Magic/Script/CHLFire.cpp
}

void StopScriptsInFiles() // 172 STOP_SCRIPTS_IN_FILES
{
	const auto sourceFilenames = PopString();

	const auto filenames = GetUniqueWords(sourceFilenames);
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&filenames]([[maybe_unused]] const std::string& name, const std::string& filename) -> bool {
		return filenames.contains(filename);
	});
}

void SetPoisoned() // 173 SET_POISONED
{
	// const auto obj = Pop().uintVal;
	// const auto poisoned = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetTemperature() // 174 SET_TEMPERATURE
{
	magic::script::SetTemperature(); // Magic/Script/CHLFire.cpp
}

void SetOnFire() // 175 SET_ON_FIRE
{
	magic::script::SetOnFire(); // Magic/Script/CHLFire.cpp
}

void SetTarget() // 176 SET_TARGET
{
	// const auto time = Popf();
	// const auto position = PopVec();
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void WalkPath() // 177 WALK_PATH
{
	const auto valTo = Popf();
	const auto valFrom = Popf();
	const auto cameraEnum = Pop().intVal;
	const auto forward = Pop().intVal != 0;
	const auto object = static_cast<entt::entity>(Pop().uintVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || !registry.AllOf<ecs::components::Transform>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "WALK_PATH: Thing not valid");
		return;
	}
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		// a living thing -> it walks the track in IN_SCRIPT 4 from, to, forward (ECS/LivingWalkPath.h)
		const bool started = ecs::living::StartWalkPath(object, cameraEnum, VillagerStates::InScript, valFrom, valTo, forward);
		if (ScriptThingTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "WALK_PATH {} track {} from {} to {} forward {} -> {}",
			                   static_cast<uint32_t>(object), cameraEnum, valFrom, valTo, forward, started);
		}
		return;
	}
	if (registry.AnyOf<ecs::components::Animal, ecs::components::Creature>(object))
	{
		// (pending) an animal's or a creature's state 28 (the same living walk)
		NotImplemented(__func__);
		return;
	}
	if (!registry.AnyOf<ecs::components::MobileObject, ecs::components::Shark>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "WALK_PATH: Thing is invalid for move path");
		return;
	}
	ecs::StartMobileWalkPath(object, cameraEnum, forward, valFrom, valTo);
}

void FocusAndPositionFollow() // 178 FOCUS_AND_POSITION_FOLLOW
{
	// POP the distance, then the thing: as POSITION_FOLLOW with that distance and the heading kept (no "behind" reset);
	// the focus follows the same thing unless FOCUS_FOLLOW gave another
	const auto distance = Popf();
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::FocusAndPositionFollow(*thing, distance);
	}
}

void GetWalkPathPercentage() // 179 GET_WALK_PATH_PERCENTAGE
{
	// 1.0 for anything but a living thing (a shark's path is not read)
	const auto object = static_cast<entt::entity>(Pop().uintVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(object) &&
	    registry.AnyOf<ecs::components::Villager, ecs::components::Animal, ecs::components::Creature>(object))
	{
		// the walk's current / duration (ECS/LivingWalkPath.h). (openblack) 0 without a path: the original reads through
		// null
		Pushf(ecs::living::GetWalkPathPercentage(object).value_or(0.0f));
		return;
	}
	Pushf(1.0f);
}

void CameraProperties() // 180 CAMERA_PROPERTIES
{
	// POP behind (raw, tested != 0), the angle (degrees, to radians), the speed, the distance; with the script mode the
	// follow's distance, speed (its time factor: 0 places at once), behind and angle are set
	const bool behind = Pop().uintVal != 0;
	const auto angle = Popf() * script_camera::k_DegreesToRadians;
	const auto speed = Popf();
	const auto distance = Popf();
	if (ScriptCameraMode(__func__))
	{
		script_camera::SetFollowProperties(distance, speed, angle, behind);
	}
}

void EnableDisableMusic() // 181 ENABLE_DISABLE_MUSIC
{
	// the thing is popped first, then the switch (stored as it is)
	const auto object = MusicThing(Pop().uintVal);
	const auto enable = Pop().intVal;
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		gameMusic->EnableThingMusic(*object, enable);
	}
}

void GetMusicObjDistance() // 182 GET_MUSIC_OBJ_DISTANCE
{
	// 0 for an invalid thing, else its music's distance
	const auto source = MusicThing(Pop().uintVal);
	float distance = 0.0f;
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && source)
	{
		distance = gameMusic->ThingMusicDistance(*source);
	}
	Pushf(distance);
}

void GetMusicEnumDistance() // 183 GET_MUSIC_ENUM_DISTANCE
{
	// with an invalid type the original pushes twice (0, then the play distance)
	const auto type = Pop().intVal;
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
	{
		for (const float value : gameMusic->ScriptMusicTypeDistances(type))
		{
			Pushf(value);
		}
	}
	else
	{
		Pushf(0.0f); // (not in the original: no GameMusic before game_music::Start; one value keeps the stack)
	}
}

void SetMusicPlayPosition() // 184 SET_MUSIC_PLAY_POSITION
{
	// z, y and x are popped first, then the thing
	const auto position = PopVec();
	const auto object = MusicThing(Pop().uintVal);
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		gameMusic->SetPlayPosition(*object, position);
	}
}

void AttachObjectLeashToObject() // 185 ATTACH_OBJECT_LEASH_TO_OBJECT
{
	// two pops: the thing to tie to, then the creature; either order works (Creature/LeashScript.h)
	const auto thing = Pop().uintVal;
	const auto creature = Pop().uintVal;
	if (Locator::leashSystem::has_value())
	{
		creature_leash::script::AttachToThing(Locator::leashSystem::value(), LeashThing(creature), LeashThing(thing),
		                                      IsLeashCreature);
	}
}

void AttachObjectLeashToHand() // 186 ATTACH_OBJECT_LEASH_TO_HAND
{
	const auto creature = Pop().uintVal;
	if (Locator::leashSystem::has_value())
	{
		creature_leash::script::AttachToHand(Locator::leashSystem::value(), LeashCreature(creature));
	}
}

void DetachObjectLeash() // 187 DETACH_OBJECT_LEASH
{
	const auto creature = Pop().uintVal;
	if (Locator::leashSystem::has_value())
	{
		creature_leash::script::Detach(Locator::leashSystem::value(), LeashCreature(creature));
	}
}

void SetCreatureOnlyDesire() // 188 SET_CREATURE_ONLY_DESIRE
{
	// const auto value = Popf();
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetCreatureOnlyDesireOff() // 189 SET_CREATURE_ONLY_DESIRE_OFF
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void RestartMusic() // 190 RESTART_MUSIC
{
	// the thing's music is restarted
	const auto object = MusicThing(Pop().uintVal);
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		gameMusic->RestartThingMusic(*object);
	}
}

void MusicPlayed191() // 191 MUSIC_PLAYED
{
	// true for an invalid thing, else whether its music is finished (a bool, type 6)
	const auto object = MusicThing(Pop().uintVal);
	bool finished = true;
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		finished = gameMusic->IsThingMusicFinished(*object) != 0;
	}
	Pushb(finished);
}

void IsOfType() // 192 IS_OF_TYPE
{
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void ClearHitObject() // 193 CLEAR_HIT_OBJECT
{
	// the scripts' hit object and what hit it are forgotten
	ecs::script_hit::SetHitObject(entt::null, entt::null);
}

void GameThingHit() // 194 GAME_THING_HIT
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SpellAtThing() // 195 SPELL_AT_THING
{
	magic::script::SpellAtThing(); // Magic/Script/CHLSpells.cpp
}

void SpellAtPos() // 196 SPELL_AT_POS
{
	magic::script::SpellAtPos(); // Magic/Script/CHLSpells.cpp
}

void CallPlayerCreature() // 197 CALL_PLAYER_CREATURE
{
	// the creature the player leads, handed to the script as a found thing; none is 0
	const auto scriptPlayer = Popf();
	PlayerNames player {};
	std::optional<entt::entity> creature;
	if (Locator::leashSystem::has_value() && magic::ScriptPlayerToGamePlayer(map_coords::FtoL(scriptPlayer), player))
	{
		creature = ecs::player_creature::PlayersCreature(Locator::leashSystem::value(), player);
	}
	if (!creature.has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "CALL_PLAYER_CREATURE: no creature of player {}", scriptPlayer);
		Pusho(0);
		return;
	}
	ecs::script_held::AddScriptThing(*creature, false);
	Pusho(static_cast<uint32_t>(*creature));
}

void GetSlowestSpeed() // 198 GET_SLOWEST_SPEED
{
	// const auto flock = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetObjectHeld199() // 199 GET_OBJECT_HELD
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void HelpSystemOn() // 200 HELP_SYSTEM_ON
{
	// whether the help system is on, a bool (type 6). Without a HelpSystem (openblack only: the original always has
	// one): false
	const auto* helpSystem = help::Get();
	Pushb(helpSystem != nullptr && helpSystem->IsHelpSystemOn());
}

void ShakeCamera() // 201 SHAKE_CAMERA
{
	// POP the seconds, the amplitude, the radius and the point (z, y, x) -> a camera shake: the drawn camera shakes
	// while it is within the radius of the point, less and less (Camera/CameraShake.h)
	const auto seconds = Popf();
	const auto amplitude = Popf();
	const auto radius = Popf();
	const auto position = PopVec();
	camera_shake::StartCameraShake(position, radius, amplitude, seconds);
}

void SetAnimationModify() // 202 SET_ANIMATION_MODIFY
{
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetAviSequence() // 203 SET_AVI_SEQUENCE
{
	// POP the sequence (raw, compared with 1 and 2), then on (raw, != 0) -> the film started or stopped. It returns at
	// once
	const auto sequence = Pop().intVal;
	const bool on = Pop().uintVal != 0;
	if (!on)
	{
		// stop: 2 -> the falling spell video ends; anything else nothing
		if (sequence == 2)
		{
			video::GetFallingSpell().End(); // nothing without FallingSpellVideo
		}
		return;
	}
	if (sequence == 1)
	{
		// start 1: "data\intro.bik" full screen (the tip video cleared, the game paused and its pause kept, the wide screen
		// on; the player object is made even when the file does not open), then the film's first 60 s: the pause given back
		// after 58 s and the picture faded out up to 60 s; and the black fade gone at once.
		// The film is played by video:: (Video/VideoPlayer.h); the player object exists even when the file does not open,
		// so the fade is always cleared
		auto& player = video::Get();
		const auto& fileSystem = Locator::filesystem::value();
		player.Play(fileSystem.FindPath("Data/intro.bik"));
		player.ScheduleIntro();
		Locator::screenFade::value().Fade().FadeBackToNormal(0.0f);
		return;
	}
	if (sequence == 2)
	{
		// start 2: the falling spell video (Video/FallingSpellVideo.h: nothing without the local player's creature; else mode
		// 2, "data\spells\fall\fall.bik" full screen with no fade of its own, the falling spell updated each frame) and the
		// black fade gone at once, also without a creature
		video::GetFallingSpell().KickOff();
		Locator::screenFade::value().Fade().FadeBackToNormal(0.0f);
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
	NotImplemented(__func__);
}

void DevFunction() // 205 DEV_FUNCTION
{
	const auto function = Pop().intVal;
	if (!Locator::leashSystem::has_value() ||
	    !ecs::player_creature::DevFunction(Locator::leashSystem::value(), function, PlayerNames::PLAYER_ONE))
	{
		NotImplemented(__func__);
	}
}

void HasMouseWheel() // 206 HAS_MOUSE_WHEEL
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void NumMouseButtons() // 207 NUM_MOUSE_BUTTONS
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void SetCreatureDevStage() // 208 SET_CREATURE_DEV_STAGE
{
	const auto stage = Pop().intVal;
	const auto object = Pop().uintVal;
	if (const auto thing = ScriptThing(object, "SET_CREATURE_DEV_STAGE", "Thing not found!"))
	{
		ecs::player_creature::SetDevelopmentStage(Locator::entitiesRegistry::value(), *thing, stage);
	}
}

void SetFixedCamRotation() // 209 SET_FIXED_CAM_ROTATION
{
	// POP the point (z, y, x) then "on"; only with the player's camera mode current ("Wrong camera mode" otherwise): the
	// player's camera then turns about that point, or no longer (Camera/PlayerCameraScript.h; not read by
	// DefaultWorldCameraModel yet)
	const auto point = PopVec();
	const bool on = Pop().uintVal != 0;
	if (script_camera::HasMode())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Wrong camera mode", __func__);
		return;
	}
	player_camera::ForceRotateAboutPoint(on ? std::optional<glm::vec3>(point) : std::nullopt);
}

void SwapCreature() // 210 SWAP_CREATURE
{
	// const auto toCreature = Pop().uintVal;
	// const auto fromCreature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetArena() // 211 GET_ARENA
{
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetFootballPitch() // 212 GET_FOOTBALL_PITCH
{
	// const auto town = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void StopAllGames() // 213 STOP_ALL_GAMES
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void AttachToGame() // 214 ATTACH_TO_GAME
{
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void DetachFromGame() // 215 DETACH_FROM_GAME
{
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void DetachUndefinedFromGame() // 216 DETACH_UNDEFINED_FROM_GAME
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetOnlyForScripts() // 217 SET_ONLY_FOR_SCRIPTS
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StartMatchWithReferee() // 218 START_MATCH_WITH_REFEREE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GameTeamSize() // 219 GAME_TEAM_SIZE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GameType() // 220 GAME_TYPE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void GameSubType() // 221 GAME_SUB_TYPE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void IsLeashed() // 222 IS_LEASHED
{
	const auto object = Pop().uintVal;
	Pushb(Locator::leashSystem::has_value() &&
	      creature_leash::script::IsLeashed(Locator::leashSystem::value(), LeashCreature(object)));
}

void SetCreatureHome() // 223 SET_CREATURE_HOME
{
	const auto position = PopVec();
	const auto object = Pop().uintVal;
	const auto thing = ScriptThing(object, "SET_CREATURE_HOME", "Thing not found!");
	if (thing.has_value() && Locator::leashSystem::has_value())
	{
		ecs::player_creature::SetHome(Locator::leashSystem::value(), Locator::entitiesRegistry::value(), *thing,
		                              OnGround(ecs::player_creature::HomeOnGround(position, 0.0f)));
	}
}

void GetHitObject() // 224 GET_HIT_OBJECT
{
	// the last thing a body in the physics found itself struck by another, handed to the script as a found thing; 0
	// for none
	const auto hit = ecs::script_hit::HitObject();
	if (hit == entt::null)
	{
		Pusho(0);
		return;
	}
	ecs::script_held::AddScriptThing(hit, false);
	Pusho(static_cast<uint32_t>(hit));
}

void GetObjectWhichHit() // 225 GET_OBJECT_WHICH_HIT
{
	// what struck it, handed to the script as a found thing; 0 for none
	const auto hitter = ecs::script_hit::ObjectWhichHit();
	if (hitter == entt::null)
	{
		Pusho(0);
		return;
	}
	ecs::script_held::AddScriptThing(hitter, false);
	Pusho(static_cast<uint32_t>(hitter));
}

void GetNearestTownOfPlayer() // 226 GET_NEAREST_TOWN_OF_PLAYER
{
	// five POPs: the radius, the player (a float, truncated), then the position's z, y and x; the script player to the
	// game's player, the map coords of the point, and the nearest of only that player's towns within r. None: "Did not
	// find town" and 0; else the town, added as a found script thing
	const auto radius = Popf();
	const auto scriptPlayer = Popf();
	const auto position = PopVec();
	PlayerNames player = PlayerNames::NEUTRAL;
	// (openblack) a player out of 0..7 has no player (the original reads through null): no town. Map coords x, z only
	// (FromMetres: the altitude is not read)
	const auto town =
	    magic::ScriptPlayerToGamePlayer(map_coords::FtoL(scriptPlayer), player)
	        ? ecs::map_cells::FindPlayerTownAtPos(map_coords::FromMetres(glm::vec2(position.x, position.z)), radius, player)
	        : entt::entity {entt::null};
	if (town == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_NEAREST_TOWN_OF_PLAYER: Did not find town");
		Pusho(0);
		return;
	}
	ecs::script_held::AddScriptThing(town, false);
	Pusho(static_cast<uint32_t>(town));
}

void SpellAtPoint() // 227 SPELL_AT_POINT
{
	magic::script::SpellAtPoint(); // Magic/Script/CHLSpells.cpp
}

void SetAttackOwnTown() // 228 SET_ATTACK_OWN_TOWN
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void IsFighting() // 229 IS_FIGHTING
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetMagicRadius() // 230 SET_MAGIC_RADIUS
{
	// const auto radius = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void TempTextWithNumber() // 231 TEMP_TEXT_WITH_NUMBER
{
	const auto withInteraction = Pop().intVal;
	const auto value = Popf();
	const auto format = PopString();
	const auto singleLine = static_cast<bool>(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->TempTextWithNumber(singleLine, WidenScriptString(format), value, withInteraction);
	}
}

void RunTextWithNumber() // 232 RUN_TEXT_WITH_NUMBER
{
	const auto withInteraction = Pop().intVal;
	const auto number = Popf();
	const auto textID = static_cast<uint32_t>(Pop().intVal);
	const auto singleLine = static_cast<bool>(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->RunTextWithNumber(singleLine, textID, number, withInteraction);
	}
}

void CreatureSpellReversion() // 233 CREATURE_SPELL_REVERSION
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetDesire() // 234 GET_DESIRE
{
	// ecs::town_desire::ScriptGetDesire: POP the desire; out of [0, 17) -> "Invalid desire" and PUSH 0 without the
	// second POP (literal: the object stays on the stack); else POP the object, PUSH the town's raw desire (0 if it is
	// not a town)
	const auto desire = Pop().intVal;
	std::vector<std::string> errors;
	const float value =
	    ecs::town_desire::ScriptGetDesire(desire, [] { return static_cast<entt::entity>(Pop().uintVal); }, &errors);
	for (const auto& error : errors)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_DESIRE: {}", error);
	}
	Pushf(value);
}

void GetEventsPerSecond() // 235 GET_EVENTS_PER_SECOND
{
	// POP the HELP_EVENT_TYPE (1..48, else "Invalid event" and 0.0); the triggers per second of its help accumulator
	// (Help/HelpProfile.h)
	const auto type = Pop().intVal;
	const auto value = help_profile::EventsPerSecond(type);
	if (!value)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_EVENTS_PER_SECOND: Invalid event {}", type);
	}
	Pushf(value.value_or(0.0f));
}

void GetTimeSince() // 236 GET_TIME_SINCE
{
	// as 235, the time since the event was last used
	const auto type = Pop().intVal;
	const auto value = help_profile::TimeSince(type);
	if (!value)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TIME_SINCE: Invalid event {}", type);
	}
	Pushf(value.value_or(0.0f));
}

void GetTotalEvents() // 237 GET_TOTAL_EVENTS
{
	// as 235, the accumulator's count: the turns in which the event happened (TeachRotate 25, TeachPitch 28, TeachZoom /
	// TrackZoomUsage 29, DoubleClicking 30 / 31)
	const auto type = Pop().intVal;
	const auto value = help_profile::TotalEvents(type);
	if (!value)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TOTAL_EVENTS: Invalid event {}", type);
	}
	Pushf(value.value_or(0.0f));
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
	NotImplemented(__func__);
}

void CreateReward() // 239 CREATE_REWARD
{
	// const auto fromSky = static_cast<bool>(Pop().intVal);
	// const auto position = PopVec();
	// const auto reward = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void CreateRewardInTown() // 240 CREATE_REWARD_IN_TOWN
{
	// const auto fromSky = static_cast<bool>(Pop().intVal);
	// const auto position = PopVec();
	// const auto town = Pop().uintVal;
	// const auto reward = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void SetFade() // 241 SET_FADE
{
	// a screen fade to the colour (every argument truncated; colour 0..255, time in seconds)
	const auto time = Popf();
	const auto blue = Popf();
	const auto green = Popf();
	const auto red = Popf();
	const auto channel = [](float value) { return static_cast<uint8_t>(static_cast<int>(value)); };
	Locator::screenFade::value().Fade().FadeTo(channel(red), channel(green), channel(blue), time);
}

void SetFadeIn() // 242 SET_FADE_IN
{
	// the screen fades back to normal
	const auto duration = Popf();
	Locator::screenFade::value().Fade().FadeBackToNormal(duration);
}

void FadeFinished() // 243 FADE_FINISHED
{
	// no fade in progress
	Pushb(Locator::screenFade::value().Fade().IsFinished());
}

void SetPlayerMagic() // 244 SET_PLAYER_MAGIC
{
	magic::script::SetPlayerMagic(); // Magic/Script/CHLSpells.cpp
}

void HasPlayerMagic() // 245 HAS_PLAYER_MAGIC
{
	magic::script::HasPlayerMagic(); // Magic/Script/CHLSpells.cpp
}

void SpiritSpeaks() // 246 SPIRIT_SPEAKS
{
	// POP the text, then the SCRIPT_SPIRIT_TYPE; the help spirit of it (the local player's alignment: inferred,
	// openblack's local player is PLAYER_ONE; the random draw uses game_random's local stream); text 0 past 6974; push
	// whether the spirit who talks the text is this one (type 6)
	auto text = static_cast<uint32_t>(Pop().intVal);
	const auto spirit = ScriptSpirit(Pop().intVal);
	if (text >= helptext::k_TextCount)
	{
		text = 0;
	}
	Pushb(help::SpiritWhoTalks(helptext::GetEntry(text).narrator) == spirit);
}

void BeliefForPlayer() // 247 BELIEF_FOR_PLAYER
{
	// const auto player = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetHelp() // 248 GET_HELP
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void SetLeashWorks() // 249 SET_LEASH_WORKS
{
	// two pops: the creature, then the value, taken as it is (set when not zero)
	const auto creature = Pop().uintVal;
	const auto value = Pop().intVal;
	if (Locator::leashSystem::has_value())
	{
		creature_leash::script::SetWorks(Locator::leashSystem::value(), LeashCreature(creature), value);
	}
}

void LoadMyCreature() // 250 LOAD_MY_CREATURE
{
	// the profile's creature, at the map cell of the point (its height is not used); nothing when the player has one
	const auto position = PopVec();
	ecs::player_creature::LoadMyCreature(glm::vec2(position.x, position.z));
}

void ObjectRelativeBelief() // 251 OBJECT_RELATIVE_BELIEF
{
	// const auto belief = Popf();
	// const auto player = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreateWithAngleAndScale() // 252 CREATE_WITH_ANGLE_AND_SCALE
{
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = static_cast<ObjectType>(Pop().intVal);
	const auto scale = Popf();
	const auto angle = Popf();

	// the angle is in degrees
	const entt::entity object = type > ObjectType::None && type <= ObjectType::AnimatedStatic
	                                ? CreateScriptObject(type, subtype, position, glm::radians(angle), scale)
	                                : entt::null;
	// added as a thing the script created
	ecs::script_held::AddScriptThing(object, true);

	Pusho(object == entt::null ? 0 : static_cast<uint32_t>(object));
}

void SetHelpSystem() // 253 SET_HELP_SYSTEM
{
	// the help system's on flag = the popped value as it is
	const auto on = Pop().intVal;
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->SetHelpOn(static_cast<uint32_t>(on));
	}
}

void SetVirtualInfluence() // 254 SET_VIRTUAL_INFLUENCE
{
	// const auto player = Popf();
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetActive() // 255 SET_ACTIVE
{
	const auto object = Pop().uintVal;
	const auto active = static_cast<bool>(Pop().intVal);
	// only the spell dispensers are ported (Magic/Script/CHLWorship.cpp)
	// a script highlight first -> activated or not
	if (object != 0 && ecs::script_highlight::IsHighlight(static_cast<entt::entity>(object)))
	{
		ecs::script_highlight::SetActivated(static_cast<entt::entity>(object), active);
		return;
	}
	if (object != 0 && magic::script::SetDispenserActive(static_cast<entt::entity>(object), active))
	{
		return;
	}
	// a scaffold: active -> its building is forced; inactive: nothing
	if (object != 0 && ecs::scaffolds::IsScaffold(static_cast<entt::entity>(object)))
	{
		if (active)
		{
			ecs::scaffolds::ForceBuildBuilding(static_cast<entt::entity>(object), std::nullopt);
		}
		return;
	}
	NotImplemented(__func__);
}

void ThingValid() // 256 THING_VALID
{
	const auto objId = Pop().uintVal;
	// TODO(Daniels118): is this the right way?
	bool valid = false;
	if (objId != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		valid = registry.Valid(static_cast<entt::entity>(objId));
	}
	Pushb(valid);
}

void VortexFadeOut() // 257 VORTEX_FADE_OUT
{
	// the thing; a vortex -> it starts fading out, else "Thing not vortex" / "vortex fade out failed" (the original's
	// script error message is empty in the shipped game)
	const auto vortex = Pop().uintVal;
	if (const auto thing = ScriptThing(vortex, "VORTEX_FADE_OUT", "vortex fade out failed"))
	{
		ecs::vortex::StartFadeOut(*thing); // nothing for a thing that is not a vortex
	}
}

void RemoveReactionOfType() // 258 REMOVE_REACTION_OF_TYPE
{
	// const auto reaction = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureLearnEverythingExcluding() // 259 CREATURE_LEARN_EVERYTHING_EXCLUDING
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void PlayedPercentage() // 260 PLAYED_PERCENTAGE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void ObjectCastByObject() // 261 OBJECT_CAST_BY_OBJECT
{
	// const auto caster = Pop().uintVal;
	// const auto spellInstance = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void IsWindMagicAtPos() // 262 IS_WIND_MAGIC_AT_POS
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
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
	NotImplemented(__func__);
}

void GetObjectFade() // 265 GET_OBJECT_FADE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void PlayHandDemo() // 266 PLAY_HAND_DEMO
{
	// pops withoutHandModify, then the wait-for-trigger flag, then the demo's name (Input/HandDemo.h: its playback for
	// the running task)
	const auto withoutHandModify = Pop().intVal != 0;
	const auto waitTrigger = Pop().intVal != 0;
	const auto name = PopString();
	hand_demo::Play(name, Locator::vm::value().GetCurrentTaskNumber(), waitTrigger, withoutHandModify);
}

void IsPlayingHandDemo() // 267 IS_PLAYING_HAND_DEMO
{
	// the negation of whether the demo plays back, so the scripts' `CALL 267; JZ loop` wait until the demo has finished
	Pushb(!hand_demo::IsPlaying(0));
}

void GetArsePosition() // 268 GET_ARSE_POSITION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void IsLeashedToObject() // 269 IS_LEASHED_TO_OBJECT
{
	// two pops: the thing, then the creature; either order works, as for ATTACH_OBJECT_LEASH_TO_OBJECT
	const auto target = Pop().uintVal;
	const auto object = Pop().uintVal;
	Pushb(Locator::leashSystem::has_value() &&
	      creature_leash::script::IsLeashedToThing(Locator::leashSystem::value(), LeashThing(object), LeashThing(target),
	                                               IsLeashCreature));
}

void GetInteractionMagnitude() // 270 GET_INTERACTION_MAGNITUDE
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void IsCreatureAvailable() // 271 IS_CREATURE_AVAILABLE
{
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void CreateHighlight() // 272 CREATE_HIGHLIGHT
{
	// pops the challenge id, the point (z, y, x) and the script highlight info row
	const auto challenge = Pop().uintVal;
	const auto position = PopVec();
	const auto row = Pop().uintVal;
	// a highlight at the map coords of the point, with the info, the challenge, 0.0 and 1.0
	const auto thing = ecs::script_highlight::Create(position, row, challenge, 0.0f, 1.0f);
	if (thing == entt::null)
	{
		// "Highlight not created" and 0. (openblack) also for a row past the four
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATE_HIGHLIGHT: Highlight not created (row {})", row);
		Pusho(0);
		return;
	}
	ecs::script_held::AddScriptThing(thing, true); // a thing the script created
	Pusho(static_cast<uint32_t>(thing));
	// (pending) the debug name of the script that created it
}

void GetObjectHeld273() // 273 GET_OBJECT_HELD
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetActionCount() // 274 GET_ACTION_COUNT
{
	// const auto creature = Pop().uintVal;
	// const auto action = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetObjectLeashType() // 275 GET_OBJECT_LEASH_TYPE
{
	// the leash picked, worn or not, as the scripts number it, -1 for none; 0 when there is no creature to ask
	const auto object = Pop().uintVal;
	Pushi(Locator::leashSystem::has_value()
	          ? creature_leash::script::TypeOf(Locator::leashSystem::value(), LeashCreature(object))
	          : 0);
}

void SetFocusFollow() // 276 SET_FOCUS_FOLLOW
{
	// the same code as FOCUS_FOLLOW (the script camera's focus follows the thing, no placing at once)
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::FocusFollow(*thing);
	}
}

void SetPositionFollow() // 277 SET_POSITION_FOLLOW
{
	// POP the thing -> the position follows it, and the focus and the position are placed at once on the follow's
	// points (the mode's seconds at 2: then followed at the time factor's pace)
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::PositionFollow(*thing);
		script_camera::PlaceFollowNow();
	}
}

void SetFocusAndPositionFollow() // 278 SET_FOCUS_AND_POSITION_FOLLOW
{
	// POP the distance, then the thing -> as FOCUS_AND_POSITION_FOLLOW, placed at once
	const auto distance = Popf();
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::FocusAndPositionFollow(*thing, distance);
		script_camera::PlaceFollowNow();
	}
}

void SetCameraLens() // 279 SET_CAMERA_LENS
{
	// the FOV set to 70 degrees in x: the argument is the TIME and the lens goes back to the default (copied as the
	// original does; its one use is SET_CAMERA_LENS(0))
	const auto time = Popf();
	script_camera::SetFov(script_camera::k_DefaultFov, time);
}

void MoveCameraLens() // 280 MOVE_CAMERA_LENS
{
	// the FOV set to lens in t (degrees, seconds of game time)
	const auto time = Popf();
	const auto lens = Popf();
	script_camera::SetFov(lens * script_camera::k_DegreesToRadians, time);
}

void CreatureReaction() // 281 CREATURE_REACTION
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureInDevScript() // 282 CREATURE_IN_DEV_SCRIPT
{
	const auto object = Pop().uintVal;
	const auto inDevScript = Pop().intVal != 0;
	if (const auto thing = ScriptThing(object, "CREATURE_IN_DEV_SCRIPT", "Thing not found!"))
	{
		ecs::player_creature::SetInDevScript(Locator::entitiesRegistry::value(), *thing, inDevScript);
	}
}

void StoreCameraDetails() // 283 STORE_CAMERA_DETAILS
{
	// the drawn camera's position and focus are stored (not the FOV)
	const auto& camera = Locator::camera::value();
	auto& state = script_camera::Get();
	state.storedPosition = camera.GetOrigin();
	state.storedFocus = camera.GetFocus();
}

void RestoreCameraDetails() // 284 RESTORE_CAMERA_DETAILS
{
	// the camera's position and focus set to the stored ones, whatever the mode
	const auto& state = script_camera::Get();
	script_camera::SetPositionAndFocus(state.storedPosition, state.storedFocus);
	if (!script_camera::Active())
	{
		// (inferred) the player's mode: openblack's player Camera has its own zoomers (not script_camera's), so it is set
		// here too
		Locator::camera::value().SetOrigin(state.storedPosition).SetFocus(state.storedFocus);
	}
}

void StartAngleSound285() // 285 START_ANGLE_SOUND
{
	// POP, then the confirmation sound on the camera's turn (audio/Services/Confirmation.h)
	const auto enable = Pop().intVal != 0;
	audio::confirmation::StartAngleSound(enable);
}

void SetCameraPosFocLens() // 286 SET_CAMERA_POS_FOC_LENS
{
	// the camera's position and focus set, and the FOV set to lens at once with the lens NOT turned into radians (copied
	// as the original does, no map uses it)
	const auto lens = Popf();
	const auto focus = PopVec();
	const auto position = PopVec();
	script_camera::SetPositionAndFocus(position, focus);
	if (!script_camera::Active())
	{
		Locator::camera::value().SetOrigin(position).SetFocus(focus); // (inferred) as in 284
	}
	script_camera::SetFov(lens, 0.0f);
}

void MoveCameraPosFocLens() // 287 MOVE_CAMERA_POS_FOC_LENS
{
	// the script camera's position and focus moved in t and the FOV set to lens in t, the lens not in radians (copied,
	// no map uses it)
	const auto time = Popf();
	const auto lens = Popf();
	const auto focus = PopVec();
	const auto position = PopVec();
	if (ScriptCameraMode(__func__))
	{
		script_camera::MovePosition(position, time);
		script_camera::MoveFocus(focus, time);
		script_camera::SetFov(lens, time);
	}
}

void GameTimeOnOff() // 288 GAME_TIME_ON_OFF
{
	// the visual time scale set to on ? 1 : 0
	const auto enable = Pop().intVal != 0;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "GAME_TIME_ON_OFF({})", enable);
	Locator::dayNightClock::value().Clock().SetRunning(enable);
}

void MoveGameTime() // 289 MOVE_GAME_TIME
{
	// the visual time slides to the hour in `duration` seconds of game time
	const auto duration = Popf();
	const auto hourOfTheDay = Popf();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "MOVE_GAME_TIME({}, {})", hourOfTheDay, duration);
	Locator::dayNightClock::value().Clock().MoveScriptTime(hourOfTheDay, duration);
}

void SetHighGraphicsDetail() // 290 SET_HIGH_GRAPHICS_DETAIL
{
	// POP the object, then the bool; no thing: "Thing not found!". The SuperVillager of the intro family
	// (ecs/SuperVillager.h)
	const auto object = Pop().uintVal;
	const auto enable = Pop().intVal != 0;
	if (const auto thing = ScriptThing(object, "SET_HIGH_GRAPHICS_DETAIL", "Thing not found!"); thing.has_value())
	{
		ecs::super_villager::SetHighGraphicsDetail(*thing, enable);
	}
}

void SetSkeleton() // 291 SET_SKELETON
{
	// const auto object = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void IsSkeleton() // 292 IS_SKELETON
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void PlayerSpellCastTime() // 293 PLAYER_SPELL_CAST_TIME
{
	magic::script::PlayerSpellCastTime(); // Magic/Script/CHLSpells.cpp
}

void PlayerSpellLastCast() // 294 PLAYER_SPELL_LAST_CAST
{
	magic::script::PlayerSpellLastCast(); // Magic/Script/CHLSpells.cpp
}

void GetLastSpellCastPos() // 295 GET_LAST_SPELL_CAST_POS
{
	magic::script::GetLastSpellCastPos(); // Magic/Script/CHLSpells.cpp
}

void AddSpotVisualTargetPos() // 296 ADD_SPOT_VISUAL_TARGET_POS
{
	// const auto position = PopVec();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void AddSpotVisualTargetObject() // 297 ADD_SPOT_VISUAL_TARGET_OBJECT
{
	// const auto target = Pop().uintVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetIndestructable() // 298 SET_INDESTRUCTABLE
{
	// POP the object, POP the flag; a script container hands it to its type's callback (TODO: openblack has none),
	// anything else sets or clears its indestructible flag
	const auto object = static_cast<entt::entity>(Pop().uintVal);
	const auto indestructible = (Pop().intVal & 1) != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_INDESTRUCTABLE: Thing not valid");
		return;
	}
	if (indestructible)
	{
		registry.AssignOrReplace<openblack::ecs::components::Indestructible>(object);
	}
	else if (registry.AllOf<openblack::ecs::components::Indestructible>(object))
	{
		registry.Remove<openblack::ecs::components::Indestructible>(object);
	}
}

void SetGraphicsClipping() // 299 SET_GRAPHICS_CLIPPING
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SpiritAppear() // 300 SPIRIT_APPEAR
{
	// POP the spirit -> ejected as a help script's spirit: it appears
	const auto spirit = ScriptSpirit(Pop().intVal);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritEject(spirit, true);
	}
}

void SpiritDisappear() // 301 SPIRIT_DISAPPEAR
{
	// POP the spirit -> sent home as a help script's spirit: it vanishes
	const auto spirit = ScriptSpirit(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->SpiritHome(spirit, 1);
	}
}

void SetFocusOnObject() // 302 SET_FOCUS_ON_OBJECT
{
	// const auto target = Pop().uintVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ReleaseObjectFocus() // 303 RELEASE_OBJECT_FOCUS
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ImmersionExists() // 304 IMMERSION_EXISTS
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetDrawLeash() // 305 SET_DRAW_LEASH
{
	// the draw leash setting = the popped value as it is
	help::script_control::GetCameraControl().drawLeash = Pop().intVal;
}

void SetDrawHighlight() // 306 SET_DRAW_HIGHLIGHT
{
	// the draw highlight setting = the popped value as it is
	help::script_control::GetCameraControl().drawHighlight = Pop().intVal;
}

void SetOpenClose() // 307 SET_OPEN_CLOSE
{
	// const auto object = Pop().uintVal;
	// const auto open = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetIntroBuilding() // 308 SET_INTRO_BUILDING
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureForceFriends() // 309 CREATURE_FORCE_FRIENDS
{
	// const auto targetCreature = Pop().uintVal;
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void MoveComputerPlayerPosition() // 310 MOVE_COMPUTER_PLAYER_POSITION
{
	// const auto withFixedHeight = static_cast<bool>(Pop().intVal);
	// const auto speed = Popf();
	// const auto position = PopVec();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void EnableDisableComputerPlayer311() // 311 ENABLE_DISABLE_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetComputerPlayerPosition() // 312 GET_COMPUTER_PLAYER_POSITION
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
}

void GetStoredCameraPosition() // 314 GET_STORED_CAMERA_POSITION
{
	// the stored camera position
	PushVec(script_camera::Get().storedPosition);
}

void GetStoredCameraFocus() // 315 GET_STORED_CAMERA_FOCUS
{
	// the stored camera focus
	PushVec(script_camera::Get().storedFocus);
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
	NotImplemented(__func__);
	Pusho(0);
}

void SetCreatureSound() // 317 SET_CREATURE_SOUND
{
	// the script's creature sound setting = the value as it is
	audio::GetScriptAudioState().creatureSound = Pop().intVal;
}

void CreatureInteractingWith() // 318 CREATURE_INTERACTING_WITH
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetSunDraw() // 319 SET_SUN_DRAW
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ObjectInfoBits() // 320 OBJECT_INFO_BITS
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void SetHurtByFire() // 321 SET_HURT_BY_FIRE
{
	magic::script::SetHurtByFire(); // Magic/Script/CHLFire.cpp
}

void ConfinedObject() // 322 CONFINED_OBJECT
{
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ClearConfinedObject() // 323 CLEAR_CONFINED_OBJECT
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetObjectFlock() // 324 GET_OBJECT_FLOCK
{
	// const auto member = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void SetPlayerBelief() // 325 SET_PLAYER_BELIEF
{
	// const auto belief = Popf();
	// const auto player = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void PlayJcSpecial() // 326 PLAY_JC_SPECIAL
{
	// on the value, 0..15: 0, 1, 2, 4, 5, 6 -> the special of that number, 3 a script GFX object, 14 / 15 the camera
	// bookmarks on / off
	const auto feature = static_cast<uint32_t>(Pop().intVal); // unsigned: a negative value is above 15
	switch (feature)
	{
	case 0: // the light onto the Son and the debug camera (the intro specials, ecs/IntroSpecial.h)
	case 1:
	case 2:
	case 4: // the intro hand
	case 5:
		ecs::intro_special::Play(static_cast<int32_t>(feature));
		break;
	case 3: // a script GFX object: (pending) in no script of the game
		NotImplemented(__func__);
		break;
	case 6:
		// the missionaries' boat, in mode 0
		openblack::ecs::missionary_boat::Create(0);
		break;
	case 14:                                            // the bookmarks on
		if (Locator::cameraBookmarkSystem::has_value()) // (openblack guard) none in the CHL tests and tools
		{
			Locator::cameraBookmarkSystem::value().SetEnabled(true);
		}
		break;
	case 15: // the bookmarks off. (FollowUs only, L50058)
		if (Locator::cameraBookmarkSystem::has_value())
		{
			Locator::cameraBookmarkSystem::value().SetEnabled(false);
		}
		break;
	default: // 7..13 and above 15: nothing (FollowUs's 18 at L52177)
		break;
	}
}

void IsPlayingJcSpecial() // 327 IS_PLAYING_JC_SPECIAL
{
	// the value truncated; 1, except 13 -> whether that special has finished (the pick-up clip's wrap: never, it is one
	// shot), pushed as a bool (type 6). No script of the game calls it
	const auto feature = static_cast<int32_t>(Popf());
	Pushb(feature == 13 ? ecs::intro_special::HasPickUpClipFinished() : true);
}

void VortexParameters() // 328 VORTEX_PARAMETERS
{
	// last argument first: the flock, b, a, the position, the town, the vortex. Town or vortex missing -> "Thing not
	// valid"; else the vortex's town and its flock parameters (the map coords of position, a, b, flock): the Out's
	// (docs/bw1-notes/vortex.md "Bringing them out")
	const auto flock = Pop().uintVal;
	const auto b = Popf();
	const auto a = Popf();
	const auto position = PopVec();
	const auto town = Pop().uintVal;
	const auto vortex = Pop().uintVal;
	const auto townThing = ScriptThing(town, "VORTEX_PARAMETERS", "Thing not valid");
	const auto vortexThing = ScriptThing(vortex, "VORTEX_PARAMETERS", "Thing not valid");
	if (!townThing || !vortexThing)
	{
		return;
	}
	// (approximate) no flock cast: a valid entity stands for it
	const auto flockEntity = static_cast<entt::entity>(flock);
	const auto flockThing = flock != 0 && Locator::entitiesRegistry::value().Valid(flockEntity) ? flockEntity : entt::null;
	ecs::vortex::SetParameters(*vortexThing, *townThing, position, a, b, flockThing);
}

void LoadCreature() // 329 LOAD_CREATURE
{
	// const auto position = PopVec();
	// const auto player = Popf();
	// const auto mindFilename = PopString();
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void IsSpellCharging() // 330 IS_SPELL_CHARGING
{
	magic::script::IsSpellCharging(); // Magic/Script/CHLWorship.cpp
}

void IsThatSpellCharging() // 331 IS_THAT_SPELL_CHARGING
{
	magic::script::IsThatSpellCharging(); // Magic/Script/CHLWorship.cpp
}

void OpposingCreature() // 332 OPPOSING_CREATURE
{
	// const auto god = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void FlockWithinLimits() // 333 FLOCK_WITHIN_LIMITS
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void HighlightProperties() // 334 HIGHLIGHT_PROPERTIES
{
	// pops the DYK_CATEGORY, the text, then the thing
	const auto category = Pop().uintVal;
	const auto text = Pop().uintVal;
	const auto object = Pop().uintVal;
	const auto thing = static_cast<entt::entity>(object);
	if (object == 0 || !ecs::script_highlight::IsHighlight(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "HIGHLIGHT_PROPERTIES: Thing not valid");
		return;
	}
	// the highlight's script id (text, category)
	ecs::script_highlight::SetScriptId(thing, text, static_cast<DykCategory>(category));
}

void LastMusicLine() // 335 LAST_MUSIC_LINE
{
	// without audio the original runs TEXT_READ instead
	const auto line = Popf();
	std::optional<bool> reached;
	{
		const auto lock = audio::game_music::Lock();
		if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
		{
			reached = gameMusic->ScriptLastMusicLine(line);
		}
	}
	if (reached)
	{
		Pushb(*reached);
	}
	else
	{
		TextRead();
	}
}

void HandDemoTrigger() // 336 HAND_DEMO_TRIGGER
{
	// the script's pending trigger, cleared as it is read (Input/HandDemo.h)
	Pushb(hand_demo::ConsumeTrigger());
}

void GetBellyPosition() // 337 GET_BELLY_POSITION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
}

void GameThingCanViewCamera() // 339 GAME_THING_CAN_VIEW_CAMERA
{
	// const auto degrees = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GamePlaySaySoundEffect() // 340 GAME_PLAY_SAY_SOUND_EFFECT
{
	// six POPs (withPos, the point, the text, alt), then the text is said (audio::voices::Say)
	const auto withPosition = Pop().intVal != 0;
	const auto position = PopVec();
	const auto text = static_cast<uint32_t>(Pop().intVal);
	const auto alt = Pop().intVal != 0;
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "GAME_PLAY_SAY_SOUND_EFFECT({}, {}, ({}, {}, {}), {})", alt, text, position.x,
	                    position.y, position.z, withPosition);
	audio::voices::Say(text, withPosition, alt, position);
}

void SetTownDesireBoost() // 341 SET_TOWN_DESIRE_BOOST
{
	// POP the boost, the desire, the thing; a town and desire < 17 and -1 <= boost <= 1 -> the town's boost of that
	// desire and its desires re-sorted (ecs::town_desire::ScriptSetTownDesireBoost)
	const auto boost = Popf();
	const auto desire = Pop().intVal;
	const auto object = Pop().uintVal;
	std::vector<std::string> errors;
	ecs::town_desire::ScriptSetTownDesireBoost(static_cast<entt::entity>(object), desire, boost, &errors);
	for (const auto& error : errors)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_TOWN_DESIRE_BOOST: {}", error);
	}
}

void IsLockedInteraction() // 342 IS_LOCKED_INTERACTION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetCreatureName() // 343 SET_CREATURE_NAME
{
	// const auto textID = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ComputerPlayerReady() // 344 COMPUTER_PLAYER_READY
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void EnableDisableComputerPlayer345() // 345 ENABLE_DISABLE_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// const auto pause = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ClearActorMind() // 346 CLEAR_ACTOR_MIND
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	// the second "START_ANGLE_SOUND", the pitch one: the confirmation sound on the camera's tilt
	const auto enable = Pop().intVal != 0;
	audio::confirmation::StartPitchSound(enable);
}

void ThingJcSpecial() // 349 THING_JC_SPECIAL
{
	// POP the object, the feature, then the bool; no thing: "Object no longer valid". The flags of a SuperVillager
	// (ecs/SuperVillager.h)
	const auto object = Pop().uintVal;
	const auto feature = Pop().intVal;
	const auto enable = Pop().intVal != 0;
	if (const auto thing = ScriptThing(object, "THING_JC_SPECIAL", "Object no longer valid"); thing.has_value())
	{
		ecs::super_villager::ThingJcSpecial(*thing, feature, enable);
	}
}

void MusicPlayed350() // 350 MUSIC_PLAYED
{
	// the music playing != type (a bool, type 6); without audio the original runs TEXT_READ instead
	const auto music = Pop().intVal;
	std::optional<bool> played;
	{
		const auto lock = audio::game_music::Lock();
		if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
		{
			played = gameMusic->ScriptMusicPlayed(music);
		}
	}
	if (played)
	{
		Pushb(*played);
	}
	else
	{
		TextRead();
	}
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
	NotImplemented(__func__);
}

void StopScriptsInFilesExcluding() // 352 STOP_SCRIPTS_IN_FILES_EXCLUDING
{
	const auto scriptNames = PopString();
	const auto sourceFilenames = PopString();

	const auto names = GetUniqueWords(scriptNames);
	const auto filenames = GetUniqueWords(sourceFilenames);
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&names, &filenames](const std::string& name, const std::string& filename) -> bool {
		return filenames.contains(filename) && !names.contains(name);
	});
}

void CreateRandomVillagerOfTribe() // 353 CREATE_RANDOM_VILLAGER_OF_TRIBE
{
	// const auto position = PopVec();
	// const auto tribe = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void ToggleLeash() // 354 TOGGLE_LEASH
{
	// the script player (a float, truncated) presses the leash key for their creature
	const auto scriptPlayer = map_coords::FtoL(Popf());
	if (Locator::leashSystem::has_value())
	{
		creature_leash::script::Toggle(Locator::leashSystem::value(), scriptPlayer);
	}
}

void GameSetMana() // 355 GAME_SET_MANA
{
	magic::script::GameSetMana(); // Magic/Script/CHLWorship.cpp
}

void SetMagicProperties() // 356 SET_MAGIC_PROPERTIES
{
	magic::script::SetMagicProperties(); // Magic/Script/CHLWorship.cpp
}

void SetGameSound() // 357 SET_GAME_SOUND
{
	// false -> every sample stopped and the game sound off (only the dialogue banks HelpSprites / Villagers play); true
	// -> on
	const auto enable = static_cast<bool>(Pop().intVal);
	audio::SetGameSound(enable);
}

void SexIsMale() // 358 SEX_IS_MALE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GetFirstHelp() // 359 GET_FIRST_HELP
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetLastHelp() // 360 GET_LAST_HELP
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void IsActive() // 361 IS_ACTIVE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetBookmarkPosition() // 362 SET_BOOKMARK_POSITION
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetScaffoldProperties() // 363 SET_SCAFFOLD_PROPERTIES
{
	// POP destroy, size (a float), type (stored raw), the object. (not verified) whether the VM gives `type` as an int
	// or as float bits
	const bool destroy = (Pop().intVal & 1) != 0;
	const auto size = Popf();
	const auto type = static_cast<int32_t>(Pop().intVal);
	const auto object = Pop().uintVal;
	// none -> "Object no longer valid!"
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_SCAFFOLD_PROPERTIES: Object no longer valid!");
		return;
	}
	// not a scaffold -> "Thing must be scaffold", and nothing more (the next scaffold test fails)
	if (!ecs::scaffolds::IsScaffold(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_SCAFFOLD_PROPERTIES: Thing must be scaffold");
		return;
	}
	// its type, its value = int(size), its destroy flag = destroy & 1
	ecs::scaffolds::SetScaffoldProperties(entity, type, size, destroy);
}

void SetComputerPlayerPersonality() // 364 SET_COMPUTER_PLAYER_PERSONALITY
{
	// const auto probability = Popf();
	// const auto aspect = PopString();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetComputerPlayerSuppression() // 365 SET_COMPUTER_PLAYER_SUPPRESSION
{
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ForceComputerPlayerAction() // 366 FORCE_COMPUTER_PLAYER_ACTION
{
	// const auto obj2 = Pop().uintVal;
	// const auto obj1 = Pop().uintVal;
	// const auto action = PopString();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void QueueComputerPlayerAction() // 367 QUEUE_COMPUTER_PLAYER_ACTION
{
	// const auto obj2 = Pop().uintVal;
	// const auto obj1 = Pop().uintVal;
	// const auto action = PopString();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetTownWithId() // 368 GET_TOWN_WITH_ID
{
	// const auto id = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void SetDisciple() // 369 SET_DISCIPLE
{
	// const auto withSound = static_cast<bool>(Pop().intVal);
	// const auto discipleType = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ReleaseComputerPlayer() // 370 RELEASE_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetComputerPlayerSpeed() // 371 SET_COMPUTER_PLAYER_SPEED
{
	// const auto speed = Popf();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetFocusFollowComputerPlayer() // 372 SET_FOCUS_FOLLOW_COMPUTER_PLAYER
{
	// POP the player (a float, truncated), the script player to the game's player, the mode checks, then the path
	// dropped, no focus thing and the focus follows the computer player's hand. Not ported: openblack has no computer
	// players
	[[maybe_unused]] const auto player = Popf();
	NotImplemented(__func__);
}

void SetPositionFollowComputerPlayer() // 373 SET_POSITION_FOLLOW_COMPUTER_PLAYER
{
	// as 372 for the position: the path dropped, no follow thing and the position follows the player. Not ported
	[[maybe_unused]] const auto player = Popf();
	NotImplemented(__func__);
}

void CallComputerPlayer() // 374 CALL_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void CallBuildingInTown() // 375 CALL_BUILDING_IN_TOWN
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void SetCanBuildWorshipsite() // 376 SET_CAN_BUILD_WORSHIPSITE
{
	magic::script::SetCanBuildWorshipsite(); // Magic/Script/CHLWorship.cpp
}

void GetFacingCameraPosition() // 377 GET_FACING_CAMERA_POSITION
{
	// the drawn camera's position + d * the camera's forward vector (taken as the unit vector from the drawn position to
	// the drawn focus: inferred)
	const auto distance = Popf();
	const auto& camera = Locator::camera::value();
	const auto origin = camera.GetOrigin();
	const auto toFocus = camera.GetFocus() - origin;
	const float length = glm::length(toFocus);
	PushVec(length > 0.0f ? origin + toFocus * (distance / length) : origin);
}

void SetComputerPlayerAttitude() // 378 SET_COMPUTER_PLAYER_ATTITUDE
{
	// const auto attitude = Popf();
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetComputerPlayerAttitude() // 379 GET_COMPUTER_PLAYER_ATTITUDE
{
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void LoadComputerPlayerPersonality() // 380 LOAD_COMPUTER_PLAYER_PERSONALITY
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SaveComputerPlayerPersonality() // 381 SAVE_COMPUTER_PLAYER_PERSONALITY
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetPlayerAlly() // 382 SET_PLAYER_ALLY
{
	// const auto percentage = Popf();
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CallFlying() // 383 CALL_FLYING
{
	// const auto excluding = static_cast<bool>(Pop().intVal);
	// const auto radius = Popf();
	// const auto position = PopVec();
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void SetObjectFadeIn() // 384 SET_OBJECT_FADE_IN
{
	// const auto time = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void IsAffectedBySpell() // 385 IS_AFFECTED_BY_SPELL
{
	// pops two values, the object last (pending: what the first one is)
	// const auto first = Pop().uintVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetMagicInObject() // 386 SET_MAGIC_IN_OBJECT
{
	magic::script::SetMagicInObject(); // Magic/Script/CHLWorship.cpp
}

void IdAdultSize() // 387 ID_ADULT_SIZE
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void ObjectCapacity() // 388 OBJECT_CAPACITY
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void ObjectAdultCapacity() // 389 OBJECT_ADULT_CAPACITY
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void SetCreatureAutoFighting() // 390 SET_CREATURE_AUTO_FIGHTING
{
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void IsAutoFighting() // 391 IS_AUTO_FIGHTING
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetCreatureQueueFightMove() // 392 SET_CREATURE_QUEUE_FIGHT_MOVE
{
	// const auto move = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetCreatureQueueFightSpell() // 393 SET_CREATURE_QUEUE_FIGHT_SPELL
{
	// const auto spell = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetCreatureQueueFightStep() // 394 SET_CREATURE_QUEUE_FIGHT_STEP
{
	// const auto step = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetCreatureFightAction() // 395 GET_CREATURE_FIGHT_ACTION
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void CreatureFightQueueHits() // 396 CREATURE_FIGHT_QUEUE_HITS
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
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
	NotImplemented(__func__);
	Pushf(0.0f);
}

void SetPlayerWindResistance() // 399 SET_PLAYER_WIND_RESISTANCE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void GetPlayerWindResistance() // 400 GET_PLAYER_WIND_RESISTANCE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void PauseUnpauseClimateSystem() // 401 PAUSE_UNPAUSE_CLIMATE_SYSTEM
{
	magic::script::PauseUnpauseClimateSystem(); // Magic/Script/CHLWeather.cpp
}

void PauseUnpauseStormCreationInClimateSystem() // 402 PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM
{
	magic::script::PauseUnpauseStormCreationInClimateSystem(); // Magic/Script/CHLWeather.cpp
}

void GetManaForSpell() // 403 GET_MANA_FOR_SPELL
{
	magic::script::GetManaForSpell(); // Magic/Script/CHLSpells.cpp
}

void KillStormsInArea() // 404 KILL_STORMS_IN_AREA
{
	magic::script::KillStormsInArea(); // Magic/Script/CHLWeather.cpp
}

void InsideTemple() // 405 INSIDE_TEMPLE
{
	// whether we are inside the citadel, a bool (type 6)
	Pushb(openblack::game_clock::IsInsideCitadel());
}

void RestartObject() // 406 RESTART_OBJECT
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetGameTimeProperties() // 407 SET_GAME_TIME_PROPERTIES
{
	// the visual time cycle (duration, percentage night, percentage change)
	const auto percentageChange = Popf();
	const auto percentageNight = Popf();
	const auto duration = Popf();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_GAME_TIME_PROPERTIES({}, {}, {})", duration, percentageNight,
	                   percentageChange);
	Locator::dayNightClock::value().Clock().SetCycle(duration, percentageNight, percentageChange);
}

void ResetGameTimeProperties() // 408 RESET_GAME_TIME_PROPERTIES
{
	Locator::dayNightClock::value().Clock().SetCycle(DayNightClock::k_DefaultDuration, DayNightClock::k_DefaultNight,
	                                                 DayNightClock::k_DefaultChange);
}

void SoundExists() // 409 SOUND_EXISTS
{
	// whether the sound is installed, pushed as a bool (type 6)
	Pushb(audio::SoundExists());
}

void GetTownWorshipDeaths() // 410 GET_TOWN_WORSHIP_DEATHS
{
	magic::script::GetTownWorshipDeaths(); // Magic/Script/CHLWorship.cpp
}

void GameClearDialogue() // 411 GAME_CLEAR_DIALOGUE
{
	// the help system's text cleared (the voices go on, docs/bw1-notes/audio.md)
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->ClearDialogue();
	}
}

void GameCloseDialogue() // 412 GAME_CLOSE_DIALOGUE
{
	// the help text closed and the help system's text cleared
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->CloseDialogue();
	}
}

void GetHandState() // 413 GET_HAND_STATE
{
	// the interface's hand state of the last turn (HandSystemInterface)
	Pushi(Locator::handSystem::value().GetInterfaceHandState());
}

void SetInterfaceCitadel() // 414 SET_INTERFACE_CITADEL
{
	// the interface citadel setting = POP() (the raw value); its only reader is the citadel entrance's tap check
	openblack::worship::citadel::SetInterfaceCitadel(Pop().uintVal);
}

void MapScriptFunction() // 415 MAP_SCRIPT_FUNCTION
{
	// const auto command = PopString();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void WithinRotation() // 416 WITHIN_ROTATION
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GetPlayerTownTotal() // 417 GET_PLAYER_TOWN_TOTAL
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void SpiritScreenPoint() // 418 SPIRIT_SCREEN_POINT
{
	// POP y, x, the spirit; "Invalid Y" / "Invalid X" (not stopping); the pixel (int(W x), int(H y)) of the screen's
	// width and height -> the spirit points at it
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptSpirit(Pop().intVal);
	CheckScreenXY("SPIRIT_SCREEN_POINT", x, y);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		const auto& screen = control->GetScreen();
		const glm::ivec2 pixel(static_cast<int32_t>(static_cast<float>(screen.width) * x),
		                       static_cast<int32_t>(static_cast<float>(screen.height) * y));
		control->SpiritScreenPoint(spirit, pixel);
	}
}

void KeyDown() // 419 KEY_DOWN
{
	// const auto key = Pop().intVal;
	// TODO(Daniels118): implement this (translate key to physical key code)
	NotImplemented(__func__);
	Pushb(false);
}

void SetFightExit() // 420 SET_FIGHT_EXIT
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetObjectClicked() // 421 GET_OBJECT_CLICKED
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetMana() // 422 GET_MANA
{
	magic::script::GetMana(); // Magic/Script/CHLWorship.cpp
}

void ClearPlayerSpellCharging() // 423 CLEAR_PLAYER_SPELL_CHARGING
{
	magic::script::ClearPlayerSpellCharging(); // Magic/Script/CHLWorship.cpp
}

void StopSoundEffect() // 424 STOP_SOUND_EFFECT
{
	// POPs bank, sample (or a HELP_TEXT with isSay), isSay (the scripts of the shipped game always pass 0)
	const auto bank = Pop().intVal;
	const auto id = Pop().uintVal;
	const auto isSay = Pop().intVal != 0;
	audio::script_sound::StopSoundEffect(isSay, id, bank);
}

void GetTotemStatue() // 425 GET_TOTEM_STATUE
{
	// const auto town = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void SetSetOnFire() // 426 SET_SET_ON_FIRE
{
	magic::script::SetSetOnFire(); // Magic/Script/CHLFire.cpp
}

void SetLandBalance() // 427 SET_LAND_BALANCE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetObjectBeliefScale() // 428 SET_OBJECT_BELIEF_SCALE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StartImmersion() // 429 START_IMMERSION
{
	// const auto effect = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StopImmersion() // 430 STOP_IMMERSION
{
	// const auto effect = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StopAllImmersion() // 431 STOP_ALL_IMMERSION
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetCreatureInTemple() // 432 SET_CREATURE_IN_TEMPLE
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
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
	NotImplemented(__func__);
}

void FadeAllDrawText() // 435 FADE_ALL_DRAW_TEXT
{
	// const auto time = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetDrawTextColour() // 436 SET_DRAW_TEXT_COLOUR
{
	// const auto blue = Popf();
	// const auto green = Popf();
	// const auto red = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetClippingWindow() // 437 SET_CLIPPING_WINDOW
{
	// const auto time = Popf();
	// const auto height = Popf();
	// const auto width = Popf();
	// const auto down = Popf();
	// const auto across = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ClearClippingWindow() // 438 CLEAR_CLIPPING_WINDOW
{
	// const auto time = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SaveGameInSlot() // 439 SAVE_GAME_IN_SLOT
{
	// const auto slot = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetObjectCarrying() // 440 SET_OBJECT_CARRYING
{
	// const auto carriedObj = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void PosValidForCreature() // 441 POS_VALID_FOR_CREATURE
{
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GetTimeSinceObjectAttacked() // 442 GET_TIME_SINCE_OBJECT_ATTACKED
{
	// const auto town = Pop().uintVal;
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetTownAndVillagerHealthTotal() // 443 GET_TOWN_AND_VILLAGER_HEALTH_TOTAL
{
	// const auto town = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GameAddForBuilding() // 444 GAME_ADD_FOR_BUILDING
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void EnableDisableAlignmentMusic() // 445 ENABLE_DISABLE_ALIGNMENT_MUSIC
{
	// the alignment music setting = the value as it is
	const auto enable = Pop().intVal;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "ENABLE_DISABLE_ALIGNMENT_MUSIC({})", enable);
	audio::GetScriptAudioState().alignmentMusic = enable;
}

void GetDeadLiving() // 446 GET_DEAD_LIVING
{
	// const auto radius = Popf();
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void AttachSoundTag() // 447 ATTACH_SOUND_TAG
{
	// POPs the object (looked up at once), bank, sample, threeD
	const auto object = MusicThing(Pop().uintVal);
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	const auto threeD = Pop().intVal != 0;
	if (object)
	{
		audio::script_sound::AttachSoundTag(threeD, sample, bank, static_cast<entt::entity>(*object));
	}
}

void DetachSoundTag() // 448 DETACH_SOUND_TAG
{
	// POPs the object (looked up at once), bank, sample
	const auto object = MusicThing(Pop().uintVal);
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	if (object)
	{
		audio::script_sound::DetachSoundTag(sample, bank, static_cast<entt::entity>(*object));
	}
}

void GetSacrificeTotal() // 449 GET_SACRIFICE_TOTAL
{
	// const auto worshipSite = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GameSoundPlaying() // 450 GAME_SOUND_PLAYING
{
	// POPs bank, sample; pushes whether it is playing as a bool (type 6)
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	Pushb(audio::script_sound::GameSoundPlaying(sample, bank));
}

void GetTemplePosition() // 451 GET_TEMPLE_POSITION
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
}

void GetSpellIconInTemple() // 453 GET_SPELL_ICON_IN_TEMPLE
{
	magic::script::GetSpellIconInTemple(); // Magic/Script/CHLWorship.cpp
}

void GameClearComputerPlayerActions() // 454 GAME_CLEAR_COMPUTER_PLAYER_ACTIONS
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetFirstInContainer() // 455 GET_FIRST_IN_CONTAINER
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetNextInContainer() // 456 GET_NEXT_IN_CONTAINER
{
	// const auto after = Pop().uintVal;
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetTempleEntrancePosition() // 457 GET_TEMPLE_ENTRANCE_POSITION
{
	// const auto height = Popf();
	// const auto radius = Popf();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SaySoundEffectPlaying() // 458 SAY_SOUND_EFFECT_PLAYING
{
	// POP the text, then alt; push audio::voices::IsSaying (type 6)
	const auto text = static_cast<uint32_t>(Pop().intVal);
	const auto alt = Pop().intVal != 0;
	Pushb(audio::voices::IsSaying(alt, text));
}

void SetHandDemoKeys() // 459 SET_HAND_DEMO_KEYS
{
	// The original's handler is empty: it does nothing and pops nothing (no script calls it). openblack's VM drops the
	// declared argument when a native pops nothing (LHVM Opcode05Sys), which the original would leave on the stack
}

// The three push a bit of the game's tutorial skip flags as a boolean (type 6); the bits are set at each new game by
// the SkipBox answer (Game::Run). SetupLand1 turns them into IsSkippingToCreatureSelect, IsSkippingCreatureGuide and
// IsKeepingOldCreature, which LandControl1 reads (docs/bw1-notes/map-loading.md).
void CanSkipTutorial() // 460 CAN_SKIP_TUTORIAL
{
	// bit 23
	Pushb(Locator::mapScriptSystem::value().Globals().tutorialSkipFlags.canSkipTutorial);
}

void CanSkipCreatureTraining() // 461 CAN_SKIP_CREATURE_TRAINING
{
	// bit 24
	Pushb(Locator::mapScriptSystem::value().Globals().tutorialSkipFlags.canSkipCreatureTraining);
}

void IsKeepingOldCreature() // 462 IS_KEEPING_OLD_CREATURE
{
	// bit 25
	Pushb(Locator::mapScriptSystem::value().Globals().tutorialSkipFlags.isKeepingOldCreature);
}

void CurrentProfileHasCreature() // 463 CURRENT_PROFILE_HAS_CREATURE
{
	// Whether the profile's mind file exists in Scripts/CreatureMind. The profile's file is the creature-file setting
	Pushb(ecs::player_creature::ProfileHasCreature());
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
	CREATE_FUNCTION_BINDING("GET_ACTION_TEXT_FOR_OBJECT", 0, 1, GetActionTextForObject);
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
