/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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

/// The most god slots that a frame can name
constexpr uint64_t k_MostSlots = 32;
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
	for (const auto& entry : *gods)
	{
		God god;
		uint64_t slot = 0;
		if (!entry.is_object() || !ReadUnsigned(entry, "slot", k_MostSlot, slot) || !ReadString(entry, "name", god.name))
		{
			return false;
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
			gods.push_back({{"slot", god.slot}, {"name", god.name}});
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
		    !ReadString(object, "game_password", join.gamePassword, false) || !ReadString(object, "name", join.name, false))
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
		if (!ReadString(object, "reason", refused.reason))
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
		if (!ReadString(object, "text", chat.text) || !ReadString(object, "from", chat.from, false))
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
		    }
		    else if constexpr (std::is_same_v<Kind, Refused>)
		    {
			    object = Json {{"type", k_Refused}, {"reason", value.reason}};
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
