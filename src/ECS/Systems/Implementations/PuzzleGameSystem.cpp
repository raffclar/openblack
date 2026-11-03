/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PuzzleGameSystem.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/MistArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PuzzleGameArchetype.h"
#include "ECS/Archetypes/PuzzleTotemArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/NoLandShadow.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/FishPuzzleRules.h"
#include "ECS/HanoiRules.h"
#include "ECS/Map.h"
#include "ECS/MazePuzzleRules.h"
#include "ECS/PuzzleGameRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/PuzzleTotemSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/TotemPuzzleRules.h"
#include "ECS/TreePuzzleRules.h"
#include "ECS/WorldObjects.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/HelpSpeech.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using openblack::ecs::Registry;
namespace archetypes = openblack::ecs::archetypes;
namespace world_objects = openblack::ecs::world_objects;
namespace hanoi = openblack::ecs::hanoi;
namespace tree_puzzle = openblack::ecs::tree_puzzle;
namespace maze_puzzle = openblack::ecs::maze_puzzle;
namespace totem_puzzle = openblack::ecs::totem_puzzle;

namespace
{
/// A piece that the puzzle makes again makes one of these sounds of the editor's bank, heard everywhere
constexpr std::string_view k_PieceBank = "editor.sad";
constexpr uint32_t k_PieceSound = 413;
constexpr uint32_t k_PieceSounds = 9;
/// The bank of the sounds of the scripts, and the bank of the game's own sounds
constexpr std::string_view k_ScriptBank = "Scriptsfx.sad";
constexpr std::string_view k_GameBank = "InGame.sad";
/// The hand that takes the largest ring says one of these lines
constexpr uint32_t k_LargestRingLine = 5267;
constexpr uint32_t k_LargestRingLines = 8;
/// The slots of the pieces in the kept states: the bases first, then the rings
constexpr size_t k_FirstRingSlot = hanoi::k_Bases;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

uint32_t LocalRandom(uint32_t count)
{
	if (count == 0 || !Locator::gameRandom::has_value())
	{
		return 0;
	}
	return Locator::gameRandom::value().LocalRand(static_cast<int32_t>(count)) % count;
}

MobileObjectInfo KindOf(const Registry& registry, entt::entity piece)
{
	const auto* mobile = registry.TryGet<const MobileObject>(piece);
	return mobile != nullptr ? mobile->type : MobileObjectInfo::None;
}

HanoiBlock* BlockOf(Registry& registry, entt::entity piece)
{
	return piece != entt::null && registry.Valid(piece) ? registry.TryGet<HanoiBlock>(piece) : nullptr;
}

entt::entity& PieceAt(PuzzleGame& game, size_t slot)
{
	return slot < k_FirstRingSlot ? game.bases.at(slot) : game.rings.at(slot - k_FirstRingSlot);
}

MobileObjectInfo KindOfSlot(size_t slot)
{
	return slot < k_FirstRingSlot ? hanoi::k_BaseKind : hanoi::k_RingKinds.at(slot - k_FirstRingSlot);
}

/// A piece of the puzzle on its place: drawn there, with its links, held by the scripts' rules for pieces
entt::entity MakePiece(entt::entity puzzle, MobileObjectInfo kind, const map_coords::MapCoords& home)
{
	auto& registry = Entities();
	const auto world = map_coords::ToWorld(Locator::terrainSystem::value(), home);
	const auto piece = archetypes::MobileObjectArchetype::Create(world, kind, 0.0f, 1.0f);
	ecs::thing_map_place::Keep(registry, piece, home);
	registry.Assign<HanoiBlock>(piece, HanoiBlock {.puzzle = puzzle, .home = home, .placedAt = world});
	// No piece can be destroyed, and a base never moves
	registry.Assign<Indestructible>(piece);
	if (kind == hanoi::k_BaseKind)
	{
		registry.Assign<Immovable>(piece);
	}
	if (Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().Refile(piece);
	}
	return piece;
}

/// The puzzle makes a piece again at its place from the piece's last state, with the same links, and a sound plays
void MakeAgain(entt::entity puzzle, PuzzleGame& game, size_t slot, const HanoiBlock& last)
{
	auto& registry = Entities();
	const auto piece = MakePiece(puzzle, KindOfSlot(slot), last.home);
	auto& block = registry.Get<HanoiBlock>(piece);
	block.below = last.below;
	block.above = last.above;
	block.fixed = last.fixed;
	if (auto* above = BlockOf(registry, block.above))
	{
		above->below = piece;
	}
	if (auto* below = BlockOf(registry, block.below))
	{
		below->above = piece;
	}
	PieceAt(game, slot) = piece;
	game.kept.at(slot) = block;
	registry.Assign<ScriptControlled>(piece);
	if (Locator::sounds::has_value())
	{
		const auto sample = k_PieceSound + LocalRandom(k_PieceSounds);
		audio::StartGameSoundEffect(entt::hashed_string(fmt::format("{}/{}", k_PieceBank, sample).c_str()).value(), {});
	}
}

/// A piece that is still there takes the place of its slot again from its own state
void ReplaceAndMakeAgain(entt::entity puzzle, PuzzleGame& game, size_t slot)
{
	auto& registry = Entities();
	auto& piece = PieceAt(game, slot);
	const auto last = registry.Get<const HanoiBlock>(piece);
	world_objects::LeaveGhost(piece);
	world_objects::Remove(piece);
	MakeAgain(puzzle, game, slot, last);
}

/// Whether the piece is where the puzzle put it. A ring in a creature's hand is where the creature took it until the
/// next turn of the creature, and then where the creature is.
bool AtPlace(const Registry& registry, entt::entity piece)
{
	const auto& block = registry.Get<const HanoiBlock>(piece);
	const auto* transform = registry.TryGet<const Transform>(piece);
	return transform != nullptr && transform->position == block.placedAt;
}

/// The top piece of the first stack that the search meets within reach of a place, in a spiral of map cells out from
/// the centre. A stack with the ring in it does not count.
entt::entity FindStack(const Registry& registry, map_coords::MapCoords centre, const map_coords::MapCoords& near,
                       entt::entity ring)
{
	if (!Locator::entitiesMap::has_value())
	{
		return entt::null;
	}
	const auto& map = Locator::entitiesMap::value();
	map_coords::Spiral spiral;
	for (int32_t cell = 0; cell < hanoi::k_SettleCells; ++cell)
	{
		for (const auto thing : map.GetAllInCell(map_coords::Cell(centre)))
		{
			if (gutils::GetDistanceInMetres(ecs::thing_map_place::Of(registry, thing), near) > hanoi::k_SettleReach ||
			    !registry.Valid(thing) || !registry.AllOf<HanoiBlock>(thing) || thing == ring)
			{
				continue;
			}
			auto top = thing;
			bool holdsRing = false;
			for (auto next = registry.Get<const HanoiBlock>(thing).above; next != entt::null;)
			{
				if (next == ring)
				{
					holdsRing = true;
					break;
				}
				top = next;
				const auto* block = registry.Valid(next) ? registry.TryGet<const HanoiBlock>(next) : nullptr;
				next = block != nullptr ? block->above : entt::null;
			}
			if (!holdsRing)
			{
				return top;
			}
		}
		map_coords::AddCells(centre, spiral.Next());
	}
	return entt::null;
}

/// The links of the puzzle's pieces for the test of a solution
hanoi::Stacks<entt::entity> StacksOf(const Registry& registry, const PuzzleGame& game)
{
	hanoi::Stacks<entt::entity> stacks {.bases = game.bases, .rings = game.rings};
	for (size_t ring = 0; ring < hanoi::k_Rings; ++ring)
	{
		const auto piece = game.rings.at(ring);
		const auto* block = piece != entt::null && registry.Valid(piece) ? registry.TryGet<const HanoiBlock>(piece) : nullptr;
		// A ring that is not there rests on nothing that the test knows
		stacks.below.at(ring) = block != nullptr ? block->below : entt::tombstone;
	}
	return stacks;
}

/// Whether the hand may take each piece, from the puzzle's state now
void UpdatePickUp(entt::entity puzzle, PuzzleGame& game)
{
	auto& registry = Entities();
	for (size_t slot = 0; slot < game.kept.size(); ++slot)
	{
		const auto piece = PieceAt(game, slot);
		const auto* block = BlockOf(registry, piece);
		if (block == nullptr)
		{
			continue;
		}
		game.kept.at(slot) = *block;
		const bool refused = hanoi::CannotBePickedUp({
		    .base = slot < k_FirstRingSlot,
		    .covered = block->above != entt::null,
		    .hasPuzzle = block->puzzle == puzzle,
		    .calm = game.calmTurns,
		    .fixed = block->fixed,
		});
		if (refused)
		{
			registry.AssignOrReplace<CannotBePickedUp>(piece);
		}
		else
		{
			registry.Remove<CannotBePickedUp>(piece);
		}
	}
}

/// The beach temple puzzle at its start: three bases in a row along the puzzle's angle, and the four rings stacked on
/// the first base, the largest at the bottom
void StartHanoi(entt::entity puzzle, PuzzleGame& game)
{
	auto& registry = Entities();
	const auto spaced = hanoi::Spaced({.cosine = game.cosineStep, .sine = game.sineStep});
	game.cosineStep = spaced.cosine;
	game.sineStep = spaced.sine;
	for (size_t base = 0; base < hanoi::k_Bases; ++base)
	{
		game.bases.at(base) =
		    MakePiece(puzzle, hanoi::k_BaseKind, hanoi::BasePlace(game.place, spaced, static_cast<int32_t>(base)));
	}
	const auto places = hanoi::StartRingPlaces(hanoi::BasePlace(game.place, spaced, 0));
	auto below = game.bases.front();
	for (size_t ring = 0; ring < hanoi::k_Rings; ++ring)
	{
		const auto piece = MakePiece(puzzle, hanoi::k_RingKinds.at(ring), places.at(ring));
		game.rings.at(ring) = piece;
		if (!hanoi::StartRingCastsLandShadow(ring))
		{
			registry.Assign<NoLandShadow>(piece);
		}
		registry.Get<HanoiBlock>(piece).below = below;
		registry.Get<HanoiBlock>(below).above = piece;
		below = piece;
	}
}

/// The steps of the puzzle's grid
hanoi::Steps GridSteps(const PuzzleGame& game)
{
	return {.cosine = game.cosineStep, .sine = game.sineStep};
}

/// A new thing goes into the map's cells at once, as the game puts each new thing in the map when it makes it
void FileInMap(entt::entity thing)
{
	if (thing != entt::null && Entities().Valid(thing) && Locator::entitiesMap::has_value())
	{
		Locator::entitiesMap::value().Refile(thing);
	}
}

/// A new small tree on a spot, of the kind that the state of the spot shows. The scripts hold it.
void MakeTree(PuzzleGame& game, int32_t across, int32_t along)
{
	auto& registry = Entities();
	const auto spot = tree_puzzle::SpotIndex(across, along);
	const auto place = hanoi::GridPlace(game.place, GridSteps(game), across, along);
	const auto world = map_coords::ToWorld(Locator::terrainSystem::value(), place);
	const auto count = game.treeCounts.at(spot);
	const auto tree = archetypes::TreeArchetype::Create(0, world, tree_puzzle::TreeKindOf(game.type, count), false, 0.0f,
	                                                    tree_puzzle::FullSizeOf(game.type, count), tree_puzzle::k_StartSize);
	ecs::thing_map_place::Keep(registry, tree, place);
	FileInMap(tree);
	registry.Assign<ScriptControlled>(tree);
	game.trees.at(spot) = tree;
	game.treesPlacedAt.at(spot) = world;
}

/// A spot on the grid changes its state. Its old tree goes and leaves a ghost, and a new small tree grows on the spot.
void ChangeSpot(PuzzleGame& game, int32_t across, int32_t along)
{
	if (!tree_puzzle::OnGrid(across, along))
	{
		return;
	}
	const auto spot = tree_puzzle::SpotIndex(across, along);
	game.treeCounts.at(spot) = tree_puzzle::Changed(game.treeCounts.at(spot));
	if (const auto old = game.trees.at(spot); old != entt::null && Entities().Valid(old))
	{
		world_objects::LeaveGhost(old);
		world_objects::Remove(old);
	}
	MakeTree(game, across, along);
}

/// A pull of the tree on a spot changes the spot and the four spots next to it
void Pull(PuzzleGame& game, int32_t across, int32_t along)
{
	for (const auto& [changedAcross, changedAlong] : tree_puzzle::PulledSpots(across, along))
	{
		ChangeSpot(game, changedAcross, changedAlong);
	}
}

/// A tree of the puzzle grows one step towards its full size. The map's cells take it out and put it in again.
void GrowTree(entt::entity tree)
{
	auto& registry = Entities();
	auto& transform = registry.Get<Transform>(tree);
	const auto fullSize = registry.Get<const Tree>(tree).maxSize;
	const float size = transform.scale.y;
	if (!(size < fullSize))
	{
		return;
	}
	const float grown = tree_puzzle::Grown(size, fullSize);
	const float ratio = grown / std::max(size, 1e-4f);
	transform.scale = glm::vec3(grown);
	if (auto* fixed = registry.TryGet<Fixed>(tree))
	{
		fixed->boundingRadius *= ratio;
	}
	registry.SetDirty();
	ecs::thing_map_place::Reinsert(tree);
}

/// The tree puzzle at its start:
/// - a small tree on each spot of the grid
/// - seven pulls of random spots, which mix the states
/// - a lantern one step out from each corner
/// - a puff of mist over each spot, and a flock of nine bats over the middle spot.
void StartTrees(PuzzleGame& game)
{
	auto& registry = Entities();
	for (int32_t across = 0; across < tree_puzzle::k_Side; ++across)
	{
		for (int32_t along = 0; along < tree_puzzle::k_Side; ++along)
		{
			MakeTree(game, across, along);
		}
	}
	auto& random = Locator::gameRandom::value();
	for (int32_t move = 0; move < tree_puzzle::k_StartMoves; ++move)
	{
		// The first random number gives the spot along, the second gives the spot across
		const auto along = static_cast<int32_t>(random.GameRand(tree_puzzle::k_Side));
		const auto across = static_cast<int32_t>(random.GameRand(tree_puzzle::k_Side));
		Pull(game, across, along);
	}
	const auto steps = GridSteps(game);
	const auto& terrain = Locator::terrainSystem::value();
	for (size_t corner = 0; corner < tree_puzzle::k_LanternSpots.size(); ++corner)
	{
		const auto& [across, along] = tree_puzzle::k_LanternSpots.at(corner);
		const auto place = hanoi::GridPlace(game.place, steps, across, along);
		const auto lantern =
		    archetypes::StreetLanternArchetype::Create(map_coords::ToWorld(terrain, place), MobileStaticInfo::StreetLantern);
		ecs::thing_map_place::Keep(registry, lantern, place);
		FileInMap(lantern);
		game.lanterns.at(corner) = lantern;
	}
	const auto middle = map_coords::ToMetres(
	    hanoi::GridPlace(game.place, steps, tree_puzzle::k_MiddleSpot.front(), tree_puzzle::k_MiddleSpot.back()));
	auto* animals = Locator::animalSystem::has_value() ? &Locator::animalSystem::value() : nullptr;
	const auto flock = animals != nullptr ? animals->CreateScriptFlock(tree_puzzle::k_FlockNumber, middle, middle,
	                                                                   tree_puzzle::k_FlockReach, tree_puzzle::k_FlockDistance)
	                                      : entt::null;
	for (int32_t across = 0; across < tree_puzzle::k_Side; ++across)
	{
		for (int32_t along = 0; along < tree_puzzle::k_Side; ++along)
		{
			const auto spot = tree_puzzle::SpotIndex(across, along);
			const auto place = hanoi::GridPlace(game.place, steps, across, along);
			game.mists.at(spot) = archetypes::MistArchetype::Create(map_coords::ToWorld(terrain, place), place.altitude,
			                                                        tree_puzzle::k_MistColour, tree_puzzle::k_MistSize,
			                                                        tree_puzzle::k_MistEdgeShrink);
			if (animals == nullptr)
			{
				continue;
			}
			const auto bat = animals->CreateBird(tree_puzzle::k_BatKind, middle, tree_puzzle::k_BatAge, flock);
			if (bat != entt::null)
			{
				animals->SetScriptState(bat, static_cast<LivingStates>(tree_puzzle::k_BatStartState));
			}
			game.bats.at(spot) = bat;
		}
	}
}

/// Whether the tree puzzle shows a solution: a tree on each spot at its full size, and all spots in the same state
bool TreesSolved(const Registry& registry, const PuzzleGame& game)
{
	for (const auto tree : game.trees)
	{
		if (tree == entt::null || !registry.Valid(tree))
		{
			return false;
		}
		if (registry.Get<const Transform>(tree).scale.y < registry.Get<const Tree>(tree).maxSize)
		{
			return false;
		}
	}
	return tree_puzzle::SameState(game.type, game.treeCounts);
}

/// The player solved the tree puzzle. The trees lose half their life, catch fire and stop being part of the puzzle. The
/// mist goes, and the bats start to die.
void SolveTrees(PuzzleGame& game)
{
	auto& registry = Entities();
	for (size_t spot = 0; spot < tree_puzzle::k_Spots; ++spot)
	{
		const auto tree = std::exchange(game.trees.at(spot), entt::null);
		world_objects::SetLife(tree, tree_puzzle::k_SolvedTreeLife);
		if (Locator::fireSystem::has_value())
		{
			Locator::fireSystem::value().SetOnFire(tree, tree_puzzle::k_SolvedTreeFireSpeed);
		}
		if (const auto mist = std::exchange(game.mists.at(spot), entt::null); mist != entt::null && registry.Valid(mist))
		{
			world_objects::Remove(mist);
		}
		const auto bat = std::exchange(game.bats.at(spot), entt::null);
		if (bat != entt::null && registry.Valid(bat) && Locator::animalSystem::has_value())
		{
			Locator::animalSystem::value().SetScriptState(bat, static_cast<LivingStates>(tree_puzzle::k_BatSolvedState));
		}
	}
	game.played = true;
}

/// One turn of the tree puzzle, spot by spot. A tree on its spot grows. A tree that went or moved off its spot counts
/// as a pull of that spot, which changes the spot and the spots next to it.
void ProcessTrees(PuzzleGame& game)
{
	auto& registry = Entities();
	for (int32_t across = 0; across < tree_puzzle::k_Side; ++across)
	{
		for (int32_t along = 0; along < tree_puzzle::k_Side; ++along)
		{
			const auto spot = tree_puzzle::SpotIndex(across, along);
			for (auto* thing : {&game.mists.at(spot), &game.bats.at(spot)})
			{
				if (*thing != entt::null && !registry.Valid(*thing))
				{
					*thing = entt::null;
				}
			}
			const auto tree = game.trees.at(spot);
			if (tree == entt::null)
			{
				continue;
			}
			const auto* transform = registry.Valid(tree) ? registry.TryGet<const Transform>(tree) : nullptr;
			if (transform != nullptr && transform->position == game.treesPlacedAt.at(spot))
			{
				GrowTree(tree);
				continue;
			}
			game.started = true;
			Pull(game, across, along);
		}
	}
}

/// The grid of a maze
maze_puzzle::Grid MazeGrid(const PuzzleGame& game)
{
	return {.place = game.place, .steps = GridSteps(game), .angle = game.angle, .scale = game.scale};
}

/// A place on the grid of a puzzle, in grid steps
map_coords::MapCoords StepPlace(const PuzzleGame& game, maze_puzzle::GridStep step)
{
	return hanoi::GridPlace(game.place, GridSteps(game), step.across, step.along);
}

/// The middle of a cell of a maze, in map units
glm::ivec2 CellPlace(const PuzzleGame& game, maze_puzzle::Cell cell)
{
	const auto place = StepPlace(game, maze_puzzle::MiddleOf(cell));
	return {place.x, place.z};
}

/// Whether a thing is there: it was made, and it did not go from the world
bool IsThere(const Registry& registry, entt::entity thing)
{
	return thing != entt::null && registry.Valid(thing);
}

/// Where a thing is in the world, on the land at its map position
glm::vec3 WorldOf(entt::entity thing)
{
	return map_coords::ToWorld(Locator::terrainSystem::value(), ecs::thing_map_place::Of(Entities(), thing));
}

/// A spot visual of the mazes at a point, for some turns or for its own life
EffectHandle StartSparkle(glm::vec3 point, float magnitude, std::optional<int> turns)
{
	if (!Locator::effectRequestSystem::has_value())
	{
		return k_NoEffectHandle;
	}
	return Locator::effectRequestSystem::value().Start({
	    .type = maze_puzzle::k_SpotVisual,
	    .position = point,
	    .magnitude = magnitude,
	    .turns = turns,
	});
}

/// The spot visual that marks the tapped cell of a Theseus maze goes
void RemoveMarker(MazeState& maze)
{
	if (maze.marker != k_NoEffectHandle && Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().Remove(maze.marker);
	}
	maze.marker = k_NoEffectHandle;
}

/// A fence of a maze on its place, at the angle of the puzzle and with its size. Nothing destroys it.
entt::entity MakeFence(const PuzzleGame& game, const maze_puzzle::Fence& fence)
{
	auto& registry = Entities();
	const auto place = StepPlace(game, maze_puzzle::FenceStep(fence));
	const auto world = map_coords::ToWorld(Locator::terrainSystem::value(), place);
	const auto made = archetypes::MobileStaticArchetype::Create(
	    world, maze_puzzle::k_FenceKind, 0.0f, 0.0f, maze_puzzle::FenceAngle(game.angle, fence.facesAcross), 0.0f, game.scale);
	ecs::thing_map_place::Keep(registry, made, place);
	registry.Assign<Indestructible>(made);
	FileInMap(made);
	return made;
}

/// The slot of a fence in the lists of a maze
entt::entity& FenceSlot(MazeState& maze, const maze_puzzle::Fence& fence)
{
	auto& fences = fence.facesAcross ? maze.acrossFences : maze.alongFences;
	return fences.at(maze_puzzle::SlotOf(fence));
}

/// Each turn of a maze, each fence cools down. The maze makes again a fence that went. A fence off its place goes, leaves
/// a ghost, and the maze makes it again on its place, where the hand can not take it.
void PutBackFences(PuzzleGame& game)
{
	auto& registry = Entities();
	for (int32_t across = 0; across <= maze_puzzle::k_MostSide; ++across)
	{
		for (int32_t along = 0; along <= maze_puzzle::k_MostSide; ++along)
		{
			for (const bool facesAcross : {true, false})
			{
				const maze_puzzle::Fence fence {.across = across, .along = along, .facesAcross = facesAcross};
				auto& slot = FenceSlot(game.maze, fence);
				if (slot == entt::null)
				{
					continue;
				}
				if (registry.Valid(slot) && Locator::fireSystem::has_value())
				{
					Locator::fireSystem::value().SetTemperature(slot, 0.0f, entt::null);
				}
				if (!registry.Valid(slot))
				{
					// A fence that the maze makes again after it went is not held from the hand
					slot = MakeFence(game, fence);
					continue;
				}
				if (ecs::thing_map_place::Of(registry, slot) == StepPlace(game, maze_puzzle::FenceStep(fence)))
				{
					continue;
				}
				world_objects::LeaveGhost(slot);
				world_objects::Remove(slot);
				slot = MakeFence(game, fence);
				registry.Assign<CannotBePickedUp>(slot);
			}
		}
	}
}

/// A bell of the lion maze on its place. Nothing destroys it, and nothing moves it.
entt::entity MakeBell(entt::entity puzzle, const PuzzleGame& game, size_t bell)
{
	auto& registry = Entities();
	const auto& [step, angle] = maze_puzzle::BellPlaces(maze_puzzle::SideOf(game.type)).at(bell);
	const auto place = StepPlace(game, step);
	const auto made =
	    archetypes::PuzzleTotemArchetype::Create(map_coords::ToWorld(Locator::terrainSystem::value(), place),
	                                             maze_puzzle::k_BellKind, angle, maze_puzzle::k_BellScale, puzzle);
	// The hand can tap it, nothing moves it, and the scripts hold it
	registry.Get<PuzzleTotem>(made).highest = maze_puzzle::k_TappableHighest;
	registry.Assign<Immovable>(made);
	registry.Assign<ScriptControlled>(made);
	ecs::thing_map_place::Keep(registry, made, place);
	FileInMap(made);
	return made;
}

/// The animals of a maze go, each with a ghost, and the maze makes them again on their first cells. A sparkle shows
/// each of them for some turns. The angles and the moves of the animals stay as they were.
void ResetMaze(entt::entity puzzle, PuzzleGame& game)
{
	auto& registry = Entities();
	auto& maze = game.maze;
	for (const auto animal : {maze.hero, maze.lion})
	{
		if (IsThere(registry, animal))
		{
			world_objects::LeaveGhost(animal);
			world_objects::Remove(animal);
		}
	}
	maze.hero = entt::null;
	maze.lion = entt::null;
	const auto hero = maze_puzzle::HeroStart(game.type);
	const auto lion = maze_puzzle::LionStart(game.type);
	maze.heroAcross = hero.across;
	maze.heroAlong = hero.along;
	maze.lionAcross = lion.across;
	maze.lionAlong = lion.along;
	if (!Locator::animalSystem::has_value())
	{
		return;
	}
	auto& animals = Locator::animalSystem::value();
	// The villager has the size of its age. The sheep and the lion are larger.
	const bool theseus = maze_puzzle::IsTheseus(game.type);
	maze.hero = animals.CreatePuzzlePiece(theseus ? maze_puzzle::k_TheseusHeroKind : maze_puzzle::k_SheepKind,
	                                      CellPlace(game, hero), maze_puzzle::k_PieceAge,
	                                      theseus ? std::nullopt : std::optional(maze_puzzle::k_LargePieceScale), puzzle);
	maze.lion = animals.CreatePuzzlePiece(maze_puzzle::k_LionKind, CellPlace(game, lion), maze_puzzle::k_PieceAge,
	                                      maze_puzzle::k_LargePieceScale, puzzle);
	// The hand can not take them, and nothing destroys them
	for (const auto animal : {maze.hero, maze.lion})
	{
		if (IsThere(registry, animal))
		{
			registry.AssignOrReplace<CannotBePickedUp>(animal);
			registry.AssignOrReplace<Indestructible>(animal);
		}
	}
	for (const auto animal : {maze.hero, maze.lion})
	{
		if (IsThere(registry, animal))
		{
			StartSparkle(WorldOf(animal), maze_puzzle::k_SparkleMagnitude, maze_puzzle::k_StartSparkleTurns);
		}
	}
}

/// A maze at its start:
/// - a fence on each wall of its layout, which the hand can not take,
/// - a palm tree at each corner and a street lantern at the middle of each edge,
/// - its animals on their first cells,
/// - for the lion maze, five bells around the maze.
void StartMaze(entt::entity puzzle, PuzzleGame& game)
{
	auto& registry = Entities();
	const auto steps = maze_puzzle::MazeSteps(GridSteps(game));
	game.cosineStep = steps.cosine;
	game.sineStep = steps.sine;
	for (const auto& fence : maze_puzzle::FencesOf(game.type))
	{
		const auto made = MakeFence(game, fence);
		registry.Assign<CannotBePickedUp>(made);
		FenceSlot(game.maze, fence) = made;
	}
	const auto side = maze_puzzle::SideOf(game.type);
	const auto& terrain = Locator::terrainSystem::value();
	const auto corners = maze_puzzle::CornerSteps(side);
	for (size_t corner = 0; corner < corners.size(); ++corner)
	{
		const auto place = StepPlace(game, corners.at(corner));
		const auto palm = archetypes::TreeArchetype::Create(0, map_coords::ToWorld(terrain, place),
		                                                    maze_puzzle::k_CornerTreeKind, false, 0.0f, 1.0f, 1.0f);
		ecs::thing_map_place::Keep(registry, palm, place);
		FileInMap(palm);
		registry.Assign<ScriptControlled>(palm);
		game.maze.palms.at(corner) = palm;
	}
	const auto edges = maze_puzzle::LanternSteps(side);
	for (size_t edge = 0; edge < edges.size(); ++edge)
	{
		const auto place = StepPlace(game, edges.at(edge));
		const auto lantern =
		    archetypes::StreetLanternArchetype::Create(map_coords::ToWorld(terrain, place), maze_puzzle::k_LanternKind);
		if (lantern != entt::null)
		{
			ecs::thing_map_place::Keep(registry, lantern, place);
			FileInMap(lantern);
		}
		game.lanterns.at(edge) = lantern;
	}
	ResetMaze(puzzle, game);
	if (maze_puzzle::IsTilt(game.type))
	{
		for (size_t bell = 0; bell < maze_puzzle::k_Bells; ++bell)
		{
			game.maze.bells.at(bell) = MakeBell(puzzle, game, bell);
		}
	}
}

/// An animal of a maze turns to the angle of its next move. When it faces that angle, it starts to go to its cell, and
/// then it arrives there.
void MovePiece(const PuzzleGame& game, entt::entity animal, uint16_t angle, int16_t& stage, maze_puzzle::Cell cell)
{
	if (!IsThere(Entities(), animal) || !Locator::animalSystem::has_value())
	{
		return;
	}
	auto& animals = Locator::animalSystem::value();
	if (animals.TurnPieceTowards(animal, angle) != angle)
	{
		return;
	}
	if (stage == 0)
	{
		animals.MovePieceTo(animal, CellPlace(game, cell), maze_puzzle::k_PieceMetresPerSecond);
		stage = 1;
	}
	else if (stage == 1 && animals.PieceArrived(animal))
	{
		stage = 2;
	}
}

/// Whether an animal of a maze goes to a place now
bool MovingToPlace(entt::entity animal)
{
	if (!Locator::animalSystem::has_value())
	{
		return false;
	}
	return Locator::animalSystem::value().TopGameStateOf(animal) == maze_puzzle::k_MoveToPosState;
}

/// One turn of a Theseus maze. When the two animals stand still on their cells, a tap on the land in line with the
/// villager moves the villager one cell, and then the lion up to two cells. Then the animals turn and go to their cells.
void ProcessTheseus(entt::entity puzzle, PuzzleGame& game, const std::optional<map_coords::MapCoords>& tapped)
{
	auto& registry = Entities();
	auto& maze = game.maze;
	if (maze.marker != k_NoEffectHandle && Locator::effectRequestSystem::has_value() &&
	    !Locator::effectRequestSystem::value().IsRunning(maze.marker))
	{
		RemoveMarker(maze);
	}
	PutBackFences(game);
	// An animal that went puts the maze back to its start
	if ((maze.hero != entt::null && !registry.Valid(maze.hero)) || (maze.lion != entt::null && !registry.Valid(maze.lion)))
	{
		ResetMaze(puzzle, game);
		return;
	}
	if (!IsThere(registry, maze.hero) || !IsThere(registry, maze.lion))
	{
		return;
	}
	const bool still = !MovingToPlace(maze.hero) && !MovingToPlace(maze.lion) && maze.lionStage == 2 && maze.heroStage == 2;
	if (still)
	{
		RemoveMarker(maze);
		// The lion caught the villager
		if (ecs::thing_map_place::Of(registry, maze.hero) == ecs::thing_map_place::Of(registry, maze.lion))
		{
			ResetMaze(puzzle, game);
			return;
		}
		if (!tapped.has_value())
		{
			return;
		}
		const auto step = maze_puzzle::TappedStep(MazeGrid(game), maze_puzzle::SideOf(game.type), tapped->x, tapped->z);
		if (!step.has_value())
		{
			return;
		}
		maze.marker = StartSparkle(map_coords::ToWorld(Locator::terrainSystem::value(), StepPlace(game, *step)),
		                           maze_puzzle::k_MarkerMagnitude, std::nullopt);
		const maze_puzzle::Cell hero {.across = maze.heroAcross, .along = maze.heroAlong};
		const auto choice = maze_puzzle::ChooseHeroMove(game.type, *step, hero);
		if (choice.move == maze_puzzle::HeroMove::None)
		{
			return;
		}
		if (choice.move == maze_puzzle::HeroMove::Step)
		{
			const auto moved = maze_puzzle::Moved(hero, choice.direction);
			maze.heroAcross = moved.across;
			maze.heroAlong = moved.along;
			maze.heroAngle = maze_puzzle::AngleOfDirection(choice.direction);
			maze.heroStage = 0;
		}
		game.started = true;
		const auto lion = maze_puzzle::MoveLion(game.type, {.across = maze.lionAcross, .along = maze.lionAlong},
		                                        {.across = maze.heroAcross, .along = maze.heroAlong});
		maze.lionAcross = lion.cell.across;
		maze.lionAlong = lion.cell.along;
		if (lion.angle.has_value())
		{
			maze.lionAngle = *lion.angle;
			maze.lionStage = 0;
		}
	}
	MovePiece(game, maze.lion, maze.lionAngle, maze.lionStage, {.across = maze.lionAcross, .along = maze.lionAlong});
	MovePiece(game, maze.hero, maze.heroAngle, maze.heroStage, {.across = maze.heroAcross, .along = maze.heroAlong});
}

/// One turn of the lion maze. When the two animals stand still, the first bell that the hand tapped rings, and the lion
/// slides in the direction of the bell until a fence stops it. The fifth bell puts the maze back to its start.
void ProcessTilt(entt::entity puzzle, PuzzleGame& game)
{
	auto& registry = Entities();
	auto& maze = game.maze;
	PutBackFences(game);
	if (maze.hero == entt::null || maze.lion == entt::null)
	{
		return;
	}
	if (!registry.Valid(maze.hero) || !registry.Valid(maze.lion))
	{
		ResetMaze(puzzle, game);
	}
	if (IsThere(registry, maze.hero) && IsThere(registry, maze.lion) && !MovingToPlace(maze.hero) && !MovingToPlace(maze.lion))
	{
		// The sheep stays on its cell
		const auto sheep = CellPlace(game, {.across = maze.heroAcross, .along = maze.heroAlong});
		const auto at = ecs::thing_map_place::Of(registry, maze.hero);
		if ((at.x != sheep.x || at.z != sheep.y) && Locator::animalSystem::has_value())
		{
			Locator::animalSystem::value().PutPiece(maze.hero, sheep);
		}
		for (size_t index = 0; index < maze_puzzle::k_MoveBells; ++index)
		{
			auto& bell = maze.bells.at(index);
			if (bell != entt::null && !registry.Valid(bell))
			{
				bell = MakeBell(puzzle, game, index);
			}
			const auto* state = IsThere(registry, bell) ? registry.TryGet<const PuzzleTotem>(bell) : nullptr;
			if (state == nullptr || state->height == 0)
			{
				continue;
			}
			const auto point = WorldOf(bell);
			StartSparkle(point, maze_puzzle::k_SparkleMagnitude, maze_puzzle::k_SparkleTurns);
			if (Locator::sounds::has_value())
			{
				audio::StartGameSoundEffect(
				    entt::hashed_string(fmt::format("{}/{}", k_ScriptBank, maze_puzzle::k_BellSound).c_str()).value(),
				    {.position = point, .owner = bell});
			}
			game.started = true;
			const auto direction = static_cast<int32_t>(index);
			const maze_puzzle::Cell from {.across = maze.lionAcross, .along = maze.lionAlong};
			const auto to = maze_puzzle::Slide(game.type, from, direction);
			if (to != from)
			{
				maze.lionAcross = to.across;
				maze.lionAlong = to.along;
				maze.lionAngle = maze_puzzle::AngleOfDirection(direction);
				maze.lionStage = 0;
			}
			break;
		}
		MovePiece(game, maze.lion, maze.lionAngle, maze.lionStage, {.across = maze.lionAcross, .along = maze.lionAlong});
		const auto reset = maze.bells.at(maze_puzzle::k_ResetBell);
		const auto* state = IsThere(registry, reset) ? registry.TryGet<const PuzzleTotem>(reset) : nullptr;
		if (state != nullptr && state->height != 0)
		{
			ResetMaze(puzzle, game);
		}
	}
	// The taps of this turn go
	for (const auto bell : maze.bells)
	{
		if (auto* state = IsThere(registry, bell) ? registry.TryGet<PuzzleTotem>(bell) : nullptr)
		{
			state->height = 0;
		}
	}
}

/// Whether a maze shows a solution. The solution ends the maze: a sparkle shows where it ended, and its animals go.
bool TestMaze(PuzzleGame& game)
{
	auto& registry = Entities();
	auto& maze = game.maze;
	if (maze_puzzle::IsTheseus(game.type))
	{
		if (!maze_puzzle::Escaped(game.type, {.across = maze.heroAcross, .along = maze.heroAlong}))
		{
			return false;
		}
		// The game reads the villager also when it went. Here the sparkle needs a villager.
		if (IsThere(registry, maze.hero))
		{
			StartSparkle(WorldOf(maze.hero), maze_puzzle::k_SparkleMagnitude, maze_puzzle::k_SparkleTurns);
		}
	}
	else
	{
		if (!IsThere(registry, maze.lion) || !IsThere(registry, maze.hero) || MovingToPlace(maze.lion) || maze.lionStage != 2)
		{
			return false;
		}
		const bool arrived = Locator::animalSystem::has_value() && Locator::animalSystem::value().PieceArrived(maze.lion);
		if (!arrived || maze.lionAcross != maze.heroAcross || maze.lionAlong != maze.heroAlong)
		{
			return false;
		}
		StartSparkle(WorldOf(maze.hero), maze_puzzle::k_SparkleMagnitude, maze_puzzle::k_SparkleTurns);
	}
	for (auto* animal : {&maze.hero, &maze.lion})
	{
		if (IsThere(registry, *animal))
		{
			world_objects::Remove(*animal);
		}
		*animal = entt::null;
	}
	RemoveMarker(maze);
	game.played = true;
	return true;
}

/// Whether the puzzle is one of the tree puzzles
bool IsTreePuzzle(script::PuzzleGameType type)
{
	return type == script::PuzzleGameType::Tree1 || type == script::PuzzleGameType::Tree2;
}

/// The test of a solution. A solved beach temple puzzle fixes its rings, which the hand can then not take.
/// The place of a totem of a totem puzzle: on the curve round the puzzle's place, or in a row across the puzzle's grid
map_coords::MapCoords TotemPlace(const PuzzleGame& game, size_t totem)
{
	if (totem_puzzle::OnCurve(game.type))
	{
		const auto offset = totem_puzzle::CurveOffset(totem, game.scale);
		return {.x = game.place.x + offset.x, .z = game.place.z + offset.z, .altitude = game.place.altitude};
	}
	return hanoi::GridPlace(game.place, GridSteps(game), static_cast<int32_t>(totem), 0);
}

/// The totem is put where its height and its slide put it
void PlaceTotem(entt::entity totem)
{
	if (Locator::puzzleTotemSystem::has_value())
	{
		Locator::puzzleTotemSystem::value().Place(totem);
	}
}

/// A totem of a totem puzzle on its place, at a height. Nothing destroys it, and nothing moves it except the hand.
entt::entity MakeTotem(entt::entity puzzle, const PuzzleGame& game, size_t index, float yAngleRadians)
{
	auto& registry = Entities();
	const auto place = TotemPlace(game, index);
	const auto made =
	    archetypes::PuzzleTotemArchetype::Create(map_coords::ToWorld(Locator::terrainSystem::value(), place),
	                                             totem_puzzle::k_TotemKind, yAngleRadians, totem_puzzle::k_TotemScale, puzzle);
	auto& totem = registry.Get<PuzzleTotem>(made);
	totem.highest = totem_puzzle::k_Highest;
	totem.height = game.totemHeights.at(index);
	registry.Assign<Immovable>(made);
	ecs::thing_map_place::Keep(registry, made, place);
	FileInMap(made);
	PlaceTotem(made);
	return made;
}

/// A totem puzzle starts: six totems, all fully down. The totems on the curve turn their faces away from the puzzle's
/// place.
void StartTotems(entt::entity puzzle, PuzzleGame& game)
{
	game.totemHeights.fill(0);
	for (size_t index = 0; index < totem_puzzle::k_Totems; ++index)
	{
		const float facing =
		    totem_puzzle::OnCurve(game.type) ? totem_puzzle::CurveFacing(game.place, TotemPlace(game, index)) : 0.0f;
		game.totems.at(index) = MakeTotem(puzzle, game, index, facing);
	}
}

/// The spot visual over a totem of a totem puzzle goes
void RemoveTotemMark(uint32_t& mark)
{
	if (mark != k_NoEffectHandle && Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().Remove(mark);
	}
	mark = k_NoEffectHandle;
}

/// One turn of a totem puzzle. It makes again each totem that went, up to the first totem that the player changed. The
/// puzzle keeps the change and gives the new heights to the totems; the other totems that change slide to theirs. A
/// spot visual then shows each totem that stands fully up. Only one change comes in each turn. A totem that the puzzle
/// makes again faces angle 0 and stays in the scripts' hold.
void ProcessTotems(entt::entity puzzle, PuzzleGame& game)
{
	auto& registry = Entities();
	for (size_t index = 0; index < totem_puzzle::k_Totems; ++index)
	{
		auto& slot = game.totems.at(index);
		if (slot != entt::null && !registry.Valid(slot))
		{
			slot = MakeTotem(puzzle, game, index, 0.0f);
			registry.Assign<ScriptControlled>(slot);
		}
		const auto* totem = IsThere(registry, slot) ? registry.TryGet<const PuzzleTotem>(slot) : nullptr;
		if (totem == nullptr || totem->height == game.totemHeights.at(index))
		{
			// A totem that slides takes its next step
			if (totem != nullptr && totem->slideSteps != 0 && Locator::puzzleTotemSystem::has_value())
			{
				Locator::puzzleTotemSystem::value().StepSlide(slot);
			}
			continue;
		}
		game.started = true;
		auto shown = game.totemHeights;
		shown.at(index) = totem->height;
		const auto change = totem_puzzle::Step(game.type, game.totemHeights, shown);
		if (!change.has_value())
		{
			return;
		}
		game.totemHeights = change->kept;
		for (size_t other = 0; other < totem_puzzle::k_Totems; ++other)
		{
			const auto thing = game.totems.at(other);
			auto* changed = IsThere(registry, thing) ? registry.TryGet<PuzzleTotem>(thing) : nullptr;
			if (changed == nullptr)
			{
				continue;
			}
			changed->previousHeight = changed->height;
			changed->height = game.totemHeights.at(other);
			if (other != change->moved)
			{
				changed->slideSteps = totem_puzzle::k_SlideSteps;
				++changed->slides;
			}
			if (Locator::puzzleTotemSystem::has_value())
			{
				Locator::puzzleTotemSystem::value().StepSlide(thing);
			}
		}
		const auto& terrain = Locator::terrainSystem::value();
		for (size_t other = 0; other < totem_puzzle::k_Totems; ++other)
		{
			auto& mark = game.totemMarks.at(other);
			RemoveTotemMark(mark);
			if (game.totemHeights.at(other) == totem_puzzle::k_Highest)
			{
				mark = StartSparkle(map_coords::ToWorld(terrain, TotemPlace(game, other)), totem_puzzle::k_MarkMagnitude,
				                    std::nullopt);
			}
		}
		return;
	}
}

/// The test of a totem puzzle's solution: all totems fully up. The spot visuals over the totems go, and each totem
/// shows a spot visual for some turns.
bool TotemsSolved(PuzzleGame& game)
{
	if (!totem_puzzle::Solved(game.totemHeights))
	{
		return false;
	}
	std::ranges::for_each(game.totemMarks, RemoveTotemMark);
	const auto& terrain = Locator::terrainSystem::value();
	for (size_t index = 0; index < totem_puzzle::k_Totems; ++index)
	{
		StartSparkle(map_coords::ToWorld(terrain, TotemPlace(game, index)), totem_puzzle::k_MarkMagnitude,
		             totem_puzzle::k_SolvedMarkTurns);
	}
	game.played = true;
	return true;
}

bool TestSolution(entt::entity puzzle, PuzzleGame& game)
{
	if (IsTreePuzzle(game.type))
	{
		// The test looks only at the trees of the puzzle. After the solution the puzzle has no trees, so the test gives no.
		if (!TreesSolved(Entities(), game))
		{
			return false;
		}
		SolveTrees(game);
		return true;
	}
	if (maze_puzzle::IsMaze(game.type))
	{
		return TestMaze(game);
	}
	if (totem_puzzle::IsTotemPuzzle(game.type))
	{
		return TotemsSolved(game);
	}
	// The fish puzzle is solved when its fish are caught
	if (game.type == script::PuzzleGameType::Fishes1)
	{
		return game.fish.has_value() && game.fish->net.caught;
	}
	if (game.type != script::PuzzleGameType::Hanoi)
	{
		return ecs::puzzle_game::PlayedBeforeSolved(game.type);
	}
	auto& registry = Entities();
	if (!hanoi::Solved(StacksOf(registry, game)))
	{
		return false;
	}
	for (const auto ring : game.rings)
	{
		if (auto* block = BlockOf(registry, ring))
		{
			block->fixed = true;
			registry.AssignOrReplace<Immovable>(ring);
		}
	}
	game.played = true;
	UpdatePickUp(puzzle, game);
	return true;
}

/// One turn of the beach temple puzzle: the puzzle makes again the bases off their places and the rings that went. A
/// ring that the hand let go of goes onto a stack near it, or back to its place after a while
void ProcessHanoi(entt::entity puzzle, PuzzleGame& game)
{
	auto& registry = Entities();
	const hanoi::Steps steps {.cosine = game.cosineStep, .sine = game.sineStep};
	bool calm = true;
	bool settled = false;
	for (size_t base = 0; base < hanoi::k_Bases; ++base)
	{
		auto& piece = game.bases.at(base);
		if (piece == entt::null)
		{
			continue;
		}
		const auto place = hanoi::BasePlace(game.place, steps, static_cast<int32_t>(base));
		if (registry.Valid(piece) && !(registry.Get<const HanoiBlock>(piece).home == place && AtPlace(registry, piece)))
		{
			// A base off its place goes and a new base stands on the place, with no links
			world_objects::LeaveGhost(piece);
			world_objects::Remove(piece);
			piece = MakePiece(puzzle, hanoi::k_BaseKind, place);
			registry.Assign<ScriptControlled>(piece);
		}
		if (!registry.Valid(piece))
		{
			MakeAgain(puzzle, game, base, game.kept.at(base));
		}
	}
	for (size_t ring = 0; ring < hanoi::k_Rings; ++ring)
	{
		const auto slot = k_FirstRingSlot + ring;
		auto& piece = game.rings.at(ring);
		if (piece != entt::null && !registry.Valid(piece))
		{
			MakeAgain(puzzle, game, slot, game.kept.at(slot));
		}
		if (piece == entt::null || AtPlace(registry, piece) || registry.AllOf<InHand>(piece))
		{
			continue;
		}
		const auto kind = KindOf(registry, piece);
		const auto at = ecs::thing_map_place::Of(registry, piece);
		const auto top = FindStack(registry, at, at, piece);
		bool remake = false;
		if (top != entt::null && hanoi::MayRestOn(kind, KindOf(registry, top)))
		{
			// The ring goes onto the stack, higher by the height of the top piece
			auto& block = registry.Get<HanoiBlock>(piece);
			auto& topBlock = registry.Get<HanoiBlock>(top);
			block.below = top;
			topBlock.above = piece;
			block.home = topBlock.home;
			block.home.altitude = hanoi::HeightOf(KindOf(registry, top)) + block.home.altitude;
			settled = true;
			game.started = true;
			remake = true;
		}
		else if (settled)
		{
			// Once a ring of this turn went onto a stack, every other ring off its place goes back to its place
			remake = true;
		}
		else
		{
			calm = false;
			game.calmTurns = hanoi::CalmAfterRestlessTurn(game.calmTurns);
			if (game.calmTurns < 0)
			{
				// After a while the ring goes back onto the stack at its place
				auto& block = registry.Get<HanoiBlock>(piece);
				const auto back = FindStack(registry, block.home, block.home, piece);
				if (auto* backBlock = BlockOf(registry, back))
				{
					backBlock->above = piece;
				}
				block.above = entt::null;
				block.below = back;
				remake = true;
			}
		}
		if (remake)
		{
			ReplaceAndMakeAgain(puzzle, game, slot);
		}
	}
	if (calm)
	{
		game.calmTurns = hanoi::CalmAfterCalmTurn(game.calmTurns);
	}
}

/// The model of the floats goes into the mesh cache the first time a fish puzzle needs it
void LoadFloatModel()
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(ecs::fish_puzzle::k_FloatMeshId.value()))
	{
		return;
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto path = fileSystem.GetPath<filesystem::Path::Misc>() / ecs::fish_puzzle::k_FloatModelFile;
	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Model {} not found", path.generic_string());
		return;
	}
	meshes.Load(ecs::fish_puzzle::k_FloatMeshId.value(), resources::L3DLoader::FromDiskTag {}, path);
}

