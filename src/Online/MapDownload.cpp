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

#include "OnlineMapLand.h"
#include "OnlineMapStoreInterface.h"

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

MapDownload::MapDownload(OnlineMapStoreInterface& store, Fetch fetch)
    : _store(store)
    , _fetch(std::move(fetch))
{
}

MapDownload::~MapDownload() = default;

std::string MapDownload::FileOf(const lobby_api::OnlineMap& map)
{
	return map.file + std::string(online_map_land::k_Extension);
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

void MapDownload::Start(const lobby_api::OnlineMap& map)
{
	++*_generation;
	_map = map;
	_state = State::Looking;
	const Guard guard(_generation);
	_store.Find(FileOf(map), map.checksum, [this, guard](bool found) {
		if (!guard.Live())
		{
			return;
		}
		if (found)
		{
			_state = State::Ready;
			return;
		}
		Download(k_Tries);
	});
}

void MapDownload::Stop()
{
	++*_generation;
	_map.reset();
	_state = State::Idle;
}

void MapDownload::Download(int tries)
{
	_state = State::Downloading;
	const Guard guard(_generation);
	_fetch(*_map, [this, guard, tries](std::optional<std::vector<uint8_t>> bytes) {
		if (!guard.Live())
		{
			return;
		}
		if (!bytes.has_value())
		{
			if (tries > 1)
			{
				Download(tries - 1);
			}
			else
			{
				_state = State::Failed;
			}
			return;
		}
		_state = State::Keeping;
		_store.Write(FileOf(*_map), std::move(*bytes), [this, guard](bool written) {
			if (guard.Live())
			{
				_state = written ? State::Ready : State::Failed;
			}
		});
	});
}

} // namespace openblack::online
