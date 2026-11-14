/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DesyncCapture.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace openblack::server
{

std::vector<synced_state::EntityHash> EntitiesOfFrame(std::span<const game_socket::SyncEntity> entities)
{
	std::vector<synced_state::EntityHash> result;
	result.reserve(entities.size());
	for (const auto& entity : entities)
	{
		result.push_back({.entity = entity.entity, .hash = entity.hash, .kind = entity.kind, .position = entity.position});
	}
	std::ranges::sort(result, {}, &synced_state::EntityHash::entity);
	return result;
}

DesyncCapture::DesyncCapture(game_socket::SyncFacts facts, Hooks hooks)
    : _facts(std::move(facts))
    , _hooks(std::move(hooks))
{
}

void DesyncCapture::Facts(uint8_t seat, const game_socket::SyncFacts& facts)
{
	_playerFacts[seat] = facts;
	if (auto found = _captures.find(seat); found != _captures.end())
	{
		found->second.record.player = facts;
	}
}

void DesyncCapture::Parted(uint8_t seat, ConnectionId connection, uint32_t partTurn, uint32_t turnsDone,
                           std::span<const std::string> parts, int64_t now)
{
	if (_captured.contains(seat) || _started >= k_MostCapturesInGame)
	{
		return;
	}
	_captured.insert(seat);
	++_started;
	Capture capture;
	capture.connection = connection;
	capture.askedAt = now;
	auto& record = capture.record;
	record.seat = seat;
	record.partTurn = partTurn;
	record.captureTurn = turnsDone + k_CaptureLeadTurns;
	record.server = _facts;
	record.server.turn = partTurn;
	if (const auto facts = _playerFacts.find(seat); facts != _playerFacts.end())
	{
		record.player = facts->second;
	}
	const auto count = std::min(parts.size(), game_socket::k_MostCaptureParts);
	for (const auto& part : parts.first(count))
	{
		record.parts.push_back({.part = part});
	}
	game_socket::SyncCapture frame {.turn = record.captureTurn, .parts = {}};
	std::ranges::transform(record.parts, std::back_inserter(frame.parts), &CaptureRecord::Part::part);
	_captures.emplace(seat, std::move(capture));
	if (_hooks.send)
	{
		_hooks.send(connection, frame);
	}
}

void DesyncCapture::TurnDone(uint32_t turn)
{
	std::vector<uint8_t> done;
	for (auto& [seat, capture] : _captures)
	{
		if (capture.serverDone || capture.record.captureTurn != turn)
		{
			continue;
		}
		capture.serverDone = true;
		std::vector<std::string> names;
		std::ranges::transform(capture.record.parts, std::back_inserter(names), &CaptureRecord::Part::part);
		if (_hooks.entities)
		{
			capture.serverEntities = _hooks.entities(names);
		}
		for (auto& part : capture.record.parts)
		{
			const auto found = std::ranges::find(capture.serverEntities, part.part, &synced_state::PartEntities::part);
			part.serverEntities = found != capture.serverEntities.end() ? static_cast<uint32_t>(found->entities.size()) : 0;
		}
		if (_hooks.world)
		{
			capture.record.serverWorld = _hooks.world();
		}
		CompareBlocks(capture);
		done.push_back(seat);
	}
	// The player can send its part of the capture before the server gets to the turn
	for (const auto seat : done)
	{
		FinishIfDone(seat);
	}
}

void DesyncCapture::Blocks(uint8_t seat, const game_socket::SyncBlocks& blocks)
{
	const auto found = _captures.find(seat);
	if (found == _captures.end() || found->second.blocks.has_value() || blocks.turn != found->second.record.captureTurn)
	{
		return;
	}
	found->second.blocks = blocks;
	CompareBlocks(found->second);
	FinishIfDone(seat);
}

void DesyncCapture::CompareBlocks(Capture& capture)
{
	if (!capture.serverDone || !capture.blocks.has_value() || capture.asked.has_value())
	{
		return;
	}
	game_socket::SyncAsk ask {.turn = capture.record.captureTurn, .parts = {}};
	for (auto& part : capture.record.parts)
	{
		const auto theirs = std::ranges::find(capture.blocks->parts, part.part, &game_socket::PartBlocks::part);
		if (theirs == capture.blocks->parts.end())
		{
			continue;
		}
		part.playerEntities = theirs->entities;
		const auto own = std::ranges::find(capture.serverEntities, part.part, &synced_state::PartEntities::part);
		const auto ownBlocks =
		    own != capture.serverEntities.end() ? synced_state::BlocksOf(own->entities) : std::vector<uint64_t> {};
		auto different = synced_state::DifferentBlocks(ownBlocks, theirs->blocks);
		if (different.empty())
		{
			part.compared = true;
			continue;
		}
		ask.parts.push_back({.part = part.part, .blocks = std::move(different)});
	}
	// An ask with no part tells the client that no block differs, so that it can rejoin
	capture.entitiesDone = ask.parts.empty();
	if (_hooks.send)
	{
		_hooks.send(capture.connection, ask);
	}
	capture.asked = std::move(ask);
}

void DesyncCapture::Entities(uint8_t seat, const game_socket::SyncEntities& entities)
{
	const auto found = _captures.find(seat);
	if (found == _captures.end() || !found->second.asked.has_value() || found->second.entitiesDone ||
	    entities.turn != found->second.record.captureTurn)
	{
		return;
	}
	auto& capture = found->second;
	for (const auto& asked : capture.asked->parts)
	{
		auto part = std::ranges::find(capture.record.parts, asked.part, &CaptureRecord::Part::part);
		const auto own = std::ranges::find(capture.serverEntities, asked.part, &synced_state::PartEntities::part);
		const auto theirs = std::ranges::find(entities.parts, asked.part, &game_socket::PartEntityHashes::part);
		if (part == capture.record.parts.end() || own == capture.serverEntities.end() || theirs == entities.parts.end())
		{
			continue;
		}
		const auto serverEntries = synced_state::EntitiesOfBlocks(own->entities, asked.blocks);
		const auto playerEntries = EntitiesOfFrame(theirs->entities);
		part->differences = synced_state::DifferentEntities(serverEntries, playerEntries);
		part->compared = true;
	}
	capture.entitiesDone = true;
	FinishIfDone(seat);
}

void DesyncCapture::World(uint8_t seat, const game_socket::SyncWorld& world)
{
	const auto found = _captures.find(seat);
	if (found == _captures.end())
	{
		return;
	}
	auto& capture = found->second;
	if (capture.worldDone || capture.worldFailed || world.turn != capture.record.captureTurn)
	{
		return;
	}
	auto& bytes = capture.record.playerWorld;
	// The parts come in order, all with the same count and size, and the world is not larger than its size
	if (world.index != capture.worldParts || (capture.worldParts > 0 && world.count != capture.worldCount) ||
	    bytes.size() + world.bytes.size() > world.size || world.size > game_socket::k_MostSyncWorldBytes)
	{
		capture.worldFailed = true;
		bytes.clear();
		FinishIfDone(seat);
		return;
	}
	capture.worldCount = world.count;
	bytes.insert(bytes.end(), world.bytes.begin(), world.bytes.end());
	++capture.worldParts;
	if (capture.worldParts == capture.worldCount)
	{
		capture.worldDone = bytes.size() == world.size;
		capture.worldFailed = !capture.worldDone;
		if (capture.worldFailed)
		{
			bytes.clear();
		}
	}
	FinishIfDone(seat);
}

void DesyncCapture::Update(int64_t now)
{
	std::vector<uint8_t> late;
	for (const auto& [seat, capture] : _captures)
	{
		if (now - capture.askedAt > k_CaptureWaitMicroseconds)
		{
			late.push_back(seat);
		}
	}
	for (const auto seat : late)
	{
		Finish(seat);
	}
}

void DesyncCapture::Gone(uint8_t seat)
{
	if (const auto found = _captures.find(seat); found != _captures.end())
	{
		found->second.gone = true;
		Finish(seat);
	}
}

void DesyncCapture::FinishAll()
{
	while (!_captures.empty())
	{
		Finish(_captures.begin()->first);
	}
}

void DesyncCapture::FinishIfDone(uint8_t seat)
{
	const auto found = _captures.find(seat);
	if (found != _captures.end() && found->second.serverDone && found->second.entitiesDone &&
	    (found->second.worldDone || found->second.worldFailed))
	{
		Finish(seat);
	}
}

void DesyncCapture::Finish(uint8_t seat)
{
	auto node = _captures.extract(seat);
	if (node.empty())
	{
		return;
	}
	auto& capture = node.mapped();
	auto& record = capture.record;
	// The player's computer resyncs when it gets this frame with no parts
	if (_hooks.send && !capture.gone)
	{
		_hooks.send(capture.connection, game_socket::SyncCapture {.turn = record.captureTurn, .parts = {}});
	}
	if (!capture.serverDone)
	{
		record.missing.emplace_back("server");
	}
	if (!capture.blocks.has_value())
	{
		record.missing.emplace_back("blocks");
	}
	else if (!capture.entitiesDone)
	{
		record.missing.emplace_back("entities");
	}
	if (!capture.worldDone)
	{
		record.missing.emplace_back("world");
		record.playerWorld.clear();
	}
	if (record.player.has_value())
	{
		record.fact = online::desync_facts::FirstDifferent(record.server, *record.player);
	}
	if (_hooks.finished)
	{
		_hooks.finished(std::move(record));
	}
}

} // namespace openblack::server
