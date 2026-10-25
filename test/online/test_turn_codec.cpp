/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <limits>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "Commands/TurnCodec.h"

using namespace openblack;
using namespace openblack::turn_codec;
using namespace openblack::player_commands;

namespace
{

entt::entity Entity(uint32_t value)
{
	return static_cast<entt::entity>(value);
}

/// One command of each kind, with fields that show the order of the bytes
std::vector<Action> EachCommand()
{
	return {
	    LeashKey {.key = creature_leash::LeashKey::NextLeash},
	    LeashTap {.creature = Entity(0x01020304)},
	    CreatureFeedback {.creature = Entity(7), .amount = -0.75f},
	    HandTake {.object = Entity(8)},
	    HandLetGo {
	        .object = Entity(9),
	        .velocity = {1.5f, -2.25f, 1e-30f},
	        .start = {.angularMomentum = {0.0f, -0.0f, 2.0f}, .origin = {100.5f, 7.0f, -9.0f}, .angles = {0.1f, -3.1f, 1.5f}}},
	    HandTap {.object = entt::null},
	    HandGive {.creature = Entity(10), .object = Entity(11)},
	    LeashOrder {.kind = LeashOrder::Kind::AtPlace, .object = entt::null, .place = {1000.5f, 12.0f, -3.0f}},
	    LeashPostTap {.post = Entity(12)},
	    HandReport {.collidePoint = {1.0f, 2.0f, 3.0f},
	                .handPoint = {4.0f, 5.0f, 6.0f},
	                .camera = {7.0f, 8.0f, 9.0f},
	                .focus = {10.0f, 11.0f, 12.0f}},
	    HandTwist {.object = Entity(13), .moved = {0.5f, 0.0f, -0.25f}},
	    HandLock {.object = Entity(14)},
	    HandUnlock {.object = Entity(15)},
	    HandApply {.target = Entity(16)},
	    TotemShare {.totem = Entity(17), .share = 0.75f},
	    CreatureBruise {.creature = Entity(18),
	                    .wound = creature_marks::Mark {.u = 1, .v = 2, .skin = 3, .age = 0, .type = 0, .column = 7}},
	    PlayerLeft {},
	    FightGround {.creature = Entity(19), .point = {-1.5f, 2.0f, 3.25f}, .replace = true},
	    FightMove {.target = Entity(20), .kind = 6, .value = 0x01020304, .replace = false},
	    FightCharge {.heldMs = 1200},
	    CreatureReaction {.creature = Entity(21),
	                      .animation = 0x0A0B0C0D,
	                      .mirrored = true,
	                      .pullsFace = true,
	                      .face = 3,
	                      .faceMs = 2000.0f,
	                      .cue = 15,
	                      .interruptsAfter = 0.25f},
	    HandClick {.thing = Entity(22), .release = true, .hasPlace = true, .placeX = -5, .placeZ = 0x7FFFFFFF},
	    SeedGesture {.kind = SeedGesture::Kind::PowerUp, .level = -1},
	    HandPick {.object = Entity(23)},
	    PlayerJoined {},
	    MiracleRequest {.seed = SpellSeedType::Heal},
	    MiracleRepeat {},
	    HandCast {.object = Entity(24),
	              .point = {1.0f, 2.0f, 3.0f},
	              .size = 12.5f,
	              .handPosition = {4.0f, 5.0f, 6.0f},
	              .cameraForward = {0.0f, -0.6f, 0.8f},
	              .handVelocity = {-7.0f, 0.0f, 8.0f},
	              .spin = -0.5f},
	    HandCastUnlock {.seed = Entity(25)},
	    HelpReport {.events = 0x0102030405060708,
	                .boxShows = false,
	                .cameraShowsWorld = true,
	                .landAtCentre = true,
	                .centreDistance = 14.5f,
	                .pitch = -0.25f,
	                .cursorOnPoint = true},
	    HelpQuery {.thing = Entity(26), .hasGround = true, .ground = {1.0f, 2.0f, 3.0f}},
	};
}

/// A frame that goes to bytes and back gives the same bytes: every field, also every bit of each float, survives
void ExpectRoundTrip(const Frame& frame)
{
	const auto bytes = Encode(frame);
	ASSERT_FALSE(bytes.empty());
	EXPECT_EQ(bytes.size(), SizeOf(frame));
	const auto decoded = Decode(bytes);
	ASSERT_EQ(decoded.error, DecodeError::None);
	ASSERT_TRUE(decoded.frame.has_value());
	EXPECT_EQ(decoded.frame->index(), frame.index());
	EXPECT_EQ(Encode(*decoded.frame), bytes);
}

/// A random command, with random bits in the floats (also NaNs, infinities and denormals)
Action RandomAction(std::mt19937& random)
{
	std::uniform_int_distribution<uint32_t> bits;
	const auto entity = [&]() { return Entity(bits(random)); };
	const auto number = [&]() { return std::bit_cast<float>(bits(random)); };
	const auto vector = [&]() { return glm::vec3 {number(), number(), number()}; };
	switch (bits(random) % 31)
	{
	case 0:
		return LeashKey {.key = static_cast<creature_leash::LeashKey>(bits(random) % 3)};
	case 1:
		return LeashTap {.creature = entity()};
	case 2:
		return CreatureFeedback {.creature = entity(), .amount = number()};
	case 3:
		return HandTake {.object = entity()};
	case 4:
		return HandLetGo {.object = entity(),
		                  .velocity = vector(),
		                  .start = {.angularMomentum = vector(), .origin = vector(), .angles = vector()}};
	case 5:
		return HandTap {.object = entity()};
	case 6:
		return HandGive {.creature = entity(), .object = entity()};
	case 7:
		return LeashOrder {.kind = static_cast<LeashOrder::Kind>(bits(random) % 4), .object = entity(), .place = vector()};
	case 8:
		return LeashPostTap {.post = entity()};
	case 9:
		return HandReport {.collidePoint = vector(), .handPoint = vector(), .camera = vector(), .focus = vector()};
	case 10:
		return HandTwist {.object = entity(), .moved = vector()};
	case 11:
		return HandLock {.object = entity()};
	case 12:
		return HandUnlock {.object = entity()};
	case 13:
		return HandApply {.target = entity()};
	case 14:
		return TotemShare {.totem = entity(), .share = number()};
	case 15:
		return FightGround {.creature = entity(), .point = vector(), .replace = bits(random) % 2 == 0};
	case 16:
		return FightMove {.target = entity(),
		                  .kind = static_cast<uint8_t>(bits(random) % 7),
		                  .value = bits(random),
		                  .replace = bits(random) % 2 == 0};
	case 17:
		return FightCharge {.heldMs = bits(random)};
	case 18:
	{
		const bool face = bits(random) % 2 == 0;
		return CreatureReaction {.creature = entity(),
		                         .animation = bits(random),
		                         .mirrored = bits(random) % 2 == 0,
		                         .pullsFace = face,
		                         .face = face ? static_cast<uint8_t>(bits(random) % 12) : uint8_t {0},
		                         .faceMs = face ? number() : 0.0f,
		                         .cue = face ? static_cast<uint8_t>(bits(random) % 19) : uint8_t {0},
		                         .interruptsAfter = number()};
	}
	case 22:
		return HandPick {.object = entity()};
	case 20:
		return SeedGesture {.kind = static_cast<SeedGesture::Kind>(bits(random) % 3),
		                    .level = static_cast<int8_t>(bits(random))};
	case 19:
	{
		const bool release = bits(random) % 2 == 0;
		const bool place = release && bits(random) % 2 == 0;
		return HandClick {.thing = entity(),
		                  .release = release,
		                  .hasPlace = place,
		                  .placeX = place ? static_cast<int32_t>(bits(random)) : 0,
		                  .placeZ = place ? static_cast<int32_t>(bits(random)) : 0};
	}
	case 21:
	{
		const auto field = [&]() { return static_cast<uint8_t>(bits(random)); };
		return CreatureBruise {
		    .creature = entity(),
		    .wound = bits(random) % 2 == 0
		                 ? std::optional(creature_marks::Mark {
		                       .u = field(), .v = field(), .skin = field(), .age = field(), .type = field(), .column = field()})
		                 : std::nullopt};
	}
	case 23:
		return PlayerJoined {};
	case 24:
		return MiracleRequest {.seed = static_cast<SpellSeedType>(bits(random) % 30)};
	case 25:
		return MiracleRepeat {};
	case 26:
		return HandCast {.object = entity(),
		                 .point = vector(),
		                 .size = number(),
		                 .handPosition = vector(),
		                 .cameraForward = vector(),
		                 .handVelocity = vector(),
		                 .spin = number()};
	case 27:
		return HandCastUnlock {.seed = entity()};
	case 28:
		return HelpReport {.events = (uint64_t {bits(random)} << 32) | bits(random),
		                   .boxShows = bits(random) % 2 == 0,
		                   .cameraShowsWorld = bits(random) % 2 == 0,
		                   .landAtCentre = bits(random) % 2 == 0,
		                   .centreDistance = number(),
		                   .pitch = number(),
		                   .cursorOnPoint = bits(random) % 2 == 0};
	case 29:
		return HelpQuery {.thing = entity(), .hasGround = bits(random) % 2 == 0, .ground = vector()};
	default:
		return PlayerLeft {};
	}
}

std::vector<Action> RandomActions(std::mt19937& random)
{
	std::vector<Action> actions(random() % (k_MostBlockCommands + 1));
	for (auto& action : actions)
	{
		action = RandomAction(random);
	}
	return actions;
}

Frame RandomFrame(std::mt19937& random)
{
	switch (random() % 5)
	{
	case 0:
		return CommandBlock {.sequence = static_cast<uint16_t>(random()),
		                     .turn = static_cast<uint32_t>(random()),
		                     .actions = RandomActions(random)};
	case 1:
	{
		TurnBundle turn {.turn = static_cast<uint32_t>(random()), .blocks = {}};
		turn.blocks.resize(random() % 40);
		for (auto& block : turn.blocks)
		{
			block = StampedBlock {.sender = static_cast<PlayerNames>(random() % 32),
			                      .sequence = static_cast<uint16_t>(random()),
			                      .actions = RandomActions(random)};
		}
		return turn;
	}
	case 2:
		return TurnChecksum {.turn = static_cast<uint32_t>(random()), .checksum = (uint64_t {random()} << 32) | random()};
	case 3:
		return Ping {.id = static_cast<uint32_t>(random()), .time = (uint64_t {random()} << 32) | random()};
	default:
		return Pong {.id = static_cast<uint32_t>(random()), .time = (uint64_t {random()} << 32) | random()};
	}
}

/// The commands of a busy player in one turn: the hand lets go of a thing and the creature gets an order on its leash.
/// The report of the hand and the camera (the point under the hand, the place of the hand, the camera and its focus:
/// four vectors and a tag) goes once a turn too. The table counts it as 49 bytes.
constexpr size_t k_HandReport = 1 + 4 * sizes::k_Vector;

std::vector<Action> BusyCommands()
{
	return {
	    HandLetGo {.object = Entity(1), .velocity = {1.0f, 2.0f, 3.0f}},
	    LeashOrder {.kind = LeashOrder::Kind::OnThing, .object = Entity(2), .place = {}},
	};
}

} // namespace

