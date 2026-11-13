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

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/gtx/transform.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/Rain.h"
#include "3D/SnowCover.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/WaterRings.h"
#include "Camera/Camera.h"
#include "Creature/CreatureFizzLook.h"
#include "ECS/AbodeKnock.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/DrawnPlace.h"
#include "ECS/PlayerColour.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/CreatureReflection.h"
#include "Graphics/DebugLines.h"
#include "Graphics/FizzDrawOrder.h"
#include "Graphics/GroundBlobs.h"
#include "Graphics/HandLight.h"
#include "Graphics/HandReflection.h"
#include "Graphics/Mesh.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/RendererDetail.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ViewFrustum.h"
#include "Graphics/ZSort.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/Rereads.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/ObjectDrawList.h"
#include "entt/locator/locator.hpp"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::detail;
using namespace openblack::ecs::systems;

void Renderer::DrawCreatureFootprints(const DrawSceneDesc& desc) const
{
	namespace prints = creature_footprints;
	// Drawn once, after the land: the land mirrored in the sea has none
	if (desc.viewId != RenderPass::Main || !Locator::footprintSystem::has_value() ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	const auto& footprints = Locator::footprintSystem::value();
	// A warm-up draws a print of no size when there is none, as the first print draws
	const bool warmUp = _warmingUp && footprints.GetPrints().empty();
	if (!footprints.IsShown() || (footprints.GetPrints().empty() && !warmUp))
	{
		return;
	}
	// White where the prints' pictures are, and their shapes in the alpha beside it
	static constexpr auto k_TextureId = entt::hashed_string("raw/misc0");
	static constexpr auto k_AlphaTextureId = entt::hashed_string("raw/misc0a");
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_TextureId.value()) || !textures.Contains(k_AlphaTextureId.value()))
	{
		return;
	}
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};
	std::vector<Vertex> vertices;
	vertices.reserve(footprints.GetPrints().size() * prints::k_Indices.size());
	for (const auto& print : footprints.GetPrints())
	{
		// Black, as opaque as the print still is
		const auto colour = static_cast<uint32_t>(print.alpha) << 24u;
		for (const auto corner : prints::k_Indices)
		{
			vertices.push_back({print.corners.at(corner), print.uvs.at(corner), colour});
		}
	}
	if (warmUp)
	{
		vertices.resize(prints::k_Indices.size(), Vertex {.position = glm::vec3(0.0f), .uv = glm::vec2(0.0f), .colour = 0});
	}
	if (vertices.empty())
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
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
	const auto* program = _shaderManager->GetShader("WorldTextured");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_TextureId));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_AlphaTextureId));
	bgfx::setVertexBuffer(0, &buffer);
	// Unlit, blended over the land, tested against depth but leaving none, both sides
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA);
	// Lying on the land, they are blended first of all that blends, straight after the land is drawn
	program->Submit(static_cast<uint16_t>(TranslucentView(desc.viewId)), std::numeric_limits<uint32_t>::max());
}

