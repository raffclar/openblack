/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSaveState.h"

#include <cmath>

#include <MindFile.h>

#include "3D/MapCoords.h"

using namespace openblack;
using namespace openblack::creature_save_state;

gamesave::ObjectRef creature_save_state::ToRef(entt::entity entity)
{
	if (entity == entt::null)
	{
		return {};
	}
	return gamesave::RefTo(gamesave::EntityKey(static_cast<uint32_t>(entt::to_integral(entity))));
}

gamesave::ObjectRef creature_save_state::ToRef(std::optional<entt::entity> entity)
{
	return entity.has_value() ? ToRef(*entity) : gamesave::ObjectRef {};
}

std::optional<entt::entity> creature_save_state::FromRef(const gamesave::ObjectRef& ref)
{
	const auto entity = gamesave::EntityOf(ref.key);
	if (!entity.has_value())
	{
		return std::nullopt;
	}
	return static_cast<entt::entity>(*entity);
}

bool creature_save_state::KeepsPlan(bool scriptControlled, bool fighting, size_t dancers)
{
	return scriptControlled || fighting || dancers > 0;
}

gamesave::SpellSlot creature_save_state::ToSlot(const creature_spells::Slot& slot, uint32_t place)
{
	return {.spell = place,
	        .phase = static_cast<uint32_t>(slot.phase),
	        .turnsLeft = slot.turnsLeft,
	        .found = slot.before,
	        .holdTurns = slot.holdTurns,
	        .miracle = ToRef(slot.miracle)};
}

creature_spells::Slot creature_save_state::FromSlot(const gamesave::SpellSlot& slot)
{
	constexpr auto k_LastPhase = static_cast<uint32_t>(creature_spells::Phase::Finishing);
	return {.phase = slot.phase <= k_LastPhase ? static_cast<creature_spells::Phase>(slot.phase) : creature_spells::Phase::Off,
	        .turnsLeft = slot.turnsLeft,
	        .holdTurns = slot.holdTurns,
	        .miracle = FromRef(slot.miracle).value_or(entt::null),
	        .before = slot.found};
}

gamesave::QueuedSpell creature_save_state::ToQueued(const creature_spells::Waiting& waiting)
{
	return {.spell = static_cast<uint32_t>(waiting.spell), .seconds = waiting.seconds, .miracle = ToRef(waiting.miracle)};
}

std::optional<creature_spells::Waiting> creature_save_state::FromQueued(const gamesave::QueuedSpell& queued,
                                                                        float turnsPerSecond)
{
	if (queued.spell >= creature_spells::k_SpellCount)
	{
		return std::nullopt;
	}
	return creature_spells::Waiting {.spell = static_cast<creature_spells::Spell>(queued.spell),
	                                 .holdTurns = creature_spells::TurnsOf(queued.seconds, turnsPerSecond),
	                                 .miracle = FromRef(queued.miracle).value_or(entt::null),
	                                 .seconds = queued.seconds};
}

creature_fizz::Fizz creature_save_state::FizzToSave(const creature_fizz::Fizz& fizz, const creature_spells::Spells& spells)
{
	if (spells.IsActive(creature_spells::Spell::Invisible))
	{
		return fizz;
	}
	// The creature keeps only whether it goes for good
	return {.now = 0.0f, .target = 0.0f, .perSecond = 0.0f, .goesForGood = fizz.goesForGood};
}

gamesave::KnownLeashes creature_save_state::ToKnown(const std::bitset<creature_leash::k_Types.size()>& known)
{
	const auto knows = [&known](LeashType type) {
		const auto index = creature_leash::IndexOf(type);
		return index.has_value() && known.test(*index);
	};
	return {.evil = knows(LeashType::Evil), .rope = knows(LeashType::Rope), .good = knows(LeashType::Good)};
}

std::bitset<creature_leash::k_Types.size()> creature_save_state::FromKnown(const gamesave::KnownLeashes& known)
{
	std::bitset<creature_leash::k_Types.size()> bits;
	const auto set = [&bits](LeashType type, bool value) {
		if (const auto index = creature_leash::IndexOf(type); index.has_value())
		{
			bits.set(*index, value);
		}
	};
	set(LeashType::Evil, known.evil);
	set(LeashType::Rope, known.rope);
	set(LeashType::Good, known.good);
	return bits;
}

uint8_t creature_save_state::PlayerNumber(std::optional<PlayerNames> player)
{
	if (!player.has_value() || *player == PlayerNames::NEUTRAL)
	{
		return gamesave::k_None;
	}
	return static_cast<uint8_t>(*player);
}

