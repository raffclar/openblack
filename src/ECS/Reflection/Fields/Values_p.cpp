/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Written by tools/inspector/generate_component_fields.py: run it again rather than editing this file.

#include <array>
#include <span>
#include <string_view>

#include "ECS/Reflection/ComponentReflection.h"
#include "Particles/ParticleSpellLink.h"
#include "Particles/RisingSparks.h"
#include "Physics/Body.h"
#include "Physics/DamageMesh.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_p(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_p(entt::meta_ctx& context)
{
	Reflect<openblack::particles::ProcessInfo>(context, ValueOnly {})
	    .Field<&openblack::particles::ProcessInfo::handPosition>("handPosition")
	    .Field<&openblack::particles::ProcessInfo::cameraForward>("cameraForward")
	    .Field<&openblack::particles::ProcessInfo::direction>("direction")
	    .Field<&openblack::particles::ProcessInfo::power>("power")
	    .Field<&openblack::particles::ProcessInfo::enabled>("enabled")
	    .Field<&openblack::particles::ProcessInfo::spin>("spin");
	Reflect<openblack::particles::rising_sparks::Spark>(context, ValueOnly {})
	    .Field<&openblack::particles::rising_sparks::Spark::ageMilliseconds>("ageMilliseconds")
	    .Field<&openblack::particles::rising_sparks::Spark::angle>("angle");
	Reflect<openblack::particles::rising_sparks::Sparks>(context, ValueOnly {})
	    .Field<&openblack::particles::rising_sparks::Sparks::sparks>("sparks")
	    .Field<&openblack::particles::rising_sparks::Sparks::firstPicture>("firstPicture");
	Reflect<openblack::physics::FlightPlace>(context, ValueOnly {})
	    .Field<&openblack::physics::FlightPlace::axes>("axes")
	    .Field<&openblack::physics::FlightPlace::centre>("centre");
	Reflect<openblack::physics::damage::Corner>(context, ValueOnly {})
	    .Field<&openblack::physics::damage::Corner::position>("position")
	    .Field<&openblack::physics::damage::Corner::uv>("uv");
	Reflect<openblack::physics::damage::Mesh>(context, ValueOnly {})
	    .Field<&openblack::physics::damage::Mesh::primitives>("primitives")
	    .Field<&openblack::physics::damage::Mesh::trianglesAtCreation>("trianglesAtCreation")
	    .Field<&openblack::physics::damage::Mesh::remaining>("remaining")
	    .Field<&openblack::physics::damage::Mesh::snowLevel>("snowLevel")
	    .Field<&openblack::physics::damage::Mesh::snowFrozen>("snowFrozen");
	Reflect<openblack::physics::damage::Primitive>(context, ValueOnly {})
	    .Field<&openblack::physics::damage::Primitive::material>("material")
	    .Field<&openblack::physics::damage::Primitive::triangles>("triangles");
	Reflect<openblack::physics::damage::Triangle>(context, ValueOnly {})
	    .Field<&openblack::physics::damage::Triangle::corners>("corners")
	    .Field<&openblack::physics::damage::Triangle::neighbours>("neighbours")
	    .Field<&openblack::physics::damage::Triangle::group>("group")
	    .Field<&openblack::physics::damage::Triangle::sizeClass>("sizeClass")
	    .Field<&openblack::physics::damage::Triangle::countedClass>("countedClass");
}
