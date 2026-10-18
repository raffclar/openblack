/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the draw of the moon that looks like the real moon (graphics::real_moon)

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <span>

#include <bgfx/bgfx.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec4.hpp>

#include "Camera/Camera.h"
#include "ECS/Components/Sky.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "Graphics/Atmosphere.h"
#include "Graphics/RealMoon.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Texture2D.h"
#include "Graphics/Underwater.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The colour of the moon's light in the halo: a little cool, as the eye sees a dim light at night
constexpr glm::vec3 k_HaloColour {0.85f, 0.9f, 1.0f};
} // namespace

void Renderer::DrawRealMoon(RenderPass viewId) const
{
	const auto* program = _shaderManager->GetShader("RealMoon");
	if (program == nullptr || !Locator::camera::has_value() || !Locator::skySystem::has_value())
	{
		return;
	}
	auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(real_moon::k_MapId.value()))
	{
		return;
	}
	const auto& sky = Locator::skySystem::value();
	const auto* moon = Locator::entitiesRegistry::value().TryGet<const ecs::components::Moon>(sky.GetMoon());
	const bool placed = moon != nullptr && moon->placement.has_value();
	if (moon == nullptr || (!placed && !_warmingUp))
	{
		return;
	}

	// The moon's place and its face from the eye. A warm-up with no moon in the sky draws it at no size.
	const glm::vec3 offset = placed ? moon->placement->offset : glm::vec3(0.0f, atmosphere::k_MoonDistance, 0.0f);
	const glm::vec3 towards = glm::normalize(offset);
	// EXPERIMENT (not for merge): far round the planet the moon shows its face as it shows over that place
	const glm::quat planetFrame = Locator::camera::value().GetPlanetFrame();
	const glm::vec3 towardsSun = glm::conjugate(planetFrame) * atmosphere::SunTowards(sky.GetClock().GetScriptTime());
	auto face = real_moon::FaceAxes(planetFrame * towards);
	face.east = glm::conjugate(planetFrame) * face.east;
	face.north = glm::conjugate(planetFrame) * face.north;
	face.toEye = glm::conjugate(planetFrame) * face.toEye;
	const glm::vec3 centre = Locator::camera::value().GetOrigin() + offset;
	const float extent = placed ? real_moon::Radius(atmosphere::k_MoonDistance) * real_moon::k_QuadRadii : 0.0f;

	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 across;
	};
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .end();
	constexpr std::array<glm::vec2, 6> k_Corners {glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(1.0f, 1.0f),
	                                              glm::vec2(1.0f, 1.0f),   glm::vec2(-1.0f, 1.0f), glm::vec2(-1.0f, -1.0f)};
	constexpr auto k_Count = static_cast<uint32_t>(k_Corners.size());
	if (bgfx::getAvailTransientVertexBuffer(k_Count, layout) != k_Count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, k_Count, layout);
	const auto vertices = std::span(reinterpret_cast<Vertex*>(buffer.data), k_Count);
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		const glm::vec2 corner = k_Corners.at(i);
		vertices[i] = {.position = centre + (face.east * corner.x + face.north * corner.y) * extent,
		               .across = corner * real_moon::k_QuadRadii};
	}

	// Its light: the phase, the air near the horizon, and the overcast
	const float clear = moon->clearSky;
	const glm::vec3 tint = real_moon::AirTint(towards.y) * clear;
	const float moonLight = underwater::PhaseBrightness(moon->phase);
	const glm::vec4 east {face.east, 0.0f};
	const glm::vec4 north {face.north, real_moon::k_QuadRadii};
	const glm::vec4 toEye {face.toEye, real_moon::EarthLight(glm::dot(towards, towardsSun))};
	const glm::vec4 sun {towardsSun, real_moon::k_Gain};
	const glm::vec4 tintAndCover {tint, real_moon::Cover(sky.GetCurrentSkyType()) * clear};
	const glm::vec4 halo {tint * k_HaloColour * real_moon::k_Gain, real_moon::HaloStrength(towards.y, moonLight) * clear};
	program->SetTextureSampler("s_moonMap", 0, *textures.Handle(real_moon::k_MapId.value()));
	program->SetUniformValue("u_moonEast", &east);
	program->SetUniformValue("u_moonNorth", &north);
	program->SetUniformValue("u_moonToEye", &toEye);
	program->SetUniformValue("u_moonSun", &sun);
	program->SetUniformValue("u_moonTint", &tintAndCover);
	program->SetUniformValue("u_moonHalo", &halo);
	bgfx::setVertexBuffer(0, &buffer);
	// It leaves no depth, so the clouds and the haze, which come after it, draw over it
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER |
	               BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
	program->Submit(static_cast<bgfx::ViewId>(viewId));
}
