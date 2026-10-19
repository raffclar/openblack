/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The words of the game's pause and speed ("Game Paused", "Slow Down Time", "Speed Up Time"), as the renderer draws
// them: their letters in the world in front of the camera, after the rest of the scene.

#define LOCATOR_IMPLEMENTATIONS

#include <cstring>

#include <bgfx/bgfx.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include "3D/OrientedText.h"
#include "Camera/Camera.h"
#include "ECS/Systems/PauseTextSystemInterface.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Renderer.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::graphics;

void Renderer::DrawPauseText(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::pauseTextSystem::has_value() || !Locator::windowing::has_value())
	{
		return;
	}
	const auto& words = Locator::pauseTextSystem::value();
	const auto* texture = words.GetTextTexture();
	const auto vertices = words.GetText(*desc.camera, glm::u16vec2(Locator::windowing::value().GetSize()));
	if (vertices.empty() || texture == nullptr)
	{
		return;
	}
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), count * sizeof(OrientedTextVertex));

	// The game draws the words after the whole scene, the hand too. The glyphs blend by their coverage and the words'
	// alpha. The scene's depth hides a letter behind the land, and the letters do not write the depth.
	const auto* shader = _shaderManager->GetShader("Text3D");
	const auto model = glm::mat4(1.0f);
	bgfx::setTransform(glm::value_ptr(model));
	bgfx::setVertexBuffer(0, &buffer);
	shader->SetTextureSampler("s_texture", 0, *texture);
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA |
	               BGFX_STATE_BLEND_ALPHA);
	// The nearest sort key of the pass that blends, which draws the farthest first, puts the words last
	shader->Submit(static_cast<bgfx::ViewId>(TranslucentView(desc.viewId)), 0);
}
