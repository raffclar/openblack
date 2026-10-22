/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Online/TokenStoreInterface.h"

namespace openblack::online
{

/// The file of the encrypted token: the text "OBLT", the version 1 as one byte, then the encrypted bytes
namespace token_file
{
constexpr std::array<uint8_t, 4> k_Magic {'O', 'B', 'L', 'T'};
constexpr uint8_t k_Version = 1;

/// The bytes of the file for the encrypted bytes
[[nodiscard]] std::vector<uint8_t> Pack(std::span<const uint8_t> encrypted);
/// The encrypted bytes of the file, none when the file is not a token file of this version
[[nodiscard]] std::optional<std::vector<uint8_t>> Unpack(std::span<const uint8_t> file);
} // namespace token_file

#if defined(_WIN32)
/// Windows: the token is in a file in the profile folder, encrypted with the data protection of Windows for the user who
/// runs the game. Only that user on that computer can decrypt it.
class DpapiTokenStore final: public TokenStoreInterface
{
public:
	DpapiTokenStore(filesystem::FileSystemInterface& files, std::filesystem::path file);

	[[nodiscard]] std::optional<std::string> Load() override;
	bool Save(std::string_view token) override;
	void Clear() override;

	/// The encryption and the decryption alone, for the tests
	[[nodiscard]] static std::optional<std::vector<uint8_t>> Protect(std::string_view token);
	[[nodiscard]] static std::optional<std::string> Unprotect(std::span<const uint8_t> encrypted);

private:
	filesystem::FileSystemInterface& _files;
	std::filesystem::path _file;
};
#endif

#if defined(__APPLE__)
/// macOS: the token is a generic password item in the keychain of the user. The account of the item is the profile
/// folder, so that two installs keep two tokens.
class KeychainTokenStore final: public TokenStoreInterface
{
public:
	explicit KeychainTokenStore(std::string account);

	[[nodiscard]] std::optional<std::string> Load() override;
	bool Save(std::string_view token) override;
	void Clear() override;

private:
	std::string _account;
};
#endif

#if defined(__linux__) && defined(OPENBLACK_HAVE_LIBSECRET)
/// Linux: the token is an item in the secret service of the user (GNOME Keyring, KWallet), through libsecret. The
/// attribute of the item is the profile folder, so that two installs keep two tokens.
class SecretServiceTokenStore final: public TokenStoreInterface
{
public:
	explicit SecretServiceTokenStore(std::string folder);

	[[nodiscard]] std::optional<std::string> Load() override;
	bool Save(std::string_view token) override;
	void Clear() override;

private:
	std::string _folder;
};
#endif

} // namespace openblack::online
