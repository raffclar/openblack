/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameBuildId.h"

#include "Commands/TurnCodec.h"
#include "Common/BuildVersion.h"

namespace openblack::online::build_id
{

std::string Compose(std::string_view version, uint8_t codec)
{
	std::string id(version);
	id += k_CodecWord;
	id += std::to_string(codec);
	return id;
}

std::string Own()
{
	return Compose(build_version::Text(), turn_codec::k_Version);
}

} // namespace openblack::online::build_id
