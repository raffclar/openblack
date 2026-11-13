/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapDownload.h"

#include <algorithm>
#include <numeric>

#include "OnlineMapLand.h"
#include "OnlineMapStoreInterface.h"
#include "Sha256.h"
#include "StartRules.h"

namespace openblack::online
{

namespace
{
/// A guard of a callback: it is live while the download that made it is the same download
class Guard
{
public:
	explicit Guard(const std::shared_ptr<uint32_t>& generation)
	    : _generation(generation)
	    , _value(*generation)
	{
	}

	[[nodiscard]] bool Live() const
	{
		const auto generation = _generation.lock();
		return generation != nullptr && *generation == _value;
	}

private:
	std::weak_ptr<uint32_t> _generation;
	uint32_t _value;
};
} // namespace

MapDownload::MapDownload(OnlineMapStoreInterface& store, Fetch fetch, Assemble assemble, uint64_t pieceBytes)
    : _store(store)
    , _fetch(std::move(fetch))
    , _assemble(std::move(assemble))
    , _pieceBytes(std::max<uint64_t>(pieceBytes, 1))
{
}

MapDownload::~MapDownload() = default;

std::string MapDownload::FileOf(const lobby_api::OnlineMap& map)
{
	return map.file + std::string(online_map_land::k_Extension);
}

uint64_t MapDownload::TransferSizeOf(const lobby_api::OnlineMap& map)
{
	return static_cast<uint64_t>(std::max<int64_t>(map.packageSha256.empty() ? map.size : map.packageSize, 0));
}

std::optional<lobby_api::OnlineMap> MapDownload::MapOfRoom(std::span<const lobby_api::OnlineMap> maps, std::string_view roomMap)
{
	if (roomMap.empty())
	{
		return std::nullopt;
	}
	const auto found = std::ranges::find(maps, roomMap, &lobby_api::OnlineMap::sha256);
	if (found == maps.end())
	{
		return std::nullopt;
	}
	return *found;
}

uint64_t MapDownload::KeptBytes(std::string_view sha256) const
{
	const auto found = std::ranges::find(_parts, sha256, &Part::sha256);
	return found != _parts.end() ? found->bytes.size() : 0;
}

MapDownload::Part* MapDownload::PartOf(std::string_view sha256)
{
	const auto found = std::ranges::find(_parts, sha256, &Part::sha256);
	return found != _parts.end() ? &*found : nullptr;
}

MapDownload::Part& MapDownload::OwnPart()
{
	if (auto* part = PartOf(_map->sha256); part != nullptr)
	{
		return *part;
	}
	return _parts.emplace_back(Part {.sha256 = _map->sha256, .bytes = {}, .total = TransferSizeOf(*_map), .fetching = false});
}

void MapDownload::Forget(std::string_view sha256)
{
	std::erase_if(_parts, [sha256](const Part& part) { return part.sha256 == sha256; });
}

void MapDownload::Trim()
{
	const auto kept = [this] {
		return std::accumulate(_parts.begin(), _parts.end(), uint64_t {0},
		                       [](uint64_t sum, const Part& part) { return sum + part.bytes.size(); });
	};
	while (kept() > k_MostKeptBytes)
	{
		// The part of the map of the download stays
		const auto oldest =
		    std::ranges::find_if(_parts, [this](const Part& part) { return !_map.has_value() || part.sha256 != _map->sha256; });
		if (oldest == _parts.end())
		{
			return;
		}
		_parts.erase(oldest);
	}
}

void MapDownload::Start(const lobby_api::OnlineMap& map)
{
	// The original game does not start again a download of the same map that runs now
	if (_map.has_value() && _map->sha256 == map.sha256 &&
	    (_state == State::Looking || _state == State::Downloading || _state == State::Keeping))
	{
		return;
	}
	++*_generation;
	_map = map;
	_state = State::Looking;
	_triesLeft = k_Tries;
	_checksLeft = k_Tries;
	const Guard guard(_generation);
	_store.Find(FileOf(map), map.checksum, [this, guard](bool found) {
		if (!guard.Live())
		{
			return;
		}
		if (found)
		{
			// The store has the full file, so the bytes that came are not necessary
			Forget(_map->sha256);
			_state = State::Ready;
			return;
		}
		_state = State::Downloading;
		Continue();
	});
}

void MapDownload::Stop()
{
	++*_generation;
	_map.reset();
	_state = State::Idle;
}

void MapDownload::Continue()
{
	auto& part = OwnPart();
	// The answer of the request that runs continues the download
	if (part.fetching)
	{
		return;
	}
	if (part.total != 0 && part.bytes.size() >= part.total)
	{
		Finish();
		return;
	}
	const uint64_t offset = part.bytes.size();
	const uint64_t count = part.total != 0 ? std::min(_pieceBytes, part.total - offset) : _pieceBytes;
	part.fetching = true;
	// The fetch can answer at once and change the parts, so the callback holds a copy of the hash
	std::weak_ptr<bool> alive = _alive;
	_fetch(*_map, offset, count,
	       [this, alive = std::move(alive), sha256 = part.sha256, offset](std::optional<lobby_api::FilePiece> piece) {
		       if (!alive.expired())
		       {
			       Received(sha256, offset, std::move(piece));
		       }
	       });
}

void MapDownload::Received(const std::string& sha256, uint64_t offset, std::optional<lobby_api::FilePiece> piece)
{
	auto* part = PartOf(sha256);
	if (part == nullptr)
	{
		return;
	}
	part->fetching = false;
	// A piece must start where the request asked, or be the full file. It must agree with the size that the part has.
	const bool good = piece.has_value() && !piece->bytes.empty() &&
	                  (piece->offset == offset || (piece->offset == 0 && piece->bytes.size() == piece->total)) &&
	                  (part->total == 0 || piece->total == part->total) && piece->offset + piece->bytes.size() <= piece->total;
	if (good)
	{
		if (piece->offset == 0)
		{
			part->bytes = std::move(piece->bytes);
		}
		else
		{
			part->bytes.insert(part->bytes.end(), piece->bytes.begin(), piece->bytes.end());
		}
		part->total = piece->total;
		Trim();
	}
	// A paused map keeps the piece and asks for nothing more
	if (!_map.has_value() || _map->sha256 != sha256 || _state != State::Downloading)
	{
		return;
	}
	if (!good)
	{
		TryFailed();
		return;
	}
	_triesLeft = k_Tries;
	Continue();
}

void MapDownload::TryFailed()
{
	if (--_triesLeft > 0)
	{
		Continue();
		return;
	}
	_state = State::Failed;
}

void MapDownload::Finish()
{
	auto bytes = std::move(OwnPart().bytes);
	Forget(_map->sha256);
	auto file = _assemble ? _assemble(*_map, std::move(bytes)) : std::optional(std::move(bytes));
	// A file that is not the room's map: the start check would refuse it. The SHA-256 of the map list finds also a changed
	// file that has the start checksum of the map. The bytes go, and the download starts again from the first byte.
	if (!file.has_value() || start_rules::LandChecksum(*file) != _map->checksum || sha256::HexOf(*file) != _map->sha256)
	{
		if (--_checksLeft > 0)
		{
			_triesLeft = k_Tries;
			Continue();
			return;
		}
		_state = State::Failed;
		return;
	}
	_state = State::Keeping;
	const Guard guard(_generation);
	_store.Write(FileOf(*_map), std::move(*file), [this, guard](bool written) {
		if (guard.Live())
		{
			_state = written ? State::Ready : State::Failed;
		}
	});
}

} // namespace openblack::online
