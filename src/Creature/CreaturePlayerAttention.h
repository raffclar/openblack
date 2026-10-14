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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

/// The rules of the actions a creature does with its player when it wants attention or wants to tell the player what it
/// thinks: when it can do them, and what it does on each turn of the steps that follow the hand and watch the player.
/// Pure, tested on made-up values. The agendas are in CreaturePlayerAgendas.h.
namespace openblack::creature_player_attention
{
/// The creature is cross with the player when its attitude to the player is less than this
constexpr float k_CrossBelow = -0.1f;
/// The creature is away from the hand when the hand is further than this part of the camera's view width at the focus
constexpr float k_AwayFromHandShare = 0.125f;
/// The half angle of the camera's view that gives that width, in radians
constexpr double k_ViewHalfAngle = 0.875;
/// The hand's speed follows its movement by this part each turn
constexpr float k_HandSpeedFollow = 0.6f;
/// The creature follows the hand for this many seconds at most, and stops this far past its own radius from the hand
constexpr float k_FollowHandSeconds = 30.0f;
constexpr float k_FollowHandReach = 4.0f;
/// The creature watches the hand when the hand moved more than this lately, else the camera
constexpr float k_WatchHandMovement = 30.0f;
/// A creature that watches the player turns when the point is more than this far away and more than a quarter turn
/// round from its heading
constexpr float k_WatchTurnDistance = 0.1f;
/// The direction to a point is near the heading when it is less than this part of a half turn round from it
constexpr uint32_t k_CloseDirectionParts = 6;
/// The clips of these actions, by their numbers in the creature's animation set
constexpr size_t k_HappyAnimation = 55;
constexpr size_t k_SadAnimation = 56;
constexpr size_t k_FriendlyWaveAnimation = 72;
constexpr size_t k_PrayAnimation = 221;
/// Going to the hand, it plays the happy clip at the end one time in this many
constexpr uint32_t k_HappyAtHandLots = 2;
/// Pointing at the hand lasts this many seconds
constexpr float k_PointAtHandSeconds = 1.0f;

/// Whether the creature is cross with its player
[[nodiscard]] bool IsCross(float attitudeToPlayer);

/// Whether a creature is away from its player's hand: the hand is further from it along the ground than an eighth of
/// the camera's view width at the focus. The two positions are map positions first. A creature without a player, or a
/// player without a hand, is never away from the hand.
[[nodiscard]] bool AwayFromHand(glm::vec3 creature, glm::vec3 hand, glm::vec3 cameraOrigin, glm::vec3 cameraFocus);

/// The speed of the player's hand: it follows the movement of each turn, made a speed, by a part each turn. Thus it
/// stays more than 0 for many turns after the hand stops.
struct HandSpeed
{
	glm::vec3 velocity {0.0f};
};
/// One turn of the hand's speed: the hand moved this much in the turn, and the game does this many turns a second
void UpdateHandSpeed(HandSpeed& speed, glm::vec3 movement, float turnsPerSecond);
/// Whether the hand moves, for the look at the hand: its speed is more than 0
[[nodiscard]] bool IsHandMoving(const HandSpeed& speed);

/// The game angle of a heading direction on the ground, 0 along +x and a quarter turn along +z
[[nodiscard]] uint16_t GameAngleOf(glm::vec2 direction);
/// Whether the direction from a creature to a point is near its heading: the two game angles are less than a sixth of a
/// half turn apart, measured straight and not round the circle; or the point is in the creature's own map cell
[[nodiscard]] bool IsDirectionClose(const map_coords::MapCoords& creature, const map_coords::MapCoords& point,
                                    uint16_t heading);

/// The turns a creature follows the hand for, from the whole turns a second
[[nodiscard]] uint32_t FollowHandTurns(uint32_t turnsPerSecond);
/// Where the creature stops from the hand
[[nodiscard]] float FollowHandReach(float radius);

/// What a creature that follows the hand sees on one turn
struct FollowHandView
{
	/// The creature has a player, and the hand is on the map
	bool hasPlayer {false};
	bool handInMap {false};
	/// Where the hand is on the ground, how far it is along the ground in metres, and where the creature stops from it
	glm::vec2 hand {0.0f};
	float distance {0.0f};
	float reach {0.0f};
	/// The body plays an action, and the hand is near the creature's heading
	bool bodyPlaying {false};
	bool directionClose {false};
};
/// What a creature that follows the hand does on one turn
enum class FollowHandAction : uint8_t
{
	/// The step is done: no player, the time is up, or it is near the hand
	Done,
	/// The hand is off the map: the rest of the agenda is no use
	Fail,
	/// It goes on as it is
	Continue,
	/// It sets off to the hand again
	WalkToHand,
};
/// One turn of following the hand. Each turn uses up one of the turns left. While the body plays an action it walks again
/// only when the hand is not near its heading.
[[nodiscard]] FollowHandAction FollowHandTurn(const FollowHandView& view, uint32_t& turnsLeft);

/// What a creature that watches the player sees on one turn
struct WatchPlayerView
{
	/// It waits for orders after it was told to go somewhere, and it has a player
	bool waitingForOrders {false};
	bool hasPlayer {false};
	bool bodyPlaying {false};
	/// How much the hand moved lately
	float handMovement {0.0f};
	/// The hand and the camera as the creature sees them: how far each is, and how far round from its heading (radians,
	/// either way)
	struct Target
	{
		glm::vec2 point {0.0f};
		float distance {0.0f};
		float turn {0.0f};
	};
	std::optional<Target> hand;
	std::optional<Target> camera;
};
/// What a creature that watches the player does on one turn
struct WatchPlayerAction
{
	bool done {false};
	/// A point to turn to
	std::optional<glm::vec2> turnTo;
};
/// One turn of watching the player. It goes on while it waits for orders. It turns to the hand when the hand moved much
/// lately, else to the camera, when that is far enough away and more than a quarter turn round.
[[nodiscard]] WatchPlayerAction WatchPlayerTurn(const WatchPlayerView& view);

} // namespace openblack::creature_player_attention
