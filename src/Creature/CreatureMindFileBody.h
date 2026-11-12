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

#include "Creature/CreatureBornWith.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreatureTattoo.h"
#include "Enums.h"

namespace openblack::creaturemind
{
struct MindFileData;
struct PhysiqueFileData;
} // namespace openblack::creaturemind

/// The creature a mind file describes besides its mind: a saved creature carries its species, name, alignment,
/// strength, fatness, size, age, energy, tiredness, tattoos, the marks on its skin and the magic mushrooms it ate. So
/// the game can make the creature again from the file alone. Older versions do not have the size or tattoos, which
/// then stay as the species starts them.
namespace openblack::creature_mind_body
{

/// What a creature counted of its life: the people, animals and creatures that it killed, the fights that it started and
/// won, and its poos. The mind file keeps these as its first six counters.
struct LifeCounts
{
	uint32_t peopleKilled {0};
	uint32_t animalsKilled {0};
	uint32_t creaturesKilled {0};
	uint32_t battlesFought {0};
	uint32_t battlesWon {0};
	uint32_t poos {0};

	bool operator==(const LifeCounts&) const = default;
};

struct Body
{
	CreatureType species {CreatureType::Unknown};
	std::u16string name;
	/// -1 (evil) to 1 (good). A file older than version 11 does not have it, and a body that loads such a file gets
	/// the alignment 0.
	std::optional<float> alignment;
	/// The alignment as the file keeps it, not held to -1..1. A load while a nice or nasty spell is on puts this
	/// value into the spell. The spell puts it back at its end.
	std::optional<float> fileAlignment;
	/// 0 to 1
	float strength {0.5f};
	std::optional<float> size;
	std::optional<creature_tattoo::Slots> tattoos;
	/// The block of the player's own pictures that its tattoos use (see creature_tattoo::PicturesBlock)
	std::optional<std::vector<uint8_t>> tattooPictures;
	/// Its fatness, 0 to 1, and the fatness its body shows, which follows it a little each turn
	float fatness {0.5f};
	float shownFatness {0.5f};
	/// How its body is doing, when known: a creature whose body hasn't been looked after yet has none to save
	std::optional<creature_physiology::Kept> needs;
	/// The wounds and blood on its skin
	std::vector<creature_marks::Mark> wounds;
	std::vector<creature_marks::Mark> blood;
	/// The values that its body got when the game made it (see creature_born::BornWith)
	std::optional<creature_born::BornWith> bornWith;
	/// The word that tells the draw to show its body with the environment map. A file older than version 22 does not
	/// have it, and a body that loads such a file keeps its own word.
	std::optional<uint32_t> environmentMap;
	/// The count of magic mushrooms that it ate, which makes it want to get high and makes it mentally ill. A file
	/// older than version 17 does not have it, and a body that loads such a file keeps its own count.
	std::optional<uint32_t> magicMushroomsEaten;
	/// What it counted of its life. A file with fewer than six counters does not have them, and a body that loads such a
	/// file keeps its own counts.
	std::optional<LifeCounts> counts;
};

/// The species of a row of the game's creature tables, which start with the Giant Ape
[[nodiscard]] std::optional<CreatureType> SpeciesFromRow(uint32_t row);

/// The alignment that a body takes from a mind file: -1 for a value below -1 or not a number, 1 for a value above 1
[[nodiscard]] float AlignmentFromFile(float alignment);

/// The creature the file describes, or none when its species row isn't one of the game's
[[nodiscard]] std::optional<Body> FromMindFile(const creaturemind::MindFileData& file);

/// The creature's body written over a file of the current version, as it is saved with its mind: everything but its
/// species and name, which go with the mind
void ToMindFile(const Body& body, creaturemind::MindFileData& file);

/// The physique file saved beside the mind: the species row, the body's size, strength and fatness as they are now, the
/// alignment it is saved with (as it was before any spell changed it) and the marks on its skin
[[nodiscard]] creaturemind::PhysiqueFileData ToPhysiqueFile(const Body& now, uint32_t speciesRow, float savedAlignment);

} // namespace openblack::creature_mind_body
