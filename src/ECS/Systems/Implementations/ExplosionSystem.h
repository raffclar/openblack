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

#include "Blast/CameraShake.h"
#include "Blast/DustPuff.h"
#include "ECS/Systems/ExplosionSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class ExplosionSystem final: public ExplosionSystemInterface
{
public:
	void AddRubble(const glm::vec3& centre, float yaw, float scale) override;
	void AddSmoke(const glm::vec3& centre, float size, dust_puff::Kind kind, std::optional<uint32_t> colour) override;
	void AddShake(const glm::vec3& position, float radius, float strength, int32_t milliseconds, bool verticalOnly) override;
	[[nodiscard]] bool IsShaking() const override { return !_shakes.empty(); }
	void Update(float milliseconds) override;
	void CollectDrawFrame(particles::draw::Frame& frame) const override;
	void AddGhost(const components::DestructionGhost& ghost) override { _ghosts.push_back(ghost); }
	[[nodiscard]] std::span<const components::DestructionGhost> Ghosts() const override { return _ghosts; }
	void Reset() override;
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;

private:
	std::vector<camera_shake::Shake> _shakes;
	std::vector<dust_puff::Puff> _puffs;
	std::vector<components::DestructionGhost> _ghosts;
};

} // namespace openblack::ecs::systems
