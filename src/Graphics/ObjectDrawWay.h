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

#include <glm/vec3.hpp>

#include "Graphics/MeshDetail.h"
#include "Graphics/ZSort.h"

/// The two ways that the game draws a model by itself, such as the seed of a miracle and the rings round it, and how
/// each way puts the model among the things that blend.
namespace openblack::graphics::object_draw
{

/// How a model is drawn
enum class Way : uint8_t
{
	/// The object draw: the distance bands choose its mesh, it fades out far off and the draw is cut after the fade. A
	/// model whose mesh is marked to be sorted whole goes among the sorted things, whatever its alpha. Any other model
	/// draws at once.
	Banded,
	/// At once, in its most detailed mesh, never sorted. Only a far cull can cut the draw.
	AtOnce,
};

/// The far cull cuts a draw at once when the model is further than this many of its radii from the camera
inline constexpr float k_FarCullRadii = 300.0f;

/// The bit of the mesh's flags that marks a model to be sorted whole among the things that blend
inline constexpr uint32_t k_SortedWholeFlag = 0x200u;

/// Whether a mesh with these flags is sorted whole among the things that blend when the object draw draws it
[[nodiscard]] constexpr bool SortedWhole(uint32_t meshFlags) noexcept
{
	return (meshFlags & k_SortedWholeFlag) != 0;
}

/// Whether the far cull cuts a draw at once. The radius is half the diagonal of the mesh's box. The game multiplies it
/// by the model's scale and then by 300, and compares the square with the square of the distance.
[[nodiscard]] inline bool FarCulled(float halfDiagonal, float scale, const glm::vec3& place, const glm::vec3& camera) noexcept
{
	const float far = (halfDiagonal * scale) * k_FarCullRadii;
	return (far * far) < zsort::Key(place, camera);
}

/// What the object draw draws: the mesh of the band and the alpha
struct Banded
{
	mesh_detail::Mesh mesh;
	uint8_t alpha;
};

/// How the object draw draws a model by its band (`choice`, chosen with the fade on). The draw is cut after the fade.
/// In the fade, the fade gives the alpha, unless the model `keepsOwnAlpha`: then the model keeps `ownAlpha`.
[[nodiscard]] constexpr std::optional<Banded> DrawBanded(const mesh_detail::Choice& choice, uint8_t ownAlpha,
                                                         bool keepsOwnAlpha) noexcept
{
	if (!choice.mesh.has_value())
	{
		return std::nullopt;
	}
	const uint8_t alpha = (choice.alpha.has_value() && !keepsOwnAlpha) ? *choice.alpha : ownAlpha;
	return Banded {.mesh = *choice.mesh, .alpha = alpha};
}

/// Whether a draw goes among the sorted things, also at an alpha of 255
[[nodiscard]] constexpr bool AmongSorted(Way way, bool sortedWhole) noexcept
{
	return way == Way::Banded && sortedWhole;
}

/// The sort depth of a draw among the things that blend: the key of a placed model at its place when it is among the
/// sorted things, else the depth of a draw at once, after the solid models and before every sorted thing
[[nodiscard]] inline uint32_t BlendDepth(Way way, bool sortedWhole, const glm::vec3& place, const glm::vec3& camera) noexcept
{
	return AmongSorted(way, sortedWhole) ? zsort::ModelDepth(place, camera) : zsort::k_AfterSolidBeforeSorted;
}

/// What decides where a standing model of the land that blends goes among the things that blend
struct StandingBlend
{
	/// It fades out in the distance, or it is a see-through copy, blended by its own alpha
	bool blended {false};
	/// Its mesh is marked to be sorted whole
	bool sortedWhole {false};
	/// Its own draw puts it among the sorted things: a hand or a creature holds it, or it is a phantom building, a yard
	/// copy, a reward chest, posed scenery of all models but one, or a model with changed materials
	bool sortedByItsDraw {false};
};

/// Whether the game draws a standing model that blends at once, at its place in the land's draw list, and not among
/// the sorted things (see zsort::AtOnce)
[[nodiscard]] constexpr bool BlendsAtOnce(const StandingBlend& thing) noexcept
{
	return thing.blended && !thing.sortedWhole && !thing.sortedByItsDraw;
}

} // namespace openblack::graphics::object_draw
