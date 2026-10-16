/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <random>

#include <entt/entity/entity.hpp>

#include "Creature/TattooEditor.h"
#include "ECS/Systems/TattooMenuSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class TattooMenuSystem final: public TattooMenuSystemInterface
{
public:
	[[nodiscard]] bool IsButtonShown(bool restartPending) const override;
	void Open() override;
	void Close() override;
	[[nodiscard]] bool IsOpen() const override { return _copy.has_value(); }
	void Update(float milliseconds, glm::ivec2 pointer, bool buttonHeld, bool leftHeld, const Viewport& viewport) override;
	[[nodiscard]] std::optional<Shown> GetShown() const override;

private:
	/// The camera's view and projection over the viewport
	[[nodiscard]] glm::mat4 View() const;
	[[nodiscard]] glm::mat4 Projection() const;
	void HoverSites(glm::ivec2 pointer);

	/// The body the copy is of, and the copy that is posed and drawn
	std::optional<entt::entity> _body;
	std::optional<entt::entity> _copy;
	/// The editor's view of its own, kept from one opening to the next
	creature_tattoo_editor::Orbit _view {.eye = creature_tattoo_editor::k_MenuEye,
	                                     .focus = creature_tattoo_editor::k_MenuFocus,
	                                     .turnSpeed = 0.0f,
	                                     .tipSpeed = 0.0f};
	Viewport _viewport;
	std::mt19937 _random {std::random_device {}()};
};

} // namespace openblack::ecs::systems
