/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The fish puzzle's nets (FishPlot): one static object of Data\MISC\Fishplot.l3d moved to each of the 7 floats and
// drawn cut by the plane: placed at the float (angle 0, scale 1.0), then DrawCutByPlane. Under the water (from the
// shoals' draw, before the sea) between the clip planes (0, -1, 0, 0) and (0, 1, 0, 0); over the water (later in
// the frame) with the default plane.

#include <limits>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include "3D/L3DMesh.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/FishPuzzle.h"
#include "ECS/Registry.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/InstanceDesc.h"
#include "Graphics/RenderModes.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
/// The object's colour that DrawCutByPlane lights per vertex: a meshed object's default white. The net never sets its
/// colour; turning on its dynamic lighting only sets a flag; and the only other writers of the colour are draws the
/// net never goes through (it is only placed and drawn cut by the plane).
constexpr uint32_t k_NetColour = 0xFFFFFFFFu;
/// Its specular, the same default 0, with the same writers
constexpr uint32_t k_NetSpecular = 0u;
} // namespace

void Renderer::DrawFishPlots(RenderPass viewId, sea_pass::SeaPlane plane) const
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	// the instance of RenderingSystemCommon::ResizeInstances: the model matrix and the fifth column of
	// argb_colour::PackInstance* (all 0: no tint, colour, specular or window; the cut takes k_NetColour)
	struct NetInstance
	{
		glm::mat4 model;
		glm::vec4 lh3d {0.0f};
	};
	static_assert(sizeof(NetInstance) == sizeof(glm::mat4) + sizeof(glm::vec4));
	std::vector<NetInstance> instances;
	entt::id_type meshId = 0;
	Locator::entitiesRegistry::value().Each<const ecs::components::FishBait>(
	    [&instances, &meshId, &meshes](const ecs::components::FishBait& bait) {
		    if (bait.net.mesh == 0 || !meshes.Contains(bait.net.mesh))
		    {
			    return;
		    }
		    meshId = bait.net.mesh;
		    // SetPosition(float, angle 0, scale 1.0)
		    for (const auto& point : ecs::FishPlotFloats(bait.net))
		    {
			    instances.push_back({glm::translate(glm::mat4(1.0f), point)});
		    }
	    },
	    entt::exclude<ecs::components::Unavailable>);
	if (instances.empty())
	{
		return;
	}
	const auto count = static_cast<uint32_t>(instances.size());
	if (!_fishPlotInstances.IsValid() || _fishPlotCapacity < count)
	{
		_fishPlotInstances.Reset(); // the old one before the new one is made
		// the layout of RenderingSystem's instances: the 4 columns of the model matrix (i_data0..3) and the colours
		// (i_data4), which vs_object reads
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
		    .end();
		_fishPlotCapacity = count + 14;
		_fishPlotInstances.Reset(bgfx::createDynamicVertexBuffer(_fishPlotCapacity, layout));
	}
	// both passes of the frame write the same floats (the mirroring is the shader's)
	bgfx::update(_fishPlotInstances.Get(), 0,
	             bgfx::copy(instances.data(), static_cast<uint32_t>(instances.size() * sizeof(NetInstance))));

	const auto mesh = meshes.Handle(meshId);
	L3DMeshSubmitDesc submitDesc = {};
	submitDesc.viewId = viewId;
	submitDesc.options = render_modes::k_ModelPass;
	// DrawCutByPlane; the part under the water goes into the reflection target, which shows through the sea (see
	// DrawPass), mirrored back there (sea_pass::Cut)
	submitDesc.sea = sea_pass::Cut(plane, k_NetColour, k_NetSpecular, viewId);
	submitDesc.instanceDesc = std::make_unique<graphics::InstanceDesc>(fromBgfx(_fishPlotInstances.Get()), 0, count);
	static const auto k_Identity = glm::mat4(1.0f);
	submitDesc.modelMatrices = &k_Identity;
	submitDesc.matrixCount = 1;
	submitDesc.program = _shaderManager->GetShader("ObjectInstanced");
	DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
}