TEST(TurnCodec, EachCommandHasItsSize)
{
	const std::vector<size_t> sizes {2, 5,  9,  5, 53, 5,  9, 18, 5, 49, 17, 5,  5, 5,  9, 12,
	                                 1, 18, 11, 5, 21, 15, 3, 5,  1, 2,  1,  61, 5, 18, 18};
	const auto commands = EachCommand();
	ASSERT_EQ(commands.size(), sizes.size());
	for (size_t i = 0; i < commands.size(); ++i)
	{
		EXPECT_EQ(SizeOf(commands[i]), sizes[i]) << i;
		// The tags follow the order of the commands. The leave of the server has tag 17.
		const size_t tag = std::holds_alternative<PlayerLeft>(commands[i]) ? 17 : i + 1;
		EXPECT_EQ(static_cast<size_t>(TagOf(commands[i])), tag) << i;
	}
}

TEST(TurnCodec, TheBytesOfABlockAreLittleEndianWithTheRawBitsOfTheFloats)
{
	const CommandBlock block {
	    .sequence = 0x0102,
	    .turn = 0x0A0B0C0D,
	    .actions = {CreatureFeedback {.creature = Entity(0x11223344), .amount = -0.75f}},
	};
	const std::vector<uint8_t> expected {
	    k_Version, 1,                // version, kind Commands
	    0x02,      0x01,             // sequence
	    0x0D,      0x0C, 0x0B, 0x0A, // turn
	    1,                           // count
	    3,                           // tag CreatureFeedback
	    0x44,      0x33, 0x22, 0x11, // creature
	    0x00,      0x00, 0x40, 0xBF, // -0.75f is 0xBF400000
	};
	EXPECT_EQ(Encode(block), expected);
}

