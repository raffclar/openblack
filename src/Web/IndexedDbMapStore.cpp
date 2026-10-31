/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IndexedDbMapStore.h"

#include <cstring>

#include <optional>

#include <emscripten/fetch.h>

#include "Online/StartRules.h"

namespace openblack::web
{

/// A read or a write of the storage that did not run its callback yet
struct IndexedDbMapStore::Work
{
	enum class Kind : uint8_t
	{
		Find,
		Write,
	};
	Kind kind {Kind::Find};
	std::string file;
	std::string key;
	uint64_t checksum {0};
	std::vector<uint8_t> bytes;
	FoundCallback found;
	WrittenCallback written;
	emscripten_fetch_t* fetch {nullptr};
	/// The result, when the storage answered
	std::optional<bool> result;
};

namespace
{

/// The key of a map in the storage of the fetch
std::string KeyOf(std::string_view file)
{
	return "openblack/online-maps/" + std::string(file);
}

void Done(emscripten_fetch_t* fetch)
{
	// The store closed this fetch as it stopped: nobody waits for the result
	if (fetch->userData == nullptr)
	{
		return;
	}
	auto& work = *static_cast<IndexedDbMapStore::Work*>(fetch->userData);
	const bool ok = fetch->status == 200;
	if (work.kind == IndexedDbMapStore::Work::Kind::Find && ok && fetch->data != nullptr)
	{
		const auto* bytes = reinterpret_cast<const uint8_t*>(fetch->data);
		work.bytes.assign(bytes, bytes + fetch->numBytes);
	}
	work.result = ok;
	work.fetch = nullptr;
	emscripten_fetch_close(fetch);
}

} // namespace

IndexedDbMapStore::IndexedDbMapStore() = default;

IndexedDbMapStore::~IndexedDbMapStore()
{
	for (auto& work : _work)
	{
		if (work->fetch != nullptr)
		{
			auto* fetch = work->fetch;
			work->fetch = nullptr;
			fetch->userData = nullptr;
			emscripten_fetch_close(fetch);
		}
	}
}

void IndexedDbMapStore::Find(std::string file, uint64_t checksum, FoundCallback done)
{
	// A map that this run found or wrote is in memory
	if (const auto kept = _maps.find(file); kept != _maps.end())
	{
		done(online::start_rules::LandChecksum(kept->second) == checksum);
		return;
	}
	if (!online::IsStoreFileName(file))
	{
		done(false);
		return;
	}
	auto work = std::make_unique<Work>();
	work->kind = Work::Kind::Find;
	work->key = KeyOf(file);
	work->file = std::move(file);
	work->checksum = checksum;
	work->found = std::move(done);

	emscripten_fetch_attr_t attributes;
	emscripten_fetch_attr_init(&attributes);
	std::strncpy(attributes.requestMethod, "GET", sizeof(attributes.requestMethod) - 1);
	// Only the storage: a map that is not there is not downloaded from the key
	attributes.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_NO_DOWNLOAD;
	attributes.onsuccess = Done;
	attributes.onerror = Done;
	attributes.userData = work.get();
	auto* raw = work.get();
	_work.push_back(std::move(work));
	raw->fetch = emscripten_fetch(&attributes, raw->key.c_str());
	if (raw->fetch == nullptr && !raw->result.has_value())
	{
		raw->result = false;
	}
}

std::optional<std::vector<uint8_t>> IndexedDbMapStore::Read(std::string_view file) const
{
	const auto kept = _maps.find(file);
	if (kept == _maps.end())
	{
		return std::nullopt;
	}
	return kept->second;
}

void IndexedDbMapStore::Write(std::string file, std::vector<uint8_t> bytes, WrittenCallback done)
{
	if (!online::IsStoreFileName(file))
	{
		done(false);
		return;
	}
	// The start of a game can read the map at once, also before the storage keeps it
	_maps[file] = bytes;
	auto work = std::make_unique<Work>();
	work->kind = Work::Kind::Write;
	work->key = KeyOf(file);
	work->file = std::move(file);
	work->bytes = std::move(bytes);
	work->written = std::move(done);

	emscripten_fetch_attr_t attributes;
	emscripten_fetch_attr_init(&attributes);
	std::strncpy(attributes.requestMethod, "EM_IDB_STORE", sizeof(attributes.requestMethod) - 1);
	attributes.attributes = EMSCRIPTEN_FETCH_REPLACE | EMSCRIPTEN_FETCH_PERSIST_FILE;
	attributes.requestData = reinterpret_cast<const char*>(work->bytes.data());
	attributes.requestDataSize = work->bytes.size();
	attributes.onsuccess = Done;
	attributes.onerror = Done;
	attributes.userData = work.get();
	auto* raw = work.get();
	_work.push_back(std::move(work));
	raw->fetch = emscripten_fetch(&attributes, raw->key.c_str());
	if (raw->fetch == nullptr && !raw->result.has_value())
	{
		raw->result = false;
	}
}

void IndexedDbMapStore::Poll()
{
	std::vector<std::unique_ptr<Work>> done;
	std::erase_if(_work, [&done](auto& work) {
		if (!work->result.has_value())
		{
			return false;
		}
		done.push_back(std::move(work));
		return true;
	});
	for (auto& work : done)
	{
		if (work->kind == Work::Kind::Find)
		{
			const bool found = *work->result && online::start_rules::LandChecksum(work->bytes) == work->checksum;
			if (found)
			{
				_maps[work->file] = std::move(work->bytes);
			}
			work->found(found);
		}
		else
		{
			work->written(*work->result);
		}
	}
}

} // namespace openblack::web
