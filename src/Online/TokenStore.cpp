/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TokenStoreInterface.h"

#if !defined(__EMSCRIPTEN__)
#include "Desktop/OsTokenStore.h"
#endif

namespace openblack::online
{

std::unique_ptr<TokenStoreInterface> MakeTokenStore([[maybe_unused]] filesystem::FileSystemInterface& files,
                                                    [[maybe_unused]] const std::optional<std::filesystem::path>& folder)
{
#if defined(__EMSCRIPTEN__)
	// The lobby keeps the token of the browser in an HttpOnly cookie
	return std::make_unique<NoTokenStore>();
#else
	if (!folder.has_value())
	{
		return std::make_unique<NoTokenStore>();
	}
#if defined(_WIN32)
	return std::make_unique<DpapiTokenStore>(files, *folder / k_TokenFileName);
#elif defined(__APPLE__)
	return std::make_unique<KeychainTokenStore>(folder->generic_string());
#elif defined(__linux__) && defined(OPENBLACK_HAVE_LIBSECRET)
	return std::make_unique<SecretServiceTokenStore>(folder->generic_string());
#else
	// No store of the operating system: the player logs in at each run
	return std::make_unique<NoTokenStore>();
#endif
#endif
}

} // namespace openblack::online
