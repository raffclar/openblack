/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>
#include <utility>

#include <nlohmann/json.hpp>

#include "GameSocket.h"
#include "JoinTicket.h"

namespace openblack::game_socket
{
namespace
{

using Json = nlohmann::json;

constexpr std::string_view k_Join = "room.join";
constexpr std::string_view k_Resume = "room.resume";
constexpr std::string_view k_Ready = "room.ready";
constexpr std::string_view k_Leave = "room.leave";
constexpr std::string_view k_Joined = "room.joined";
constexpr std::string_view k_Refused = "room.refused";
constexpr std::string_view k_Start = "game.start";
constexpr std::string_view k_Resumed = "room.resumed";
constexpr std::string_view k_PlayerState = "player.state";
constexpr std::string_view k_StartQuery = "start.query";
constexpr std::string_view k_StartReply = "start.reply";
constexpr std::string_view k_StartCancel = "start.cancel";
constexpr std::string_view k_StartStop = "start.stop";
constexpr std::string_view k_StartTransfer = "start.transfer";
constexpr std::string_view k_StartCreature = "start.creature";
constexpr std::string_view k_StopRequest = "game.stop_request";
constexpr std::string_view k_GameStop = "game.stop";
constexpr std::string_view k_RoomChat = "room.chat";
constexpr std::string_view k_ObserverLeft = "observer.left";
constexpr std::string_view k_SyncParts = "sync.parts";
constexpr std::string_view k_Loaded = "game.loaded";
constexpr std::string_view k_LoadWait = "game.wait";
constexpr std::string_view k_DataPrint = "data.print";
constexpr std::string_view k_SyncFacts = "sync.facts";
constexpr std::string_view k_SyncCapture = "sync.capture";
constexpr std::string_view k_SyncBlocks = "sync.blocks";
constexpr std::string_view k_SyncAsk = "sync.ask";
constexpr std::string_view k_SyncEntities = "sync.entities";
constexpr std::string_view k_SyncWorld = "sync.world";
/// The most bytes of the hints of a copy of the game (the patch and the language)
constexpr size_t k_MostHintBytes = 16;
/// The most bytes of the words of the file in a refusal "data"
constexpr size_t k_MostRefusedFileBytes = 1024;
/// The most bytes of the path of the file in a refusal "data"
constexpr size_t k_MostRefusedPathBytes = 260;
/// The most blocks of one part that a "sync.ask" frame names
constexpr size_t k_MostAskBlocks = 64;
/// The longest wait for the loads that a frame can give: the 6 hours of a lobby game
constexpr uint64_t k_MostLoadWaitSeconds = 6ULL * 60 * 60;

/// The most god slots that a frame can name
constexpr uint64_t k_MostSlots = 32;
/// The highest number of an observer, as the turn codec of the game takes it
constexpr uint64_t k_MostObserverNumber = 127;
/// The number of the winning conditions of a network game
constexpr uint64_t k_ConditionCount = 15;
/// The largest value of a winning condition: the largest whole number that the game keeps for it
constexpr uint64_t k_MostConditionValue = std::numeric_limits<int32_t>::max();

std::string Dump(const Json& value)
{
	// Text that is not valid UTF-8 is replaced, because the library must not throw
	return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}

const Json* Member(const Json& object, std::string_view key)
{
	const auto found = object.find(key);
	return found != object.end() ? &*found : nullptr;
}

bool ReadString(const Json& object, std::string_view key, std::string& out, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!member->is_string())
	{
		return false;
	}
	out = member->get_ref<const std::string&>();
	return true;
}

bool ReadUnsigned(const Json& object, std::string_view key, uint64_t most, uint64_t& out, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!member->is_number_unsigned() && !(member->is_number_integer() && member->get<int64_t>() >= 0))
	{
		return false;
	}
	const auto value = member->get<uint64_t>();
	if (value > most)
	{
		return false;
	}
	out = value;
	return true;
}

/// The parts of a "sync.parts" frame: an array of [name, hash] pairs, with the hash as 16 hexadecimal digits
bool ReadParts(const Json& object, std::vector<std::pair<std::string, uint64_t>>& out)
{
	const auto* member = Member(object, "parts");
	if (member == nullptr || !member->is_array() || member->size() > k_MostSyncParts)
	{
		return false;
	}
	for (const auto& entry : *member)
	{
		if (!entry.is_array() || entry.size() != 2 || !entry[0].is_string() || !entry[1].is_string())
		{
			return false;
		}
		const auto& name = entry[0].get_ref<const std::string&>();
		const auto& hex = entry[1].get_ref<const std::string&>();
		if (name.size() > k_MostSyncPartNameBytes || hex.empty() || hex.size() > 16)
		{
			return false;
		}
		uint64_t value = 0;
		for (const char c : hex)
		{
			uint64_t digit = 0;
			if (c >= '0' && c <= '9')
			{
				digit = static_cast<uint64_t>(c - '0');
			}
			else if (c >= 'a' && c <= 'f')
			{
				digit = static_cast<uint64_t>(c - 'a' + 10);
			}
			else
			{
				return false;
			}
			value = (value << 4U) | digit;
		}
		out.emplace_back(name, value);
	}
	return true;
}

