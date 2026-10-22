/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WebPointer.h"

#include <cstdint>

#include <atomic>

#include <emscripten.h>
#include <emscripten/threading.h>

namespace
{

/// The pointer lock state that the page writes from its main thread. The game thread reads it. This state belongs to
/// the browser page, not to the game, thus it is not a service of the Locator.
std::atomic<bool>& LockState()
{
	static std::atomic<bool> locked {false};
	return locked;
}

/// The place of the click that took the lock, as x and y in the high and low 32 bits; a negative x when not known
std::atomic<int64_t>& LockClick()
{
	static std::atomic<int64_t> click {-1LL << 32};
	return click;
}

} // namespace

/// The page calls this on its main thread when the pointer lock of the game canvas starts or stops (see
/// web/page/input.js)
/// The page gives the place of the click that took the lock, in the pixels of the window, or -1 for x when it does not
/// know it.
extern "C" EMSCRIPTEN_KEEPALIVE void openblack_web_pointer_lock_changed(int locked, int x, int y)
{
	LockClick().store((static_cast<int64_t>(x) << 32) | static_cast<uint32_t>(y), std::memory_order_relaxed);
	LockState().store(locked != 0, std::memory_order_release);
}

std::optional<glm::ivec2> openblack::web::PointerLockClick() noexcept
{
	const auto click = LockClick().load(std::memory_order_relaxed);
	const auto x = static_cast<int32_t>(click >> 32);
	if (x < 0)
	{
		return std::nullopt;
	}
	return glm::ivec2(x, static_cast<int32_t>(static_cast<uint32_t>(click & 0xFFFFFFFF)));
}

bool openblack::web::PointerLocked() noexcept
{
	return LockState().load(std::memory_order_acquire);
}

void openblack::web::AllowPointerLock()
{
	// The page gets this call after the window has its event listeners, because the main thread does the calls in order
	// clang-format off
	MAIN_THREAD_ASYNC_EM_ASM({
		if (globalThis.openblackInput) {
			globalThis.openblackInput.allowLock();
		}
	});
	// clang-format on
}

void openblack::web::FreePointer()
{
	// The call does not wait for the main thread
	// clang-format off
	MAIN_THREAD_ASYNC_EM_ASM({
		if (globalThis.openblackInput) {
			globalThis.openblackInput.freePointer();
		}
	});
	// clang-format on
}

void openblack::web::RequestFullscreen(bool fullscreen)
{
	// clang-format off
	MAIN_THREAD_ASYNC_EM_ASM({
		if (globalThis.openblackInput) {
			globalThis.openblackInput.requestFullscreen($0 !== 0);
		}
	}, fullscreen ? 1 : 0);
	// clang-format on
}
