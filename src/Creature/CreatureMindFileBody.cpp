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
#include <iterator>
#include <ranges>
#include <vector>

#include <MindFile.h>

#include "Creature/CreatureMorph.h"

using namespace openblack;
using namespace openblack::creature_mind_body;

namespace
{
/// Of the six further values of the body the file keeps, the second is its exhaustion
constexpr size_t k_ExhaustionNeed = 1;
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

std::optional<Body> creature_mind_body::FromMindFile(const creaturemind::MindFileData& file)
{
	const auto species = SpeciesFromRow(file.speciesRow);
	if (!species.has_value())
	{
		return std::nullopt;
	}
	Body body {.species = *species, .name = file.name};
	const auto finite = [](float value) { return std::isfinite(value); };
	if (file.alignment.has_value() && finite(*file.alignment))
	{
		body.alignment = std::clamp(*file.alignment, -1.0f, 1.0f);
	}
	if (finite(file.physique.strength))
	{
		body.strength = std::clamp(file.physique.strength, 0.0f, 1.0f);
	}
	if (file.physique.size.has_value() && finite(*file.physique.size) && *file.physique.size > 0.0f)
	{
		body.size = std::clamp(*file.physique.size, creature_morph::k_MinScale, creature_morph::k_MaxScale);
	}
	if (file.tattooHeader.has_value())
	{
		creature_tattoo::Slots slots {};
		std::ranges::transform(*file.tattooHeader, slots.begin(), creature_tattoo::FromWord);
		body.tattoos = slots;
	}
	const auto& physique = file.physique;
	const auto share = [&finite](float value, float otherwise) {
		return finite(value) ? std::clamp(value, 0.0f, 1.0f) : otherwise;
	};
	body.fatness = share(physique.fatness, body.fatness);
	body.shownFatness = share(physique.shownFatness, body.shownFatness);
	creature_physiology::Kept needs {.age = physique.age, .turns = physique.turns};
	if (finite(physique.energy))
	{
		needs.energy = std::max(physique.energy, 0.0f);
	}
	needs.exhaustion = share(physique.needs[k_ExhaustionNeed], needs.exhaustion);
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
	}
	if (body.tattoos.has_value())
	{
		std::array<uint32_t, creature_tattoo::k_SlotCount> words {};
		std::ranges::transform(*body.tattoos, words.begin(), creature_tattoo::ToWord);
		file.tattooHeader = words;
	}
	physique.wounds.clear();
	std::ranges::transform(body.wounds, std::back_inserter(physique.wounds), creature_marks::WoundToWord);
	physique.blood.clear();
	std::ranges::transform(body.blood, std::back_inserter(physique.blood), creature_marks::BloodToWord);
}
