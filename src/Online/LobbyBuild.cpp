/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyBuild.h"

#include <fmt/format.h>

#include "Commands/TurnCodec.h"
#include "Common/BuildVersion.h"

namespace openblack::online
{

std::string BuildIdOf(std::string_view versionText, uint32_t codecVersion)
{
	return fmt::format("{}/codec{}", versionText, codecVersion);
}

std::string ThisBuildId()
{
	return BuildIdOf(build_version::Text(), turn_codec::k_Version);
}

} // namespace openblack::online
