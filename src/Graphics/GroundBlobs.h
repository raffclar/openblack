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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The villagers' ground blobs: a short dark shadow stretched from each foot over the land, away from a light low in
/// the south east, fading as it goes.
namespace openblack::graphics::ground_blobs
{

/// The bones whose origins are a villager's feet
inline constexpr std::array<size_t, 2> k_FootBones = {21, 18};
/// Things this low, in the sea, have none
inline constexpr float k_LowestHeight = 0.2f;
/// How far over the land a blob lies
inline constexpr float k_Lift = 0.2f;
/// How far over the land the blob of a villager too far away to be drawn lies, and how many times as wide it is
inline constexpr float k_FarLift = 0.5f;
inline constexpr float k_FarWidening = 3.0f;

/// A blob's quad: from just behind the foot, across it, out to its far end
struct Quad
{
	std::array<glm::vec3, 4> corners;
};
/// The texture coordinates of a quad's corners, and their opacity: whole at the foot, none at the far end
inline constexpr std::array<glm::vec2, 4> k_Uvs = {glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f),
                                                   glm::vec2(0.0f, 1.0f)};
inline constexpr std::array<float, 4> k_Opacity = {1.0f, 1.0f, 0.0f, 0.0f};
/// The quad's two triangles
inline constexpr std::array<uint16_t, 6> k_Indices = {0, 1, 2, 0, 2, 3};

/// How far and which way the shadows fall for a thing of a scale standing on land of a normal: the light's offset laid
/// flat on the land
[[nodiscard]] glm::vec3 Fall(const glm::vec3& landNormal, float scale);
/// A blob from a foot, falling by `fall`
[[nodiscard]] Quad MakeQuad(const glm::vec3& foot, const glm::vec3& fall);
/// The two blobs of a pair of feet: each falls by the shadow's fall and leans halfway towards the other foot
[[nodiscard]] std::array<Quad, 2> Feet(const glm::vec3& first, const glm::vec3& second, const glm::vec3& fall);

/// The blob of a villager too far away to be drawn, standing at `foot` (on the land, lifted by k_FarLift): both its
/// feet at its position on land taken as flat, and wider. Its two quads are the same quad, so it is darker.
[[nodiscard]] std::array<Quad, 2> Far(const glm::vec3& foot, float scale);

/// The dark smudge a villager too far away to be drawn shows as, standing over its far blob: a sheet facing the view,
/// black and a little see-through, of the first frame of the smoke texture. Its size is set by a scale given once (the
/// first villager seen that far away); it stands as high as the villager's own scale puts it.
inline constexpr float k_SmudgeHalfWidth = 0.3f;
inline constexpr float k_SmudgeTallness = 3.0f;
inline constexpr float k_SmudgeRise = 0.8f;
inline constexpr uint8_t k_SmudgeAlpha = 150;
/// The texture coordinates of its corners: the first of the smoke texture's 8 by 8 frames
inline constexpr std::array<glm::vec2, 4> k_SmudgeUvs = {glm::vec2(0.0f, 0.0f), glm::vec2(0.125f, 0.0f),
                                                         glm::vec2(0.125f, 0.125f), glm::vec2(0.0f, 0.125f)};

/// The smudge's corners, top left first, round to the bottom left: `right` and `up` are the view's
[[nodiscard]] std::array<glm::vec3, 4> SmudgeCorners(const glm::vec3& position, float scale, float smudgeScale,
                                                     const glm::vec3& right, const glm::vec3& up);

} // namespace openblack::graphics::ground_blobs