TEST(TurnCodec, EachFrameKindGoesToBytesAndBack)
{
	ExpectRoundTrip(CommandBlock {.sequence = 65535, .turn = 4'000'000'000U, .actions = {}});
	auto commands = EachCommand();
	commands.resize(k_MostBlockCommands);
	ExpectRoundTrip(CommandBlock {.sequence = 3, .turn = 81'200, .actions = commands});
	const auto all = EachCommand();
	TurnBundle turn {.turn = 81'201, .blocks = {}};
	for (size_t i = 0; i < all.size(); ++i)
	{
		turn.blocks.push_back(
		    {.sender = static_cast<PlayerNames>((i * 2) % 31), .sequence = static_cast<uint16_t>(i), .actions = {all[i]}});
	}
	ExpectRoundTrip(turn);
	ExpectRoundTrip(TurnBundle {.turn = 0, .blocks = {}});
	ExpectRoundTrip(TurnChecksum {.turn = 81'201, .checksum = 0xFEDCBA9876543210ULL});
	ExpectRoundTrip(Ping {.id = 5, .time = 123'456'789'012ULL});
	ExpectRoundTrip(Pong {.id = 5, .time = 123'456'789'012ULL});
}

TEST(TurnCodec, TheFieldsOfTheCommandsComeBackExactly)
{
	const auto denormal = std::numeric_limits<float>::denorm_min();
	const TurnBundle turn {
	    .turn = 9,
	    .blocks = {{
	        .sender = static_cast<PlayerNames>(31),
	        .sequence = 77,
	        .actions =
	            {
	                HandLetGo {.object = Entity(42),
	                           .velocity = {-0.0f, 1.0f, denormal},
	                           .start = {.angularMomentum = {-0.0f, 0.0f, 5.0f},
	                                     .origin = {11.0f, 12.0f, 13.0f},
	                                     .angles = {0.5f, -0.25f, 2.0f}}},
	                CreatureFeedback {.creature = Entity(43), .amount = 2.0f},
	                LeashOrder {.kind = LeashOrder::Kind::Tie, .object = Entity(44), .place = {1.0f, 2.0f, 3.0f}},
	                LeashKey {.key = creature_leash::LeashKey::PreviousLeash},
	            },
	    }},
	};
	auto bytes = Encode(turn);
	ASSERT_EQ(bytes.size(), 94u);
	// The test puts NaNs in the bytes, because a NaN constant of the compiler can lose its signalling bit. The velocity's
	// y is at byte 21, and the amount is at byte 70.
	const auto put = [&bytes](size_t at, uint32_t value) {
		for (size_t i = 0; i < 4; ++i)
		{
			bytes[at + i] = static_cast<uint8_t>(value >> (8 * i));
		}
	};
	ASSERT_EQ(bytes[12], static_cast<uint8_t>(CommandTag::HandLetGo));
	put(21, 0x7F800001U);
	put(70, 0xFFC12345U);
	// After the velocity come the spin, the origin and the angles of the start, in this order
	const auto at = [&bytes](size_t from) {
		uint32_t value = 0;
		for (size_t i = 0; i < 4; ++i)
		{
			value |= static_cast<uint32_t>(bytes[from + i]) << (8 * i);
		}
		return value;
	};
	EXPECT_EQ(at(29), 0x80000000U);
	EXPECT_EQ(at(37), std::bit_cast<uint32_t>(5.0f));
	EXPECT_EQ(at(41), std::bit_cast<uint32_t>(11.0f));
	EXPECT_EQ(at(53), std::bit_cast<uint32_t>(0.5f));
	EXPECT_EQ(at(61), std::bit_cast<uint32_t>(2.0f));
	const auto decoded = Decode(bytes);
	ASSERT_EQ(decoded.error, DecodeError::None);
	EXPECT_EQ(Encode(*decoded.frame), bytes);
	const auto& back = std::get<TurnBundle>(*decoded.frame);
	ASSERT_EQ(back.blocks.size(), 1u);
	const auto& block = back.blocks[0];
	EXPECT_EQ(static_cast<int>(block.sender), 31);
	EXPECT_EQ(block.sequence, 77);
	ASSERT_EQ(block.actions.size(), 4u);
	const auto& letGo = std::get<HandLetGo>(block.actions[0]);
	EXPECT_EQ(letGo.object, Entity(42));
	EXPECT_EQ(std::bit_cast<uint32_t>(letGo.velocity.x), 0x80000000U);
	EXPECT_EQ(std::bit_cast<uint32_t>(letGo.velocity.y), 0x7F800001U);
	EXPECT_EQ(std::bit_cast<uint32_t>(letGo.velocity.z), std::bit_cast<uint32_t>(denormal));
	EXPECT_EQ(std::bit_cast<uint32_t>(letGo.start.angularMomentum.x), 0x80000000U);
	EXPECT_EQ(letGo.start.origin, glm::vec3(11.0f, 12.0f, 13.0f));
	EXPECT_EQ(letGo.start.angles, glm::vec3(0.5f, -0.25f, 2.0f));
	EXPECT_EQ(std::bit_cast<uint32_t>(std::get<CreatureFeedback>(block.actions[1]).amount), 0xFFC12345U);
	const auto& order = std::get<LeashOrder>(block.actions[2]);
	EXPECT_EQ(order.kind, LeashOrder::Kind::Tie);
	EXPECT_EQ(order.object, Entity(44));
	EXPECT_EQ(order.place.z, 3.0f);
	EXPECT_EQ(std::get<LeashKey>(block.actions[3]).key, creature_leash::LeashKey::PreviousLeash);
}

TEST(TurnCodec, TheEncoderRefusesAFrameThatBreaksALimit)
{
	auto commands = EachCommand();
	commands.resize(k_MostBlockCommands + 1, HandTake {.object = Entity(1)});
	EXPECT_TRUE(Encode(CommandBlock {.sequence = 0, .turn = 0, .actions = commands}).empty());
	EXPECT_TRUE(
	    Encode(TurnBundle {.turn = 0, .blocks = {{.sender = static_cast<PlayerNames>(32), .sequence = 0, .actions = {}}}})
	        .empty());
	// A turn larger than the largest frame
	TurnBundle huge {.turn = 0, .blocks = {}};
	huge.blocks.resize(2000, StampedBlock {.sender = PlayerNames::PLAYER_ONE, .sequence = 0, .actions = commands});
	for (auto& block : huge.blocks)
	{
		block.actions.resize(k_MostBlockCommands);
	}
	EXPECT_GT(SizeOf(huge), k_MostFrameBytes);
	std::vector<uint8_t> out {1, 2, 3};
	EXPECT_FALSE(Append(huge, out));
	EXPECT_EQ(out, (std::vector<uint8_t> {1, 2, 3}));
}

TEST(TurnCodec, TheDecoderRefusesABadFrame)
{
	const auto block = Encode(CommandBlock {.sequence = 1, .turn = 2, .actions = {HandTake {.object = Entity(5)}}});

	EXPECT_EQ(Decode({}).error, DecodeError::Empty);
	auto wrong = block;
	wrong[0] = k_Version + 1;
	EXPECT_EQ(Decode(wrong).error, DecodeError::Version);
	wrong = block;
	wrong[1] = 9;
	EXPECT_EQ(Decode(wrong).error, DecodeError::Kind);
	wrong = block;
	wrong.push_back(0);
	EXPECT_EQ(Decode(wrong).error, DecodeError::TrailingBytes);
	wrong = block;
	wrong[9] = 0;
	EXPECT_EQ(Decode(wrong).error, DecodeError::Command);
	wrong = block;
	wrong[9] = 200;
	EXPECT_EQ(Decode(wrong).error, DecodeError::Command);
	wrong = block;
	wrong[8] = static_cast<uint8_t>(k_MostBlockCommands + 1);
	EXPECT_EQ(Decode(wrong).error, DecodeError::Value);

	auto key = Encode(CommandBlock {.sequence = 0, .turn = 0, .actions = {LeashKey {.key = creature_leash::LeashKey::Leash}}});
	key.back() = 3;
	EXPECT_EQ(Decode(key).error, DecodeError::Value);
	auto order = Encode(CommandBlock {.sequence = 0, .turn = 0, .actions = {LeashOrder {.kind = LeashOrder::Kind::Untie}}});
	order[10] = 4;
	EXPECT_EQ(Decode(order).error, DecodeError::Value);

	// A fight move of a kind that no move has, or a button flag that is not 0 or 1
	auto move = Encode(CommandBlock {.sequence = 0, .turn = 0, .actions = {FightMove {.target = Entity(1), .kind = 6}}});
	move[14] = 7;
	EXPECT_EQ(Decode(move).error, DecodeError::Value);
	move[14] = 6;
	move.back() = 2;
	EXPECT_EQ(Decode(move).error, DecodeError::Value);
	// A reaction with no face keeps the bytes of the face empty
	auto reaction = Encode(CommandBlock {.sequence = 0, .turn = 0, .actions = {CreatureReaction {.creature = Entity(1)}}});
	EXPECT_EQ(Decode(reaction).error, DecodeError::None);
	reaction[20] = 1;
	EXPECT_EQ(Decode(reaction).error, DecodeError::Value);
	// A press of the hand has no place
	auto click = Encode(CommandBlock {.sequence = 0, .turn = 0, .actions = {HandClick {.thing = Entity(1)}}});
	EXPECT_EQ(Decode(click).error, DecodeError::None);
	click[15] = 1;
	EXPECT_EQ(Decode(click).error, DecodeError::Value);
	// A seed gesture of no kind
	auto gesture = Encode(CommandBlock {.sequence = 0, .turn = 0, .actions = {SeedGesture {}}});
	gesture[10] = 3;
	EXPECT_EQ(Decode(gesture).error, DecodeError::Value);

	auto turn = Encode(TurnBundle {.turn = 1, .blocks = {{.sender = PlayerNames::NEUTRAL, .sequence = 0, .actions = {}}}});
	turn[8] = 32;
	EXPECT_EQ(Decode(turn).error, DecodeError::Value);

	std::vector<uint8_t> tooLarge(k_MostFrameBytes + 1, 0);
	tooLarge[0] = k_Version;
	EXPECT_EQ(Decode(tooLarge).error, DecodeError::TooLarge);
}

TEST(TurnCodec, EveryCutOfAFrameIsTruncated)
{
	const auto all = EachCommand();
	TurnBundle turn {.turn = 3, .blocks = {}};
	for (size_t i = 0; i < all.size(); ++i)
	{
		turn.blocks.push_back({.sender = PlayerNames::PLAYER_TWO, .sequence = static_cast<uint16_t>(i), .actions = {all[i]}});
	}
	for (const Frame& frame : {Frame {turn}, Frame {CommandBlock {.sequence = 1, .turn = 2, .actions = {all[4], all[7]}}},
	                           Frame {TurnChecksum {.turn = 1, .checksum = 2}}, Frame {Ping {.id = 1, .time = 2}}})
	{
		const auto bytes = Encode(frame);
		for (size_t cut = 1; cut < bytes.size(); ++cut)
		{
			const auto decoded = Decode(std::span(bytes).first(cut));
			EXPECT_EQ(decoded.error, DecodeError::Truncated) << "cut at " << cut << " of " << bytes.size();
			EXPECT_FALSE(decoded.frame.has_value());
		}
	}
}

TEST(TurnCodec, RandomFramesGoToBytesAndBack)
{
	std::mt19937 random(20261022);
	for (int i = 0; i < 2000; ++i)
	{
		ExpectRoundTrip(RandomFrame(random));
	}
}

TEST(TurnCodec, RandomChangesToTheBytesNeverGiveAFrameWithOtherBytes)
{
	// A changed frame either fails, or reads as a frame that gives exactly the changed bytes again
	std::mt19937 random(7);
	for (int i = 0; i < 3000; ++i)
	{
		auto bytes = Encode(RandomFrame(random));
		const auto changes = 1 + random() % 4;
		for (uint32_t change = 0; change < changes; ++change)
		{
			switch (random() % 3)
			{
			case 0:
				bytes[random() % bytes.size()] = static_cast<uint8_t>(random());
				break;
			case 1:
				bytes.resize(random() % (bytes.size() + 1));
				break;
			default:
				bytes.push_back(static_cast<uint8_t>(random()));
				break;
			}
			if (bytes.empty())
			{
				break;
			}
		}
		const auto decoded = Decode(bytes);
		if (decoded.error == DecodeError::None)
		{
			ASSERT_TRUE(decoded.frame.has_value());
			EXPECT_EQ(Encode(*decoded.frame), bytes);
		}
		else
		{
			EXPECT_FALSE(decoded.frame.has_value());
		}
	}
}

TEST(TurnCodec, TheReportOfTheHandKeepsTheOrderOfItsPoints)
{
	const HandReport report {.collidePoint = {1.0f, 2.0f, 3.0f},
	                         .handPoint = {4.0f, 5.0f, 6.0f},
	                         .camera = {7.0f, 8.0f, 9.0f},
	                         .focus = {10.0f, 11.0f, 12.0f}};
	const auto bytes = Encode(CommandBlock {.sequence = 0, .turn = 1, .actions = {report}});
	ASSERT_EQ(bytes.size(), sizes::k_Header + sizes::k_CommandsBody + k_HandReport);
	ASSERT_EQ(bytes[9], static_cast<uint8_t>(CommandTag::HandReport));
	// Twelve floats after the tag: the collide point, the hand, the camera and the focus, each x, y, z
	for (size_t i = 0; i < 12; ++i)
	{
		uint32_t value = 0;
		for (size_t b = 0; b < 4; ++b)
		{
			value |= static_cast<uint32_t>(bytes[10 + 4 * i + b]) << (8 * b);
		}
		EXPECT_EQ(std::bit_cast<float>(value), static_cast<float>(i + 1)) << i;
	}
	const auto decoded = Decode(bytes);
	ASSERT_EQ(decoded.error, DecodeError::None);
	EXPECT_EQ(std::get<HandReport>(std::get<CommandBlock>(*decoded.frame).actions[0]), report);
}

TEST(TurnCodec, ATurnOf32BusyPlayersFitsTheBudgetOfTheReport)
{
	// The report of multiplayer-32-1: about 84 bytes of commands, hand and camera a player, and about 2.7 KB a turn.
	// Since that report, the let-go has its start: 36 bytes more.
	const auto commands = BusyCommands();
	size_t commandBytes = 0;
	for (const auto& action : commands)
	{
		commandBytes += SizeOf(action);
	}
	EXPECT_EQ(commandBytes, 71u);
	const auto playerBytes = commandBytes + k_HandReport;
	EXPECT_EQ(playerBytes, 120u);

	TurnBundle turn {.turn = 1, .blocks = {}};
	for (size_t player = 0; player < 32; ++player)
	{
		turn.blocks.push_back({.sender = static_cast<PlayerNames>(player), .sequence = 0, .actions = commands});
	}
	const auto encoded = Encode(turn);
	// The commands without the hand report
	EXPECT_EQ(encoded.size(), sizes::k_Header + sizes::k_TurnBody + 32 * (sizes::k_Stamp + commandBytes));
	EXPECT_EQ(encoded.size(), 2408u);
	// With the hand report: 3976 bytes. The stamps and the counts add less than 4 percent to the 3840 bytes
	// (32 x 120) of the report.
	const auto withReport = encoded.size() + 32 * k_HandReport;
	EXPECT_EQ(withReport, 3976u);
	EXPECT_LT(withReport * 100, 3840u * 104);

	// The block that one busy client sends, and its checksum
	const auto up = Encode(CommandBlock {.sequence = 0, .turn = 1, .actions = commands});
	EXPECT_EQ(up.size() + k_HandReport, 129u);
	EXPECT_EQ(Encode(TurnChecksum {.turn = 1, .checksum = 2}).size(), 14u);
	EXPECT_EQ(Encode(Ping {.id = 1, .time = 2}).size(), 14u);
}

TEST(TurnCodec, AWorldPartGoesToBytesAndBack)
{
	ExpectRoundTrip(WorldPart {.turn = 7, .part = 1, .parts = 3, .bytes = {1, 2, 3, 250}});
	ExpectRoundTrip(WorldPart {.turn = 7, .part = 0, .parts = 1, .bytes = {}});
	const auto bytes = Encode(WorldPart {.turn = 0x01020304, .part = 2, .parts = 5, .bytes = {9}});
	const std::vector<uint8_t> expected {k_Version, 6, 4, 3, 2, 1, 2, 0, 5, 0, 9};
	EXPECT_EQ(bytes, expected);
}

TEST(TurnCodec, AWorldPartOutOfItsCountIsRefused)
{
	EXPECT_TRUE(Encode(WorldPart {.turn = 1, .part = 3, .parts = 3, .bytes = {}}).empty());
	EXPECT_TRUE(
	    Encode(WorldPart {.turn = 1, .part = 0, .parts = 1, .bytes = std::vector<uint8_t>(k_WorldPartBytes + 1)}).empty());
	const std::vector<uint8_t> bad {k_Version, 6, 1, 0, 0, 0, 3, 0, 3, 0};
	EXPECT_EQ(Decode(bad).error, DecodeError::Value);
}

TEST(TurnCodec, AWorldStateSplitsIntoPartsThatGoBackTogether)
{
	std::vector<uint8_t> state((k_WorldPartBytes * 2) + 17);
	for (size_t i = 0; i < state.size(); ++i)
	{
		state[i] = static_cast<uint8_t>(i * 31);
	}
	const auto parts = SplitWorld(40, state);
	ASSERT_EQ(parts.size(), 3u);
	std::vector<uint8_t> joined;
	for (size_t i = 0; i < parts.size(); ++i)
	{
		EXPECT_EQ(parts[i].turn, 40u);
		EXPECT_EQ(parts[i].part, i);
		EXPECT_EQ(parts[i].parts, 3u);
		EXPECT_LE(parts[i].bytes.size(), k_WorldPartBytes);
		EXPECT_FALSE(Encode(parts[i]).empty());
		joined.insert(joined.end(), parts[i].bytes.begin(), parts[i].bytes.end());
	}
	EXPECT_EQ(joined, state);
	// An empty state is one empty part
	const auto none = SplitWorld(3, {});
	ASSERT_EQ(none.size(), 1u);
	EXPECT_TRUE(none[0].bytes.empty());
}

TEST(TurnCodec, AMiracleRequestCarriesOnlyASeedOfTheGame)
{
	const auto bytes =
	    Encode(CommandBlock {.sequence = 0, .turn = 0, .actions = {MiracleRequest {.seed = SpellSeedType::BeamExplosion}}});
	const auto decoded = Decode(bytes);
	ASSERT_EQ(decoded.error, DecodeError::None);
	ASSERT_TRUE(decoded.frame.has_value());
	const auto& block = std::get<CommandBlock>(*decoded.frame);
	EXPECT_EQ(std::get<MiracleRequest>(block.actions.at(0)).seed, SpellSeedType::BeamExplosion);

	// A seed number after the last seed of the game is refused
	auto wrong = bytes;
	wrong.back() = static_cast<uint8_t>(SpellSeedType::BeamExplosion) + 1;
	EXPECT_EQ(Decode(wrong).error, DecodeError::Value);
}

TEST(TurnCodec, AHelpReportKeepsItsEventsAndRefusesUnknownFlags)
{
	const HelpReport report {.events = uint64_t {1} << 48, .boxShows = true, .cameraShowsWorld = false};
	const auto bytes = Encode(CommandBlock {.sequence = 0, .turn = 0, .actions = {report}});
	const auto decoded = Decode(bytes);
	ASSERT_EQ(decoded.error, DecodeError::None);
	ASSERT_TRUE(decoded.frame.has_value());
	EXPECT_EQ(std::get<HelpReport>(std::get<CommandBlock>(*decoded.frame).actions.at(0)), report);

	// The byte of the flags comes after the tag and the eight bytes of the events. A flag after the fourth is refused.
	auto wrong = bytes;
	const auto flagsAt = wrong.size() - 9;
	wrong[flagsAt] = 16;
	EXPECT_EQ(Decode(wrong).error, DecodeError::Value);
}