/// A hash as 16 hexadecimal digits, which JSON keeps with no loss
std::string HexOf(uint64_t value)
{
	constexpr std::string_view k_Digits = "0123456789abcdef";
	std::string text(16, '0');
	for (size_t i = 0; i < text.size(); ++i)
	{
		text[text.size() - 1 - i] = k_Digits[(value >> (4 * i)) & 0xFU];
	}
	return text;
}

bool ReadBool(const Json& object, std::string_view key, bool& out, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!member->is_boolean())
	{
		return false;
	}
	out = member->get<bool>();
	return true;
}

/// A hash from 1 to 16 hexadecimal digits in lower case
bool ParseHex(std::string_view hex, uint64_t& out)
{
	if (hex.empty() || hex.size() > 16)
	{
		return false;
	}
	uint64_t value = 0;
	for (const char c : hex)
	{
		uint64_t digit = 0;
		if (c >= '0' && c <= '9')
		{
			digit = static_cast<uint64_t>(c - '0');
		}
		else if (c >= 'a' && c <= 'f')
		{
			digit = static_cast<uint64_t>(c - 'a' + 10);
		}
		else
		{
			return false;
		}
		value = (value << 4U) | digit;
	}
	out = value;
	return true;
}

bool ReadHex(const Json& object, std::string_view key, uint64_t& out, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	return member->is_string() && ParseHex(member->get_ref<const std::string&>(), out);
}

/// A text of at most a count of bytes
bool ReadShort(const Json& object, std::string_view key, std::string& out, size_t most, bool required = false)
{
	return ReadString(object, key, out, required) && out.size() <= most;
}

/// An array of texts, each of at most a count of bytes
bool ReadNames(const Json& object, std::string_view key, std::vector<std::string>& out, size_t mostNames, size_t mostBytes,
               bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!member->is_array() || member->size() > mostNames)
	{
		return false;
	}
	for (const auto& entry : *member)
	{
		if (!entry.is_string() || entry.get_ref<const std::string&>().size() > mostBytes)
		{
			return false;
		}
		out.push_back(entry.get<std::string>());
	}
	return true;
}

bool ReadTurn(const Json& object, uint32_t& out)
{
	uint64_t turn = 0;
	if (!ReadUnsigned(object, "turn", std::numeric_limits<uint32_t>::max(), turn))
	{
		return false;
	}
	out = static_cast<uint32_t>(turn);
	return true;
}

/// The array of parts of a frame of a capture: each part an object that the reader reads
template <typename Part, typename ReadPart>
bool ReadCaptureParts(const Json& object, std::vector<Part>& out, const ReadPart& read)
{
	const auto* member = Member(object, "parts");
	if (member == nullptr || !member->is_array() || member->size() > k_MostCaptureParts)
	{
		return false;
	}
	for (const auto& entry : *member)
	{
		Part part;
		if (!entry.is_object() || !ReadShort(entry, "part", part.part, k_MostSyncPartNameBytes, true) || !read(entry, part))
		{
			return false;
		}
		out.push_back(std::move(part));
	}
	return true;
}

/// Reads an alignment, a number from -1 to 1 that the frame holds as a float. A frame with no alignment gives 0.
bool ReadAlignment(const Json& object, std::string_view key, float& out)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		out = 0.0f;
		return true;
	}
	if (!member->is_number())
	{
		return false;
	}
	const auto value = member->get<double>();
	if (!(value >= -1.0 && value <= 1.0))
	{
		return false;
	}
	out = static_cast<float>(value);
	return true;
}

constexpr std::string_view StateName(PlayerState::State state) noexcept
{
	switch (state)
	{
	case PlayerState::State::Away:
		return "away";
	case PlayerState::State::Back:
		return "back";
	case PlayerState::State::Left:
		return "left";
	}
	return "away";
}

std::optional<PlayerState::State> StateFrom(std::string_view name) noexcept
{
	for (const auto state : {PlayerState::State::Away, PlayerState::State::Back, PlayerState::State::Left})
	{
		if (StateName(state) == name)
		{
			return state;
		}
	}
	return std::nullopt;
}

