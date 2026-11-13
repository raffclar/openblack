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
/// 2. A map that the store does not have comes from the lobby in pieces. When the lobby does not give a piece, the
///    download asks one more time, as the original game does.
/// 3. When all the bytes came, the download checks the file against the start checksum and the SHA-256 of the list.
///    A bad file removes the bytes, and the download starts again one time.
/// 4. The store keeps the file. The download is then ready.
/// A second failure of the lobby, or a store that cannot keep the file, ends the download with a failure. The room then
/// shows the box "Could not download map from Internet.".
///
/// The download keeps the bytes of each map that did not come fully (deviation 257). A start of another map pauses
/// the download of the map before it. A later start of the paused map continues from the bytes that it has. The bytes
/// are only in memory, so a part of a file never goes in the store and never loads as a map.
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

	/// Gets a piece of the file that the lobby gives for a map: at most count bytes from the offset. Nothing when the
	/// lobby did not give the piece.
	using Fetch = std::function<void(const lobby_api::OnlineMap& map, uint64_t offset, uint64_t count,
	                                 std::function<void(std::optional<lobby_api::FilePiece> piece)> done)>;
	/// Makes the bytes of the map from all the bytes that came, for example from a map package. Nothing when it cannot.
	using Assemble =
	    std::function<std::optional<std::vector<uint8_t>>(const lobby_api::OnlineMap& map, std::vector<uint8_t> bytes)>;

	/// The tries of the lobby before the download fails
	static constexpr int k_Tries = 2;
	/// The most bytes of one piece
	static constexpr uint64_t k_PieceBytes = 256 * 1024;
	/// The most bytes that the download keeps for the maps that did not come fully. The oldest part goes first.
	static constexpr uint64_t k_MostKeptBytes = 64 * 1024 * 1024;

	/// An empty assemble gives the bytes that came as the map
	MapDownload(OnlineMapStoreInterface& store, Fetch fetch, Assemble assemble = {}, uint64_t pieceBytes = k_PieceBytes);
	~MapDownload();
	MapDownload(const MapDownload&) = delete;
	MapDownload& operator=(const MapDownload&) = delete;
	MapDownload(MapDownload&&) = delete;
	MapDownload& operator=(MapDownload&&) = delete;

	/// Starts the download of the map of a room in place of the download before it, which pauses. A start of the map
	/// that the download looks for, gets or keeps now does nothing.
	void Start(const lobby_api::OnlineMap& map);
	/// Stops the download and keeps the bytes that came. A result that comes later does not change the state.
	void Stop();

	[[nodiscard]] State GetState() const noexcept { return _state; }
	/// The map of the download
	[[nodiscard]] const std::optional<lobby_api::OnlineMap>& GetMap() const noexcept { return _map; }
	/// The bytes that the download keeps for a map, by the SHA-256 of the map. 0 when it keeps none.
	[[nodiscard]] uint64_t KeptBytes(std::string_view sha256) const;

	/// The name of the file of a map in the store, for example "obm_32p_1.map"
	[[nodiscard]] static std::string FileOf(const lobby_api::OnlineMap& map);
	/// The bytes of the file that the lobby gives for a map: the map package when the map has one, else the map file. 0
	/// when the list does not give the size.
	[[nodiscard]] static uint64_t TransferSizeOf(const lobby_api::OnlineMap& map);
	/// The map of the list that a room plays, by the hash that the room gives. None when the room plays a land of the
	/// game, or the list has no such map.
	[[nodiscard]] static std::optional<lobby_api::OnlineMap> MapOfRoom(std::span<const lobby_api::OnlineMap> maps,
	                                                                   std::string_view roomMap);

private:
	/// The bytes that came for one map
	struct Part
	{
		std::string sha256;
		std::vector<uint8_t> bytes;
		/// The bytes of the full file, 0 while no answer and no list gave it
		uint64_t total {0};
		/// A request of a piece of this map did not answer yet
		bool fetching {false};
	};

	/// The part of a map, or nothing
	[[nodiscard]] Part* PartOf(std::string_view sha256);
	/// The part of the map of the download. It makes a new part when there is none.
	Part& OwnPart();
	void Forget(std::string_view sha256);
	/// Removes the oldest parts of other maps while the kept bytes are more than the limit
	void Trim();
	/// Asks for the next piece, or checks the file when all the bytes came
	void Continue();
	void Received(const std::string& sha256, uint64_t offset, std::optional<lobby_api::FilePiece> piece);
	/// A try of the lobby failed: asks again, or ends the download with a failure
	void TryFailed();
	void Finish();

	OnlineMapStoreInterface& _store;
	Fetch _fetch;
	Assemble _assemble;
	uint64_t _pieceBytes;
	State _state {State::Idle};
	std::optional<lobby_api::OnlineMap> _map;
	/// The tries of the lobby for the next piece
	int _triesLeft {k_Tries};
	/// The checks of a full file before the download fails
	int _checksLeft {k_Tries};
	/// The parts, the oldest first
	std::vector<Part> _parts;
	/// The callbacks of the lobby hold a weak copy, so that an answer after the end of the download does nothing
	std::shared_ptr<bool> _alive {std::make_shared<bool>(true)};
	/// Each start and stop takes a new number. A callback of the store with an older number does nothing.
	std::shared_ptr<uint32_t> _generation {std::make_shared<uint32_t>(0)};
};

} // namespace openblack::online
