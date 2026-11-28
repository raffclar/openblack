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

#include <atomic>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace openblack::ecs
{

/// What the check does when the draw reads a registry of the world. The draw must read only the drawn copy, thus a turn
/// can play beside the draw.
enum class DrawReadCheck : uint8_t
{
	/// There is no check
	Off,
	/// The check counts each read, and the log gets the first read of each kind
	Report,
	/// The first read stops the game
	Fail,
};

/// Counts the reads of the registries that the draw makes on the thread of the draw. The game starts the watch before
/// the draw and stops it after. The reads of other threads, such as the checksum beside the draw, do not count.
class DrawReadWatch
{
public:
	/// One kind of read: the registry, the call and the components
	struct Read
	{
		std::string registry;
		std::string what;
		uint64_t count {0};
		/// The return addresses of the first read of this kind, from the innermost call. It is empty where the platform
		/// cannot give them.
		std::vector<uintptr_t> stack;
	};

	explicit DrawReadWatch(DrawReadCheck check, std::function<void(const Read&)> onFail = {});

	/// The calling thread starts to draw
	void Start() noexcept;
	/// The draw ends
	void Stop() noexcept;
	[[nodiscard]] bool Watching() const noexcept { return _watching.load(); }
	[[nodiscard]] DrawReadCheck GetCheck() const noexcept { return _check; }

	/// The registry calls it at each read. It counts the read when the watch is on and the calling thread draws.
	void Note(std::string_view registry, std::string_view call, std::initializer_list<std::string_view> components) noexcept;

	/// The kinds of reads, in the order of their first reads
	[[nodiscard]] std::vector<Read> GetReads() const;

	/// The names of the functions and the lines of the return addresses of a read's stack, from the innermost call. It
	/// is empty where the platform cannot find them.
	[[nodiscard]] static std::vector<std::string> DescribeStack(std::span<const uintptr_t> stack);

private:
	DrawReadCheck _check;
	std::function<void(const Read&)> _onFail;
	std::thread::id _thread;
	/// Other threads read it, thus it is atomic. Start writes the thread before it.
	std::atomic<bool> _watching {false};
	mutable std::mutex _readsMutex;
	std::vector<Read> _reads;
};

} // namespace openblack::ecs
