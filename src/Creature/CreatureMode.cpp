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
/// A metre value kept as a map position and read back: the product of the two floats is exact, then truncated
float AsMapPosition(float metres)
{
	const auto fixed = static_cast<double>(metres) * static_cast<double>(map_coords::k_FixedPerMetre);
	if (!(fixed > -2147483648.0 && fixed < 2147483648.0))
	{
		return map_coords::ToMetres(static_cast<int32_t>(0x80000000u));
	}
	return map_coords::ToMetres(static_cast<int32_t>(fixed));
}
} // namespace

std::optional<glm::vec3> TemplePen(glm::vec3 templePosition, const glm::mat3& templeRotation, glm::vec3 templeScale,
                                   std::span<const glm::mat4> markedPlaces)
{
	if (markedPlaces.size() <= k_TemplePenPlace)
	{
		return std::nullopt;
	}
	const auto local = glm::vec3(markedPlaces[k_TemplePenPlace][3]);
	const auto world = templePosition + (templeRotation * (local * templeScale));
	return glm::vec3(AsMapPosition(world.x), world.y, AsMapPosition(world.z));
}

glm::vec3 PenOf(std::optional<glm::vec3> home, std::optional<glm::vec3> temple, glm::vec3 noPen)
{
	return temple.value_or(home.value_or(noPen));
}

} // namespace openblack::creature_mode
