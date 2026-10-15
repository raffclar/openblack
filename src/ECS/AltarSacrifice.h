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

/// The rules of a sacrifice by the hand at a worship site's altar. The player lets go of a villager, an animal, a tree
/// or a dead tree with the pointer on the altar: the altar takes it, and its life goes into the site's stored prayer
/// power.
namespace openblack::ecs::altar_sacrifice
{

/// What the hand gives to the altar
enum class Offering : uint8_t
{
	Villager,
	Animal,
	Tree,
	DeadTree,
};

/// What the rules need to know of the thing given and of the altar's site
struct Offered
{
	Offering kind {Offering::Villager};
	/// Its life, 0 to 1
	float life {1.0f};
	/// The sacrifice value of its kind in the game's tables
	float tableValue {0.0f};
	bool child {false};
	/// A script made it indestructible
	bool indestructible {false};
	/// The altar's worship site stands wholly built
	bool siteBuilt {false};
};

/// What the sacrifice does
struct Sacrifice
{
	/// The prayer power that goes into the site's store, and into the count of what was sacrificed there
	float prayer {0.0f};
	/// The player's creature may see the deed and copy it
	bool deed {false};
	/// A villager dies as a sacrifice, put down to the player
	bool villagerDies {false};
	/// An animal counts as sacrificed by the player, and moves the player's alignment
	bool animalCounts {false};
	/// A light rises from the top of the altar
	bool light {false};
	/// A ghost of the thing flickers out where it was
	bool ghost {false};
	/// The prayer power floats up as a number from the point the pointer is on, for the player at this machine only
	bool number {false};
};

/// A villager is worth a quarter more as a child
constexpr float k_ChildBonus = 1.25f;
/// An animal's death moves its sacrificer's alignment half as much as a person's
constexpr float k_AnimalAlignmentShare = 0.5f;
/// The game's sacrifice sound, from its own bank, where the hand is
constexpr uint32_t k_SacrificeSample = 179;
/// The colour of the number that floats up (0xAARRGGBB): red
constexpr uint32_t k_NumberColour = 0xFFFF0000;
/// The help event the player at this machine triggers by using a held thing on an altar
constexpr uint32_t k_AltarHelpEvent = 7;
/// The help event the player at this machine triggers by using a held thing on anything else
constexpr uint32_t k_UsedOnThingHelpEvent = 6;

/// The sacrifice value of a thing: its kind's value, from half of it with no life left to all of it at full life
[[nodiscard]] float SacrificeValue(float life, float tableValue);

/// What a sacrifice at the altar does, none when the altar refuses the thing and it stays in the hand. A villager is
/// refused while the site isn't built and when it is indestructible; a tree is refused when it is indestructible.
[[nodiscard]] std::optional<Sacrifice> Judge(const Offered& offered);

/// The change to its sacrificer's alignment from an animal sacrificed, from the change for a sacrifice in the player's
/// table
[[nodiscard]] float AnimalAlignmentChange(float sacrificeChange);

/// The cry a sacrificed villager gives: a random one of these samples of the editor's bank
struct Cry
{
	uint32_t first {0};
	uint32_t count {0};
};
/// A man's or a woman's cry; a child gives none (see docs/vanilla-bugs.md)
[[nodiscard]] std::optional<Cry> CryOf(bool child, bool female);

} // namespace openblack::ecs::altar_sacrifice
