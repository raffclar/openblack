/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>
#include <glm/mat3x3.hpp>

#include "Enums.h"

namespace openblack
{
struct GMobileStaticInfo;
}

namespace openblack::ecs::archetypes
{
class MobileStaticArchetype
{
public:
	static entt::entity Create(const glm::vec3& position, MobileStaticInfo type, float altitude, float xAngleRadians,
	                           float yAngleRadians, float zAngleRadians, float scale);
	/// A mobile static from its info with only a Y angle (CREATE_MOBILESTATIC, CHL CREATE): info 8 -> a bonfire (with
	/// temperature 100), info 6 -> nothing, otherwise a Rock or a MobileStatic
	/// @return entt::null when nothing is created
	static entt::entity CreateFromInfo(const glm::vec3& position, MobileStaticInfo type, float altitude, float yAngleRadians,
	                                   float scale);
	/// CREATE_MOBILE_STATIC: info 6 -> a base-only object, info 7 -> nothing, otherwise CreateFromInfo with the Y angle and
	/// scale; then the X, Y and Z angles and the scale set on what was made
	/// @return entt::null when nothing is created
	static entt::entity CreateWithXYZAngles(const glm::vec3& position, MobileStaticInfo type, float altitude,
	                                        float xAngleRadians, float yAngleRadians, float zAngleRadians, float scale);
	/// The mobile static's rotation: affine::RotationYXZ(yAngle, xAngle, zAngle), whose rows are openblack's rotation
	/// columns
	static glm::mat3 XYZRotation(float xAngleRadians, float yAngleRadians, float zAngleRadians);
	MobileStaticArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
