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

#include <algorithm>
#include <chrono>
#include <span>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/AlignmentHistory.h"
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

/// The value that the land keeps for an alignment, from 0, good, to 2, evil. The alignment becomes a goodness from 0 to
/// 1, held there. A goodness that is not a number becomes 0. The value is two times the evilness, 1 less the goodness.
[[nodiscard]] inline float LandValue(float alignment)
{
	float goodness = (alignment + 1.0f) * 0.5f;
	if (!(goodness >= 0.0f))
	{
		goodness = 0.0f;
	}
	else if (goodness > 1.0f)
	{
		goodness = 1.0f;
	}
	const float evilness = 1.0f - goodness;
	return evilness + evilness;
}

/// The alignment that lights the temple's insides, from the value of the land: 1 less that value
[[nodiscard]] inline float TempleLightAlignment(float land)
{
	return 1.0f - land;
}

/// The alignment that the music and the ambience follow, from the value of the land: 2 less that value, less 1
[[nodiscard]] inline float AudioAlignment(float land)
{
	return (2.0f - land) - 1.0f;
}

/// The value of the sky after a frame of game time in whole milliseconds. The sky moves to the value of the land by a
/// hundredth of each millisecond, times 0.1, and stops at that value.
[[nodiscard]] inline float SkyLandValue(float sky, float land, int64_t milliseconds)
{
	const float time = static_cast<float>(milliseconds) * 0.01f;
	if (!(land >= sky))
	{
		const float moved = sky - (time * 0.1f);
		return moved >= land ? moved : land;
	}
	if (land > sky)
	{
		const float moved = (time * 0.1f) + sky;
		return moved <= land ? moved : land;
	}
	return sky;
}

/// A script can change an alignment only by an amount from -1 to 1. Other amounts, and an amount that is not a number,
/// change nothing.
[[nodiscard]] inline bool ScriptChangeInRange(float change)
{
	return change >= -1.0f && change <= 1.0f;
}

/// An alignment held between -1 and 1. A value that is not a number becomes -1.
[[nodiscard]] inline float HeldAlignment(float alignment)
{
	if (!(alignment >= -1.0f))
	{
		return -1.0f;
	}
	return alignment > 1.0f ? 1.0f : alignment;
}

/// The alignment after an amount is added to it at once, held between -1 and 1. A sum that is not a number becomes -1.
[[nodiscard]] inline float AddedAlignment(float alignment, float change)
{
	return HeldAlignment(alignment + change);
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
	/// The alignment, held between -1 and 1. A value that is not a number becomes -1.
	virtual void SetPlayerAlignment(PlayerNames player, float alignment) = 0;
	/// Added to the alignment, held between -1 and 1, as the script's SET_ALIGNMENT does
	virtual void AddPlayerAlignment(PlayerNames player, float change) = 0;
	/// A change the player's deeds made, such as a miracle hurting or healing, which moves the alignment over the turns to
	/// come no faster than the game's limit a turn
	virtual void AddPendingAlignment(PlayerNames player, float change) = 0;
	/// The change still to come
	[[nodiscard]] virtual float GetPendingAlignment(PlayerNames player) const = 0;
	/// Adds a change to the total of its cause in the history of the player at this computer. The changes of other
	/// players are not kept. Only a debug window shows the totals.
	virtual void AddToHistory(PlayerNames /*player*/, components::AlignmentCause /*cause*/, float /*change*/) {}

	/// In the turn of the player, after its stones and before its computer mind: the waiting change of the player moves
	/// its alignment. The alignment of the neutral player then goes back to 0.
	virtual void PlayerTurn(PlayerNames player) = 0;
	/// At the end of the players' turns: the camera takes the alignment of the player of most influence at its eye, the
	/// neutral player's where no player has any. The eye is the camera that the player of this computer last reported,
	/// so that the frames do not change it. Before the first report, the eye is at the middle of the world's axes.
	virtual void UpdatePlayersTurn() = 0;
	/// Each game turn: the plan of each creature adds its change, and the waiting change moves its alignment
	virtual void UpdateCreaturesTurn() = 0;
	/// At the end of the creature's own turn: its plan adds its change, and the waiting change moves its alignment
	virtual void CreatureTurn(entt::entity /*creature*/) {}
	/// The sky turns toward the camera's alignment, by game time, none while paused
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;

	/// The value of the land at the camera, as of the last turn, from 0, good, to 2, evil (alignment::LandValue)
	[[nodiscard]] virtual float GetCameraLand() const = 0;
	/// The value of the land that the sky shows, from 0, good, to 2, evil. It moves to the value at the camera.
	[[nodiscard]] virtual float GetSkyLand() const = 0;

	/// The alignment at the camera that the ambience and the land's music follow, from -1, evil, to 1, good
	[[nodiscard]] float GetCameraAlignment() const { return alignment::AudioAlignment(GetCameraLand()); }
	/// The alignment at the camera that lights the temple's insides, from -1, evil, to 1, good
	[[nodiscard]] float GetTempleLightAlignment() const { return alignment::TempleLightAlignment(GetCameraLand()); }
};

} // namespace openblack::ecs::systems
