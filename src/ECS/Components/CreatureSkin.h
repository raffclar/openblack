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

#include <array>
#include <optional>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/gtc/type_precision.hpp>

#include "Creature/CreatureMarks.h"
#include "Creature/CreatureTattoo.h"

namespace openblack::ecs::components
{

/// The tattoos a creature wears, in its eight slots
struct CreatureTattoos
{
	creature_tattoo::Slots slots {};
	/// The block of the player's own pictures that the slots use (see creature_tattoo::PicturesBlock), empty when the
	/// creature has none
	std::vector<uint8_t> pictures;
	/// The tattoos that the skin shows in place of the slots: the tattoos being edited, while the tattoo editor shows
	/// this creature, or the change from the tattoo editor that waits for its turn
	std::optional<creature_tattoo::Slots> shown;
	/// When set, the skin shows shownPictures in place of the block of pictures, while a change from the tattoo editor
	/// waits for its turn
	bool picturesShown {false};
	std::vector<uint8_t> shownPictures;
	/// Goes up each time that the game paints the tattoos again. A change of the slots alone does not paint them.
	uint32_t revision {0};
};

/// On a player's entity: the masks that the tattoos of the player's creatures paint with (see creature_tattoo::Masks)
struct TattooMasks
{
	creature_tattoo::Masks masks {};
};

/// The wounds, burns and blood on a creature's skins
struct CreatureMarks
{
	creature_marks::Marks marks {};
	/// Goes up with each change, which paints the skins again
	uint32_t revision {0};
};

/// A creature's skins as painted: each of its base mesh's skins blended towards its evil or good mesh's, with its
/// tattoos and marks over them, 4 bits a channel
struct CreatureSkin
{
	struct Skin
	{
		uint32_t id;
		/// 256 by 256 texels, blue the lowest 4 bits and alpha the highest
		std::vector<uint16_t> texels;
	};
	std::vector<Skin> skins;

	/// What the skins were last painted with
	struct Painted
	{
		entt::id_type baseMesh;
		entt::id_type variantMesh;
		uint8_t weight;
		uint32_t tattoos;
		uint32_t marks;
		bool art;

		bool operator==(const Painted&) const = default;
	};
	std::optional<Painted> painted;
	/// Goes up each time the skins are painted, for the renderer to take them up
	uint32_t revision {0};
	/// The colour of the painted skin under the point of the eyelids, 0 to 255 a channel. None when the species has no
	/// eyelids or the skin has no colour there.
	std::optional<glm::u8vec3> lidColour;
};

/// Wounds that bleed send drips of blood down the skin, up to eight at once. Each drip is kept as a point on a triangle
/// of the body: the submesh, the triangle by its first index, and how far along its first side (s) and third side (t).
struct CreatureDrips
{
	/// A drip runs for this many milliseconds of game time; a place whose drip has run that long is free
	static constexpr int32_t k_RunMs = 12000;
	struct Drip
	{
		uint32_t subMesh {0};
		uint32_t firstIndex {0};
		float s {0.0f};
		float t {0.0f};
		int32_t ms {k_RunMs};
	};
	std::array<Drip, 8> drips {};
};

} // namespace openblack::ecs::components
