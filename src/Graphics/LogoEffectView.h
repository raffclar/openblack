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
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "FrameBuffer.h"
#include "Texture2D.h"

namespace openblack::l3d
{
class L3DFile;
}

namespace openblack::graphics
{

/// Draws the logo effect of the first start into a picture of its own, which keeps each frame under the next. The sky
/// sphere blends over the last frame, so that the dots leave trails. The interface draws over the picture in the
/// overlay pass and then shows the picture on the screen.
class LogoEffectView
{
public:
	/// One dot: a square of the dot texture that faces the camera
	struct Dot
	{
		/// The corners in turn round the square, in the world, from the top left of the picture
		std::array<glm::vec3, 4> corners;
		/// Alpha, red, green and blue, from the high byte
		uint32_t argb;
		/// Added to what is under it, else blended over it by its alpha
		bool additive;
	};

	/// A frame of the effect
	struct Scene
	{
		glm::vec3 position;
		glm::vec3 right;
		glm::vec3 up;
		glm::vec3 forward;
		/// The world's origin in the camera's axes
		glm::vec3 offset;
		/// The scales from a point in front of the camera to the clip space, across and up
		float clipScaleX;
		float clipScaleY;
		/// Where the middle of the effect's own screen is in the clip space of the picture, which can be wider
		glm::vec2 clipShift;
		float nearClip;
		/// The sky sphere's colour: alpha, red, green and blue
		uint32_t skyColour;
		std::span<const Dot> dots;
		/// Where on the dot texture each dot's picture is
		glm::vec2 dotUvMin;
		glm::vec2 dotUvMax;
		const Texture2D* dotTexture;
	};

	/// A black picture of a size, with the sky sphere of a model; none when the model has no sphere
	static std::unique_ptr<LogoEffectView> Create(glm::u16vec2 size, const l3d::L3DFile& sky);
	~LogoEffectView();
	LogoEffectView(const LogoEffectView&) = delete;
	LogoEffectView& operator=(const LogoEffectView&) = delete;

	/// Draws the sky sphere and the dots over the last frame, in the logo effect's pass
	void Draw(const Scene& scene);

	/// The picture, for the interface to show on the screen
	[[nodiscard]] const Texture2D& GetPicture() const { return _target.GetColorAttachment(); }
	[[nodiscard]] glm::u16vec2 GetSize() const noexcept { return _size; }

private:
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t abgr;
	};

	LogoEffectView(glm::u16vec2 size, std::vector<Vertex> skyVertices, std::vector<uint16_t> skyIndices,
	               std::unique_ptr<Texture2D> skySkin);

	glm::u16vec2 _size;
	FrameBuffer _target;
	std::vector<Vertex> _skyVertices;
	std::vector<uint16_t> _skyIndices;
	std::unique_ptr<Texture2D> _skySkin;
	/// The first frame clears the picture to black; the next frames keep it
	bool _cleared {false};
};

} // namespace openblack::graphics
