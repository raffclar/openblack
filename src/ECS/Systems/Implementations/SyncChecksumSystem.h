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
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/meta/fwd.hpp>

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
	void SetTurnListener(std::function<void(const TurnChecksum&)> listener) override { _listener = std::move(listener); }
	[[nodiscard]] std::function<void(const TurnChecksum&)> GetTurnListener() const override { return _listener; }
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	bool StartRowDump(const std::filesystem::path& path, row_dump::Settings settings) override;
	void DumpPart(uint32_t turn, std::string_view reason, uint32_t source) override;
#endif

private:
	/// The names of the saved components, by which the checksum reads the world
	std::unique_ptr<entt::meta_ctx> _reflection;
	/// The threads that share the hash of the components. The first checksum makes them.
	std::unique_ptr<WorkerPool> _pool;
	bool _enabled {false};
	bool _keepParts {false};
	bool _byField {false};
	std::optional<uint32_t> _dumpTurn;
	std::vector<std::string> _dumpComponents;
	std::optional<TurnChecksum> _last;
	/// The unregistered components that the log already told of
	synced_state::UnregisteredReporter _reported;
	std::function<void(const TurnChecksum&)> _listener;
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	/// Tools: the file of the row dump, and the dump that writes to it
	std::unique_ptr<std::ofstream> _rowFile;
	std::unique_ptr<row_dump::RowDump> _rowDump;
#endif
};

} // namespace openblack::ecs::systems