/// One turn of the fish puzzle. Its first turn makes the ring of floats and the two shoals. Then the shoals swim, also
/// after the solution, until the fish are caught; each fish that is caught makes a ring on the water.
void ProcessFish(PuzzleGame& game, const std::optional<glm::vec3>& scare)
{
	auto& random = Locator::gameRandom::value();
	// The draws come from the seed that every computer of a game shares
	const fish_shoal::Random draw = [&random](float low, float high) { return low + random.GameFloatRand(high - low); };
	if (!game.fish.has_value())
	{
		const glm::vec3 middle {map_coords::ToMetres(game.place.x), game.place.altitude, map_coords::ToMetres(game.place.z)};
		game.fish = ecs::fish_puzzle::Make(middle, draw);
		LoadFloatModel();
	}
	constexpr auto k_TurnMilliseconds = static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count());
	const auto caught = ecs::fish_puzzle::StepTurn(*game.fish, k_TurnMilliseconds, scare, draw);
	if (caught.empty() || !Locator::waterRingSystem::has_value())
	{
		return;
	}
	for (const auto& point : caught)
	{
		Locator::waterRingSystem::value().Add({.position = point, .growth = ecs::fish_puzzle::k_CaughtRingGrowth});
	}
}

/// One turn of a puzzle game, with the place on the land that a player tapped in this turn and the last scare of the
/// fish
void Process(entt::entity puzzle, PuzzleGame& game, const std::optional<map_coords::MapCoords>& tapped,
             const std::optional<glm::vec3>& fishScare)
{
	if (game.type == script::PuzzleGameType::Fishes1)
	{
		ProcessFish(game, fishScare);
	}
	if (ecs::puzzle_game::LetsGoOfThingsThatWent(game.type))
	{
		// The puzzle lets go of each palm tree and each lantern that went. It does not make them again.
		const auto& registry = Entities();
		for (size_t corner = 0; corner < game.lanterns.size(); ++corner)
		{
			for (auto* thing : {&game.maze.palms.at(corner), &game.lanterns.at(corner)})
			{
				if (*thing != entt::null && !registry.Valid(*thing))
				{
					*thing = entt::null;
				}
			}
		}
	}
	if (game.played)
	{
		return;
	}
	if (TestSolution(puzzle, game))
	{
		game.played = true;
		return;
	}
	if (game.type == script::PuzzleGameType::Hanoi)
	{
		ProcessHanoi(puzzle, game);
		UpdatePickUp(puzzle, game);
	}
	else if (IsTreePuzzle(game.type))
	{
		ProcessTrees(game);
	}
	else if (maze_puzzle::IsTheseus(game.type))
	{
		ProcessTheseus(puzzle, game, tapped);
	}
	else if (maze_puzzle::IsTilt(game.type))
	{
		ProcessTilt(puzzle, game);
	}
	else if (totem_puzzle::IsTotemPuzzle(game.type))
	{
		ProcessTotems(puzzle, game);
	}
}
} // namespace

