/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <span>
#include <utility>

#include <L3DFile.h>

#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/Reward.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "GameProviderShared.h"
#include "Help/AdvisorModel.h"
#include "Help/AdvisorVoices.h"
#include "Help/DialogueText.h"
#include "Help/Spirits.h"
#include "Locator.h"
#include "Nature/Fireflies.h"
#include "Online/NetworkSession.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

// The advisors

namespace
{

/// How an advisor's mesh, posed with its bones this frame, lies before the camera: its depths along the view, the
/// vertices the projection clips, and its triangles by the way they wind on the screen
struct AdvisorDepths
{
	float nearestVertex {0.0f};
	float furthestVertex {0.0f};
	size_t vertices {0};
	size_t closerThanNear {0};
	size_t clippedNear {0};
	size_t clippedFar {0};
	size_t clockwise {0};
	size_t counterClockwise {0};
	size_t mirroredBones {0};
};

std::optional<AdvisorDepths> MeasureAdvisorDepths(const help::spirits::AdvisorModel& model, std::span<const glm::mat4> bones,
                                                  const Camera& camera)
{
	l3d::L3DFile file;
	if (bones.empty() || file.Open(model.file.mesh) != l3d::L3DResult::Success)
	{
		return std::nullopt;
	}
	const auto view = camera.GetViewMatrix(Camera::Interpolation::Current);
	const auto viewProjection = camera.GetProjectionMatrix(Camera::Projection::ReversedZ) * view;
	AdvisorDepths depths {.nearestVertex = std::numeric_limits<float>::max(),
	                      .furthestVertex = std::numeric_limits<float>::lowest()};
	for (const auto& bone : bones)
	{
		depths.mirroredBones += glm::determinant(glm::mat3(bone)) < 0.0f ? 1 : 0;
	}
	for (uint32_t submesh = 0; submesh < file.GetSubmeshHeaders().size(); ++submesh)
	{
		const auto& header = file.GetSubmeshHeaders()[submesh];
		// The drawn level of detail only, as the renderer picks it
		if (header.flags.isPhysics || header.flags.status != 0 || (header.flags.lodMask & 1) != 1)
		{
			continue;
		}
		const auto vertices = file.GetVertexSpan(submesh);
		std::vector<glm::vec4> clip(vertices.size(), glm::vec4(0.0f));
		size_t vertex = 0;
		for (const auto& group : file.GetVertexGroupSpan(submesh))
		{
			const auto& bone = bones[std::min<size_t>(group.boneIndex, bones.size() - 1)];
			for (uint16_t i = 0; i < group.vertexCount && vertex < vertices.size(); ++i, ++vertex)
			{
				const auto& position = vertices[vertex].position;
				const glm::vec4 world = bone * glm::vec4(position.x, position.y, position.z, 1.0f);
				// The view is left handed: it looks down its positive z
				const float depth = (view * world).z;
				depths.nearestVertex = std::min(depths.nearestVertex, depth);
				depths.furthestVertex = std::max(depths.furthestVertex, depth);
				depths.closerThanNear += depth < camera.GetNearClip() ? 1 : 0;
				clip[vertex] = viewProjection * world;
				// Reversed, depth runs from w at the near plane to 0 at the far plane; the renderer clips outside it
				depths.clippedNear += clip[vertex].z > clip[vertex].w ? 1 : 0;
				depths.clippedFar += clip[vertex].z < 0.0f ? 1 : 0;
				++depths.vertices;
			}
		}
		const auto indices = file.GetIndexSpan(submesh);
		uint32_t firstVertex = 0;
		uint32_t firstIndex = 0;
		for (const auto& primitive : file.GetPrimitiveSpan(submesh))
		{
			for (uint32_t triangle = 0; triangle < primitive.numTriangles; ++triangle)
			{
				std::array<glm::vec2, 3> corner {};
				bool inside = true;
				for (uint32_t k = 0; k < 3; ++k)
				{
					const uint32_t index = firstIndex + 3 * triangle + k;
					const size_t at = index < indices.size() ? size_t {indices[index]} + firstVertex : clip.size();
					if (at >= clip.size() || clip[at].w <= 0.0f)
					{
						inside = false;
						break;
					}
					corner.at(k) = glm::vec2(clip[at]) / clip[at].w;
				}
				if (!inside)
				{
					continue;
				}
				const glm::vec2 a = corner[1] - corner[0];
				const glm::vec2 b = corner[2] - corner[0];
				const float area = a.x * b.y - a.y * b.x;
				depths.counterClockwise += area > 0.0f ? 1 : 0;
				depths.clockwise += area < 0.0f ? 1 : 0;
			}
			firstVertex += primitive.numVertices;
			firstIndex += primitive.numTriangles * 3;
		}
	}
	return depths.vertices > 0 ? std::optional(depths) : std::nullopt;
}

/// Where an advisor hovers and clings on the screen, and where its bones are drawn there, even off the screen
void AddAdvisorPlacement(Json& item, const ecs::systems::AdvisorSystemInterface& advisors,
                         const ecs::systems::AdvisorSystemInterface::Draw& draw, const Camera* camera)
{
	const auto& controller = advisors.GetController();
	const auto& dude = controller.Dude(draw.advisor);
	const auto& screen = controller.GetScreen();
	const auto unit = static_cast<float>(screen.HoverUnit());
	const auto hover = dude.Hover();
	item["state"] = dude.State();
	item["hover"] = {hover.x, hover.y};
	// Both of the hover's axes are measured in hover units from the screen's middle
	item["hover_pixel"] = {hover.x * unit + static_cast<float>(screen.HalfWidth()),
	                       hover.y * unit + static_cast<float>(screen.HalfHeight())};
	item["screen"] = {
	    {"width", screen.width}, {"height", screen.height}, {"hover_unit", screen.HoverUnit()}, {"reach", screen.Reach()}};
	item["cling_edge"] = static_cast<int>(dude.ClingEdge());
	item["cling_target"] = {dude.ClingTarget().x, dude.ClingTarget().y};
	if (camera == nullptr || draw.bones.empty())
	{
		return;
	}
	const auto viewProjection = camera->GetViewProjectionMatrix();
	const auto toPixel = [&viewProjection, &screen](const glm::mat4& bone) -> std::optional<glm::vec2> {
		const auto clip = viewProjection * bone[3];
		if (clip.w <= 0.0f)
		{
			return std::nullopt;
		}
		const auto ndc = glm::vec2(clip) / clip.w;
		return glm::vec2((ndc.x + 1.0f) * 0.5f * static_cast<float>(screen.width),
		                 (1.0f - ndc.y) * 0.5f * static_cast<float>(screen.height));
	};
	if (const auto root = toPixel(draw.bones.front()))
	{
		item["root_pixel"] = {root->x, root->y};
	}
	// How far up and down the screen its bones reach
	float top = std::numeric_limits<float>::max();
	float bottom = std::numeric_limits<float>::lowest();
	for (const auto& bone : draw.bones)
	{
		if (const auto pixel = toPixel(bone))
		{
			top = std::min(top, pixel->y);
			bottom = std::max(bottom, pixel->y);
		}
	}
	if (top <= bottom)
	{
		item["bones_pixel_y"] = {top, bottom};
	}
}

} // namespace

