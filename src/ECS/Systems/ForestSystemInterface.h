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

#include <map>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{
/// A forest: its centre, the empty timer and the planting attempts. Its trees are the ones whose Tree::forestId is its
/// id (the original keeps two lists sorted by distance to the centre).
struct ForestData
{
	glm::vec3 centre;
	uint16_t emptyTimer {0};
	uint16_t attempts {0};
	uint32_t created {0}; ///< creation order (the original's list is newest first)
	entt::entity bigForest {entt::null};
	bool scenic {false}; ///< The town's scenic forest (MakeScenicForest)
};
} // namespace openblack::ecs

namespace openblack::ecs::systems
{
/// The game's forests: every forest by id, each town's forests, the ids and the turn the last tree was planted
/// (the forest functions of ECS/Trees.h go through it)
class ForestSystemInterface
{
public:
	using Forests = std::map<uint32_t, ForestData>;
	/// Each town's forests, head first
	using TownForestLists = std::unordered_map<uint32_t, std::vector<uint32_t>>;

	virtual ~ForestSystemInterface() = default;

	/// A forest at `centre` with that id (0: the next free id; a given id raises the next one past it); its id
	[[nodiscard]] virtual uint32_t Create(uint32_t id, glm::vec3 centre) = 0;
	/// Every forest, by id
	[[nodiscard]] virtual Forests& All() = 0;
	[[nodiscard]] virtual TownForestLists& TownLists() = 0;

	/// The turn the last tree of the world was planted by a forest
	[[nodiscard]] virtual uint32_t LastTreeCreatedTurn() const = 0;
	virtual void SetLastTreeCreatedTurn(uint32_t turn) = 0;

	/// A magic tree of this forest was deleted (noted once per tree, for the forest spell to look at)
	virtual void NoteForestLostAMagicTree(uint32_t forestId) = 0;
	/// Whether the forest lost a magic tree since it was last asked; its notes go
	[[nodiscard]] virtual bool TakeForestLostAMagicTree(uint32_t forestId) = 0;
	/// No forest has lost a magic tree (every land load, with the spells)
	virtual void ClearForestsThatLostAMagicTree() = 0;

	/// A land is loaded: no forests, no town lists, ids from 1, no tree planted yet. The creation order and the
	/// forests that lost a magic tree stay (ClearForestsThatLostAMagicTree empties those)
	virtual void Clear() = 0;
};
} // namespace openblack::ecs::systems
