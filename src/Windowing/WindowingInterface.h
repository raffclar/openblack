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

#include <glm/vec2.hpp>

#include "Visual/DisplayMode.h"

namespace openblack::windowing
{

class WindowingInterface
{
public:
	struct NativeHandles
	{
		void* nativeWindow;
		void* nativeDisplay;
	};

	[[nodiscard]] virtual void* GetHandle() const = 0;
	[[nodiscard]] virtual NativeHandles GetNativeHandles() const = 0;
	[[nodiscard]] virtual uint32_t GetID() const = 0;
	/// The size of the back buffer in pixels. The game draws, lays out its screens and reads the pointer in these pixels.
	[[nodiscard]] virtual glm::ivec2 GetSize() const = 0;
	/// The pixels of the back buffer for one unit of the window's mouse events. It is 1 on the desktop. In a web browser
	/// it is the device pixel ratio of the page.
	[[nodiscard]] virtual glm::vec2 GetPixelScale() const { return glm::vec2(1.0f); }
	[[nodiscard]] virtual float GetAspectRatio() const = 0;
	[[nodiscard]] virtual bool IsMinimised() const = 0;
	/// Whether the game runs in a window rather than taking the whole screen
	[[nodiscard]] virtual bool IsWindowed() const = 0;

	virtual WindowingInterface& SetDisplayMode(DisplayMode mode) = 0;
};

} // namespace openblack::windowing
