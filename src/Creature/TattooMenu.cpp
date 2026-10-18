/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TattooMenu.h"

#include <cmath>

#include <algorithm>
#include <iterator>
#include <ranges>

#include <PhysiqueFile.h>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "Creature/CreatureMindFileBody.h"

using namespace openblack;

tattoo_menu::Shown tattoo_menu::StandIn()
{
	return {.species = CreatureType::GiantApe,
	        .morph = {.evilGood = 0.0f, .thinFat = 0.5f, .weakStrong = 0.5f},
	        .wounds = {},
	        .blood = {}};
}

tattoo_menu::Shown tattoo_menu::FromPhysique(const std::optional<creaturemind::PhysiqueFileData>& physique,
                                             const SpeciesStrength& speciesStrength)
{
	if (!physique.has_value())
	{
		return StandIn();
	}
	const auto species = creature_mind_body::SpeciesFromRow(physique->speciesRow);
	if (!species.has_value())
	{
		return StandIn();
	}
	Shown shown {.species = *species,
	             .morph = creature_morph::FromAttributes(physique->alignment, physique->fatness, physique->strength,
	                                                     speciesStrength(*species)),
	             .wounds = {},
	             .blood = {}};
	const auto marks = [](const std::vector<uint32_t>& words, auto fromWord) {
		std::vector<creature_marks::Mark> kept;
		std::ranges::transform(words | std::views::take(creature_marks::k_MaxMarks), std::back_inserter(kept), fromWord);
		return kept;
	};
	shown.wounds = marks(physique->wounds, creature_marks::WoundFromWord);
	shown.blood = marks(physique->blood, creature_marks::BloodFromWord);
	return shown;
}

bool tattoo_menu::ButtonShown(bool physiqueKept, bool creatureShown, bool restartPending)
{
	return physiqueKept && creatureShown && !restartPending;
}

std::optional<glm::vec3> tattoo_menu::PointerPoint(const glm::mat4& view, const glm::mat4& projection, glm::vec2 screen,
                                                   glm::vec2 pixel)
{
	if (screen.x <= 0.0f || screen.y <= 0.0f)
	{
		return std::nullopt;
	}
	// A point on the ray through the pixel, in the camera's space, then the point on that ray at the depth
	const glm::vec4 clip {((2.0f * pixel.x) / screen.x) - 1.0f, 1.0f - ((2.0f * pixel.y) / screen.y), 0.5f, 1.0f};
	auto onRay = glm::inverse(projection) * clip;
	if (onRay.w == 0.0f || onRay.z == 0.0f)
	{
		return std::nullopt;
	}
	onRay /= onRay.w;
	const auto scale = k_PointerDepth / onRay.z;
	const glm::vec4 local {onRay.x * scale, onRay.y * scale, k_PointerDepth, 1.0f};
	return glm::vec3(glm::inverse(view) * local);
}

std::vector<tattoo_menu::Layer> tattoo_menu::DrawOrder(bool hasHair, bool hairShown)
{
	std::vector<Layer> layers {Layer::Body};
	if (hasHair && hairShown)
	{
		layers.push_back(Layer::Hair);
	}
	return layers;
}
