/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include "CreatureArtSystem.h"

#include <algorithm>
#include <string_view>

#include <LHVM.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "ECS/Components/Creature.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The natives that make a thing of the type and the subtype that a script pushes first
constexpr std::array<std::string_view, 2> k_MakeNativeNames = {"CREATE", "CREATE_WITH_ANGLE_AND_SCALE"};

/// The script that runs the control script of each land by the number of the land. A scan through the scripts that it
/// runs finds the creatures of every land. Thus the system scans only its own code, and then the control script of the
/// land when it starts.
constexpr std::string_view k_LandControlScript = "LandControlAll";

size_t Index(CreatureType species)
{
	return static_cast<size_t>(species);
}

bool IsSpecies(CreatureType species)
{
	return species > CreatureType::Unknown && species < CreatureType::_COUNT;
}
} // namespace

void CreatureArtSystem::HoldSpecies(CreatureType species)
{
	if (IsSpecies(species))
	{
		_held.set(Index(species));
	}
}

void CreatureArtSystem::HoldSkinFile(CreatureType species, entt::id_type file)
{
	if (IsSpecies(species))
	{
		_held.set(Index(species));
		_skinFiles.at(Index(species)).push_back(file);
	}
}

bool CreatureArtSystem::IsAskedFor(CreatureType species) const
{
	return !IsSpecies(species) || !_held.test(Index(species)) || _asked.test(Index(species));
}

void CreatureArtSystem::AskFor(CreatureType species)
{
	if (IsAskedFor(species) || !Locator::resources::has_value())
	{
		return;
	}
	_asked.set(Index(species));
	auto& resources = Locator::resources::value();
	// The animations come first, then the base mesh, which the game reads when it makes the creature
	resources.GetCreatureRigs().Prefetch(creature::GetRigId(species));
	auto& meshes = resources.GetMeshes();
	for (const auto id : creature_art::MeshIds(species))
	{
		meshes.Prefetch(id);
	}
	auto& files = resources.GetL3DFiles();
	for (const auto id : _skinFiles.at(Index(species)))
	{
		files.Prefetch(id);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Asked for the art of creature species {}", static_cast<int>(species));
}

void CreatureArtSystem::AskForWorld()
{
	if (_held.none())
	{
		return;
	}
	Locator::entitiesRegistry::value().Each<const components::Creature>(
	    [this](entt::entity, const components::Creature& creature) { AskFor(creature.species); });
	_code = nullptr;
}

void CreatureArtSystem::FollowScripts()
{
	const auto& vm = Locator::vm::value();
	const auto& code = vm.GetInstructions();
	if (_code == code.data() && _codeSize == code.size())
	{
		return;
	}
	_code = code.data();
	_codeSize = code.size();
	_lastTask = 0;
	_scanned.clear();
	_makeNatives.clear();
	if (const auto* functions = vm.GetFunctions(); functions != nullptr)
	{
		for (uint32_t i = 0; i < functions->size(); ++i)
		{
			if (std::ranges::find(k_MakeNativeNames, functions->at(i).name) != k_MakeNativeNames.end())
			{
				_makeNatives.push_back(i);
			}
		}
	}
	// The code of a script ends where the next script starts
	std::vector<uint32_t> starts;
	starts.reserve(vm.GetScripts().size());
	for (const auto& script : vm.GetScripts())
	{
		starts.push_back(script.instructionAddress);
	}
	std::ranges::sort(starts);
	_scripts.clear();
	for (const auto& script : vm.GetScripts())
	{
		const auto next = std::ranges::upper_bound(starts, script.instructionAddress);
		const auto end = next != starts.end() ? *next : static_cast<uint32_t>(code.size());
		_scripts.emplace(script.scriptId, creature_art::ScriptCode {.begin = script.instructionAddress, .end = end});
	}
}

void CreatureArtSystem::Update()
{
	if (_held.none() || (_asked | ~_held).all() || !Locator::vm::has_value())
	{
		return;
	}
	FollowScripts();
	const auto& vm = Locator::vm::value();
	const auto& tasks = vm.GetTasks();
	// The numbers of the tasks go up. A task with a higher number than the last task started since the last frame.
	if (tasks.empty() || tasks.rbegin()->first <= _lastTask)
	{
		return;
	}
	const creature_art::CodeOfScript codeOf = [this](uint32_t id) -> std::optional<creature_art::ScriptCode> {
		const auto found = _scripts.find(id);
		return found != _scripts.end() ? std::optional(found->second) : std::nullopt;
	};
	for (auto task = tasks.upper_bound(_lastTask); task != tasks.end(); ++task)
	{
		const auto script = codeOf(task->second.scriptId);
		if (!script.has_value())
		{
			continue;
		}
		const bool followRuns = task->second.name != k_LandControlScript;
		for (const auto species :
		     creature_art::SpeciesMade(vm.GetInstructions(), *script, _makeNatives, codeOf, followRuns, _scanned))
		{
			AskFor(species);
		}
	}
	_lastTask = tasks.rbegin()->first;
}
