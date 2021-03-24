/*****************************************************************************
 * Copyright (c) 2018-2021 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <string>
#include <filesystem>

namespace openblack
{
class Level
{
public:
	Level(std::filesystem::path path) : _scriptPath(path), _name(path.filename().string()) {}
	const std::string& GetName() const { return _name; };
	const std::filesystem::path& GetScriptPath() const { return _scriptPath; };
private:
	std::string _name;
	std::filesystem::path _scriptPath;
};
}
