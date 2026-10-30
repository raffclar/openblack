/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <LobbyApi.h>

namespace openblack::online
{
class OnlineMapStoreInterface;

/// The land of the room that a player gets from the lobby (deviation 169), as the original game gets a missing map of
/// its room from the map server of its lobby:
/// 1. When the player comes into the room, the download looks for the room's map in the store, by its file name and
///    its start checksum.
/// 2. A map that the store does not have comes from the lobby. When the lobby does not give it, the download asks one
///    more time, as the original game does.
/// 3. The store keeps the file. The download is then ready.
/// A second failure of the lobby, or a store that cannot keep the file, ends the download with a failure. The room then
/// shows the box "Could not download map from Internet.".
class MapDownload
{
public:
	enum class State : uint8_t
	{
		/// No map: the room plays a land of the game, or the download stopped
		Idle,
		/// The download looks for the map in the store
		Looking,
		/// The map comes from the lobby
		Downloading,
		/// The store keeps the file
		Keeping,
		/// The store has the map
		Ready,
		/// The lobby did not give the map, or the store could not keep it
		Failed,
	};

	/// Gets the bytes of the file of a map from the lobby, or nothing when the lobby did not give them
	using Fetch = std::function<void(const lobby_api::OnlineMap& map,
	                                 std::function<void(std::optional<std::vector<uint8_t>> bytes)> done)>;

	/// The tries of the lobby before the download fails
	static constexpr int k_Tries = 2;

	MapDownload(OnlineMapStoreInterface& store, Fetch fetch);
	~MapDownload();
	MapDownload(const MapDownload&) = delete;
	MapDownload& operator=(const MapDownload&) = delete;
	MapDownload(MapDownload&&) = delete;
	MapDownload& operator=(MapDownload&&) = delete;

	/// Starts the download of the map of a room in place of the download before it
	void Start(const lobby_api::OnlineMap& map);
	/// Stops the download. A result that comes later does nothing.
	void Stop();

	[[nodiscard]] State GetState() const noexcept { return _state; }
	/// The map of the download
	[[nodiscard]] const std::optional<lobby_api::OnlineMap>& GetMap() const noexcept { return _map; }

	/// The name of the file of a map in the store, for example "obm_32p_1.map"
	[[nodiscard]] static std::string FileOf(const lobby_api::OnlineMap& map);
	/// The map of the list that a room plays, by the hash that the room gives. None when the room plays a land of the
	/// game, or the list has no such map.
	[[nodiscard]] static std::optional<lobby_api::OnlineMap> MapOfRoom(std::span<const lobby_api::OnlineMap> maps,
	                                                                   std::string_view roomMap);

private:
	void Download(int tries);

	OnlineMapStoreInterface& _store;
	Fetch _fetch;
	State _state {State::Idle};
	std::optional<lobby_api::OnlineMap> _map;
	/// Each start and stop takes a new number. A callback of an older number does nothing.
	std::shared_ptr<uint32_t> _generation {std::make_shared<uint32_t>(0)};
};

} // namespace openblack::online
