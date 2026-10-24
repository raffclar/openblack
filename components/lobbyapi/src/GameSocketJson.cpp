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

#include <nlohmann/json.hpp>

#include "GameSocket.h"

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

/// The most god slots that a frame can name
constexpr uint64_t k_MostSlots = 32;

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
		uint64_t seed = 0;
		uint64_t turn = 0;
		if (!ReadString(object, "land", start.land, false) || !ReadBool(object, "testbed", start.testbed, false) ||
		    !ReadUnsigned(object, "seed", k_MostTurn, seed) || !ReadUnsigned(object, "turn", k_MostTurn, turn, false) ||
		    !ReadString(object, "opening", start.opening, false))
		{
			return std::nullopt;
		}
		start.seed = static_cast<uint32_t>(seed);
		start.turn = static_cast<uint32_t>(turn);
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
		    }
		    else if constexpr (std::is_same_v<Kind, Refused>)
		    {
			    object = Json {{"type", k_Refused}, {"reason", value.reason}};
		    }
		    else if constexpr (std::is_same_v<Kind, Start>)
		    {
			    object = Json {{"type", k_Start},
			                   {"land", value.land},
			                   {"testbed", value.testbed},
			                   {"seed", value.seed},
			                   {"turn", value.turn}};
			    if (!value.opening.empty())
			    {
				    object["opening"] = value.opening;
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, Resumed>)
		    {
			    object = Json {{"type", k_Resumed}, {"turn", value.turn}};
			    if (value.world)
			    {
				    object["world"] = true;
			    }
		    }
		    else
		    {
			    object = Json {{"type", k_PlayerState}, {"slot", value.slot}, {"state", StateName(value.state)}};
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