entt::entity PuzzleGameSystem::Create(script::PuzzleGameType type, glm::vec3 point, float yAngleRadians, float scale)
{
	auto& registry = Entities();
	uint32_t order = 0;
	registry.Each<const PuzzleGame>([&order](const PuzzleGame& other) { order = std::max(order, other.order + 1); });
	PuzzleGame game {
	    .type = type,
	    .place = map_coords::FromMetres({point.x, point.z}),
	    .angle = static_cast<uint16_t>(hanoi::GameAngleOf(yAngleRadians)),
	    .scale = scale,
	    .order = order,
	};
	const auto steps = hanoi::StepsOf(game.angle, scale);
	game.cosineStep = steps.cosine;
	game.sineStep = steps.sine;
	const auto world = map_coords::ToWorld(Locator::terrainSystem::value(), game.place);
	const auto puzzle = archetypes::PuzzleGameArchetype::Create(world, yAngleRadians, game);
	if (type == script::PuzzleGameType::Hanoi)
	{
		auto& made = registry.Get<PuzzleGame>(puzzle);
		StartHanoi(puzzle, made);
		UpdatePickUp(puzzle, made);
	}
	else if (IsTreePuzzle(type))
	{
		StartTrees(registry.Get<PuzzleGame>(puzzle));
	}
	else if (maze_puzzle::IsMaze(type))
	{
		StartMaze(puzzle, registry.Get<PuzzleGame>(puzzle));
	}
	else if (totem_puzzle::IsTotemPuzzle(type))
	{
		StartTotems(puzzle, registry.Get<PuzzleGame>(puzzle));
	}
	return puzzle;
}

