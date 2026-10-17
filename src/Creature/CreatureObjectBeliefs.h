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

#include <array>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDecisionTree.h"

/// What a creature believes about the things it has come across. Each thing it sees, carries, is shown or acts on is
/// remembered as a belief of the thing's kind (by the game's belief types: a forest, a temple, a flock and so on), with
/// where the thing was when it last came across it, until the thing is gone from the world. A creature keeps only so
/// many beliefs of each kind; a new one beyond that takes the place of an old one. Its mind scroll counts the temples,
/// flocks and forests it believes in. Also here: how a creature's eyes sweep the map cells about it, a few cells each
/// turn in growing rings, and which of them it can see. Everything here is pure.
namespace openblack::creature_object_beliefs
{

/// The kinds of thing a creature keeps beliefs about, numbered as the game's belief types
inline constexpr uint32_t k_KindCount = 24;
/// How many beliefs of each kind a creature keeps at most: forty towns, fifty forests, twenty temples, a hundred abodes
/// and so on, one of the kind that stands for what it is doing, forty flocks
inline constexpr std::array<uint32_t, k_KindCount> k_Capacity {40,  50,  20,  100, 50,  100, 100, 100, 100, 30,  100, 40,
                                                               100, 100, 100, 100, 100, 100, 100, 100, 1,   100, 40,  20};

/// A creature keeps its beliefs in two lists. The first list holds the whole things that a desire can be about:
/// towns, forests, temples, creatures and flocks. The second list holds all other things.
[[nodiscard]] bool InListOfWholeThings(uint32_t kind);

/// What a creature believes about one thing: its kind and where it was when the creature last came across it
struct Belief
{
	entt::entity thing {entt::null};
	uint32_t kind {0};
	glm::vec3 place {0.0f};
	/// For another creature: the game turn it last looked that creature over, none before it ever has
	std::optional<uint32_t> inspected;
	/// The thing that this thing is part of (a villager's town, a tree's forest), as the creature knew it when the belief
	/// was new. None when the thing is part of nothing, or when the creature had no belief about the whole thing.
	std::optional<entt::entity> partOf;
	/// The order in which the beliefs were made: a newer belief has a larger number
	uint64_t order {0};
	/// The game turn on which the creature last found no way to the thing, none if it never did
	std::optional<uint32_t> blockedTurn;
	/// The attributes of the thing as the creature last took them in. The decision trees test these values, not the
	/// thing as it is now. None until the creature takes in the thing.
	std::optional<creature_tree::Belief> attributes;
	/// The game turn on which seeing the thing last stirred the creature's desire sources
	uint32_t sourcesTurn {0};
};

/// Whether the belief about a thing of a kind takes in the thing again each time the creature comes across it. A belief
/// about a building does not: it keeps the attributes of the first sight, and the sight stirs no desire.
[[nodiscard]] bool TakesInAgain(uint32_t kind);
/// The creature takes in what it finds of the thing now. A new belief takes all the attributes. A belief that the
/// creature has already takes only the attributes that change with the thing, if its kind takes in the thing again.
void TakeIn(Belief& belief, bool isNew, const creature_tree::Belief& now);

/// After the creature finds no way to a thing, it does not choose the thing for a time: 10 seconds for a thing that
/// moves, 60 seconds for a thing that is fixed in the land (a building, a tree), at 10 game turns each second
inline constexpr uint32_t k_BlockedTurns = 100;
inline constexpr uint32_t k_BlockedTurnsFixed = 600;
/// Whether the creature found no way to the thing in the time before this turn
[[nodiscard]] bool BlockedRecently(const Belief& belief, uint32_t turn, bool fixed);

/// A creature's beliefs about things, a fixed number of places for each kind
class Store
{
public:
	/// Comes across a thing of a kind at a place: the belief about it is told where it is now, or else a new belief is
	/// made. With all its kind's places taken, the new belief takes the place the oldest-kept marker is at, which moves on
	/// only when the belief there is forgotten, so that while the kind stays full every new belief replaces the last
	/// one put there. None for a kind beyond the game's. Whether the belief is new, as only a new one leads the creature
	/// on to what the thing belongs to.
	struct Added
	{
		Belief* belief {nullptr};
		bool isNew {false};
	};
	Added Add(entt::entity thing, uint32_t kind, glm::vec3 place);
	[[nodiscard]] const Belief* Find(entt::entity thing) const;
	[[nodiscard]] Belief* Find(entt::entity thing);
	/// Forgets the belief about a thing; whether there was one
	bool Forget(entt::entity thing);
	/// Forgets every belief about a thing that is gone
	template <typename Gone>
	void ForgetIf(Gone gone)
	{
		std::vector<entt::entity> forgotten;
		for (const auto& [thing, at] : _index)
		{
			if (gone(thing))
			{
				forgotten.push_back(thing);
			}
		}
		for (const auto thing : forgotten)
		{
			Forget(thing);
		}
	}
	/// Records that the creature found no way to the thing on a turn, if the store has a belief about the thing
	void MarkBlocked(entt::entity thing, uint32_t turn);
	/// Records that the thing is part of a whole thing, if the store has a belief about the thing
	void JoinTo(entt::entity part, entt::entity whole);
	/// The beliefs about the parts of a whole thing, the newest first
	[[nodiscard]] std::vector<Belief> PartsOf(entt::entity whole) const;
	/// Every belief, the newest first
	[[nodiscard]] std::vector<Belief> NewestFirst() const;
	/// Every belief in the order that the creature looks through its two lists: the list of whole things first, then
	/// the list of all other things. Each list has the newest belief first.
	[[nodiscard]] std::vector<Belief> InListOrder() const;
	/// How many beliefs it holds of a kind, and in all
	[[nodiscard]] size_t Count(uint32_t kind) const;
	[[nodiscard]] size_t Size() const { return _index.size(); }
	/// Every belief, kind by kind, each kind in the order of its places
	[[nodiscard]] std::vector<Belief> All() const;

private:
	struct Place
	{
		uint32_t kind;
		size_t slot;
	};
	struct Pool
	{
		/// Its places, made when the kind is first believed in
		std::vector<std::optional<Belief>> slots;
		size_t used {0};
		/// The place the next belief replaces once all are taken: the first place taken, moving on to the next place when
		/// the belief there is forgotten; none while nothing is held
		std::optional<size_t> oldest;
	};

