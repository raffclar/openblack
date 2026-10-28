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

#include "Common/PlayerProfiles.h"
#include "Online/CreatureUploadDialog.h"
#include "Online/HttpClientInterface.h"

namespace openblack::creature_upload::app
{

/// What the tool knows when it starts
struct Setup
{
	/// The folder of the game, for the language file and the icon of the game's tool. None when the tool cannot find it.
	std::optional<std::filesystem::path> gamePath;
	player_profiles::Store store;
	player_profiles::Profiles profiles;
	/// The https:// origin of the lobby
	std::string origin;
};

/// The files of the creature of the profile with the name, read and checked
[[nodiscard]] dialog::GatherFiles GatherFor(const Setup& setup, const std::string& profileName);

/// The request that does the sign-out can continue after the dialog closes. This waits for it a short time.
void FinishRequests(online::HttpClientInterface& http);

#if defined(_WIN32)
/// Shows the dialog of the tool until the player closes it
int RunDialog(const Setup& setup, online::HttpClientInterface& http, dialog::UploadFlow& flow, const dialog::Texts& texts);
/// For the look page: writes pictures of the dialog of this tool and of the dialog of the game's tool in the stages of
/// an upload, and the texts of the messages. The player cannot see the dialogs, and they do not take the focus. No request goes
/// to the network.
int WriteLookPictures(const Setup& setup, const dialog::Texts& texts, const std::filesystem::path& folder);
#endif

} // namespace openblack::creature_upload::app