void Renderer::DrawGroundBlobs(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::terrainSystem::has_value() ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	static constexpr auto k_TextureId = entt::hashed_string("raw/human_shadow");
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_TextureId.value()))
	{
		return;
	}
	const auto& island = Locator::terrainSystem::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};
	std::vector<Vertex> vertices;
	const auto addQuad = [&vertices](const ground_blobs::Quad& quad) {
		for (const auto corner : ground_blobs::k_Indices)
		{
			const auto opacity = static_cast<uint32_t>(ground_blobs::k_Opacity.at(corner) * 255.0f);
			vertices.push_back({quad.corners.at(corner), ground_blobs::k_Uvs.at(corner), (opacity << 24u) | 0xFFFFFFu});
		}
	};
	const auto& farVillagers = Locator::rendereringSystem::value().GetContext().farVillagers;
	// Room for the most quads the frame can make: two for each villager, each far villager and each foot of an animal
	const auto& animalDraws = Locator::rendereringSystem::value().GetContext().animalDraws;
	vertices.reserve(ground_blobs::k_Indices.size() *
	                 ((2 * (desc.entities.Size<ecs::components::Villager>() + farVillagers.size())) +
	                  (ground_blobs::k_MostAnimalFeet * animalDraws.size())));
	const auto isFar = [&farVillagers](entt::entity entity) {
		const auto found = std::ranges::lower_bound(farVillagers, entity, {}, &RenderContext::FarVillager::entity);
		return found != farVillagers.end() && found->entity == entity;
	};
	// Every villager out of doors and out of the sea casts one from each foot, on the land beneath it; one too far away
	// to be drawn a wider one from where it stands, as if the land there were flat
	// A villager a script draws in high detail casts none: it lays its shadow on the land instead (GroundShadow)
	const ecs::drawn_place::DrawnPlaces drawnPlaces(desc.entities);
	// A blob out of the view of the pass puts no pixel on the screen, so the function does not make it. Each foot is on
	// the land, lifted, so the test uses a column from the lowest land to the highest land. The column is wider and taller
	// than that by the largest distance from the point to a corner.
	const auto seenBy = view_frustum::FromViewProjection(desc.camera->GetProjectionMatrix() *
	                                                     desc.camera->GetViewMatrix(Camera::Interpolation::Current));
	const auto seesColumn = [&seenBy](const glm::vec3& centre, float reach, float lift) {
		return view_frustum::SeesWorldBox(
		    seenBy, glm::vec3(centre.x - reach, -reach, centre.z - reach),
		    glm::vec3(centre.x + reach, view_frustum::k_HighestLand + lift + reach, centre.z + reach));
	};
	desc.entities.Each<const ecs::components::Villager, const ecs::components::Transform, const ecs::components::Mesh>(
	    [&](entt::entity entity, const ecs::components::Villager& /*villager*/, const ecs::components::Transform& placed,
	        const ecs::components::Mesh& mesh) {
		    // A villager in a creature's hand is where the frame draws the hand
		    const auto transform = drawnPlaces.Of(entity, placed);
		    if (transform.position.y <= ground_blobs::k_LowestHeight || !meshes.Contains(mesh.id) || isFar(entity))
		    {
			    return;
		    }
		    // From the feet as they are drawn this frame, in the pose of its clip
		    const auto& rest = meshes.Handle(mesh.id)->GetBoneMatrices();
		    const auto* pose = desc.entities.TryGet<const ecs::components::VillagerPose>(entity);
		    const auto bones = ground_blobs::FootBones(
		        pose != nullptr ? std::span<const glm::mat4>(pose->bones) : std::span<const glm::mat4> {}, rest);
		    if (std::ranges::any_of(ground_blobs::k_FootBones, [&bones](size_t bone) { return bone >= bones.size(); }))
		    {
			    return;
		    }
		    // The feet are no farther from where the body is drawn than the origins of their bones, at the largest scale
		    const auto firstReach = ground_blobs::BoneReach(bones[ground_blobs::k_FootBones[0]], transform.scale);
		    const auto secondReach = ground_blobs::BoneReach(bones[ground_blobs::k_FootBones[1]], transform.scale);
		    if (firstReach.has_value() && secondReach.has_value() &&
		        !seesColumn(ecs::components::DrawnPosition(transform.position, pose),
		                    ground_blobs::FeetReach(*firstReach, *secondReach, transform.scale.x), ground_blobs::k_Lift))
		    {
			    return;
		    }
		    // Where its body is drawn this frame, gliding between its turns
		    const auto model = glm::translate(ecs::components::DrawnPosition(transform.position, pose)) *
		                       glm::mat4(ecs::components::DrawnRotation(transform.rotation, pose)) *
		                       glm::scale(transform.scale);
		    const auto foot = [&](size_t bone) {
			    auto position = glm::vec3(model * bones[bone] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			    position.y = island.GetHeightAt(glm::vec2(position.x, position.z)) + ground_blobs::k_Lift;
			    return position;
		    };
		    const auto normal = island.GetNormalAt(glm::vec2(transform.position.x, transform.position.z));
		    const auto fall = ground_blobs::Fall(normal, transform.scale.x);
		    for (const auto& quad :
		         ground_blobs::Feet(foot(ground_blobs::k_FootBones[0]), foot(ground_blobs::k_FootBones[1]), fall))
		    {
			    addQuad(quad);
		    }
	    },
	    entt::exclude<ecs::components::AtHome, ecs::components::HiddenByState, ecs::components::HighDetail>);
	// Every animal drawn out of the sea casts one from each foot that its model gives, on the land beneath that foot.
	// The model gives each foot as a point on a bone, in the space of that bone.
	for (const auto& [entity, meshId, model] : animalDraws)
	{
		const auto* transform = desc.entities.TryGet<const ecs::components::Transform>(entity);
		if (transform == nullptr || model[3].y <= ground_blobs::k_LowestHeight || !meshes.Contains(meshId))
		{
			continue;
		}
		const auto drawnMesh = meshes.Handle(meshId);
		const auto& frames = drawnMesh->GetBoneFrames();
		const auto count = ground_blobs::AnimalFootCount(frames.size());
		const auto bones = ecs::posed_model::BonesOf(desc.entities, entity, *drawnMesh);
		if (count == 0 || std::any_of(frames.begin(), frames.begin() + static_cast<std::ptrdiff_t>(count),
		                              [&bones](const auto& frame) { return frame.bone >= bones.size(); }))
		{
			continue;
		}
		std::array<glm::vec3, ground_blobs::k_MostAnimalFeet> feet {};
		for (size_t i = 0; i < count; ++i)
		{
			auto& foot = feet.at(i);
			foot = glm::vec3(model * bones[frames[i].bone] * frames[i].frame * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			foot.y = island.GetHeightAt(glm::vec2(foot.x, foot.z)) + ground_blobs::k_Lift;
		}
		const auto normal = island.GetNormalAt(glm::vec2(model[3].x, model[3].z));
		const auto fall = ground_blobs::Fall(normal, transform->scale.x);
		const auto quads = ground_blobs::AnimalFeet(std::span<const glm::vec3>(feet).first(count), fall);
		for (size_t i = 0; i < count; ++i)
		{
			addQuad(quads.at(i));
		}
	}
	for (const auto& far : farVillagers)
	{
		if (!seesColumn(far.position, ground_blobs::FarReach(far.scale), ground_blobs::k_FarLift))
		{
			continue;
		}
		auto foot = far.position;
		foot.y = island.GetHeightAt(glm::vec2(foot.x, foot.z)) + ground_blobs::k_FarLift;
		for (const auto& quad : ground_blobs::Far(foot, far.scale))
		{
			addQuad(quad);
		}
	}
	if (vertices.empty())
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
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(Vertex));
	const auto* program = _shaderManager->GetShader("Blob");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_TextureId));
	bgfx::setVertexBuffer(0, &buffer);
	// Blended over the land, tested against depth but leaving none, both sides
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA);
	program->Submit(static_cast<bgfx::ViewId>(desc.viewId));
}

