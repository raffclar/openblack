/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "EditProviders.h"

namespace openblack::inspector
{

/// The inspector's writes made through the game's own paths, reached through the locator: the editor's archetypes and
/// moves, the physics, the sounds and the world's removal of things
class GameWorldEdit final: public WorldEditInterface
{
public:
	[[nodiscard]] std::vector<std::string> Kinds() const override;
	[[nodiscard]] std::vector<std::string> TypeNames(std::string_view kind) const override;
	[[nodiscard]] float GroundHeight(glm::vec2 point) const override;

	std::variant<entt::entity, std::string> Create(std::string_view kind, int32_t type, glm::vec3 position, float yaw) override;
	std::string Move(entt::entity entity, glm::vec3 position) override;
	std::string Turn(entt::entity entity, float yaw) override;
	std::string Remove(entt::entity entity, RemoveHow how) override;
	void Changed(entt::entity entity, std::string_view component) override;

	[[nodiscard]] Presence PresenceOf(entt::entity entity, std::optional<glm::vec3> lastPosition) const override;
};

} // namespace openblack::inspector
