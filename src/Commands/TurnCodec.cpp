/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TurnCodec.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <type_traits>

#include "PlayerSlots.h"

namespace openblack::turn_codec
{
namespace
{

using namespace player_commands;

static_assert(sizeof(entt::id_type) == sizes::k_Entity, "An entity goes on the wire as 32 bits");
static_assert(std::numeric_limits<float>::is_iec559, "A float goes on the wire as its IEEE 754 bits");

template <class... Ts>
struct Overloaded: Ts...
{
	using Ts::operator()...;
};

/// Adds little-endian numbers to a buffer
class Writer
{
public:
	explicit Writer(std::vector<uint8_t>& out)
	    : _out(out)
	{
	}

	template <class T>
	void Number(T value)
	{
		static_assert(std::is_unsigned_v<T>);
		for (size_t i = 0; i < sizeof(T); ++i)
		{
			_out.push_back(static_cast<uint8_t>(value >> (8 * i)));
		}
	}
	void Byte(uint8_t value) { _out.push_back(value); }
	void Float(float value) { Number(std::bit_cast<uint32_t>(value)); }
	void Entity(entt::entity value) { Number(static_cast<uint32_t>(entt::to_integral(value))); }
	void Vector(const glm::vec3& value)
	{
		Float(value.x);
		Float(value.y);
		Float(value.z);
	}

private:
	std::vector<uint8_t>& _out;
};

/// Reads little-endian numbers from a frame. After the first read past the end, each read gives 0 and the reader is bad.
class Reader
{
public:
	explicit Reader(std::span<const uint8_t> bytes)
	    : _bytes(bytes)
	{
	}

	template <class T>
	T Number()
	{
		static_assert(std::is_unsigned_v<T>);
		if (_bytes.size() - _at < sizeof(T))
		{
			_bad = true;
			_at = _bytes.size();
			return 0;
		}
		T value = 0;
		for (size_t i = 0; i < sizeof(T); ++i)
		{
			value |= static_cast<T>(static_cast<T>(_bytes[_at + i]) << (8 * i));
		}
		_at += sizeof(T);
		return value;
	}
	uint8_t Byte() { return Number<uint8_t>(); }
	float Float() { return std::bit_cast<float>(Number<uint32_t>()); }
	entt::entity Entity() { return static_cast<entt::entity>(Number<uint32_t>()); }
	glm::vec3 Vector()
	{
		const float x = Float();
		const float y = Float();
		const float z = Float();
		return {x, y, z};
	}

