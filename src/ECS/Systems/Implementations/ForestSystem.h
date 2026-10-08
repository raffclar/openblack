/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/ForestSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The forests kept for the whole game; Game::LoadMap empties them for every land (ecs::ClearForests)
class ForestSystem final: public ForestSystemInterface
{
public:
	[[nodiscard]] uint32_t Create(uint32_t id, glm::vec3 centre) override;
	[[nodiscard]] Forests& All() override;
	[[nodiscard]] TownForestLists& TownLists() override;

	[[nodiscard]] uint32_t LastTreeCreatedTurn() const override;
	void SetLastTreeCreatedTurn(uint32_t turn) override;

	void NoteForestLostAMagicTree(uint32_t forestId) override;
	[[nodiscard]] bool TakeForestLostAMagicTree(uint32_t forestId) override;
	void ClearForestsThatLostAMagicTree() override;

	void Clear() override;

private:
	TownForestLists _townLists;
	/// How many forests were made in this game: their creation order
	uint32_t _created {0};
	Forests _forests;
	/// The next free forest id
	uint32_t _nextId {1};
	/// The turn the last tree of the world was planted by a forest (the planting chance and the water miracle's
	/// 40-turn cooldown both use it)
	uint32_t _lastTreeCreatedTurn {0};
	/// The forests that lost a magic tree since the forest spell last looked, one entry per tree
	std::vector<uint32_t> _forestsThatLostAMagicTree;
};
} // namespace openblack::ecs::systems
