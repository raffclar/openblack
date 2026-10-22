/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OsTokenStore.h"

#include <algorithm>
#include <exception>
#include <memory>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"

#if defined(_WIN32)
#include <windows.h>
// The data protection of Windows comes after windows.h, and its types before its functions
#include <wincrypt.h>
// clang-format off
#include <dpapi.h>
// clang-format on
#endif

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif

#if defined(__linux__) && defined(OPENBLACK_HAVE_LIBSECRET)
#include <libsecret/secret.h>
#endif

namespace openblack::online
{

namespace token_file
{

std::vector<uint8_t> Pack(std::span<const uint8_t> encrypted)
{
	std::vector<uint8_t> file(k_Magic.begin(), k_Magic.end());
	file.push_back(k_Version);
	file.insert(file.end(), encrypted.begin(), encrypted.end());
	return file;
}

std::optional<std::vector<uint8_t>> Unpack(std::span<const uint8_t> file)
{
	if (file.size() <= k_Magic.size() + 1 || !std::ranges::equal(file.first(k_Magic.size()), k_Magic) ||
	    file[k_Magic.size()] != k_Version)
	{
		return std::nullopt;
	}
	const auto encrypted = file.subspan(k_Magic.size() + 1);
	return std::vector<uint8_t>(encrypted.begin(), encrypted.end());
}

} // namespace token_file

#if defined(_WIN32)
namespace
{

/// Extra bytes for the encryption, so that another program that uses the data protection of Windows for the same user
/// cannot decrypt the token by chance
constexpr std::string_view k_Entropy = "openblack lobby refresh token";

struct LocalDeleter
{
	void operator()(BYTE* bytes) const { LocalFree(bytes); }
};

DATA_BLOB BlobOf(std::span<const uint8_t> bytes)
{
	return DATA_BLOB {
	    .cbData = static_cast<DWORD>(bytes.size()),
	    .pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(bytes.data())),
	};
}

} // namespace

DpapiTokenStore::DpapiTokenStore(filesystem::FileSystemInterface& files, std::filesystem::path file)
    : _files(files)
    , _file(std::move(file))
{
}

std::optional<std::vector<uint8_t>> DpapiTokenStore::Protect(std::string_view token)
{
	auto in = BlobOf({reinterpret_cast<const uint8_t*>(token.data()), token.size()});
	auto entropy = BlobOf({reinterpret_cast<const uint8_t*>(k_Entropy.data()), k_Entropy.size()});
	DATA_BLOB out {};
	if (CryptProtectData(&in, L"openblack lobby", &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out) == FALSE)
	{
		return std::nullopt;
	}
	const std::unique_ptr<BYTE, LocalDeleter> owned(out.pbData);
	return std::vector<uint8_t>(out.pbData, out.pbData + out.cbData);
}

std::optional<std::string> DpapiTokenStore::Unprotect(std::span<const uint8_t> encrypted)
{
	auto in = BlobOf(encrypted);
	auto entropy = BlobOf({reinterpret_cast<const uint8_t*>(k_Entropy.data()), k_Entropy.size()});
	DATA_BLOB out {};
	if (CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out) == FALSE)
	{
		return std::nullopt;
	}
	const std::unique_ptr<BYTE, LocalDeleter> owned(out.pbData);
	std::string token(reinterpret_cast<const char*>(out.pbData), out.cbData);
	// The decrypted token does not stay in the memory that Windows gave
	SecureZeroMemory(out.pbData, out.cbData);
	return token;
}

std::optional<std::string> DpapiTokenStore::Load()
{
	std::vector<uint8_t> file;
	try
	{
		if (!_files.Exists(_file))
		{
			return std::nullopt;
		}
		file = _files.ReadAll(_file);
	}
	catch (const std::exception& error)
	{
		if (const auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_WARN(logger, "Could not read the lobby session file: {}", error.what());
		}
		return std::nullopt;
	}
	const auto encrypted = token_file::Unpack(file);
	if (!encrypted.has_value())
	{
		return std::nullopt;
	}
	return Unprotect(*encrypted);
}

bool DpapiTokenStore::Save(std::string_view token)
{
	const auto encrypted = Protect(token);
	if (!encrypted.has_value())
	{
		if (const auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_WARN(logger, "Windows could not encrypt the lobby session");
		}
		return false;
	}
	// The new file replaces the old file in one step, so that a stop of the game cannot leave half a file
	auto temporary = _file;
	temporary += ".new";
	if (!_files.MakeFolder(_file.parent_path()) || !_files.WriteAll(temporary, token_file::Pack(*encrypted)) ||
	    !_files.Rename(temporary, _file))
	{
		if (const auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_WARN(logger, "Could not write the lobby session file");
		}
		return false;
	}
	return true;
}

