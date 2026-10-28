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

#include "Creature/CreatureFace.h"
#include "Creature/CreatureFight.h"
#include "PlayerSlots.h"

namespace openblack::turn_codec
{
namespace
{

using namespace player_commands;

/// The last kind of a fight move, so that a command with another kind does not read
constexpr auto k_LastFightMoveKind = static_cast<uint8_t>(creature_fight::Move::Kind::Spell);

/// The largest block of tattoo pictures: its head, and one picture of 64 by 64 four-bit levels for each slot
constexpr size_t k_MostTattooPictures = 0x28 + creature_tattoo::k_SlotCount * 0x800;

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
		               out.Vector(command.start.angularMomentum);
		               out.Vector(command.start.origin);
		               out.Vector(command.start.angles);
	               },
	               [&out](const HandTap& command) { out.Entity(command.object); },
	               [&out](const HandGive& command) {
		               out.Entity(command.creature);
		               out.Entity(command.object);
		               out.Byte(command.leftHand ? 1 : 0);
	               },
	               [&out](const LeashOrder& command) {
		               out.Byte(static_cast<uint8_t>(command.kind));
		               out.Entity(command.object);
		               out.Vector(command.place);
	               },
	               [&out](const LeashPostTap& command) { out.Entity(command.post); },
	               [&out](const HandTwist& command) {
		               out.Entity(command.object);
		               out.Vector(command.moved);
	               },
	               [&out](const HandLock& command) { out.Entity(command.object); },
	               [&out](const HandUnlock& command) { out.Entity(command.object); },
	               [&out](const HandApply& command) { out.Entity(command.target); },
	               [&out](const CreatureBruise& command) {
		               out.Entity(command.creature);
		               out.Byte(static_cast<uint8_t>(command.wound.has_value() ? 1 : 0));
		               const auto wound = command.wound.value_or(creature_marks::Mark {});
		               for (const auto byte : {wound.u, wound.v, wound.skin, wound.age, wound.type, wound.column})
		               {
			               out.Byte(byte);
		               }
	               },
	               [&out](const TotemShare& command) {
		               out.Entity(command.totem);
		               out.Float(command.share);
	               },
	               [&out](const FightGround& command) {
		               out.Entity(command.creature);
		               out.Vector(command.point);
		               out.Byte(static_cast<uint8_t>(command.replace ? 1 : 0));
	               },
	               [&out](const FightMove& command) {
		               out.Entity(command.target);
		               out.Byte(command.kind);
		               out.Number(command.value);
		               out.Byte(static_cast<uint8_t>(command.replace ? 1 : 0));
	               },
	               [&out](const FightCharge& command) { out.Number(command.heldMs); },
	               [&out](const SeedGesture& command) {
		               out.Byte(static_cast<uint8_t>(command.kind));
		               out.Byte(static_cast<uint8_t>(command.level));
	               },
	               [&out](const HandPick& command) { out.Entity(command.object); },
	               [&out](const HandClick& command) {
		               out.Entity(command.thing);
		               out.Byte(static_cast<uint8_t>(command.release ? 1 : 0));
		               out.Byte(static_cast<uint8_t>(command.hasPlace ? 1 : 0));
		               out.Number(static_cast<uint32_t>(command.placeX));
		               out.Number(static_cast<uint32_t>(command.placeZ));
	               },
	               [&out](const CreatureReaction& command) {
		               out.Entity(command.creature);
		               out.Number(command.animation);
		               out.Byte(static_cast<uint8_t>(command.mirrored ? 1 : 0));
		               out.Byte(static_cast<uint8_t>(command.pullsFace ? 1 : 0));
		               out.Byte(command.face);
		               out.Float(command.faceMs);
		               out.Byte(command.cue);
		               out.Float(command.interruptsAfter);
	               },
	               [](const PlayerLeft& /*command*/) {},
	               [](const PlayerJoined& /*command*/) {},
	               [&out](const MiracleRequest& command) { out.Byte(static_cast<uint8_t>(command.seed)); },
	               [](const MiracleRepeat& /*command*/) {},
	               [&out](const HandCast& command) {
		               out.Entity(command.object);
		               out.Vector(command.point);
		               out.Float(command.size);
		               out.Vector(command.handPosition);
		               out.Vector(command.cameraForward);
		               out.Vector(command.handVelocity);
		               out.Float(command.spin);
	               },
	               [&out](const HandCastUnlock& command) { out.Entity(command.seed); },
	               [&out](const LeashPick& command) { out.Byte(static_cast<uint8_t>(command.type)); },
	               [](const HandShakeOff& /*command*/) {},
	               [&out](const GripDust& command) { out.Vector(command.point); },
	               [&out](const HelpReport& command) {
		               out.Number(command.events);
		               out.Byte(static_cast<uint8_t>((command.boxShows ? 1u : 0u) | (command.cursorOnPoint ? 2u : 0u) |
		                                             (command.cameraSeesLand ? 4u : 0u) |
		                                             (command.cameraNearAndSteep ? 8u : 0u) |
		                                             (command.cameraSeesSky ? 16u : 0u)));
	               },
	               [&out](const HelpQuery& command) {
		               out.Entity(command.thing);
		               out.Byte(static_cast<uint8_t>(command.hasGround ? 1 : 0));
		               out.Vector(command.ground);
	               },
	               [&out](const ViewReport& command) {
		               out.Vector(command.eye);
		               out.Vector(command.focus);
		               out.Float(command.lens);
		               out.Float(command.nearClip);
		               out.Number(command.screenWidth);
		               out.Number(command.screenHeight);
		               out.Float(command.aspect);
		               out.Float(command.inclusionDistance);
		               out.Number(command.cameraOrders);
		               out.Byte(static_cast<uint8_t>((command.cameraArrived ? 1u : 0u) | (command.gripsLand ? 2u : 0u) |
		                                             (command.otherButton ? 4u : 0u)));
	               },
	               [&out](const GiveCallOff& command) { out.Entity(command.creature); },
	               [&out](const CreatureTattoo& command) {
		               out.Entity(command.creature);
		               for (const auto& slot : command.slots)
		               {
			               out.Byte(slot.design);
			               out.Byte(slot.site);
			               out.Byte(slot.colour.r);
			               out.Byte(slot.colour.g);
			               out.Byte(slot.colour.b);
		               }
		               out.Byte(static_cast<uint8_t>(command.hasPictures ? 1 : 0));
		               out.Number(static_cast<uint16_t>(command.pictures.size()));
		               for (const auto byte : command.pictures)
		               {
			               out.Byte(byte);
		               }
	               },
	               [&out](const LeashLength& command) { out.Float(command.length); },
	               [](const CreatureFightCheat& /*command*/) {},
	               [](const TempleLeashOff& /*command*/) {},
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
		const auto velocity = in.Vector();
		const auto angularMomentum = in.Vector();
		const auto origin = in.Vector();
		action = HandLetGo {
		    .object = object,
		    .velocity = velocity,
		    .start = {.angularMomentum = angularMomentum, .origin = origin, .angles = in.Vector()},
		};
		return DecodeError::None;
	}
	case CommandTag::HandTap:
		action = HandTap {.object = in.Entity()};
		return DecodeError::None;
	case CommandTag::HandGive:
	{
		const auto creature = in.Entity();
		const auto object = in.Entity();
		const auto side = in.Byte();
		if (side > 1)
		{
			return DecodeError::Value;
		}
		action = HandGive {.creature = creature, .object = object, .leftHand = side != 0};
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
	case CommandTag::HandTwist:
	{
		const auto object = in.Entity();
		action = HandTwist {.object = object, .moved = in.Vector()};
		return DecodeError::None;
	}
	case CommandTag::HandLock:
		action = HandLock {.object = in.Entity()};
		return DecodeError::None;
	case CommandTag::HandUnlock:
		action = HandUnlock {.object = in.Entity()};
		return DecodeError::None;
	case CommandTag::HandApply:
		action = HandApply {.target = in.Entity()};
		return DecodeError::None;
	case CommandTag::CreatureBruise:
	{
		const auto creature = in.Entity();
		const auto wounded = in.Byte();
		creature_marks::Mark wound;
		for (auto* byte : {&wound.u, &wound.v, &wound.skin, &wound.age, &wound.type, &wound.column})
		{
			*byte = in.Byte();
		}
		// No wound has its bytes empty, so that each command has one way of bytes
		if (wounded > 1 || (wounded == 0 && wound != creature_marks::Mark {}))
		{
			return DecodeError::Value;
		}
		action = CreatureBruise {.creature = creature, .wound = wounded != 0 ? std::optional(wound) : std::nullopt};
		return DecodeError::None;
	}
	case CommandTag::TotemShare:
	{
		const auto totem = in.Entity();
		action = TotemShare {.totem = totem, .share = in.Float()};
		return DecodeError::None;
	}
	case CommandTag::FightGround:
	{
		const auto creature = in.Entity();
		const auto point = in.Vector();
		const auto replace = in.Byte();
		if (replace > 1)
		{
			return DecodeError::Value;
		}
		action = FightGround {.creature = creature, .point = point, .replace = replace != 0};
		return DecodeError::None;
	}
	case CommandTag::FightMove:
	{
		const auto target = in.Entity();
		const auto kind = in.Byte();
		const auto value = in.Number<uint32_t>();
		const auto replace = in.Byte();
		if (kind > k_LastFightMoveKind || replace > 1)
		{
			return DecodeError::Value;
		}
		action = FightMove {.target = target, .kind = kind, .value = value, .replace = replace != 0};
		return DecodeError::None;
	}
	case CommandTag::FightCharge:
		action = FightCharge {.heldMs = in.Number<uint32_t>()};
		return DecodeError::None;
	case CommandTag::SeedGesture:
	{
		const auto kind = in.Byte();
		const auto level = static_cast<int8_t>(in.Byte());
		if (kind > static_cast<uint8_t>(SeedGesture::Kind::Shake))
		{
			return DecodeError::Value;
		}
		action = SeedGesture {.kind = static_cast<SeedGesture::Kind>(kind), .level = level};
		return DecodeError::None;
	}
	case CommandTag::HandPick:
		action = HandPick {.object = in.Entity()};
		return DecodeError::None;
	case CommandTag::HandClick:
	{
		HandClick click {};
		click.thing = in.Entity();
		const auto release = in.Byte();
		const auto hasPlace = in.Byte();
		click.placeX = static_cast<int32_t>(in.Number<uint32_t>());
		click.placeZ = static_cast<int32_t>(in.Number<uint32_t>());
		// A press clicks a thing and has no place; no place has its bytes empty
		const bool placeEmpty = hasPlace != 0 || (click.placeX == 0 && click.placeZ == 0);
		if (release > 1 || hasPlace > 1 || (release == 0 && hasPlace != 0) || !placeEmpty)
		{
			return DecodeError::Value;
		}
		click.release = release != 0;
		click.hasPlace = hasPlace != 0;
		action = click;
		return DecodeError::None;
	}
	case CommandTag::CreatureReaction:
	{
		CreatureReaction reaction {};
		reaction.creature = in.Entity();
		reaction.animation = in.Number<uint32_t>();
		const auto mirrored = in.Byte();
		const auto pullsFace = in.Byte();
		reaction.face = in.Byte();
		reaction.faceMs = in.Float();
		reaction.cue = in.Byte();
		reaction.interruptsAfter = in.Float();
		// No face has its bytes empty, so that each command has one way of bytes
		const bool noFaceEmpty =
		    pullsFace != 0 || (reaction.face == 0 && std::bit_cast<uint32_t>(reaction.faceMs) == 0 && reaction.cue == 0);
		if (mirrored > 1 || pullsFace > 1 || reaction.face >= creature_face::k_FaceCount ||
		    reaction.cue >= creature_face::k_CueCount || !noFaceEmpty)
		{
			return DecodeError::Value;
		}
		reaction.mirrored = mirrored != 0;
		reaction.pullsFace = pullsFace != 0;
		action = reaction;
		return DecodeError::None;
	}
	case CommandTag::PlayerLeft:
		action = PlayerLeft {};
		return DecodeError::None;
	case CommandTag::PlayerJoined:
		action = PlayerJoined {};
		return DecodeError::None;
	case CommandTag::MiracleRequest:
	{
		const auto seed = in.Byte();
		if (seed > static_cast<uint8_t>(SpellSeedType::BeamExplosion))
		{
			return DecodeError::Value;
		}
		action = MiracleRequest {.seed = static_cast<SpellSeedType>(seed)};
		return DecodeError::None;
	}
	case CommandTag::MiracleRepeat:
		action = MiracleRepeat {};
		return DecodeError::None;
	case CommandTag::HandCast:
	{
		HandCast cast {};
		cast.object = in.Entity();
		cast.point = in.Vector();
		cast.size = in.Float();
		cast.handPosition = in.Vector();
		cast.cameraForward = in.Vector();
		cast.handVelocity = in.Vector();
		cast.spin = in.Float();
		action = cast;
		return DecodeError::None;
	}
	case CommandTag::HandCastUnlock:
		action = HandCastUnlock {.seed = in.Entity()};
		return DecodeError::None;
	case CommandTag::LeashPick:
	{
		// Only a leash that a creature can wear
		const auto type = static_cast<LeashType>(in.Byte());
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (type != LeashType::Evil && type != LeashType::Rope && type != LeashType::Good)
		{
			return DecodeError::Value;
		}
		action = LeashPick {.type = type};
		return DecodeError::None;
	}
	case CommandTag::HandShakeOff:
		action = HandShakeOff {};
		return DecodeError::None;
	case CommandTag::CreatureFightCheat:
		action = CreatureFightCheat {};
		return DecodeError::None;
	case CommandTag::GripDust:
		action = GripDust {.point = in.Vector()};
		return DecodeError::None;
	case CommandTag::HelpReport:
	{
		HelpReport report {};
		report.events = in.Number<uint64_t>();
		const auto flags = in.Byte();
		if (flags > 31)
		{
			return DecodeError::Value;
		}
		report.boxShows = (flags & 1u) != 0;
		report.cursorOnPoint = (flags & 2u) != 0;
		report.cameraSeesLand = (flags & 4u) != 0;
		report.cameraNearAndSteep = (flags & 8u) != 0;
		report.cameraSeesSky = (flags & 16u) != 0;
		action = report;
		return DecodeError::None;
	}
	case CommandTag::HelpQuery:
	{
		HelpQuery query {};
		query.thing = in.Entity();
		const auto hasGround = in.Byte();
		if (hasGround > 1)
		{
			return DecodeError::Value;
		}
		query.hasGround = hasGround == 1;
		query.ground = in.Vector();
		action = query;
		return DecodeError::None;
	}
	case CommandTag::ViewReport:
	{
		ViewReport report {};
		report.eye = in.Vector();
		report.focus = in.Vector();
		report.lens = in.Float();
		report.nearClip = in.Float();
		report.screenWidth = in.Number<uint16_t>();
		report.screenHeight = in.Number<uint16_t>();
		report.aspect = in.Float();
		report.inclusionDistance = in.Float();
		report.cameraOrders = in.Number<uint32_t>();
		const auto flags = in.Byte();
		if (flags > 7)
		{
			return DecodeError::Value;
		}
		report.cameraArrived = (flags & 1u) != 0;
		report.gripsLand = (flags & 2u) != 0;
		report.otherButton = (flags & 4u) != 0;
		action = report;
		return DecodeError::None;
	}
	case CommandTag::GiveCallOff:
		action = GiveCallOff {.creature = in.Entity()};
		return DecodeError::None;
	case CommandTag::CreatureTattoo:
	{
		CreatureTattoo tattoo {.creature = in.Entity()};
		for (auto& slot : tattoo.slots)
		{
			slot.design = in.Byte();
			slot.site = in.Byte();
			slot.colour.r = in.Byte();
			slot.colour.g = in.Byte();
			slot.colour.b = in.Byte();
			// A design is one of the sixteen, and a site is four bits
			if (slot.design >= creature_tattoo::k_DesignCount || slot.site > creature_tattoo::k_NoSite)
			{
				return in.Bad() ? DecodeError::Truncated : DecodeError::Value;
			}
		}
		const auto hasPictures = in.Byte();
		const auto size = in.Number<uint16_t>();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (hasPictures > 1 || size > k_MostTattooPictures || (hasPictures == 0 && size != 0))
		{
			return DecodeError::Value;
		}
		if (size > in.Left())
		{
			return DecodeError::Truncated;
		}
		tattoo.hasPictures = hasPictures == 1;
		tattoo.pictures.resize(size);
		for (auto& byte : tattoo.pictures)
		{
			byte = in.Byte();
		}
		action = std::move(tattoo);
		return DecodeError::None;
	}
	case CommandTag::LeashLength:
		action = LeashLength {.length = in.Float()};
		return DecodeError::None;
	case CommandTag::TempleLeashOff:
		action = TempleLeashOff {};
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
	                      [](const WorldPart& part) { return part.part < part.parts && part.bytes.size() <= k_WorldPartBytes; },
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
	                      [](const HandTwist& /*command*/) { return CommandTag::HandTwist; },
	                      [](const HandLock& /*command*/) { return CommandTag::HandLock; },
	                      [](const HandUnlock& /*command*/) { return CommandTag::HandUnlock; },
	                      [](const HandApply& /*command*/) { return CommandTag::HandApply; },
	                      [](const TotemShare& /*command*/) { return CommandTag::TotemShare; },
	                      [](const CreatureBruise& /*command*/) { return CommandTag::CreatureBruise; },
	                      [](const FightGround& /*command*/) { return CommandTag::FightGround; },
	                      [](const FightMove& /*command*/) { return CommandTag::FightMove; },
	                      [](const FightCharge& /*command*/) { return CommandTag::FightCharge; },
	                      [](const CreatureReaction& /*command*/) { return CommandTag::CreatureReaction; },
	                      [](const HandClick& /*command*/) { return CommandTag::HandClick; },
	                      [](const SeedGesture& /*command*/) { return CommandTag::SeedGesture; },
	                      [](const PlayerLeft& /*command*/) { return CommandTag::PlayerLeft; },
	                      [](const HandPick& /*command*/) { return CommandTag::HandPick; },
	                      [](const PlayerJoined& /*command*/) { return CommandTag::PlayerJoined; },
	                      [](const MiracleRequest& /*command*/) { return CommandTag::MiracleRequest; },
	                      [](const MiracleRepeat& /*command*/) { return CommandTag::MiracleRepeat; },
	                      [](const HandCast& /*command*/) { return CommandTag::HandCast; },
	                      [](const HandCastUnlock& /*command*/) { return CommandTag::HandCastUnlock; },
	                      [](const LeashPick& /*command*/) { return CommandTag::LeashPick; },
	                      [](const HandShakeOff& /*command*/) { return CommandTag::HandShakeOff; },
	                      [](const GripDust& /*command*/) { return CommandTag::GripDust; },
	                      [](const HelpReport& /*command*/) { return CommandTag::HelpReport; },
	                      [](const HelpQuery& /*command*/) { return CommandTag::HelpQuery; },
	                      [](const ViewReport& /*command*/) { return CommandTag::ViewReport; },
	                      [](const GiveCallOff& /*command*/) { return CommandTag::GiveCallOff; },
	                      [](const CreatureTattoo& /*command*/) { return CommandTag::CreatureTattoo; },
	                      [](const LeashLength& /*command*/) { return CommandTag::LeashLength; },
	                      [](const CreatureFightCheat& /*command*/) { return CommandTag::CreatureFightCheat; },
	                      [](const TempleLeashOff& /*command*/) { return CommandTag::TempleLeashOff; },
	                  },
	                  action);
}

