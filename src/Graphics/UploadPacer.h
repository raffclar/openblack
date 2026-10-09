/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <condition_variable>
#include <cstddef>

#include <limits>
#include <mutex>

namespace openblack::graphics
{

/// Paces how much the loading threads hand the graphics card each frame. The renderer makes everything handed to it in
/// a frame before it can draw the next, so a thread making a large model waits, between its buffers and textures, until
/// the next frame has room for more. Only threads that took the pacer with Scope wait; the main thread never does.
class UploadPacer
{
public:
	/// Until the first frame anything goes: there is no frame to keep smooth yet
	static constexpr size_t k_Unlimited = std::numeric_limits<size_t>::max();

	/// Makes the calling thread's uploads go through this pacer while it lasts
	class Scope
	{
	public:
		explicit Scope(UploadPacer& pacer);
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;
		~Scope();
	};

	/// Lets every upload through at once while it lasts, for when the main thread waits for a load
	class Hurry
	{
	public:
		explicit Hurry(UploadPacer& pacer);
		Hurry(const Hurry&) = delete;
		Hurry& operator=(const Hurry&) = delete;
		~Hurry();

	private:
		UploadPacer& _pacer;
	};

	/// Before about `bytes` more are made: on a thread pacing its uploads, waits until this frame has room for them. The
	/// first upload of a frame always goes, however large.
	static void Pace(size_t bytes);

	/// A new frame has begun, with room for about `bytes` of uploads
	void BeginFrame(size_t bytes);

	/// Lets every upload through from now on, as when the game stops
	void Release();

	/// How many bytes went this frame
	[[nodiscard]] size_t GetUploaded() const;

private:
	void Take(size_t bytes);

	mutable std::mutex _mutex;
	std::condition_variable _wake;
	size_t _allowance {k_Unlimited};
	size_t _uploaded {0};
	size_t _hurries {0};
	bool _released {false};
};

} // namespace openblack::graphics
