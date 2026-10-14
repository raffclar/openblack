/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FightTranscriptSystem.h"

#include <format>
#include <system_error>
#include <tuple>

#include <entt/entity/entity.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

void FightTranscriptSystem::Frame(uint64_t frame, float gameMs)
{
	_frame = frame;
	_gameMs += gameMs;
}

fight_transcript::Stamp FightTranscriptSystem::Now() const
{
	return {.frame = _frame, .turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u, .gameMs = _gameMs};
}

uint32_t FightTranscriptSystem::Begin(entt::entity first, entt::entity second, fight_transcript::Fields fields)
{
	const auto id = _recorder.Begin(entt::to_integral(first), entt::to_integral(second), Now(), std::move(fields));
	Write(id);
	return id;
}

std::optional<uint64_t> FightTranscriptSystem::Note(entt::entity creature, std::string_view kind,
                                                    fight_transcript::Fields fields, std::optional<uint64_t> cause)
{
	if (creature == entt::null)
	{
		return std::nullopt;
	}
	const auto index = _recorder.Add(entt::to_integral(creature), kind, Now(), std::move(fields), cause);
	if (index.has_value())
	{
		Write(*_recorder.FightOf(entt::to_integral(creature)));
	}
	return index;
}

std::optional<uint64_t> FightTranscriptSystem::LastOf(entt::entity creature, std::string_view kind) const
{
	return creature == entt::null ? std::nullopt : _recorder.LastOf(entt::to_integral(creature), kind);
}

void FightTranscriptSystem::End(entt::entity creature)
{
	if (const auto fight = _recorder.FightOf(entt::to_integral(creature)); fight.has_value())
	{
		_recorder.End(*fight, Now());
	}
}

void FightTranscriptSystem::ScriptRead(entt::entity creature, std::string_view native, double answer)
{
	if (creature == entt::null)
	{
		return;
	}
	const auto fight = _recorder.FightOf(entt::to_integral(creature));
	if (!fight.has_value())
	{
		return;
	}
	const auto key = std::tuple(*fight, entt::to_integral(creature), std::string(native));
	auto found = _reads.find(key);
	if (found != _reads.end() && found->second.answer == answer)
	{
		++found->second.repeats;
		return;
	}
	const auto repeats = found != _reads.end() ? found->second.repeats : 0u;
	const auto index = Note(creature, "script_read",
	                        {{"creature", static_cast<int64_t>(entt::to_integral(creature))},
	                         {"native", native},
	                         {"answer", answer},
	                         {"same_reads_before", repeats}},
	                        std::nullopt);
	if (index.has_value())
	{
		_reads[key] = {.answer = answer, .repeats = 0};
	}
}

void FightTranscriptSystem::Clear()
{
	_recorder.Clear();
	_files.clear();
	_reads.clear();
}

void FightTranscriptSystem::SetOutputFolder(std::optional<std::filesystem::path> folder)
{
	_files.clear();
	_folder = std::move(folder);
	if (_folder.has_value())
	{
		std::error_code error;
		std::filesystem::create_directories(*_folder, error);
		if (error)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The fight transcript cannot use the folder {}: {}",
			                   _folder->generic_string(), error.message());
			_folder.reset();
		}
	}
}

void FightTranscriptSystem::Write(uint32_t fight)
{
	if (!_folder.has_value())
	{
		return;
	}
	const auto* found = _recorder.Find(fight);
	if (found == nullptr || found->entries.empty())
	{
		return;
	}
	auto file = _files.find(fight);
	if (file == _files.end())
	{
		const auto path = *_folder / std::format("fight-{}.jsonl", fight);
		file = _files.emplace(fight, std::ofstream(path, std::ios::app)).first;
	}
	file->second << fight_transcript::ToJsonLine(*found, found->entries.back()) << '\n';
	file->second.flush();
}
