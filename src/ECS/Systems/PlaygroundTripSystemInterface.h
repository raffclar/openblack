/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/SavedStateInterface.h"

namespace openblack::ecs::systems
{

/// The trip to the Gods' Playground and back (see playground_trip). The game asks the questions. This system does what
/// the answers start, and keeps whether a lesson of the Playground runs. A saved game keeps whether every player's
/// influence covers the whole land, and whether Escape asked for the end of a lesson. It does not keep whether a lesson
/// runs.
class PlaygroundTripSystemInterface: public SavedStateInterface
{
public:
	virtual ~PlaygroundTripSystemInterface() = default;

	/// Whether every player's influence covers the whole land, as it does from the trip to the Playground until the player
	/// leaves
	[[nodiscard]] virtual bool InfluenceEverywhere() const = 0;
	/// Whether a lesson of the Playground runs. The trip there starts one.
	[[nodiscard]] virtual bool IsLessonRunning() const = 0;
	/// A lesson tells the game that it runs, as it starts
	virtual void StartLesson() = 0;
	/// Escape asks for the end of the lesson that runs. Returns false when no lesson runs.
	virtual bool AskLessonEnd() = 0;
	/// At the end of the game turn: a lesson that Escape asked to end ends now. Every thing that a script made goes, every
	/// script stops, the scripts let go of the screen, the scrolls that run each lesson again start, and the interface is
	/// normal again.
	virtual void EndTurn() = 0;

	/// The first part of the trip to the Playground: the game saves into the slot of the save on exit. Returns whether
	/// it wrote every file.
	virtual bool SaveBeforeTrip() = 0;
	/// The second part: every script stops, the Playground's land loads with no loading screen, every player's influence
	/// covers the whole land, and the lessons start. Returns whether the land loaded.
	virtual bool Travel() = 0;
	/// The player leaves the Playground: the interface is normal, the influence goes back to its usual reach, no lesson
	/// runs, and the game in the slot of the save on exit loads. Returns whether it loaded.
	virtual bool Leave() = 0;
};

} // namespace openblack::ecs::systems
