/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// The game lights its models with a single point light and an ambient level, per vertex and in whole numbers: a vertex
/// facing the light at an intensity I = round(255 n.l) has its colour times f / 256, where f is the ambient when it
/// faces away and ambient + (255 - ambient) I / 256 when it faces it. The light is a distant sun by day; in the darker
/// half of the night it moves next to the god hand, a little towards the camera. Inside the temple it stands off to one
/// side of the rooms, up and away along -x. assets/shaders/model_light.sh is the shader side.
namespace openblack::model_light
{

/// The sun, far off at the same height as it is away along -x and -z
constexpr glm::vec3 k_Sun {-500000.0f, 500000.0f, -500000.0f};
/// The temple's light, from the temple's origin: everything drawn in its rooms is lit from here
constexpr glm::vec3 k_Temple {-1000.0f, 1000.0f, -500.0f};
/// The ambient level, of 255
constexpr float k_Ambient = 90.0f;

/// Where the light is this frame. `skyType` runs from 0 at night to 2 by day: at night the hand carries the light, never
/// below 10 over the ground under it. Inside the temple, whose origin is given then, the temple's light takes its place.
[[nodiscard]] glm::vec3 FrameLight(glm::vec3 hand, float groundUnderHand, const glm::vec3& camera, float skyType,
                                   std::optional<glm::vec3> templeOrigin);

/// The uniform the object shaders read: the light's position, and the ambient
[[nodiscard]] glm::vec4 Uniform(const glm::vec3& light, float ambient = k_Ambient);
/// The light's direction in the space of a model whose axes and origin in the world are given: from the origin, not
/// from the vertex, as the game takes it, and unit long; a mirrored space keeps the light on the right side. As
/// model_light.sh's ModelLightLocal.
[[nodiscard]] glm::vec3 LocalDirection(const glm::vec3& light, const glm::vec3& axisX, const glm::vec3& axisY,
                                       const glm::vec3& axisZ, const glm::vec3& origin);
/// The factor a vertex's colour is lit by, of 256, for its normal in the model's space and the light's local direction:
/// I = round(255 n.l), halves to even; the ambient when I < 0, else ambient + ((255 - ambient) I >> 8). As
/// model_light.sh's ModelLightFactor.
[[nodiscard]] float Factor(const glm::vec3& normal, const glm::vec3& localLight, float ambient);
/// The factor, of 256, the game's posed models (the hand, the advisors, the glow over a hand holding a miracle) are lit by
/// instead: 255 (1.3 n.l + 0.4) with the fraction dropped, from 0.4 when facing away up to 1. As model_light.sh's
/// PosedModelLightFactor.
[[nodiscard]] float PosedFactor(const glm::vec3& normal, const glm::vec3& localLight);

} // namespace openblack::model_light
