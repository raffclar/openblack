/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSaveState.h"

using namespace openblack;
using namespace openblack::creature_save_state;

gamesave::Ref creature_save_state::ToRef(entt::entity entity)
{
	return entity == entt::null ? 0u : static_cast<gamesave::Ref>(entt::to_integral(entity)) + 1u;
}

gamesave::Ref creature_save_state::ToRef(std::optional<entt::entity> entity)
{
	return entity.has_value() ? ToRef(*entity) : 0u;
}

std::optional<entt::entity> creature_save_state::FromRef(gamesave::Ref ref)
{
	if (ref == 0)
	{
		return std::nullopt;
	}
	return static_cast<entt::entity>(ref - 1);
}

bool creature_save_state::KeepsPlan(bool scriptControlled, bool fighting, size_t dancers)
{
	return scriptControlled || fighting || dancers > 0;
}

gamesave::SpellSlot creature_save_state::ToSlot(const creature_spells::Slot& slot)
{
	return {.start = gamesave::k_SpellSlotStart,
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
	return player.has_value() ? static_cast<uint8_t>(*player) : gamesave::k_None;
}

std::optional<PlayerNames> creature_save_state::PlayerFromNumber(uint8_t number)
{
	if (number >= static_cast<uint8_t>(PlayerNames::_COUNT))
	{
		return std::nullopt;
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

gamesave::Agenda creature_save_state::AgendaAfterLoad(gamesave::Agenda agenda)
{
	agenda.word08 = 1;
	agenda.state = 0;
	agenda.word20 = 1;
	return agenda;
}