std::optional<PlayerNames> creature_save_state::PlayerFromNumber(uint8_t number)
{
	if (number >= static_cast<uint8_t>(PlayerNames::NEUTRAL))
	{
		return PlayerNames::NEUTRAL;
	}
	return static_cast<PlayerNames>(number);
}

uint32_t creature_save_state::LeashTypeNumber(LeashType type)
{
	return type == LeashType::None ? 0u : static_cast<uint32_t>(type);
}

std::optional<LeashType> creature_save_state::LeashTypeFromNumber(uint32_t number)
{
	switch (number)
	{
	case static_cast<uint32_t>(LeashType::Evil):
		return LeashType::Evil;
	case static_cast<uint32_t>(LeashType::Rope):
		return LeashType::Rope;
	case static_cast<uint32_t>(LeashType::Good):
		return LeashType::Good;
	default:
		return std::nullopt;
	}
}

bool creature_save_state::ResolvesOnLoad(std::optional<size_t> mindFileSize)
{
	return mindFileSize.value_or(0) > 0;
}

gamesave::Agenda creature_save_state::AgendaAfterLoad(gamesave::Agenda agenda)
{
	agenda.word08 = 1;
	agenda.state = 0;
	agenda.word20 = 1;
	return agenda;
}

namespace
{
template <typename T>
void Pick(T& out, const T& baseline, const T& now)
{
	if (!(now == baseline))
	{
		out = now;
	}
}
} // namespace

gamesave::CreatureObject creature_save_state::MergeOnto(const gamesave::CreatureObject& loaded,
                                                        const gamesave::CreatureObject& baseline,
                                                        const gamesave::CreatureObject& now)
{
	auto merged = loaded;
	Pick(merged.thing, baseline.thing, now.thing);
	Pick(merged.position.position, baseline.position.position, now.position.position);
	Pick(merged.position.flags, baseline.position.flags, now.position.flags);
	auto& object = merged.object;
	Pick(object.info, baseline.object.info, now.object.info);
	Pick(object.coords, baseline.object.coords, now.object.coords);
	Pick(object.creationIndex, baseline.object.creationIndex, now.object.creationIndex);
	Pick(object.life, baseline.object.life, now.object.life);
	Pick(object.scale, baseline.object.scale, now.object.scale);
	Pick(object.heading, baseline.object.heading, now.object.heading);
	Pick(object.fire, baseline.object.fire, now.object.fire);
	Pick(object.physics, baseline.object.physics, now.object.physics);
	Pick(merged.move, baseline.move, now.move);
	Pick(merged.living, baseline.living, now.living);

	auto& record = merged.creature;
	const auto& was = baseline.creature;
	const auto& is = now.creature;
	Pick(record.version, was.version, is.version);
	Pick(record.savedInTemple, was.savedInTemple, is.savedInTemple);
	Pick(record.index, was.index, is.index);
	Pick(record.owner, was.owner, is.owner);
	for (size_t i = 0; i < record.spells.size(); ++i)
	{
		Pick(record.spells.at(i), was.spells.at(i), is.spells.at(i));
	}
	Pick(record.queued, was.queued, is.queued);
	Pick(record.mindProcessing, was.mindProcessing, is.mindProcessing);
	Pick(record.largestSize, was.largestSize, is.largestSize);
	Pick(record.smallestSize, was.smallestSize, is.smallestSize);
	Pick(record.mayGoThroughVortex, was.mayGoThroughVortex, is.mayGoThroughVortex);
	Pick(record.fizzedForGood, was.fizzedForGood, is.fizzedForGood);
	Pick(record.fizz, was.fizz, is.fizz);
	Pick(record.fizzTarget, was.fizzTarget, is.fizzTarget);
	Pick(record.fizzPerSecond, was.fizzPerSecond, is.fizzPerSecond);
	Pick(record.freeze, was.freeze, is.freeze);
	Pick(record.word12C0, was.word12C0, is.word12C0);
	Pick(record.spellVisuals, was.spellVisuals, is.spellVisuals);
	Pick(record.handParticles, was.handParticles, is.handParticles);
	Pick(record.targetPoint, was.targetPoint, is.targetPoint);
	Pick(record.targetParticles, was.targetParticles, is.targetParticles);
	Pick(record.targetObject, was.targetObject, is.targetObject);
	Pick(record.hasTarget, was.hasTarget, is.hasTarget);
	Pick(record.arena, was.arena, is.arena);
	Pick(record.home, was.home, is.home);
	Pick(record.fightHealth, was.fightHealth, is.fightHealth);
	Pick(record.autoFight, was.autoFight, is.autoFight);
	Pick(record.dancePartners, was.dancePartners, is.dancePartners);
	Pick(record.dance, was.dance, is.dance);
	Pick(record.canDie, was.canDie, is.canDie);
	Pick(record.strength, was.strength, is.strength);
	Pick(record.size, was.size, is.size);
	Pick(record.alignment, was.alignment, is.alignment);
	Pick(record.animationPace, was.animationPace, is.animationPace);
	Pick(record.autoScale, was.autoScale, is.autoScale);
	Pick(record.autoScaleShare, was.autoScaleShare, is.autoScaleShare);
	Pick(record.plan, was.plan, is.plan);
	Pick(record.creedPowers, was.creedPowers, is.creedPowers);
	return merged;
}

