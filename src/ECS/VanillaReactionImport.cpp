/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaReactionImport.h"

#include <cstring>

#include <algorithm>
#include <ranges>
#include <span>

#include "VanillaWorld.h"

namespace openblack::ecs::vanilla_reaction_import
{

namespace
{

/// The player of a player record of the file. None for no record, or a number that is not a player.
std::optional<PlayerNames> PlayerOfRecord(const gamesave::VanillaWorld& world, uint32_t number)
{
	const auto* record = number != 0 ? world.Record(number) : nullptr;
	if (record == nullptr || !record->Is(gamesave::VanillaType::Player))
	{
		return std::nullopt;
	}
	const auto player = world.Byte(*record, "number");
	if (!player.has_value() || *player >= static_cast<uint8_t>(PlayerNames::_COUNT))
	{
		return std::nullopt;
	}
	return static_cast<PlayerNames>(*player);
}

/// The word of a named value of a record
uint32_t WordOf(const gamesave::VanillaWorld& world, const gamesave::VanillaField& field)
{
	uint32_t word = 0;
	const auto bytes = world.Bytes();
	if (field.size == sizeof(word) && field.at + sizeof(word) <= bytes.size())
	{
		std::memcpy(&word, bytes.data() + field.at, sizeof(word));
	}
	return word;
}

ImportedReaction ReadReaction(const gamesave::VanillaWorld& world, const gamesave::VanillaRecord& record)
{
	const auto takers = world.Refs(record, "reactionTakers");
	const auto firstTaken = world.Word(record, "reactionFirstTaken").value_or(0);
	return {.number = record.number,
	        .target = world.Ref(record, "reactionTarget").value_or(0),
	        .dance = world.Ref(record, "reactionDance").value_or(0),
	        .takers = {takers.begin(), takers.end()},
	        .type = world.Word(record, "reactionType").value_or(0),
	        .firstTaken = firstTaken != 0 ? std::optional(firstTaken) : std::nullopt,
	        .spreads = world.Word(record, "reactionSpreads").value_or(1) != 0,
	        .player = PlayerOfRecord(world, world.Ref(record, "reactionPlayer").value_or(0)),
	        .reach = world.Float(record, "reactionReach").value_or(0.0f)};
}

} // namespace

std::vector<ImportedReaction> ReadReactions(const gamesave::VanillaWorld& world)
{
	std::vector<uint32_t> order;
	if (!world.Records().empty() && world.Game().Is(gamesave::VanillaType::Game))
	{
		if (const auto lists = world.Fields(world.Game(), "lists"); lists.size() > k_GameReactionList)
		{
			// The list has the newest first
			for (const auto number : world.Refs(*lists.at(k_GameReactionList)) | std::views::reverse)
			{
				order.push_back(number);
			}
		}
	}
	for (const auto* record : world.OfType(gamesave::VanillaType::Reaction))
	{
		if (std::ranges::find(order, record->number) == order.end())
		{
			order.push_back(record->number);
		}
	}
	std::vector<ImportedReaction> reactions;
	for (const auto number : order)
	{
		const auto* record = world.Record(number);
		if (record != nullptr && record->Is(gamesave::VanillaType::Reaction))
		{
			reactions.push_back(ReadReaction(world, *record));
		}
	}
	return reactions;
}

std::vector<magic::villager_reaction::Memory::Entry> ReadMemory(const gamesave::VanillaWorld& world,
                                                                const gamesave::VanillaRecord& record)
{
	const auto types = world.Fields(record, "reactionDoneType");
	const auto turns = world.Fields(record, "reactionDoneTurn");
	std::vector<magic::villager_reaction::Memory::Entry> entries;
	for (size_t i = 0; i < std::min(types.size(), turns.size()); ++i)
	{
		entries.push_back({.type = WordOf(world, *types.at(i)), .turn = WordOf(world, *turns.at(i))});
	}
	// The game adds each kind after the others. When it remembers three, the oldest goes first.
	if (entries.size() > k_RememberedKinds)
	{
		entries.erase(entries.begin(), entries.end() - static_cast<std::ptrdiff_t>(k_RememberedKinds));
	}
	return entries;
}

systems::ReactionSystemInterface::Active ActiveOf(const ImportedReaction& file, const Initiator& initiator, uint32_t turn)
{
	systems::ReactionSystemInterface::Active active {
	    .source = {.initiator = initiator.entity,
	               .type = static_cast<Reaction>(file.type),
	               .player = file.player.value_or(PlayerNames::NEUTRAL),
	               .position = initiator.position,
	               .impressiveValue = initiator.impressiveValue,
	               .playerless = !file.player.has_value()},
	    .reach = file.reach,
	    .spreads = file.spreads,
	};
	// The time of the reaction counts from its first taker. The reactions system counts the age of the reaction, so the
	// reaction is as old as the turns since then.
	if (file.firstTaken.has_value())
	{
		active.age = turn >= *file.firstTaken ? turn - *file.firstTaken : 0;
		active.firstTaken = 0;
	}
	return active;
}

} // namespace openblack::ecs::vanilla_reaction_import
