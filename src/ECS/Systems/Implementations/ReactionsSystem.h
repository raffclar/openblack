/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include "ECS/Systems/ReactionsSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The reactions kept for the whole game; magic::OnLoadMap empties them for every land (reactions::Clear)
class ReactionsSystem final: public ReactionsSystemInterface
{
public:
	[[nodiscard]] uint32_t Add(effects::reactions::Reaction reaction) override;
	[[nodiscard]] effects::reactions::Reaction* Find(uint32_t id) override;
	[[nodiscard]] std::vector<effects::reactions::Reaction>& List() override;
	[[nodiscard]] const std::vector<effects::reactions::Reaction>& List() const override;
	void Erase(std::span<const uint32_t> ids) override;

	void SetInTurn(bool inTurn) override;
	[[nodiscard]] bool InTurn() const override;

	[[nodiscard]] uint32_t NextJoinOrder() override;
	void ResetJoinOrder() override;

	void SetReactionHandler(effects::reactions::LivingClass living, effects::reactions::LivingReactionHandler handler) override;
	[[nodiscard]] effects::reactions::LivingReactionHandler
	ReactionHandler(effects::reactions::LivingClass living) const override;
	void SetShutDownHandler(effects::reactions::LivingClass living, effects::reactions::LivingShutDownHandler handler) override;
	[[nodiscard]] std::span<const effects::reactions::LivingShutDownHandler> ShutDownHandlers() const override;

	void Clear() override;

private:
	static constexpr std::size_t k_ClassCount = 3;

	std::vector<effects::reactions::Reaction> _reactions;
	uint32_t _nextId {1};
	/// Between BeginTurn and EndTurn: the map cells were rebuilt at the start of the turn
	bool _inTurn {false};
	/// Goes up for every villager that joins a reaction; only ResetJoinOrder puts it back to 0
	uint32_t _joinOrder {0};
	std::array<effects::reactions::LivingReactionHandler, k_ClassCount> _handlers {};
	std::array<effects::reactions::LivingShutDownHandler, k_ClassCount> _shutDownHandlers {};
};
} // namespace openblack::ecs::systems
