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
#include "Audio/GameSoundEffects.h"
#include "Audio/HelpSpeech.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/PuzzleGameArchetype.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/HanoiRules.h"
#include "ECS/Map.h"
#include "ECS/PuzzleGameRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using openblack::ecs::Registry;
namespace archetypes = openblack::ecs::archetypes;
namespace world_objects = openblack::ecs::world_objects;
namespace hanoi = openblack::ecs::hanoi;

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
	if (Locator::audio::has_value())
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

/// Whether the piece is where the puzzle put it
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
		registry.Get<HanoiBlock>(piece).below = below;
		registry.Get<HanoiBlock>(below).above = piece;
		below = piece;
	}
}

/// The test of a solution. A solved beach temple puzzle fixes its rings, which the hand can then not take.
bool TestSolution(entt::entity puzzle, PuzzleGame& game)
{
	if (game.type != script::PuzzleGameType::Hanoi)
	{
		return ecs::puzzle_game::PlayedBeforeSolved(game.type).value_or(false);
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

/// One turn of a puzzle game
void Process(entt::entity puzzle, PuzzleGame& game)
{
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
}
} // namespace

entt::entity PuzzleGameSystem::Create(script::PuzzleGameType type, glm::vec3 point, float yAngleRadians, float scale)
{
	if (!ecs::puzzle_game::PlayedBeforeSolved(type).has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "No puzzle game of kind {} is made", static_cast<int>(type));
		return entt::null;
	}
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
