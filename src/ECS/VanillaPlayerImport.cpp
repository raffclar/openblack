/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaPlayerImport.h"

#include <cstring>

#include <algorithm>
#include <bit>

#include "3D/MapCoords.h"
#include "VanillaWorld.h"

using namespace openblack;
using namespace openblack::ecs;
using gamesave::VanillaRecord;
using gamesave::VanillaType;
using gamesave::VanillaWorld;

namespace
{

std::optional<gamesave::MapCoords> PositionOf(const VanillaWorld& world, const VanillaRecord& record)
{
	const auto bytes = world.Raw(record, "position");
	if (bytes.size() != sizeof(gamesave::MapCoords))
	{
		return std::nullopt;
	}
	gamesave::MapCoords coords;
	std::memcpy(&coords, bytes.data(), sizeof(coords));
	return coords;
}

vanilla_player_import::ImportedComputerGod ReadComputerGod(const VanillaWorld& world, const VanillaRecord& record)
{
	vanilla_player_import::ImportedComputerGod god;
	god.switchedOn = world.Word(record, "switchedOn").value_or(0) != 0;
	god.paused = world.Word(record, "paused").value_or(0) != 0;
	god.scriptHold = world.Word(record, "scriptHold").value_or(0) != 0;
	god.started = world.Word(record, "started").value_or(0) != 0;
	god.handSpeed = world.Float(record, "handSpeed").value_or(god.handSpeed);
	god.hand = PositionOf(world, record).value_or(gamesave::MapCoords {});
	// The file keeps a count of requests. The game reads each request that the file has into its place.
	const auto requests = world.Raw(record, "castRequests");
	for (size_t i = 0; i < std::min(god.castRequests.size(), requests.size() / sizeof(uint32_t)); ++i)
	{
		std::memcpy(&god.castRequests.at(i), requests.data() + i * sizeof(uint32_t), sizeof(uint32_t));
	}
	const auto tree = world.Fields(record, "tree");
	const auto bytes = world.Bytes();
	for (size_t i = 0; i + 1 < tree.size(); i += 2)
	{
		uint32_t suppression = 0;
		uint32_t weight = 0;
		std::memcpy(&suppression, bytes.data() + tree.at(i)->at, sizeof(suppression));
		std::memcpy(&weight, bytes.data() + tree.at(i + 1)->at, sizeof(weight));
		god.nodes.emplace_back(std::bit_cast<float>(suppression), std::bit_cast<float>(weight));
	}
	for (const auto number : world.Refs(record, "attitudes"))
	{
		const auto* attitude = number != 0 ? world.Record(number) : nullptr;
		const auto value = attitude != nullptr && attitude->Is(VanillaType::ComputerAttitude)
		                       ? world.Float(*attitude, "attitude")
		                       : std::nullopt;
		god.attitudes.push_back(value.value_or(components::ComputerAttitudes::k_StartValue));
	}
	god.casts = world.Refs(record, "casts").size();
	god.seen = world.Refs(record, "seen").size();
	god.queue = world.Refs(record, "queue").size();
	return god;
}

} // namespace

std::vector<vanilla_player_import::ImportedPlayer> vanilla_player_import::ReadPlayers(const VanillaWorld& world)
{
	std::vector<ImportedPlayer> players;
	for (const auto& record : world.Records())
	{
		if (!record.Is(VanillaType::Player))
		{
			continue;
		}
		const auto number = world.Byte(record, "number");
		if (!number.has_value() || *number >= static_cast<uint8_t>(PlayerNames::_COUNT))
		{
			continue;
		}
		ImportedPlayer player {.player = static_cast<PlayerNames>(*number),
		                       .computerType = world.Word(record, "type").value_or(0) == k_ComputerPlayerType};
		const auto computer = world.Ref(record, "computer").value_or(0);
		const auto* god = computer != 0 ? world.Record(computer) : nullptr;
		if (god != nullptr && god->Is(VanillaType::ComputerPlayer))
		{
			player.computer = ReadComputerGod(world, *god);
		}
		players.push_back(std::move(player));
	}
	return players;
}

bool vanilla_player_import::IsUnusedGod(const ImportedPlayer& player, const ImportedComputerGod& file)
{
	const components::ComputerPlayer unused;
	if (player.computerType || file.switchedOn || file.started || file.paused || file.scriptHold ||
	    file.handSpeed != unused.speed || std::ranges::any_of(file.castRequests, [](uint32_t count) { return count != 0; }))
	{
		return false;
	}
	const auto tree = computer_mind::MakeMind();
	return file.nodes.size() == tree.nodes.size() &&
	       std::ranges::equal(file.nodes, tree.nodes,
	                          [](const std::pair<float, float>& saved, const computer_mind::Node& node) {
		                          return saved.first == node.suppression && saved.second == node.weight;
	                          });
}

components::ComputerPlayer vanilla_player_import::ComputerOf(const ImportedPlayer& player, const ImportedComputerGod& file,
                                                             float landHeight)
{
	components::ComputerPlayer computer;
	computer.switchedOn = file.switchedOn;
	computer.marked = player.computerType;
	computer.started = file.started;
	computer.speed = file.handSpeed;
	computer.hand.speed = file.handSpeed;
	// The game makes a new tree after the load, and the tree takes the suppression and the weight of each node
	computer.mind = computer_mind::MakeMind();
	for (size_t i = 0; i < std::min(computer.mind.nodes.size(), file.nodes.size()); ++i)
	{
		computer.mind.nodes.at(i).suppression = file.nodes.at(i).first;
		computer.mind.nodes.at(i).weight = file.nodes.at(i).second;
	}
	computer.mind.paused = file.paused;
	computer.mind.scriptHold = file.scriptHold;
	computer.mind.castRequested = file.castRequests;
	// The hand is where the file keeps it, and the camera of the god is at the hand between actions
	computer.hand.coords = {.x = file.hand.x, .z = file.hand.z, .altitude = file.hand.altitude};
	computer.hand.hand = {map_coords::ToMetres(file.hand.x), landHeight + file.hand.altitude,
	                      map_coords::ToMetres(file.hand.z)};
	computer.hand.camera = computer.hand.hand;
	return computer;
}

void vanilla_player_import::PutAttitudes(components::ComputerAttitudes& attitudes, const ImportedComputerGod& file)
{
	for (size_t i = 0; i < std::min(attitudes.kept.size(), file.attitudes.size()); ++i)
	{
		attitudes.kept.at(i) = file.attitudes.at(i);
	}
}
