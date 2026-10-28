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

#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The record of the animations that a creature's body plays, for the developer tools. Each game turn, the log gets the
/// clips that the pose of the turn used. A clip that starts opens an entry, and a clip that stops closes it. The log keeps
/// the newest closed entries up to a limit. It also keeps the clips that something asked for but that did not play: a
/// clip that the species does not have, or a clip that the body refused because it played another action.
///
/// The log is not a part of the world: the game turns never read it, and a save does not keep it.
namespace openblack::creature_animation_log
{

/// The layer of the body that plays a clip
enum class Layer : uint8_t
{
	/// What the body plays by itself: the stand, an action that plays once, a start, loop and end, or a dance
	Body,
	/// Clips that play together by weight in place of the body's own: a walk, an action with a thing, a fight
	Blend,
	/// The face's expression
	Face,
	/// A gesture on top of the body, such as a nod
	Gesture,
	/// The wobble that a blow sends through the body
	Wobble,
};

[[nodiscard]] std::string_view Name(Layer layer);

/// A clip as the pose of one turn used it
struct Clip
{
	Layer layer {Layer::Body};
	/// The place of the clip in the creature spec file
	size_t animation {0};
	/// The time in the clip, in milliseconds
	float timeMs {0.0f};
	/// The weight of the clip among the clips that play together, 1 for a clip that plays alone
	float weight {1.0f};
	bool mirrored {false};
	/// The second clip of a pair that blends keyframe by keyframe, and the weight towards it
	std::optional<size_t> blendedWith {};
	float blendWeight {0.0f};
	/// The species has the clip. When it does not, the body shows the stand.
	bool found {true};
	/// The clip plays again from its start at its end
	bool looping {false};
	/// How long the clip lasts, in milliseconds, when the species has it
	std::optional<uint32_t> durationMs {};
};

/// What asked the body for its clips in the turn that a clip started
struct Cause
{
	/// The part of the game that gave the clip: "body", "object action", "fight", "walk" and so on
	std::string by;
	/// The activity of the creature's mind
	std::string activity;
	/// The action of the mind's plan, by its place in the mind's tables, and its name when the tables are there
	std::optional<uint32_t> planAction {};
	std::string planActionName;
	/// The step of the mind's agenda that plays: its place in the agenda, its kind (creature_mind::Step::Kind) and the clip
	/// that it asks for
	std::optional<size_t> agendaStep {};
	std::optional<uint8_t> agendaStepKind {};
	std::optional<size_t> agendaStepClip {};
	/// The action that the creature does with a thing, when it does one
	std::string objectAction;
	/// The stage of the creature's fight, when it fights
	std::string fightStage;
	/// The kind and the phase of the body's own action, as "once", "sequence loop" or "dance start"
	std::string body;
};

/// A clip that played for one or more turns
struct Entry
{
	Clip first;
	Clip last;
	/// The turn of the first pose that used the clip
	uint32_t startTurn {0};
	/// The turn of the first pose that did not use the clip. None while the clip plays.
	std::optional<uint32_t> endTurn {};
	/// How many poses used the clip
	uint32_t turns {0};
	/// How far the clip moved on in the last turn, in milliseconds of the clip
	float lastAdvanceMs {0.0f};
	Cause cause;
};

/// Why an asked clip did not play
enum class Problem : uint8_t
{
	/// The species does not have the clip: the body showed the stand in its place, or the action ended at once
	Missing,
	/// The body played another action, and it refused the new one
	Refused,
};

[[nodiscard]] std::string_view Name(Problem problem);

struct ProblemEntry
{
	Problem problem {Problem::Missing};
	Layer layer {Layer::Body};
	size_t animation {0};
	uint32_t turn {0};
	/// The part of the game that asked for the clip
	std::string by;
	/// What the body did in its place
	std::string detail;
	/// The number of times that the same problem came again in the turns after this one, without a break
	uint32_t repeats {0};
};

/// The log of one creature
class Log
{
public:
	static constexpr size_t k_DefaultCapacity = 256;

	explicit Log(size_t capacity = k_DefaultCapacity);

	/// The pose of a turn used these clips. A clip that is not open opens an entry with the cause. A clip that is open
	/// continues, unless its time went back and it does not loop or relax as a face does: then it played again from its
	/// start. An open clip that
	/// the pose did not use closes at this turn. A clip that the species does not have also makes a Missing problem.
	void Posed(uint32_t turn, std::span<const Clip> clips, const Cause& cause);
	/// A clip did not play. The same problem in the next turn counts as a repeat of this one.
	void Note(ProblemEntry problem);
	void Clear();

	/// The clips that play now, in the order that the last pose used them
	[[nodiscard]] const std::vector<Entry>& Current() const { return _open; }
	/// The closed entries, the oldest first
	[[nodiscard]] const std::deque<Entry>& History() const { return _closed; }
	/// The problems, the oldest first
	[[nodiscard]] const std::deque<ProblemEntry>& Problems() const { return _problems; }
	[[nodiscard]] std::optional<uint32_t> LastTurn() const { return _lastTurn; }

private:
	void Close(Entry entry, uint32_t turn);

	size_t _capacity;
	std::vector<Entry> _open;
	std::deque<Entry> _closed;
	std::deque<ProblemEntry> _problems;
	std::optional<uint32_t> _lastTurn;
};

/// Whether two clips of two turns are the same clip: the same layer, clip, pair and side
[[nodiscard]] bool SameClip(const Clip& a, const Clip& b);

} // namespace openblack::creature_animation_log
