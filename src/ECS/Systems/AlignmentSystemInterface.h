/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <chrono>
#include <span>

#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

namespace alignment
{

/// A player's influence and alignment at one place
struct PlayerAtPlace
{
	float influence;
	float alignment;
};

/// The alignment of the player of most influence at a place, the players in their order. The neutral player, whose
/// alignment is 0, holds the place until a player has more than no influence there, and a later player has to have
/// more than an earlier one to take it.
[[nodiscard]] inline float MostInfluentialAlignment(std::span<const PlayerAtPlace> players)
{
	float best = 0.0f;
	float alignment = 0.0f;
	for (const auto& player : players)
	{
		if (player.influence > best)
		{
			best = player.influence;
			alignment = player.alignment;
		}
	}
	return alignment;
}

/// The alignment as the land shows it: taken as a goodness from 0 to 1, held there, and back to -1, evil, to 1, good
[[nodiscard]] inline float LandAlignment(float alignment)
{
	const float goodness = std::clamp((alignment + 1.0f) * 0.5f, 0.0f, 1.0f);
	return (goodness * 2.0f) - 1.0f;
}

} // namespace alignment

/// The one source of how good or evil the world looks. Each player has an alignment, from -1, evil, to 1, good
/// (components::Alignment). Every game turn the camera takes the alignment of the player whose influence it is in,
/// which lights the temple's insides and picks the ambience, and the sky turns to it a whole a second of game time.
/// Each temple's outside shows its own player's.
class AlignmentSystemInterface
{
public:
	virtual ~AlignmentSystemInterface() = default;

	/// None for a player who isn't in the game
	[[nodiscard]] virtual float GetPlayerAlignment(PlayerNames player) const = 0;
	/// The alignment, held between -1 and 1
	virtual void SetPlayerAlignment(PlayerNames player, float alignment) = 0;
	/// Added to the alignment, held between -1 and 1, as the script's SET_ALIGNMENT does
	virtual void AddPlayerAlignment(PlayerNames player, float change) = 0;
	/// A change the player's deeds made, such as a miracle hurting or healing, which moves the alignment over the turns to
	/// come no faster than the game's limit a turn
	virtual void AddPendingAlignment(PlayerNames player, float change) = 0;
	/// The change still to come
	[[nodiscard]] virtual float GetPendingAlignment(PlayerNames player) const = 0;

	/// At the end of a game turn the camera takes the alignment of the player of most influence at its eye, the neutral
	/// player's where no player has any
	virtual void UpdateTurn(const glm::vec3& eye) = 0;
	/// The sky turns toward the camera's alignment, by game time, none while paused
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;

	/// The alignment the camera is in, as of the last turn, from -1, evil, to 1, good, which the temple's insides are lit by
	/// and the ambience and the land's music follow
	[[nodiscard]] virtual float GetCameraAlignment() const = 0;
	/// The alignment the sky shows, from -1, evil, to 1, good
	[[nodiscard]] virtual float GetSkyAlignment() const = 0;
};

} // namespace openblack::ecs::systems
