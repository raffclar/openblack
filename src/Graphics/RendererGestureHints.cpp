/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <limits>

#include <bgfx/bgfx.h>
#include <glm/gtc/type_ptr.hpp>

#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "Creature/TempleLeashes.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "Gestures/GestureHints.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The depth runs from 1 at the near clip to 0 at infinity, as the screen's camera has it
constexpr auto k_ReverseZ =
    glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f);

/// Drawn solid, the nearest in front, culling the backs of the collar's faces as the game's objects are
constexpr uint64_t k_CollarState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_CULL_CW |
                                   BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;

/// The views the collars are drawn in, one for each, as many as there are leashes
constexpr std::array k_CollarPasses = {RenderPass::GestureHintCollar0, RenderPass::GestureHintCollar1,
                                       RenderPass::GestureHintCollar2};
} // namespace

void Renderer::DrawGestureHintCollars(const DrawSceneDesc& desc) const
{
	namespace hints = gesture_hints;
	using ecs::components::LeashPost;
	if (!Locator::gestureSystem::has_value() || !Locator::windowing::has_value() || desc.camera == nullptr)
	{
		return;
	}
	const auto& row = Locator::gestureSystem::value().GetHints();
	if (row.hints.empty() || row.alpha <= 0.0f)
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& textures = Locator::resources::value().GetTextures();
	if (!meshes.Contains(LeashPost::k_MeshId) || !textures.Contains(LeashPost::k_TextureId))
	{
		return;
	}
	const auto& mesh = *meshes.Handle(LeashPost::k_MeshId);
	const auto skin = textures.Handle(LeashPost::k_TextureId)->GetNativeHandle();
	const auto screen = static_cast<glm::vec2>(Locator::windowing::value().GetSize());
	const float focal = hints::FocalPixels(hints::RowWidth(screen), desc.camera->GetHorizontalFieldOfView());
	const auto view = hints::CollarView();

	size_t used = 0;
	for (size_t slot = 0; slot < row.hints.size() && used < k_CollarPasses.size(); ++slot)
	{
		const auto& hint = row.hints.at(slot);
		if (hint.path.empty() || hint.leash == LeashType::None)
		{
			continue;
		}
		const auto picture = hints::PictureSquare(hints::Square(slot, screen));
		const auto rect = hints::PixelsOf(picture);
		if (rect.width <= 0 || rect.height <= 0 || rect.x < 0 || rect.y < 0)
		{
			continue;
		}
		const auto pass = k_CollarPasses.at(used++);
		const auto viewId = static_cast<bgfx::ViewId>(pass);
		if (desc.frameBuffer != nullptr)
		{
			desc.frameBuffer->Bind(pass);
		}
		// The picture's square has its depth cleared, so the collar shows over the world behind it
		bgfx::setViewRect(viewId, static_cast<uint16_t>(rect.x), static_cast<uint16_t>(rect.y),
		                  static_cast<uint16_t>(rect.width), static_cast<uint16_t>(rect.height));
		bgfx::setViewClear(viewId, BGFX_CLEAR_DEPTH, 0, 0.0f, 0);
		const auto projection =
		    k_ReverseZ * hints::CollarProjection(rect, (picture.min + picture.max) * 0.5f, focal, desc.camera->GetNearClip());
		bgfx::setViewTransform(viewId, glm::value_ptr(view), glm::value_ptr(projection));
		bgfx::touch(viewId);

		const auto model = hints::CollarModel(row.collars.at(static_cast<size_t>(hint.leash)));
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = pass;
		submitDesc.program = _shaderManager->GetShader("Object");
		submitDesc.state = k_CollarState;
		submitDesc.modelMatrices = &model;
		submitDesc.matrixCount = 1;
		// The leash's band of the leash texture, not slid round it
		submitDesc.skinTexture = &skin;
		submitDesc.uvOffset = glm::vec2(0.0f, temple_leashes::Band(hint.leash));
		submitDesc.useMaterialBlending = true;
		submitDesc.useMaterialCulling = true;
		DrawMesh(mesh, submitDesc, std::numeric_limits<uint8_t>::max());
	}
}