	[[nodiscard]] bool Bad() const { return _bad; }
	[[nodiscard]] size_t Left() const { return _bytes.size() - _at; }
	[[nodiscard]] bool AtEnd() const { return _at == _bytes.size(); }

private:
	std::span<const uint8_t> _bytes;
	size_t _at {0};
	bool _bad {false};
};

void WriteAction(Writer& out, const Action& action)
{
	out.Byte(static_cast<uint8_t>(TagOf(action)));
	std::visit(Overloaded {
	               [&out](const LeashKey& command) { out.Byte(static_cast<uint8_t>(command.key)); },
	               [&out](const LeashTap& command) { out.Entity(command.creature); },
	               [&out](const CreatureFeedback& command) {
		               out.Entity(command.creature);
		               out.Float(command.amount);
	               },
	               [&out](const HandTake& command) { out.Entity(command.object); },
	               [&out](const HandLetGo& command) {
		               out.Entity(command.object);
		               out.Vector(command.velocity);
	               },
	               [&out](const HandTap& command) { out.Entity(command.object); },
	               [&out](const HandGive& command) {
		               out.Entity(command.creature);
		               out.Entity(command.object);
	               },
	               [&out](const LeashOrder& command) {
		               out.Byte(static_cast<uint8_t>(command.kind));
		               out.Entity(command.object);
		               out.Vector(command.place);
	               },
	               [&out](const LeashPostTap& command) { out.Entity(command.post); },
	               [&out](const HandReport& command) {
		               out.Vector(command.collidePoint);
		               out.Vector(command.handPoint);
		               out.Vector(command.camera);
		               out.Vector(command.focus);
	               },
	           },
	           action);
}

/// Reads one command. Returns the error of the command, or None.
DecodeError ReadAction(Reader& in, Action& action)
{
	switch (static_cast<CommandTag>(in.Byte()))
	{
	case CommandTag::LeashKey:
	{
		const auto key = in.Byte();
		if (key > static_cast<uint8_t>(creature_leash::LeashKey::NextLeash))
		{
			return DecodeError::Value;
		}
		action = LeashKey {.key = static_cast<creature_leash::LeashKey>(key)};
		return DecodeError::None;
	}
	case CommandTag::LeashTap:
		action = LeashTap {.creature = in.Entity()};
		return DecodeError::None;
	case CommandTag::CreatureFeedback:
	{
		const auto creature = in.Entity();
		action = CreatureFeedback {.creature = creature, .amount = in.Float()};
		return DecodeError::None;
	}
	case CommandTag::HandTake:
		action = HandTake {.object = in.Entity()};
		return DecodeError::None;
	case CommandTag::HandLetGo:
	{
		const auto object = in.Entity();
		action = HandLetGo {.object = object, .velocity = in.Vector()};
		return DecodeError::None;
	}
	case CommandTag::HandTap:
		action = HandTap {.object = in.Entity()};
		return DecodeError::None;
	case CommandTag::HandGive:
	{
		const auto creature = in.Entity();
		action = HandGive {.creature = creature, .object = in.Entity()};
		return DecodeError::None;
	}
	case CommandTag::LeashOrder:
	{
		const auto kind = in.Byte();
		if (kind > static_cast<uint8_t>(LeashOrder::Kind::Untie))
		{
			return DecodeError::Value;
		}
		const auto object = in.Entity();
		action = LeashOrder {.kind = static_cast<LeashOrder::Kind>(kind), .object = object, .place = in.Vector()};
		return DecodeError::None;
	}
	case CommandTag::LeashPostTap:
		action = LeashPostTap {.post = in.Entity()};
		return DecodeError::None;
	case CommandTag::HandReport:
	{
		const auto collidePoint = in.Vector();
		const auto handPoint = in.Vector();
		const auto camera = in.Vector();
		action = HandReport {.collidePoint = collidePoint, .handPoint = handPoint, .camera = camera, .focus = in.Vector()};
		return DecodeError::None;
	}
	}
	return in.Bad() ? DecodeError::Truncated : DecodeError::Command;
}

DecodeError ReadActions(Reader& in, std::vector<Action>& actions)
{
	const auto count = in.Byte();
	if (count > k_MostBlockCommands)
	{
		return DecodeError::Value;
	}
	actions.resize(count);
	for (auto& action : actions)
	{
		if (const auto error = ReadAction(in, action); error != DecodeError::None)
		{
			return error;
		}
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
	}
	return DecodeError::None;
}

[[nodiscard]] bool ValidActions(const std::vector<Action>& actions)
{
	return actions.size() <= k_MostBlockCommands;
}

[[nodiscard]] bool ValidFrame(const Frame& frame)
{
	return std::visit(Overloaded {
	                      [](const CommandBlock& block) { return ValidActions(block.actions); },
	                      [](const TurnBundle& turn) {
		                      if (turn.blocks.size() > std::numeric_limits<uint16_t>::max())
		                      {
			                      return false;
		                      }
		                      return std::ranges::all_of(turn.blocks, [](const StampedBlock& block) {
			                      return static_cast<size_t>(block.sender) < player_slots::k_MaxPlayers &&
			                             ValidActions(block.actions);
		                      });
	                      },
	                      [](const TurnChecksum& /*checksum*/) { return true; },
	                      [](const Ping& /*ping*/) { return true; },
	                      [](const Pong& /*pong*/) { return true; },
	                  },
	                  frame);
}

size_t SizeOfActions(const std::vector<Action>& actions)
{
	size_t size = 1;
	for (const auto& action : actions)
	{
		size += SizeOf(action);
	}
	return size;
}

DecodeResult Fail(DecodeError error)
{
	return {.error = error, .frame = std::nullopt};
}

} // namespace

CommandTag TagOf(const Action& action) noexcept
{
	return std::visit(Overloaded {
	                      [](const LeashKey& /*command*/) { return CommandTag::LeashKey; },
	                      [](const LeashTap& /*command*/) { return CommandTag::LeashTap; },
	                      [](const CreatureFeedback& /*command*/) { return CommandTag::CreatureFeedback; },
	                      [](const HandTake& /*command*/) { return CommandTag::HandTake; },
	                      [](const HandLetGo& /*command*/) { return CommandTag::HandLetGo; },
	                      [](const HandTap& /*command*/) { return CommandTag::HandTap; },
	                      [](const HandGive& /*command*/) { return CommandTag::HandGive; },
	                      [](const LeashOrder& /*command*/) { return CommandTag::LeashOrder; },
	                      [](const LeashPostTap& /*command*/) { return CommandTag::LeashPostTap; },
	                      [](const HandReport& /*command*/) { return CommandTag::HandReport; },
	                  },
	                  action);
}

size_t SizeOf(const Action& action) noexcept
{
	using namespace sizes;
	const size_t fields = std::visit(Overloaded {
	                                     [](const LeashKey& /*command*/) { return size_t {1}; },
	                                     [](const LeashTap& /*command*/) { return k_Entity; },
	                                     [](const CreatureFeedback& /*command*/) { return k_Entity + 4; },
	                                     [](const HandTake& /*command*/) { return k_Entity; },
	                                     [](const HandLetGo& /*command*/) { return k_Entity + k_Vector; },
	                                     [](const HandTap& /*command*/) { return k_Entity; },
	                                     [](const HandGive& /*command*/) { return 2 * k_Entity; },
	                                     [](const LeashOrder& /*command*/) { return 1 + k_Entity + k_Vector; },
	                                     [](const LeashPostTap& /*command*/) { return k_Entity; },
	                                     [](const HandReport& /*command*/) { return 4 * k_Vector; },
	                                 },
	                                 action);
	return 1 + fields;
}

size_t SizeOf(const Frame& frame) noexcept
{
	using namespace sizes;
	return std::visit(
	    Overloaded {
	        [](const CommandBlock& block) { return k_Header + k_CommandsBody - 1 + SizeOfActions(block.actions); },
	        [](const TurnBundle& turn) {
		        size_t size = k_Header + k_TurnBody;
		        for (const auto& block : turn.blocks)
		        {
			        size += k_Stamp - 1 + SizeOfActions(block.actions);
		        }
		        return size;
	        },
	        [](const TurnChecksum& /*checksum*/) { return k_Checksum; },
	        [](const Ping& /*ping*/) { return k_Ping; },
	        [](const Pong& /*pong*/) { return k_Ping; },
	    },
	    frame);
}

bool Append(const Frame& frame, std::vector<uint8_t>& out)
{
	const auto size = SizeOf(frame);
	if (!ValidFrame(frame) || size > k_MostFrameBytes)
	{
		return false;
	}
	out.reserve(out.size() + size);
	Writer writer(out);
	writer.Byte(k_Version);
	const auto writeActions = [&writer](const std::vector<Action>& actions) {
		writer.Byte(static_cast<uint8_t>(actions.size()));
		for (const auto& action : actions)
		{
			WriteAction(writer, action);
		}
	};
	std::visit(Overloaded {
	               [&](const CommandBlock& block) {
		               writer.Byte(static_cast<uint8_t>(FrameKind::Commands));
		               writer.Number(block.sequence);
		               writer.Number(block.turn);
		               writeActions(block.actions);
	               },
	               [&](const TurnBundle& turn) {
		               writer.Byte(static_cast<uint8_t>(FrameKind::Turn));
		               writer.Number(turn.turn);
		               writer.Number(static_cast<uint16_t>(turn.blocks.size()));
		               for (const auto& block : turn.blocks)
		               {
			               writer.Byte(static_cast<uint8_t>(block.sender));
			               writer.Number(block.sequence);
			               writeActions(block.actions);
		               }
	               },
	               [&](const TurnChecksum& checksum) {
		               writer.Byte(static_cast<uint8_t>(FrameKind::Checksum));
		               writer.Number(checksum.turn);
		               writer.Number(checksum.checksum);
	               },
	               [&](const Ping& ping) {
		               writer.Byte(static_cast<uint8_t>(FrameKind::Ping));
		               writer.Number(ping.id);
		               writer.Number(ping.time);
	               },
	               [&](const Pong& pong) {
		               writer.Byte(static_cast<uint8_t>(FrameKind::Pong));
		               writer.Number(pong.id);
		               writer.Number(pong.time);
	               },
	           },
	           frame);
	return true;
}

std::vector<uint8_t> Encode(const Frame& frame)
{
	std::vector<uint8_t> out;
	if (!Append(frame, out))
	{
		out.clear();
	}
	return out;
}

DecodeResult Decode(std::span<const uint8_t> bytes)
{
	if (bytes.empty())
	{
		return Fail(DecodeError::Empty);
	}
	if (bytes.size() > k_MostFrameBytes)
	{
		return Fail(DecodeError::TooLarge);
	}
	Reader in(bytes);
	if (in.Byte() != k_Version)
	{
		return Fail(DecodeError::Version);
	}
	const auto kind = static_cast<FrameKind>(in.Byte());
	if (in.Bad())
	{
		return Fail(DecodeError::Truncated);
	}
	Frame frame;
	switch (kind)
	{
	case FrameKind::Commands:
	{
		CommandBlock block;
		block.sequence = in.Number<uint16_t>();
		block.turn = in.Number<uint32_t>();
		if (const auto error = ReadActions(in, block.actions); error != DecodeError::None)
		{
			return Fail(error);
		}
		frame = std::move(block);
		break;
	}
	case FrameKind::Turn:
	{
		TurnBundle turn;
		turn.turn = in.Number<uint32_t>();
		const auto count = in.Number<uint16_t>();
		// Each block has at least its stamp. When the rest of the frame is too short for the blocks, the frame stops
		// before its end.
		if (in.Bad() || count > in.Left() / sizes::k_Stamp)
		{
			return Fail(DecodeError::Truncated);
		}
		turn.blocks.resize(count);
		for (auto& block : turn.blocks)
		{
			const auto sender = in.Byte();
			if (sender >= player_slots::k_MaxPlayers)
			{
				return Fail(in.Bad() ? DecodeError::Truncated : DecodeError::Value);
			}
			block.sender = static_cast<PlayerNames>(sender);
			block.sequence = in.Number<uint16_t>();
			if (const auto error = ReadActions(in, block.actions); error != DecodeError::None)
			{
				return Fail(error);
			}
		}
		frame = std::move(turn);
		break;
	}
	case FrameKind::Checksum:
	{
		const auto turn = in.Number<uint32_t>();
		frame = TurnChecksum {.turn = turn, .checksum = in.Number<uint64_t>()};
		break;
	}
	case FrameKind::Ping:
	{
		const auto id = in.Number<uint32_t>();
		frame = Ping {.id = id, .time = in.Number<uint64_t>()};
		break;
	}
	case FrameKind::Pong:
	{
		const auto id = in.Number<uint32_t>();
		frame = Pong {.id = id, .time = in.Number<uint64_t>()};
		break;
	}
	default:
		return Fail(DecodeError::Kind);
	}
	if (in.Bad())
	{
		return Fail(DecodeError::Truncated);
	}
	if (!in.AtEnd())
	{
		return Fail(DecodeError::TrailingBytes);
	}
	return {.error = DecodeError::None, .frame = std::move(frame)};
}

} // namespace openblack::turn_codec
