/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed runner: the commands that run and stop scripts.

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <fmt/format.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Commands/PlayerCommands.h"
#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Sound/GameMusic.h"
#include "TestbedScenarioRunner.h"
#include "TestbedScenarioRunnerParts.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::runner_parts;
using openblack::ecs::components::Creature;
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
} // namespace

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

std::optional<entt::entity> Runner::ScriptFlock()
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(_scriptFlock) && ecs::script_flocks::IsFlock(registry, _scriptFlock))
	{
		return _scriptFlock;
	}
	const auto first = ObjectAt(0);
	if (!first.has_value())
	{
		return std::nullopt;
	}
	// The flock starts where the first object is, as a script makes one at a point
	const auto& at = registry.Get<const ecs::components::Transform>(*first).position;
	_scriptFlock = ecs::script_flocks::Create(registry, map_coords::FromMetres({at.x, at.z}));
	// The scenario keeps the flock as a script variable keeps it. Without the reference, the scripts' turn lets go of the
	// flock and its members leave it.
	auto& scripts = Locator::scriptObjects::value();
	static_cast<void>(scripts.Register(_scriptFlock, true));
	scripts.AddReference(_scriptFlock);
	for (const auto object : _objects)
	{
		if (registry.Valid(object) && registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(object))
		{
			static_cast<void>(ecs::script_flocks::AddLiving(registry, _scriptFlock, object));
		}
	}
	return _scriptFlock;
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
	case ScriptArgument::Kind::Temple:
		if (argument.index < _temples.size())
		{
			object = _temples.at(argument.index);
		}
		break;
	case ScriptArgument::Kind::Position:
		// A position is three values, which only a native call gives
		return std::nullopt;
	case ScriptArgument::Kind::Result:
		return _lastNativeResult;
	case ScriptArgument::Kind::LastResult:
		return _lastNativeResult;
	case ScriptArgument::Kind::Flock:
		object = ScriptFlock();
		break;
	case ScriptArgument::Kind::Town:
	{
		const auto& towns = Locator::entitiesRegistry::value().Context().towns;
		const auto found = towns.find(ScenarioTown(_middle));
		if (found == towns.end())
		{
			return std::nullopt;
		}
		object = found->second;
		break;
	}
	case ScriptArgument::Kind::Made:
		if (argument.index >= _madeByNatives.size())
		{
			return std::nullopt;
		}
		return lhvm::VMValue(_madeByNatives.at(argument.index));
	case ScriptArgument::Kind::Text:
		// A text is not a value: only a native call gives it
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
		if (argument.kind == ScriptArgument::Kind::Text)
		{
			arguments.emplace_back(std::string(argument.text));
			continue;
		}
		const auto given = ScriptValueOf(argument);
		if (!given.has_value())
		{
			return "an argument names nothing of the scenario's";
		}
		const auto type = ScriptDataTypeOf(argument.kind);
		arguments.emplace_back(std::pair(*given, type));
		if (const auto kept = static_cast<entt::entity>(given->uintVal);
		    argument.kept && type == lhvm::DataType::Object && std::ranges::find(_keptByScript, kept) == _keptByScript.end())
		{
			// The scenario keeps the thing as a script variable keeps it, so that it stays in the control of the script
			auto& scripts = Locator::scriptObjects::value();
			static_cast<void>(scripts.Register(kept, false));
			scripts.AddReference(kept);
			_keptByScript.push_back(kept);
		}
	}
	std::vector<std::pair<lhvm::VMValue, lhvm::DataType>> results;
	const auto id = static_cast<uint32_t>(std::distance(functions->begin(), found));
	if (!vm.CallNative(id, arguments, results))
	{
		return fmt::format("{} could not run now", command.script);
	}
	_lastNativeResult = results.empty() ? std::nullopt : std::optional(results.front().first);
	for (const auto& [value, type] : results)
	{
		if (type == lhvm::DataType::Object)
		{
			_madeByNatives.push_back(value.uintVal);
		}
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
