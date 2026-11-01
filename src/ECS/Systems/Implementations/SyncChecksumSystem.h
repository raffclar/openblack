/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include <fstream>
#endif
#include <cstdint>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/meta/fwd.hpp>

#include "Common/SideThread.h"
#include "Common/WorkerPool.h"
#include "ECS/SyncedState.h"
#include "ECS/Systems/SyncChecksumSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class SyncChecksumSystem final: public SyncChecksumSystemInterface
{
public:
	SyncChecksumSystem();
	~SyncChecksumSystem() override;

	void ProcessTurn() override;
	void SetEnabled(bool enabled) override { _enabled = enabled; }
	[[nodiscard]] bool IsEnabled() const override { return _enabled; }
	void SetDump(uint32_t turn, std::vector<std::string> components) override
	{
		_dumpTurn = turn;
		_dumpComponents = std::move(components);
	}
	void SetKeepParts(bool keep, bool byField) override
	{
		_keepParts = keep;
		_byField = byField;
	}
	[[nodiscard]] std::optional<TurnChecksum> GetLast() const override { return _last; }
	[[nodiscard]] const synced_state::Hash* GetHashOfTurn(uint32_t turn, bool byField) const override
	{
		return _lastHash.has_value() && _lastHash->turn == turn && _lastHash->byField == byField ? &_lastHash->hash : nullptr;
	}
	void SetTurnListener(std::function<void(const TurnChecksum&)> listener) override { _listener = std::move(listener); }
	[[nodiscard]] std::function<void(const TurnChecksum&)> GetTurnListener() const override { return _listener; }
	void SetHashListener(std::function<void(uint32_t turn, const synced_state::Hash& hash)> listener) override
	{
		_hashListener = std::move(listener);
	}
	void SetHashBesideDraw(bool beside) override { _besideDraw = beside; }
	[[nodiscard]] bool HashWaits(uint32_t turn) const override { return _waiting.has_value() && _waiting->turn == turn; }
	void StartHashBesideDraw() override;
	void FinishHash() override;
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	bool StartRowDump(const std::filesystem::path& path, row_dump::Settings settings) override;
	void DumpPart(uint32_t turn, std::string_view reason, uint32_t source) override;
	void WriteWorldStates(world_state_at_turn::Settings settings) override
	{
		_worldStates = std::move(settings);
		_enabled = true;
	}
#endif

private:
	/// What a turn kept for its checksum, apart from the hash of the world: the synced values of the systems
	struct TurnValues
	{
		uint32_t turn {0};
		uint32_t seed {0};
		uint64_t draws {0};
		std::optional<std::array<int64_t, 5>> music;
	};
	[[nodiscard]] WorkerPool& Pool();
	/// Makes the checksum of the turn from the hash of its world, and gives it to the listeners
	void Finish(const TurnValues& values, synced_state::Hash hash, bool byField, std::vector<std::string> dump);

	/// The names of the saved components, by which the checksum reads the world
	std::unique_ptr<entt::meta_ctx> _reflection;
	/// The threads that share the hash of the components. The first checksum makes them.
	std::unique_ptr<WorkerPool> _pool;
	/// The rows of the last checksum, so that the next checksum mixes again only the rows that changed
	ecs::reflection::HashMemo _memo;
	bool _enabled {false};
	bool _keepParts {false};
	bool _byField {false};
	std::optional<uint32_t> _dumpTurn;
	std::vector<std::string> _dumpComponents;
	std::optional<TurnChecksum> _last;
	/// The hash of the synced world of the last checksum, for a tool that records the same world
	struct TurnHash
	{
		uint32_t turn {0};
		bool byField {false};
		synced_state::Hash hash;
	};
	std::optional<TurnHash> _lastHash;
	/// The unregistered components that the log already told of
	synced_state::UnregisteredReporter _reported;
	std::function<void(const TurnChecksum&)> _listener;
	std::function<void(uint32_t, const synced_state::Hash&)> _hashListener;
	bool _besideDraw {false};
	/// The turn whose hash waits for the draw
	std::optional<TurnValues> _waiting;
	/// The hash that the threads make beside the draw, when they started it
	std::optional<synced_state::Hash> _besideHash;
	bool _besideStarted {false};
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	/// Tools: the file of the row dump, or the standard output with a tag on each line, and the dump that writes to it
	std::unique_ptr<row_dump::TaggedLines> _rowLines;
	std::unique_ptr<std::ostream> _rowFile;
	std::unique_ptr<row_dump::RowDump> _rowDump;
	/// Tools: the turns at which the system writes the world state, and the folder
	std::optional<world_state_at_turn::Settings> _worldStates;
#endif
	/// The thread that starts the hash beside the draw and shares it with the pool. It is the last member, so that it
	/// stops before the memo and the pool stop.
	std::unique_ptr<SideThread> _side;
};

} // namespace openblack::ecs::systems
