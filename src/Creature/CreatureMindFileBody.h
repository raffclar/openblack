/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <string>
#include <vector>

#include "Creature/CreatureMarks.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreatureTattoo.h"
#include "Enums.h"

namespace openblack::creaturemind
{
struct MindFileData;
}

/// The creature a mind file describes besides its mind: a saved creature carries its species, name, alignment,
/// strength, fatness, size, age, energy, tiredness, tattoos and the marks on its skin, so that a creature can be made
/// again from the file alone. Older versions leave out the
/// size, alignment or tattoos, which then stay as the species starts them.
namespace openblack::creature_mind_body
{

struct Body
{
	CreatureType species {CreatureType::Unknown};
	std::u16string name;
	/// -1 (evil) to 1 (good)
	std::optional<float> alignment;
	/// 0 to 1
	float strength {0.5f};
	std::optional<float> size;
	std::optional<creature_tattoo::Slots> tattoos;
	/// Its fatness, 0 to 1, and the fatness its body shows, which follows it a little each turn
	float fatness {0.5f};
	float shownFatness {0.5f};
	/// How its body is doing, when known: a creature whose body hasn't been looked after yet has none to save
	std::optional<creature_physiology::Kept> needs;
	/// The wounds and blood on its skin
	std::vector<creature_marks::Mark> wounds;
	std::vector<creature_marks::Mark> blood;
};

/// The species of a row of the game's creature tables, which start with the Giant Ape
[[nodiscard]] std::optional<CreatureType> SpeciesFromRow(uint32_t row);

/// The creature the file describes, or none when its species row isn't one of the game's
[[nodiscard]] std::optional<Body> FromMindFile(const creaturemind::MindFileData& file);

/// The creature's body written over a file of the current version, as it is saved with its mind: everything but its
/// species and name, which go with the mind
void ToMindFile(const Body& body, creaturemind::MindFileData& file);

} // namespace openblack::creature_mind_body
