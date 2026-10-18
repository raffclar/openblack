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
#include <bit>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/GameHeading.h"
#include "Creature/CreatureLocomotion.h"

/// How a creature walks along its route, one game turn at a time, as the game does it.
///
/// When the route is ready, the creature looks at a point ahead along the route and starts in one of three ways:
/// 1. It steps off sideways or backwards into the walk, when the point is further than each of its three steps go.
/// 2. It walks straight on, when it nearly faces the point.
/// 3. Else it turns on the spot first.
///
/// While it walks, it speeds up to its top speed, slower uphill. It slows down so that it can stop at the end of the
/// route, and so that it can go round the next arc. A route that turns too far from the way that the creature faces
/// stops it, and it starts again from the turn. All the maths is in single precision, in the game's order.
namespace openblack::creature_walk_planner
{

/// The length of a game turn in seconds, and the speeding up and slowing down in units a second each second
constexpr float k_TurnSeconds = std::bit_cast<float>(0x3DCCCCCDu);
constexpr float k_Acceleration = 12.0f;
/// The slowest speed that the caps allow
constexpr float k_LeastCapSpeed = std::bit_cast<float>(0x3DCCCCCDu);
/// A route that turns this far from the way that a walking creature faces stops it. The game keeps 30 degrees as a
/// float.
constexpr float k_CornerTurn = std::bit_cast<float>(0x3F060A92u);
/// When it starts, a creature this nearly facing the point walks straight on. The game keeps 15 degrees as a float.
constexpr float k_StraightTurn = std::bit_cast<float>(0x3E860A92u);
/// The point that a creature looks at when it starts is never nearer than this
constexpr float k_LeastLookAhead = std::bit_cast<float>(0x3C23D70Au);
/// A creature slower than this does not change what its legs play
constexpr float k_StillSpeed = std::bit_cast<float>(0x38D1B717u);
/// The steps measure their move in units a turn: this many turns make a second
constexpr float k_TurnsPerSecond = 10.0f;

/// What the creature's walk does this turn
enum class Phase : uint8_t
{
	/// It turns on the spot
	Turn,
	/// It steps off into the walk
	Step,
	/// It walks along the route
	Walk,
};

/// One of the six step animations: the move of its root over the animation in mesh units, and its number of frames
struct StepClip
{
	glm::vec3 displacement;
	int32_t frames;
};
/// The 0, 90 and 180 degree steps of a side, when the species has all three
using StepSet = std::optional<std::array<StepClip, 3>>;

/// How the creature starts towards a point
struct Start
{
	Phase phase;
	/// The heading from the creature to the point, which a turn or a step turns to
	GameHeading heading;
	/// The side of the steps: the right steps when the point is on the right of the creature or straight ahead
	creature_locomotion::Side side;
	/// The pair of steps blended into one, and how long the step lasts. The step lasts as long as its move along the
	/// new heading takes at the creature's walking speed.
	std::optional<creature_locomotion::Pair> steps;
	glm::vec3 stepDisplacement {0.0f};
	int32_t stepMs {0};
};

/// The point that the creature looks at when it starts: along the route's heading from where it stands, as far as
/// the leg that it is on is long, and never nearer than k_LeastLookAhead
[[nodiscard]] glm::vec2 LookAhead(glm::vec2 position, GameHeading routeHeading, float legLength);

/// How the creature starts towards a point from where it stands. A species without the steps of the side always
/// turns on the spot first.
[[nodiscard]] Start ChooseStart(glm::vec2 from, glm::vec2 to, GameHeading heading, float scale, float walkSpeed,
                                const StepSet& rightSteps, const StepSet& leftSteps);

/// What comes after the leg that the creature walks on
struct NextLeg
{
	enum class Kind : uint8_t
	{
		/// It is on the last leg: it stops at the end
		None,
		Straight,
		/// An arc round a circle of this radius
		Arc,
	};
	Kind kind;
	float radius {0.0f};
};

/// The fastest that the creature may walk this turn. On the last leg it must be able to stop at the end. Before an
/// arc it must be able to slow to the speed that the arc allows. Before a straight leg only its top speed limits it.
/// Neither cap is slower than k_LeastCapSpeed.
[[nodiscard]] float SpeedCap(float topSpeed, float legLength, float travelled, const NextLeg& next);

/// The point the creature's radius ahead of it, where the slope is measured
[[nodiscard]] glm::vec2 SlopePoint(glm::vec2 position, GameHeading heading, float radius);

/// How the slope ahead changes the speed: slower uphill and a little faster downhill, from 0.3 to 1.1 times
[[nodiscard]] float SlopeFactor(float altitudeAhead, float altitude, float radius);

/// The speed after one turn: it speeds up towards the target, never past it, and the cap then limits it. It never
/// slows down towards a lower target.
[[nodiscard]] float NextSpeed(float speed, float target, float cap);

/// How far the creature goes along its route in one turn at a speed
[[nodiscard]] float TurnDistance(float speed);

/// Whether a turn of the route away from the way that the creature walks stops it
[[nodiscard]] bool WalkTurnsTooFar(GameHeading heading, GameHeading routeHeading);

/// Whether a turn of the route away from the heading of a step stops the step
[[nodiscard]] bool StepTurnsTooFar(GameHeading stepHeading, GameHeading routeHeading);

/// While it steps, the creature's heading turns with the route: by the turn of the route since the last turn
[[nodiscard]] GameHeading StepHeading(GameHeading heading, GameHeading lastRouteHeading, GameHeading routeHeading);

/// The speed of a step this turn: the part of the step's move that one turn plays, as a speed along the route
[[nodiscard]] float StepSpeed(const glm::vec3& displacement, int32_t turnMs, float scale, int32_t stepMs, GameHeading heading);

/// The walk animation's time when a step ends and the walk starts: the share of the step played, in the walk. After a
/// left step the walk starts half a cycle on.
[[nodiscard]] int32_t WalkTimeAfterStep(int32_t stepTimeMs, int32_t stepMs, int32_t walkMs, creature_locomotion::Side side);

/// An animation's length in milliseconds, and the distance that its root moves over it in mesh units
struct Clip
{
	int32_t durationMs;
	float stride;
};

/// A time on through a looped animation, once round at most and never at its end
[[nodiscard]] int32_t AdvanceLoop(int32_t timeMs, int32_t advanceMs, int32_t durationMs);

/// The time in the stand at the creature's point of its breath, never at its end
[[nodiscard]] int32_t StandTime(int32_t standMs, float breathPhase);

/// The legs' two animations: below walking speed the stand and the walk, at walking speed and above it the run and
/// the walk. Each has a weight and a time.
struct Gait
{
	std::array<size_t, 2> animations;
	std::array<float, 2> weights;
	std::array<int32_t, 2> timesMs;
};

/// Blends the gait by how fast the creature goes. The walk moves on by the distance covered over the blend's stride,
/// so the feet keep to the ground; the run keeps in step with the walk. The weights are not limited, so above running
/// speed the run is played past its full weight. A creature slower than k_StillSpeed keeps what its legs played.
[[nodiscard]] std::optional<Gait> BlendGait(float speed, float walkSpeed, float runSpeed, float distance, float scale,
                                            const Clip& stand, const Clip& walk, const Clip& run, int32_t walkTimeMs,
                                            float breathPhase);

} // namespace openblack::creature_walk_planner
