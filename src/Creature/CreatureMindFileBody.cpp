/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMindFileBody.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <iterator>
#include <ranges>
#include <vector>

#include <MindFile.h>
#include <PhysiqueFile.h>

#include "Creature/CreatureMorph.h"

using namespace openblack;
using namespace openblack::creature_mind_body;

namespace
{
/// The six further values of the body that the file keeps: the first is its poo, the second its exhaustion and the third
/// its dehydration. The last three are the slow, walk and run speeds that the body got from its species.
constexpr size_t k_PooNeed = 0;
constexpr size_t k_ExhaustionNeed = 1;
constexpr size_t k_DehydrationNeed = 2;
constexpr size_t k_SlowSpeedNeed = 3;
constexpr size_t k_WalkSpeedNeed = 4;
constexpr size_t k_RunSpeedNeed = 5;
} // namespace

std::optional<CreatureType> creature_mind_body::SpeciesFromRow(uint32_t row)
{
	if (row == 0)
	{
		return CreatureType::GiantApe;
	}
	if (row < creaturemind::k_SpeciesRows)
	{
		return static_cast<CreatureType>(row);
	}
	return std::nullopt;
}

float creature_mind_body::AlignmentFromFile(float alignment)
{
	// A value that is not a number, or below -1, becomes -1; a value above 1 becomes 1
	if (!(alignment >= -1.0f))
	{
		return -1.0f;
	}
	return alignment > 1.0f ? 1.0f : alignment;
}

std::optional<Body> creature_mind_body::FromMindFile(const creaturemind::MindFileData& file)
{
	const auto species = SpeciesFromRow(file.speciesRow);
	if (!species.has_value())
	{
		return std::nullopt;
	}
	Body body {.species = *species, .name = file.name};
	// The body keeps each value as the file has it, as the game does. Only a value that is not a number is not taken.
	const auto finite = [](float value) { return std::isfinite(value); };
	if (file.alignment.has_value())
	{
		body.alignment = AlignmentFromFile(*file.alignment);
	}
	if (finite(file.physique.strength))
	{
		body.strength = file.physique.strength;
	}
	if (file.physique.size.has_value() && finite(*file.physique.size))
	{
		body.size = *file.physique.size;
	}
	if (file.tattooHeader.has_value())
	{
		creature_tattoo::Slots slots {};
		std::ranges::transform(*file.tattooHeader, slots.begin(), creature_tattoo::FromWord);
		body.tattoos = slots;
	}
	if (file.tattoo.has_value())
	{
		body.tattooPictures = *file.tattoo;
	}
	const auto& physique = file.physique;
	const auto share = [&finite](float value, float otherwise) { return finite(value) ? value : otherwise; };
	body.fatness = share(physique.fatness, body.fatness);
	body.shownFatness = share(physique.shownFatness, body.shownFatness);
	creature_physiology::Kept needs {.age = physique.age, .turns = physique.turns};
	if (finite(physique.energy))
	{
		needs.energy = physique.energy;
	}
	needs.exhaustion = share(physique.needs[k_ExhaustionNeed], needs.exhaustion);
	needs.poo = share(physique.needs[k_PooNeed], needs.poo);
	needs.dehydration = share(physique.needs[k_DehydrationNeed], needs.dehydration);
	// A file older than version 6 does not keep the itchiness
	if (physique.scratch.has_value())
	{
		needs.itchiness = share(*physique.scratch, needs.itchiness);
	}
	body.needs = needs;
	const auto marks = [](const std::vector<uint32_t>& words, auto fromWord) {
		std::vector<creature_marks::Mark> kept;
		kept.reserve(std::min(words.size(), creature_marks::k_MaxMarks));
		for (const auto word : words | std::views::take(creature_marks::k_MaxMarks))
		{
			kept.push_back(fromWord(word));
		}
		return kept;
	};
	body.wounds = marks(physique.wounds, creature_marks::WoundFromWord);
	body.blood = marks(physique.blood, creature_marks::BloodFromWord);
	// The body takes these values as the file has them, also a value that is not a number
	body.bornWith = creature_born::BornWith {.facing = physique.flags,
	                                         .slowSpeed = physique.needs[k_SlowSpeedNeed],
	                                         .walkSpeed = physique.needs[k_WalkSpeedNeed],
	                                         .runSpeed = physique.needs[k_RunSpeedNeed]};
	if (physique.unknown3.has_value())
	{
		body.environmentMap = std::bit_cast<uint32_t>(*physique.unknown3);
	}
	return body;
}

void creature_mind_body::ToMindFile(const Body& body, creaturemind::MindFileData& file)
{
	if (body.alignment.has_value())
	{
		file.alignment = *body.alignment;
	}
	auto& physique = file.physique;
	physique.strength = body.strength;
	if (body.size.has_value())
	{
		physique.size = *body.size;
	}
	physique.fatness = body.fatness;
	physique.shownFatness = body.shownFatness;
	if (body.needs.has_value())
	{
		physique.age = body.needs->age;
		physique.turns = body.needs->turns;
		physique.energy = body.needs->energy;
		physique.needs[k_ExhaustionNeed] = body.needs->exhaustion;
		physique.needs[k_PooNeed] = body.needs->poo;
		physique.needs[k_DehydrationNeed] = body.needs->dehydration;
		physique.scratch = body.needs->itchiness;
	}
	if (body.tattoos.has_value())
	{
		std::array<uint32_t, creature_tattoo::k_SlotCount> words {};
		std::ranges::transform(*body.tattoos, words.begin(), creature_tattoo::ToWord);
		file.tattooHeader = words;
	}
	if (body.tattooPictures.has_value())
	{
		file.tattoo = *body.tattooPictures;
	}
	if (body.bornWith.has_value())
	{
		physique.flags = body.bornWith->facing;
		physique.needs[k_SlowSpeedNeed] = body.bornWith->slowSpeed;
		physique.needs[k_WalkSpeedNeed] = body.bornWith->walkSpeed;
		physique.needs[k_RunSpeedNeed] = body.bornWith->runSpeed;
	}
	if (body.environmentMap.has_value())
	{
		physique.unknown3 = std::bit_cast<int32_t>(*body.environmentMap);
	}
	physique.wounds.clear();
	std::ranges::transform(body.wounds, std::back_inserter(physique.wounds), creature_marks::WoundToWord);
	physique.blood.clear();
	std::ranges::transform(body.blood, std::back_inserter(physique.blood), creature_marks::BloodToWord);
}

creaturemind::PhysiqueFileData creature_mind_body::ToPhysiqueFile(const Body& now, uint32_t speciesRow, float savedAlignment)
{
	creaturemind::PhysiqueFileData physique {.speciesRow = speciesRow,
	                                         .drawnSize = now.size.value_or(1.0f),
	                                         .strength = now.strength,
	                                         .fatness = now.fatness,
	                                         .alignment = savedAlignment};
	std::ranges::transform(now.blood, std::back_inserter(physique.blood), creature_marks::BloodToWord);
	std::ranges::transform(now.wounds, std::back_inserter(physique.wounds), creature_marks::WoundToWord);
	return physique;
}
