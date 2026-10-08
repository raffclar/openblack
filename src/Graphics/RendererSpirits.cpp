/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The advisor spirits (Help/SpiritsRuntime.h), drawn in the frame or in the overlay, and their trail:
// - the model, a skinned object drawn in the frame (in-world blend >= 0.5) or from the end-of-frame callback (priority
//   100, after the Z reset quad, before the 2D rectangles' flush at 10000 and HelpText at 20000;
//   docs/bw1-notes/original-frame.md row 24), skinned with the world bone matrices of its animation stack. 97 / 73
//   bones: the default object program (BGFX_CONFIG_MAX_BONES 128, 48 on D3D9, vs_object.sc), as
//   Renderer::DrawSubMesh picks it for more than 32 bones;
// - the halo and the puff, sprites with the smoke material (mode 6, two-sided);
// - the rainbow trail, world triangles with rainbow.raw in mode 15 and ZFUNC LESS, from the "before" callback
//   (priority 100, after the Z-sorter's flush and before the Z reset).
// All of it read from the frame's graphics::OverlayFrame (Graphics/OverlayFrame.h), which help::spirits::Runtime
// fills before the draw (Game.cpp FillOverlayFrame): nothing here asks the spirits' runtime.

#include <cstring>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <tuple>
#include <vector>

#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/LandMorph.h"
#include "Camera/Camera.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/InstanceDesc.h"
#include "Graphics/ModelLight.h"
#include "Graphics/OverlayFrame.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Help/SpiritsRuntime.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();
constexpr entt::id_type k_Rainbow = entt::hashed_string("raw/rainbow").value();
constexpr entt::id_type k_RainbowAlpha = entt::hashed_string("raw/rainbowa").value();

/// The trail's material (rainbow.raw, mode 15, ALPHAREF 5, two-sided). (approximate) the WorldQuad
/// program has no alpha test, so the ALPHAREF 5 of mode 15 is not applied
constexpr render_modes::Material k_RainbowMaterial {render_modes::Mode::TexturedChromaAlpha, render_modes::k_TwoSided};

/// One instance of RenderingSystemCommon's layout: the model matrix (its [0][3] the fade, 1 - alpha) and the colour
/// fields of argb_colour::PackInstance*
struct SpiritInstance
{
	glm::mat4 model {1.0f};
	glm::vec4 lh3d {0.0f};
};
static_assert(sizeof(SpiritInstance) == sizeof(glm::mat4) + sizeof(glm::vec4));
/// Two dudes in the frame, two in the overlay: each range is written once a frame
constexpr uint32_t k_SpiritInstances = 4;

const bgfx::VertexLayout& QuadLayout()
{
	static const bgfx::VertexLayout k_Layout = []() {
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		    .end();
		return layout;
	}();
	return k_Layout;
}
} // namespace

void Renderer::SubmitSpiritQuads(RenderPass viewId, const std::vector<SpiritQuadVertex>& vertices, uint32_t texture,
                                 uint32_t alpha, uint64_t state) const
{
	const auto& textures = Locator::resources::value().GetTextures();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (count == 0 || !textures.Contains(texture) || !textures.Contains(alpha) ||
	    bgfx::getAvailTransientVertexBuffer(count, QuadLayout()) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, QuadLayout());
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(SpiritQuadVertex));
	const auto* program = _shaderManager->GetShader("WorldQuad");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(texture));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(alpha));
	bgfx::setVertexBuffer(0, &buffer);
	bgfx::setState(state);
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
}