namespace openblack::inspector::game_providers
{

std::unique_ptr<ProviderInterface> AdvisorsProvider()
{
	auto provider = std::make_unique<FunctionProvider>("advisors");
	provider->Add(
	    Query("draws",
	          "What is drawn of the advisors this frame: near the screen or in the world, alpha, how far out in "
	          "the world, where they hover and cling on the screen (and the screen they move on: its size, the pixels in "
	          "a hover unit and how many hover units out its sides lie) and where their bones are drawn, and how "
	          "close their posed meshes come to the camera beside its near plane",
	          {}, ResultKind::List),
	    Serve<Locator::advisorSystem>("the advisors", [](const ecs::systems::AdvisorSystemInterface& advisors,
	                                                     const QueryContext& /*c*/) {
		    const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
		    Json items = Json::array();
		    for (const auto& draw : advisors.GetDraws())
		    {
			    Json item = {{"advisor", draw.advisor},  {"near_screen", draw.nearScreen}, {"alpha", draw.alpha},
			                 {"in_world", draw.inWorld}, {"bones", draw.bones.size()},     {"sprites", draw.sprites.size()}};
			    if (!draw.bones.empty())
			    {
				    item["root"] = Point(glm::vec3(draw.bones.front()[3]));
			    }
			    AddAdvisorPlacement(item, advisors, draw, camera);
			    const auto* model = advisors.GetModel(draw.advisor);
			    if (camera != nullptr && model != nullptr)
			    {
				    item["near_clip"] = camera->GetNearClip();
				    if (const auto depths = MeasureAdvisorDepths(*model, draw.bones, *camera))
				    {
					    item["nearest_vertex_depth"] = depths->nearestVertex;
					    item["furthest_vertex_depth"] = depths->furthestVertex;
					    item["vertices"] = depths->vertices;
					    item["vertices_closer_than_near"] = depths->closerThanNear;
					    item["vertices_clipped_near"] = depths->clippedNear;
					    item["vertices_clipped_far"] = depths->clippedFar;
					    item["triangles_clockwise"] = depths->clockwise;
					    item["triangles_counter_clockwise"] = depths->counterClockwise;
					    item["mirrored_bones"] = depths->mirroredBones;
				    }
			    }
			    items.push_back(std::move(item));
		    }
		    return items;
	    }));
	return provider;
}

} // namespace openblack::inspector::game_providers
