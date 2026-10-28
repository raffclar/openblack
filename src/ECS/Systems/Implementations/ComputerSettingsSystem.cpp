/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerSettingsSystem.h"

#include <fstream>
#include <iterator>
#include <memory>
#include <system_error>
#include <utility>

#include <spdlog/spdlog.h>

using namespace openblack::ecs::systems;

namespace
{

std::shared_ptr<spdlog::logger> Log()
{
	return spdlog::get("game");
}

/// Takes the changed value when it holds one that differs; true when it did
template <typename T>
bool Take(std::optional<T>& kept, const std::optional<T>& changed)
{
	if (!changed.has_value() || kept == changed)
	{
		return false;
	}
	kept = changed;
	return true;
}

bool TakeAll(openblack::computer_settings::Settings& kept, const openblack::computer_settings::Settings& changed)
{
	bool any = false;
	any = Take(kept.detailLevel, changed.detailLevel) || any;
	any = Take(kept.maxDetailLevel, changed.maxDetailLevel) || any;
	any = Take(kept.effectsVolume, changed.effectsVolume) || any;
	any = Take(kept.musicVolume, changed.musicVolume) || any;
	any = Take(kept.width, changed.width) || any;
	any = Take(kept.height, changed.height) || any;
	any = Take(kept.windowMode, changed.windowMode) || any;
	any = Take(kept.vsync, changed.vsync) || any;
	return any;
}

} // namespace

ComputerSettingsSystem::ComputerSettingsSystem(std::optional<std::filesystem::path> file)
    : _file(std::move(file))
{
	if (!_file.has_value())
	{
		return;
	}
	std::error_code error;
	if (!std::filesystem::exists(*_file, error))
	{
		if (const auto log = Log())
		{
			SPDLOG_LOGGER_INFO(log, "No settings of this computer at {}: the defaults hold", _file->generic_string());
		}
		return;
	}
	std::ifstream stream(*_file, std::ios::binary);
	_text.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
	auto [settings, problems] = computer_settings::Parse(_text);
	_settings = std::move(settings);
	if (const auto log = Log())
	{
		SPDLOG_LOGGER_INFO(log, "The settings of this computer are read from {}", _file->generic_string());
		for (const auto& problem : problems)
		{
			SPDLOG_LOGGER_WARN(log, "In {}: {}; the default holds", _file->generic_string(), problem);
		}
	}
}

void ComputerSettingsSystem::Keep(const computer_settings::Settings& changed)
{
	if (!TakeAll(_settings, changed) || !_file.has_value())
	{
		return;
	}
	auto text = computer_settings::Update(_text, changed);
	std::ofstream stream(*_file, std::ios::binary | std::ios::trunc);
	stream << text;
	stream.close();
	if (!stream)
	{
		if (const auto log = Log())
		{
			SPDLOG_LOGGER_WARN(log, "The settings of this computer cannot be written to {}", _file->generic_string());
		}
		return;
	}
	_text = std::move(text);
}
