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
#include "ECS/Archetypes/PuzzleGameArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/CreatureObjectAction.h"
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
#include "ECS/HanoiRules.h"
#include "ECS/Map.h"
#include "ECS/PuzzleGameRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/TreePuzzleRules.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
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

namespace
{
/// A piece that the puzzle makes again makes one of these sounds of the editor's bank, heard everywhere
constexpr std::string_view k_PieceBank = "editor.sad";
constexpr uint32_t k_PieceSound = 413;
constexpr uint32_t k_PieceSounds = 9;
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

/// Where the creature took the piece, while a creature holds it. The map keeps the piece there until the creature lets
/// it go, so the puzzle sees it there too.
const HeldByCreature::Taken* TakenFrom(const Registry& registry, entt::entity piece)
{
	const auto* holder = registry.TryGet<const HeldByCreature>(piece);
	return holder != nullptr && holder->takenFrom.has_value() ? &*holder->takenFrom : nullptr;
}

/// Whether the piece is where the puzzle put it
bool AtPlace(const Registry& registry, entt::entity piece)
{
	const auto& block = registry.Get<const HanoiBlock>(piece);
	if (const auto* taken = TakenFrom(registry, piece))
	{
		return taken->position == block.placedAt;
	}
	const auto* transform = registry.TryGet<const Transform>(piece);
	return transform != nullptr && transform->position == block.placedAt;
}

/// Where the piece is on the map
map_coords::MapCoords PlaceOf(const Registry& registry, entt::entity piece)
{
	const auto* taken = TakenFrom(registry, piece);
	return taken != nullptr ? taken->place : ecs::thing_map_place::Of(registry, piece);
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

/// Whether the puzzle is one of the tree puzzles
bool IsTreePuzzle(script::PuzzleGameType type)
{
	return type == script::PuzzleGameType::Tree1 || type == script::PuzzleGameType::Tree2;
}

/// The test of a solution. A solved beach temple puzzle fixes its rings, which the hand can then not take.
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
		const auto at = PlaceOf(registry, piece);
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

/// One turn of a puzzle game
void Process(entt::entity puzzle, PuzzleGame& game)
{
	if (ecs::puzzle_game::LetsGoOfThingsThatWent(game.type))
	{
		// The puzzle lets go of each lantern that went. It does not make the lantern again.
		const auto& registry = Entities();
		for (auto& lantern : game.lanterns)
		{
			if (lantern != entt::null && !registry.Valid(lantern))
			{
				lantern = entt::null;
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
			Process(puzzle, *game);
		}
	}
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
