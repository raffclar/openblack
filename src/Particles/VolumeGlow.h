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

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The glowing shell some effects lay over a model, as over a worthy artefact: every edge of the model's triangles is
/// drawn out along its corners' normals into a band, bright where the model is seen side on and fading to nothing at its
/// outer edge, added to what is behind it with a sliding texture
namespace openblack::particles::volume_glow
{

/// The textures a shell may be drawn with, by the number its rule names
inline constexpr std::array<std::string_view, 7> k_Textures {
    "S_VolLight", "S_VolLight2", "S_VolLight3", "S_VolLight4", "S_LightSheetStars", "S_Teleport_stars", "S_TileLandscape",
};

/// How a rule draws its shells, as its file sets it; the defaults are those of a rule that sets nothing
struct Settings
{
	int texture {2};
	/// How fast the texture slides across and up, in sheets a second
	glm::vec2 animSpeed {0.25f, 0.16f};
	/// How far out the shell reaches, in parts of the model's half diagonal, at no strength and at full strength
	float zoomMin {0.1f};
	float zoomMax {1.0f};
	/// How much of the texture a shell spans: across, by how side on the model is seen, and up, from its inner edge
	glm::vec2 textureFraction {0.18f, 0.16f};
	/// 0..255
	int alpha {207};
	bool usePlayerColour {false};
	/// The strength sets the shell's alpha rather than how far it reaches
	bool alphaFromStrength {false};
};

/// The effect's strength as the shell takes it: 0 to 1
[[nodiscard]] float Strength(float power);
/// How far the shell reaches, in parts of the model's half diagonal: by the strength, times the scale of the atom kept on
/// the object. A shell whose strength sets its alpha keeps the least reach, whatever its atom's scale.
[[nodiscard]] float Zoom(const Settings& settings, float power, float atomScale);
/// The shell's alpha before the collection's fade, 0..255
[[nodiscard]] uint32_t Alpha(const Settings& settings, float power);
/// The shell's colour, 0xAARRGGBB: white, or its player's colour when it takes one and there is a player; its alpha the
/// collection's alpha of the shell's
[[nodiscard]] uint32_t Argb(const Settings& settings, uint32_t alpha, uint8_t collectionAlpha,
                            std::optional<uint32_t> playerRgb);
/// How far the texture has slid after some seconds: 0 to 1 the way it moves
[[nodiscard]] glm::vec2 UvOffset(glm::vec2 animSpeed, float seconds);

/// A corner's normal in the camera's frame as the shell measures it: the model's axes in the camera's frame (x right,
/// y up, z ahead), widened across and up by the projection's scales, each then made one long, and summed by the normal
[[nodiscard]] glm::vec3 ViewNormal(glm::vec3 normal, const std::array<glm::vec3, 3>& axes, glm::vec2 projectionScale);
/// How bright a corner of the shell's inner edge is, 0..255 (255 is the whole colour), by its normal in the camera's
/// frame (x right, y up): the more side on, the brighter. A model moved by bones measures side on another way.
[[nodiscard]] uint32_t EdgeLevel(glm::vec2 viewNormal, bool boned);
/// The colour of a corner of the inner edge at a level: red, green and blue scaled as whole bytes, the alpha kept
[[nodiscard]] uint32_t InnerArgb(uint32_t argb, uint32_t level);
/// Where a corner reads the texture: across by how much its normal points up in the camera's frame, up from the slide
/// at the inner edge to a fraction further back at the outer
[[nodiscard]] glm::vec2 TextureCoordinates(float viewNormalY, glm::vec2 offset, glm::vec2 fraction, bool outer);

/// The shell's triangles over a model's triangles: each model corner i becomes two corners, 2i on the model and 2i + 1
/// pushed out, and each edge of each triangle a band of two triangles between them, in the order drawn. The model's
/// triangles themselves aren't drawn.
[[nodiscard]] std::vector<uint32_t> ShellTriangles(std::span<const uint16_t> triangles);

} // namespace openblack::particles::volume_glow
