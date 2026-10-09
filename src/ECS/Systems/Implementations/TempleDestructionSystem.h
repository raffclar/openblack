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
#include <utility>
#include <vector>

#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/TempleDestructionWorld.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::components
{
struct Temple;
}

namespace openblack::ecs::systems
{

class TempleDestructionSystem final: public TempleDestructionSystemInterface
{
public:
	/// In the game's own world
	TempleDestructionSystem();
	/// In a world of its own, as tests give it
	explicit TempleDestructionSystem(std::unique_ptr<temple_world::World> world);
	~TempleDestructionSystem() override;

	void Start(entt::entity temple) override;
	void ProcessTurn() override;
	void EndTurn() override;

private:
	/// One turn of a temple being destroyed, in the order the game takes its steps
	void Step(entt::entity entity, components::Temple& temple);
	/// The temple goes, with its way in
	void RemoveTemple(entt::entity temple);

	std::unique_ptr<temple_world::World> _world;
	/// The loops sounding over temples being destroyed, by temple, stopped when their temple goes another way
	std::vector<std::pair<entt::entity, entt::entity>> _loops;
};

} // namespace openblack::ecs::systems