	std::array<Pool, k_KindCount> _pools {};
	std::unordered_map<entt::entity, Place> _index;
	/// How many beliefs the store made
	uint64_t _made {0};
};

/// How a creature's eyes sweep about it: the cells of a square around its own, its own cell first and then ring after
/// ring outwards, a ninth of them each turn (at most nine), starting again from its own cell once all have been looked
/// at
struct Sweep
{
	/// The cells still to look at in this sweep, the ring it is on (1 for its own cell) and the place in the ring
	uint32_t left {0};
	uint32_t ring {1};
	uint32_t step {0};
};

/// The cells a sweep covers for a creature shown at a size: a square whose side is the odd number the square root of
/// sixty times its size plus a hundred and sixty comes down to
[[nodiscard]] uint32_t SweepCells(float shownSize);
/// The cell at a place in a ring about a cell: ring 1 is the cell itself; ring r goes round the square r - 1 cells out,
/// along its four sides in turn starting from the corner at the least x and z
[[nodiscard]] glm::ivec2 RingCell(glm::ivec2 centre, uint32_t ring, uint32_t step);
/// The cells to look at this turn, moving the sweep on; a sweep that has finished starts again with the cells for the
/// creature's size now. Cells beyond the map are among them, and are only passed over.
[[nodiscard]] std::vector<glm::ivec2> NextCells(Sweep& sweep, glm::ivec2 centre, float shownSize);

/// A creature can see a point in a direction (as a game angle) no more than a third of a circle either side of the way
/// it looks (`looking`, a game angle), and anything in its own cell
inline constexpr int32_t k_SightHalfAngle = 0x2AA;
[[nodiscard]] bool CanSee(uint16_t looking, uint16_t direction, bool sameCell);

/// A town of any player or of none, and the map cell where it stands
struct TownCell
{
	uint32_t id;
	glm::ivec2 cell;
};
/// How far apart two cells are for finding the nearest town: the longer of the two sides, and half the shorter
[[nodiscard]] int32_t TownCellDistance(glm::ivec2 a, glm::ivec2 b);
/// Each turn the creature comes across the nearest town that is not the town it came across last, and that town's
/// storage pit. So with two towns or more it comes across the two nearest in turn. Of towns equally near, the first in
/// the list counts. None when there is no other town.
[[nodiscard]] std::optional<uint32_t> NearestOtherTown(glm::ivec2 from, std::span<const TownCell> towns,
                                                       std::optional<uint32_t> last);

} // namespace openblack::creature_object_beliefs
