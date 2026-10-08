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

#include <glm/vec3.hpp>

#include "ECS/Weather/Storms.h"

// The storm miracle's particle systems (SF_LightningStormPush and SF_StormCast): the cloud cores drift with the wind
// (UR_CloudMoverNew), the clouds gather round each core and register a storm that rains on the land (UR_CloudGather),
// fork lightning strikes from random clouds (UR_Lightning of Rules/Lightning.cpp in its parent mode) and, at power-up
// level 1, a tornado sucks up objects and piles (UR_Tornado). UR_StormCast is the swirl at the hand while casting.
// See docs/bw1-notes/miracles.md, "Tormenta".

namespace openblack::psys::storm
{

// ---- the formulas, exposed for the tests ----

/// What UR_CloudGather registers. `radius` is the RadiusFloatProvider value (1.2 x magnitude), `magnitude` the
/// manager's, `power` the process strength, `heading` the manager heading at the first update ((cameraForward.x, 0,
/// cameraForward.z), not normalised), `rain` the storm spell's rain amount (or < 0 without a storm spell: 100).
struct GatherStormInput
{
	float radius {0.0f};
	float magnitude {0.0f};
	float power {1.0f};
	glm::vec3 heading {0.0f};
	float rainAmount {-1.0f};
	bool rainOn {true};
	float timeToForm {10.0f};
	float cloudHeight {0.0f}; ///< the CloudHeight float provider value
	float windMinSpeed {40.0f};
	float windMaxSpeed {100.0f};
	float magnitudeForWindMinSpeed {20.0f};
	float magnitudeForWindMaxSpeed {100.0f};
};
[[nodiscard]] weather::storms::StormDescriptor GatherStormDescriptor(const GatherStormInput& input);

/// UR_Tornado: the funnel radius at the height fraction h (clamped 0..1),
/// (BaseRadius + (TopRadius - BaseRadius) h^2) x tornadoScale
[[nodiscard]] float FunnelRadius(float h, float baseRadius, float topRadius, float tornadoScale);
/// Schlick's bias: x^(ln b / ln 0.5)
[[nodiscard]] float Bias(float b, float x);
/// Schlick's gain: x < 0.5 ? bias(1 - g, 2x) / 2 : 1 - bias(1 - g, 2 - 2x) / 2
[[nodiscard]] float Gain(float g, float x);
/// The midpoint (RK2) rate of d rho / dt = -0.5 (rho - r), the flying atoms' pull towards the
/// funnel wall
[[nodiscard]] float RadialRate(float r, float rho, float dt);

/// UR_CloudGather, one cloud at its formed fraction f = age / TimeToForm
struct CloudLook
{
	float grow;  ///< FracToMaxSize ramp: f / Frac, then 1 - (f - Frac) / (1 - Frac)
	float ratio; ///< MaxCloudRatio + (MinCloudRatio - MaxCloudRatio) f
	int colour;  ///< trunc(MaxColor + (MinColor - MaxColor) f)
	int alpha;   ///< trunc(MinAlpha + (MaxAlpha - MinAlpha) grow)
	float scale; ///< (MinScaleFactor + (MaxScaleFactor - MinScaleFactor) grow) x the CloudScale float provider
};
struct CloudLookParams
{
	float fracToMaxSize {0.5f};
	float minCloudRatio {1.0f}, maxCloudRatio {5.0f};
	int minColor {50}, maxColor {255};
	int minAlpha {100}, maxAlpha {100};
	float minScaleFactor {0.0f}, maxScaleFactor {1.0f};
	float scaleProvider {1.0f};
};
[[nodiscard]] CloudLook CloudLookAt(float f, const CloudLookParams& params);

/// UR_CloudGather: the collection-age smoothstep s = (3 - 2t) t^2 with t = clamp(age x 0.1, 0, 1);
/// returns c + (1 - c) s (CloudRatioMaxCollection or CollectionRadiusInitialScale as c)
[[nodiscard]] float CollectionScale(float collectionAge, float c);

// ---- the game side ----

/// Every frame (magic::Update): the objects the tornados carry follow their atoms (each atom's draw matrix is copied
/// into its object's 3D object)
void UpdateCarriedObjects();
/// How many strikes the clouds have made since the program started (the test hooks)
[[nodiscard]] size_t StrikeCount();
/// How many objects the tornados carry now (the traces and tests)
[[nodiscard]] size_t CarriedObjectCount();
/// OPENBLACK_STORM_TRACE (read once)
[[nodiscard]] bool TraceEnabled();

} // namespace openblack::psys::storm
