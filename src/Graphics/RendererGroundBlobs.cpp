/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS 1

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
#include "Common/WorkerPool.h"
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
#include "Graphics/DrawnFrame.h"
#include "Graphics/FizzDrawOrder.h"
#include "Graphics/GroundBlobs.h"
#include "Graphics/HandLight.h"
#include "Graphics/HandReflection.h"
#include "Graphics/Mesh.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/QuadIndices.h"
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

namespace
{
/// The texture of the ground blobs
constexpr auto k_GroundBlobTextureId = entt::hashed_string("raw/human_shadow");
/// The sheet of the creatures' footprints and its alpha
constexpr auto k_FootprintTextureId = entt::hashed_string("raw/misc0");
constexpr auto k_FootprintAlphaTextureId = entt::hashed_string("raw/misc0a");

/// The corners of a quad of the flat effects on the land
constexpr size_t k_QuadCorners = 4;

/// The layout of the corners of the flat effects on the land: a position, a place on the texture and a colour
const bgfx::VertexLayout& FlatEffectLayout()
{
	static const auto k_Layout = [] {
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

void Renderer::CollectCreatureFootprints(DrawnEffects& effects, const DrawSceneDesc& desc) const
{
	namespace prints = creature_footprints;
	auto& vertices = effects.footprints;
	vertices.clear();
	// Drawn once, after the land: the land mirrored in the sea has none
	if (desc.viewId != RenderPass::Main || !desc.drawIsland || !Locator::footprintSystem::has_value() ||
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
	using Vertex = DrawnEffects::Vertex;
	// Each print gives its four corners. The draw joins them into its two triangles with shared indices.
	vertices.reserve(footprints.GetPrints().size() * k_QuadCorners);
	for (const auto& print : footprints.GetPrints())
	{
		// Black, as opaque as the print still is
		const auto colour = static_cast<uint32_t>(print.alpha) << 24u;
		for (size_t corner = 0; corner < k_QuadCorners; ++corner)
		{
			vertices.push_back({print.corners.at(corner), print.uvs.at(corner), colour});
		}
	}
	if (warmUp)
	{
		vertices.resize(k_QuadCorners, Vertex {.position = glm::vec3(0.0f), .uv = glm::vec2(0.0f), .colour = 0});
	}
}

void Renderer::DrawCreatureFootprints(const DrawSceneDesc& desc) const
{
	const auto& vertices = desc.frame->effects.footprints;
	if (desc.viewId != RenderPass::Main || vertices.empty())
	{
		return;
	}
	// White where the prints' pictures are, and their shapes in the alpha beside it
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_FootprintTextureId.value()) || !textures.Contains(k_FootprintAlphaTextureId.value()))
	{
		return;
	}
	const auto count = static_cast<uint32_t>(vertices.size());
	const auto& layout = FlatEffectLayout();
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	if (!_footprintQuads)
	{
		_footprintQuads = std::make_unique<QuadIndices>(creature_footprints::k_Indices);
	}
	if (!_footprintQuads->Bind(count / k_QuadCorners))
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(DrawnEffects::Vertex));
	const auto* program = _shaderManager->GetShader("WorldTextured");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_FootprintTextureId));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_FootprintAlphaTextureId));
	bgfx::setVertexBuffer(0, &buffer);
	// Unlit, blended over the land, tested against depth but leaving none, both sides
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA);
	// Lying on the land, they are blended first of all that blends, straight after the land is drawn
	program->Submit(static_cast<uint16_t>(TranslucentView(desc.viewId)), std::numeric_limits<uint32_t>::max());
}

namespace
{

/// The fewest villagers for which the worker threads make the ground blobs
constexpr size_t k_LeastCastersOnThreads = 64;
/// How many runs of villagers each lane of the worker threads gets, so that one slow run does not hold the others
constexpr size_t k_CasterRunsPerLane = 4;

} // namespace

