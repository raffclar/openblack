/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#ifndef LOCATOR_IMPLEMENTATIONS
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include <cstdint>

#include <entt/entity/entity.hpp>

#include "ECS/Systems/AbodeKnockSystemInterface.h"

namespace openblack::ecs::systems
{

class AbodeKnockSystem final: public AbodeKnockSystemInterface
{
public:
	bool Tap(entt::entity abode, glm::vec3 handPoint, bool ownHand) override;
	void Update(std::chrono::milliseconds frame) override;
	[[nodiscard]] std::optional<entt::entity> GetReadoutTown() const override;
	[[nodiscard]] float GetReadoutScale() const override { return _readoutScale; }
	bool TakeHandKnock() override;

private:
	/// The town last knocked on
	entt::entity _knockedTown {entt::null};
	/// Milliseconds left of the read-out, 0 when it isn't showing
	uint32_t _readoutMs {0};
	float _readoutScale {0.0f};
	/// The knocking sound the next knock plays, taken in turn by every knock
	uint32_t _knockSound {0};
	bool _handKnock {false};
};

} // namespace openblack::ecs::systems