void PuzzleGameSystem::ProcessTurn()
{
	auto& registry = Entities();
	std::vector<std::pair<uint32_t, entt::entity>> puzzles;
	registry.Each<const PuzzleGame>(
	    [&puzzles](entt::entity puzzle, const PuzzleGame& game) { puzzles.emplace_back(game.order, puzzle); });
	std::ranges::sort(puzzles, std::ranges::greater {});
	for (const auto& [order, puzzle] : puzzles)
	{
		if (auto* game = registry.Valid(puzzle) ? registry.TryGet<PuzzleGame>(puzzle) : nullptr)
		{
			Process(puzzle, *game, _tappedPlace, _fishScare);
		}
	}
	// The tapped place and the scare of the fish are for this turn only
	_tappedPlace.reset();
	_fishScare.reset();
}

std::optional<bool> PuzzleGameSystem::Played(entt::entity puzzle)
{
	auto& registry = Entities();
	auto* game = puzzle != entt::null && registry.Valid(puzzle) ? registry.TryGet<PuzzleGame>(puzzle) : nullptr;
	if (game == nullptr)
	{
		return std::nullopt;
	}
	return TestSolution(puzzle, *game);
}

std::optional<script::PuzzleGameStatus> PuzzleGameSystem::State(entt::entity puzzle)
{
	auto& registry = Entities();
	auto* game = puzzle != entt::null && registry.Valid(puzzle) ? registry.TryGet<PuzzleGame>(puzzle) : nullptr;
	if (game == nullptr)
	{
		return std::nullopt;
	}
	if (!TestSolution(puzzle, *game))
	{
		return game->started ? script::PuzzleGameStatus::GameInProgress : script::PuzzleGameStatus::GameNotYetBegun;
	}
	return script::PuzzleGameStatus::Won;
}

