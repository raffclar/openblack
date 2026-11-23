/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TurnCodec.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <bit>
#include <initializer_list>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "Common/CreatureEditActs.h"
#include "Common/CreatureEditValues.h"
#include "Common/DebugCheats.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFight.h"
#include "PlayerSlots.h"
#include "Story/StoryLandSkip.h"

namespace openblack::turn_codec
{
namespace
{

using namespace player_commands;

/// The last kind of a fight move, so that a command with another kind does not read
constexpr auto k_LastFightMoveKind = static_cast<uint8_t>(creature_fight::Move::Kind::Spell);

/// The largest block of tattoo pictures: its head, and one picture of 64 by 64 four-bit levels for each slot
constexpr size_t k_MostTattooPictures = 0x28 + creature_tattoo::k_SlotCount * 0x800;

/// The count of the choices of an act on the miracles of a town. The choices go in one byte, one bit each.
constexpr unsigned k_TownMiracleFlagCount = 5;

uint8_t TownMiracleFlags(const DebugTownMiracle& command)
{
	const std::array choices {command.give, command.withPlain, command.allLevels, command.openSite, command.anyLand};
	unsigned flags = 0;
	for (unsigned i = 0; i < choices.size(); ++i)
	{
		flags |= (choices.at(i) ? 1U : 0U) << i;
	}
	return static_cast<uint8_t>(flags);
}

void SetTownMiracleFlags(DebugTownMiracle& command, uint8_t flags)
{
	const auto bit = [flags](unsigned index) { return ((static_cast<unsigned>(flags) >> index) & 1U) != 0; };
	command.give = bit(0);
	command.withPlain = bit(1);
	command.allLevels = bit(2);
	command.openSite = bit(3);
	command.anyLand = bit(4);
}

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
	void Double(double value) { Number(std::bit_cast<uint64_t>(value)); }
	void Entity(entt::entity value) { Number(static_cast<uint32_t>(entt::to_integral(value))); }
	void Vector(const glm::vec3& value)
	{
		Float(value.x);
		Float(value.y);
		Float(value.z);
	}
	/// The nine numbers of a turn, by columns
	void Matrix(const glm::mat3& value)
	{
		for (int column = 0; column < 3; ++column)
		{
			Vector(value[column]);
		}
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
	float Float()
	{
		const auto value = std::bit_cast<float>(Number<uint32_t>());
		_finite = _finite && std::isfinite(value);
		return value;
	}
	double Double()
	{
		const auto value = std::bit_cast<double>(Number<uint64_t>());
		_finite = _finite && std::isfinite(value);
		return value;
	}
	entt::entity Entity() { return static_cast<entt::entity>(Number<uint32_t>()); }
	glm::vec3 Vector()
	{
		const float x = Float();
		const float y = Float();
		const float z = Float();
		return {x, y, z};
	}
	/// The nine numbers of a turn, by columns
	glm::mat3 Matrix()
	{
		glm::mat3 value {1.0f};
		for (int column = 0; column < 3; ++column)
		{
			value[column] = Vector();
		}
		return value;
	}

	[[nodiscard]] bool Bad() const { return _bad; }
	/// False after a read of a number that is not finite: an infinity or not a number
	[[nodiscard]] bool Finite() const { return _finite; }
	[[nodiscard]] size_t Left() const { return _bytes.size() - _at; }
	[[nodiscard]] bool AtEnd() const { return _at == _bytes.size(); }

private:
	std::span<const uint8_t> _bytes;
	size_t _at {0};
	bool _bad {false};
	bool _finite {true};
};

/// The god slot of a player, as a byte of a frame. The check of the frame before the write makes sure that the player
/// has a slot.
[[nodiscard]] uint8_t SlotByte(PlayerNames player)
{
	return static_cast<uint8_t>(player_slots::SlotOfPlayer(player).value_or(0));
}

/// The players that get a line of the chat: their count, then the god slot of each
void WriteRecipients(Writer& out, const std::vector<PlayerNames>& recipients)
{
	out.Byte(static_cast<uint8_t>(recipients.size()));
	for (const auto recipient : recipients)
	{
		out.Byte(SlotByte(recipient));
	}
}

/// Reads the players that get a line of the chat. A count or a slot past the slots of the game is out of range.
DecodeError ReadRecipients(Reader& in, std::vector<PlayerNames>& recipients)
{
	const auto count = in.Byte();
	if (count > player_slots::k_MaxPlayers)
	{
		return DecodeError::Value;
	}
	recipients.resize(count);
	for (auto& recipient : recipients)
	{
		const auto player = player_slots::PlayerOfSlot(in.Byte());
		if (!player.has_value())
		{
			return DecodeError::Value;
		}
		recipient = *player;
	}
	return in.Bad() ? DecodeError::Truncated : DecodeError::None;
}

/// A new creature of the editor: its species, owner, place and body, then its tattoos
void WriteCreatureSpawn(Writer& out, const EditorCreatureSpawn& spawn)
{
	out.Byte(spawn.species);
	out.Byte(spawn.owner);
	out.Vector(spawn.position);
	out.Float(spawn.radians);
	out.Float(spawn.scale);
	out.Float(spawn.alignment);
	out.Float(spawn.fatness);
	out.Float(spawn.strength);
	out.Byte(static_cast<uint8_t>(spawn.hasTattoos ? 1 : 0));
	for (const auto& slot : spawn.tattoos)
	{
		out.Byte(slot.design);
		out.Byte(slot.site);
		out.Byte(slot.colour.r);
		out.Byte(slot.colour.g);
		out.Byte(slot.colour.b);
	}
}

/// Reads a new creature of the editor. The species is one of the creatures of the game, the owner is one of the players,
/// and each tattoo has a known design and site.
DecodeError ReadCreatureSpawn(Reader& in, EditorCreatureSpawn& spawn)
{
	spawn.species = in.Byte();
	spawn.owner = in.Byte();
	spawn.position = in.Vector();
	spawn.radians = in.Float();
	spawn.scale = in.Float();
	spawn.alignment = in.Float();
	spawn.fatness = in.Float();
	spawn.strength = in.Float();
	const auto hasTattoos = in.Byte();
	for (auto& slot : spawn.tattoos)
	{
		slot.design = in.Byte();
		slot.site = in.Byte();
		slot.colour.r = in.Byte();
		slot.colour.g = in.Byte();
		slot.colour.b = in.Byte();
	}
	if (in.Bad())
	{
		return DecodeError::Truncated;
	}
	const bool known = spawn.species > static_cast<uint8_t>(CreatureType::Unknown) &&
	                   spawn.species < static_cast<uint8_t>(CreatureType::_COUNT) &&
	                   spawn.owner < static_cast<uint8_t>(PlayerNames::_COUNT) && hasTattoos <= 1;
	const bool tattoosKnown = std::ranges::all_of(spawn.tattoos, [](const auto& slot) {
		return slot.design < creature_tattoo::k_DesignCount && slot.site <= creature_tattoo::k_NoSite;
	});
	if (!known || !tattoosKnown)
	{
		return DecodeError::Value;
	}
	spawn.hasTattoos = hasTattoos == 1;
	return DecodeError::None;
}

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
		               out.Entity(command.icon);
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
		               out.Double(command.interruptsAfter);
		               out.Byte(static_cast<uint8_t>(command.slap ? 1 : 0));
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
		                                             (command.otherButton ? 4u : 0u) | (command.mouseWheel ? 8u : 0u) |
		                                             (command.cameraTurned ? 16u : 0u) | (command.insideTemple ? 32u : 0u)));
		               out.Number(command.cameraIcons);
		               // The keys held, as their count and then each key's number
		               out.Byte(static_cast<uint8_t>(command.keysHeld.count()));
		               for (size_t key = 0; key < command.keysHeld.size(); ++key)
		               {
			               if (command.keysHeld.test(key))
			               {
				               out.Byte(static_cast<uint8_t>(key));
			               }
		               }
		               out.Number(command.mouseButtons);
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
	               [&out](const ChatSpeech& command) {
		               WriteRecipients(out, command.recipients);
		               out.Number(static_cast<uint16_t>(command.text.size()));
		               for (const auto character : command.text)
		               {
			               out.Number(static_cast<uint16_t>(character));
		               }
	               },
	               [&out](const ChatTaunt& command) {
		               WriteRecipients(out, command.recipients);
		               out.Number(command.taunt);
	               },
	               [](const QuickSave& /*command*/) {},
	               [](const QuickLoad& /*command*/) {},
	               [](const IconChargeStop& /*command*/) {},
	               [&out](const DialogueClick& command) {
		               out.Byte(static_cast<uint8_t>((command.click ? 1u : 0u) | (command.skipKey ? 2u : 0u)));
	               },
	               [&out](const PuzzleTap& command) {
		               out.Number(static_cast<uint32_t>(command.placeX));
		               out.Number(static_cast<uint32_t>(command.placeZ));
	               },
	               [&out](const FishScare& command) { out.Vector(command.point); },
	               [](const ObserversJoined& /*command*/) {},
	               [](const ObserversLeft& /*command*/) {},
	               [&out](const ObserverIn& command) {
		               out.Byte(static_cast<uint8_t>(command.name.size()));
		               for (const auto character : command.name)
		               {
			               out.Number(static_cast<uint16_t>(character));
		               }
	               },
	               [](const ObserverOut& /*command*/) {},
	               [&out](const HandLockRelease& command) { out.Entity(command.creature); },
	               [&out](const EditorEdit& command) {
		               out.Byte(static_cast<uint8_t>(command.kind));
		               out.Entity(command.object);
		               out.Byte(command.placeKind);
		               out.Number(static_cast<uint32_t>(command.placeType));
		               out.Number(command.challenge);
		               out.Vector(command.position);
		               out.Float(command.radians);
		               out.Float(command.scale);
		               out.Number(command.request);
		               if (command.kind == EditorEdit::Kind::Pose)
		               {
			               out.Matrix(command.rotation);
			               out.Byte(command.keepsAngles ? 1 : 0);
			               out.Vector(command.keptAngles);
			               out.Vector(command.poseScale);
		               }
	               },
	               [&out](const EditorRemake& command) {
		               out.Number(command.request);
		               out.Byte(command.placeKind);
		               out.Number(static_cast<uint32_t>(command.placeType));
		               out.Number(command.challenge);
		               out.Vector(command.position);
		               out.Matrix(command.rotation);
		               out.Byte(command.keepsAngles ? 1 : 0);
		               out.Vector(command.keptAngles);
		               out.Vector(command.scale);
		               out.Byte(command.creature ? 1 : 0);
		               out.Byte(command.owner);
		               out.Number(command.mind);
		               out.Float(command.size);
		               out.Float(command.alignment);
		               out.Float(command.fatness);
		               out.Float(command.strength);
	               },
	               [&out](const DebugCheat& command) {
		               out.Byte(command.cheat);
		               out.Byte(command.on ? 1 : 0);
	               },
	               [&out](const EditorCreatureEdit& command) {
		               out.Entity(command.creature);
		               out.Byte(command.value);
		               out.Float(command.amount);
	               },
	               [&out](const EditorVillagerEdit& command) {
		               out.Entity(command.villager);
		               out.Byte(static_cast<uint8_t>(command.field));
		               out.Byte(command.index);
		               out.Number(command.number);
		               out.Float(command.amount);
	               },
	               [&out](const EditorCreatureSpawn& command) {
		               WriteCreatureSpawn(out, command);
		               out.Number(command.request);
	               },
	               [&out](const EditorMindLoad& command) {
		               out.Byte(static_cast<uint8_t>(command.use));
		               out.Number(command.file);
		               if (command.use == EditorMindLoad::Use::LoadInto)
		               {
			               out.Entity(command.creature);
		               }
		               else
		               {
			               WriteCreatureSpawn(out, command.spawn);
		               }
	               },
	               [&out](const EditorCreatureAct& command) {
		               out.Entity(command.creature);
		               out.Byte(command.act);
		               out.Entity(command.other);
		               out.Vector(command.position);
		               out.Float(command.amount);
		               out.Number(command.index);
		               for (const auto arg : command.args)
		               {
			               out.Byte(arg);
		               }
	               },
	               [&out](const DebugLandSkip& command) { out.Byte(command.land); },
	               [&out](const EditorScript& command) {
		               out.Byte(static_cast<uint8_t>(command.act));
		               out.Number(command.id);
		               out.Number(command.address);
		               out.Number(command.index);
		               out.Number(command.value);
		               out.Byte(static_cast<uint8_t>(command.parameters.size()));
		               for (const auto parameter : command.parameters)
		               {
			               out.Float(parameter);
		               }
	               },
	               [&out](const EditorLandScript& command) {
		               out.Byte(static_cast<uint8_t>(command.act));
		               out.Number(command.command);
		               out.Number(command.file);
	               },
	               [&out](const EditorScriptProgram& command) {
		               out.Byte(static_cast<uint8_t>(command.use));
		               out.Number(command.file);
		               out.Number(command.script);
	               },
	               [&out](const EditorLandStroke& command) {
		               out.Byte(command.mode);
		               out.Float(command.innerRadius);
		               out.Float(command.outerRadius);
		               out.Byte(command.step);
		               out.Byte(command.country);
		               out.Float(command.centre.x);
		               out.Float(command.centre.y);
	               },
	               [&out](const EditorPath& command) {
		               out.Byte(command.kind);
		               out.Number(static_cast<uint16_t>(command.points.size()));
		               for (const auto& point : command.points)
		               {
			               out.Float(point.x);
			               out.Float(point.y);
		               }
		               out.Entity(command.firstHolder);
		               out.Entity(command.lastHolder);
	               },
	               [&out](const DebugAdvisor& command) {
		               out.Byte(static_cast<uint8_t>(command.act));
		               out.Byte(command.advisor);
		               out.Float(command.place.x);
		               out.Float(command.place.y);
		               out.Vector(command.position);
		               out.Byte(command.inWorld ? 1 : 0);
		               out.Byte(command.anim);
		               out.Float(command.speed);
		               out.Byte(command.emotion);
	               },
	               [&out](const DebugWorld& command) {
		               out.Byte(static_cast<uint8_t>(command.act));
		               out.Byte(command.player);
		               out.Byte(command.index);
		               out.Float(command.value);
	               },
	               [&out](const DebugConsoleLine& command) {
		               out.Number(static_cast<uint16_t>(command.text.size()));
		               for (const auto character : command.text)
		               {
			               out.Byte(static_cast<uint8_t>(character));
		               }
	               },
	               [&out](const DebugMagic& command) {
		               out.Byte(static_cast<uint8_t>(command.act));
		               out.Byte(command.magic);
		               out.Vector(command.position);
		               out.Vector(command.forward);
		               out.Entity(command.creature);
		               out.Float(command.value);
		               out.Byte(command.on ? 1 : 0);
	               },
	               [&out](const DebugTownMiracle& command) {
		               out.Number(command.town);
		               out.Byte(command.magic);
		               out.Byte(TownMiracleFlags(command));
	               },
	               [&out](const DebugWeather& command) {
		               out.Byte(static_cast<uint8_t>(command.act));
		               for (const auto value :
		                    {command.temperature, command.rain, command.snow, command.overcast, command.windX, command.windZ})
		               {
			               out.Byte(static_cast<uint8_t>(value));
		               }
		               out.Float(command.seconds);
		               out.Float(command.fadeSeconds);
		               out.Float(command.thunderWait.x);
		               out.Float(command.thunderWait.y);
		               out.Float(command.boltWait.x);
		               out.Float(command.boltWait.y);
		               out.Float(command.cloudHeight);
		               out.Float(command.rainSpeed);
		               out.Entity(command.storm);
		               out.Byte(command.on ? 1 : 0);
	               },
	               [&out](const DebugSky& command) {
		               out.Byte(static_cast<uint8_t>(command.act));
		               out.Float(command.hour);
		               out.Byte(command.on ? 1 : 0);
		               out.Number(static_cast<uint64_t>(command.date));
	               },
	               [&out](const DebugPhysics& command) {
		               out.Entity(command.object);
		               out.Number(command.type);
		               out.Float(command.height);
		               out.Float(command.scale);
		               out.Float(command.place.x);
		               out.Float(command.place.y);
		               out.Vector(command.velocity);
	               },
	               [&out](const DebugParticle& command) {
		               out.Byte(static_cast<uint8_t>(command.act));
		               out.Byte(static_cast<uint8_t>(command.file.size()));
		               for (const auto character : command.file)
		               {
			               out.Byte(static_cast<uint8_t>(character));
		               }
		               out.Byte(command.type);
		               out.Vector(command.position);
		               out.Float(command.magnitude);
		               out.Byte(command.player);
		               out.Byte(command.drawPath);
		               out.Byte(command.targets);
		               out.Number(command.effect);
		               out.Byte(command.paused ? 1 : 0);
	               },
	               [&out](const DebugTempleVillager& command) {
		               out.Byte(static_cast<uint8_t>(command.act));
		               out.Entity(command.town);
		               out.Entity(command.villager);
		               out.Vector(command.home);
		               out.Float(command.value);
		               out.Byte(command.tribe);
		               out.Byte(command.number);
		               out.Byte(command.age);
		               out.Byte(command.count);
		               out.Byte(command.send ? 1 : 0);
		               out.Byte(static_cast<uint8_t>(command.villagers.size()));
		               for (const auto villager : command.villagers)
		               {
			               out.Entity(villager);
		               }
	               },
	               [&out](const DebugVillagerPath& command) {
		               out.Byte(static_cast<uint8_t>(command.act));
		               out.Entity(command.villager);
		               out.Vector(command.position);
		               out.Number(static_cast<uint32_t>(command.goal.x));
		               out.Number(static_cast<uint32_t>(command.goal.y));
		               out.Float(command.speed);
	               },
	               [&out](const MusicMood& command) {
		               out.Entity(command.creature);
		               out.Number(command.track);
		               out.Float(command.mood);
	               },
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
		const auto amount = in.Float();
		// The game sends the sum of the strokes and the slaps in the range from -1 to 1
		if (amount < -1.0f || amount > 1.0f)
		{
			return DecodeError::Value;
		}
		action = CreatureFeedback {.creature = creature, .amount = amount};
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
		const auto share = in.Float();
		// The share of the people of the town is from 0 to 1
		if (share < 0.0f || share > 1.0f)
		{
			return DecodeError::Value;
		}
		action = TotemShare {.totem = totem, .share = share};
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
		const auto icon = in.Entity();
		if (kind > static_cast<uint8_t>(SeedGesture::Kind::Shake))
		{
			return DecodeError::Value;
		}
		action = SeedGesture {.kind = static_cast<SeedGesture::Kind>(kind), .level = level, .icon = icon};
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
		reaction.interruptsAfter = in.Double();
		const auto slap = in.Byte();
		if (mirrored > 1 || slap > 1)
		{
			return DecodeError::Value;
		}
		reaction.mirrored = mirrored != 0;
		reaction.slap = slap != 0;
		action = reaction;
		return DecodeError::None;
	}
	case CommandTag::PlayerLeft:
		action = PlayerLeft {};
		return DecodeError::None;
	case CommandTag::PlayerJoined:
		action = PlayerJoined {};
		return DecodeError::None;
	case CommandTag::ObserversJoined:
		action = ObserversJoined {};
		return DecodeError::None;
	case CommandTag::ObserversLeft:
		action = ObserversLeft {};
		return DecodeError::None;
	case CommandTag::ObserverIn:
	{
		const auto length = in.Byte();
		if (length > k_MostObserverNameCharacters)
		{
			return DecodeError::Value;
		}
		if (in.Left() < size_t {length} * 2)
		{
			return DecodeError::Truncated;
		}
		ObserverIn observer;
		observer.name.resize(length);
		for (auto& character : observer.name)
		{
			character = static_cast<char16_t>(in.Number<uint16_t>());
		}
		action = std::move(observer);
		return DecodeError::None;
	}
	case CommandTag::ObserverOut:
		action = ObserverOut {};
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
		if (flags > 63)
		{
			return DecodeError::Value;
		}
		report.cameraArrived = (flags & 1u) != 0;
		report.gripsLand = (flags & 2u) != 0;
		report.otherButton = (flags & 4u) != 0;
		report.mouseWheel = (flags & 8u) != 0;
		report.cameraTurned = (flags & 16u) != 0;
		report.insideTemple = (flags & 32u) != 0;
		report.cameraIcons = in.Number<uint16_t>();
		const auto keys = in.Byte();
		// The sender gives the keys in rising order, once each. Any other order gives other bytes when the frame goes out
		// again.
		int previousKey = -1;
		for (uint8_t i = 0; i < keys; ++i)
		{
			const auto key = in.Byte();
			if (key >= report.keysHeld.size() || static_cast<int>(key) <= previousKey)
			{
				return DecodeError::Value;
			}
			previousKey = key;
			report.keysHeld.set(key);
		}
		report.mouseButtons = in.Number<uint32_t>();
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
	case CommandTag::ChatSpeech:
	{
		ChatSpeech speech;
		if (const auto error = ReadRecipients(in, speech.recipients); error != DecodeError::None)
		{
			return error;
		}
		const auto length = in.Number<uint16_t>();
		if (length > k_MostChatCharacters)
		{
			return DecodeError::Value;
		}
		if (in.Left() < size_t {length} * 2)
		{
			return DecodeError::Truncated;
		}
		speech.text.resize(length);
		for (auto& character : speech.text)
		{
			character = static_cast<char16_t>(in.Number<uint16_t>());
		}
		action = std::move(speech);
		return DecodeError::None;
	}
	case CommandTag::ChatTaunt:
	{
		ChatTaunt taunt;
		if (const auto error = ReadRecipients(in, taunt.recipients); error != DecodeError::None)
		{
			return error;
		}
		taunt.taunt = in.Number<uint32_t>();
		action = std::move(taunt);
		return DecodeError::None;
	}
	case CommandTag::QuickSave:
		action = QuickSave {};
		return DecodeError::None;
	case CommandTag::QuickLoad:
		action = QuickLoad {};
		return DecodeError::None;
	case CommandTag::IconChargeStop:
		action = IconChargeStop {};
		return DecodeError::None;
	case CommandTag::DialogueClick:
	{
		const auto flags = in.Byte();
		if (flags > 3)
		{
			return DecodeError::Value;
		}
		action = DialogueClick {.click = (flags & 1u) != 0, .skipKey = (flags & 2u) != 0};
		return DecodeError::None;
	}
	case CommandTag::PuzzleTap:
	{
		const auto placeX = static_cast<int32_t>(in.Number<uint32_t>());
		action = PuzzleTap {.placeX = placeX, .placeZ = static_cast<int32_t>(in.Number<uint32_t>())};
		return DecodeError::None;
	}
	case CommandTag::FishScare:
		action = FishScare {.point = in.Vector()};
		return DecodeError::None;
	case CommandTag::HandLockRelease:
		action = HandLockRelease {.creature = in.Entity()};
		return DecodeError::None;
	case CommandTag::EditorEdit:
	{
		const auto kind = in.Byte();
		if (kind > static_cast<uint8_t>(EditorEdit::Kind::Pose))
		{
			return DecodeError::Value;
		}
		EditorEdit edit {.kind = static_cast<EditorEdit::Kind>(kind)};
		edit.object = in.Entity();
		edit.placeKind = in.Byte();
		edit.placeType = static_cast<int32_t>(in.Number<uint32_t>());
		edit.challenge = in.Number<uint32_t>();
		edit.position = in.Vector();
		edit.radians = in.Float();
		edit.scale = in.Float();
		edit.request = in.Number<uint16_t>();
		if (edit.kind == EditorEdit::Kind::Pose)
		{
			edit.rotation = in.Matrix();
			const auto keeps = in.Byte();
			edit.keptAngles = in.Vector();
			edit.poseScale = in.Vector();
			if (in.Bad())
			{
				return DecodeError::Truncated;
			}
			if (keeps > 1)
			{
				return DecodeError::Value;
			}
			edit.keepsAngles = keeps == 1;
		}
		action = edit;
		return DecodeError::None;
	}
	case CommandTag::EditorRemake:
	{
		EditorRemake remake {.request = in.Number<uint16_t>(), .placeKind = in.Byte()};
		remake.placeType = static_cast<int32_t>(in.Number<uint32_t>());
		remake.challenge = in.Number<uint32_t>();
		remake.position = in.Vector();
		remake.rotation = in.Matrix();
		const auto keeps = in.Byte();
		remake.keptAngles = in.Vector();
		remake.scale = in.Vector();
		const auto creature = in.Byte();
		remake.owner = in.Byte();
		remake.mind = in.Number<uint32_t>();
		remake.size = in.Float();
		remake.alignment = in.Float();
		remake.fatness = in.Float();
		remake.strength = in.Float();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (keeps > 1 || creature > 1 || remake.owner >= static_cast<uint8_t>(PlayerNames::_COUNT))
		{
			return DecodeError::Value;
		}
		remake.keepsAngles = keeps == 1;
		remake.creature = creature == 1;
		action = remake;
		return DecodeError::None;
	}
	case CommandTag::DebugCheat:
	{
		const auto cheat = in.Byte();
		const auto on = in.Byte();
		if (!debug_cheats::CheatFrom(cheat).has_value() || on > 1)
		{
			return DecodeError::Value;
		}
		action = DebugCheat {.cheat = cheat, .on = on == 1};
		return DecodeError::None;
	}
	case CommandTag::EditorCreatureEdit:
	{
		const auto creature = in.Entity();
		const auto value = in.Byte();
		if (!creature_edit_values::ValueFrom(value).has_value())
		{
			return DecodeError::Value;
		}
		action = EditorCreatureEdit {.creature = creature, .value = value, .amount = in.Float()};
		return DecodeError::None;
	}
	case CommandTag::EditorCreatureAct:
	{
		EditorCreatureAct act {.creature = in.Entity(), .act = in.Byte()};
		if (!creature_edit_acts::ActFrom(act.act).has_value())
		{
			return DecodeError::Value;
		}
		act.other = in.Entity();
		act.position = in.Vector();
		act.amount = in.Float();
		act.index = in.Number<uint16_t>();
		for (auto& arg : act.args)
		{
			arg = in.Byte();
		}
		action = act;
		return DecodeError::None;
	}
	case CommandTag::DebugLandSkip:
	{
		const auto land = in.Byte();
		if (!story_land_skip::GoesAsCommand(land))
		{
			return in.Bad() ? DecodeError::Truncated : DecodeError::Value;
		}
		action = DebugLandSkip {.land = land};
		return DecodeError::None;
	}
	case CommandTag::EditorVillagerEdit:
	{
		EditorVillagerEdit edit {.villager = in.Entity()};
		const auto field = in.Byte();
		if (field > static_cast<uint8_t>(EditorVillagerEdit::Field::State))
		{
			return in.Bad() ? DecodeError::Truncated : DecodeError::Value;
		}
		edit.field = static_cast<EditorVillagerEdit::Field>(field);
		edit.index = in.Byte();
		edit.number = in.Number<uint32_t>();
		edit.amount = in.Float();
		action = edit;
		return DecodeError::None;
	}
	case CommandTag::EditorCreatureSpawn:
	{
		EditorCreatureSpawn spawn;
		if (const auto error = ReadCreatureSpawn(in, spawn); error != DecodeError::None)
		{
			return error;
		}
		spawn.request = in.Number<uint16_t>();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		action = spawn;
		return DecodeError::None;
	}
	case CommandTag::EditorMindLoad:
	{
		const auto use = in.Byte();
		EditorMindLoad load {.file = in.Number<uint64_t>()};
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		// The use is a load into a creature or a new creature
		if (use > static_cast<uint8_t>(EditorMindLoad::Use::Spawn))
		{
			return DecodeError::Value;
		}
		load.use = static_cast<EditorMindLoad::Use>(use);
		if (load.use == EditorMindLoad::Use::LoadInto)
		{
			load.creature = in.Entity();
			if (in.Bad())
			{
				return DecodeError::Truncated;
			}
		}
		else if (const auto error = ReadCreatureSpawn(in, load.spawn); error != DecodeError::None)
		{
			return error;
		}
		action = load;
		return DecodeError::None;
	}
	case CommandTag::EditorScript:
	{
		const auto act = in.Byte();
		EditorScript script {.id = in.Number<uint32_t>()};
		script.address = in.Number<uint32_t>();
		script.index = in.Number<uint16_t>();
		script.value = in.Number<uint32_t>();
		const auto count = in.Byte();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (act > static_cast<uint8_t>(EditorScript::Act::SetTaskVariable) || count > k_MostEditorScriptParameters)
		{
			return DecodeError::Value;
		}
		if (static_cast<size_t>(count) * 4 > in.Left())
		{
			return DecodeError::Truncated;
		}
		script.act = static_cast<EditorScript::Act>(act);
		script.parameters.resize(count);
		for (auto& parameter : script.parameters)
		{
			parameter = in.Float();
		}
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		action = std::move(script);
		return DecodeError::None;
	}
	case CommandTag::EditorLandScript:
	{
		const auto act = in.Byte();
		EditorLandScript script {.command = in.Number<uint32_t>()};
		script.file = in.Number<uint64_t>();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (act > static_cast<uint8_t>(EditorLandScript::Act::Breakpoint))
		{
			return DecodeError::Value;
		}
		script.act = static_cast<EditorLandScript::Act>(act);
		action = script;
		return DecodeError::None;
	}
	case CommandTag::EditorScriptProgram:
	{
		const auto use = in.Byte();
		EditorScriptProgram program {.file = in.Number<uint64_t>()};
		program.script = in.Number<uint32_t>();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (use > static_cast<uint8_t>(EditorScriptProgram::Use::GamesProgram))
		{
			return DecodeError::Value;
		}
		program.use = static_cast<EditorScriptProgram::Use>(use);
		action = program;
		return DecodeError::None;
	}
	case CommandTag::EditorLandStroke:
	{
		EditorLandStroke stroke {.mode = in.Byte()};
		stroke.innerRadius = in.Float();
		stroke.outerRadius = in.Float();
		stroke.step = in.Byte();
		stroke.country = in.Byte();
		stroke.centre.x = in.Float();
		stroke.centre.y = in.Float();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		// The mode is raise, lower or paint
		if (stroke.mode > 2)
		{
			return DecodeError::Value;
		}
		action = stroke;
		return DecodeError::None;
	}
	case CommandTag::EditorPath:
	{
		EditorPath path {.kind = in.Byte()};
		const auto count = in.Number<uint16_t>();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		// The kind is a footpath or a stream
		if (path.kind > 1 || count > k_MostEditorPathPoints)
		{
			return DecodeError::Value;
		}
		if (static_cast<size_t>(count) * 8 > in.Left())
		{
			return DecodeError::Truncated;
		}
		path.points.resize(count);
		for (auto& point : path.points)
		{
			point.x = in.Float();
			point.y = in.Float();
		}
		path.firstHolder = in.Entity();
		path.lastHolder = in.Entity();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		action = std::move(path);
		return DecodeError::None;
	}
	case CommandTag::DebugAdvisor:
	{
		const auto act = in.Byte();
		DebugAdvisor advisor {.advisor = in.Byte()};
		advisor.place.x = in.Float();
		advisor.place.y = in.Float();
		advisor.position = in.Vector();
		const auto inWorld = in.Byte();
		advisor.anim = in.Byte();
		advisor.speed = in.Float();
		advisor.emotion = in.Byte();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		// The act is one of the acts of the window, the advisor is both, the good one or the evil one
		if (act > static_cast<uint8_t>(DebugAdvisor::Act::Feel) || advisor.advisor > 2 || inWorld > 1)
		{
			return DecodeError::Value;
		}
		advisor.act = static_cast<DebugAdvisor::Act>(act);
		advisor.inWorld = inWorld == 1;
		action = advisor;
		return DecodeError::None;
	}
	case CommandTag::DebugWorld:
	{
		const auto act = in.Byte();
		DebugWorld world {.player = in.Byte()};
		world.index = in.Byte();
		world.value = in.Float();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (act > static_cast<uint8_t>(DebugWorld::Act::BonusCreature))
		{
			return DecodeError::Value;
		}
		world.act = static_cast<DebugWorld::Act>(act);
		action = world;
		return DecodeError::None;
	}
	case CommandTag::DebugConsoleLine:
	{
		const auto count = in.Number<uint16_t>();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (count > k_MostConsoleLineBytes)
		{
			return DecodeError::Value;
		}
		if (count > in.Left())
		{
			return DecodeError::Truncated;
		}
		DebugConsoleLine line;
		line.text.resize(count);
		for (auto& character : line.text)
		{
			character = static_cast<char>(in.Byte());
		}
		action = std::move(line);
		return DecodeError::None;
	}
	case CommandTag::DebugMagic:
	{
		const auto act = in.Byte();
		DebugMagic magic {.magic = in.Byte()};
		magic.position = in.Vector();
		magic.forward = in.Vector();
		magic.creature = in.Entity();
		magic.value = in.Float();
		const auto on = in.Byte();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (act > static_cast<uint8_t>(DebugMagic::Act::InfinitePrayer) || on > 1)
		{
			return DecodeError::Value;
		}
		magic.act = static_cast<DebugMagic::Act>(act);
		magic.on = on == 1;
		action = magic;
		return DecodeError::None;
	}
	case CommandTag::DebugTownMiracle:
	{
		DebugTownMiracle town {.town = in.Number<uint32_t>()};
		town.magic = in.Byte();
		const auto flags = in.Byte();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (flags >= (1U << k_TownMiracleFlagCount))
		{
			return DecodeError::Value;
		}
		SetTownMiracleFlags(town, flags);
		action = town;
		return DecodeError::None;
	}
	case CommandTag::DebugWeather:
	{
		const auto act = in.Byte();
		DebugWeather weather;
		for (auto* value :
		     {&weather.temperature, &weather.rain, &weather.snow, &weather.overcast, &weather.windX, &weather.windZ})
		{
			*value = static_cast<int8_t>(in.Byte());
		}
		weather.seconds = in.Float();
		weather.fadeSeconds = in.Float();
		weather.thunderWait.x = in.Float();
		weather.thunderWait.y = in.Float();
		weather.boltWait.x = in.Float();
		weather.boltWait.y = in.Float();
		weather.cloudHeight = in.Float();
		weather.rainSpeed = in.Float();
		weather.storm = in.Entity();
		const auto on = in.Byte();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (act > static_cast<uint8_t>(DebugWeather::Act::EndStorm) || on > 1)
		{
			return DecodeError::Value;
		}
		weather.act = static_cast<DebugWeather::Act>(act);
		weather.on = on == 1;
		action = weather;
		return DecodeError::None;
	}
	case CommandTag::DebugSky:
	{
		const auto act = in.Byte();
		DebugSky sky {.hour = in.Float()};
		const auto on = in.Byte();
		sky.date = static_cast<int64_t>(in.Number<uint64_t>());
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (act > static_cast<uint8_t>(DebugSky::Act::Reset) || on > 1)
		{
			return DecodeError::Value;
		}
		sky.act = static_cast<DebugSky::Act>(act);
		sky.on = on == 1;
		action = sky;
		return DecodeError::None;
	}
	case CommandTag::DebugPhysics:
	{
		DebugPhysics physics {.object = in.Entity()};
		physics.type = in.Number<uint16_t>();
		physics.height = in.Float();
		physics.scale = in.Float();
		physics.place.x = in.Float();
		physics.place.y = in.Float();
		physics.velocity = in.Vector();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		action = physics;
		return DecodeError::None;
	}
	case CommandTag::DebugParticle:
	{
		const auto act = in.Byte();
		const auto count = in.Byte();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (act > static_cast<uint8_t>(DebugParticle::Act::Delete) || count > k_MostParticleFileBytes)
		{
			return DecodeError::Value;
		}
		if (count > in.Left())
		{
			return DecodeError::Truncated;
		}
		DebugParticle particle {.act = static_cast<DebugParticle::Act>(act)};
		particle.file.resize(count);
		for (auto& character : particle.file)
		{
			character = static_cast<char>(in.Byte());
		}
		particle.type = in.Byte();
		particle.position = in.Vector();
		particle.magnitude = in.Float();
		particle.player = in.Byte();
		particle.drawPath = in.Byte();
		particle.targets = in.Byte();
		particle.effect = in.Number<uint32_t>();
		const auto paused = in.Byte();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (paused > 1)
		{
			return DecodeError::Value;
		}
		particle.paused = paused == 1;
		action = std::move(particle);
		return DecodeError::None;
	}
	case CommandTag::DebugTempleVillager:
	{
		const auto act = in.Byte();
		DebugTempleVillager temple {.town = in.Entity()};
		temple.villager = in.Entity();
		temple.home = in.Vector();
		temple.value = in.Float();
		temple.tribe = in.Byte();
		temple.number = in.Byte();
		temple.age = in.Byte();
		temple.count = in.Byte();
		const auto send = in.Byte();
		const auto count = in.Byte();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (act > static_cast<uint8_t>(DebugTempleVillager::Act::TakeAway) || send > 1 || count > k_MostTakenVillagers)
		{
			return DecodeError::Value;
		}
		if (static_cast<size_t>(count) * sizes::k_Entity > in.Left())
		{
			return DecodeError::Truncated;
		}
		temple.act = static_cast<DebugTempleVillager::Act>(act);
		temple.send = send == 1;
		temple.villagers.resize(count);
		for (auto& villager : temple.villagers)
		{
			villager = in.Entity();
		}
		action = std::move(temple);
		return DecodeError::None;
	}
	case CommandTag::DebugVillagerPath:
	{
		const auto act = in.Byte();
		DebugVillagerPath path {.villager = in.Entity()};
		path.position = in.Vector();
		path.goal.x = static_cast<int32_t>(in.Number<uint32_t>());
		path.goal.y = static_cast<int32_t>(in.Number<uint32_t>());
		path.speed = in.Float();
		if (in.Bad())
		{
			return DecodeError::Truncated;
		}
		if (act > static_cast<uint8_t>(DebugVillagerPath::Act::Speed))
		{
			return DecodeError::Value;
		}
		path.act = static_cast<DebugVillagerPath::Act>(act);
		action = path;
		return DecodeError::None;
	}
	case CommandTag::MusicMood:
	{
		const auto creature = in.Entity();
		const auto track = in.Number<uint32_t>();
		action = MusicMood {.creature = creature, .track = track, .mood = in.Float()};
		return DecodeError::None;
	}
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
		// No command of the game has a number that is not finite
		if (!in.Finite())
		{
			return DecodeError::Value;
		}
	}
	return DecodeError::None;
}

