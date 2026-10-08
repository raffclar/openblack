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
#include <span>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/vec3.hpp>

namespace openblack
{

/// The weather system's palette.raw: 32 by 32 colours. Its first three rows are the good, neutral and evil colours of the
/// land through the day; its next ones a dark colour, the moon's and a warm one, by alignment.
class LandLightPalette
{
public:
	static constexpr size_t k_Side = 32;
	static constexpr entt::hashed_string k_Id = entt::hashed_string("weather/palette");

	/// Red, green, blue and alpha bytes; throws for a palette of the wrong size
	explicit LandLightPalette(std::span<const uint8_t> bytes);

	/// 0xAARRGGBB
	[[nodiscard]] uint32_t At(size_t row, size_t column) const { return _colours.at(row * k_Side + column); }

private:
	std::vector<uint32_t> _colours;
};

/// The land's light: 256 colours, one for each level of a cell's luminosity, rebuilt every frame from the palette for
/// the time of day and the alignment the sky shows. The land's vertices take the colour of their luminosity, the land's
/// reflection half of it and the sea the brightest one.
///
/// The darkest 48 levels ramp from the dark colour up to the land's colour and on to the warm one, the rest from the
/// dark colour up to the land's, all divided by 200 rather than 255, so the brightest cells come out brighter than the
/// palette.
class LandLightTable
{
public:
	static constexpr size_t k_Size = 256;

	/// skyType runs from 0 by day through 1 at dusk to 2 at night (the sky type of sky_type, which gives the time
	/// column and the haze), and alignment from -1, evil, to 1, good. The overcast at the camera, 0 for a clear sky and
	/// 1 for a full one, darkens the land's colour and draws the haze in. A flash of lightning, 0 to 255, takes every
	/// light and the haze towards white.
	void Build(const LandLightPalette& palette, float skyType, float alignment, float overcast, uint8_t flash = 0) noexcept;

	/// The distance haze of the frame: from `nearDistance` to `farDistance` from the camera things fade towards the haze
	/// colour, which is added to them, while their own colour is scaled down to k of 256. The colour is a third of the
	/// land's, k follows its brightness, and the haze closes in at dusk.
	struct Haze
	{
		float nearDistance {400.0f};
		float farDistance {900.0f};
		float k {256.0f};
		glm::vec3 colour {0.0f}; ///< 0 to 255
	};

	/// A table with every light 0xFFFFFFFF, a white land colour and the default haze: the game's table before the first
	/// Build
	[[nodiscard]] static LandLightTable Unset() noexcept;
	/// A 0xAARRGGBB colour as red, green and blue from 0 to 1
	[[nodiscard]] static glm::vec3 ToColour(uint32_t colour) noexcept;

	/// The table as RGBA8 texels
	[[nodiscard]] const std::array<uint32_t, k_Size>& GetTexels() const noexcept { return _texels; }
	/// The light of a level of luminosity, 0xAARRGGBB
	[[nodiscard]] uint32_t GetRaw(size_t index) const noexcept { return _table[index]; }
	/// The same as red, green and blue from 0 to 1
	[[nodiscard]] glm::vec3 GetColour(size_t index) const noexcept;
	[[nodiscard]] const Haze& GetHaze() const noexcept { return _haze; }
	/// The land's colour of the frame, by the time of day, alignment and overcast, 0xAARRGGBB
	[[nodiscard]] uint32_t GetLandColour() const noexcept { return _landColour; }
	/// The palette's warm colour of the frame, which the darkest levels go on to, 0xAARRGGBB
	[[nodiscard]] uint32_t GetWarmColour() const noexcept { return _warmColour; }
	/// The moon's colour of the frame, by the alignment, 0xAARRGGBB
	[[nodiscard]] uint32_t GetMoonColour() const noexcept { return _moonColour; }

private:
	std::array<uint32_t, k_Size> _table {};  ///< 0xAARRGGBB
	std::array<uint32_t, k_Size> _texels {}; ///< the same colours as RGBA8
	Haze _haze;
	uint32_t _landColour {0xFFFFFFFFu};
	uint32_t _warmColour {0xFFFFFFFFu};
	uint32_t _moonColour {0xFFFFFFFFu};
};

} // namespace openblack
