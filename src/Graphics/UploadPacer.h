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

	/// What a frame has room for. The renderer gives every buffer and texture a memory allocation of its own, which
	/// costs about as much for a small one as for a large one, so a model of many small parts stalls a frame as surely
	/// as one large texture: both how many are made and how many bytes they hold are limited.
	struct Allowance
	{
		size_t bytes {k_Unlimited};
		size_t uploads {k_Unlimited};
	};

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

	/// Before one more buffer or texture of about `bytes` is made: on a thread pacing its uploads, waits until this
	/// frame has room for it. The first upload of a frame always goes, however large.
	static void Pace(size_t bytes);

	/// A new frame has begun, with room for this much
	void BeginFrame(Allowance allowance);

	/// Lets every upload through from now on, as when the game stops
	void Release();

	/// How many bytes went this frame
	[[nodiscard]] size_t GetUploaded() const;

	/// How many buffers and textures went this frame
	[[nodiscard]] size_t GetUploadCount() const;

private:
	void Take(size_t bytes);

	mutable std::mutex _mutex;
	std::condition_variable _wake;
	Allowance _allowance;
	size_t _uploaded {0};
	size_t _uploadCount {0};
	size_t _hurries {0};
	bool _released {false};
};

} // namespace openblack::graphics
