/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/PuzzleGameSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class PuzzleGameSystem final: public PuzzleGameSystemInterface
{
public:
	entt::entity Create(script::PuzzleGameType type, glm::vec3 point, float yAngleRadians, float scale) override;
	void ProcessTurn() override;
	[[nodiscard]] std::optional<bool> Played(entt::entity puzzle) override;
	[[nodiscard]] std::optional<script::PuzzleGameStatus> State(entt::entity puzzle) override;
	void TakenInHand(entt::entity piece) override;
	void Forget(entt::entity puzzle) override;
	bool DeletePieces(entt::entity puzzle) override;
};

} // namespace openblack::ecs::systems
