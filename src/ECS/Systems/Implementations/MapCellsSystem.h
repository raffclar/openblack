/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "ECS/Systems/MapCellsSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The map cells kept for the whole game; magic::OnLoadMap empties them for every land (map_cells::Clear)
class MapCellsSystem final: public MapCellsSystemInterface
{
public:
	[[nodiscard]] map_cells::lists::Cell* CellAt(glm::ivec2 cell) override;
	[[nodiscard]] const map_cells::lists::Cell* CellIfAny(glm::ivec2 cell) const override;
	[[nodiscard]] std::span<const map_cells::lists::Cell> Cells() const override;

	[[nodiscard]] map_cells::lists::Link* LinkOf(entt::entity object) override;
	map_cells::lists::Link& AddLink(entt::entity object, map_cells::lists::Link link) override;
	void RemoveLink(entt::entity object) override;
	[[nodiscard]] const std::unordered_map<entt::entity, map_cells::lists::Link>& Links() const override;

	void Clear() override;
	void UseRegistry(const Registry* registry) override;

	void BeginReadBatch() override;
	void EndReadBatch() override;
	[[nodiscard]] const map_cells::lists::ReadFilter* BatchFilter() const override;

private:
	/// One entity index's place in the flat index: the linked entity that holds it and its link (a node of _links,
	/// whose address stays the same until it is erased), and whether two linked entities have ever had this index at
	/// once (a recycled index linked before the old entity left), after which a miss asks _links
	struct IndexSlot
	{
		entt::entity entity {entt::null};
		map_cells::lists::Link* link {nullptr};
		bool shared {false};
	};

	std::vector<map_cells::lists::Cell> _cells;
	/// The owner of the links; its order is the order Links() walks
	std::unordered_map<entt::entity, map_cells::lists::Link> _links;
	/// The links by entity index, so that a lookup is an array read; it only mirrors _links
	std::vector<IndexSlot> _index;
	/// The registry the links belong to
	const Registry* _registry {nullptr};
	/// The snapshot of the live read batch, if any, and how deep they nest
	std::optional<map_cells::lists::ReadFilter> _batchFilter;
	int _batchDepth {0};
};
} // namespace openblack::ecs::systems