/// The players of a line of the chat are slots of the game, and a line of the chat or the name of an observer is not too
/// long
[[nodiscard]] bool ValidChat(const Action& action)
{
	const auto validRecipients = [](const std::vector<PlayerNames>& recipients) {
		return recipients.size() <= player_slots::k_MaxPlayers && std::ranges::all_of(recipients, [](PlayerNames recipient) {
			       return player_slots::SlotOfPlayer(recipient).has_value();
		       });
	};
	if (const auto* speech = std::get_if<ChatSpeech>(&action); speech != nullptr)
	{
		return validRecipients(speech->recipients) && speech->text.size() <= k_MostChatCharacters;
	}
	if (const auto* taunt = std::get_if<ChatTaunt>(&action); taunt != nullptr)
	{
		return validRecipients(taunt->recipients);
	}
	if (const auto* observer = std::get_if<ObserverIn>(&action); observer != nullptr)
	{
		return observer->name.size() <= k_MostObserverNameCharacters;
	}
	return true;
}

[[nodiscard]] bool ValidActions(const std::vector<Action>& actions)
{
	return actions.size() <= k_MostBlockCommands && std::ranges::all_of(actions, ValidChat);
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
			                      return player_slots::SlotOfPlayer(block.sender).has_value() &&
			                             block.observer <= k_MostObserverNumber && ValidActions(block.actions);
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
	                      [](const ChatSpeech& /*command*/) { return CommandTag::ChatSpeech; },
	                      [](const ChatTaunt& /*command*/) { return CommandTag::ChatTaunt; },
	                      [](const QuickSave& /*command*/) { return CommandTag::QuickSave; },
	                      [](const QuickLoad& /*command*/) { return CommandTag::QuickLoad; },
	                      [](const MusicMood& /*command*/) { return CommandTag::MusicMood; },
	                      [](const IconChargeStop& /*command*/) { return CommandTag::IconChargeStop; },
	                      [](const DialogueClick& /*command*/) { return CommandTag::DialogueClick; },
	                      [](const PuzzleTap& /*command*/) { return CommandTag::PuzzleTap; },
	                      [](const FishScare& /*command*/) { return CommandTag::FishScare; },
	                      [](const ObserversJoined& /*command*/) { return CommandTag::ObserversJoined; },
	                      [](const ObserversLeft& /*command*/) { return CommandTag::ObserversLeft; },
	                      [](const ObserverIn& /*command*/) { return CommandTag::ObserverIn; },
	                      [](const ObserverOut& /*command*/) { return CommandTag::ObserverOut; },
	                      [](const HandLockRelease& /*command*/) { return CommandTag::HandLockRelease; },
	                      [](const EditorEdit& /*command*/) { return CommandTag::EditorEdit; },
	                      [](const DebugCheat& /*command*/) { return CommandTag::DebugCheat; },
	                      [](const EditorCreatureEdit& /*command*/) { return CommandTag::EditorCreatureEdit; },
	                      [](const EditorCreatureAct& /*command*/) { return CommandTag::EditorCreatureAct; },
	                      [](const DebugLandSkip& /*command*/) { return CommandTag::DebugLandSkip; },
	                      [](const EditorVillagerEdit& /*command*/) { return CommandTag::EditorVillagerEdit; },
	                      [](const EditorCreatureSpawn& /*command*/) { return CommandTag::EditorCreatureSpawn; },
	                      [](const EditorLandStroke& /*command*/) { return CommandTag::EditorLandStroke; },
	                      [](const EditorPath& /*command*/) { return CommandTag::EditorPath; },
	                      [](const EditorRemake& /*command*/) { return CommandTag::EditorRemake; },
	                      [](const EditorMindLoad& /*command*/) { return CommandTag::EditorMindLoad; },
	                      [](const EditorScript& /*command*/) { return CommandTag::EditorScript; },
	                      [](const EditorLandScript& /*command*/) { return CommandTag::EditorLandScript; },
	                      [](const EditorScriptProgram& /*command*/) { return CommandTag::EditorScriptProgram; },
	                      [](const DebugAdvisor& /*command*/) { return CommandTag::DebugAdvisor; },
	                      [](const DebugWorld& /*command*/) { return CommandTag::DebugWorld; },
	                      [](const DebugConsoleLine& /*command*/) { return CommandTag::DebugConsoleLine; },
	                      [](const DebugMagic& /*command*/) { return CommandTag::DebugMagic; },
	                      [](const DebugTownMiracle& /*command*/) { return CommandTag::DebugTownMiracle; },
	                      [](const DebugWeather& /*command*/) { return CommandTag::DebugWeather; },
	                      [](const DebugSky& /*command*/) { return CommandTag::DebugSky; },
	                      [](const DebugPhysics& /*command*/) { return CommandTag::DebugPhysics; },
	                      [](const DebugParticle& /*command*/) { return CommandTag::DebugParticle; },
	                      [](const DebugTempleVillager& /*command*/) { return CommandTag::DebugTempleVillager; },
	                      [](const DebugVillagerPath& /*command*/) { return CommandTag::DebugVillagerPath; },
	                  },
	                  action);
}

