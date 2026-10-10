/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TattooEditor.h"

#include <cmath>

#include <algorithm>
#include <ranges>

using namespace openblack;
using namespace openblack::creature_tattoo_editor;

namespace
{
/// The view's speeds are in radians a 0.3 seconds' worth, and die away by e to the power of 8 a 0.3 seconds' worth
constexpr float k_TurnScale = 0.0003f;
constexpr float k_SpeedDecay = -8.0f;
/// How far the pointer from where the rotation button was pressed speeds it up, a millisecond
constexpr float k_SteerScale = 0.001f;
constexpr float k_TurnSteer = 0.2f;
constexpr float k_TipSteer = 0.1f;
/// The lowest the view goes: a quarter as high above the focus as it is far from it across the ground
constexpr float k_LowestTip = 0.25f;
} // namespace

Session creature_tattoo_editor::Open(const creature_tattoo::Slots& slots)
{
	return {.slots = slots, .opened = slots, .changed = false};
}

bool creature_tattoo_editor::Drop(Session& session, uint8_t design, const glm::u8vec3& colour, std::optional<uint8_t> site)
{
	if (!site.has_value() || *site >= creature_tattoo::k_SlotCount)
	{
		return false;
	}
	session.changed = true;
	auto& slots = session.slots;
	const auto found =
	    std::ranges::find_if(slots, [&](const auto& slot) { return slot.site == *site && slot.design == design; });
	if (found != slots.end())
	{
		// The same symbol again on the same place only takes the new colour
		found->colour = colour;
		return false;
	}
	const auto slot = creature_tattoo::SlotFor(slots, *site, design);
	if (!slot.has_value())
	{
		return false;
	}
	slots.at(*slot) = {.design = design, .site = *site, .colour = colour};
	return true;
}

std::optional<creature_tattoo::Slot> creature_tattoo_editor::Lift(Session& session, uint8_t site)
{
	auto& slots = session.slots;
	const auto found =
	    std::ranges::find_if(slots | std::views::reverse, [site](const auto& slot) { return slot.site == site; });
	if (found == (slots | std::views::reverse).end())
	{
		return std::nullopt;
	}
	auto lifted = *found;
	found->site = creature_tattoo::k_NoSite;
	session.changed = true;
	return lifted;
}

Accept creature_tattoo_editor::Ok(const Session& session, bool networkGame)
{
	return networkGame && session.changed ? Accept::Warn : Accept::Close;
}

glm::uvec2 creature_tattoo_editor::PaletteCell(glm::ivec2 point, const PickerRect& rect)
{
	const auto cell = [](int offset, int length, int count) {
		const auto value = (offset * count) / length;
		// The first is taken below one, the last above the one before it
		if (value < 1)
		{
			return 0u;
		}
		return static_cast<uint32_t>(std::min(value, count - 1));
	};
	return {cell(point.x - rect.min.x, rect.max.x - rect.min.x, static_cast<int>(creature_tattoo::k_PaletteColumns)),
	        cell(point.y - rect.min.y, rect.max.y - rect.min.y, static_cast<int>(creature_tattoo::k_PaletteRows))};
}

float creature_tattoo_editor::SliderPosition(int y, const PickerRect& rect)
{
	const auto position = static_cast<float>(y - rect.min.y) / static_cast<float>(rect.max.y - rect.min.y);
	return position > 0.0f ? std::min(position, 1.0f) : 0.0f;
}

std::optional<uint8_t> creature_tattoo_editor::SiteUnderPointer(std::span<const SiteOnScreen> sites, glm::ivec2 pointer)
{
	std::optional<uint8_t> nearest;
	int best = 0;
	for (const auto& site : sites)
	{
		if (site.facingCamera <= k_FacingLimit)
		{
			continue;
		}
		const auto offset = site.screen - pointer;
		const auto distance = (offset.x * offset.x) + (offset.y * offset.y);
		if (!nearest.has_value() || distance < best)
		{
			best = distance;
			nearest = site.site;
		}
	}
	if (nearest.has_value() && best > k_PickDistanceSquared)
	{
		return std::nullopt;
	}
	return nearest;
}

Orbit creature_tattoo_editor::CaveView(const glm::vec3& creature, float scale)
{
	auto reach = scale * 15.0f;
	if (reach <= 1.0f)
	{
		reach = 1.0f;
	}
	else if (reach >= 20.0f)
	{
		reach = 20.0f;
	}
	return {.eye = creature + glm::vec3(reach * -1.5f, reach * 0.75f, 0.0f),
	        .focus = creature + glm::vec3(0.0f, reach * 0.5f, 0.0f)};
}

void creature_tattoo_editor::Steer(Orbit& orbit, glm::ivec2 grab, glm::ivec2 pointer, float milliseconds)
{
	const auto scale = milliseconds * k_SteerScale;
	orbit.turnSpeed += static_cast<float>(grab.x - pointer.x) * scale * k_TurnSteer;
	orbit.tipSpeed -= static_cast<float>(grab.y - pointer.y) * scale * k_TipSteer;
}

void creature_tattoo_editor::Turn(Orbit& orbit, float milliseconds)
{
	const auto step = milliseconds * k_TurnScale;
	const auto offset = orbit.eye - orbit.focus;
	const auto angle = orbit.turnSpeed * step;
	const auto cosine = std::cos(angle);
	const auto sine = std::sin(angle);
	const auto across = (cosine * offset.x) - (sine * offset.z);
	const auto along = (cosine * offset.z) + (sine * offset.x);
	const auto distance = std::sqrt((across * across) + (along * along));
	const auto wanted = (orbit.tipSpeed * distance * step) + offset.y;
	auto height = distance * k_LowestTip;
	if (height < wanted)
	{
		height = std::min(wanted, distance);
	}
	const auto decay = std::exp(step * k_SpeedDecay);
	orbit.turnSpeed *= decay;
	orbit.tipSpeed *= decay;
	orbit.eye = orbit.focus + glm::vec3(across, height, along);
}