void PuzzleGameSystem::TakenInHand(entt::entity piece)
{
	auto& registry = Entities();
	auto* block = BlockOf(registry, piece);
	if (block == nullptr)
	{
		return;
	}
	if (auto* below = BlockOf(registry, block->below))
	{
		below->above = entt::null;
	}
	block->above = entt::null;
	block->below = entt::null;
	// The hand that takes the largest ring says a line
	if (KindOf(registry, piece) == hanoi::k_RingKinds.front() && Locator::helpSpeechSystem::has_value())
	{
		Locator::helpSpeechSystem::value().Say(k_LargestRingLine + LocalRandom(k_LargestRingLines), audio::SpeechVoice::First,
		                                       std::nullopt);
	}
	const auto puzzle = block->puzzle;
	if (auto* game = puzzle != entt::null && registry.Valid(puzzle) ? registry.TryGet<PuzzleGame>(puzzle) : nullptr)
	{
		UpdatePickUp(puzzle, *game);
	}
}

void PuzzleGameSystem::Forget(entt::entity puzzle)
{
	auto& registry = Entities();
	auto* game = puzzle != entt::null && registry.Valid(puzzle) ? registry.TryGet<PuzzleGame>(puzzle) : nullptr;
	if (game == nullptr)
	{
		return;
	}
	for (const auto ring : game->rings)
	{
		if (auto* block = BlockOf(registry, ring))
		{
			block->puzzle = entt::null;
			registry.AssignOrReplace<CannotBePickedUp>(ring);
		}
	}
}

