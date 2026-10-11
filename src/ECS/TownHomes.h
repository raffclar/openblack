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

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>

/// Where a town's people live, as the game has it: a villager joining a town moves into the building that suits it best,
/// or is one of the town's homeless; a building's people are made homeless when it goes. A building's people and a town's
/// homeless are both kept newest first.
namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::town_homes
{

/// How many a building can house, and whether it works
struct AbodeRoom
{
	uint32_t maxAdults {0};
	uint32_t maxChildren {0};
	bool functional {false};
	/// It has been built: its places count among its town's room from then on
	bool built {false};
	/// The share of its adult places past which it is too crowded
	float tooCrowded {1.0f};
};

/// What the rules ask of the game's buildings and people
struct Homes
{
	/// The room of a building, none for what isn't one
	std::function<std::optional<AbodeRoom>(entt::entity abode)> room;
	/// A villager is about to leave its home: one inside it comes out and decides again what to do
	std::function<void(entt::entity villager)> leaving;
	/// Whether a villager is inside its home
	std::function<bool(entt::entity villager)> atHome;
	/// A villager is about to leave its town alive: the children of the town whose mother it is lose her
	std::function<void(entt::entity villager, entt::entity town)> leavingTown;
};

/// The people living in a building, by age, and the men among its adults
struct Household
{
	uint32_t adults {0};
	uint32_t adultMen {0};
	uint32_t children {0};
};

/// A town's people: by sex, all ages, its adults, and the places for adults in its built homes
struct TownPeople
{
	uint32_t men {0};
	uint32_t women {0};
	uint32_t adults {0};
	uint32_t adultPlaces {0};
};

/// What a building lacks, for evening out a town's homes: more men (against the town's share of men to women) and more
/// people (against how full the town's homes are, at half weight). Below nothing it has too many.
struct ShuffleWants
{
	float men {0.0f};
	float people {0.0f};
	/// It has a place left for an adult
	bool roomForAdult {false};
};

/// A move evening out two buildings of a town
struct ShuffleMove
{
	enum class Kind : uint8_t
	{
		/// A man of `from` moves to `to`, and a woman of `to` moves to `from`
		Swap,
		/// One of `from` of the sex wanted moves to `to`
		Take,
	};
	Kind kind {Kind::Swap};
	/// Into which building and out of which, by their place in the list given
	size_t to {0};
	size_t from {0};
	/// For a take, whether a man is wanted
	bool man {false};

	bool operator==(const ShuffleMove&) const = default;
};

/// What happened to a villager that looked for a better home
enum class NewHome : uint8_t
{
	/// Its home isn't too crowded, or it is a child
	Stays,
	/// It moved into a better one of its town
	Moved,
	/// It found none and was made homeless, or already was
	MadeHomeless,
	AlreadyHomeless,
	/// It has no town to look in
	NoTown,
};

/// What a building is like for one more villager
struct Occupancy
{
	/// Whether the newcomer is a child
	bool child {false};
	uint32_t adults {0};
	uint32_t children {0};
	uint32_t maxAdults {0};
	uint32_t maxChildren {0};
	/// Its people, and those of them of the newcomer's sex
	uint32_t people {0};
	uint32_t sameSex {0};
	/// From the building to the newcomer, in metres
	float distance {0.0f};
};

/// How well a building suits one more villager: by how much room it has left for the villager's age, then a building
/// mostly of the other sex and a near one are liked better. Nothing for a building that can't house its age at all.
[[nodiscard]] float ScoreForAddingVillager(const Occupancy& occupancy);

/// The functional building of a town that suits a villager best, scoring above a least score: the first of the best,
/// in the town's order
[[nodiscard]] entt::entity FindAbodeWithSpace(const Registry& registry, const Homes& homes, entt::entity town,
                                              entt::entity villager, float leastScore);

/// A villager joins a town: one without a home there moves into the building that suits it best, or is one of the
/// homeless. False for a town nobody can live in.
bool AddVillagerToTown(Registry& registry, const Homes& homes, entt::entity town, entt::entity villager);

/// A villager moves into a building, leaving the homeless or its old home, and joins the building's town
void AddVillagerToAbode(Registry& registry, const Homes& homes, entt::entity abode, entt::entity villager);

/// A villager leaves its home and keeps its town, joining the town's homeless at the head. False with no town, or when
/// it is already homeless there.
bool MakeHomeless(Registry& registry, const Homes& homes, entt::entity villager);

/// The town a building belongs to, none if it has gone
[[nodiscard]] entt::entity TownOfAbode(const Registry& registry, entt::entity abode);

/// Who lives in a building
[[nodiscard]] Household HouseholdOf(const Registry& registry, entt::entity abode);

/// The other of a building's couple: the first adult man and the first adult woman to move into it are its couple. None
/// for one who isn't of its couple, or when the other place is empty. A couple member who moves out alive keeps its place
/// in the old building's couple; the couple ends only when one of them dies.
[[nodiscard]] entt::entity SpouseOf(const Registry& registry, entt::entity abode, entt::entity villager);

/// Whether a building has so many adults that one of them would rather live elsewhere: at or past its share of its
/// places, or always for a building with no places for adults
[[nodiscard]] bool IsTooCrowded(uint32_t adults, uint32_t maxAdults, float tooCrowded);

/// The people of a town whose mother a villager is, in the town's order: those of each of its buildings in turn (the
/// newest building first, each building's people newest first), then its homeless
[[nodiscard]] std::vector<entt::entity> ChildrenOf(const Registry& registry, entt::entity town, entt::entity mother);

/// A villager leaves its town, alive: the town's children whose mother it is lose her, then it leaves its home or the
/// homeless, then the town itself
void RemoveVillagerFromTown(Registry& registry, const Homes& homes, entt::entity villager);

/// A villager has died: it leaves its home (ending the couple it was one of) and its town, with no one told
void RemoveDeadVillager(Registry& registry, entt::entity villager);

/// A villager moves into a building whatever its room. One coming from another town leaves that town first and, when
/// the building has no room for its age, joins the new town only, to find a home there or be homeless.
void MoveInAnyway(Registry& registry, const Homes& homes, entt::entity abode, entt::entity villager);

/// A villager moves into a building with room for its age. Whether it did.
bool MoveInIfRoom(Registry& registry, const Homes& homes, entt::entity abode, entt::entity villager);

/// Whether a town has room for another adult in its built homes; one with no places at all lets in up to a few people
[[nodiscard]] bool HasRoomForNewcomer(const Registry& registry, const Homes& homes, entt::entity town,
                                      uint32_t emptyTownAllows);

/// A villager moves into a town: into its best building, else (with room in the town) its village centre or newest
/// building, whatever their room. False when it found nowhere.
bool MoveIntoTown(Registry& registry, const Homes& homes, entt::entity town, entt::entity villager, uint32_t emptyTownAllows);

/// A villager is put down by a building: it moves in when there is room for its age, else it tries the building's town
bool MoveInByBuilding(Registry& registry, const Homes& homes, entt::entity abode, entt::entity villager,
                      uint32_t emptyTownAllows);

/// An adult whose home is too crowded, or who has none, looks for a building of its town that suits it better than its
/// own; finding none it is homeless
NewHome LookForBetterHome(Registry& registry, const Homes& homes, entt::entity villager);

/// What a building lacks, from its people and its town's
[[nodiscard]] ShuffleWants WantsOf(const TownPeople& town, const Household& household, uint32_t maxAdults);

/// The moves to try, in turn, to even out a town's homes, given in the town's order: the buildings most out of balance
/// first (in the order the game's sort leaves them), each paired with the later one that evens it out best, moving one
/// villager when only one of the pair wants people and it has room, else swapping a man for a woman
[[nodiscard]] std::vector<ShuffleMove> EvenOutMoves(std::span<const ShuffleWants> abodes);

/// Whether a town evens out its homes this turn: once in so many turns, each town a little after the one before it
[[nodiscard]] bool EvenOutDue(uint32_t townId, uint32_t turn, uint32_t every);

/// A town evens out its homes: the first of its moves that finds people out of doors to move is made. Whether one was.
bool EvenOutHomes(Registry& registry, const Homes& homes, entt::entity town);

} // namespace openblack::ecs::town_homes
