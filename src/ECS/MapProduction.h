/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include <cstdint>

#include <functional>
#include <optional>
#include <vector>

#include <entt/entity/registry.hpp>
#include <entt/signal/sigh.hpp>

#include "Map.h"
#include "MapCells.h"

namespace openblack::ecs
{
class Registry;

/// The map's cells, filled as things are made and emptied as they go, through the registry's signals: nothing is
/// rebuilt from the whole registry. Things made since the last look are filed, in the order they were made, before any
/// search of a cell or change of the cells. A thing that moves in the game's turn goes into its new cell as it moves.
class MapProduction final: public MapInterface
{
public:
	/// The outline on the ground of a thing that covers its outline, none for a thing without a model
	using OutlineSource = std::function<std::optional<map_cells::Outline>(const Registry& registry, entt::entity entity)>;

	MapProduction();
	/// The map with another source of outlines, for tests that have no models
	explicit MapProduction(OutlineSource outlines);
	~MapProduction() override;
	MapProduction(const MapProduction&) = delete;
	MapProduction& operator=(const MapProduction&) = delete;
	MapProduction(MapProduction&&) = delete;
	MapProduction& operator=(MapProduction&&) = delete;

	[[nodiscard]] std::span<const entt::entity> GetFixedInGridCell(const CellId& cellId) const override;
	[[nodiscard]] std::span<const entt::entity> GetFixedInGridCell(const glm::vec3& pos) const override;
	[[nodiscard]] std::span<const entt::entity> GetMobileInGridCell(const CellId& cellId) const override;
	[[nodiscard]] std::span<const entt::entity> GetMobileInGridCell(const glm::vec3& pos) const override;
	[[nodiscard]] std::span<const entt::entity> GetFiledFixedInGridCell(const CellId& cellId) const override;
	[[nodiscard]] std::span<const entt::entity> GetFiledMobileInGridCell(const CellId& cellId) const override;
	[[nodiscard]] std::vector<entt::entity> GetAllInCell(glm::ivec2 cell) const override;
	[[nodiscard]] std::span<const entt::entity> GetOnBlock(glm::ivec2 block) const override;
	[[nodiscard]] std::span<const entt::entity> GetOffBlocks() const override;

	void Sync() override;
	void Refile(entt::entity entity) override;
	void Moved(entt::entity entity) override;
	void FileNew() override;
	void FileResidents() override;
	[[nodiscard]] bool InMap(entt::entity entity) const override;
	/// Keeps the order of the things in each cell and each block, and the things made since the last sync
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;

	/// How a thing goes into the cells, by what it is
	struct Kind
	{
		map_cells::Placement placement;
		bool coversOutline;
		bool moves;
	};
	/// How a thing goes into the cells; none for what is not on the map
	[[nodiscard]] static std::optional<Kind> KindOf(const Registry& registry, entt::entity entity);

private:
	void OnMade(entt::registry& registry, entt::entity entity);
	void OnGone(entt::registry& registry, entt::entity entity);
	void OnResidentGone(entt::registry& registry, entt::entity entity);
	/// Just before an entity goes from the registry
	void OnDestroying(entt::entity entity);

	/// Files the things made since the last look, in the order they were made
	void FileMade() const;
	/// Gives the lists the land's blocks, before anything goes on them
	void KnowLandBlocks() const;
	void File(entt::entity entity, const Kind& kind) const;
	void TakeOut(entt::entity entity) const;
	[[nodiscard]] std::vector<uint32_t> CellsFor(entt::entity entity, const Kind& kind) const;
	/// Whether a thing in the cells is now somewhere that puts it in other cells
	[[nodiscard]] bool MovedSinceFiled(entt::entity entity) const;
	/// The index of the cell of a thing's place on the map, none off the map
	[[nodiscard]] std::optional<uint32_t> CellOfPlace(entt::entity entity) const;

	Registry* _registry {nullptr};
	OutlineSource _outlines;
	std::vector<entt::connection> _connections;
	/// The cells are filled lazily by the searches, which are const
	mutable map_cells::CellLists _lists;
	mutable std::vector<entt::entity> _made;
};

} // namespace openblack::ecs