bool PuzzleGameSystem::DeletePieces(entt::entity puzzle)
{
	auto& registry = Entities();
	auto* game = puzzle != entt::null && registry.Valid(puzzle) ? registry.TryGet<PuzzleGame>(puzzle) : nullptr;
	if (game == nullptr)
	{
		return false;
	}
	const auto pieces =
	    ecs::puzzle_game::TakePiecesForDelete(*game, [&registry](entt::entity thing) { return registry.Valid(thing); });
	for (const auto piece : pieces)
	{
		if (registry.Valid(piece))
		{
			world_objects::Remove(piece);
		}
	}
	return true;
}

void PuzzleGameSystem::TapPlace(const map_coords::MapCoords& place)
{
	_tappedPlace = place;
}

bool PuzzleGameSystem::TapTotem(entt::entity totem)
{
	auto& registry = Entities();
	auto* state = totem != entt::null && registry.Valid(totem) ? registry.TryGet<PuzzleTotem>(totem) : nullptr;
	if (state == nullptr)
	{
		return false;
	}
	if (state->highest == maze_puzzle::k_TappableHighest)
	{
		state->height = maze_puzzle::k_TappableHighest;
	}
	// Each tap makes a sound that the player hears everywhere
	if (Locator::sounds::has_value())
	{
		audio::StartGameSoundEffect(
		    entt::hashed_string(fmt::format("{}/{}", k_GameBank, maze_puzzle::k_BellTapSound).c_str()).value(), {});
	}
	return true;
}

void PuzzleGameSystem::ScareFish(glm::vec3 point)
{
	_fishScare = point;
}

bool PuzzleGameSystem::HasFishPuzzle() const
{
	bool found = false;
	Entities().Each<const PuzzleGame>(
	    [&found](const PuzzleGame& game) { found = found || game.type == script::PuzzleGameType::Fishes1; });
	return found;
}

void PuzzleGameSystem::AfterLoad()
{
	_tappedPlace.reset();
	_fishScare.reset();
	auto& registry = Entities();
	registry.Each<PuzzleGame>([](PuzzleGame& game) { ecs::puzzle_game::ForgetUnsavedState(game); });
}
