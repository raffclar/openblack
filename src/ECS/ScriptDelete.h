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

/// What the scripts' "delete" does to a thing, for each of its four ways: at once, with fade, with explode and with
/// temple explode
namespace openblack::ecs::script_delete
{

/// The speed of the pieces of a model that a script deletes with explode, in metres a second
inline constexpr float k_ExplodeSpeed = 15.0f;
/// The speed of the pieces of a temple that a script deletes with temple explode while the temple is in its destruction
inline constexpr float k_TempleExplodeSpeed = 80.0f;

/// What the script tells the thing to do. The values are the values that the scripts give.
enum class Mode : int32_t
{
	Now = 0,
	Fade = 1,
	Explode = 2,
	TempleExplode = 3,
};

/// The facts about the thing that the script deletes
struct Target
{
	/// A puzzle game that a script made
	bool puzzleGame {false};
	/// One of the objects of the world, which a miracle can hit, and not only a thing with a place, such as a town
	bool object {false};
	bool creature {false};
	bool temple {false};
	/// The temple is in its destruction now
	bool templeDestroying {false};
};

/// What occurs to the thing
enum class Step : uint8_t
{
	/// The script lets go of the thing, and the thing stays
	LetGo,
	/// The puzzle game removes its pieces and stays. The script lets go of it
	ClearPuzzle,
	/// The thing goes at once
	Remove,
	/// The creature fizzes out over two seconds and goes for good
	Fizz,
	/// A ghost of the thing flickers out where it stood, and the thing goes
	GhostThenRemove,
	/// The model of the thing breaks into pieces where it stood, and the thing goes
	BreakUpThenRemove,
	/// The temple starts its destruction
	StartTempleDestruction,
	/// The model of the temple breaks into pieces. The temple stays until its destruction ends
	BreakUpTemple,
};

/// What a script's delete in a mode does to a thing. A mode that is not one of the four only lets go of the thing.
[[nodiscard]] constexpr Step Choose(int32_t mode, const Target& target)
{
	if (target.puzzleGame)
	{
		return Step::ClearPuzzle;
	}
	switch (static_cast<Mode>(mode))
	{
	case Mode::Now:
		return Step::Remove;
	case Mode::Fade:
		if (!target.object)
		{
			return Step::LetGo;
		}
		return target.creature ? Step::Fizz : Step::GhostThenRemove;
	case Mode::Explode:
		return target.object ? Step::BreakUpThenRemove : Step::LetGo;
	case Mode::TempleExplode:
		if (!target.object || !target.temple)
		{
			return Step::LetGo;
		}
		return target.templeDestroying ? Step::BreakUpTemple : Step::StartTempleDestruction;
	}
	return Step::LetGo;
}

} // namespace openblack::ecs::script_delete
