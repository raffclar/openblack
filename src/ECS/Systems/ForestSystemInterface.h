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
#include <span>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Systems/SavedStateInterface.h"
#include "Enums.h"
#include "Magic/ForestRules.h"

namespace openblack::ecs::components
{
struct Spell;
}

namespace openblack::ecs::systems
{

/// The land's forests and the forests the forest miracle plants.
///
/// Every game turn each forest, the newest first, grows its growing trees and now and then spreads, planting a young tree
/// beside one of its grown trees (see Nature/ForestGrowth.h); trees in no forest never grow. A forest miracle's forest
/// is a forest like any other, and besides, while the miracle lasts all its trees grow a little each turn; once it has
/// gone they wither away. A forest left with no trees goes some while later. But when a tree of a forest miracle goes
/// from the world and leaves its forest with no tree, the forest goes at once.
/// The trees of each forest, by the number of the forest, in the order that the trees are walked
using ForestTreeLists = std::unordered_map<uint32_t, std::vector<entt::entity>>;

class ForestSystemInterface: public SavedStateInterface
{
public:
	~ForestSystemInterface() override = default;

	/// The forest miracle plants its trees, their wood worth more by its caster's tribal power; how many it planted
	virtual uint32_t Plant(entt::entity spell, components::Spell& miracle, float tribalPower) = 0;
	/// Whether a forest may be planted at a map position: no building there whose fire would be in it, and room for a tree
	[[nodiscard]] virtual bool CanGrowAt(const map_coords::MapCoords& place) const = 0;
	/// Whether a forest miracle's forest still has trees
	[[nodiscard]] virtual bool HasTrees(entt::entity spell) const = 0;
	/// The forest of a forest miracle: none, on the land, or gone from the land since the miracle's last turn
	[[nodiscard]] virtual magic::forest::MiracleForest ForestOf(entt::entity spell) const = 0;
	/// A forest miracle lets go of its forest that went from the land
	virtual void LetGoOfGoneForest(entt::entity spell) = 0;
	/// A forest miracle of a saved game of the game takes its forest of the land again: the trees of its spell in that
	/// forest are its trees, which it grows and keeps from then on. How many trees it took.
	virtual uint32_t RestoreForest(entt::entity /*spell*/, components::Spell& /*miracle*/, entt::entity /*landForest*/)
	{
		return 0;
	}
	/// How many trees a forest miracle's forest has at this moment, a tree in the hand among them
	[[nodiscard]] virtual uint32_t StandingTrees(entt::entity spell) const = 0;
	/// How tall the tallest tree of a forest miracle's forest stands (none tall with no trees left); none before the
	/// miracle has planted it, and none after the miracle let go of its forest that went
	[[nodiscard]] virtual std::optional<float> TallestTreeOf(entt::entity spell) const = 0;
	/// A young tree of the same kind is planted near a tree of a forest, as the water miracle plants one by a full grown
	/// tree: no sooner than 41 turns after the last tree any forest gained this way (none in a land's first 41 turns), at
	/// the first free spot of up to 160 tried round it. One planted by a forest miracle's tree joins that miracle's
	/// forest and goes with it. The tree planted, none when it is too soon, the tree is in no forest, or no spot is free.
	virtual std::optional<entt::entity> AddTreeNear(entt::entity tree) = 0;
	/// A forest of the land is made about a point: with the land's script's number for it, or else the next number
	/// (a number given moves the count past it); a big forest's own, or a town's forest of the lone trees about it. The
	/// newest forest is met first. Its number.
	virtual uint32_t MakeLandForest(std::optional<uint32_t> id, glm::vec3 position, entt::entity bigForest, bool scenic) = 0;
	/// The first forest met with a number, none when there is none
	[[nodiscard]] virtual std::optional<entt::entity> LandForestOf(uint32_t id) const = 0;
	/// The land's forests, the newest first
	[[nodiscard]] virtual std::vector<entt::entity> LandForests() const = 0;
	/// A forest's grown or still growing trees, the nearest its place first (the one listed first when as near)
	[[nodiscard]] virtual std::vector<entt::entity> TreesOf(entt::entity forest, bool growing) const = 0;
	/// How many trees a forest has, grown and growing: the sizes of both lists of TreesOf, from one walk with no lists
	[[nodiscard]] virtual size_t TreeCountOf(entt::entity forest) const = 0;
	/// The trees of the forests, in one walk of the trees. A caller that asks about many forests makes the lists one
	/// time. The lists are correct until a tree changes.
	[[nodiscard]] virtual ForestTreeLists ListTreesOfForests(std::span<const entt::entity> forests) const = 0;
	/// The same trees as TreesOf, from lists that ListTreesOfForests made after the last change of a tree. For a forest
	/// that is not in the lists, the search walks all the trees.
	[[nodiscard]] virtual std::vector<entt::entity> ListedTreesOf(entt::entity forest, bool growing,
	                                                              const ForestTreeLists& lists) const = 0;
	/// The wood in a forest: its big forest's and every tree's
	[[nodiscard]] virtual float WoodOf(entt::entity forest) const = 0;
	/// The wood of a tree as a forest counts it: its life, the worth of a forest miracle's tree, the wood value of its
	/// kind, its scale and the land's balance for wood, not truncated. 0 for a thing that is not a tree.
	[[nodiscard]] virtual float TreeWoodOf(entt::entity tree) const = 0;
	/// The point of a forest nearest a point: on its big forest's edge towards it, or else the forest's place
	[[nodiscard]] virtual glm::vec3 NearestPointOf(entt::entity forest, glm::vec3 to) const = 0;
	/// Wood taken from a big forest: it comes off divided by the forest's life; a forest left with none goes from the
	/// land (its forest stays), and one whose wood has drifted far enough from its size shrinks to fit and plants a young
	/// pine round its new edge. How much was taken.
	virtual uint32_t TakeBigForestWood(entt::entity bigForest, uint32_t amount) = 0;
	/// A tree taken from a big forest: a full grown conifer's wood taken from it, then the conifer made at its place, one
	/// of its forest's trees. The tree, none when there is no such big forest.
	virtual std::optional<entt::entity> TreeFromBigForest(entt::entity bigForest) = 0;
	/// Each town lists the forests near enough its storage pit with wood in them
	virtual void AssignForestsToTowns() = 0;
	/// One town lists the forests near enough its storage pit with wood in them, in place of the forests it listed before
	virtual void AssignForestsToTown(entt::entity town) = 0;
	/// The forest of its own a town sends a villager at a point to: the nearest of the forests it lists, by the edge of
	/// its big forest (none when the point is inside it) or the forest's place, within the town's reach for forests; an
	/// ordinary forest before the town's forest of lone trees. None when it lists none near enough.
	[[nodiscard]] virtual std::optional<entt::entity> NearestTownForest(entt::entity town, glm::vec3 point) const = 0;
	/// The nearest of the land's forests with trees whose place is nearer a point than a reach, the newest of equals
	[[nodiscard]] virtual std::optional<entt::entity> NearestForestWithTrees(glm::vec3 point, float reach) const = 0;
	/// The nearest of the land's forests with no trees and no big forest nearer a point than a reach goes at once.
	/// Whether one went.
	virtual bool RemoveEmptyForestNear(glm::vec3 point, float reach) = 0;
	/// The tree in the middle of a forest: the nearest its place of its grown and growing trees; none without trees
	[[nodiscard]] virtual std::optional<entt::entity> CentreTreeOf(entt::entity forest) const = 0;
	/// The same tree as CentreTreeOf, from lists that ListTreesOfForests made after the last change of a tree. For a
	/// forest that is not in the lists, the search walks all the trees.
	[[nodiscard]] virtual std::optional<entt::entity> ListedCentreTreeOf(entt::entity forest,
	                                                                     const ForestTreeLists& lists) const = 0;
	/// The map position where a tiger's or a wolf's flock makes its home. The animal chooses among the forests from its own
	/// map position. None keeps the home.
	[[nodiscard]] virtual std::optional<map_coords::MapCoords> ForestLair(AnimalInfo kind,
	                                                                      const map_coords::MapCoords& from) const = 0;

	/// Once the land is laid out: each town gathers the lone trees about it, out to a little beyond the reach of its
	/// forests, into a forest of its own (taking a tree from another town's such forest when it stands nearer this town)
	virtual void MakeScenicForests() = 0;

	/// A tree joins a forest by its number: among its growing trees when made to grow and still short of its largest
	/// size, or else among its grown trees
	virtual void JoinForest(entt::entity tree, uint32_t forest) = 0;
	/// A tree joins a forest by its number, among its growing trees or among its grown trees as asked. It comes after
	/// the trees that joined before it. The import of a game's file uses this.
	virtual void ListInForest(entt::entity tree, uint32_t forest, bool growing) = 0;
	/// The import of a game's file gives the turn that a forest last got a tree near another tree, and the number of
	/// the next forest without a number
	virtual void ResumeCounts(uint32_t lastTreeAddedTurn, uint32_t nextLandForestId) = 0;
	/// Once a game turn, after the fields and before the living: each forest, the newest first, grows its growing trees
	/// and may spread, and one long empty goes
	virtual void GrowForests() = 0;
	/// Once a game turn, after the miracles: the forest miracles' forests grow or wither
	virtual void ProcessTurn() = 0;
	/// A new land: no forests
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
