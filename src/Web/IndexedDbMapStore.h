/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(__EMSCRIPTEN__)
#error "The web platform code is only for the web build"
#endif

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Online/OnlineMapStoreInterface.h"

namespace openblack::web
{

/// The store of the lands of network games in the web build: the IndexedDB storage of the site, through the fetch of
/// Emscripten, with the file name as the key. The store keeps a copy in memory of each map that it found or wrote,
/// because the start of a game reads the map at once.
class IndexedDbMapStore final: public online::OnlineMapStoreInterface
{
public:
	IndexedDbMapStore();
	~IndexedDbMapStore() override;
	IndexedDbMapStore(const IndexedDbMapStore&) = delete;
	IndexedDbMapStore& operator=(const IndexedDbMapStore&) = delete;
	IndexedDbMapStore(IndexedDbMapStore&&) = delete;
	IndexedDbMapStore& operator=(IndexedDbMapStore&&) = delete;

	void Find(std::string file, uint64_t checksum, FoundCallback done) override;
	[[nodiscard]] std::optional<std::vector<uint8_t>> Read(std::string_view file) const override;
	void Write(std::string file, std::vector<uint8_t> bytes, WrittenCallback done) override;
	void Poll() override;

	struct Work;

private:
	std::vector<std::unique_ptr<Work>> _work;
	std::map<std::string, std::vector<uint8_t>, std::less<>> _maps;
};

} // namespace openblack::web
