/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "DanceSystem.h"

#include <cctype>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <DanceFile.h>
#include <entt/core/hashed_string.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
constexpr uint32_t k_TurnsPerSecond = 1000 / static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count());

/// Where a dance's file is: its name in the table is under the game's scripts folder
std::filesystem::path DanceFilePath(const std::string& name)
{
	// The table's names separate their folders with one or two backslashes
	std::vector<std::string> parts;
	std::string part;
	for (const char c : name)
	{
		if (c == '\\' || c == '/')
		{
			if (!part.empty())
			{
				parts.push_back(std::move(part));
				part.clear();
			}
			continue;
		}
		part.push_back(c);
	}
	if (!part.empty())
	{
		parts.push_back(std::move(part));
	}
	const auto lower = [](std::string text) {
		std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	};
	auto path = Locator::filesystem::value().GetPath<filesystem::Path::Scripts>();
	for (std::size_t i = 0; i < parts.size(); ++i)
	{
		if (i == 0 && lower(parts[i]) == "scripts")
		{
			continue;
		}
		path /= parts[i];
	}
	return path;
}

/// A dance's file, read once
std::shared_ptr<const dance::DanceFile> LoadDanceFile(const std::string& name)
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return nullptr;
	}
	auto& cache = Locator::resources::value().GetDanceFiles();
	const auto id = entt::hashed_string(name.c_str()).value();
	if (!cache.Contains(id))
	{
		try
		{
			cache.Load(id, resources::DanceFileLoader::FromDiskTag {}, DanceFilePath(name));
		}
		catch (const std::runtime_error& error)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The dance {} can't be loaded: {}", name, error.what());
			return nullptr;
		}
	}
	return cache.Handle(id).handle();
}

/// A villager that has finished dancing decides what to do next
void FinishedDancing(entt::entity living)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.TryGet<LivingAction>(living);
	if (action == nullptr || !registry.AllOf<Villager>(living) || !Locator::livingActionSystem::has_value())
	{
		return;
	}
	Locator::livingActionSystem::value().VillagerSetState(*action, LivingAction::Index::Top, VillagerStates::DecideWhatToDo,
	                                                      false);
}

/// Whether a thing a dance is danced about is still about: a villager stops being so as it dies
bool Available(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return false;
	}
	const auto* action = registry.TryGet<const LivingAction>(thing);
	if (action == nullptr || !registry.AllOf<Villager>(thing) || !Locator::livingActionSystem::has_value())
	{
		return true;
	}
	return Locator::livingActionSystem::value().VillagerGetState(*action, LivingAction::Index::Top) != VillagerStates::Dying;
}
} // namespace

entt::entity DanceSystem::Create(uint32_t type, const glm::vec3& place, entt::entity centre, uint32_t durationTurns,
                                 bool madeByScript)
{
	if (!Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	const auto& table = Locator::infoConstants::value().dance;
	if (type >= table.size())
	{
		return entt::null;
	}
	const auto& info = table.at(type);
	const std::string_view field(info.fileName.data(), info.fileName.size());
	const std::string fileName(field.substr(0, field.find('\0')));
	auto file = LoadDanceFile(fileName);
	if (file == nullptr)
	{
		return entt::null;
	}
	// TODO(opening): a town's dance makes footpaths to it from the town's storage pit
	return dances::Create(Locator::entitiesRegistry::value(),
	                      {.type = type,
	                       .autostart = info.startsAutomatically != 0,
	                       .place = map_coords::FromMetres(glm::xz(place)),
	                       .centre = centre,
	                       .durationTurns = durationTurns,
	                       .madeByScript = madeByScript},
	                      std::move(file));
}

void DanceSystem::Destroy(entt::entity dance)
{
	dances::Destroy(Locator::entitiesRegistry::value(), dance, &FinishedDancing);
}

void DanceSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> all;
	registry.Each<const Dance>([&all](entt::entity entity, const Dance&) { all.push_back(entity); });
	const dances::TurnContext context {.turn = Locator::time::value().GetTurn(),
	                                   .turnsPerSecond = k_TurnsPerSecond,
	                                   .available = &Available,
	                                   .finished = &FinishedDancing};
	for (const auto entity : all)
	{
		if (dances::IsDance(registry, entity))
		{
			dances::ProcessTurn(registry, entity, context);
		}
	}
}
