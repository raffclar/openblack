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

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// The world object of a shield spell (a single map fixed object in the global shield list). The magic shield is only
/// a sphere for the tests; the physical shield is the solid, growing MSH_S_SOLID_SHIELD dome.
/// Logic: Magic/Objects/MapShield.cpp.
struct MapShield
{
	enum class Kind : uint8_t
	{
		Magic,    ///< MAGIC_TYPE_SHIELD (19)
		Physical, ///< MAGIC_TYPE_PHYSICAL_SHIELD (20)
	};
	Kind kind {Kind::Magic};
	entt::entity spell {entt::null};         ///< the shield spell (null once dying)
	MagicType magicType {MagicType::Shield}; ///< the spell's shield info
	glm::vec3 position {0.0f};               ///< MapCoords: x, z and the height above the land
	float objectScale {1.0f};                ///< the object's scale: the mesh's size for the tests

	// ---- the physical shield ----
	uint32_t creationTurn {0};
	uint8_t alpha {0xFF};      ///< the draw's max(40, min(1, strength) x 255)
	glm::mat3 rotation {1.0f}; ///< the current matrix's rows (RotY(angle))
	glm::vec3 translation {0.0f};
	float scale {1.0f};
	glm::mat3 previousRotation {1.0f}; ///< the last turn's, for the draw lerp
	glm::vec3 previousTranslation {0.0f};
	float previousScale {1.0f};
	float startScale {1.0f}; ///< finalScale x 0.01
	float finalScale {1.0f}; ///< 0.017 x radius
	float startSpin {1.0f};  ///< clamp(the spell's curl, -3, 3) rad/s
	float endSpin {1.0f};    ///< +-0.15 with the start's sign
	float angle {0.0f};
	float bob {0.0f};
	uint32_t fx {0}; ///< SF_PhysicalShieldFX (psys::manager id)
	bool dying {false};
	float dieTime {0.0f}; ///< seconds
};

} // namespace openblack::ecs::components
