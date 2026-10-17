/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureRecord.h"

#include "SavedObjects.h"

using namespace openblack::gamesave;

namespace
{
/// The most queued spells or dance partners that a read accepts, so that bad bytes cannot ask for a huge list
constexpr uint32_t k_MostListed = 0x10000;
/// The most steps that an agenda has
constexpr uint32_t k_MostSteps = 32;

void Put(StreamWriter& writer, const SubAction& step)
{
	writer.Put(step.action);
	Write(writer, step.object);
	writer.Put(step.integer);
	writer.Put(step.point);
	writer.Put(step.real);
	writer.Put(step.spell);
	writer.Put(step.word28);
	Write(writer, step.other);
	writer.Put(step.orderKind);
}

void Get(StreamReader& reader, SubAction& step)
{
	reader.Get(step.action);
	Read(reader, step.object);
	reader.Get(step.integer);
	reader.Get(step.point);
	reader.Get(step.real);
	reader.Get(step.spell);
	reader.Get(step.word28);
	Read(reader, step.other);
	reader.Get(step.orderKind);
}

void Put(StreamWriter& writer, const KeptPlan& plan)
{
	writer.Put(plan.desire);
	writer.Put(plan.action);
	for (const auto& object : plan.objects)
	{
		Write(writer, object);
	}
	const auto& agenda = plan.agenda;
	writer.Put(agenda.word08);
	writer.Put(agenda.current);
	writer.Put(agenda.state);
	writer.Put(agenda.count);
	writer.Put(agenda.word18);
	writer.Put(agenda.word1C);
	writer.Put(agenda.word20);
	// The game writes the steps from the current step to the last, as many as the count says
	const auto steps = agenda.count > agenda.current ? agenda.count - agenda.current : 0u;
	for (uint32_t i = 0; i < steps; ++i)
	{
		Put(writer, i < agenda.steps.size() ? agenda.steps[i] : SubAction {});
	}
	Write(writer, agenda.creature);
	Write(writer, agenda.other);
}

bool Get(StreamReader& reader, KeptPlan& plan)
{
	reader.Get(plan.desire);
	reader.Get(plan.action);
	for (auto& object : plan.objects)
	{
		Read(reader, object);
	}
	auto& agenda = plan.agenda;
	reader.Get(agenda.word08);
	reader.Get(agenda.current);
	reader.Get(agenda.state);
	reader.Get(agenda.count);
	reader.Get(agenda.word18);
	reader.Get(agenda.word1C);
	reader.Get(agenda.word20);
	if (reader.Failed() || agenda.count > k_MostSteps)
	{
		return false;
	}
	const auto steps = agenda.count > agenda.current ? agenda.count - agenda.current : 0u;
	agenda.steps.resize(steps);
	for (auto& step : agenda.steps)
	{
		Get(reader, step);
	}
	Read(reader, agenda.creature);
	Read(reader, agenda.other);
	return !reader.Failed();
}
} // namespace

std::array<SpellSlot, k_CreatureSpellCount> openblack::gamesave::NewSpellSlots()
{
	std::array<SpellSlot, k_CreatureSpellCount> slots {};
	for (uint32_t i = 0; i < slots.size(); ++i)
	{
		slots.at(i).spell = i;
	}
	return slots;
}

void openblack::gamesave::Write(StreamWriter& writer, const CreatureRecord& record)
{
	writer.Put(record.version);
	writer.Put(record.savedInTemple);
	writer.Put(record.index);
	writer.Put(record.owner);
	for (const auto& slot : record.spells)
	{
		writer.Put(slot.spell);
		writer.Put(slot.phase);
		writer.Put(slot.turnsLeft);
		writer.Put(slot.found);
		writer.Put(slot.holdTurns);
		Write(writer, slot.miracle);
	}
	writer.Put(static_cast<uint32_t>(record.queued.size()));
	for (const auto& queued : record.queued)
	{
		writer.Put(queued.spell);
		writer.Put(queued.seconds);
		Write(writer, queued.miracle);
	}
	writer.Put(record.mindProcessing);
	writer.Put(record.largestSize);
	writer.Put(record.smallestSize);
	writer.Put(record.mayGoThroughVortex);
	writer.Put(record.fizzedForGood);
	writer.Put(record.fizz);
	writer.Put(record.fizzTarget);
	writer.Put(record.fizzPerSecond);
	writer.Put(record.freeze);
	writer.Put(record.word12C0);
	for (const auto& visual : record.spellVisuals)
	{
		Write(writer, visual);
	}
	for (const auto& particles : record.handParticles)
	{
		Write(writer, particles);
	}
	writer.Put(record.targetPoint);
	Write(writer, record.targetParticles);
	Write(writer, record.targetObject);
	writer.Put(record.hasTarget);
	Write(writer, record.arena);
	writer.Put(record.home);
	writer.Put(record.fightHealth);
	writer.Put(record.autoFight);
	writer.Put(static_cast<uint32_t>(record.dancePartners.size()));
	for (const auto& partner : record.dancePartners)
	{
		Write(writer, partner);
	}
	Write(writer, record.dance);
	writer.Put(record.canDie);
	writer.Put(record.strength);
	writer.Put(record.size);
	writer.Put(record.alignment);
	writer.Put(record.animationPace);
	writer.Put(record.autoScale);
	writer.Put(record.autoScaleShare);
	writer.Put(static_cast<uint32_t>(record.plan.has_value() ? 1 : 0));
	if (record.plan.has_value())
	{
		Put(writer, *record.plan);
	}
	for (const auto& power : record.creedPowers)
	{
		writer.Put(power.scale);
		writer.Put(power.power);
	}
}