/// Reads the members of a start: the land, the seed and the start of the server's world
bool ReadStart(const Json& object, Start& start)
{
	constexpr auto k_MostNumber = std::numeric_limits<uint32_t>::max();
	uint64_t seed = 0;
	uint64_t turn = 0;
	if (!ReadString(object, "land", start.land, false) || !ReadBool(object, "testbed", start.testbed, false) ||
	    !ReadUnsigned(object, "seed", k_MostNumber, seed) || !ReadUnsigned(object, "turn", k_MostNumber, turn, false) ||
	    !ReadString(object, "opening", start.opening, false) || !ReadBool(object, "gods_ring_map", start.godsRingMap, false))
	{
		return false;
	}
	start.seed = static_cast<uint32_t>(seed);
	start.turn = static_cast<uint32_t>(turn);
	if (!ReadBool(object, "all_computer_gods", start.allComputerGods, false))
	{
		return false;
	}
	if (const auto* aiSeed = Member(object, "ai_seed"); aiSeed != nullptr && !aiSeed->is_null())
	{
		uint64_t value = 0;
		if (!ReadUnsigned(object, "ai_seed", k_MostNumber, value))
		{
			return false;
		}
		start.aiSeed = static_cast<uint32_t>(value);
	}
	if (const auto* timeLimit = Member(object, "time_limit"); timeLimit != nullptr && !timeLimit->is_null())
	{
		uint64_t value = 0;
		if (!ReadUnsigned(object, "time_limit", k_MostNumber, value))
		{
			return false;
		}
		start.timeLimit = static_cast<uint32_t>(value);
	}
	if (const auto* scenario = Member(object, "scenario"); scenario != nullptr && !scenario->is_null())
	{
		if (!scenario->is_string() || scenario->get<std::string>().empty())
		{
			return false;
		}
		start.scenario = scenario->get<std::string>();
	}
	if (const auto* conditions = Member(object, "conditions"); conditions != nullptr && !conditions->is_null())
	{
		if (!conditions->is_array() || conditions->size() > k_ConditionCount)
		{
			return false;
		}
		for (const auto& each : *conditions)
		{
			uint64_t condition = 0;
			uint64_t value = 0;
			if (!each.is_object() || !ReadUnsigned(each, "condition", k_ConditionCount - 1, condition) ||
			    !ReadUnsigned(each, "value", k_MostConditionValue, value))
			{
				return false;
			}
			start.conditions.push_back({.condition = static_cast<uint8_t>(condition), .value = static_cast<uint32_t>(value)});
		}
	}
	if (const auto* names = Member(object, "names"); names != nullptr && !names->is_null())
	{
		if (!names->is_array() || names->size() > k_MostSlots)
		{
			return false;
		}
		for (const auto& name : *names)
		{
			if (!name.is_string())
			{
				return false;
			}
			start.names.push_back(name.get<std::string>());
		}
	}
	// Each god of the list has a slot and a name
	const auto gods = object.find("gods");
	if (gods == object.end())
	{
		return true;
	}
	if (!gods->is_array() || gods->size() > k_MostSlots)
	{
		return false;
	}
	constexpr uint64_t k_MostSlot = std::numeric_limits<uint8_t>::max();
	constexpr auto k_MostAccount = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
	for (const auto& entry : *gods)
	{
		God god;
		uint64_t slot = 0;
		if (!entry.is_object() || !ReadUnsigned(entry, "slot", k_MostSlot, slot) || !ReadString(entry, "name", god.name) ||
		    !ReadBool(entry, "staff", god.staff, false))
		{
			return false;
		}
		if (const auto* member = Member(entry, "account"); member != nullptr && !member->is_null())
		{
			uint64_t value = 0;
			if (!ReadUnsigned(entry, "account", k_MostAccount, value))
			{
				return false;
			}
			god.account = static_cast<int64_t>(value);
		}
		god.slot = static_cast<uint8_t>(slot);
		start.gods.push_back(std::move(god));
	}
	return true;
}

/// Writes the members of a start. The frame leaves out an empty opening, a false map for 32 gods and the tool members that are
/// not set.
void WriteStart(const Start& start, Json& object)
{
	object["land"] = start.land;
	object["testbed"] = start.testbed;
	object["seed"] = start.seed;
	object["turn"] = start.turn;
	if (!start.opening.empty())
	{
		object["opening"] = start.opening;
	}
	if (start.godsRingMap)
	{
		object["gods_ring_map"] = true;
	}
	if (start.allComputerGods)
	{
		object["all_computer_gods"] = true;
	}
	if (start.aiSeed.has_value())
	{
		object["ai_seed"] = *start.aiSeed;
	}
	if (start.timeLimit.has_value())
	{
		object["time_limit"] = *start.timeLimit;
	}
	if (start.scenario.has_value())
	{
		object["scenario"] = *start.scenario;
	}
	if (!start.conditions.empty())
	{
		auto& conditions = object["conditions"] = Json::array();
		for (const auto& each : start.conditions)
		{
			conditions.push_back({{"condition", each.condition}, {"value", each.value}});
		}
	}
	if (!start.names.empty())
	{
		object["names"] = start.names;
	}
	if (!start.gods.empty())
	{
		auto& gods = object["gods"] = Json::array();
		for (const auto& god : start.gods)
		{
			auto entry = Json {{"slot", god.slot}, {"name", god.name}};
			if (god.account.has_value())
			{
				entry["account"] = *god.account;
			}
			if (god.staff)
			{
				entry["staff"] = true;
			}
			gods.push_back(std::move(entry));
		}
	}
}

