/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureRecord.h"

using namespace openblack::gamesave;

namespace
{
/// The most queued spells or dance partners that a read accepts, so that bad bytes cannot ask for a huge list
constexpr uint32_t k_MostListed = 0x10000;
/// The most steps that an agenda has
constexpr uint32_t k_MostSteps = 32;

void Put(ByteWriter& writer, const SubAction& step)
{
	writer.Put(step.action);
	writer.Put(step.object);
	writer.Put(step.word10);
	writer.Put(step.point);
	writer.Put(step.word20);
	writer.Put(step.word24);
	writer.Put(step.word28);
	writer.Put(step.other);
	writer.Put(step.word30);
}

void Get(ByteReader& reader, SubAction& step)
{
	reader.Get(step.action);
	reader.Get(step.object);
	reader.Get(step.word10);
	reader.Get(step.point);
	reader.Get(step.word20);
	reader.Get(step.word24);
	reader.Get(step.word28);
	reader.Get(step.other);
	reader.Get(step.word30);
}

void Put(ByteWriter& writer, const KeptPlan& plan)
{
	writer.Put(plan.desire);
	writer.Put(plan.action);
	for (const auto object : plan.objects)
	{
		writer.Put(object);
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
	writer.Put(agenda.creature);
	writer.Put(agenda.other);
}

bool Get(ByteReader& reader, KeptPlan& plan)
{
	reader.Get(plan.desire);
	reader.Get(plan.action);
	for (auto& object : plan.objects)
	{
		reader.Get(object);
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
	reader.Get(agenda.creature);
	reader.Get(agenda.other);
	return !reader.Failed();
}
} // namespace

void openblack::gamesave::Write(ByteWriter& writer, const CreatureRecord& record)
{
	writer.Put(record.version);
	writer.Put(record.savedInTemple);
	writer.Put(record.index);
	writer.Put(record.owner);
	for (const auto& slot : record.spells)
	{
		writer.Put(slot.start);
		writer.Put(slot.phase);
		writer.Put(slot.turnsLeft);
		writer.Put(slot.found);
		writer.Put(slot.holdTurns);
		writer.Put(slot.miracle);
	}
	writer.Put(static_cast<uint32_t>(record.queued.size()));
	for (const auto& queued : record.queued)
	{
		writer.Put(queued.spell);
		writer.Put(queued.seconds);
		writer.Put(queued.miracle);
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
	for (const auto visual : record.spellVisuals)
	{
		writer.Put(visual);
	}
	for (const auto particles : record.handParticles)
	{
		writer.Put(particles);
	}
	writer.Put(record.targetPoint);
	writer.Put(record.targetParticles);
	writer.Put(record.targetObject);
	writer.Put(record.hasTarget);
	writer.Put(record.arena);
	writer.Put(record.home);
	writer.Put(record.fightHealth);
	writer.Put(record.autoFight);
	writer.Put(static_cast<uint32_t>(record.dancePartners.size()));
	for (const auto partner : record.dancePartners)
	{
		writer.Put(partner);
	}
	writer.Put(record.dance);
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

std::vector<uint8_t> openblack::gamesave::Write(const CreatureRecord& record)
{
	ByteWriter writer;
	Write(writer, record);
	return writer.Take();
}

std::optional<CreatureRecord> openblack::gamesave::ReadCreatureRecord(ByteReader& reader)
{
	CreatureRecord record;
	reader.Get(record.version);
	reader.Get(record.savedInTemple);
	reader.Get(record.index);
	reader.Get(record.owner);
	for (auto& slot : record.spells)
	{
		reader.Get(slot.start);
		reader.Get(slot.phase);
		reader.Get(slot.turnsLeft);
		reader.Get(slot.found);
		reader.Get(slot.holdTurns);
		reader.Get(slot.miracle);
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
		reader.Get(queued.miracle);
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
		reader.Get(visual);
	}
	for (auto& particles : record.handParticles)
	{
		reader.Get(particles);
	}
	reader.Get(record.targetPoint);
	reader.Get(record.targetParticles);
	reader.Get(record.targetObject);
	reader.Get(record.hasTarget);
	reader.Get(record.arena);
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
		reader.Get(partner);
	}
	reader.Get(record.dance);
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

std::optional<CreatureRecord> openblack::gamesave::ReadCreatureRecord(std::span<const uint8_t> bytes)
{
	ByteReader reader(bytes);
	return ReadCreatureRecord(reader);
}

void openblack::gamesave::Write(ByteWriter& writer, const LeashRecord& record)
{
	writer.Put(record.on);
	writer.Put(record.works);
	writer.Put(record.type);
	writer.Put(record.player);
	writer.Put(record.word20);
	writer.Put(record.tiedTo);
	writer.Put(record.length);
	writer.Put(record.fullLength);
	writer.Put(record.holder);
	writer.Put(record.word34);
}

std::optional<LeashRecord> openblack::gamesave::ReadLeashRecord(ByteReader& reader)
{
	LeashRecord record;
	reader.Get(record.on);
	reader.Get(record.works);
	reader.Get(record.type);
	reader.Get(record.player);
	reader.Get(record.word20);
	reader.Get(record.tiedTo);
	reader.Get(record.length);
	reader.Get(record.fullLength);
	reader.Get(record.holder);
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
