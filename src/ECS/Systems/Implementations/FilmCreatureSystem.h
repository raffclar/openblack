/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <entt/entity/entity.hpp>

#include "3D/FilmCameraPath.h"
#include "ECS/Systems/FilmCreatureSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class FilmCreatureSystem final: public FilmCreatureSystemInterface
{
public:
	void Start() override;
	void Update(int32_t filmMs, std::chrono::steady_clock::time_point now) override;
	void End() override;
	[[nodiscard]] bool IsRunning() const override { return _copy.has_value(); }
	[[nodiscard]] std::optional<Shown> GetShown() const override;

private:
	std::optional<entt::entity> _copy;
	std::shared_ptr<const FilmCameraPath> _path;
	glm::mat4 _view {1.0f};
	std::optional<std::chrono::steady_clock::time_point> _lastFrame;
};

} // namespace openblack::ecs::systems