size_t SizeOf(const Action& action) noexcept
{
	using namespace sizes;
	const size_t fields = std::visit(
	    Overloaded {
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
	        [](const CreatureReaction& /*command*/) { return k_Entity + 4 + 1 + 8 + 1; },
	        [](const HandClick& /*command*/) { return k_Entity + 2 + 4 + 4; },
	        [](const SeedGesture& /*command*/) { return 2 + k_Entity; },
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
	        [](const ViewReport& command) {
		        // The camera icons, the count of keys held, each key held and the count of mouse buttons follow the
		        // flags
		        return 2 * k_Vector + 7 * 4 + 1 + 2 + 1 + command.keysHeld.count();
	        },
	        [](const GiveCallOff& /*command*/) { return k_Entity; },
	        [](const CreatureTattoo& command) {
		        return k_Entity + 5 * creature_tattoo::k_SlotCount + 1 + 2 + command.pictures.size();
	        },
	        [](const LeashLength& /*command*/) { return size_t {4}; },
	        [](const ChatSpeech& command) { return 1 + command.recipients.size() + 2 + (2 * command.text.size()); },
	        [](const ChatTaunt& command) { return 1 + command.recipients.size() + 4; },
	        [](const QuickSave& /*command*/) { return size_t {0}; },
	        [](const QuickLoad& /*command*/) { return size_t {0}; },
	        [](const MusicMood& /*command*/) { return k_Entity + 4 + 4; },
	        [](const IconChargeStop& /*command*/) { return size_t {0}; },
	        [](const DialogueClick& /*command*/) { return size_t {1}; },
	        [](const PuzzleTap& /*command*/) { return size_t {4 + 4}; },
	        [](const FishScare& /*command*/) { return k_Vector; },
	        [](const ObserversJoined& /*command*/) { return size_t {0}; },
	        [](const ObserversLeft& /*command*/) { return size_t {0}; },
	        [](const ObserverIn& command) { return 1 + (2 * command.name.size()); },
	        [](const ObserverOut& /*command*/) { return size_t {0}; },
	        [](const HandLockRelease& /*command*/) { return k_Entity; },
	        [](const EditorEdit& command) {
		        // A pose adds the turn, the switch of the kept angles, the kept angles and the scale of each axis
		        const size_t pose = command.kind == EditorEdit::Kind::Pose ? (3 * k_Vector) + 1 + k_Vector + k_Vector : 0;
		        return 1 + k_Entity + 1 + 4 + 4 + k_Vector + 4 + 4 + 2 + pose;
	        },
	        [](const EditorRemake& /*command*/) {
		        return 2 + 1 + 4 + 4 + k_Vector + (3 * k_Vector) + 1 + k_Vector + k_Vector + 1 + 1 + 4 + (4 * 4);
	        },
	        [](const DebugCheat& /*command*/) { return size_t {2}; },
	        [](const EditorCreatureEdit& /*command*/) { return k_Entity + 1 + 4; },
	        [](const EditorCreatureAct& command) { return k_Entity + 1 + k_Entity + k_Vector + 4 + 2 + command.args.size(); },
	        [](const DebugLandSkip& /*command*/) { return size_t {1}; },
	        [](const EditorVillagerEdit& /*command*/) { return k_Entity + 1 + 1 + 4 + 4; },
	        [](const EditorCreatureSpawn& /*command*/) { return k_CreatureSpawn + 2; },
	        [](const EditorMindLoad& command) {
		        return 1 + 8 + (command.use == EditorMindLoad::Use::LoadInto ? k_Entity : k_CreatureSpawn);
	        },
	        [](const EditorLandStroke& /*command*/) { return size_t {1 + 4 + 4 + 1 + 1 + 4 + 4}; },
	        [](const EditorPath& command) { return 1 + 2 + (8 * command.points.size()) + (2 * k_Entity); },
	        [](const EditorScript& command) { return 1 + 4 + 4 + 2 + 4 + 1 + (4 * command.parameters.size()); },
	        [](const EditorLandScript& /*command*/) { return size_t {1 + 4 + 8}; },
	        [](const EditorScriptProgram& /*command*/) { return size_t {1 + 8 + 4}; },
	        [](const DebugAdvisor& /*command*/) { return size_t {1 + 1 + 8 + k_Vector + 1 + 1 + 4 + 1}; },
	        [](const DebugWorld& /*command*/) { return size_t {1 + 1 + 1 + 4}; },
	        [](const DebugConsoleLine& command) { return 2 + command.text.size(); },
	        [](const DebugMagic& /*command*/) { return size_t {1 + 1 + k_Vector + k_Vector + k_Entity + 4 + 1}; },
	        [](const DebugTownMiracle& /*command*/) { return size_t {4 + 1 + 1}; },
	        [](const DebugWeather& /*command*/) { return size_t {1 + 6 + (8 * 4) + k_Entity + 1}; },
	        [](const DebugSky& /*command*/) { return size_t {1 + 4 + 1 + 8}; },
	        [](const DebugPhysics& /*command*/) { return k_Entity + 2 + 4 + 4 + 8 + k_Vector; },
	        [](const DebugParticle& command) { return 1 + 1 + command.file.size() + 1 + k_Vector + 4 + 1 + 1 + 1 + 4 + 1; },
	        [](const DebugTempleVillager& command) {
		        return 1 + k_Entity + k_Entity + k_Vector + 4 + 4 + 1 + 1 + (k_Entity * command.villagers.size());
	        },
	        [](const DebugVillagerPath& /*command*/) { return 1 + k_Entity + k_Vector + 8 + 4; },
	    },
	    action);
	return 1 + fields;
}

size_t BlockSize(const StampedBlock& block) noexcept
{
	return sizes::k_Stamp - 1 + SizeOfActions(block.actions);
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
			               writer.Byte(SlotByte(block.sender));
			               writer.Byte(block.observer);
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
			const auto sender = player_slots::PlayerOfSlot(in.Byte());
			if (!sender.has_value())
			{
				return Fail(in.Bad() ? DecodeError::Truncated : DecodeError::Value);
			}
			block.sender = *sender;
			block.observer = in.Byte();
			if (block.observer > k_MostObserverNumber)
			{
				return Fail(in.Bad() ? DecodeError::Truncated : DecodeError::Value);
			}
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
