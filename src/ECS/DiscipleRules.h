/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "Enums.h"

/// The rules of disciples: a villager held in the player's hand takes up the work of what it is held near, a field, a
/// tree, a fish farm, a building to build, a house to move into, another town to preach to or trade with, a villager to
/// breed with, and keeps at it once put down. Free of the game's state so they can be tested on their own.
namespace openblack::disciple
{

/// Things are looked for in this many map cells round the held villager, its own cell first
inline constexpr size_t k_CellsSearched = 9;
/// How far from a thing's edge its interest falls away to nothing, in metres
inline constexpr float k_InterestReach = 6.0f;
/// A thing of less interest than this isn't looked at at all
inline constexpr float k_LeastInterest = 0.001f;

/// How much a thing held near draws the villager: the game's distance falloff of the gap between their edges (none when
/// they overlap) out to six metres, times how much its kind draws villagers
[[nodiscard]] float Interest(float edgeGap, float kindDesire);

/// The things worth looking at, in the order they are looked at: the most interesting first, those of equal interest in
/// the order they were found. Things of too little interest are left out.
[[nodiscard]] std::vector<size_t> OrderByInterest(std::span<const float> interests);

/// What a thing near the held villager is, as the choice reads it
enum class NearKind : uint8_t
{
	/// Any building of a town other than those below, such as a creche, a graveyard, a wonder or the totem
	Building,
	/// A house people live in
	House,
	StoragePit,
	TownCentre,
	Workshop,
	WorshipSite,
	Field,
	Tree,
	/// A wood the land is laid out with
	Forest,
	FishFarm,
	Villager,
};

/// What the choice needs to know of a thing held near
struct NearFacts
{
	NearKind kind {NearKind::Building};
	/// The player the thing belongs to, its town's; none for a thing of no town
	std::optional<PlayerNames> owner;
	/// Whether it belongs to a town, and whether that town can be lived in
	bool hasTown {false};
	bool townHabitable {true};
	/// A building or field: whether it is built up, whether it is whole, and whether it works (built and not broken)
	bool built {true};
	bool repaired {true};
	bool functional {true};
	/// A house: whether it has room for another adult, and whether it is the held villager's own home
	bool roomForAdults {false};
	bool heldVillagersHome {false};
	/// Whether it is of the held villager's town
	bool heldVillagersTown {false};
	/// A town centre: whether its town has room for another villager
	bool townHasRoom {false};
	/// A worship site: whether it is the held villager's town's
	bool heldVillagersWorshipSite {false};
	/// Another villager: whether it is about its life, of an age to make love, and a man
	bool villagerFunctional {false};
	bool villagerSexuallyActive {false};
	bool villagerMale {false};
};

/// What the choice needs to know of the held villager and the hand holding it
struct HeldFacts
{
	/// The player of the hand holding it, and its own player
	std::optional<PlayerNames> hand;
	std::optional<PlayerNames> villager;
	bool sexuallyActive {false};
	bool male {false};
	bool child {false};
	/// A script holds it, it isn't about its life, it is dead or dying: it takes up no work but worship (for a child)
	bool otherwiseRefused {false};
};

/// The work a thing offers the held villager, if any
[[nodiscard]] VillagerDisciple Offered(const NearFacts& near, const HeldFacts& held);

/// The work the held villager takes up, and which of the things looked at (by its index there) offered it
struct Choice
{
	VillagerDisciple type {VillagerDisciple::None};
	std::optional<size_t> from;
};

/// Each game turn in the hand the villager takes up the work of the first thing, in the order looked at, that offers it
/// some it can take. A child, or one otherwise refused, takes up only worship, and only a child that. With nothing
/// offering, it goes back to the work it had when it was picked up.
[[nodiscard]] Choice ChooseWhileHeld(std::span<const NearFacts> ordered, const HeldFacts& held, VillagerDisciple atPickUp);

/// The work a villager is taken to have had as it is picked up: a missionary picked up has given up preaching
[[nodiscard]] VillagerDisciple AtPickUp(VillagerDisciple current);

/// What the disciples of each kind are and do once made, as the game's table of disciples has them
struct Kind
{
	/// The state it is put in as it lands by a thing, none for walking up to the thing and looking at it first
	VillagerStates landingState {VillagerStates::InvalidState};
	/// Others of its town about it take up the same work, once, when it sets about its work
	bool leadsFollowers {false};
	/// It glows, and shows its symbol in or under the hand
	bool glows {false};
	/// With no work to be had it waits by the town centre
	bool waitsWhenIdle {false};
	/// The town desire making it moves its player's alignment; none for no desire
	std::optional<TownDesireInfo> desire;
	/// Put by a thing of another town, it moves into that town
	bool movesIntoTown {false};
};
[[nodiscard]] Kind KindOf(VillagerDisciple type);

/// The cell of the hand's symbol sheet showing a disciple of a kind, eight to a row; none for a kind with no symbol
[[nodiscard]] std::optional<uint32_t> SymbolCell(VillagerDisciple type);

/// The guidance bank's sample that says a disciple was made, none for a kind with nothing to say. A villager made to
/// move house is said one of two ways, at random by `chance` (0 to 1), and an evil player's way differs.
[[nodiscard]] std::optional<uint32_t> MadeSample(VillagerDisciple type, float playerAlignment, float chance);

/// How making a disciple moves its player's alignment, before the player's alignment damps it: good when its town wants
/// what it does more than the good threshold, evil when less than the evil one, nothing between or for a kind serving no
/// desire
[[nodiscard]] float AlignmentForMaking(std::optional<float> townDesire, float evilBelow, float goodAbove, float change);
/// A change of alignment as the player's alignment damps it: a change the way the player already leans counts for less,
/// the other way for more, by half the player's alignment
[[nodiscard]] float DampedAlignment(float playerAlignment, float change);

/// Where an idle disciple waits, from the town centre: a little beyond its edge, within an eighth of a turn either side
/// of the side the disciple comes from (the angle and distance to step from the centre)
struct WaitStep
{
	float radians;
	float metres;
};
/// `towardsDisciple` is the angle from the centre to the disciple; `random(limit)` gives a number from 0 to limit, the
/// angle's taken first
[[nodiscard]] WaitStep WaitStepFromCentre(float towardsDisciple, float centreRadius, const std::function<float(float)>& random);

/// An idle disciple looks for work again this many game turns after finding none
inline constexpr int32_t k_IdleRetryTurns = 300;
/// The turns before it looks again when its town's wants have just changed: random below this
inline constexpr uint32_t k_IdleRetryWhenChanged = 10;

/// A villager made a disciple walks up to the thing it was put by when farther than this from its edge
inline constexpr float k_InspectReach = 5.0f;

} // namespace openblack::disciple
