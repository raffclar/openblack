/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/TipBubbleSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class TipBubbleSystem final: public TipBubbleSystemInterface
{
public:
	void Show(entt::entity sign, uint32_t text) override;
	void Hide() override;
	[[nodiscard]] entt::entity GetSign() const override { return _sign; }
	[[nodiscard]] uint32_t GetText() const override { return _text; }

	void ProcessTurn() override;
	void UpdateFrame(float gameMilliseconds) override;
	void Hover() override;

	[[nodiscard]] bool IsUp() const override;
	[[nodiscard]] float GetDisplayTime() const override { return _displayTime; }
	[[nodiscard]] std::optional<glm::vec3> GetAnchor() const override;
	[[nodiscard]] help::tip_bubble::Scroll& GetScroll() override { return _scroll; }
	[[nodiscard]] const help::tip_bubble::Scroll& GetScroll() const override { return _scroll; }

	void Reset() override;

private:
	entt::entity _sign {entt::null};
	uint32_t _text {0};
	/// Kept from one opening to the next: the first bubble of a game shows only from the next turn
	float _displayTime {0.0f};
	help::tip_bubble::Scroll _scroll;
};

} // namespace openblack::ecs::systems
