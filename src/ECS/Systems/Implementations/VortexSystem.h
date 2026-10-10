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

#include "ECS/Systems/VortexSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct Vortex;
}

namespace openblack::ecs::systems
{

class VortexSystem final: public VortexSystemInterface
{
public:
	entt::entity Create(glm::vec3 position, VortexType type, float altitude) override;
	bool StartFadeOut(entt::entity vortex) override;
	[[nodiscard]] float GetOpenness(entt::entity vortex) const override;
	void ProcessTurn() override;
	void UpdateFrame(float gameSeconds) override;
	[[nodiscard]] std::span<const GroundMark> GetGroundMarks() const override { return _groundMarks; }

private:
	/// A vortex's effects start where it is made
	static void StartEffects(components::Vortex& vortex);
	/// They go at once with it
	static void DeleteEffects(const components::Vortex& vortex);
	/// The land under a vortex is levelled as far as an amount: its heights are read the first time
	static void LevelGround(components::Vortex& vortex, float amount);

	std::vector<GroundMark> _groundMarks;
};

} // namespace openblack::ecs::systems
