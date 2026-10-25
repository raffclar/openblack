/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DrawTrace.h"

#include <algorithm>
#include <fstream>

#include <fmt/format.h>

#include "GameRandom.h"
#include "LHVM.h"
#include "Locator.h"

using namespace openblack::draw_trace;

std::string openblack::draw_trace::ShortFile(std::string_view path)
{
	std::string out(path);
	std::ranges::replace(out, '\\', '/');
	if (const auto found = out.rfind("/src/"); found != std::string::npos)
	{
		return out.substr(found + 1);
	}
	return out;
}

std::string openblack::draw_trace::DrawLine(uint32_t turn, Phase phase, uint64_t index, const std::source_location& site,
                                            std::string_view context)
{
	return fmt::format("draw\t{}\t{}\t{}\t{}:{}\t{}\t{}", turn, NameOf(phase), index, ShortFile(site.file_name()), site.line(),
	                   site.function_name(), context);
}

DrawTrace::DrawTrace(std::unique_ptr<std::ostream> out)
    : _out(std::move(out))
{
}

std::shared_ptr<DrawTrace> DrawTrace::ToFile(const std::filesystem::path& path)
{
	auto file = std::make_unique<std::ofstream>(path, std::ios::out | std::ios::trunc);
	if (!file->is_open())
	{
		return nullptr;
	}
	return std::make_shared<DrawTrace>(std::move(file));
}

void DrawTrace::BeginTurn(uint32_t turn) noexcept
{
	_turn = turn;
	_phase = Phase::Turn;
}

void DrawTrace::EndTurn()
{
	_phase = Phase::Frame;
	// The file keeps each whole turn, also when a tool stops the program
	_out->flush();
}

void DrawTrace::Draw(uint64_t index, const std::source_location& site)
{
	*_out << DrawLine(_turn, _phase, index, site, _context ? _context() : std::string()) << '\n';
}

void DrawTrace::Restart()
{
	// The new game has no turn yet
	_turn = 0;
	_phase = Phase::Frame;
	// A file trace keeps all lines up to the restart, also when the program stops later
	*_out << "restart\n" << std::flush;
}

void DrawTrace::TaskStopped(std::string_view task)
{
	*_out << "stop\t" << _turn << '\t' << NameOf(_phase) << '\t' << task << '\n';
}

void DrawTrace::SetDraws(uint64_t draws)
{
	*_out << "set\t" << draws << '\n';
}

namespace
{
/// The script task that runs, as its file, its name and the address of its next instruction, or nothing
std::string ScriptTaskContext()
{
	if (!openblack::Locator::vm::has_value())
	{
		return {};
	}
	const auto& vm = openblack::Locator::vm::value();
	const auto number = vm.GetCurrentTaskNumber();
	const auto found = vm.GetTasks().find(number);
	if (number == 0 || found == vm.GetTasks().end())
	{
		return {};
	}
	return fmt::format("script {} {} at {}", found->second.filename, found->second.name, found->second.instructionAddress);
}
} // namespace

bool openblack::draw_trace::Start(GameRandomInterface& random, const std::filesystem::path& path)
{
	auto trace = DrawTrace::ToFile(path);
	if (trace == nullptr)
	{
		return false;
	}
	trace->SetContext(ScriptTaskContext);
	random.SetDrawTrace(std::move(trace));
	return true;
}

TurnScope::TurnScope(DrawTrace* trace, uint32_t turn) noexcept
    : _trace(trace)
{
	if (_trace != nullptr)
	{
		_trace->BeginTurn(turn);
	}
}

TurnScope::~TurnScope()
{
	if (_trace != nullptr)
	{
		_trace->EndTurn();
	}
}
