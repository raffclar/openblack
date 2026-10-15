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

#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureIdleMind.h"

/// The emotes of a creature and the rules that let it do them: the howl, being sad, silly faces, being pathetic, running
/// away, stroking a hurt thing to heal it, looking at the sun or the moon, sleeping on the spot and sitting down. It
/// also keeps the rules that tell if the creature did an action too recently to do it again.
namespace openblack::creature_emote
{
using creature_mind::Random;
using creature_mind::Step;

/// The clips of the creature's animation set that these agendas play
constexpr size_t k_HowlAnimation = 217;
constexpr size_t k_CryAnimation = 219;
constexpr size_t k_PickMeAnimation = 74;
constexpr size_t k_FrightenedAnimation = 61;
constexpr size_t k_StrokeAnimation = 100;
constexpr size_t k_PutDownAnimation = 97;

/// Howling at the player: turning to face the camera with the head on it, then the howl. No face is pulled.
[[nodiscard]] std::vector<Step> HowlAtPlayer(glm::vec2 camera);

/// Being sad: waiting two seconds and up to three more, in whole turns, with a sad face and the head down at its own
/// feet; then, in equal shares, the sad clip or the cry
[[nodiscard]] std::vector<Step> BeSad(uint32_t self, float turnsPerSecond, const Random& random);
constexpr float k_SadWaitSeconds = 2.0f;
constexpr float k_SadWaitExtraSeconds = 3.0f;

/// Pulling silly faces at a villager: going near it (two times its own height away), turning to face it for a tenth of
/// a second, two times in three the playful clip, then three faces picked at random from the first ten, one each turn,
/// each held for five seconds. The head looks at the villager from the turn to the end. The second face completes the
/// action.
[[nodiscard]] std::vector<Step> PullSillyFaces(uint32_t villager, float height, const Random& random);
constexpr float k_SillyFacesHeights = 2.0f;
constexpr float k_SillyFacesFaceSeconds = 0.1f;
constexpr uint32_t k_SillyFacesPlayLots = 3;
constexpr uint32_t k_SillyFaceCount = 10;
constexpr float k_SillyFaceSeconds = 5.0f;

/// Being pathetic to the player: following the camera's point of focus, turning to face the camera, then the pick-me
/// clip. The head looks at the camera while it follows and turns. No face is pulled.
[[nodiscard]] std::vector<Step> BePathetic(glm::vec2 camera, float height);
/// Following the camera lasts thirty seconds at most. The creature arrives within twice one more than its height.
constexpr float k_FollowCameraSeconds = 30.0f;
[[nodiscard]] float FollowCameraReach(float height);
/// Walking to the point of focus, it stops this much nearer than the reach
constexpr float k_FollowCameraStopShort = 2.0f;

/// What a creature that follows the camera does on one turn
struct FollowCameraView
{
	/// The point of focus of its player's camera, on the land; none without a player
	std::optional<glm::vec3> focus;
	glm::vec2 position {0.0f};
	bool moving {false};
	/// While it moves: whether the point of focus is less than a sixth of a turn from the way its body faces, or in its
	/// own map cell
	bool focusAhead {false};
};
enum class FollowCameraAction : uint8_t
{
	/// The step is done
	Done,
	/// It keeps on as it goes
	Continue,
	/// It walks to the point of focus again
	MoveAgain,
};
/// One turn of following the camera. With no player, or with its time up, it is done. Standing, it is done once it is
/// within the reach, or when the point of focus is at the map's origin, else it walks again. Moving, it walks again
/// when the point of focus is no longer ahead of it.
[[nodiscard]] FollowCameraAction FollowCameraTurn(const FollowCameraView& view, uint32_t& turnsLeft, float reach);

/// What a creature runs away from
struct RunFrom
{
	/// Where the thing or the player's hand is as the run starts
	glm::vec2 point {0.0f};
	/// The thing, none for the player's hand
	std::optional<uint32_t> object;
};
/// Running away: one time in two a frightened start with wide eyes, then the run with a frightened face, looking back
/// at what it runs from in turns, then one time in two another frightened start. A run from a thing also opens the
/// eyes wide. The run completes the action.
[[nodiscard]] std::vector<Step> RunAway(const RunFrom& from, const Random& random);
constexpr uint32_t k_FrightenedStartLots = 2;

/// The creature looks back at what it runs from for one time, then looks about for another, and so on. The state stays
/// with the creature from one run to the next.
struct RunLookBack
{
	bool lookingBack {false};
	uint32_t turns {0};
};
/// One turn of a run: whether the head looks back at what it runs from on this turn. The turn that a time ends on still
/// looks the old way.
[[nodiscard]] bool RunLooksBack(RunLookBack& state, float turnsPerSecond, float backSeconds, float aheadSeconds);

/// Stroking a hurt thing: picking it up unless it holds something it can stroke already, stroking what it holds (which
/// completes the action), healing the thing, then putting down what it holds. The face is of compassion all along.
[[nodiscard]] std::vector<Step> StrokeAndHeal(uint32_t thing, bool holdingStrokeable);

/// Looking at the sun or the moon: turning to face the point, with the head on it, then one time in two pointing at it
/// for three seconds. An evil creature looking at the moon then shows its anger, amazed.
[[nodiscard]] std::vector<Step> LookAtSky(glm::vec3 point, bool evilAtTheMoon, const Random& random);
/// The sun stands far off to this point of the land
constexpr glm::vec3 k_SunPoint {-50000.0f, 0.0f, -50000.0f};
constexpr float k_PointAtSkySeconds = 3.0f;
constexpr uint32_t k_PointAtSkyLots = 2;
constexpr size_t k_AngryAnimation = 53;

/// Sleeping on the spot at a clear place near the creature: one time in two a sleepy yawn, walking to the place until
/// within its radius on the ground, sleeping until rested, and getting up dazed with a grimace. It does not face down
/// the slope.
[[nodiscard]] std::vector<Step> SleepAtClearPlace(glm::vec2 place, float radius, const Random& random);
/// The clear place is as wide as four times the creature's height
constexpr float k_SleepOnTheSpotHeights = 4.0f;

/// Sitting down by a thing: walking to the place until within its radius on the ground, turning to face the thing for
/// a tenth of a second with the head on it, facing down the slope, then sitting ten seconds and up to five more while
/// it looks about
[[nodiscard]] std::vector<Step> SitDownBy(uint32_t thing, glm::vec2 place, float radius, const Random& random);
/// Left to itself, it looks for a clear place as wide as four times its radius on the ground about the thing
constexpr float k_SitDownRadii = 4.0f;
constexpr float k_SitFaceSeconds = 0.1f;
constexpr float k_SitSeconds = 10.0f;
constexpr float k_SitExtraSeconds = 5.0f;

/// A random fraction of a whole number of seconds, as the game draws it, from 0 to the most
[[nodiscard]] float FloatRandom(const Random& random, float most);
/// Seconds as whole turns of the game, back in seconds
[[nodiscard]] float WholeTurnSeconds(float seconds, float turnsPerSecond);

/// The rules that let a creature choose an action
/// The sun shows by day, the moon by night
[[nodiscard]] constexpr bool SunVisible(bool night)
{
	return !night;
}
[[nodiscard]] constexpr bool MoonVisible(bool night)
{
	return night;
}
/// It sleeps at home only when its player has a temple and it is less than 1000 from home
constexpr float k_FurthestToSleepAtHome = 1000.0f;
[[nodiscard]] bool NotTooFarFromHome(std::optional<float> homeDistance);
/// It sleeps on the spot or by a thing only on a leash, with no temple, or more than 140 from home
constexpr float k_NearestToSleepAway = 140.0f;
[[nodiscard]] bool LeashedOrTooFarFromHome(bool leashed, std::optional<float> homeDistance);
/// It runs from the player only when its player's hand is less than 10 from it and it dislikes its player
constexpr float k_FrighteningHandDistance = 10.0f;
[[nodiscard]] bool PlayerNearbyAndFrightening(std::optional<float> handDistance, float attitudeToPlayer);

/// Whether it did an action within some seconds: the turns since it last did it, counted in whole seconds, are fewer,
/// and not none
[[nodiscard]] bool DoneWithin(uint32_t turnsSinceDone, uint32_t turnsPerSecond, float seconds);
/// It does not scratch or sit again within two seconds of the last time
constexpr float k_RecentlySeconds = 2.0f;
/// It does not show off again within 200 turns of the last time that it did so to its end
constexpr uint32_t k_ShowOffAgainTurns = 200;
[[nodiscard]] bool ShowedOffRecently(uint32_t lastShowOffTurn, uint32_t turn);
} // namespace openblack::creature_emote
