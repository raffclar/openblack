/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include "Common/WorkerPool.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureHairSystem final: public CreatureHairSystemInterface
{
public:
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	void StepCopy(Registry& copies, entt::entity copy, std::chrono::duration<float, std::milli> frameTime,
	              const Registry& bodies) override;
	[[nodiscard]] bool IsShown() const override { return _shown; }
	void SetShown(bool shown) override;

private:
	/// The threads that move the hair of many bodies at the same time, made at the first frame
	[[nodiscard]] WorkerPool& Workers();

	bool _shown {true};
	std::unique_ptr<WorkerPool> _workers;
};

} // namespace openblack::ecs::systems
