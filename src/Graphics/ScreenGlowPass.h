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
#include <chrono>
#include <memory>
#include <optional>

#include <glm/vec2.hpp>

#include "Common/FramePace.h"
#include "GraphicsHandle.h"

namespace openblack::graphics
{

class FrameBuffer;
class ShaderManager;

/// The soft glow of the whole screen under a good sky (see ScreenGlow.h), on the GPU. In a frame with the glow, the
/// window's passes draw into a target of the window's size. One pass then copies that frame into the window with the
/// glow added. Another pass steps the 16 by 16 points of the glow towards that frame, at most 60 times a second. The
/// points stay from frame to frame, also through the frames without the glow, as the game kept them.
class ScreenGlowPass
{
public:
	ScreenGlowPass();
	~ScreenGlowPass();
	ScreenGlowPass(const ScreenGlowPass&) = delete;
	ScreenGlowPass& operator=(const ScreenGlowPass&) = delete;

	/// Asks for the glow in this frame, which draws the land, at a strength of 255 (0 for none). The frame's real
	/// time tells whether the points step.
	void Want(uint8_t strength, std::chrono::milliseconds frameRealTime);
	/// Call this last before bgfx ends the frame. It sends the window's passes to the target of the frame and submits the
	/// two passes of the glow, if the frame asked for the glow. The copy goes to the window, or to the target that the
	/// window's passes draw into for a picture.
	void Submit(const ShaderManager& shaders, glm::u16vec2 window, std::optional<FrameBufferHandle> windowTarget);
	/// Call this after bgfx ended the frame. It gives the window's passes back to the window.
	void FrameEnded();
	/// After a restore of the device, the points have no values until their next step
	void Lost();

	/// Whether the device can keep the points: whole numbers in 32-bit floats
	[[nodiscard]] static bool Supported();

private:
	/// The frame without the glow, at the window's size
	std::unique_ptr<FrameBuffer> _frame;
	/// The points: the values of the last step, and the target of the next step
	std::array<std::unique_ptr<FrameBuffer>, 2> _points;
	/// The points that hold the values of the last step
	uint8_t _current {0};
	/// Whether the points stepped since they were made
	bool _hasValues {false};
	/// The strength of the glow in this frame, if the frame asked for the glow
	std::optional<uint8_t> _wanted;
	/// Whether the points step in this frame
	bool _stepDue {false};
	/// Whether the window's passes draw into the frame's target in this frame
	bool _redirected {false};
	frame_pace::Pace _pace;
};

} // namespace openblack::graphics
