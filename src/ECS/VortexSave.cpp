/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VortexSave.h"

#include <fstream>

#include <spdlog/spdlog.h>

#include "ECS/Systems/ScriptStateInterface.h"
#include "LHScriptX/Script.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// What this module keeps between calls (Locator::scriptState)
struct VortexSaveState
{
	std::optional<std::filesystem::path> saveFolder {};
};

VortexSaveState& VortexSaveData()
{
	return openblack::Locator::scriptState::value().Get<VortexSaveState>();
}
} // namespace

void vortex_save::SetSaveFolder(std::optional<std::filesystem::path> folder)
{
	VortexSaveData().saveFolder = std::move(folder);
}

std::optional<std::filesystem::path> vortex_save::SaveFolder()
{
	return VortexSaveData().saveFolder;
}

std::shared_ptr<vortex_save::Reader> vortex_save::OpenReader()
{
	auto& state = VortexSaveData();
	// the Out always has a reader: without a file its first line fails, the Out deletes it that even turn and makes
	// no villager then; the next even turn starts the 30.
	// (pending) the save folder: openblack has no saved games, so no file is read yet
	auto reader = std::make_shared<Reader>();
	if (!state.saveFolder.has_value())
	{
		return reader;
	}
	// "vortex.txt" in the save folder
	std::ifstream file(*state.saveFolder / "vortex.txt");
	if (!file)
	{
		return reader;
	}
	// (pending) the interpreter's own line reading (blank lines, comments): here one text line is one command
	for (std::string line; std::getline(file, line);)
	{
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}
		reader->lines.push_back(std::move(line));
	}
	return reader;
}

std::optional<entt::entity> vortex_save::ReadNext(Reader& reader, const map_coords::MapCoords& at)
{
	reader.reads += 1;
	if (reader.next >= reader.lines.size())
	{
		return std::nullopt; // the end of the file: no object
	}
	const auto& line = reader.lines[reader.next++];
	// the position offset and no tribe override. (pending) the script's player = the local player: not ported (no
	// command reads it yet)
	lhscriptx::Script::SetPositionOffset(at);
	lhscriptx::Script::SetTribeOverride(std::nullopt);
	// (openblack) the original does not clear the last created object before the line (docs/bw1-notes/vortex.md): the
	// same while every CREATE_* sets it. (pending) only the villager commands set it yet, so it is cleared here: a line
	// that creates something else gives null instead of handing the previous villager over again
	lhscriptx::Script::SetLastCreated(entt::null);
	// (openblack) the original leaves the position offset pointing at the caller's coordinates until the next loader
	// sets it; here it is cleared after the line, even when the interpreter throws, so that a console line or another
	// script does not get the vortex's offset
	struct OffsetGuard
	{
		~OffsetGuard() { lhscriptx::Script::SetPositionOffset(std::nullopt); }
	} guard;
	try
	{
		lhscriptx::Script script;
		script.Load(line);
	}
	catch (const std::exception& e)
	{
		// (openblack) a line the interpreter cannot take is skipped, as the map loader does
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "VortexSave: line {} not read ({})", reader.next, e.what());
		return entt::entity {entt::null};
	}
	return lhscriptx::Script::LastCreated();
}
