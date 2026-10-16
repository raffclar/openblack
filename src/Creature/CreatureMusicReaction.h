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
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"

/// How the local player's creature reacts when a custom music track starts. The mood of the track is what the creature
/// felt about it before: 0 for a track it has not felt anything about, above 0 for a track it likes, below 0 for a track
/// it does not like. Pure, tested with made-up values.
namespace openblack::creature_music
{

enum class Reaction : uint8_t
{
	/// A new track: it is curious
	New,
	/// A track it likes: it is happy and dances
	Liked,
	/// A track it does not like: it is sad, frightened or angry
	Disliked,
};
[[nodiscard]] Reaction ReactionTo(float mood);

/// The plan that the creature is made to do as an order. The plan's own steps are replaced by the reaction's steps.
struct Plan
{
	creature_desires::Desire desire {creature_desires::Desire::IdleWithPlayer};
	std::string_view action;
};
/// It looks at the camera one time in three, else it points at the camera; for a track it does not like, it is sad. The
/// roll is from 0 to 2.
[[nodiscard]] Plan PlanFor(float mood, uint32_t roll);

/// Where the camera is, for the steps that turn to it and point at it. With no camera, the creature points at nothing.
struct Camera
{
	glm::vec3 position {0.0f};
	bool known {false};
};

/// The steps for a new track. The alignment is the creature's, from -1 (evil) to 1 (good).
[[nodiscard]] std::vector<creature_mind::Step> NewTrack(const creature_mind::Random& random, float alignment,
                                                        const Camera& camera);
/// The steps for a track that it likes
[[nodiscard]] std::vector<creature_mind::Step> LikedTrack(const creature_mind::Random& random, float mood,
                                                          const Camera& camera);
/// The steps for a track that it does not like
[[nodiscard]] std::vector<creature_mind::Step> DislikedTrack(const creature_mind::Random& random, float alignment,
                                                             const Camera& camera);

/// The number kept for what the creature does next: ten times the mood, the fraction cut off
[[nodiscard]] int32_t FollowUpValue(float mood);
/// After a track that it likes, it shows that it is happy one time in five, else it dances. The roll is from 0 to 99.
[[nodiscard]] bool ShowsHappyAfterLikedTrack(uint32_t roll);
/// When it dances after a track it likes very much (a number above 8), one time in eight it goes to dance with villagers
[[nodiscard]] bool MayDanceWithVillagers(int32_t value);

/// The help message raised for a track it likes and for a track it does not like, in the miscellaneous messages. A new
/// track raises none. The game also has a text for frightening music, but nothing tells it.
constexpr uint32_t k_NiceMusicHelp = 0;
constexpr uint32_t k_NastyMusicHelp = 1;

/// The desire made dominant after the reaction, and for how many seconds
struct Dominant
{
	creature_desires::Desire desire {creature_desires::Desire::IdleWithPlayer};
	float seconds {0.0f};
};
/// For a track it does not like, the roll (0 to 99) picks sadness or fear
[[nodiscard]] Dominant DominantAfter(Reaction reaction, uint32_t roll);

} // namespace openblack::creature_music