void Renderer::CollectGroundBlobs(DrawnEffects& effects, const DrawSceneDesc& desc) const
{
	auto& vertices = effects.groundBlobs;
	vertices.clear();
	// Only the main view draws the blobs, with the things on the land
	if (desc.viewId != RenderPass::Main || !desc.drawEntities || !Locator::terrainSystem::has_value() ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_GroundBlobTextureId.value()))
	{
		return;
	}
	const auto& island = Locator::terrainSystem::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	using Vertex = DrawnEffects::Vertex;
	// Each quad gives its four corners. The draw joins them into its two triangles with shared indices.
	const auto addQuadTo = [](std::vector<Vertex>& to, const ground_blobs::Quad& quad) {
		for (size_t corner = 0; corner < k_QuadCorners; ++corner)
		{
			const auto opacity = static_cast<uint32_t>(ground_blobs::k_Opacity.at(corner) * 255.0f);
			to.push_back({quad.corners.at(corner), ground_blobs::k_Uvs.at(corner), (opacity << 24u) | 0xFFFFFFu});
		}
	};
	const auto addQuad = [&vertices, &addQuadTo](const ground_blobs::Quad& quad) { addQuadTo(vertices, quad); };
	const auto& farVillagers = Locator::rendereringSystem::value().GetContext().farVillagers;
	// Room for the most quads the frame can make: two for each villager, each far villager and each foot of an animal
	const auto& animalDraws = Locator::rendereringSystem::value().GetContext().animalDraws;
	vertices.reserve(k_QuadCorners * ((2 * (desc.entities.Size<ecs::components::Villager>() + farVillagers.size())) +
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
	const auto seenBy = SceneFrustum(desc);
	const float highestLand =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().HighestHeight() : view_frustum::k_HighestLand;
	const auto seesColumn = [&seenBy, highestLand](const glm::vec3& centre, float reach, float lift) {
		return view_frustum::SeesWorldBox(seenBy, glm::vec3(centre.x - reach, -reach, centre.z - reach),
		                                  glm::vec3(centre.x + reach, highestLand + lift + reach, centre.z + reach));
	};
	// The villagers that can cast blobs, in the order of the walk. This thread does the walk and the lookups of the
	// meshes, because a lookup can load a mesh.
	struct Caster
	{
		entt::entity entity;
		ecs::components::Transform transform;
		std::span<const glm::mat4> rest;
	};
	std::vector<Caster> casters;
	casters.reserve(desc.entities.Size<ecs::components::Villager>());
	desc.entities.Each<const ecs::components::Villager, const ecs::components::Transform, const ecs::components::Mesh>(
	    [&](entt::entity entity, const ecs::components::Villager& /*villager*/, const ecs::components::Transform& placed,
	        const ecs::components::Mesh& mesh) {
		    // A villager in a creature's hand is where the frame draws the hand
		    const auto transform = drawnPlaces.Of(entity, placed);
		    if (transform.position.y <= ground_blobs::k_LowestHeight || !meshes.Contains(mesh.id) || isFar(entity))
		    {
			    return;
		    }
		    casters.push_back({.entity = entity, .transform = transform, .rest = meshes.Handle(mesh.id)->GetBoneMatrices()});
	    },
	    entt::exclude<ecs::components::AtHome, ecs::components::HiddenByState, ecs::components::HighDetail>);
	// The blobs of one villager. This reads the world and writes only into `to`, so the worker threads can do it.
	const auto castBlobs = [&](const Caster& caster, std::vector<Vertex>& to) {
		const auto& transform = caster.transform;
		// From the feet as they are drawn this frame, in the pose of its clip
		const auto* pose = desc.entities.TryGet<const ecs::components::VillagerPose>(caster.entity);
		const auto bones = ground_blobs::FootBones(
		    pose != nullptr ? std::span<const glm::mat4>(pose->bones) : std::span<const glm::mat4> {}, caster.rest);
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
		                   glm::mat4(ecs::components::DrawnRotation(transform.rotation, pose)) * glm::scale(transform.scale);
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
			addQuadTo(to, quad);
		}
	};
	// The worker threads make the blobs of runs of the villagers. The runs join in the order of the walk, so the blobs
	// blend in the same order as when one thread makes all of them.
	const size_t runs = casters.size() >= k_LeastCastersOnThreads
	                        ? std::min(casters.size(), _workers->GetLaneCount() * k_CasterRunsPerLane)
	                        : 1;
	std::vector<std::vector<Vertex>> runVertices(runs);
	const auto castRun = [&](size_t run, size_t /*lane*/) {
		const auto first = casters.size() * run / runs;
		const auto last = casters.size() * (run + 1) / runs;
		auto& to = runVertices.at(run);
		to.reserve(k_QuadCorners * 2 * (last - first));
		for (size_t index = first; index < last; ++index)
		{
			castBlobs(casters[index], to);
		}
	};
	if (runs > 1)
	{
		_workers->ForEach(runs, castRun);
	}
	else
	{
		castRun(0, 0);
	}
	for (const auto& run : runVertices)
	{
		vertices.insert(vertices.end(), run.begin(), run.end());
	}
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
}

