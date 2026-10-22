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
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace openblack::filesystem
{
class FileSystemInterface;
}

/// The store of the refresh token of the lobby, which keeps the player logged in between two runs of the game. The
/// operating system encrypts the token for the user, so that another user or a copy of the file cannot use it:
/// - Windows: a file in the profile folder, encrypted with the data protection of Windows (DPAPI) for the user.
/// - macOS: an item in the keychain of the user.
/// - Linux: an item in the secret service of the user (libsecret).
/// - The browser: the lobby keeps the token in an HttpOnly cookie. The script of the page cannot read it, and the game
///   keeps nothing.
namespace openblack::online
{

class TokenStoreInterface
{
public:
	virtual ~TokenStoreInterface() = default;

	/// The token that the store keeps, if there is one that the store can decrypt
	[[nodiscard]] virtual std::optional<std::string> Load() = 0;
	/// Keeps the token in place of the token before it. False when the store cannot keep it.
	virtual bool Save(std::string_view token) = 0;
	/// Removes the token
	virtual void Clear() = 0;
};

/// A store that keeps nothing. The browser uses it, because the cookie holds the token. A desktop with no store of
/// the operating system uses it too: the player then logs in at each run of the game.
class NoTokenStore final: public TokenStoreInterface
{
public:
	[[nodiscard]] std::optional<std::string> Load() override { return std::nullopt; }
	bool Save(std::string_view /*token*/) override { return false; }
	void Clear() override {}
};

/// The name of the file of the token in the profile folder, on Windows
constexpr std::string_view k_TokenFileName = "lobby_session.dat";

/// The store of this platform. On Windows the file is in the folder given. None of the stores for macOS and Linux
/// use the folder, but they keep the item for that folder, so that two installs keep two tokens.
[[nodiscard]] std::unique_ptr<TokenStoreInterface> MakeTokenStore(filesystem::FileSystemInterface& files,
                                                                  const std::optional<std::filesystem::path>& folder);

} // namespace openblack::online
