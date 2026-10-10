/*******************************************************************************
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
#include <optional>
#include <string>
#include <utility>

#include <bgfx/bgfx.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Registry.h"
#include "ECS/VillagerEyes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;
namespace villager_eyes = openblack::ecs::villager_eyes;

namespace
{
/// The texture the eyeballs are drawn with, every face's iris in it
constexpr auto k_EyeballTextureId = entt::hashed_string("raw/misc0");

/// The first skin of a model's first submesh: the detailed villagers' heads, which their lids are drawn with
std::optional<TextureHandle> HeadSkin(const L3DMesh& model)
{
	const auto& subMeshes = model.GetSubMeshes();
	if (subMeshes.empty() || subMeshes.front()->GetPrimitives().empty())
	{
		return std::nullopt;
	}
	const auto& skins = model.GetSkins();
	const auto skin = skins.find(subMeshes.front()->GetPrimitives().front().skinID);
	if (skin == skins.end() || !skin->second)
	{
		return std::nullopt;
	}
	return skin->second->GetNativeHandle();
}
} // namespace

void Renderer::DrawVillagerEyes(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !desc.drawEntities)
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_EyeballTextureId.value()))
	{
		return;
	}
	const auto eyeballTexture = textures.Handle(k_EyeballTextureId)->GetNativeHandle();

	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = desc.viewId;
	submitDesc.program = _shaderManager->GetShader("Object");
	// Opaque, over the face where they stand in front of it, each piece culled as its material says: the eyes'
	// frames are mirrored, so which side of a piece is culled follows from how it faces the view
	submitDesc.state = BGFX_STATE_WRITE_MASK | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
	submitDesc.useMaterialCulling = true;
	submitDesc.matrixCount = 1;
	submitDesc.creatureShadows = false;
	// Lit as the villager's body is, by the land's light and colour where it stands and the haze there, but shaded by the
	// sun only once for each eye
	submitDesc.tint = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);

	desc.entities.Each<const ecs::components::HighDetail, const ecs::components::Mesh>(
	    [&](const ecs::components::HighDetail& detail, const ecs::components::Mesh& mesh) {
		    if (!detail.drawnEyes.has_value() || !detail.face.has_value() || !meshes.Contains(mesh.id))
		    {
			    return;
		    }
		    const auto face = *detail.face;
		    std::array<const L3DMesh*, villager_eyes::k_PieceCount> pieces {};
		    for (size_t i = 0; i < pieces.size(); ++i)
		    {
			    const auto id = resources::HashIdentifier(
			        "misc/" + std::string(villager_eyes::PieceFile(face, static_cast<villager_eyes::Piece>(i))));
			    if (!meshes.Contains(id))
			    {
				    return;
			    }
			    pieces.at(i) = meshes.Handle(id).operator->();
		    }
		    const auto headSkin = HeadSkin(*meshes.Handle(mesh.id));
		    if (!headSkin.has_value())
		    {
			    return;
		    }
		    // The eyeball reads its face's iris, and the lids a patch of the head's skin
		    const std::array eyeballSkin {std::pair {0u, eyeballTexture}};
		    const std::array lidSkin {std::pair {0u, *headSkin}};
		    const auto draw = [&](const L3DMesh& piece, const glm::mat4& matrix) {
			    submitDesc.modelMatrices = &matrix;
			    DrawMesh(piece, submitDesc, std::numeric_limits<uint8_t>::max());
		    };
		    // Each eye in its own shade of the villager's light: its eyeball turned as it looks, then its upper and lower
		    // lids
		    for (const auto side : {villager_eyes::Side::Right, villager_eyes::Side::Left})
		    {
			    const auto& eye = detail.drawnEyes->eyes.at(static_cast<size_t>(side));
			    submitDesc.shadeAt = glm::vec4(detail.drawnEyes->standing, static_cast<float>(eye.shade));
			    submitDesc.subMeshTextures = eyeballSkin;
			    submitDesc.uvOffset = villager_eyes::IrisOffset(face);
			    submitDesc.uvScale = 1.0f;
			    draw(*pieces.at(static_cast<size_t>(villager_eyes::Piece::Eyeball)), eye.eyeball);
			    const bool right = side == villager_eyes::Side::Right;
			    submitDesc.subMeshTextures = lidSkin;
			    submitDesc.uvOffset = glm::vec2(0.0f);
			    submitDesc.uvScale = villager_eyes::k_LidTextureScale;
			    draw(*pieces.at(
			             static_cast<size_t>(right ? villager_eyes::Piece::RightUpperLid : villager_eyes::Piece::LeftUpperLid)),
			         eye.upperLid);
			    draw(*pieces.at(
			             static_cast<size_t>(right ? villager_eyes::Piece::RightLowerLid : villager_eyes::Piece::LeftLowerLid)),
			         eye.lowerLid);
		    }
	    });
}
