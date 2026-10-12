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

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// Which of its meshes a model with several is drawn as, chosen afresh every frame by how deep into the view it stands:
/// its high mesh close by, its standard one further off, its low one beyond, then faded out and at last left out. The
/// bands grow with the model's size and importance. There is no blending and no lag between bands: a model switches
/// as soon as it crosses one.
namespace openblack::graphics::mesh_detail
{

/// The most a model's reach may be, however big or important it is
inline constexpr float k_MostReach = 5.0f;
/// The depths, as so many reaches into the view, at which the standard mesh, the low one, the fading and the leaving
/// out start
inline constexpr float k_StandardFrom = 23.333334f;
inline constexpr float k_LowFrom = 66.666664f;
inline constexpr float k_FadeFrom = 86.666664f;
inline constexpr float k_GoneFrom = 173.33333f;

/// The meshes a model holds, from the most detailed
enum class Mesh : uint8_t
{
	High,
	Standard,
	Low,
};

/// How a model is drawn this frame
struct Choice
{
	/// The mesh it is drawn as, or none: it isn't drawn as a mesh at all
	std::optional<Mesh> mesh;
	/// While it fades out, how opaque it is drawn, of 255
	std::optional<uint8_t> alpha;
};

/// How deep into the view a point is: its distance along the way the camera looks, not the straight distance to it, so
/// that a model at the side of the view keeps the detail of one in the middle at the same depth. It doesn't depend on
/// the field of view or the screen's size.
[[nodiscard]] float ViewDepth(const glm::vec3& point, const glm::vec3& eye, const glm::vec3& forward) noexcept;

/// The radius of the sphere around a model's bounding box (half the box's diagonal) at the model's scale
[[nodiscard]] float ScaledRadius(const glm::vec3& boxSize, float scale) noexcept;

/// How far a model's bands reach, one unit of depth for each: its importance (0 for most things) and its bounding
/// sphere's radius at its scale make it bigger, as the model detail setting does (see detail_level::ModelDetail)
[[nodiscard]] float Reach(float importance, float scaledRadius, float modelDetail) noexcept;

/// How a model is drawn whose bounding sphere's centre is `depth` into the view. One that `disappears` fades out and is
/// then left out; any other is drawn as its low mesh however far it is.
[[nodiscard]] Choice Choose(float depth, float reach, bool disappears) noexcept;

/// The bit a mesh choice stands for among a model's parts: each part names the choices it is drawn at, so a model with
/// one mesh can still draw fewer or other parts further off (a tree's far card)
[[nodiscard]] constexpr uint8_t LevelBit(Mesh mesh) noexcept
{
	return static_cast<uint8_t>(1u << static_cast<uint8_t>(mesh));
}

/// Whether a part that names the choices `partLevels` is drawn at the choice `mesh`
[[nodiscard]] constexpr bool PartDrawn(uint8_t partLevels, Mesh mesh) noexcept
{
	return (partLevels & LevelBit(mesh)) != 0;
}

/// Whether a model's parts differ between the choices: some part, of those ever drawn, isn't drawn at every one
[[nodiscard]] constexpr bool PartsDifferByLevel(uint8_t partLevels) noexcept
{
	constexpr uint8_t k_Every = 0b111;
	return (partLevels & k_Every) != 0 && (partLevels & k_Every) != k_Every;
}

/// The matrix a paper thing (a tree, a field's crop) is drawn by as its low mesh: standing upright where it stands, at
/// its scale, and turned with the camera so that its face looks back along the way the camera looks. Every paper thing
/// turns the same way at once, by the camera's heading alone.
[[nodiscard]] glm::mat4 PaperMatrix(const glm::vec3& position, float scale, const glm::vec3& eye,
                                    const glm::vec3& focus) noexcept;
/// The turn every paper thing takes for a camera, worked out once a frame (see PaperMatrix)
[[nodiscard]] glm::mat3 PaperTurn(const glm::vec3& eye, const glm::vec3& focus) noexcept;
/// A paper thing's matrix by the turn of its camera
[[nodiscard]] glm::mat4 PaperMatrix(const glm::vec3& position, float scale, const glm::mat3& turn) noexcept;

/// How important a villager is to keep in detail: a thousandth of the age it was made at
[[nodiscard]] float VillagerImportance(uint32_t ageWhenMade) noexcept;

} // namespace openblack::graphics::mesh_detail