std::optional<Frame> ParseObject(const Json& object, std::string_view type)
{
	constexpr auto k_MostTurn = std::numeric_limits<uint32_t>::max();
	if (type == k_Join)
	{
		Join join;
		if (!ReadString(object, "ticket", join.ticket, false) ||
		    !ReadString(object, "game_password", join.gamePassword, false) || !ReadString(object, "name", join.name, false) ||
		    !ReadString(object, "build", join.build, false) || !ReadString(object, "map", join.map, false))
		{
			return std::nullopt;
		}
		return join;
	}
	if (type == k_Resume)
	{
		Resume resume;
		uint64_t turn = 0;
		if (!ReadString(object, "session", resume.session) || !ReadUnsigned(object, "turn", k_MostTurn, turn) ||
		    !ReadBool(object, "world", resume.world, false))
		{
			return std::nullopt;
		}
		resume.turn = static_cast<uint32_t>(turn);
		return resume;
	}
	if (type == k_Ready)
	{
		Ready ready;
		if (!ReadBool(object, "ready", ready.ready, false))
		{
			return std::nullopt;
		}
		return ready;
	}
	if (type == k_Leave)
	{
		return Leave {};
	}
	if (type == k_Joined)
	{
		Joined joined;
		std::string role;
		uint64_t slot = 0;
		const auto* slotMember = Member(object, "slot");
		if (!ReadString(object, "role", role) || !ReadString(object, "session", joined.session) ||
		    !ReadString(object, "name", joined.name, false))
		{
			return std::nullopt;
		}
		const auto known = RoleFrom(role);
		if (!known.has_value())
		{
			return std::nullopt;
		}
		joined.role = *known;
		if (slotMember != nullptr && !slotMember->is_null())
		{
			if (!ReadUnsigned(object, "slot", k_MostSlots - 1, slot))
			{
				return std::nullopt;
			}
			joined.slot = static_cast<uint8_t>(slot);
		}
		// A god always has a slot, and an observer never has one
		if (joined.slot.has_value() != (joined.role == Role::God))
		{
			return std::nullopt;
		}
		// Only an observer has a number of an observer
		uint64_t observer = 0;
		if (!ReadUnsigned(object, "observer", k_MostObserverNumber, observer, false) ||
		    (observer != 0 && joined.role != Role::Observer))
		{
			return std::nullopt;
		}
		joined.observer = static_cast<uint8_t>(observer);
		if (!ReadBool(object, "staff", joined.staff, false) || !ReadBool(object, "capture", joined.capture, false) ||
		    !ReadNames(object, "data_files", joined.dataFiles, k_MostDataFiles, k_MostDataPathBytes, false))
		{
			return std::nullopt;
		}
		if (const auto* startMember = Member(object, "start"); startMember != nullptr && !startMember->is_null())
		{
			Start start;
			if (!startMember->is_object() || !ReadStart(*startMember, start))
			{
				return std::nullopt;
			}
			joined.start = std::move(start);
		}
		return joined;
	}
	if (type == k_Refused)
	{
		Refused refused;
		if (!ReadString(object, "reason", refused.reason) || !ReadString(object, "build", refused.build, false) ||
		    !ReadShort(object, "file", refused.file, k_MostRefusedFileBytes) ||
		    !ReadShort(object, "path", refused.path, k_MostRefusedPathBytes))
		{
			return std::nullopt;
		}
		return refused;
	}
	if (type == k_Start)
	{
		Start start;
		if (!ReadStart(object, start))
		{
			return std::nullopt;
		}
		return start;
	}
	if (type == k_Resumed)
	{
		uint64_t turn = 0;
		bool world = false;
		if (!ReadUnsigned(object, "turn", k_MostTurn, turn) || !ReadBool(object, "world", world, false))
		{
			return std::nullopt;
		}
		return Resumed {.turn = static_cast<uint32_t>(turn), .world = world};
	}
	if (type == k_DataPrint)
	{
		DataPrint print;
		const auto* files = Member(object, "files");
		if (files == nullptr || !files->is_array() || files->size() > k_MostDataFiles ||
		    !ReadShort(object, "patch", print.patch, k_MostHintBytes) ||
		    !ReadShort(object, "language", print.language, k_MostHintBytes))
		{
			return std::nullopt;
		}
		for (const auto& entry : *files)
		{
			DataFile file;
			if (!entry.is_array() || entry.size() != 4 || !entry[0].is_string() || !entry[1].is_boolean() ||
			    !entry[2].is_number_unsigned() || !entry[3].is_string())
			{
				return std::nullopt;
			}
			file.path = entry[0].get<std::string>();
			file.present = entry[1].get<bool>();
			file.size = entry[2].get<uint64_t>();
			if (file.path.empty() || file.path.size() > k_MostDataPathBytes ||
			    !ParseHex(entry[3].get_ref<const std::string&>(), file.hash))
			{
				return std::nullopt;
			}
			print.files.push_back(std::move(file));
		}
		return print;
	}
	if (type == k_SyncFacts)
	{
		SyncFacts facts;
		uint64_t seed = 0;
		if (!ReadTurn(object, facts.turn) || !ReadShort(object, "build", facts.build, k_MostFactBytes) ||
		    !ReadShort(object, "land", facts.land, k_MostFactBytes) ||
		    !ReadShort(object, "land_sha256", facts.landSha256, k_MostFactBytes) ||
		    !ReadShort(object, "map", facts.map, k_MostFactBytes) ||
		    !ReadUnsigned(object, "seed", std::numeric_limits<uint32_t>::max(), seed, false) ||
		    !ReadHex(object, "data", facts.data, false) || !ReadShort(object, "patch", facts.patch, k_MostHintBytes) ||
		    !ReadShort(object, "language", facts.language, k_MostHintBytes))
		{
			return std::nullopt;
		}
		facts.seed = static_cast<uint32_t>(seed);
		return facts;
	}
	if (type == k_SyncCapture)
	{
		SyncCapture capture;
		if (!ReadTurn(object, capture.turn) ||
		    !ReadNames(object, "parts", capture.parts, k_MostCaptureParts, k_MostSyncPartNameBytes))
		{
			return std::nullopt;
		}
		return capture;
	}
	if (type == k_SyncBlocks)
	{
		SyncBlocks blocks;
		const auto read = [](const Json& entry, PartBlocks& part) {
			uint64_t entities = 0;
			const auto* list = Member(entry, "blocks");
			if (!ReadUnsigned(entry, "entities", std::numeric_limits<uint32_t>::max(), entities) || list == nullptr ||
			    !list->is_array() || list->size() > k_MostSyncBlocks)
			{
				return false;
			}
			part.entities = static_cast<uint32_t>(entities);
			for (const auto& block : *list)
			{
				uint64_t value = 0;
				if (!block.is_string() || !ParseHex(block.get_ref<const std::string&>(), value))
				{
					return false;
				}
				part.blocks.push_back(value);
			}
			return true;
		};
		if (!ReadTurn(object, blocks.turn) || !ReadCaptureParts(object, blocks.parts, read))
		{
			return std::nullopt;
		}
		return blocks;
	}
	if (type == k_SyncAsk)
	{
		SyncAsk ask;
		const auto read = [](const Json& entry, PartAsk& part) {
			const auto* list = Member(entry, "blocks");
			if (list == nullptr || !list->is_array() || list->size() > k_MostAskBlocks)
			{
				return false;
			}
			for (const auto& block : *list)
			{
				if (!block.is_number_unsigned() || block.get<uint64_t>() >= k_MostSyncBlocks)
				{
					return false;
				}
				part.blocks.push_back(block.get<uint32_t>());
			}
			return true;
		};
		if (!ReadTurn(object, ask.turn) || !ReadCaptureParts(object, ask.parts, read))
		{
			return std::nullopt;
		}
		return ask;
	}
	if (type == k_SyncEntities)
	{
		SyncEntities entities;
		const auto read = [](const Json& entry, PartEntityHashes& part) {
			const auto* list = Member(entry, "entities");
			if (list == nullptr || !list->is_array() || list->size() > k_MostSyncEntities)
			{
				return false;
			}
			for (const auto& item : *list)
			{
				SyncEntity entity;
				if (!item.is_array() || item.size() != 4 || !item[0].is_number_unsigned() ||
				    item[0].get<uint64_t>() > std::numeric_limits<uint32_t>::max() || !item[1].is_string() ||
				    !ParseHex(item[1].get_ref<const std::string&>(), entity.hash) || !item[2].is_string() ||
				    item[2].get_ref<const std::string&>().size() > k_MostSyncPartNameBytes)
				{
					return false;
				}
				entity.entity = item[0].get<uint32_t>();
				entity.kind = item[2].get<std::string>();
				const auto& place = item[3];
				if (place.is_array() && place.size() == 3 &&
				    std::ranges::all_of(place, [](const Json& v) { return v.is_number(); }))
				{
					entity.position =
					    std::array<float, 3> {place[0].get<float>(), place[1].get<float>(), place[2].get<float>()};
				}
				else if (!place.is_null())
				{
					return false;
				}
				part.entities.push_back(std::move(entity));
			}
			return true;
		};
		if (!ReadTurn(object, entities.turn) || !ReadCaptureParts(object, entities.parts, read))
		{
			return std::nullopt;
		}
		return entities;
	}
	if (type == k_SyncWorld)
	{
		SyncWorld world;
		uint64_t index = 0;
		uint64_t count = 0;
		uint64_t size = 0;
		std::string data;
		constexpr auto k_MostChunks = (k_MostSyncWorldBytes + k_MostSyncWorldChunkBytes - 1) / k_MostSyncWorldChunkBytes;
		// Base64 holds 3 bytes in 4 characters
		constexpr auto k_MostDataCharacters = (k_MostSyncWorldChunkBytes + 2) / 3 * 4;
		if (!ReadTurn(object, world.turn) || !ReadUnsigned(object, "index", k_MostChunks - 1, index) ||
		    !ReadUnsigned(object, "count", k_MostChunks, count) || !ReadUnsigned(object, "size", k_MostSyncWorldBytes, size) ||
		    !ReadShort(object, "data", data, k_MostDataCharacters, true) || index >= count)
		{
			return std::nullopt;
		}
		auto bytes = join_ticket::DecodeBase64Url(data);
		if (!bytes.has_value() || bytes->size() > k_MostSyncWorldChunkBytes)
		{
			return std::nullopt;
		}
		world.index = static_cast<uint32_t>(index);
		world.count = static_cast<uint32_t>(count);
		world.size = static_cast<uint32_t>(size);
		world.bytes = std::move(*bytes);
		return world;
	}
	if (type == k_SyncParts)
	{
		SyncParts parts;
		uint64_t turn = 0;
		if (!ReadUnsigned(object, "turn", k_MostTurn, turn) || !ReadParts(object, parts.parts))
		{
			return std::nullopt;
		}
		parts.turn = static_cast<uint32_t>(turn);
		return parts;
	}
	if (type == k_ObserverLeft)
	{
		ObserverLeft left;
		if (!ReadString(object, "name", left.name))
		{
			return std::nullopt;
		}
		return left;
	}
	if (type == k_PlayerState)
	{
		uint64_t slot = 0;
		std::string state;
		if (!ReadUnsigned(object, "slot", k_MostSlots - 1, slot) || !ReadString(object, "state", state))
		{
			return std::nullopt;
		}
		const auto known = StateFrom(state);
		if (!known.has_value())
		{
			return std::nullopt;
		}
		return PlayerState {.slot = static_cast<uint8_t>(slot), .state = *known};
	}
	if (type == k_StartQuery)
	{
		StartQuery query;
		if (!ReadString(object, "land", query.land, false) || !ReadBool(object, "testbed", query.testbed, false) ||
		    !ReadUnsigned(object, "checksum", std::numeric_limits<uint64_t>::max(), query.checksum))
		{
			return std::nullopt;
		}
		return query;
	}
	if (type == k_StartReply)
	{
		StartReply reply;
		if (!ReadBool(object, "have", reply.have) ||
		    !ReadUnsigned(object, "checksum", std::numeric_limits<uint64_t>::max(), reply.checksum, false))
		{
			return std::nullopt;
		}
		return reply;
	}
	if (type == k_StartCancel)
	{
		return StartCancel {};
	}
	if (type == k_StartStop)
	{
		return StartStop {};
	}
	if (type == k_StartTransfer)
	{
		return StartTransfer {};
	}
	if (type == k_StartCreature)
	{
		StartCreature creature;
		std::string file;
		if (!ReadString(object, "file", file, false))
		{
			return std::nullopt;
		}
		// Base64 holds 3 bytes in 4 characters: a longer text is a file that is too large
		if (file.size() > (k_MostCreatureFileBytes + 2) / 3 * 4)
		{
			return std::nullopt;
		}
		auto bytes = join_ticket::DecodeBase64Url(file);
		if (!bytes.has_value() || bytes->size() > k_MostCreatureFileBytes)
		{
			return std::nullopt;
		}
		creature.file = std::move(*bytes);
		if (const auto* slotMember = Member(object, "slot"); slotMember != nullptr && !slotMember->is_null())
		{
			uint64_t slot = 0;
			if (!ReadUnsigned(object, "slot", k_MostSlots - 1, slot))
			{
				return std::nullopt;
			}
			creature.slot = static_cast<uint8_t>(slot);
		}
		if (!ReadAlignment(object, "alignment", creature.alignment))
		{
			return std::nullopt;
		}
		return creature;
	}
	if (type == k_StopRequest)
	{
		return StopRequest {};
	}
	if (type == k_Loaded)
	{
		return Loaded {};
	}
	if (type == k_LoadWait)
	{
		const auto* names = Member(object, "loading");
		if (names == nullptr || !names->is_array() || names->size() > k_MostSlots)
		{
			return std::nullopt;
		}
		LoadWait wait;
		uint64_t secondsLeft = 0;
		if (!ReadUnsigned(object, "seconds_left", k_MostLoadWaitSeconds, secondsLeft))
		{
			return std::nullopt;
		}
		wait.secondsLeft = static_cast<uint32_t>(secondsLeft);
		for (const auto& name : *names)
		{
			if (!name.is_string())
			{
				return std::nullopt;
			}
			wait.loading.push_back(name.get<std::string>());
		}
		return wait;
	}
	if (type == k_GameStop)
	{
		uint64_t turn = 0;
		if (!ReadUnsigned(object, "turn", k_MostTurn, turn))
		{
			return std::nullopt;
		}
		return GameStop {.turn = static_cast<uint32_t>(turn)};
	}
	if (type == k_RoomChat)
	{
		RoomChat chat;
		if (!ReadString(object, "text", chat.text) || !ReadString(object, "from", chat.from, false) ||
		    !ReadString(object, "to", chat.to, false) || !ReadBool(object, "private", chat.privateLine, false) ||
		    !ReadBool(object, "team", chat.team, false))
		{
			return std::nullopt;
		}
		return chat;
	}
	return std::nullopt;
}

} // namespace

