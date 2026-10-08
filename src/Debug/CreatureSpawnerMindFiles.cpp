/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>

#include <MindFile.h>
#include <fmt/format.h>
#include <imgui.h>

#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindFileBody.h"
#include "CreatureSpawner.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::creature_spawner;

void CreatureSpawner::DrawMindFiles(bool intoSelected) noexcept
{
	if (!Locator::filesystem::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	if (!_mindFilesListed || ImGui::SmallButton("List again"))
	{
		// The game's own minds, and the creatures it saved between lands
		_mindFiles.clear();
		const auto& fileSystem = Locator::filesystem::value();
		const auto folder = fileSystem.GetPath<filesystem::Path::CreatureMind>(true);
		if (fileSystem.Exists(folder))
		{
			fileSystem.Iterate(folder, false, [this](const std::filesystem::path& file) {
				if (IsMindFileName(file.filename().string()))
				{
					_mindFiles.push_back(file);
				}
			});
		}
		std::ranges::sort(_mindFiles);
		_mindFilesListed = true;
	}
	if (!ImGui::TreeNode(intoSelected ? "load" : "spawn", "The game's mind files: %zu", _mindFiles.size()))
	{
		return;
	}
	for (const auto& path : _mindFiles)
	{
		ImGui::PushID(path.generic_string().c_str());
		if (ImGui::SmallButton(intoSelected ? "Load" : "Spawn from"))
		{
			UseMindFile(path, intoSelected);
		}
		ImGui::SameLine();
		ImGui::TextUnformatted(path.filename().string().c_str());
		ImGui::PopID();
	}
	ImGui::TreePop();
}

void CreatureSpawner::UseMindFile(const std::filesystem::path& path, bool intoSelected) noexcept
{
	const auto name = path.filename().string();
	auto& minds = Locator::resources::value().GetCreatureMinds();
	const auto id = minds.Load(name, resources::CreatureMindLoader::FromDiskTag {}, path).first->first;
	const auto handle = minds.Handle(id);
	if (!handle || !handle->Loaded())
	{
		_lastMindFile = fmt::format("Couldn't load {}: {}", name,
		                            handle ? creaturemind::ResultToStr(handle->result) : std::string("not loaded"));
		return;
	}
	const auto& data = handle->data;
	const auto body = creature_mind_body::FromMindFile(data);
	const auto who = fmt::format("\"{}\", a version {} mind{}", Narrow(data.name), data.version,
	                             body.has_value() ? fmt::format(" of a {}", SpeciesName(body->species)) : "");

	if (intoSelected)
	{
		if (!_selected.has_value() || !Locator::creatureMindSystem::has_value())
		{
			_lastMindFile = "No creature is selected to load the mind into";
			return;
		}
		Locator::creatureMindSystem::value().LoadMind(*_selected, std::make_shared<const creaturemind::MindFileData>(data));
		_lastMindFile = fmt::format("Loaded {} from {}", who, name);
		return;
	}

	// Spawning from the file: the creature it describes, as far as the file's version keeps it
	if (!body.has_value())
	{
		_lastMindFile = fmt::format("Couldn't spawn from {}: its species isn't one the game knows", name);
		return;
	}
	_species = body->species;
	UseSpeciesDefaults();
	_defaultsFor = _species;
	_alignment = body->alignment.value_or(_alignment);
	_strength = body->strength;
	_scale = body->size.value_or(_scale);
	_spawnTattoos = body->tattoos;
	_spawnMind = id;
	_spawnMindName = fmt::format("{} from {}", who, name);
	_placing = true;
	_commanding = false;
	_clicked = false;
	_lastMindFile = fmt::format("Spawning {}: left click on the land to place it", who);
}

void CreatureSpawner::DrawSpawnMind() noexcept
{
	ImGui::SeparatorText("Mind");
	if (_spawnMind != 0)
	{
		ImGui::TextWrapped("Spawning %s", _spawnMindName.c_str());
		if (ImGui::Button("Fresh mind"))
		{
			_spawnMind = 0;
			_spawnTattoos.reset();
			_spawnMindName.clear();
		}
	}
	else
	{
		ImGui::TextUnformatted("Spawning with a fresh mind");
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("A mind file gives new creatures a saved creature's species, name, alignment, strength, size, "
		                  "tattoos and what it has learnt");
	}
	DrawMindFiles(false);
	if (!_lastMindFile.empty())
	{
		ImGui::TextWrapped("%s", _lastMindFile.c_str());
	}
}
