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

/// How far a model's bands reach, one unit of depth for each: its importance (0 for most things) and its bounding
/// sphere's radius at its scale make it bigger, as the model detail setting does (see detail_level::ModelDetail)
[[nodiscard]] float Reach(float importance, float scaledRadius, float modelDetail) noexcept;

/// How a model is drawn whose bounding sphere's centre is `depth` into the view. One that `disappears` fades out and is
/// then left out; any other is drawn as its low mesh however far it is.
[[nodiscard]] Choice Choose(float depth, float reach, bool disappears) noexcept;

/// How important a villager is to keep in detail: a thousandth of the age it was made at
[[nodiscard]] float VillagerImportance(uint32_t ageWhenMade) noexcept;

} // namespace openblack::graphics::mesh_detail
