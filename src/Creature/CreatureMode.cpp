/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMode.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>

#include "3D/MapCoords.h"

namespace openblack::creature_mode
{

float CreatureHeight(float size)
{
	return k_HeightPerSize * std::max(size, 0.05f);
}

KeyAction OnCreatureKey(std::optional<entt::entity> following, std::optional<entt::entity> yours)
{
	if (!yours.has_value())
	{
		return KeyAction::None;
	}
	return following == yours ? KeyAction::Leave : KeyAction::Enter;
}

std::optional<entt::entity> DoubleClicks::OnPress(const Press& press)
{
	const auto last = _last;
	_last = press;
	if (!last.has_value() || !press.creature.has_value() || last->creature != press.creature)
	{
		return std::nullopt;
	}
	const bool quick = press.milliseconds - last->milliseconds <= k_DoubleClickMilliseconds;
	const bool still = std::abs(press.screen.x - last->screen.x) <= k_DoubleClickSlop &&
	                   std::abs(press.screen.y - last->screen.y) <= k_DoubleClickSlop;
	if (!quick || !still)
	{
		return std::nullopt;
	}
	// A third quick press starts a new double click rather than making another
	_last.reset();
	return press.creature;
}

std::optional<creature_physiology::Faint> PassOutFrom(const creature_panel::Values& values)
{
	using creature_physiology::Faint;
	if (creature_panel::Percent(values.damage) >= 100)
	{
		return Faint::OutOfLife;
	}
	if (creature_panel::Percent(values.hunger) >= 100)
	{
		return Faint::Starving;
	}
	if (creature_panel::Percent(values.tiredness) >= 100)
	{
		return Faint::Exhausted;
	}
	return std::nullopt;
}

namespace
{
/// The marked place of the pen in the world, as the game turns, scales and moves it. Each step is a float step, in the
/// game's order of operations.
std::optional<glm::vec3> PenInWorld(glm::vec3 templePosition, float yAngle, float scale,
                                    std::span<const glm::mat4> markedPlaces)
{
	if (markedPlaces.size() <= k_TemplePenPlace)
	{
		return std::nullopt;
	}
	const auto local = glm::vec3(markedPlaces[k_TemplePenPlace][3]);
	// The rows of the turn about the vertical, each scaled by the size of the temple
	const auto cosine = static_cast<float>(std::cos(static_cast<double>(yAngle)));
	const auto sine = static_cast<float>(std::sin(static_cast<double>(yAngle)));
	const std::array<glm::vec3, 3> rows {
	    glm::vec3(scale * cosine, scale * -0.0f, scale * sine),
	    glm::vec3(scale * 0.0f, scale * 1.0f, scale * 0.0f),
	    glm::vec3(scale * -sine, scale * 0.0f, scale * cosine),
	};
	const float x = ((local.y * rows[1].x + local.z * rows[2].x) + local.x * rows[0].x) + templePosition.x;
	const float y = ((local.x * rows[0].y + local.y * rows[1].y) + local.z * rows[2].y) + templePosition.y;
	const float z = ((local.x * rows[0].z + local.y * rows[1].z) + local.z * rows[2].z) + templePosition.z;
	return glm::vec3(x, y, z);
}
} // namespace

std::optional<map_coords::MapCoords> TemplePenOnMap(glm::vec3 templePosition, float yAngle, float scale,
                                                    std::span<const glm::mat4> markedPlaces)
{
	const auto world = PenInWorld(templePosition, yAngle, scale, markedPlaces);
	if (!world.has_value())
	{
		return std::nullopt;
	}
	return map_coords::MapCoords {.x = map_coords::ToFixed(world->x), .z = map_coords::ToFixed(world->z), .altitude = 0.0f};
}

std::optional<glm::vec3> TemplePen(glm::vec3 templePosition, float yAngle, float scale, std::span<const glm::mat4> markedPlaces)
{
	const auto world = PenInWorld(templePosition, yAngle, scale, markedPlaces);
	const auto onMap = TemplePenOnMap(templePosition, yAngle, scale, markedPlaces);
	if (!world.has_value() || !onMap.has_value())
	{
		return std::nullopt;
	}
	return glm::vec3(map_coords::ToMetres(onMap->x), world->y, map_coords::ToMetres(onMap->z));
}

glm::vec3 PenOf(std::optional<glm::vec3> home, std::optional<glm::vec3> temple, glm::vec3 noPen)
{
	return temple.value_or(home.value_or(noPen));
}

} // namespace openblack::creature_mode
