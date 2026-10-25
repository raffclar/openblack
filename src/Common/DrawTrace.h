/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(OPENBLACK_DEVELOPER_TOOLS)
#error "The draw trace is a developer tool: a build without the developer tools has no trace."
#endif

#include <cstdint>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <source_location>
#include <string>
#include <string_view>

/// The draw trace of the developer tools: a list of each draw on the synced seed, with its turn and the place of its call.
/// Two computers of one network game must make the same draws in the same order, so the first line where their traces
/// are not the same shows the call that made the extra draw.
///
/// The trace is a text file with one line for each event, and tabs between the fields:
/// - "draw", the turn, "turn" or "frame", the count of synced draws after the draw, the file and line, the function, and
///   the context: the script task that runs, or nothing.
/// - "restart", when the generator goes back to its start for a new network game.
/// - "set" and the count, when a world load puts back the count of synced draws.
/// - "stop", the turn, the phase and the script task, when a script task stops. A task that stops makes the tasks
///   before it in the list count the turn two times, so a stop at another turn moves the waits of other tasks.
namespace openblack
{
class GameRandomInterface;
}

namespace openblack::draw_trace
{

/// Where a draw occurs: in a game turn, or between the turns (a frame step or other code outside the turns)
enum class Phase : uint8_t
{
	Turn,
	Frame,
};

/// The word for the phase in the trace
[[nodiscard]] constexpr std::string_view NameOf(Phase phase) noexcept
{
	return phase == Phase::Turn ? "turn" : "frame";
}

/// The path of a source file from its "src" folder on, with forward slashes. A path with no "src" folder stays as it is.
[[nodiscard]] std::string ShortFile(std::string_view path);

/// The line of the trace for a draw, without the line end
[[nodiscard]] std::string DrawLine(uint32_t turn, Phase phase, uint64_t index, const std::source_location& site,
                                   std::string_view context = {});

/// Writes the events of the synced generator to a stream. The game turn tells the trace when each turn starts and ends.
class DrawTrace
{
public:
	explicit DrawTrace(std::unique_ptr<std::ostream> out);

	/// A trace that writes to a new file at the path, or none when the file does not open
	[[nodiscard]] static std::shared_ptr<DrawTrace> ToFile(const std::filesystem::path& path);

	/// The game turn with this number starts. The draws until the end of the turn are in the turn phase.
	void BeginTurn(uint32_t turn) noexcept;
	/// The game turn ends. The draws until the next turn are in the frame phase.
	void EndTurn();

	/// Records a draw on the synced seed. The index is the count of synced draws after this draw.
	void Draw(uint64_t index, const std::source_location& site);
	/// Records that the generator goes back to its start. The turn goes back to 0.
	void Restart();
	/// Records that a world load puts back the count of synced draws
	void SetDraws(uint64_t draws);
	/// Records that a script task stops
	void TaskStopped(std::string_view task);
	/// The function that gives the context of each draw, such as the script task that runs. None gives no context.
	void SetContext(std::function<std::string()> context) { _context = std::move(context); }

	[[nodiscard]] uint32_t Turn() const noexcept { return _turn; }
	[[nodiscard]] Phase CurrentPhase() const noexcept { return _phase; }

private:
	std::unique_ptr<std::ostream> _out;
	uint32_t _turn {0};
	Phase _phase {Phase::Frame};
	std::function<std::string()> _context;
};

/// Starts a trace of the generator's synced draws in a new file at the path, with the script task that runs as the
/// context of each draw. False when the file does not open.
[[nodiscard]] bool Start(GameRandomInterface& random, const std::filesystem::path& path);

/// Tells a trace that a turn starts, and that it ends when the scope ends. A null trace does nothing.
class TurnScope
{
public:
	TurnScope(DrawTrace* trace, uint32_t turn) noexcept;
	~TurnScope();
	TurnScope(const TurnScope&) = delete;
	TurnScope& operator=(const TurnScope&) = delete;
	TurnScope(TurnScope&&) = delete;
	TurnScope& operator=(TurnScope&&) = delete;

private:
	DrawTrace* _trace;
};

} // namespace openblack::draw_trace
