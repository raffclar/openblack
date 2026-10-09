/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <memory>

#include <Inspector.h>
#include <InspectorServer.h>
#include <entt/meta/context.hpp>

#include "ECS/Systems/InspectorSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::inspector
{
class GameProvider;
class RunTargetInterface;
} // namespace openblack::inspector

namespace openblack::ecs::systems
{

/// The inspector's server and its providers: the registry's entities and components, the objects about a point, the
/// moon, the splashes and particle effects, and the game's run control
class InspectorSystem final: public InspectorSystemInterface
{
public:
	explicit InspectorSystem(std::unique_ptr<inspector::Server> server);
	~InspectorSystem() override;
	InspectorSystem(const InspectorSystem&) = delete;
	InspectorSystem& operator=(const InspectorSystem&) = delete;

	void Service() override;
	[[nodiscard]] uint16_t GetPort() const override;

private:
	std::unique_ptr<inspector::Server> _server;
	/// The components' reflection, the inspector's own rather than the library's shared one
	std::unique_ptr<entt::meta_ctx> _reflection;
	std::unique_ptr<inspector::RunTargetInterface> _runTarget;
	inspector::Inspector _inspector;
	/// Owned by the inspector, told of each frame
	inspector::GameProvider* _game {nullptr};
};

} // namespace openblack::ecs::systems