std::optional<CreatureRecord> openblack::gamesave::ReadCreatureRecord(StreamReader& reader)
{
	CreatureRecord record;
	reader.Get(record.version);
	reader.Get(record.savedInTemple);
	reader.Get(record.index);
	reader.Get(record.owner);
	for (auto& slot : record.spells)
	{
		reader.Get(slot.spell);
		reader.Get(slot.phase);
		reader.Get(slot.turnsLeft);
		reader.Get(slot.found);
		reader.Get(slot.holdTurns);
		Read(reader, slot.miracle);
	}
	uint32_t count = 0;
	if (!reader.Get(count) || count > k_MostListed)
	{
		return std::nullopt;
	}
	record.queued.resize(count);
	for (auto& queued : record.queued)
	{
		reader.Get(queued.spell);
		reader.Get(queued.seconds);
		Read(reader, queued.miracle);
	}
	// Older records have no processing word, and the mind processes
	if (record.version > k_LastVersionWithoutProcessing)
	{
		reader.Get(record.mindProcessing);
	}
	reader.Get(record.largestSize);
	reader.Get(record.smallestSize);
	reader.Get(record.mayGoThroughVortex);
	reader.Get(record.fizzedForGood);
	reader.Get(record.fizz);
	reader.Get(record.fizzTarget);
	reader.Get(record.fizzPerSecond);
	reader.Get(record.freeze);
	reader.Get(record.word12C0);
	for (auto& visual : record.spellVisuals)
	{
		Read(reader, visual);
	}
	for (auto& particles : record.handParticles)
	{
		Read(reader, particles);
	}
	reader.Get(record.targetPoint);
	Read(reader, record.targetParticles);
	Read(reader, record.targetObject);
	reader.Get(record.hasTarget);
	Read(reader, record.arena);
	reader.Get(record.home);
	reader.Get(record.fightHealth);
	reader.Get(record.autoFight);
	if (!reader.Get(count) || count > k_MostListed)
	{
		return std::nullopt;
	}
	record.dancePartners.resize(count);
	for (auto& partner : record.dancePartners)
	{
		Read(reader, partner);
	}
	Read(reader, record.dance);
	reader.Get(record.canDie);
	reader.Get(record.strength);
	reader.Get(record.size);
	reader.Get(record.alignment);
	reader.Get(record.animationPace);
	reader.Get(record.autoScale);
	reader.Get(record.autoScaleShare);
	uint32_t kept = 0;
	reader.Get(kept);
	if (kept != 0)
	{
		KeptPlan plan;
		if (!Get(reader, plan))
		{
			return std::nullopt;
		}
		record.plan = std::move(plan);
	}
	for (auto& power : record.creedPowers)
	{
		reader.Get(power.scale);
		reader.Get(power.power);
	}
	if (reader.Failed())
	{
		return std::nullopt;
	}
	return record;
}

void openblack::gamesave::Write(StreamWriter& writer, const LeashRecord& record)
{
	writer.Put(record.on);
	writer.Put(record.works);
	writer.Put(record.type);
	writer.Put(record.player);
	writer.Put(record.word20);
	Write(writer, record.tiedTo);
	writer.Put(record.length);
	writer.Put(record.fullLength);
	Write(writer, record.holder);
	writer.Put(record.word34);
}

std::optional<LeashRecord> openblack::gamesave::ReadLeashRecord(StreamReader& reader)
{
	LeashRecord record;
	reader.Get(record.on);
	reader.Get(record.works);
	reader.Get(record.type);
	reader.Get(record.player);
	reader.Get(record.word20);
	Read(reader, record.tiedTo);
	reader.Get(record.length);
	reader.Get(record.fullLength);
	Read(reader, record.holder);
	reader.Get(record.word34);
	if (reader.Failed())
	{
		return std::nullopt;
	}
	return record;
}

uint8_t openblack::gamesave::ToByte(const KnownLeashes& known)
{
	return static_cast<uint8_t>((known.evil ? 1 : 0) + (known.rope ? 2 : 0) + (known.good ? 4 : 0));
}

KnownLeashes openblack::gamesave::KnownLeashesFromByte(uint8_t byte)
{
	return {.evil = (byte & 1) != 0, .rope = (byte & 2) != 0, .good = (byte & 4) != 0};
}