size_t SizeOf(const Action& action) noexcept
{
	using namespace sizes;
	const size_t fields =
	    std::visit(Overloaded {
	                   [](const LeashKey& /*command*/) { return size_t {1}; },
	                   [](const CreatureFightCheat& /*command*/) { return size_t {0}; },
	                   [](const TempleLeashOff& /*command*/) { return size_t {0}; },
	                   [](const LeashTap& /*command*/) { return k_Entity; },
	                   [](const CreatureFeedback& /*command*/) { return k_Entity + 4; },
	                   [](const HandTake& /*command*/) { return k_Entity; },
	                   [](const HandLetGo& /*command*/) { return k_Entity + 4 * k_Vector; },
	                   [](const HandTap& /*command*/) { return k_Entity; },
	                   [](const HandGive& /*command*/) { return 2 * k_Entity + 1; },
	                   [](const LeashOrder& /*command*/) { return 1 + k_Entity + k_Vector; },
	                   [](const LeashPostTap& /*command*/) { return k_Entity; },
	                   [](const HandReport& /*command*/) { return 4 * k_Vector; },
	                   [](const HandTwist& /*command*/) { return k_Entity + k_Vector; },
	                   [](const HandLock& /*command*/) { return k_Entity; },
	                   [](const HandUnlock& /*command*/) { return k_Entity; },
	                   [](const HandApply& /*command*/) { return k_Entity; },
	                   [](const TotemShare& /*command*/) { return k_Entity + 4; },
	                   [](const CreatureBruise& /*command*/) { return k_Entity + 7; },
	                   [](const FightGround& /*command*/) { return k_Entity + k_Vector + 1; },
	                   [](const FightMove& /*command*/) { return k_Entity + 1 + 4 + 1; },
	                   [](const FightCharge& /*command*/) { return size_t {4}; },
	                   [](const CreatureReaction& /*command*/) { return k_Entity + 4 + 3 + 4 + 1 + 4; },
	                   [](const HandClick& /*command*/) { return k_Entity + 2 + 4 + 4; },
	                   [](const SeedGesture& /*command*/) { return size_t {2}; },
	                   [](const PlayerLeft& /*command*/) { return size_t {0}; },
	                   [](const HandPick& /*command*/) { return k_Entity; },
	                   [](const PlayerJoined& /*command*/) { return size_t {0}; },
	                   [](const MiracleRequest& /*command*/) { return size_t {1}; },
	                   [](const MiracleRepeat& /*command*/) { return size_t {0}; },
	                   [](const HandCast& /*command*/) { return k_Entity + 4 * k_Vector + 4 + 4; },
	                   [](const HandCastUnlock& /*command*/) { return k_Entity; },
	                   [](const LeashPick& /*command*/) { return size_t {1}; },
	                   [](const HandShakeOff& /*command*/) { return size_t {0}; },
	                   [](const GripDust& /*command*/) { return k_Vector; },
	                   [](const HelpReport& /*command*/) { return size_t {8 + 1}; },
	                   [](const HelpQuery& /*command*/) { return k_Entity + 1 + k_Vector; },
	                   [](const ViewReport& /*command*/) { return 2 * k_Vector + 6 * 4 + 1; },
	                   [](const GiveCallOff& /*command*/) { return k_Entity; },
	                   [](const CreatureTattoo& command) {
		                   return k_Entity + 5 * creature_tattoo::k_SlotCount + 1 + 2 + command.pictures.size();
	                   },
	                   [](const LeashLength& /*command*/) { return size_t {4}; },
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
	        [](const WorldPart& part) { return k_WorldPartHead + part.bytes.size(); },
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
	               [&](const WorldPart& part) {
		               writer.Byte(static_cast<uint8_t>(FrameKind::WorldPart));
		               writer.Number(part.turn);
		               writer.Number(part.part);
		               writer.Number(part.parts);
		               for (const auto byte : part.bytes)
		               {
			               writer.Byte(byte);
		               }
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

std::vector<WorldPart> SplitWorld(uint32_t turn, std::span<const uint8_t> state)
{
	const auto count = std::max<size_t>(1, (state.size() + k_WorldPartBytes - 1) / k_WorldPartBytes);
	std::vector<WorldPart> parts;
	if (count > std::numeric_limits<uint16_t>::max())
	{
		return parts;
	}
	parts.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		const auto from = i * k_WorldPartBytes;
		const auto length = std::min(k_WorldPartBytes, state.size() - std::min(from, state.size()));
		const auto bytes = state.subspan(std::min(from, state.size()), length);
		parts.push_back({.turn = turn,
		                 .part = static_cast<uint16_t>(i),
		                 .parts = static_cast<uint16_t>(count),
		                 .bytes = std::vector<uint8_t>(bytes.begin(), bytes.end())});
	}
	return parts;
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
	case FrameKind::WorldPart:
	{
		WorldPart part;
		part.turn = in.Number<uint32_t>();
		part.part = in.Number<uint16_t>();
		part.parts = in.Number<uint16_t>();
		if (in.Bad())
		{
			return Fail(DecodeError::Truncated);
		}
		if (part.part >= part.parts || in.Left() > k_WorldPartBytes)
		{
			return Fail(DecodeError::Value);
		}
		part.bytes.reserve(in.Left());
		while (!in.AtEnd())
		{
			part.bytes.push_back(in.Byte());
		}
		frame = std::move(part);
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