std::string_view RoleName(Role role) noexcept
{
	return role == Role::Observer ? "observer" : "god";
}

std::optional<Role> RoleFrom(std::string_view name) noexcept
{
	if (name == "god")
	{
		return Role::God;
	}
	if (name == "observer")
	{
		return Role::Observer;
	}
	return std::nullopt;
}

std::string ToJson(const Frame& frame)
{
	return std::visit(
	    [](const auto& value) {
		    using Kind = std::decay_t<decltype(value)>;
		    Json object;
		    if constexpr (std::is_same_v<Kind, Join>)
		    {
			    object = Json {{"type", k_Join}};
			    if (!value.ticket.empty())
			    {
				    object["ticket"] = value.ticket;
			    }
			    if (!value.gamePassword.empty())
			    {
				    object["game_password"] = value.gamePassword;
			    }
			    if (!value.name.empty())
			    {
				    object["name"] = value.name;
			    }
			    if (!value.build.empty())
			    {
				    object["build"] = value.build;
			    }
			    if (!value.map.empty())
			    {
				    object["map"] = value.map;
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, Resume>)
		    {
			    object = Json {{"type", k_Resume}, {"session", value.session}, {"turn", value.turn}};
			    if (value.world)
			    {
				    object["world"] = true;
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, Ready>)
		    {
			    object = Json {{"type", k_Ready}, {"ready", value.ready}};
		    }
		    else if constexpr (std::is_same_v<Kind, Leave>)
		    {
			    object = Json {{"type", k_Leave}};
		    }
		    else if constexpr (std::is_same_v<Kind, Joined>)
		    {
			    object = Json {{"type", k_Joined}, {"role", RoleName(value.role)}, {"session", value.session}};
			    object["slot"] = value.slot.has_value() ? Json(*value.slot) : Json(nullptr);
			    if (value.observer != 0)
			    {
				    object["observer"] = value.observer;
			    }
			    if (value.staff)
			    {
				    object["staff"] = true;
			    }
			    if (!value.name.empty())
			    {
				    object["name"] = value.name;
			    }
			    if (value.start.has_value())
			    {
				    Json start = Json::object();
				    WriteStart(*value.start, start);
				    object["start"] = std::move(start);
			    }
			    if (!value.dataFiles.empty())
			    {
				    object["data_files"] = value.dataFiles;
			    }
			    if (value.capture)
			    {
				    object["capture"] = true;
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, Refused>)
		    {
			    object = Json {{"type", k_Refused}, {"reason", value.reason}};
			    if (!value.build.empty())
			    {
				    object["build"] = value.build;
			    }
			    if (!value.file.empty())
			    {
				    object["file"] = value.file;
			    }
			    if (!value.path.empty())
			    {
				    object["path"] = value.path;
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, Start>)
		    {
			    object = Json {{"type", k_Start}};
			    WriteStart(value, object);
		    }
		    else if constexpr (std::is_same_v<Kind, Resumed>)
		    {
			    object = Json {{"type", k_Resumed}, {"turn", value.turn}};
			    if (value.world)
			    {
				    object["world"] = true;
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, PlayerState>)
		    {
			    object = Json {{"type", k_PlayerState}, {"slot", value.slot}, {"state", StateName(value.state)}};
		    }
		    else if constexpr (std::is_same_v<Kind, StartQuery>)
		    {
			    object = Json {
			        {"type", k_StartQuery}, {"land", value.land}, {"testbed", value.testbed}, {"checksum", value.checksum}};
		    }
		    else if constexpr (std::is_same_v<Kind, StartReply>)
		    {
			    object = Json {{"type", k_StartReply}, {"have", value.have}, {"checksum", value.checksum}};
		    }
		    else if constexpr (std::is_same_v<Kind, StartCancel>)
		    {
			    object = Json {{"type", k_StartCancel}};
		    }
		    else if constexpr (std::is_same_v<Kind, StartStop>)
		    {
			    object = Json {{"type", k_StartStop}};
		    }
		    else if constexpr (std::is_same_v<Kind, StartTransfer>)
		    {
			    object = Json {{"type", k_StartTransfer}};
		    }
		    else if constexpr (std::is_same_v<Kind, StopRequest>)
		    {
			    object = Json {{"type", k_StopRequest}};
		    }
		    else if constexpr (std::is_same_v<Kind, GameStop>)
		    {
			    object = Json {{"type", k_GameStop}, {"turn", value.turn}};
		    }
		    else if constexpr (std::is_same_v<Kind, RoomChat>)
		    {
			    object = Json {{"type", k_RoomChat}, {"text", value.text}};
			    if (!value.from.empty())
			    {
				    object["from"] = value.from;
			    }
			    if (!value.to.empty())
			    {
				    object["to"] = value.to;
			    }
			    if (value.privateLine)
			    {
				    object["private"] = true;
			    }
			    if (value.team)
			    {
				    object["team"] = true;
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, ObserverLeft>)
		    {
			    object = Json {{"type", k_ObserverLeft}, {"name", value.name}};
		    }
		    else if constexpr (std::is_same_v<Kind, SyncParts>)
		    {
			    auto parts = Json::array();
			    for (const auto& [name, hash] : value.parts)
			    {
				    parts.push_back(Json::array({name, HexOf(hash)}));
			    }
			    object = Json {{"type", k_SyncParts}, {"turn", value.turn}, {"parts", std::move(parts)}};
		    }
		    else if constexpr (std::is_same_v<Kind, DataPrint>)
		    {
			    auto files = Json::array();
			    for (const auto& file : value.files)
			    {
				    files.push_back(Json::array({file.path, file.present, file.size, HexOf(file.hash)}));
			    }
			    object = Json {{"type", k_DataPrint}, {"files", std::move(files)}};
			    if (!value.patch.empty())
			    {
				    object["patch"] = value.patch;
			    }
			    if (!value.language.empty())
			    {
				    object["language"] = value.language;
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, SyncFacts>)
		    {
			    object = Json {{"type", k_SyncFacts},
			                   {"turn", value.turn},
			                   {"build", value.build},
			                   {"land", value.land},
			                   {"land_sha256", value.landSha256},
			                   {"map", value.map},
			                   {"seed", value.seed},
			                   {"data", HexOf(value.data)},
			                   {"patch", value.patch},
			                   {"language", value.language}};
		    }
		    else if constexpr (std::is_same_v<Kind, SyncCapture>)
		    {
			    object = Json {{"type", k_SyncCapture}, {"turn", value.turn}, {"parts", value.parts}};
		    }
		    else if constexpr (std::is_same_v<Kind, SyncBlocks>)
		    {
			    auto parts = Json::array();
			    for (const auto& part : value.parts)
			    {
				    auto blocks = Json::array();
				    for (const auto block : part.blocks)
				    {
					    blocks.push_back(HexOf(block));
				    }
				    parts.push_back(Json {{"part", part.part}, {"entities", part.entities}, {"blocks", std::move(blocks)}});
			    }
			    object = Json {{"type", k_SyncBlocks}, {"turn", value.turn}, {"parts", std::move(parts)}};
		    }
		    else if constexpr (std::is_same_v<Kind, SyncAsk>)
		    {
			    auto parts = Json::array();
			    for (const auto& part : value.parts)
			    {
				    parts.push_back(Json {{"part", part.part}, {"blocks", part.blocks}});
			    }
			    object = Json {{"type", k_SyncAsk}, {"turn", value.turn}, {"parts", std::move(parts)}};
		    }
		    else if constexpr (std::is_same_v<Kind, SyncEntities>)
		    {
			    auto parts = Json::array();
			    for (const auto& part : value.parts)
			    {
				    auto entities = Json::array();
				    for (const auto& entity : part.entities)
				    {
					    // The position goes as the doubles of the floats, which read back to the same floats
					    const auto place = entity.position.has_value()
					                           ? Json::array({static_cast<double>((*entity.position)[0]),
					                                          static_cast<double>((*entity.position)[1]),
					                                          static_cast<double>((*entity.position)[2])})
					                           : Json(nullptr);
					    entities.push_back(Json::array({entity.entity, HexOf(entity.hash), entity.kind, place}));
				    }
				    parts.push_back(Json {{"part", part.part}, {"entities", std::move(entities)}});
			    }
			    object = Json {{"type", k_SyncEntities}, {"turn", value.turn}, {"parts", std::move(parts)}};
		    }
		    else if constexpr (std::is_same_v<Kind, SyncWorld>)
		    {
			    object =
			        Json {{"type", k_SyncWorld},  {"turn", value.turn}, {"index", value.index},
			              {"count", value.count}, {"size", value.size}, {"data", join_ticket::EncodeBase64Url(value.bytes)}};
		    }
		    else if constexpr (std::is_same_v<Kind, Loaded>)
		    {
			    object = Json {{"type", k_Loaded}};
		    }
		    else if constexpr (std::is_same_v<Kind, LoadWait>)
		    {
			    object = Json {{"type", k_LoadWait}, {"loading", value.loading}, {"seconds_left", value.secondsLeft}};
		    }
		    else
		    {
			    // The alignment goes as the double of the float, which reads back to the same float
			    object = Json {{"type", k_StartCreature},
			                   {"file", join_ticket::EncodeBase64Url(value.file)},
			                   {"alignment", static_cast<double>(value.alignment)}};
			    if (value.slot.has_value())
			    {
				    object["slot"] = *value.slot;
			    }
		    }
		    return Dump(object);
	    },
	    frame);
}

std::optional<Frame> Parse(std::string_view text)
{
	const auto value = Json::parse(text.begin(), text.end(), nullptr, false);
	if (value.is_discarded() || !value.is_object())
	{
		return std::nullopt;
	}
	std::string type;
	if (!ReadString(value, "type", type))
	{
		return std::nullopt;
	}
	return ParseObject(value, type);
}

} // namespace openblack::game_socket