glm::vec3 creature_save_state::TargetPoint(glm::vec3 at, float thingHeight)
{
	constexpr double k_FixedPerMetre = 65536.0 / 10.0;
	constexpr double k_OverThePlace = 2.0;
	const auto onMap = [](float metres) {
		return map_coords::ToMetres(static_cast<int32_t>(std::llround(static_cast<double>(metres) * k_FixedPerMetre)));
	};
	return {onMap(at.x), static_cast<float>(static_cast<double>(at.y) + k_OverThePlace + static_cast<double>(thingHeight)),
	        onMap(at.z)};
}

std::optional<uint32_t> creature_save_state::CreatureInfo(CreatureType species)
{
	// The game numbers the kinds of creature from the last row of its creature tables down: the first row (the Giant
	// Ape) has the highest number
	constexpr uint32_t k_FirstRowInfo = 2517;
	const auto row = species == CreatureType::GiantApe ? 0u : static_cast<uint32_t>(species);
	if (species == CreatureType::Unknown || row >= creaturemind::k_SpeciesRows)
	{
		return std::nullopt;
	}
	return k_FirstRowInfo - row;
}

gamesave::Agenda creature_save_state::AgendaToSave(const creature_mind::IdleMind& idle, const gamesave::ObjectRef& creature)
{
	// A new agenda of the game has the first word 1 and no main step. The first word is 0 once a step started.
	gamesave::Agenda agenda {.word08 = idle.stepStarted ? 0u : 1u,
	                         .current = static_cast<uint32_t>(idle.step),
	                         .state = idle.stepStarted ? 1u : 0u,
	                         .count = static_cast<uint32_t>(idle.agenda.size()),
	                         .word18 = 0,
	                         .word1C = 0xFFFFFFFFu,
	                         .word20 = 0,
	                         .steps = {},
	                         .creature = creature,
	                         .other = {}};
	for (size_t i = 0; i < idle.agenda.size(); ++i)
	{
		if (idle.agenda[i].main)
		{
			agenda.word1C = static_cast<uint32_t>(i);
		}
	}
	for (size_t i = idle.step; i < idle.agenda.size(); ++i)
	{
		const auto& step = idle.agenda[i];
		if (!step.saved.has_value())
		{
			agenda.steps.clear();
			agenda.count = agenda.current;
			return agenda;
		}
		const auto& saved = *step.saved;
		agenda.steps.push_back(
		    {.action = saved.kind,
		     .object = step.object.has_value() ? ToRef(static_cast<entt::entity>(*step.object)) : gamesave::ObjectRef {},
		     .integer = saved.integer,
		     .point = {saved.point.x, saved.point.y, saved.point.z},
		     .real = saved.real});
	}
	return agenda;
}

gamesave::LeashStatusObject creature_save_state::MergeLeashOnto(const gamesave::LeashStatusObject& loaded,
                                                                const gamesave::LeashStatusObject& baseline,
                                                                const gamesave::LeashStatusObject& now)
{
	auto merged = loaded;
	auto& leash = merged.leash;
	const auto& was = baseline.leash;
	const auto& is = now.leash;
	Pick(merged.thing, baseline.thing, now.thing);
	Pick(leash.on, was.on, is.on);
	Pick(leash.works, was.works, is.works);
	Pick(leash.type, was.type, is.type);
	Pick(leash.player, was.player, is.player);
	Pick(leash.word20, was.word20, is.word20);
	Pick(leash.tiedTo, was.tiedTo, is.tiedTo);
	Pick(leash.length, was.length, is.length);
	Pick(leash.fullLength, was.fullLength, is.fullLength);
	Pick(leash.holder, was.holder, is.holder);
	Pick(leash.word34, was.word34, is.word34);
	return merged;
}

void creature_save_state::Rekey(gamesave::CreatureObject& creature,
                                const std::function<gamesave::ObjectKey(gamesave::ObjectKey)>& rekey)
{
	gamesave::ForEachReference(creature, [&rekey](gamesave::ObjectRef& ref) {
		if (!ref.Empty())
		{
			ref.key = rekey(ref.key);
		}
	});
}
