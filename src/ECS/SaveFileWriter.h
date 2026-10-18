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

#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

/// The writer of the files of a saved game on a thread of its own, so that a save does not hold up the frames of the
/// game. The game makes the contents of the files on its own thread, then gives the writer one job for each file.
namespace openblack::ecs::save_files
{

class BackgroundWriter
{
public:
	/// A job writes one file and tells whether the file is written
	using Job = std::function<bool()>;
	/// Hears of each job that did not write its file, by the name of the job, on the thread of the writer
	using Failed = std::function<void(const std::string& name)>;

	explicit BackgroundWriter(Failed failed = {});
	/// Writes every job that waits, then stops the thread
	~BackgroundWriter();
	BackgroundWriter(const BackgroundWriter&) = delete;
	BackgroundWriter& operator=(const BackgroundWriter&) = delete;
	BackgroundWriter(BackgroundWriter&&) = delete;
	BackgroundWriter& operator=(BackgroundWriter&&) = delete;

	/// Adds a job after the jobs that wait. The jobs run in the order that they came.
	void Queue(std::string name, Job job);
	/// Waits until every job that came before is done, as before the game reads a saved game back
	void Flush();
	/// The jobs that wait or run now
	[[nodiscard]] size_t Pending() const;
	/// The jobs that did not write their files since the writer started
	[[nodiscard]] size_t Failures() const;

private:
	struct Named
	{
		std::string name;
		Job job;
	};

	void Run(const std::stop_token& stop);

	Failed _failed;
	mutable std::mutex _mutex;
	std::condition_variable_any _wake;
	std::condition_variable _done;
	std::deque<Named> _jobs;
	size_t _running {0};
	size_t _failures {0};
	/// Last, so that it stops before the members that it uses go
	std::jthread _thread;
};

} // namespace openblack::ecs::save_files
