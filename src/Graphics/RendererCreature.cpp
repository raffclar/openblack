/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The creatures' bodies, hair and eyes. Each body is the instanced model of its row, drawn with its own pose (ECS/
// Animations.h), its shape blended from its species' meshes in the morphing program, and its painted skins; its hair
// and eyes follow it at once. What to draw comes from Graphics/CreatureDraw.h, collected once a frame. On a land
// without a creature the list is empty and none of this is reached.

#include <cstring>

#include <algorithm>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>

#include <bgfx/bgfx.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "Camera/Camera.h"
#include "Creature/CreatureTattoo.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Registry.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Haze.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/WorldTriangles.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{

/// A colour's red, green and blue, 0 to 255
glm::ivec3 Rgb(uint32_t argb)
{
	return {static_cast<int>((argb >> 16u) & 0xFFu), static_cast<int>((argb >> 8u) & 0xFFu), static_cast<int>(argb & 0xFFu)};
}
} // namespace

void Renderer::CollectCreatures() const
{
	_frameCreatures.clear();
	if (!Locator::entitiesRegistry::has_value() || !Locator::rendereringSystem::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	// through the const registry: a land without a creature gains no storage
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto& meshes = Locator::resources::value().GetMeshes();
	_frameCreatures = creature_draw::Bodies(registry, Locator::rendereringSystem::value().GetContext().entityInstances,
	                                        [&meshes](entt::id_type id) -> std::optional<size_t> {
		                                        if (!meshes.Contains(id))
		                                        {
			                                        return std::nullopt;
		                                        }
		                                        return meshes.Handle(id)->GetBoneMatrices().size();
	                                        });
	_creatureSkins.Update(
	    registry,
	    []() -> std::unique_ptr<Texture2D> {
		    auto texture = std::make_unique<Texture2D>("Creature Skin");
		    texture->Create(creature_tattoo::k_SkinSize, creature_tattoo::k_SkinSize, 1, TextureFormat::BGRA4, Wrapping::Repeat,
		                    Filter::Linear, nullptr);
		    // out of textures: the skin keeps its species' own
		    return bgfx::isValid(toBgfx(texture->GetNativeHandle())) ? std::move(texture) : nullptr;
	    },
	    [](Texture2D& texture, std::span<const uint16_t> texels) {
		    texture.Update(texels.data(), static_cast<uint32_t>(texels.size_bytes()));
	    });
}

bool Renderer::HasCreature(uint32_t offset, uint32_t count) const
{
	if (_frameCreatures.empty())
	{
		return false;
	}
	const auto first = std::ranges::lower_bound(_frameCreatures, offset, {}, &creature_draw::Body::instance);
	return first != _frameCreatures.end() && first->instance < offset + count;
}

void Renderer::UseCreatureBody(const creature_draw::Body& body, L3DMeshSubmitDesc& desc) const
{
	desc.paintedSkins = body.entity;
	if (body.morph.has_value())
	{
		desc.morphTargets = &*body.morph;
		desc.program = _shaderManager->GetShader("ObjectMorphInstanced");
	}
}

void Renderer::BindMorphTargets(const L3DMesh& mesh, const L3DSubMesh& subMesh,
                                const creature_draw::MorphTargets& targets) const
{
	if (!_morphStreamLayouts.front().IsValid())
	{
		// each variant's position and normal as the attributes vs_object_morph_instanced takes them in, the rest of the
		// vertex skipped: L3DSubMesh packs a position, texture coordinates, a normal and two 16-bit bone indices
		constexpr std::array<std::pair<bgfx::Attrib::Enum, bgfx::Attrib::Enum>, 3> k_Attributes {{
		    {bgfx::Attrib::Tangent, bgfx::Attrib::Bitangent},
		    {bgfx::Attrib::TexCoord1, bgfx::Attrib::TexCoord2},
		    {bgfx::Attrib::Color1, bgfx::Attrib::Weight},
		}};
		constexpr uint8_t k_TexCoordBytes = 2 * sizeof(float);
		constexpr uint8_t k_IndicesBytes = 2 * sizeof(int16_t);
		for (size_t axis = 0; axis < k_Attributes.size(); ++axis)
		{
			bgfx::VertexLayout layout;
			layout.begin()
			    .add(k_Attributes.at(axis).first, 3, bgfx::AttribType::Float)
			    .skip(k_TexCoordBytes)
			    .add(k_Attributes.at(axis).second, 3, bgfx::AttribType::Float)
			    .skip(k_IndicesBytes)
			    .end();
			_morphStreamLayouts.at(axis).Reset(bgfx::createVertexLayout(layout));
		}
	}
	const auto& subMeshes = mesh.GetSubMeshes();
	const auto found = std::ranges::find_if(subMeshes, [&subMesh](const auto& other) { return other.get() == &subMesh; });
	const auto index = static_cast<size_t>(std::distance(subMeshes.begin(), found));
	const auto& base = subMesh.GetMesh().GetVertexBuffer();
	const auto& meshes = Locator::resources::value().GetMeshes();
	for (size_t axis = 0; axis < targets.meshes.size(); ++axis)
	{
		const VertexBuffer* buffer = &base;
		if (meshes.Contains(targets.meshes.at(axis)))
		{
			const auto target = meshes.Handle(targets.meshes.at(axis));
			if (index < target->GetSubMeshes().size())
			{
				const auto& candidate = target->GetSubMeshes()[index]->GetMesh().GetVertexBuffer();
				if (candidate.GetCount() == base.GetCount() && candidate.GetStrideBytes() == base.GetStrideBytes())
				{
					buffer = &candidate;
				}
			}
		}
		buffer->BindStream(static_cast<uint8_t>(axis + 1), fromBgfx(_morphStreamLayouts.at(axis).Get()));
	}
}

const Texture2D* Renderer::SkinTexture(const L3DMesh& mesh, uint32_t skinId, entt::entity paintedSkins) const
{
	if (paintedSkins != entt::null)
	{
		if (const auto* painted = _creatureSkins.Find(paintedSkins, skinId); painted != nullptr)
		{
			return painted;
		}
	}
	return world_triangles::PrimitiveTexture(mesh, skinId);
}

void Renderer::DrawCreatureParts(RenderPass viewId, const Camera& camera, const ecs::systems::RenderContext& context,
                                 const creature_draw::Body& body) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& textures = Locator::resources::value().GetTextures();

	// The hair first, at the end of the body's own draw (after the shadows over it), then the eyes, as the original
	// does. The hair: ribbons facing the camera, in the land's light under the creature with the land's colour and the
	// haze added
	if (const auto* hair = registry.TryGet<const ecs::components::CreatureHair>(body.entity);
	    hair != nullptr && !hair->groups.empty())
	{
		const auto origin = glm::vec3(context.instanceUniforms.at(body.instance)[3]);
		glm::ivec3 light {255};
		glm::ivec3 added {0};
		if (IsLandLit())
		{
			const auto sample = land_light::At(land_light::CurrentTable(), glm::vec2(origin.x, origin.z));
			uint32_t diffuse = sample.diffuse;
			const auto specular = haze::ApplyObject(
			    _haze, haze::Depth(camera.GetViewMatrix(Camera::Interpolation::Current), origin), sample.specular, &diffuse);
			light = Rgb(diffuse);
			added = Rgb(specular);
		}
		const auto drawn = creature_draw::BuildHair(*hair, camera.GetOrigin(), light, added);
		const auto& white = RevolvedSurfaceWhiteTexture();
		const bool textured = textures.Contains(ecs::components::CreatureHair::k_TextureId) &&
		                      textures.Contains(ecs::components::CreatureHair::k_AlphaTextureId);
		if (!drawn.textured.indices.empty())
		{
			world_triangles::SubmitRaw(viewId, drawn.textured.vertices, drawn.textured.indices,
			                           textured ? *textures.Handle(ecs::components::CreatureHair::k_TextureId) : white,
			                           textured ? *textures.Handle(ecs::components::CreatureHair::k_AlphaTextureId) : white,
			                           render_modes::materials::k_CreatureHair, *_shaderManager);
		}
		if (!drawn.plain.indices.empty())
		{
			world_triangles::SubmitRaw(viewId, drawn.plain.vertices, drawn.plain.indices, white, white,
			                           render_modes::materials::k_CreatureHairPlain, *_shaderManager);
		}
	}

	// The eyes: each eyeball, then its eyelid in the colour of the skin under it, in the body's light
	if (const auto* eyes = registry.TryGet<const ecs::components::CreatureEyes>(body.entity); eyes != nullptr)
	{
		L3DMeshSubmitDesc submit {};
		submit.viewId = viewId;
		submit.program = _shaderManager->GetShader("ObjectInstanced");
		submit.options = render_modes::k_ModelPass;
		for (const auto& part : creature_draw::Eyes(*eyes, [&meshes](entt::id_type id) { return meshes.Contains(id); }))
		{
			submit.modelMatrices = &part.model;
			submit.matrixCount = 1;
			bgfx::InstanceDataBuffer row {};
			submit.transientInstance = nullptr;
			submit.instanceDesc = std::make_unique<InstanceDesc>(context.instanceUniformBuffer, body.instance, 1);
			if (part.tint.has_value())
			{
				constexpr auto k_Stride = static_cast<uint16_t>(sizeof(creature_draw::InstanceRow));
				if (bgfx::getAvailInstanceDataBuffer(1, k_Stride) < 1)
				{
					continue; // (openblack guard) no room left this frame
				}
				bgfx::allocInstanceDataBuffer(&row, 1, k_Stride);
				const auto drawn = creature_draw::EyeRow(context.instanceUniforms.at(body.instance),
				                                         context.instanceColours.at(body.instance), part.tint);
				std::memcpy(row.data, drawn.data(), sizeof(drawn));
				submit.transientInstance = &row;
			}
			DrawMesh(*meshes.Handle(part.mesh), submit, std::numeric_limits<uint8_t>::max());
		}
	}
}
