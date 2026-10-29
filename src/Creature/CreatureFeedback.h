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

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/X87.h"
#include "Common/Zoomer.h"
#include "Creature/CreatureFace.h"

/// How the player's hand rewards and punishes a creature. The hand can't pick a creature up: clicking one holds the
/// hand to it instead. Held still over its body, the hand strokes it, and the creature plays a pleased animation for
/// the part of the body stroked. Swept fast across it, the hand slaps it, high or low and hard or gently, and it reels.
/// Each stroke and slap adds to a running sum, which the creature's mind is told of as the hand lets go.
namespace openblack::creature_feedback
{
/// The parts of the body a stroke can land on, each nearest one of the body's bones
enum class BodyPart : uint8_t
{
	Head,
	RightArmpit,
	LeftArmpit,
	Belly,
	Groin,
	RightFoot,
	LeftFoot,
	RightHand,
	LeftHand,
};
constexpr size_t k_BodyPartCount = 9;

/// For each part stroked: the pleased animation, whether it plays mirrored (the left side's are the right's mirrored),
/// and the face pulled for two seconds: an aah for the head and belly, an ooh for the groin and a smile elsewhere
constexpr std::array<size_t, k_BodyPartCount> k_RewardAnimations {154, 155, 155, 157, 158, 159, 159, 161, 161};
constexpr std::array<bool, k_BodyPartCount> k_RewardMirrored {false, false, true, false, false, false, true, false, true};
constexpr std::array<creature_face::Face, k_BodyPartCount> k_RewardFaces {
    creature_face::Face::Aah,   creature_face::Face::Smile, creature_face::Face::Smile,
    creature_face::Face::Aah,   creature_face::Face::Ooh,   creature_face::Face::Smile,
    creature_face::Face::Smile, creature_face::Face::Smile, creature_face::Face::Smile,
};
constexpr float k_RewardFaceMs = 2000.0f;
/// The face pulled being stroked on a part
[[nodiscard]] constexpr creature_face::Request RewardFace(BodyPart part)
{
	return {.face = k_RewardFaces.at(static_cast<size_t>(part)),
	        .milliseconds = k_RewardFaceMs,
	        .cue = creature_face::Cue::Stroked};
}

/// The part nearest a point the hand touches, given where each part's bone is, in part order
[[nodiscard]] BodyPart NearestPart(const glm::vec3& touch, std::span<const glm::vec3, k_BodyPartCount> parts);

/// The clocks a stroke waits on, in milliseconds
struct StrokeClocks
{
	/// Since the last stroke or slap played, or since taking hold
	float sinceTouch {0.0f};
	/// Starts at 0 on taking hold and is set to k_ReadyAfterTouchMs after each stroke or slap that plays: a stroke needs
	/// it at k_ReadyForStrokeMs, so the first comes two seconds after taking hold and the next a second after the last
	float ready {0.0f};
	/// How long the hand has been on the body, in all, since taking hold or the last slap
	float contact {0.0f};
	/// Since taking hold. A slap needs k_SlapReadyMs of it. Strokes and slaps do not start it again.
	float sinceHold {0.0f};
};
constexpr float k_ReadyForStrokeMs = 2000.0f;
constexpr float k_ReadyAfterTouchMs = 1000.0f;
/// The hand has to have been on the body this long, in all, before it strokes, and the cursor must be moving across
constexpr float k_StrokeHoldMs = 1000.0f;
/// After a stroke or slap the next stroke waits this long at no reward, twice that at the most punishment, nothing at the
/// most reward
constexpr float k_StrokeWaitMs = 1250.0f;
[[nodiscard]] float StrokeWaitMs(float sum);
/// Whether a stroke is due on a part: another part than the last stroked, the clocks run long enough for the reward so far
[[nodiscard]] bool StrokeDue(const StrokeClocks& clocks, std::optional<BodyPart> last, BodyPart part, float sum);

/// A stroke interrupts no body action that is this far through or less. The game keeps this limit as a double.
constexpr double k_StrokeInterruptsAfter = 0.8;
/// A slap interrupts no body action that is this far through or less. The game keeps this limit as a float.
constexpr double k_SlapInterruptsAfter = static_cast<double>(0.35f);
/// Whether the body refuses an action that the hand forces on it. The share is how far the body action is through its
/// animation. The body refuses the action when the share is equal to the limit or less.
[[nodiscard]] constexpr bool RefusesInterrupt(float share, double limit)
{
	return x87::LessEqual(share, limit);
}
/// Whether the body takes an action that the hand forces on it. An action on an object plays in place of the body
/// layer, so its share comes first. A body that plays no action takes the forced action.
[[nodiscard]] constexpr bool TakesForcedAction(std::optional<float> objectActionShare, std::optional<float> bodyShare,
                                               double limit)
{
	const auto share = objectActionShare.has_value() ? objectActionShare : bodyShare;
	return !share.has_value() || !RefusesInterrupt(*share, limit);
}
/// A slap needs this long since the hand took hold of the creature. Strokes and slaps do not start this time again.
constexpr float k_SlapReadyMs = 1000.0f;
/// Set the clocks after a stroke or a slap that plays. The time since the hand took hold does not change.
void Touched(StrokeClocks& clocks);
/// Whether the hand has held the creature long enough for a slap
[[nodiscard]] bool SlapReady(const StrokeClocks& clocks);

/// The hand's sweep is weighed as a slap only as it first comes onto the body, or as the cursor crosses the middle of the
/// screen, on the body or not
[[nodiscard]] bool SlapConsidered(bool onBody, bool wasOnBody, float cursorX, float lastCursorX, float screenMiddle);

/// The slapping animations, from the head to the feet, and their gentle versions, all to the right side
constexpr size_t k_SlapHead = 165;
constexpr size_t k_SlapWaist = 167;
constexpr size_t k_SlapFeet = 169;
constexpr size_t k_GentleOffset = 6;
/// Heights up the creature, as shares of its height, below which a slap is at the feet, then at the waist; above that
/// at the head, and nothing at or above the last
constexpr float k_FeetBelow = 0.4f;
constexpr float k_WaistBelow = 0.7f;
constexpr float k_SlapAbove = 1.1f;
/// The hand slaps when it moves faster than this many of the creature's heights a second, gently below the second
constexpr float k_SlapSpeed = 5.0f;
constexpr float k_HardSlapSpeed = 9.0f;

enum class SlapBand : uint8_t
{
	Feet,
	Waist,
	Head,
};

struct Slap
{
	size_t animation;
	SlapBand band;
	bool gentle;
	/// Played to the left side, when the hand swept to the left across the screen
	bool mirrored;
};

/// The slap a hand moving across the body makes, if any: by its height above the ground and its speed across the land,
/// against the creature's height, and which way it moves across the screen
[[nodiscard]] std::optional<Slap> ClassifySlap(float handHeight, float speed, float creatureHeight, bool sweepsLeft);

/// Hard slaps are counted at one height; the third in a row there that lands on the body bruises the creature where it
/// lands and the count starts again. A slap at another height starts the count afresh.
struct HardSlaps
{
	SlapBand band {SlapBand::Feet};
	uint32_t count {0};
};
constexpr uint32_t k_HardSlapsToBruise = 3;
/// Counts a slap; whether it bruises
[[nodiscard]] bool CountSlap(HardSlaps& slaps, const Slap& slap, bool onBody);
/// The bruise's column in the skin's marks: 7 for a hard slap just past the hard speed, less the faster, down to 0
[[nodiscard]] uint8_t BruiseColumn(float speed, float creatureHeight);
/// A bruise takes this much of the creature's life, never leaving less
constexpr float k_BruiseLife = 0.01f;
[[nodiscard]] float LifeAfterBruise(float life);

/// The hand's cycles while it is held to a creature
enum class TouchCycle : uint8_t
{
	/// Beside the body
	Wiggle,
	/// The caress, which taking hold starts with and stroking plays
	Tickle,
	Slap,
};
/// The cycle after a step: a fast hand slaps, stroking caresses, and off the body a caress turns back to the wiggle; a
/// slapping hand keeps slapping until it strokes again
[[nodiscard]] TouchCycle NextCycle(TouchCycle cycle, bool fast, bool stroking, bool onBody);

/// What each stroke and slap adds to the running sum of the player's feedback, which stays from -1 to 1
constexpr float k_StrokeAmount = 0.1f;
constexpr float k_GentleSlapAmount = 0.1f;
constexpr float k_HardSlapAmount = 0.2f;
/// Slapping a creature that was enjoying being stroked, the sum past this, takes twice as much off
constexpr float k_EnjoyingSum = 0.25f;

[[nodiscard]] float AfterStroke(float sum);
[[nodiscard]] float AfterSlap(float sum, bool gentle);
/// What the mind is told when the hand lets go
[[nodiscard]] float Delivered(float sum);
/// Feedback this slight only makes the creature look at the player
constexpr float k_SlightFeedback = 0.01f;

/// The body of the creature at the time that the hand lets go of it
struct ReleaseBody
{
	/// The head looks straight ahead: it turned back fully on the two axes
	bool headStraight {false};
	/// The body plays an action or moves
	bool bodyBusy {false};
	/// The body lies dead and holds the last frame of its fall
	bool lyingDead {false};
	/// The face shows an expression, or it is not yet back from one
	bool faceShown {false};
};
/// The hand tells the mind how it treated the creature only when the creature is not still at the release. The
/// creature is still when its head looks straight ahead, its face shows nothing, and its body is idle or lies dead.
[[nodiscard]] bool ReleaseGivesFeedback(const ReleaseBody& body);
/// The feedback of a release reaches the mind after this number of turns. In the first turn after the release, the
/// creature is free, and the plan that it had during the hold starts again. The feedback comes at the start of the
/// next turn.
constexpr uint32_t k_ReleaseFeedbackTurns = 1;

/// How the creature's attitude to the player follows feedback: mostly what it was, nudged by what it got
[[nodiscard]] float AttitudeAfter(float attitude, float feedback);
/// The average of the feedback it has had, the newest counting most
[[nodiscard]] float AverageAfter(float average, float feedback);

/// A piece of the body the hand can touch: a capsule round the line from one bone to the next
struct Capsule
{
	glm::vec3 from;
	glm::vec3 to;
	float radius;
};
/// A creature of size 1 is about this many units tall
constexpr float k_HeightAtSizeOne = 15.0f;
/// The body is taken as capsules round its bones this thick, as shares of its height
constexpr float k_BodyRadiusShare = 0.12f;
/// The body as posed: a capsule from each bone to its parent, the bones' matrices in the mesh's space placed in the world
[[nodiscard]] std::vector<Capsule> BodyCapsules(std::span<const uint32_t> parents, std::span<const glm::mat4> boneMatrices,
                                                const glm::mat4& placement, float radius);
/// How far a point is outside the body, or less than 0 inside it
[[nodiscard]] float DistanceOutside(const glm::vec3& point, std::span<const Capsule> capsules);
/// Where a line of sight first touches the body, as how many of the direction's lengths along it, if it does
[[nodiscard]] std::optional<float> RayHit(const glm::vec3& origin, const glm::vec3& direction,
                                          std::span<const Capsule> capsules);

/// Held to a creature, the hand stays on the line of sight through the cursor. Its distance from the camera eases to a
/// point a little short of the place where the line meets the creature's posed mesh.
/// The hand stays nearer to the camera than the creature's middle by this share of the creature's height
constexpr float k_HandKeepsBackShare = 0.25f;
/// On the body, the hand rests short of the mesh by this share of the height, and it eases there in this time
constexpr float k_HandRestsShortShare = 0.055f;
constexpr float k_HandRestSeconds = 0.3f;
/// When the hand is deeper than this share of the height before its rest, it jumps to that place
constexpr float k_HandJumpsShortShare = 0.03f;
/// When the hand takes hold, it is short of the creature's position by this distance for each unit of size
constexpr float k_HandStartsShortPerSize = 0.825f;
/// The hand comes no nearer to the camera than this distance, and goes no farther than its reach
constexpr float k_HandNearestToCamera = 2.0f;
constexpr float k_HandReach = 1800.0f;
/// The distance of the hand from the camera when it takes hold of a creature
[[nodiscard]] float HandStartDistance(float cameraToCreature, float size);
/// Keep the hand nearer to the camera than the creature's middle. A hand that is farther jumps there at once.
void KeepHandBack(Zoomer& distance, float cameraToMiddle, float height);
/// Send the hand to its rest a little short of the body. A hand that is too deep first jumps nearer to the camera.
void RestHandOnBody(Zoomer& distance, float cameraToTouch, float height);
/// The distance at which the hand shows on the line of sight
[[nodiscard]] float HandDistanceFromCamera(const Zoomer& distance);
/// When the line through the cursor does not touch the body, the hand tries again through the middle of the cursor's
/// places in this frame and in the last frame, in whole pixels
[[nodiscard]] glm::ivec2 SecondTryCursor(glm::ivec2 cursor, glm::ivec2 lastCursor);

/// Held to a creature, the hand's up eases round: taking hold it starts pointing from the creature towards the camera; on
/// the body, as the cursor moves across the screen, it turns to the body's surface tipped towards the camera over 0.9
/// seconds; off the body it stands back up over 2.5 seconds; resting still on the body it keeps turning as it was.
constexpr float k_HandStartSeconds = 1.0f;
constexpr float k_HandTurnsToBodySeconds = 0.9f;
constexpr float k_HandStandsUpSeconds = 2.5f;
/// How much the camera's way counts beside the body's surface in the hand's up on the body
constexpr float k_HandTowardsCamera = 0.4f;
struct HandUpGlide
{
	glm::vec3 destination;
	float seconds;
};
/// The hand's up when it takes hold of the creature
[[nodiscard]] glm::vec3 HandUpOnTakingHold(const glm::vec3& creaturePosition, const glm::vec3& camera);
/// Where the hand's up is sent this frame, if anywhere. normal is the way out of the body where the hand touches it.
[[nodiscard]] std::optional<HandUpGlide> HandUpGlideTo(bool onBody, bool cursorMoved, const glm::vec3& normal,
                                                       const glm::vec3& creaturePosition, const glm::vec3& camera);

/// The point on the line of sight at a distance from the camera. The game divides the distance by the length of the
/// way one time and multiplies each part of the way by that share. With no way, the point is the camera.
[[nodiscard]] glm::vec3 PointOnSight(const glm::vec3& camera, const glm::vec3& lineOfSight, float distance);
/// The way of the line of sight as the game uses it to find the creature's skin: the point one unit along the line,
/// less the camera. The two subtractions can make it differ from the unit way in the last bits.
[[nodiscard]] glm::vec3 SightWay(const glm::vec3& camera, const glm::vec3& lineOfSight);

/// Held by the hand, the creature looks at the hand, at a point a little nearer the camera than it; with the hand near
/// its middle it looks at the camera instead. Shares of its height.
constexpr float k_LooksAtCameraWithin = 0.6f;
constexpr float k_LooksNearerTheCamera = 0.3f;
/// lineOfSight is the way from the camera through the cursor
[[nodiscard]] glm::vec3 HeldLookPoint(const glm::vec3& hand, const glm::vec3& middle, const glm::vec3& camera,
                                      const glm::vec3& lineOfSight, float creatureHeight);
/// The look point of HeldLookPoint from the hand and the camera that a player reported. The hand is on the line of
/// sight through the cursor, so the line from the camera to the hand gives the way of the line of sight. None when the
/// hand is at the camera.
[[nodiscard]] std::optional<glm::vec3> ReportedLookPoint(const glm::vec3& hand, const glm::vec3& middle,
                                                         const glm::vec3& camera, float creatureHeight);
} // namespace openblack::creature_feedback
