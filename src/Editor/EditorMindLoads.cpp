/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditorMindLoads.h"

#include <algorithm>

#include <MindFile.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "Editor/EditorCreatureEdits.h"
#include "Locator.h"

namespace openblack::editor::mind_loads
{

namespace
{
constexpr uint64_t k_HashStart = 14695981039346656037ULL;
constexpr uint64_t k_HashPrime = 1099511628211ULL;
} // namespace

uint64_t HashOf(std::span<const uint8_t> bytes) noexcept
{
	uint64_t hash = k_HashStart;
	for (const auto byte : bytes)
	{
		hash ^= byte;
		hash *= k_HashPrime;
	}
	return hash;
}

std::pair<MindFileStore::Added, uint64_t> MindFileStore::Add(std::vector<uint8_t> bytes)
{
	const auto hash = HashOf(bytes);
	if (!Find(hash).empty())
	{
		return {Added::Known, hash};
	}
	if (bytes.empty() || _files.size() >= k_MostKeptFiles)
	{
		return {Added::Refused, hash};
	}
	_files.emplace_back(hash, std::move(bytes));
	return {Added::New, hash};
}

std::span<const uint8_t> MindFileStore::Find(uint64_t hash) const noexcept
{
	const auto found = std::ranges::find(_files, hash, &std::pair<uint64_t, std::vector<uint8_t>>::first);
	return found != _files.end() ? std::span<const uint8_t>(found->second) : std::span<const uint8_t> {};
}

std::shared_ptr<const creaturemind::MindFileData> ReadMind(std::span<const uint8_t> bytes)
{
	if (bytes.empty())
	{
		return nullptr;
	}
	auto data = std::make_shared<creaturemind::MindFileData>();
	// The creature tools read a file that they open with its names
	if (creaturemind::Read(bytes, *data, creaturemind::MindNames::Always) != creaturemind::MindResult::Success)
	{
		return nullptr;
	}
	return data;
}

std::optional<std::string_view> MindLoadRefusal(const player_commands::EditorMindLoad& load, bool kept) noexcept
{
	if (!kept)
	{
		return "no mind file with this hash came before the command";
	}
	if (load.use == player_commands::EditorMindLoad::Use::Spawn)
	{
		return CreatureSpawnRefusal(load.spawn);
	}
	if (load.creature == entt::null)
	{
		return "the command names no creature";
	}
	return std::nullopt;
}

MindLoadResult ApplyMindLoad(const player_commands::EditorMindLoad& load, std::span<const uint8_t> file,
                             const MindLoadWorld& world)
{
	if (const auto refusal = MindLoadRefusal(load, !file.empty()))
	{
		return {.creature = entt::null, .refusal = *refusal};
	}
	auto mind = ReadMind(file);
	if (mind == nullptr)
	{
		return {.creature = entt::null, .refusal = "the mind file does not read"};
	}
	entt::entity creature = entt::null;
	if (load.use == player_commands::EditorMindLoad::Use::Spawn)
	{
		creature = world.spawn(load);
		if (creature == entt::null)
		{
			return {.creature = entt::null, .refusal = "the world refused the new creature"};
		}
	}
	else
	{
		if (!world.isCreature(load.creature))
		{
			return {.creature = entt::null, .refusal = "the thing is not a creature"};
		}
		creature = load.creature;
	}
	world.loadMind(creature, std::move(mind));
	return {.creature = creature, .refusal = {}};
}

MindLoadResult RunMindLoad(const player_commands::EditorMindLoad& load, std::span<const uint8_t> file)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::creatureMindSystem::has_value())
	{
		return {.creature = entt::null, .refusal = "the world has no creature minds"};
	}
	const MindLoadWorld world {
	    .isCreature =
	        [](entt::entity entity) {
		        const auto& registry = Locator::entitiesRegistry::value();
		        return registry.Valid(entity) && registry.AllOf<ecs::components::Creature>(entity) &&
		               registry.AllOf<ecs::components::CreatureMindState>(entity);
	        },
	    .spawn = [](const player_commands::EditorMindLoad& command) { return ApplyCreatureSpawn(command.spawn); },
	    .loadMind =
	        [](entt::entity creature, std::shared_ptr<const creaturemind::MindFileData> mind) {
		        // A new creature takes up the mind on its first turn of thought, as in a game alone
		        Locator::creatureMindSystem::value().LoadMind(creature, std::move(mind));
	        },
	};
	return ApplyMindLoad(load, file, world);
}

bool SendMindLoad(std::span<const uint8_t> file, player_commands::EditorMindLoad load)
{
	if (file.empty() || !Locator::networkGame::has_value() || !Locator::playerCommandSystem::has_value())
	{
		return false;
	}
	// The bytes go first on the game socket. The server sends them to every computer before the turn of the command.
	if (!Locator::networkGame::value().SendEditorMindFile({file.begin(), file.end()}))
	{
		return false;
	}
	load.file = HashOf(file);
	Locator::playerCommandSystem::value().Submit(load);
	return true;
}

} // namespace openblack::editor::mind_loads
