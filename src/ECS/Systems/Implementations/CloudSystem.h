/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include "ECS/Systems/CloudSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CloudSystem final: public CloudSystemInterface
{
public:
	void Reset() override;
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	void UpdateStormClouds(std::chrono::milliseconds frameRealTime) override;
	void AnimateStormClouds(std::chrono::duration<float, std::milli> gameTime) override;
	[[nodiscard]] std::span<const StormCloud> GetStormClouds() const override { return _stormClouds; }

private:
	/// The clouds of all the storms, each storm's in the order that they gathered
	std::vector<StormCloud> _stormClouds;
	/// The real time since the storm clouds last stepped that counts towards the next step
	std::chrono::duration<float, std::milli> _sinceStormStep {0.0f};
};

} // namespace openblack::ecs::systems
