/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Resources/Rereads.h"

#include <utility>

#include <PackFile.h>
#include <spdlog/spdlog.h>

namespace openblack::resources
{

Rereads::Rereads(Reader reader)
    : _reader(std::move(reader))
{
}

Rereads::~Rereads() = default;

std::vector<uint8_t> Rereads::File(const std::filesystem::path& path) const
{
	++_reads;
	return _reader(path);
}

const pack::PackFile* Rereads::Pack(const std::filesystem::path& path)
{
	if (const auto it = _packs.find(path); it != _packs.end())
	{
		return it->second.get();
	}
	// A pack that cannot be read is kept as null, thus the restore reads it only one time
	auto& kept = _packs[path];
	const auto bytes = File(path);
	if (bytes.empty())
	{
		return nullptr;
	}
	auto pack = std::make_unique<pack::PackFile>();
	if (const auto result = pack->Open(bytes); result != pack::PackResult::Success)
	{
		if (const auto logger = spdlog::get("graphics"))
		{
			logger->error("Unable to read {} again after a restore of the graphics device: {}", path.generic_string(),
			              pack::ResultToStr(result));
		}
		return nullptr;
	}
	kept = std::move(pack);
	return kept.get();
}

} // namespace openblack::resources
