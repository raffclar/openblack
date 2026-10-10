/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/core/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/DayNightClock.h"
#include "3D/LandLightFrame.h"
#include "3D/SkyDome.h"
#include "Graphics/Moon.h"
#include "Graphics/Sun.h"

namespace openblack::ecs::components
{

/// The sky's dome: a mesh about the world painted with the dome's pictures, blended for the time of day
struct SkyDome
{
	entt::id_type meshId;
	/// The pictures of the dome, a layer for each alignment from evil to good and, in each, for each time of day from
	/// night to day
	entt::id_type textureId;
	/// How far the blend of the pictures follows the sky
	sky_dome::Follow follow {2.0f};
	/// The rows of the dome to blend again this frame
	sky_dome::FrameRows frameRows {};
	/// The clouds over the camera this frame, 0 for a clear sky and 1 for a full one, which the sun and moon show
	/// through
	float overcast {0.0f};
	/// What the land's light of this frame is built from: the sky's type, the alignment it shows and the weather at the
	/// camera, worked out once a frame before it is drawn, for the moon's colour and the land's light table
	LandLightInputs landLight {};
};

/// The clock of day and night the sky follows, on the dome's entity
struct DayNightCycle
{
	DayNightClock clock;
};

/// A body of the sky drawn as a mesh: its mesh, texture and the texture its alpha is taken from
struct CelestialBody
{
	entt::id_type meshId;
	entt::id_type textureId;
	entt::id_type alphaTextureId;
};

/// A body's glow, a square about it drawn from part of a texture
struct CelestialGlow
{
	entt::id_type textureId;
	entt::id_type alphaTextureId;
};

/// The sun: where it stands this frame, and how it is drawn
struct Sun
{
	/// Where it stands and how strongly it shows before any overcast, 0 to 255; none while it is down
	std::optional<graphics::sun::Placement> placement;
	/// Its warm colour, 0 to 1
	glm::vec3 colour {0x95 / 255.0f, 0x7C / 255.0f, 0x63 / 255.0f};
	/// How strongly it shows through the overcast, 0 to 255
	float strength {0.0f};
};

/// The moon: where it stands from the camera this frame, its phase, and how it is drawn
struct Moon
{
	/// The phase of the real moon, 0 to 2 pi
	float phase {0.0f};
	/// Where it stands from the camera and how strongly it shows before any overcast, 0 to 200; none while it is down
	std::optional<graphics::moon::Placement> placement;
	/// Its colour for the time of day and alignment, 0 to 1
	glm::vec3 colour {1.0f};
	/// How strongly it shows through the overcast, 0 to 255
	float strength {0.0f};
};

} // namespace openblack::ecs::components
