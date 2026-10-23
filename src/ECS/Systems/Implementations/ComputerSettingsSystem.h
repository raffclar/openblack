/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "ECS/Systems/ComputerSettingsSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class ComputerSettingsSystem final: public ComputerSettingsSystemInterface
{
public:
	/// Reads the file, if there is one. Without a file the settings are the defaults and nothing is written.
	explicit ComputerSettingsSystem(std::optional<std::filesystem::path> file);

	[[nodiscard]] const computer_settings::Settings& GetSettings() const override { return _settings; }
	void Keep(const computer_settings::Settings& changed) override;

private:
	std::optional<std::filesystem::path> _file;
	computer_settings::Settings _settings;
	/// The text of the file as it was read or last written, so that its other members stay
	std::string _text;
};

} // namespace openblack::ecs::systems