void DpapiTokenStore::Clear()
{
	if (_files.Exists(_file))
	{
		_files.Remove(_file);
	}
}
#endif

#if defined(__APPLE__)
namespace
{

constexpr std::string_view k_KeychainService = "openblack lobby";

struct CfDeleter
{
	void operator()(CFTypeRef value) const
	{
		if (value != nullptr)
		{
			CFRelease(value);
		}
	}
};

using CfPointer = std::unique_ptr<const void, CfDeleter>;

CfPointer CfText(std::string_view text)
{
	return CfPointer(CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8*>(text.data()),
	                                         static_cast<CFIndex>(text.size()), kCFStringEncodingUTF8, false));
}

/// The query of the item of an account, with more keys and values after the keys of the item
CfPointer Query(const std::string& account, std::span<const void* const> keys = {}, std::span<const void* const> values = {})
{
	const auto service = CfText(k_KeychainService);
	const auto name = CfText(account);
	std::vector<const void*> allKeys {kSecClass, kSecAttrService, kSecAttrAccount};
	std::vector<const void*> allValues {kSecClassGenericPassword, service.get(), name.get()};
	allKeys.insert(allKeys.end(), keys.begin(), keys.end());
	allValues.insert(allValues.end(), values.begin(), values.end());
	return CfPointer(CFDictionaryCreate(nullptr, allKeys.data(), allValues.data(), static_cast<CFIndex>(allKeys.size()),
	                                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

} // namespace

KeychainTokenStore::KeychainTokenStore(std::string account)
    : _account(std::move(account))
{
}

std::optional<std::string> KeychainTokenStore::Load()
{
	const std::array<const void*, 2> keys {kSecReturnData, kSecMatchLimit};
	const std::array<const void*, 2> values {kCFBooleanTrue, kSecMatchLimitOne};
	const auto query = Query(_account, keys, values);
	CFTypeRef found = nullptr;
	if (SecItemCopyMatching(static_cast<CFDictionaryRef>(query.get()), &found) != errSecSuccess || found == nullptr)
	{
		return std::nullopt;
	}
	const CfPointer data(found);
	const auto* bytes = CFDataGetBytePtr(static_cast<CFDataRef>(found));
	return std::string(reinterpret_cast<const char*>(bytes),
	                   static_cast<size_t>(CFDataGetLength(static_cast<CFDataRef>(found))));
}

bool KeychainTokenStore::Save(std::string_view token)
{
	Clear();
	const CfPointer data(
	    CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(token.data()), static_cast<CFIndex>(token.size())));
	// The item is for this computer only, and only while the user is logged in
	const std::array<const void*, 2> keys {kSecValueData, kSecAttrAccessible};
	const std::array<const void*, 2> values {data.get(), kSecAttrAccessibleWhenUnlockedThisDeviceOnly};
	const auto item = Query(_account, keys, values);
	return SecItemAdd(static_cast<CFDictionaryRef>(item.get()), nullptr) == errSecSuccess;
}

void KeychainTokenStore::Clear()
{
	const auto query = Query(_account);
	SecItemDelete(static_cast<CFDictionaryRef>(query.get()));
}
#endif

#if defined(__linux__) && defined(OPENBLACK_HAVE_LIBSECRET)
namespace
{

const SecretSchema* Schema()
{
	static const SecretSchema schema = {
	    "org.openblack.LobbySession",
	    SECRET_SCHEMA_NONE,
	    {
	        {"folder", SECRET_SCHEMA_ATTRIBUTE_STRING},
	        {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
	    },
	};
	return &schema;
}

} // namespace

SecretServiceTokenStore::SecretServiceTokenStore(std::string folder)
    : _folder(std::move(folder))
{
}

std::optional<std::string> SecretServiceTokenStore::Load()
{
	GError* error = nullptr;
	gchar* found = secret_password_lookup_sync(Schema(), nullptr, &error, "folder", _folder.c_str(), nullptr);
	if (error != nullptr)
	{
		g_error_free(error);
	}
	if (found == nullptr)
	{
		return std::nullopt;
	}
	std::string token(found);
	secret_password_free(found);
	return token;
}

bool SecretServiceTokenStore::Save(std::string_view token)
{
	GError* error = nullptr;
	const std::string text(token);
	const auto stored = secret_password_store_sync(Schema(), SECRET_COLLECTION_DEFAULT, "openblack lobby session", text.c_str(),
	                                               nullptr, &error, "folder", _folder.c_str(), nullptr);
	if (error != nullptr)
	{
		g_error_free(error);
	}
	return stored != FALSE;
}

void SecretServiceTokenStore::Clear()
{
	GError* error = nullptr;
	secret_password_clear_sync(Schema(), nullptr, &error, "folder", _folder.c_str(), nullptr);
	if (error != nullptr)
	{
		g_error_free(error);
	}
}
#endif

} // namespace openblack::online
