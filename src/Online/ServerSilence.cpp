/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ServerSilence.h"

#include <fmt/format.h>

namespace openblack::online::server_silence
{

std::u16string Text(int64_t microseconds)
{
	const auto tenths = std::max<int64_t>(microseconds, 0) / 100'000;
	const auto text = fmt::format("{}.{}s", tenths / 10, tenths % 10);
	return {text.begin(), text.end()};
}

} // namespace openblack::online::server_silence