void Renderer::DrawFarVillagerSmudges(const DrawSceneDesc& desc) const
{
	using ecs::components::Mist;
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& textures = Locator::resources::value().GetTextures();
	if (desc.viewId != RenderPass::Main || renderCtx.farVillagers.empty() || !textures.Contains(Mist::k_TextureId) ||
	    !textures.Contains(Mist::k_AlphaTextureId))
	{
		return;
	}
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	constexpr std::array<uint16_t, 6> k_Triangles = {0, 1, 2, 0, 2, 3};
	const auto smudges = static_cast<uint32_t>(renderCtx.farVillagers.size());
	const auto vertexCount = smudges * 4;
	const auto indexCount = smudges * 6;
	if (vertexCount > std::numeric_limits<uint16_t>::max() ||
	    bgfx::getAvailTransientVertexBuffer(vertexCount, layout) < vertexCount ||
	    bgfx::getAvailTransientIndexBuffer(indexCount) < indexCount)
	{
		return;
	}
	bgfx::TransientVertexBuffer vertexBuffer;
	bgfx::TransientIndexBuffer indexBuffer;
	bgfx::allocTransientVertexBuffer(&vertexBuffer, vertexCount, layout);
	bgfx::allocTransientIndexBuffer(&indexBuffer, indexCount);
	const auto vertices = std::span(reinterpret_cast<Vertex*>(vertexBuffer.data), vertexCount);
	const auto indices = std::span(reinterpret_cast<uint16_t*>(indexBuffer.data), indexCount);
	// Black, a little see-through
	constexpr auto k_Colour = static_cast<uint32_t>(ground_blobs::k_SmudgeAlpha) << 24u;
	const auto right = desc.camera->GetRight();
	const auto up = desc.camera->GetUp();
	for (size_t i = 0; i < renderCtx.farVillagers.size(); ++i)
	{
		const auto& far = renderCtx.farVillagers.at(i);
		const auto corners = ground_blobs::SmudgeCorners(far.position, far.scale, renderCtx.farSmudgeScale, right, up);
		for (size_t c = 0; c < corners.size(); ++c)
		{
			vertices[(i * 4) + c] = {corners.at(c), ground_blobs::k_SmudgeUvs.at(c), k_Colour};
		}
		for (size_t t = 0; t < k_Triangles.size(); ++t)
		{
			indices[(i * 6) + t] = static_cast<uint16_t>((i * 4) + k_Triangles.at(t));
		}
	}
	const auto* program = _shaderManager->GetShader("WorldTextured");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(Mist::k_TextureId));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(Mist::k_AlphaTextureId));
	bgfx::setVertexBuffer(0, &vertexBuffer);
	bgfx::setIndexBuffer(&indexBuffer);
	// Both sides, blended over what is behind, tested against but not writing depth; far off, among the first to blend
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA);
	program->Submit(static_cast<bgfx::ViewId>(TranslucentView(desc.viewId)),
	                zsort::Depth(renderCtx.farVillagers.front().position, desc.camera->GetOrigin()));
}

