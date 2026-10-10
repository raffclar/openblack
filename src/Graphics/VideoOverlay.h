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
#include <memory>
#include <optional>
#include <span>

#include <glm/vec2.hpp>

#include "Graphics/RenderPass.h"
#include "Video/VideoRules.h"

namespace openblack::graphics
{

class Texture2D;

/// A decoded Bink picture drawn as one screen quad: its Y, U and V planes as three single-channel textures, turned to
/// colour in the shader with the game's video library's tables. The planes are uploaded only when the picture changes.
class VideoOverlay
{
public:
	/// One picture's planes, Y full size and U, V half size each way, rows `stride` bytes apart; empty spans draw black
	struct Planes
	{
		std::span<const uint8_t> y;
		std::span<const uint8_t> u;
		std::span<const uint8_t> v;
		uint32_t yStride {0};
		uint32_t chromaStride {0};
		uint32_t width {0};
		uint32_t height {0};
		/// Goes up with each new picture
		uint32_t serial {0};
	};

	VideoOverlay();
	~VideoOverlay();
	VideoOverlay(const VideoOverlay&) = delete;
	VideoOverlay& operator=(const VideoOverlay&) = delete;

	/// Draws the picture over `rect` of a view `resolution` pixels big, at `alpha` (0 to 255), alpha blended with no
	/// depth test. `sixteenBit` shows it as the game's 16-bit copy did, each channel cut to 5 bits
	void Draw(RenderPass view, glm::u16vec2 resolution, const Planes& planes, video::ScreenRect rect, uint8_t alpha,
	          bool sixteenBit);

private:
	/// Makes the plane textures for a picture's size, then uploads its planes when they're new
	void Upload(const Planes& planes);

	std::unique_ptr<Texture2D> _colourTables;
	std::array<std::unique_ptr<Texture2D>, 3> _planes;
	std::array<glm::u16vec2, 3> _planeSizes {};
	std::optional<uint32_t> _uploadedSerial;
	const void* _uploadedData {nullptr};
};

} // namespace openblack::graphics
