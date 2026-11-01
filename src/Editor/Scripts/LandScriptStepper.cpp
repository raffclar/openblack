/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandScriptStepper.h"

#include <cctype>

#include <algorithm>
#include <ranges>

#include <fmt/format.h>

namespace openblack::editor::scripts
{

namespace
{
[[nodiscard]] char Lower(char c)
{
	return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

[[nodiscard]] bool Holds(std::string_view text, std::string_view filter)
{
	const auto found = std::ranges::search(text, filter, {}, Lower, Lower);
	return !found.empty() || filter.empty();
}
} // namespace

std::string ArgumentText(const lhscriptx::Token& argument)
{
	using Type = lhscriptx::Token::Type;
	switch (argument.GetType())
	{
	case Type::Identifier:
		return argument.Identifier();
	case Type::String:
		return fmt::format("\"{}\"", argument.StringValue());
	case Type::Integer:
		return fmt::format("{}", *argument.IntegerValue());
	case Type::Float:
		return fmt::format("{}", *argument.FloatValue());
	default:
		return "?";
	}
}

std::string ArgumentsText(const lhscriptx::Command& command)
{
	std::string text;
	for (const auto& argument : command.arguments)
	{
		if (!text.empty())
		{
			text += ", ";
		}
		text += ArgumentText(argument);
	}
	return text;
}

bool CommandMatches(const lhscriptx::Command& command, std::string_view filter)
{
	return Holds(command.name, filter) || Holds(ArgumentsText(command), filter);
}

LandScriptStepper::LandScriptStepper(size_t count)
{
	Reset(count);
}

void LandScriptStepper::Reset(size_t count)
{
	_states.assign(count, State::NotDone);
	_breakpoints.clear();
	_next = 0;
}

void LandScriptStepper::Restart() noexcept
{
	std::ranges::fill(_states, State::NotDone);
	_next = 0;
}

std::optional<size_t> LandScriptStepper::Next() const noexcept
{
	return _next < _states.size() ? std::optional(_next) : std::nullopt;
}

LandScriptStepper::State LandScriptStepper::StateOf(size_t index) const noexcept
{
	return index < _states.size() ? _states[index] : State::NotDone;
}

void LandScriptStepper::ToggleBreakpoint(size_t index)
{
	if (index >= _states.size())
	{
		return;
	}
	if (!_breakpoints.erase(index))
	{
		_breakpoints.insert(index);
	}
}

bool LandScriptStepper::DoNext(const Run& run)
{
	const auto index = _next++;
	const bool done = run(index);
	_states[index] = done ? State::Done : State::Failed;
	return done;
}

bool LandScriptStepper::Step(const Run& run)
{
	if (!Next().has_value())
	{
		return false;
	}
	DoNext(run);
	return true;
}

size_t LandScriptStepper::Continue(const Run& run)
{
	size_t count = 0;
	while (Next().has_value())
	{
		// The first command runs also when it has a breakpoint, so that a run can continue past a breakpoint
		if (count > 0 && HasBreakpoint(_next))
		{
			break;
		}
		++count;
		if (!DoNext(run))
		{
			break;
		}
	}
	return count;
}

size_t LandScriptStepper::RunTo(size_t index, const Run& run)
{
	size_t count = 0;
	while (_next < std::min(index, _states.size()))
	{
		++count;
		if (!DoNext(run))
		{
			break;
		}
	}
	return count;
}

void LandScriptStepper::Skip() noexcept
{
	if (_next < _states.size())
	{
		_states[_next++] = State::Skipped;
	}
}

} // namespace openblack::editor::scripts