void Renderer::DrawKnockReadout(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::abodeKnockSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& knocks = Locator::abodeKnockSystem::value();
	const auto town = knocks.GetReadoutTown();
	static constexpr auto k_TextureId = entt::hashed_string("raw/misc0");
	static constexpr auto k_AlphaTextureId = entt::hashed_string("raw/misc0a");
	const auto& textures = Locator::resources::value().GetTextures();
	const auto* townData = town.has_value() ? desc.entities.TryGet<const ecs::components::Town>(*town) : nullptr;
	if (townData == nullptr || !textures.Contains(k_TextureId.value()) || !textures.Contains(k_AlphaTextureId.value()))
	{
		return;
	}
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};
	std::vector<Vertex> vertices;
	// The little person is the second picture of the bottom row of the sheet's eight by eight
	constexpr float k_Cell = 1.0f / 8.0f;
	constexpr glm::vec2 k_Person {1.0f * k_Cell, 7.0f * k_Cell};
	constexpr std::array<glm::vec2, 4> k_Corners {glm::vec2 {-1.0f, 1.0f}, glm::vec2 {1.0f, 1.0f}, glm::vec2 {1.0f, -1.0f},
	                                              glm::vec2 {-1.0f, -1.0f}};
	constexpr std::array<glm::vec2, 4> k_Uvs {glm::vec2 {0.0f, 0.0f}, glm::vec2 {k_Cell, 0.0f}, glm::vec2 {k_Cell, k_Cell},
	                                          glm::vec2 {0.0f, k_Cell}};
	const auto right = desc.camera->GetRight();
	const auto up = desc.camera->GetUp();
	const auto& infos = Locator::infoConstants::value().abode;
	const auto& meshes = Locator::resources::value().GetMeshes();
	const float scale = knocks.GetReadoutScale();
	const auto origin = desc.camera->GetOrigin();
	// Each house of the town shows its read-out when the renderer draws the house. Only the houses that this frame draws
	// show one, in the order of the draw, and each takes its own place among what blends.
	struct Row
	{
		uint32_t firstVertex;
		uint32_t vertexCount;
		uint32_t depth;
	};
	std::vector<Row> rows;
	const auto& drawn = Locator::rendereringSystem::value().GetContext().drawnObjects;
	for (const auto& object : drawn)
	{
		const auto abode = object.entity;
		const auto* data = desc.entities.TryGet<const ecs::components::Abode>(abode);
		const auto* transform = desc.entities.TryGet<const ecs::components::Transform>(abode);
		const auto* mesh = desc.entities.TryGet<const ecs::components::Mesh>(abode);
		if (data == nullptr || transform == nullptr || mesh == nullptr || !meshes.Contains(mesh->id) ||
		    std::ranges::find(townData->abodes, abode) == townData->abodes.end())
		{
			continue;
		}
		const auto adults = static_cast<uint32_t>(std::ranges::count_if(data->inhabitants, [&desc](entt::entity villager) {
			const auto* person = desc.entities.TryGet<const ecs::components::Villager>(villager);
			return person != nullptr && person->lifeStage == ecs::components::Villager::LifeStage::Adult;
		}));
		// Above the house by its model's height and a little more
		auto point = transform->position;
		point.y += meshes.Handle(mesh->id)->GetBoundingBox().Size().y + ecs::abode_knock::k_RaisedAbove;
		const auto places = infos.at(static_cast<size_t>(data->type)).maxVillagersInAbode;
		const auto first = static_cast<uint32_t>(vertices.size());
		for (const auto& marker : ecs::abode_knock::Readout(places, adults, scale))
		{
			const auto centre = point + right * marker.offset;
			// The colour as the vertices take it, a byte each of red, green, blue and alpha from the lowest
			const auto abgr =
			    (marker.colour & 0xFF00FF00u) | ((marker.colour & 0xFFu) << 16u) | ((marker.colour >> 16u) & 0xFFu);
			for (size_t c = 0; c < k_Corners.size(); ++c)
			{
				vertices.push_back({centre + (right * k_Corners.at(c).x + up * k_Corners.at(c).y) * marker.size,
				                    k_Person + k_Uvs.at(c), abgr});
			}
		}
		const auto rowCount = static_cast<uint32_t>(vertices.size()) - first;
		if (rowCount != 0)
		{
			rows.push_back({.firstVertex = first, .vertexCount = rowCount, .depth = zsort::Depth(point, origin)});
		}
	}
	const auto count = static_cast<uint32_t>(vertices.size());
	const auto quads = rows.empty() ? 0u : std::ranges::max(rows, {}, &Row::vertexCount).vertexCount / 4;
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	if (count == 0 || bgfx::getAvailTransientVertexBuffer(count, layout) < count ||
	    bgfx::getAvailTransientIndexBuffer(quads * 6) < quads * 6)
	{
		return;
	}
	bgfx::TransientVertexBuffer vertexBuffer;
	bgfx::TransientIndexBuffer indexBuffer;
	bgfx::allocTransientVertexBuffer(&vertexBuffer, count, layout);
	bgfx::allocTransientIndexBuffer(&indexBuffer, quads * 6);
	std::memcpy(vertexBuffer.data, vertices.data(), count * sizeof(Vertex));
	// The quads of a row, counted from the row's first vertex
	const auto indices = std::span(reinterpret_cast<uint16_t*>(indexBuffer.data), quads * 6);
	constexpr std::array<uint16_t, 6> k_Triangles = {0, 1, 2, 2, 3, 0};
	for (uint32_t q = 0; q < quads; ++q)
	{
		for (size_t t = 0; t < k_Triangles.size(); ++t)
		{
			indices[(q * 6) + t] = static_cast<uint16_t>((q * 4) + k_Triangles.at(t));
		}
	}
	const auto* program = _shaderManager->GetShader("WorldTextured");
	const auto viewId = static_cast<bgfx::ViewId>(TranslucentView(desc.viewId));
	for (const auto& row : rows)
	{
		program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_TextureId));
		program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_AlphaTextureId));
		bgfx::setVertexBuffer(0, &vertexBuffer, row.firstVertex, row.vertexCount);
		bgfx::setIndexBuffer(&indexBuffer, 0, row.vertexCount / 4 * 6);
		// Blended over what is behind, both sides, tested against depth but leaving none
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA);
		program->Submit(viewId, row.depth);
	}
}
