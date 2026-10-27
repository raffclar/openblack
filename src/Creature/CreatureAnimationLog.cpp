/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureAnimationLog.h"

#include <algorithm>
#include <utility>

namespace openblack::creature_animation_log
{

std::string_view Name(Layer layer)
{
	switch (layer)
	{
	case Layer::Body:
		return "body";
	case Layer::Blend:
		return "blend";
	case Layer::Face:
		return "face";
	case Layer::Gesture:
		return "gesture";
	case Layer::Wobble:
		return "wobble";
	}
	return "body";
}

std::string_view Name(Problem problem)
{
	switch (problem)
	{
	case Problem::Missing:
		return "missing";
	case Problem::Refused:
		return "refused";
	}
	return "missing";
}

bool SameClip(const Clip& a, const Clip& b)
{
	return a.layer == b.layer && a.animation == b.animation && a.blendedWith == b.blendedWith && a.mirrored == b.mirrored;
}

Log::Log(size_t capacity)
    : _capacity(std::max<size_t>(capacity, 1))
{
}

void Log::Close(Entry entry, uint32_t turn)
{
	entry.endTurn = turn;
	_closed.push_back(std::move(entry));
	while (_closed.size() > _capacity)
	{
		_closed.pop_front();
	}
}

void Log::Posed(uint32_t turn, std::span<const Clip> clips, const Cause& cause)
{
	std::vector<Entry> open;
	open.reserve(clips.size());
	for (const auto& clip : clips)
	{
		const auto found = std::ranges::find_if(_open, [&clip](const Entry& entry) { return SameClip(entry.last, clip); });
		// A clip that does not loop and whose time went back played again from its start
		const bool continues = found != _open.end() && (clip.looping || clip.timeMs >= found->last.timeMs);
		if (continues)
		{
			auto entry = std::move(*found);
			_open.erase(found);
			entry.lastAdvanceMs = clip.timeMs - entry.last.timeMs;
			if (entry.lastAdvanceMs < 0.0f && clip.durationMs.has_value())
			{
				entry.lastAdvanceMs += static_cast<float>(*clip.durationMs);
			}
			entry.last = clip;
			++entry.turns;
			open.push_back(std::move(entry));
			continue;
		}
		if (found != _open.end())
		{
			auto restarted = std::move(*found);
			_open.erase(found);
			Close(std::move(restarted), turn);
		}
		open.push_back({.first = clip, .last = clip, .startTurn = turn, .turns = 1, .cause = cause});
		if (!clip.found)
		{
			Note({.problem = Problem::Missing,
			      .layer = clip.layer,
			      .animation = clip.animation,
			      .turn = turn,
			      .by = cause.by,
			      .detail = "the species has no such clip: the body showed the stand"});
		}
	}
	for (auto& entry : _open)
	{
		Close(std::move(entry), turn);
	}
	_open = std::move(open);
	_lastTurn = turn;
}

void Log::Note(ProblemEntry problem)
{
	if (!_problems.empty())
	{
		auto& last = _problems.back();
		const auto lastSeen = last.turn + last.repeats;
		if (last.problem == problem.problem && last.layer == problem.layer && last.animation == problem.animation &&
		    last.by == problem.by && (problem.turn == lastSeen || problem.turn == lastSeen + 1))
		{
			last.repeats += problem.turn == lastSeen ? 0 : 1;
			return;
		}
	}
	_problems.push_back(std::move(problem));
	while (_problems.size() > _capacity)
	{
		_problems.pop_front();
	}
}

void Log::Clear()
{
	_open.clear();
	_closed.clear();
	_problems.clear();
	_lastTurn.reset();
}

} // namespace openblack::creature_animation_log
