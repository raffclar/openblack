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
#include <filesystem>
#include <string>

namespace openblack
{
class Bitmap16B
{
public:
	Bitmap16B() = delete;

	explicit Bitmap16B(const void* data);
	~Bitmap16B();

	[[nodiscard]] unsigned int Width() const { return _width; }
	[[nodiscard]] unsigned int Height() const { return _height; }
	uint16_t* Data() { return _data; }
	[[nodiscard]] size_t Size() const { return _size; };

private:
	unsigned int _width;
	unsigned int _height;
	uint16_t* _data;
	size_t _size;

public:
	static Bitmap16B* LoadFromFile(const std::filesystem::path& path);
};

/// A texel of 5 bits for each of red, green and blue and 1 bit of alpha, as red, green, blue and alpha bytes. Each colour
/// is the nearest byte to its 5-bit value. This is for a device that does not have the 16-bit texture format.
[[nodiscard]] constexpr std::array<uint8_t, 4> Texel1555Bytes(uint16_t texel) noexcept
{
	constexpr uint32_t k_Five = 31;
	const auto expand = [](uint32_t value) { return static_cast<uint8_t>(((value * 255U) + (k_Five / 2)) / k_Five); };
	const auto alpha = static_cast<uint8_t>((texel & 0x8000U) != 0 ? 0xFF : 0x00);
	return {expand((texel >> 10U) & k_Five), expand((texel >> 5U) & k_Five), expand(texel & k_Five), alpha};
}

} // namespace openblack
