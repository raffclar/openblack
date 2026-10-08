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

#include <algorithm>
#include <span>
#include <vector>

#include "BikFile.h"
#include "Graphics/Rgb16.h"

/// The picture decoder behind the video player: what the Bink library does for the original's player (open, then
/// decode and advance one frame at a time). The 16-bit copy is not the decoder's: the player does it with
/// graphics::rgb16.
namespace openblack::video
{

class IVideoDecoder
{
public:
	virtual ~IVideoDecoder() = default;

	/// Get ready to decode `file` (it outlives the decoder). False: the film cannot be decoded (the player then has no
	/// film)
	virtual bool Open(const BikFile& file) = 0;
	/// Decodes frame `index` (0, 1, 2, ... in order: Bink 1 frames are deltas of the one before): the picture as RGBA8,
	/// width * height * 4 bytes, valid up to the next call. Empty: the frame failed, and the player keeps the picture it
	/// had
	[[nodiscard]] virtual std::span<const uint8_t> DecodeNext(uint32_t index) = 0;
	/// Decodes frame `index` as DecodeNext does, without making its colours (the player shows only the last frame of
	/// those it catches up): the size DecodeNext would have given, 0 when the frame failed
	virtual size_t DecodeOnly(uint32_t index)
	{
		const auto pixels = DecodeNext(index);
		if (!pixels.empty())
		{
			_lastPicture = pixels;
		}
		return pixels.size();
	}
	/// The last picture decoded through the player's 16-bit framebuffer: `texels` as graphics::rgb16::Quantize fills it,
	/// `rgba` as graphics::rgb16::Expand gives it back
	virtual void CopyPicture(graphics::rgb16::Format format, std::span<uint16_t> texels, std::span<uint8_t> rgba)
	{
		graphics::rgb16::Quantize(format, _lastPicture, texels);
		graphics::rgb16::Expand(format, texels, rgba);
	}

private:
	/// The picture of the last DecodeOnly that gave one (valid as DecodeNext's)
	std::span<const uint8_t> _lastPicture;
};

/// The picture when there is no decoder: every frame is opaque black (the tests, and the fallback when a decoder
/// refuses a film), so the player behaves as the
/// original (pause, wide screen, fade, skip) over a black screen
class NullVideoDecoder final: public IVideoDecoder
{
public:
	bool Open(const BikFile& file) override
	{
		_frames = file.FrameCount();
		_pixels.assign(static_cast<size_t>(file.Width()) * file.Height() * 4, 0);
		for (size_t i = 3; i < _pixels.size(); i += 4)
		{
			_pixels[i] = 0xFF;
		}
		return true;
	}

	[[nodiscard]] std::span<const uint8_t> DecodeNext(uint32_t index) override
	{
		if (index >= _frames)
		{
			return {};
		}
		return _pixels;
	}

	size_t DecodeOnly(uint32_t index) override { return index < _frames ? _pixels.size() : 0; }

	/// Black through the framebuffer: the texel of (0, 0, 0) and what it expands to
	void CopyPicture(graphics::rgb16::Format format, std::span<uint16_t> texels, std::span<uint8_t> rgba) override
	{
		const size_t pixels = std::min(_pixels.size() / 4, std::min(texels.size(), rgba.size() / 4));
		const uint16_t black = graphics::rgb16::Pack(format, 0, 0, 0);
		const auto sampled = graphics::rgb16::Unpack(format, black);
		for (size_t i = 0; i < pixels; ++i)
		{
			texels[i] = black;
			std::copy(sampled.begin(), sampled.end(), rgba.begin() + static_cast<ptrdiff_t>(i * 4));
		}
	}

private:
	uint32_t _frames {0};
	std::vector<uint8_t> _pixels;
};

} // namespace openblack::video
