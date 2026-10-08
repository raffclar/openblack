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
#include <vector>

#include <entt/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs
{

/// Once per game turn: a tree that belongs to a forest and has not reached its maximum size grows every growTurns
/// turns. Only forests process their trees in the original, so the script's forest-less trees (every tree of Land 1 and
/// Land 2) never grow.
void ProcessTreesTurn(uint32_t turn);

/// How much a tree grows on its grow turn: growthAmount x (1 + 0.01 x rainMultiplier x rain) x (1 + 0.5 x
/// landAlignment), rain (the larger of raining and snowing) and alignment both taken at the tree. Rain is the 0..127
/// byte and the land alignment is -1..1, so the worst land halves the growth and the best adds half.
[[nodiscard]] float TreeGrowthAmount(float growthAmount, float rainMultiplier, float rain, float landAlignment);

/// Per frame: the brightness of every tree this frame and the leaf rustle of the tall ones next to the camera.
void UpdateTrees(float seconds);

/// Every RGB channel of a tree's colour is multiplied by this / 256: 200 (looking against the light) to 255. Only
/// trees use it.
[[nodiscard]] uint8_t TreeBrightness();

/// Grows by `amount` up to maxSize (raising maxSize first when `raiseMax`), and re-registers the obstacle circle for
/// the new scale. Returns how much it actually grew.
float GrowTree(entt::entity tree, float amount, bool raiseMax);

/// A forest at `centre` with that id (0 takes the next free id, a given id raises the counter past it; CREATE_FOREST
/// and a fallen tree's new forest). Returns its id.
uint32_t CreateForest(uint32_t id, glm::vec3 centre);

/// Whether a forest with that id exists (CREATE_TREE / CREATE_NEW_TREE look the script's id up in the forest list and
/// pass no forest when there is none).
[[nodiscard]] bool IsInForest(uint32_t forestId);

/// The id to store in a tree for the script's forest id: the id itself if that forest exists, else 0 (no forest).
[[nodiscard]] uint32_t ResolveForestId(int32_t scriptForestId);

/// The forest a town's replanted trees join (the original keeps a list of forests per town, and a fallen tree joins
/// the last of them). Creates one at `at` the first time the town needs it.
uint32_t TownForestId(uint32_t townId, glm::vec3 at);

/// The nearest forest whose centre is within `radius` of `at` (for the forest miracle), if any.
[[nodiscard]] std::optional<uint32_t> NearestForest(glm::vec3 at, float radius);

/// Moves a tree into a forest (out of the old one, into the new one; 0 = none).
void SetTreeForest(entt::entity tree, uint32_t forestId);

/// Plants a sapling of `parent`'s type next to it: 32 angles (from a random one, 2pi/32 apart) x 5 radii
/// (a random whole 5-9 m, then (r + 2) mod 10), the first free spot on land; the new tree is size 0.1, grows to
/// 0.8 + rand(0.4), at a random angle, in the forest. Returns the tree or entt::null when nothing fits.
entt::entity PlantTreeNear(uint32_t forestId, entt::entity parent);

/// The tree's side of the water miracle: a growing tree grows by waterMultiplier x growAmount; with `raiseMaximum`
/// (the spell subtype 0x17) a full grown one also grows, by half that times GetDistanceModifier(size, 3), past its
/// maximum. A full grown tree of a forest watered without it, more than 40 turns after the last new tree of the world,
/// plants a sapling next to itself (returned; the caller gives the player the good alignment and the 0xE statistic).
entt::entity ApplyWaterSpell(entt::entity tree, bool raiseMaximum);

/// Every tree of the forest grows by `amount` (GrowTree with no raise of the maximum); returns the sum of what they
/// grew.
float GrowAllTrees(uint32_t forestId, float amount);

/// Every tree of the forest shrinks by `amount` (a tree that would reach 0 is deleted and adds nothing); returns the
/// sum of what they shrank.
float ShrinkAllTrees(uint32_t forestId, float amount);

/// The height (mesh height x scale) of the forest's tallest tree, 0 if it has none.
[[nodiscard]] float TallestTreeHeight(uint32_t forestId);

/// The forests in the order of the original's list: the newest first (the tiger and wolf lair searches walk it)
[[nodiscard]] std::vector<uint32_t> ForestsNewestFirst();

/// Its centre (the position it was created at)
[[nodiscard]] glm::vec3 ForestCentre(uint32_t forestId);

/// How many trees it has, grown and growing
[[nodiscard]] size_t ForestTreeCount(uint32_t forestId);

/// Its full grown trees, nearest its centre first (the list is kept sorted by distance to the centre; equal distances
/// keep the order they joined in)
[[nodiscard]] std::vector<entt::entity> GrownTreesByDistance(uint32_t forestId);

/// How a tree goes: Removed, the entity is destroyed (DeleteTree); BecameDeadTree, the tree is deleted but the entity
/// stays as the dead tree that took over its 3D object, fire included (FellTree, or a tree that falls and lands). The
/// dead tree takes the fire over, so a listener keeps it for BecameDeadTree.
enum class TreeDeletion
{
	Removed,
	BecameDeadTree,
};

/// Called with each tree (or dead tree) just before it goes: fire, reactions, the hand and the like clean up after
/// it.
using TreeDeletedListener = std::function<void(entt::entity, TreeDeletion)>;

/// Tells the listeners (for code that turns a Tree into a DeadTree itself, like the hand's MakeDeadTree)
void NotifyTreeDeleted(entt::entity tree, TreeDeletion how);
void AddTreeDeletedListener(TreeDeletedListener listener);

/// A tree or dead tree: out of its forest, out of the physics, the listeners told, and gone.
void DeleteTree(entt::entity tree);

/// Every tree of both its lists deleted (DeleteTree), then the forest itself leaves the forest list.
void DeleteForest(uint32_t forestId);

/// A tree: life x wood value multiplier (1) x woodValue x scale x the land balance's wood value; a dead tree: life x
/// woodValue x scale^3 (the original cubes the scale there).
[[nodiscard]] float TreeWoodValue(entt::entity tree);

/// The default WOOD resource: a tree, (int)TreeWoodValue; a dead tree, (int)(woodValue x its wood multiplier (1, copied
/// from the tree) x scale): no life, no land balance. What a store gets for it.
[[nodiscard]] uint32_t TreeWood(entt::entity tree);

/// n wood taken from a dead tree; with no more than n left it is deleted and gives what it had, otherwise it SHRINKS:
/// its scale becomes (wood - n) / (woodValue x multiplier). Returns what was taken.
uint32_t RemoveWood(entt::entity deadTree, uint32_t amount);

/// The log a villager carries it as: a tree, its info's carriedType; a dead tree, 0-3 when its mesh is one of the four
/// carried logs (MeshPack 406 / 347 / 348 / 349), else the tree info's carriedType; a big forest, its info's
/// carriedType.
[[nodiscard]] CarriedTreeType TreeCarriedType(entt::entity tree);

/// Reaction 12 (REACT_TO_WOOD) for a new dead tree made from a standing tree, with that tree's player: every such dead
/// tree tells the villagers near it "wood here". Only that path makes it: a dropped log and the scripts' dead trees get
/// none. Called by FellTree; (pending: a tree that falls and lands should call it too, with the player that threw it).
/// Returns the reaction id
uint32_t CreateDeadTreeReaction(entt::entity deadTree, PlayerNames player);
/// A dead tree that lands: the rock landing, then reaction 12 with the player that threw it (the hand's player, else
/// none): a log that lands tells the villagers again. Not for a felled tree (its landing only fixes it in place).
/// (pending: the dead tree physics landing handler and the hand's landing path) no caller yet; `byPlayer` is
/// PhysicsObject::byPlayer
uint32_t DeadTreeEndPhysicsReaction(entt::entity deadTree, bool byPlayer);

/// Called only by a forester chopping the tree: the tree becomes a felled
/// DeadTree with its mesh, thrown into the physics by the forester: velocity 0.2 x its height along the direction from
/// the forester to the tree, spin 0.4 rad/s about the tree's own axis (cos a, 0, sin a) (body space: the fall depends on
/// its yaw),
/// adjusted to the ground. Returns the felled tree (the same entity) or entt::null.
entt::entity FellTree(entt::entity tree, entt::entity chopper);

/// The log a loaded villager drops: a dead Pine of no player, life 1.0, tilted pi/2 about x, with `mesh` as its mesh
/// and `woodMultiplier`. Not put into physics here (the caller does). Trees have no owner.
entt::entity CreateDroppedLog(glm::vec3 position, uint32_t mesh, float woodMultiplier);

/// The object's 2D radius (scale x the larger of the mesh's half extents in x and z): ecs::object::Get2DRadius, with
/// the class overrides
[[nodiscard]] float Object2DRadius(entt::entity object);

/// Where `who` stands to work on the tree: the tree's position plus, towards `who`, who's 2D radius + 0.9
[[nodiscard]] glm::vec3 TreeWorkingPos(entt::entity tree, entt::entity who);

/// The tree a villager finds near it: the 9 cells around `who` in spiral order ((0,0) (-1,0) (-1,-1) (0,-1) (1,-1)
/// (1,0) (1,1) (0,1) (-1,1)), in each only the FIRST tree of the cell that is not INDESTRUCTIBLE (set only by puzzle
/// objects: no tree has it in a normal game); the nearest by Dist2D(who, its working position), from 99999. No other
/// rule (no distance limit, no scenic bit, no size, no forest). entt::null when none; the caller tells "touching" (10)
/// from "found" (1) with IsTouching.
[[nodiscard]] entt::entity FindTreeNearVillager(entt::entity who);

/// The BigForest it belongs to (entt::null if none) and whether it is its town's scenic forest
[[nodiscard]] entt::entity ForestBigForest(uint32_t forestId);
void SetForestBigForest(uint32_t forestId, entt::entity bigForest);
[[nodiscard]] bool IsScenicForest(uint32_t forestId);

/// The forest's wood: its BigForest's wood value (life x wood) plus every tree's TreeWoodValue
[[nodiscard]] float ForestWood(uint32_t forestId);

/// Of the heads of its two lists (grown and growing, each sorted by distance to the centre) the one nearer the centre;
/// the grown one on a tie. entt::null when it has none.
[[nodiscard]] entt::entity ForestCentreTree(uint32_t forestId);

/// Over the forest list, the forest whose CENTRE is nearest `at` within `max` (max
/// shrinks to the best so far); onlyEmpty: no trees and no BigForest; otherwise at least one tree (BigForest and scenic
/// not looked at)
[[nodiscard]] std::optional<uint32_t> FindForest(glm::vec3 at, float max, bool onlyEmpty);

/// The town's scenic forest (made at `townCentre` if the town has
/// none) takes every tree within 250 + 10 m of the town centre (a spiral over the cells that stops at the first cell
/// farther than that) that has no forest, or whose forest is scenic and whose tree is nearer the town centre than that
/// forest's centre. It does not add the forest to the town list (AssignForestsToTown does).
void MakeScenicForest(uint32_t townId, glm::vec3 townCentre);

/// The town's forest list cleared and filled again with every forest whose nearest point (a BigForest's nearest edge,
/// otherwise its centre) is within the town info's maxDistanceForTownForest (250) of `reference` (the town's storage
/// pit, or its temporary store point) and that has wood (ForestWood > 0). Called when the town features are assigned
/// (after MakeScenicForest, for every town) and when a scaffold finishes a building; not when trees are made or
/// planted.
void AssignForestsToTown(uint32_t townId, glm::vec3 reference);

/// The town's forests (head first: new ones are inserted at the head)
[[nodiscard]] std::vector<uint32_t> TownForests(uint32_t townId);

/// Over the town's list, the forest nearest `at` (a BigForest's nearest edge, 0 when `at` is inside its 2D radius,
/// else its centre) within 250; a non-scenic one wins, the scenic forest only when there is no other. No wood check
/// here.
[[nodiscard]] std::optional<uint32_t> FindNearestForestToPos(uint32_t townId, glm::vec3 at);

/// Its position plus, towards `who`, 0.5 x its 2D radius
[[nodiscard]] glm::vec3 BigForestArrivePos(entt::entity bigForest, entt::entity who);

/// WOOD only: n / life wood taken; when it has no more than that, it gives what it had (its wood value, truncated) and
/// is deleted; otherwise its wood goes down, and once its wood value is more than 250 away from life x scale x
/// woodValue it is rescaled to wood value / (life x woodValue) and a sapling is planted at its edge (up to 10 random
/// angles at its radius, on land with no object nearer than 4; a Pine of its forest, size 0.05, maximum
/// 0.75 + rand(0.5), random angle).
uint32_t BigForestRemoveWood(entt::entity bigForest, uint32_t amount);

/// On map load: the forests go with the map.
void ClearForests();

} // namespace openblack::ecs
