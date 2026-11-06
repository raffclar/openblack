/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IndexedDbMapStore.h"

#include <optional>

#include <emscripten/em_js.h>

#include "Online/HttpClientInterface.h"
#include "Online/StartRules.h"

// The game runs in a worker thread, and the IndexedDB storage of the fetch of Emscripten opens only in the main thread
// of the page. The store thus opens its own database in the worker of the game. Each request keeps its state in a list
// until the game takes the result. The states: 0 the request waits, 1 the request is done, 2 the request failed.
// clang-format off
EM_JS(int, openblack_maps_start, (int write, const char* key, const uint8_t* data, double length), {
	const maps = globalThis.openblackMaps || (globalThis.openblackMaps = {next: 1, open: {}, db: null});
	const id = maps.next++;
	const entry = {state: 0, bytes: null};
	maps.open[id] = entry;
	if (typeof indexedDB === 'undefined') {
		entry.state = 2;
		return id;
	}
	const name = UTF8ToString(key);
	// A copy, because the storage cannot keep a view of the shared memory of the threads
	const bytes = write ? HEAPU8.slice(data, data + length) : null;
	if (!maps.db) {
		maps.db = new Promise((resolve, reject) => {
			let request;
			try {
				request = indexedDB.open('openblack-online-maps', 1);
			} catch (error) {
				reject(error);
				return;
			}
			request.onupgradeneeded = () => request.result.createObjectStore('maps');
			request.onsuccess = () => resolve(request.result);
			request.onerror = () => reject(request.error);
		});
	}
	maps.db.then((db) => {
		const transaction = db.transaction('maps', write ? 'readwrite' : 'readonly');
		const store = transaction.objectStore('maps');
		const request = write ? store.put(bytes, name) : store.get(name);
		// A full storage (a quota error) stops the transaction
		transaction.onabort = () => { entry.state = 2; };
		transaction.onerror = () => { entry.state = 2; };
		transaction.oncomplete = () => {
			if (write) {
				entry.state = 1;
			} else if (request.result instanceof Uint8Array) {
				entry.bytes = request.result;
				entry.state = 1;
			} else {
				entry.state = 2;
			}
		};
	}).catch(() => { entry.state = 2; });
	return id;
});

EM_JS(int, openblack_maps_state, (int id, double* length), {
	const entry = globalThis.openblackMaps && globalThis.openblackMaps.open[id];
	if (!entry) {
		return 2;
	}
	HEAPF64[length >> 3] = entry.bytes ? entry.bytes.length : 0;
	return entry.state;
});

EM_JS(void, openblack_maps_take, (int id, uint8_t* buffer), {
	const entry = globalThis.openblackMaps && globalThis.openblackMaps.open[id];
	if (!entry) {
		return;
	}
	delete globalThis.openblackMaps.open[id];
	if (entry.bytes && buffer) {
		HEAPU8.set(entry.bytes, buffer);
	}
});
EM_JS_DEPS(openblack_online_maps, "$UTF8ToString");
// clang-format on

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
	uint64_t checksum {0};
	std::vector<uint8_t> bytes;
	FoundCallback found;
	WrittenCallback written;
	/// The request of the storage
	int request {0};
	/// The result, when the storage answered
	std::optional<bool> result;
};

IndexedDbMapStore::IndexedDbMapStore() = default;

IndexedDbMapStore::~IndexedDbMapStore()
{
	// The storage finishes a request that waits by itself. The game only forgets the result.
	for (const auto& work : _work)
	{
		openblack_maps_take(work->request, nullptr);
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
	work->file = std::move(file);
	work->checksum = checksum;
	work->found = std::move(done);
	work->request = openblack_maps_start(0, work->file.c_str(), nullptr, 0.0);
	_work.push_back(std::move(work));
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
	auto work = std::make_unique<Work>();
	work->kind = Work::Kind::Write;
	work->file = std::move(file);
	work->written = std::move(done);
	work->request = openblack_maps_start(1, work->file.c_str(), bytes.data(), static_cast<double>(bytes.size()));
	// The start of a game can read the map at once, also before the storage keeps it
	_maps[work->file] = std::move(bytes);
	_work.push_back(std::move(work));
}

void IndexedDbMapStore::Poll()
{
	std::vector<std::unique_ptr<Work>> done;
	std::erase_if(_work, [&done](auto& work) {
		double length = 0.0;
		const int state = openblack_maps_state(work->request, &length);
		if (state == 0)
		{
			return false;
		}
		// The store keeps no map larger than a download can be
		work->result = state == 1 && length <= static_cast<double>(online::k_MostLargeAnswerBytes);
		if (work->kind == Work::Kind::Find && *work->result)
		{
			work->bytes.resize(static_cast<size_t>(length));
		}
		openblack_maps_take(work->request, work->bytes.data());
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