void Renderer::DrawSpirits(const Camera& camera, const OverlayFrame& frame, bool overlay) const
{
	const auto inPass = [overlay](const SpiritOverlay& spirit) { return spirit.overlay == overlay; };
	if (std::none_of(frame.spirits.begin(), frame.spirits.end(), inPass))
	{
		return;
	}
	// the overlay: its own view after the Z reset (ConfigureView clears its depth), with the drawn camera; the frame's
	// dudes go after the Z-sorter's drain (inferred: the in-frame draw comes after the models, row 20 of the frame)
	const auto viewId = overlay ? RenderPass::FinishFrame3D : RenderPass::MainBlended;
	_shaderManager->SetCamera(viewId, camera); // MainBlended's is only set by the entities' section
	if (!_spiritInstances.IsValid())
	{
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
		    .end();
		_spiritInstances.Reset(bgfx::createDynamicVertexBuffer(k_SpiritInstances, layout));
	}
	// the in-world colour needs this frame's land light table, which UpdateLandLight builds inside DrawScene (once it
	// moves out, the colour can come filled)
	const LandLightTable* table = IsLandLit() ? _landLight.get() : nullptr;
	std::array<SpiritInstance, help::spirits::k_Dudes> instances {};
	for (const auto& spirit : frame.spirits)
	{
		if (!inPass(spirit))
		{
			continue;
		}
		auto& instance = instances.at(static_cast<size_t>(spirit.advisor));
		// the bones are in the world, so the instance is the identity; the object colour's alpha byte as the fade, its
		// rgb and the specular as the spirit sets them
		instance.model = spirit.model;
		instance.model[0][3] = 1.0f - static_cast<float>(spirit.alpha) / 255.0f;
		uint32_t colour = spirit.colour;
		uint32_t specular = spirit.specular;
		if (spirit.landLightPoint && table != nullptr)
		{
			const auto sample = land_light::At(*table, *spirit.landLightPoint);
			std::tie(colour, specular) = help::spirits::Runtime::WorldColour(sample, spirit.inWorld);
		}
		argb_colour::PackInstanceColour(instance.lh3d, colour);
		argb_colour::PackInstanceSpecular(instance.lh3d, specular);
	}
	const uint32_t first = overlay ? help::spirits::k_Dudes : 0;
	bgfx::update(_spiritInstances.Get(), first,
	             bgfx::copy(instances.data(), static_cast<uint32_t>(instances.size() * sizeof(SpiritInstance))));

	for (const auto& spirit : frame.spirits)
	{
		if (!inPass(spirit))
		{
			continue;
		}
		// nothing at alpha 0
		if (spirit.alpha != 0 && spirit.mesh != nullptr && !spirit.bones.empty() &&
		    spirit.bones.size() == spirit.mesh->GetBoneMatrices().size())
		{
			// the light moved for the overlay, put back at the end
			std::optional<model_light::ScopedLight> light;
			if (spirit.lightPosition)
			{
				light.emplace(*spirit.lightPosition);
			}
			// alpha < 255: (inferred) the blended path, the alpha table with the alpha
			const bool fading = spirit.alpha < 255;
			L3DMeshSubmitDesc desc = {};
			desc.viewId = viewId;
			desc.program = land_morph::ObjectProgram(*_shaderManager, false);
			desc.options = fading ? render_modes::StateOptions {.msaa = true} : render_modes::k_ModelPass;
			desc.table = fading ? render_modes::Table::GlobalAlpha : render_modes::Table::Normal;
			desc.globalAlpha = spirit.alpha;
			desc.modelMatrices = spirit.bones.data();
			desc.matrixCount = static_cast<uint8_t>(spirit.bones.size());
			desc.instanceDesc = std::make_unique<graphics::InstanceDesc>(fromBgfx(_spiritInstances.Get()),
			                                                             first + static_cast<uint32_t>(spirit.advisor), 1);
			desc.noHaze = true; // the spirit's own colour: no distance haze
			DrawMesh(*spirit.mesh, desc, std::numeric_limits<uint8_t>::max());
		}
		// the halo, then the puff, smoke.raw in mode 6, already as world triangles
		SubmitSpiritQuads(viewId, spirit.sprites, k_Smoke, k_SmokeAlpha, render_modes::State(render_modes::materials::k_Smoke));
	}
}

void Renderer::DrawSpiritTrails(const Camera& camera, const OverlayFrame& frame) const
{
	if (frame.spiritTrails.empty())
	{
		return;
	}
	_shaderManager->SetCamera(RenderPass::MainBlended, camera);
	// ZFUNC LESS during the draw, LESSEQUAL after (openblack's LessEqual is strict)
	SubmitSpiritQuads(RenderPass::MainBlended, frame.spiritTrails, k_Rainbow, k_RainbowAlpha,
	                  render_modes::State(k_RainbowMaterial, {.zFunc = render_modes::ZFunc::LessEqual}));
}
