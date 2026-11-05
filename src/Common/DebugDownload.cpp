/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DebugDownload.h"

#if defined(__EMSCRIPTEN__) && defined(OPENBLACK_DEVELOPER_TOOLS)
#include <system_error>

#include "Web/WebPlatform.h"
#endif

void openblack::debug_download::Offer(const std::filesystem::path& file)
{
#if defined(__EMSCRIPTEN__) && defined(OPENBLACK_DEVELOPER_TOOLS)
	std::error_code error;
	const auto full = std::filesystem::absolute(file, error);
	if (!error && IsOutsideFolder(full, std::filesystem::path(web::k_WorkingFolder) / "Profiles"))
	{
		web::OfferDownload(full);
	}
#else
	static_cast<void>(file);
#endif
}