void Renderer::DrawGroundBlobs(const DrawSceneDesc& desc) const
{
	const auto& vertices = desc.frame->effects.groundBlobs;
	if (desc.viewId != RenderPass::Main || vertices.empty())
	{
		return;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_GroundBlobTextureId.value()))
	{
		return;
	}
	const auto count = static_cast<uint32_t>(vertices.size());
	const auto& layout = FlatEffectLayout();
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	if (!_groundBlobQuads)
	{
		_groundBlobQuads = std::make_unique<QuadIndices>(ground_blobs::k_Indices);
	}
	if (!_groundBlobQuads->Bind(count / k_QuadCorners))
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(DrawnEffects::Vertex));
	const auto* program = _shaderManager->GetShader("Blob");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_GroundBlobTextureId));
	bgfx::setVertexBuffer(0, &buffer);
	// Blended over the land, tested against depth but leaving none, both sides
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA);
	program->Submit(static_cast<bgfx::ViewId>(desc.viewId));
}

void Renderer::DrawFarVillagerSmudges(const DrawSceneDesc& desc) const
{
	using ecs::components::Mist;
	if (desc.frame == nullptr || desc.frame->models == nullptr)
	{
		return;
	}
	const auto& renderCtx = *desc.frame->models;
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

void Renderer::CollectKnockReadout(DrawnEffects& effects, const DrawSceneDesc& desc) const
{
	auto& vertices = effects.knockVertices;
	auto& rows = effects.knockRows;
	vertices.clear();
	rows.clear();
	if (!Locator::abodeKnockSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& knocks = Locator::abodeKnockSystem::value();
	const auto town = knocks.GetReadoutTown();
	static constexpr auto k_TextureId = entt::hashed_string("raw/misc0");
	static constexpr auto k_AlphaTextureId = entt::hashed_string("raw/misc0a");
	const auto& textures = Locator::resources::value().GetTextures();
	const auto* townData = town.has_value() ? desc.entities.TryGet<const ecs::components::Town>(*town) : nullptr;
	bool residentInHand = false;
	desc.entities.Each<const ecs::components::Abode>(
	    [&residentInHand](entt::entity, const ecs::components::Abode& abode) { residentInHand |= abode.residentInHand; });
	if ((townData == nullptr && !residentInHand) || !textures.Contains(k_TextureId.value()) ||
	    !textures.Contains(k_AlphaTextureId.value()))
	{
		return;
	}
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
	// show one, in the order of the draw, and each takes its own place among what blends. A house with one of its
	// people in a hand shows its teal marker in place of the read-out, in every town.
	const auto& drawn = Locator::rendereringSystem::value().GetContext().drawnObjects;
	for (const auto& object : drawn)
	{
		const auto abode = object.entity;
		const auto* data = desc.entities.TryGet<const ecs::components::Abode>(abode);
		const auto* transform = desc.entities.TryGet<const ecs::components::Transform>(abode);
		const auto* mesh = desc.entities.TryGet<const ecs::components::Mesh>(abode);
		if (data == nullptr || transform == nullptr || mesh == nullptr || !meshes.Contains(mesh->id))
		{
			continue;
		}
		const bool readout = townData != nullptr && std::ranges::find(townData->abodes, abode) != townData->abodes.end();
		if (!data->residentInHand && !readout)
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
		const auto markers = data->residentInHand ? std::vector {ecs::abode_knock::HeldResidentMarker()}
		                                          : ecs::abode_knock::Readout(places, adults, scale);
		for (const auto& marker : markers)
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
}

void Renderer::DrawKnockReadout(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main)
	{
		return;
	}
	using Vertex = DrawnEffects::Vertex;
	const auto& vertices = desc.frame->effects.knockVertices;
	const auto& rows = desc.frame->effects.knockRows;
	static constexpr auto k_TextureId = entt::hashed_string("raw/misc0");
	static constexpr auto k_AlphaTextureId = entt::hashed_string("raw/misc0a");
	const auto& textures = Locator::resources::value().GetTextures();
	const auto count = static_cast<uint32_t>(vertices.size());
	const auto quads = rows.empty() ? 0u : std::ranges::max(rows, {}, &DrawnEffects::KnockRow::vertexCount).vertexCount / 4;
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
