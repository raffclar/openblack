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

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace openblack
{

/// The services that only the main thread may use
enum class MainThreadApi : uint8_t
{
	/// The calls of the graphics library: buffers, textures and their updates
	Graphics,
	/// The renderer and the rendering system: draws, screen pictures and the drawn objects
	Renderer,
	/// The sounds and the music
	Audio,

	_Count
};

/// The names of the services, in the order of the enumeration, for the logs
inline constexpr std::array<std::string_view, static_cast<size_t>(MainThreadApi::_Count)> k_MainThreadApiNames {
    "graphics",
    "renderer",
    "audio",
};

/// The name of a service that only the main thread may use
[[nodiscard]] constexpr std::string_view Name(MainThreadApi api)
{
	return api < MainThreadApi::_Count ? k_MainThreadApiNames.at(static_cast<size_t>(api)) : std::string_view("unknown");
}

/// What the check does when turn code uses a service of the main thread directly, not through the queue
enum class TurnApiCheck : uint8_t
{
	/// There is no check
	Off,
	/// The check counts each place, and the log gets the first call of each place
	Report,
	/// The first call stops the game
	Fail,
};

/// A place where turn code used a service of the main thread directly
struct DirectCall
{
	MainThreadApi api;
	/// The function of the service that the turn code called
	std::string what;
	/// The file and the line of that function
	std::string where;
	/// The step of the turn that made the call, if the turn told the queue its steps
	std::optional<uint32_t> turnStep;
	uint32_t count {0};
};

/// The work that a game turn gives to the main thread.
///
/// The graphics library, the renderer and the audio are for the main thread only. A turn that needs them posts its work
/// here. When the turn runs on the main thread, as it does now, the work runs at once, at the same point of the turn and
/// in the same order as a direct call. When the turn runs on another thread, the work waits, and the main thread runs it
/// in the order of the posts when it calls RunPending.
///
/// The check of the turn's calls (only in builds with the developer tools) finds the turn code that uses these services
/// directly: such code is not safe when the turn runs on another thread.
class MainThreadQueue
{
public:
	/// The thread that makes the queue is its main thread
	MainThreadQueue();
	MainThreadQueue(const MainThreadQueue&) = delete;
	MainThreadQueue& operator=(const MainThreadQueue&) = delete;
	~MainThreadQueue();

	/// Opened by the main thread: while it is open, the posts of the main thread wait too, as the posts of a turn on
	/// another thread wait. Tools and tests use it.
	class Deferral
	{
	public:
		explicit Deferral(MainThreadQueue& queue);
		Deferral(const Deferral&) = delete;
		Deferral& operator=(const Deferral&) = delete;
		~Deferral();

	private:
		MainThreadQueue& _queue;
	};

	/// Open while the calling thread does the steps of a game turn
	class TurnScope
	{
	public:
		explicit TurnScope(MainThreadQueue& queue);
		TurnScope(const TurnScope&) = delete;
		TurnScope& operator=(const TurnScope&) = delete;
		~TurnScope();

	private:
		MainThreadQueue* _previous;
		std::optional<uint32_t> _previousStep;
	};

	/// Open while the calling thread does work from the queue, which may use the services of the main thread
	class WorkScope
	{
	public:
		WorkScope();
		WorkScope(const WorkScope&) = delete;
		WorkScope& operator=(const WorkScope&) = delete;
		~WorkScope();
	};

	/// Gives work to the main thread. It runs at once, unless the post comes from a turn on another thread or from the
	/// main thread while a deferral is open. Work that waits keeps copies of what it uses.
	template <typename Work>
	void Post(MainThreadApi api, Work&& work)
	{
		static_assert(std::is_invocable_v<Work&>, "The work takes no arguments");
		if (RunsAtOnce())
		{
			const WorkScope scope;
			std::invoke(work);
			return;
		}
		Keep(api, std::function<void()>(std::forward<Work>(work)));
	}

	/// Runs the work that waits, in the order of the posts, on the main thread. Work that it posts runs too.
	/// The number of pieces of work done.
	size_t RunPending();

	/// The number of pieces of work that wait
	[[nodiscard]] size_t GetPendingCount() const;
	/// The pieces of work that wait for each service
	[[nodiscard]] std::array<size_t, static_cast<size_t>(MainThreadApi::_Count)> GetPendingByApi() const;

	[[nodiscard]] bool IsMainThread() const noexcept;
	/// A post from the calling thread runs at once
	[[nodiscard]] bool RunsAtOnce() const noexcept;
	/// The calling thread is in a turn, and not in work from the queue
	[[nodiscard]] bool InTurnCode() const noexcept;

	/// The turn tells the queue which of its steps it does, so that the check can name it
	void SetTurnStep(std::optional<uint32_t> step) noexcept;

	void SetCheck(TurnApiCheck check) noexcept;
	[[nodiscard]] TurnApiCheck GetCheck() const noexcept;
	/// What the Fail check does at the first direct call. Without one, the check writes the call to the log and stops the
	/// game.
	void SetFailHandler(std::function<void(const DirectCall&)> handler);
	/// The places where turn code used the services directly, in the order of their first calls
	[[nodiscard]] std::vector<DirectCall> GetDirectCalls() const;
	void ClearDirectCalls();

	/// The entry points of the services call it. When the check is on and the calling thread is in turn code, the check
	/// counts the call. It does nothing in builds without the developer tools.
	static void CheckDirect(MainThreadApi api, std::string_view what,
	                        std::source_location where = std::source_location::current()) noexcept;

private:
	struct Pending
	{
		MainThreadApi api;
		std::function<void()> work;
	};

	void Keep(MainThreadApi api, std::function<void()> work);
	void Record(MainThreadApi api, std::string_view what, const std::source_location& where) noexcept;

	const std::thread::id _mainThread;
	/// The deferrals open on the main thread. Only the main thread uses it.
	int _deferrals {0};
	mutable std::mutex _pendingMutex;
	std::vector<Pending> _pending;
	std::atomic<TurnApiCheck> _check {TurnApiCheck::Off};
	mutable std::mutex _callsMutex;
	std::vector<DirectCall> _calls;
	std::function<void(const DirectCall&)> _onFail;
};

} // namespace openblack
