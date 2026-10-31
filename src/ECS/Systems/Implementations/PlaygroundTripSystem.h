/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/PlaygroundTripSystemInterface.h"
#include "Story/PlaygroundTrip.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class PlaygroundTripSystem final: public PlaygroundTripSystemInterface
{
public:
	[[nodiscard]] bool InfluenceEverywhere() const final { return _influenceEverywhere; }
	[[nodiscard]] bool IsLessonRunning() const final { return _lessons.running; }
	void StartLesson() final { playground_trip::StartLesson(_lessons); }
	bool AskLessonEnd() final { return playground_trip::AskEnd(_lessons); }
	void EndTurn() final;

	bool SaveBeforeTrip() final;
	bool Travel() final;
	bool Leave() final;

	void SaveState(reflection::BinaryWriter& out) const final;
	bool LoadState(reflection::BinaryReader& in) final;

private:
	playground_trip::Lessons _lessons;
	bool _influenceEverywhere {false};
};

} // namespace openblack::ecs::systems
