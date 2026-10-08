/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{
/// The Rock class of the original: mobile statics whose info.dat mobileType is 2 (Rock, Boulder* .. Squarerock*,
/// Meteor). Taller rocks break in two when tapped (Tap -> SplitInTwo).
class Rocks
{
public:
	[[nodiscard]] static bool IsRock(entt::entity entity);
	/// Get2DRadius (a rock does not override it): max(half size x, half size z) of the mesh box x
	/// scale (ecs::object::Get2DRadius).
	[[nodiscard]] static float Radius2D(entt::entity entity);
	/// GetHeight: 2 x half size y of the mesh box x scale (ecs::object::GetHeight).
	[[nodiscard]] static float Height(entt::entity entity);
	/// Rocks with a 2D radius over 3.6 cannot be lifted.
	[[nodiscard]] static bool ValidForPlaceInHand(entt::entity entity);
	/// Taller than 0.7.
	[[nodiscard]] static bool ValidToTap(entt::entity entity);
	/// Splits the rock and plays LH_SAMPLE_G_ROCKTAP_01..04 in turn at the hand.
	static std::array<entt::entity, 2> Tap(entt::entity entity, glm::vec3 handPosition);
	/// Two rocks of the same type, scale x 0.7935, the Y angle only, at
	/// Pos +- (cos a, 0, sin a) x 0.7935 x radius 2D (a random); the original is deleted. Both halves enter physics
	/// with the given velocity and angular momentum (a flying rock's, halved). Returns them.
	static std::array<entt::entity, 2> SplitInTwo(entt::entity entity, glm::vec3 velocity = glm::vec3(0.0f),
	                                              glm::vec3 angularMomentum = glm::vec3(0.0f));
	Rocks() = delete;
};
} // namespace openblack::ecs
