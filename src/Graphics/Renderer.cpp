/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>
#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <bit>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include <SDL_video.h>
#include <bgfx/platform.h>
#include <bimg/bimg.h>
#include <bx/file.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/AffineMatrix.h"
#include "3D/Billboard.h"
#include "3D/Clouds.h"
#include "3D/DayNightClock.h"
#include "3D/FrameAnim.h"
#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/LandMorph.h"
#include "3D/NightLights.h"
#include "3D/ObjectMatrix.h"
#include "3D/OceanInterface.h"
#include "3D/SkyInterface.h"
#include "3D/SkyType.h"
#include "3D/SkyWeather.h"
#include "Audio/Services/Guidance.h"
#include "Audio/Services/SpookyVoices.h"
#include "Camera/Camera.h"
#include "Common/TruncateToInt.h"
#include "ECS/Animations.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreaturePose.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Physics/Buildings.h"
#include "ECS/Physics/Dust.h"
#include "ECS/Physics/FragMesh.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/RenderFrameSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/WaterRings.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/DebugLines.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GameFont.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/InputPromptFrame.h"
#include "Graphics/ModelLight.h"
#include "Graphics/OverlayFrame.h"
#include "Graphics/Primitive.h"
#include "Graphics/RegionOnScreen.h"
#include "Graphics/RenderModes.h"
#include "Graphics/SeaPass.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShadowList.h"
#include "Graphics/SuperVillagerFrame.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/WorldTriangles.h" // the exploded pieces
#include "Graphics/ZSort.h"
#include "Help/HelpTextDisplay.h"
#include "Locator.h"
#include "Particles/Creators/Mist.h"
#include "Particles/Rules/ExplodeObject.h" // the exploded pieces
#include "Profiler.h"
#include "Renderer.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Video/FallingSpellVideo.h"
#include "Video/VideoPlayer.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::ecs::systems;

namespace openblack
{
// clang-format off
constexpr auto k_BgfxDefaultStateInvertedZ = 0 \
                                     | BGFX_STATE_WRITE_RGB \
                                     | BGFX_STATE_WRITE_A \
                                     | BGFX_STATE_WRITE_Z \
                                     | BGFX_STATE_CULL_CW \
                                     | BGFX_STATE_DEPTH_TEST_GREATER \
                                     | BGFX_STATE_MSAA;
// clang-format on

struct BgfxCallback: public bgfx::CallbackI
{
	constexpr static std::array<std::string_view, bgfx::Fatal::Count> k_CodeLookup = {
	    "DebugCheck",            //
	    "InvalidShader",         //
	    "UnableToInitialize",    //
	    "UnableToCreateTexture", //
	    "DeviceLost",            //
	};

	~BgfxCallback() override = default;

	void fatal(const char* filePath, uint16_t line, bgfx::Fatal::Enum code, const char* str) override
	{
		const auto* codeStr = k_CodeLookup.at(code).data();
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "bgfx: {}:{}: FATAL ({}): {}", filePath, line, codeStr, str);

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_CRITICAL
		spdlog::get("graphics")
		    ->log(spdlog::source_loc {filePath, line, SPDLOG_FUNCTION}, spdlog::level::critical, "FATAL ({}): {}", codeStr,
		          str);
#endif

		// Must terminate, continuing will cause crash anyway.
		throw std::runtime_error(std::string("bgfx: ") + filePath + ":" + std::to_string(line) + ": FATAL (" + codeStr +
		                         "): " + str);
	}

	void traceVargs([[maybe_unused]] const char* filePath, [[maybe_unused]] uint16_t line, const char* format,
	                va_list argList) override
	{
		std::array<char, 0x2000> temp;
		char* out = temp.data();
		int32_t len = vsnprintf(out, temp.size(), format, argList);
		if (static_cast<int32_t>(temp.size()) < len)
		{
			out = reinterpret_cast<char*>(alloca(len + 1));
			len = vsnprintf(out, len, format, argList);
		}
		if (len > 0)
		{
			out[len] = '\0';
			if (len > 0 && out[len - 1] == '\n')
			{
				out[len - 1] = '\0';
			}
// TODO(bwrsandman): change level to trace
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
			spdlog::get("graphics")->log(spdlog::source_loc {filePath, line, SPDLOG_FUNCTION}, spdlog::level::debug, out);
#endif
		}
		else
		{
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_ERROR
			spdlog::get("graphics")
			    ->log(spdlog::source_loc {filePath, line, SPDLOG_FUNCTION}, spdlog::level::err,
			          "bgfx: failed to format message: {}", format);
#endif
		}
	}
	void profilerBegin([[maybe_unused]] const char* name, [[maybe_unused]] uint32_t abgr, [[maybe_unused]] const char* filePath,
	                   [[maybe_unused]] uint16_t line) override
	{
	}
	void profilerBeginLiteral([[maybe_unused]] const char* name, [[maybe_unused]] uint32_t abgr,
	                          [[maybe_unused]] const char* filePath, [[maybe_unused]] uint16_t line) override
	{
	}
	void profilerEnd() override {}
	// Reading and writing to shader cache
	uint32_t cacheReadSize([[maybe_unused]] uint64_t id) override { return 0; }
	bool cacheRead([[maybe_unused]] uint64_t id, [[maybe_unused]] void* data, [[maybe_unused]] uint32_t size) override
	{
		return false;
	}
	void cacheWrite([[maybe_unused]] uint64_t id, [[maybe_unused]] const void* data, [[maybe_unused]] uint32_t size) override {}
	// Saving a screenshot
	void screenShot(const char* filePath, uint32_t width, uint32_t height, uint32_t pitch, const void* data,
	                [[maybe_unused]] uint32_t size, bool yflip) override
	{
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Taking a screenshot...");

		const auto ext = std::filesystem::path(filePath).extension();
		if (std::filesystem::path(filePath).extension() == ".png")
		{
			bx::FileWriter writer;
			bx::Error err;
			if (bx::open(&writer, filePath, false, &err))
			{
				// Strip out alpha for screenshot
				std::vector<uint32_t> noAlpha;
				noAlpha.resize(size / sizeof(noAlpha[0]), 0);
				memcpy(noAlpha.data(), data, size);
				for (uint32_t y = 0; y < height; ++y)
				{
					for (uint32_t x = 0; x < width; ++x)
					{
						noAlpha[x + pitch / sizeof(noAlpha[0]) * y] |= 0xFF000000;
					}
				}

				bimg::imageWritePng(&writer, width, height, pitch, noAlpha.data(), bimg::TextureFormat::BGRA8, yflip, &err);
				bx::close(&writer);
				SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Screenshot ({}x{}) saved at {}", width, height, filePath);
			}
			else
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Failed to save Screenshot ({}x{}) at {}: {}", width, height,
				                    filePath, std::string(err.getMessage().getCPtr(), err.getMessage().getLength()));
			}
		}
		else
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Not Implemented: {} screenshot ({}x{}) requested at {}", ext.string(),
			                   width, height, filePath);
		}
	}
	// Saving a video
	void captureBegin(uint32_t width, uint32_t height, [[maybe_unused]] uint32_t pitch,
	                  [[maybe_unused]] bgfx::TextureFormat::Enum format, [[maybe_unused]] bool yflip) override
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Not Implemented: Video Capture Begin ({}x{}) requested", width, height);
	}
	void captureEnd() override { SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Not Implemented: Video Capture End requested"); }
	void captureFrame([[maybe_unused]] const void* data, [[maybe_unused]] uint32_t size) override
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Not Implemented: Video Capture Frame requested");
	}
};

} // namespace openblack

std::unique_ptr<RendererInterface> RendererInterface::Create(GraphicsBackend backend, bool vsync) noexcept
{
	bgfx::Init init {};
	switch (backend)
	{
	case GraphicsBackend::Noop:
		init.type = bgfx::RendererType::Noop;
		break;
	case GraphicsBackend::Direct3D12:
		init.type = bgfx::RendererType::Direct3D12;
		break;
	case GraphicsBackend::Metal:
		init.type = bgfx::RendererType::Metal;
		break;
	case GraphicsBackend::Vulkan:
		init.type = bgfx::RendererType::Vulkan;
		break;
	default:
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Got impossible graphics backend.");
		return nullptr;
	}

	// Get render area size
	glm::uvec2 drawableSize;
	if (backend != GraphicsBackend::Noop)
	{
		const auto& window = Locator::windowing::value();

		drawableSize = static_cast<glm::uvec2>(window.GetSize());
		init.resolution.width = static_cast<uint32_t>(drawableSize.x);
		init.resolution.height = static_cast<uint32_t>(drawableSize.y);

		// Get Native Handles from SDL window
		const auto handles = window.GetNativeHandles();
		init.platformData.nwh = handles.nativeWindow;
		init.platformData.ndt = handles.nativeDisplay;
	}

	uint32_t bgfxReset = BGFX_RESET_NONE;
	auto bgfxCallback = std::make_unique<BgfxCallback>();
	if (vsync)
	{
		bgfxReset |= BGFX_RESET_VSYNC;
	}
	init.resolution.reset = bgfxReset;
	init.callback = dynamic_cast<bgfx::CallbackI*>(bgfxCallback.get());
	// (openblack guard) the exploded pieces' triangles go up in the
	// frame's transient vertex buffer (world_triangles), a tree is about 0.5 MB of them; bgfx's default is 6 MB for all
	init.limits.transientVbSize = 32 << 20;

	if (!bgfx::init(init))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Failed to initialize bgfx.");
		return nullptr;
	}

	const bgfx::Caps* caps = bgfx::getCaps();
	if ((caps->supported & BGFX_CAPS_TEXTURE_2D_ARRAY) == 0 || caps->limits.maxTextureLayers < 9)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Graphics device must support texture layers.");
		return nullptr;
	}

	return std::make_unique<Renderer>(bgfxReset, std::move(bgfxCallback));
}

Renderer::Renderer(uint32_t bgfxReset, std::unique_ptr<BgfxCallback>&& bgfxCallback) noexcept
    : _shaderManager(std::make_unique<ShaderManager>())
    , _shadows(std::make_unique<shadow_list::List>())
    , _bgfxCallback(std::move(bgfxCallback))
    , _bgfxReset(bgfxReset)
{
	_shaderManager->LoadShaders();
	_plane = Primitive::CreatePlane();

	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::Main), bgfx::ViewMode::Sequential);
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::MainBlended), bgfx::ViewMode::Sequential);
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::FinishFrame3D), bgfx::ViewMode::Sequential);
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::FinishFrameIcons), bgfx::ViewMode::Sequential);
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay), bgfx::ViewMode::Sequential);
	// what is under the sea is painted in order too: the sky, the moon's
	// reflection, the mirrored land, the parts under the water, the hand glow
	bgfx::setViewMode(static_cast<bgfx::ViewId>(graphics::RenderPass::Reflection), bgfx::ViewMode::Sequential);

	// give debug names to views
	// TODO (#749) use std::views::enumerate
	for (bgfx::ViewId i = 0; const auto& name : k_RenderPassNames)
	{
		bgfx::setViewName(i, name.data());
		++i;
	}
}

Renderer::~Renderer() noexcept
{
	_clouds.reset();
	for (auto& font : _fonts)
	{
		font.reset(); // its texture before bgfx::shutdown
	}
	_shadows.reset(); // its textures before bgfx::shutdown
	// the handles before bgfx::shutdown too: the members themselves only go after it
	_landLightTexture.Reset();
	_landCellsTexture.Reset();
	_videoTexture.Reset();
	_videoAlphaTexture.Reset();
	_fishPlotInstances.Reset();
	_spiritInstances.Reset();
	_templeMapFrameBuffer.reset();
	// made on the first draw that needs it, such as the temple's: its texture before bgfx::shutdown as well
	_revolvedSurfaceWhite.reset();
	// the creatures' painted skins and the morph streams' layouts, before bgfx::shutdown too
	_creatureSkins.Clear();
	for (auto& layout : _morphStreamLayouts)
	{
		layout.Reset();
	}
	_plane.reset();
	_shaderManager.reset();
	bgfx::frame();
	bgfx::shutdown();
}

void Renderer::ConfigureView(graphics::RenderPass viewId, glm::u16vec2 resolution, uint32_t clearColor) const noexcept
{
	bgfx::setViewClear(static_cast<bgfx::ViewId>(viewId), BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor, 0.0f, 0);
	bgfx::setViewRect(static_cast<bgfx::ViewId>(viewId), 0, 0, resolution.x, resolution.y);
	if (viewId == graphics::RenderPass::Main)
	{
		const auto blended = static_cast<bgfx::ViewId>(graphics::RenderPass::MainBlended);
		bgfx::setViewClear(blended, BGFX_CLEAR_NONE);
		bgfx::setViewRect(blended, 0, 0, resolution.x, resolution.y);
		// the end of the frame: the Z reset quad (pre-transformed, z = 1 over the whole screen, ZFUNC ALWAYS)
		// before the "after" callbacks: their 3D view starts with the depth at the far value of the main clear
		const auto finishFrame3D = static_cast<bgfx::ViewId>(graphics::RenderPass::FinishFrame3D);
		bgfx::setViewClear(finishFrame3D, BGFX_CLEAR_DEPTH, 0, 0.0f, 0);
		bgfx::setViewRect(finishFrame3D, 0, 0, resolution.x, resolution.y);
		const auto overlay = static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay);
		bgfx::setViewClear(overlay, BGFX_CLEAR_NONE);
		bgfx::setViewRect(overlay, 0, 0, resolution.x, resolution.y);
		// the input prompt icons' pass 0, set up as ScreenOverlay (no clear, the whole screen; identity transforms at submit)
		const auto icons = static_cast<bgfx::ViewId>(graphics::RenderPass::FinishFrameIcons);
		bgfx::setViewClear(icons, BGFX_CLEAR_NONE);
		bgfx::setViewRect(icons, 0, 0, resolution.x, resolution.y);
		_resolution = resolution;
	}
}

void Renderer::Reset(glm::u16vec2 resolution) const noexcept
{
	bgfx::reset(resolution.x, resolution.y, _bgfxReset);
}

graphics::ShaderManager& Renderer::GetShaderManager() const noexcept
{
	return *_shaderManager;
}

Renderer::MeshDrawValues Renderer::GetMeshDrawValues() const
{
	const auto& island = Locator::terrainSystem::value();
	const auto extent = island.GetExtent();
	const auto& cellMap = island.GetCellMap();
	const auto cellMapSize = glm::vec2(cellMap.GetResolution());
	return {
	    .heightMap = island.GetHeightMap(),
	    .cellMap = cellMap,
	    .islandExtent = glm::vec4(extent.minimum, extent.maximum),
	    .cellMapInfo = {extent.minimum, cellMapSize},
	    .modelLight = model_light::Uniform(),
	    .lit = IsLandLit(),
	    .frameCells = _landCellsTexture.IsValid() && glm::vec2(_landCellsSize) == cellMapSize,
	};
}

void Renderer::DrawSubMesh(const graphics::L3DMesh& mesh, const graphics::L3DSubMesh& subMesh, const L3DMeshSubmitDesc& desc,
                           bool preserveState, std::optional<MeshDrawValues>& values,
                           std::optional<uint32_t>& transformCache) const
{
	assert(&subMesh.GetMesh());
	// meshes without bones use the variant of the program with a single model matrix, meshes with up to 32 bones the one
	// with 32 (see vs_object.sc)
	const auto* program = !mesh.IsBoned()                       ? StaticVariant(desc.program)
	                      : mesh.GetBoneMatrices().size() <= 32 ? BonesVariant32(desc.program)
	                                                            : desc.program;
	// We don't draw physics meshes, we haven't implemented statuses (building and graves) and modern GPUs can handle high lod.
	// Window submeshes have no LOD bits: the original draws them only at night, without the LOD test (the abode's
	// draw); vs_object hides them on the instances whose windows are not lit.
	const bool window = subMesh.GetFlags().isWindow && desc.instanceDesc != nullptr;
	if (!desc.drawAll &&
	    (subMesh.IsPhysics() || subMesh.GetFlags().status != 0 || ((subMesh.GetFlags().lodMask & 1) != 1 && !window)))
	{
		return;
	}
	// A FragMesh's triangles: the main view gets them lit and drawn on the CPU (FragMesh::AppendDraw, in DrawPass); its
	// sub-meshes stay for the other views. Also out of the projected shadows over the object (DrawShadowsOnObject, the
	// same view): the fragments are not drawn as an object, so the objects' shadow loop never runs over them (inferred:
	// that FragMesh receives none)
	if (subMesh.IsCpuDrawn() && (desc.viewId == RenderPass::Main || desc.viewId == RenderPass::MainBlended))
	{
		return;
	}

	if (!values.has_value())
	{
		values.emplace(GetMeshDrawValues());
	}
	const auto& drawValues = *values;

	bool lastPreserveState = false;
	// the pass of the opaque models: the normal table and every primitive in its own mode
	const bool modelPass = desc.table == render_modes::Table::Normal && !desc.mode.has_value();
	const auto& primitives = subMesh.GetPrimitives();
	for (auto it = primitives.begin(); it != primitives.end(); ++it)
	{
		const auto& prim = *it;

		const bool hasNext = std::next(it) != primitives.end();

		// a creature's painted skins in place of its mesh's (RendererCreature.cpp); the mesh's own for everything else
		const Texture2D* texture = SkinTexture(mesh, prim.skinID, desc.paintedSkins);
		const Texture2D* nextTexture = !hasNext ? nullptr : SkinTexture(mesh, std::next(it)->skinID, desc.paintedSkins);

		// Material blending of the original (L3D material type): AlphaTextured & co. blend with the texture alpha, e.g.
		// the fading wrist of the hand and the soft edges of buildings. Chroma materials stay alpha tested.
		const bool blended = prim.blend != L3DSubMesh::Primitive::BlendMode::Disabled && !prim.thresholdAlpha;
		// Setting the material: the mode of the primitive's material through the current table, or the mode
		// every primitive is drawn in
		const auto drawn =
		    render_modes::Select(desc.mode.value_or(static_cast<render_modes::Mode>(prim.materialType)), desc.table);
		const auto sameMaterial = [&prim](const L3DSubMesh::Primitive& other) {
			return other.materialType == prim.materialType && other.blend == prim.blend &&
			       other.thresholdAlpha == prim.thresholdAlpha && other.depthWrite == prim.depthWrite &&
			       other.alphaCutoutThreshold == prim.alphaCutoutThreshold && other.wrap == prim.wrap &&
			       other.uvOffset == prim.uvOffset;
		};
		const bool primitivePreserveState =
		    texture != nullptr && texture == nextTexture && sameMaterial(*std::next(it)) && (preserveState || hasNext);
		// the main pass draws the opaque primitives; the blended ones go through the back-to-front list (Z-sorter)
		if ((desc.blendFilter == 1 && blended) || (desc.blendFilter == 2 && !blended))
		{
			continue;
		}

		uint32_t skip = Mesh::k_SkipNone;
		if (!lastPreserveState)
		{
			if (desc.modelMatrices != nullptr && desc.matrixCount > 0)
			{
				// the matrices are copied into the frame's matrix cache once per mesh draw; every later primitive
				// reuses that copy, which holds the same matrices
				if (transformCache.has_value())
				{
					bgfx::setTransform(*transformCache, desc.matrixCount);
				}
				else
				{
					transformCache = bgfx::setTransform(desc.modelMatrices, desc.matrixCount);
				}
			}
			if (texture != nullptr)
			{
				// Materials without the tiling bit are clamped (the original's tiling flag is only set for particle
				// meshes)
				const uint32_t samplerFlags =
				    prim.wrap ? UINT32_MAX
				              : (texture->GetSamplerFlags() & ~(BGFX_SAMPLER_U_MASK | BGFX_SAMPLER_V_MASK)) |
				                    BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
				program->SetTextureSampler("s_diffuse", 0, *texture, samplerFlags);
			}
			if (desc.morphWithTerrain)
			{
				program->SetTextureSampler("s_heightmap", 1, drawValues.heightMap);   // vs
				program->SetUniformValue("u_islandExtent", &drawValues.islandExtent); // vs
			}
			if (desc.isSky)
			{
				// vs_object reads u_objectClip in both its branches (sea_plane.sh) and bgfx keeps the last value of a
				// uniform: the sky is never a sea draw, so it must not inherit the last net's or shark's unmirror
				program->SetUniformValue("u_objectClip", &sea_pass::k_NoClip); // vs
			}
			if (!desc.isSky)
			{
				const bool lit = drawValues.lit;
				// x, by desc.sea.light (sea_pass::SeaLight): 0 white, 1 lit like the original (Normal), 2 B's constant
				// colour (Constant: z = the object colour's r 65536 + g 256 + b), 3 the land colour only (LastDraw), 4 C,
				// DrawCutByPlane (Cut: y = the colour's alpha, z = its rgb, w = the object specular's rgb)
				// y: the colour boost, w: 1 = no haze + 2 x the land light mode (land_light::ObjectMode), modes 1 and 3
				constexpr uint32_t k_Rgb = 0x00FFFFFFu;
				glm::vec4 u_objectLight = {lit ? 1.0f : 0.0f, desc.lightBoost, -1.0f,
				                           (desc.noHaze ? 1.0f : 0.0f) + 2.0f * static_cast<float>(desc.landLightMode)};
				switch (desc.sea.light)
				{
				case sea_pass::SeaLight::Normal:
					break;
				case sea_pass::SeaLight::Constant:
					// the object's colour and specular, both read per vertex. The colour's alpha (the hand's 0x65) only
					// reaches what the stage's alpha takes from the diffuse: no alternate mode table here (the
					// under-water draw only uses it for objects with the global alpha flag; (inferred) the hand's object
					// never gets it, see Renderer::DrawUnderWater) and the hand's AlphaTextured takes the texture's alpha, so
					// it is left out
					u_objectLight = {2.0f, desc.lightBoost, static_cast<float>(desc.sea.argb & k_Rgb),
					                 static_cast<float>(desc.sea.specular & k_Rgb)};
					break;
				case sea_pass::SeaLight::LastDraw:
					u_objectLight.x = lit ? 3.0f : 0.0f;
					break;
				case sea_pass::SeaLight::Cut:
					// z = -1: each instance's own colour and specular (sea_pass::CutAtoms, the PSys mesh atoms)
					u_objectLight = {4.0f, static_cast<float>(desc.sea.argb >> 24) / 255.0f,
					                 desc.sea.perInstanceColour ? -1.0f : static_cast<float>(desc.sea.argb & k_Rgb),
					                 static_cast<float>(desc.sea.specular & k_Rgb)};
					break;
				}
				// x: the plane kept (fs), y: 1 = mirrored back in y = 0 (vs) (sea_plane.sh)
				const auto u_objectClip = sea_pass::PackClip(desc.sea);
				program->SetUniformValue("u_objectClip", &u_objectClip);                              // vs, fs
				program->SetTextureSampler("s_landLightTable", 3, fromBgfx(_landLightTexture.Get())); // vs
				// this frame's cells (land_light), or the loaded ones (the same layout) before the first frame's
				if (drawValues.frameCells)
				{
					program->SetTextureSampler("s_landCells", 4, fromBgfx(_landCellsTexture.Get())); // vs
				}
				else
				{
					program->SetTextureSampler("s_landCells", 4, drawValues.cellMap); // vs
				}
				program->SetUniformValue("u_cellMap", &drawValues.cellMapInfo); // vs
				program->SetUniformValue("u_objectLight", &u_objectLight);      // vs
				program->SetUniformValue("u_haze", &_hazeUniforms[0]);          // vs
				program->SetUniformValue("u_hazeColour", &_hazeUniforms[1]);    // vs
				// xyz: the engine's one light, w: the ambient (model_light.sh)
				program->SetUniformValue("u_modelLight", &drawValues.modelLight); // vs
				// x: a window sub-mesh, w: 1 if the primitive takes the object's texture offset
				// (L3DSubMesh::Primitive::uvOffset)
				const glm::vec4 u_window = {subMesh.GetFlags().isWindow ? 1.0f : 0.0f, 0.0f, 0.0f, prim.uvOffset ? 1.0f : 0.0f};
				program->SetUniformValue("u_window", &u_window); // vs
				const glm::vec4 u_materialColour = {glm::vec3(prim.colour), texture == nullptr ? 1.0f : 0.0f};
				program->SetUniformValue("u_materialColour", &u_materialColour); // fs
				if (desc.dynamicShadow.has_value())
				{
					program->SetTextureSampler("s_dynamicShadow", 5, *desc.dynamicShadow);
					program->SetUniformValue("u_dynamicShadowBox", &desc.dynamicShadowBox);
					program->SetUniformValue("u_dynamicShadowCull", &desc.dynamicShadowCull);
				}
			}
			if (!desc.isSky)
			{
				// y: the drawn mode's ALPHAREF / 255 (-1 without alpha test), w: its stage 0 alpha
				const auto alpha = render_modes::PrimitiveAlpha(
				    drawn, desc.table, static_cast<uint8_t>(std::lround(prim.alphaCutoutThreshold * 255.0f)), desc.globalAlpha);
				const glm::vec4 u_skyAlphaThreshold = {
				    0.0f, // x: unused (fs_object reads only y, z, w)
				    alpha.ref,
				    0.0f, // z: unused
				    static_cast<float>(alpha.source),
				};
				program->SetUniformValue("u_skyAlphaThreshold", &u_skyAlphaThreshold);
			}
			if (desc.morphTargets != nullptr)
			{
				// how far a creature's body is pulled towards each of its meshes (vs_object_morph_instanced)
				const glm::vec4 u_morphWeights {desc.morphTargets->weights, 0.0f};
				program->SetUniformValue("u_morphWeights", &u_morphWeights); // vs
			}
		}
		else
		{
			skip |= Mesh::k_SkipRenderState;
			skip |= Mesh::k_SkipVertexBuffer;
		}

		{
			if (desc.transientInstance != nullptr && (skip & Mesh::k_SkipInstanceBuffer) == 0)
			{
				bgfx::setInstanceDataBuffer(desc.transientInstance);
			}
			else if (desc.instanceDesc != nullptr && (skip & Mesh::k_SkipInstanceBuffer) == 0)
			{
				bgfx::setInstanceDataBuffer(toBgfx(desc.instanceDesc->GetRawHandle()), desc.instanceDesc->GetStart(),
				                            desc.instanceDesc->GetCount());
			}
			// (openblack guard) a mesh whose buffers bgfx could not create (out of handles) is not drawn
			if (!subMesh.GetMesh().GetVertexBuffer().IsValid() ||
			    (subMesh.GetMesh().IsIndexed() && !subMesh.GetMesh().GetIndexBuffer().IsValid()))
			{
				bgfx::discard(BGFX_DISCARD_ALL);
				lastPreserveState = false;
				continue;
			}
			if (subMesh.GetMesh().IsIndexed() && (skip & Mesh::k_SkipIndexBuffer) == 0)
			{
				subMesh.GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
			}
			if ((skip & Mesh::k_SkipVertexBuffer) == 0)
			{
				subMesh.GetMesh().GetVertexBuffer().Bind();
				if (desc.morphTargets != nullptr)
				{
					BindMorphTargets(mesh, subMesh, *desc.morphTargets);
				}
			}
			auto viewId = desc.viewId;
			// Setting the material: the culling from the material's two-sided bit (D3DCULL_CCW; the mirrored reflection camera
			// flips it, and a mesh mirrored back in the sea flips it back: sea_pass::SeaPassState::FaceCull)
			auto options = desc.options;
			if (!desc.isSky && options.cull == render_modes::Cull::None)
			{
				options.cull = sea_pass::ForPass(viewId).FaceCull(sea_pass::Surface::Model, prim.twoSided, desc.sea.unmirror);
			}
			// Blended: drawn after every opaque model (MainBlended) so what lies behind is already in the target
			if (blended && modelPass && viewId == RenderPass::Main)
			{
				viewId = RenderPass::MainBlended;
			}
			const auto state = render_modes::PrimitiveState(drawn, options, blended);
			if ((skip & Mesh::k_SkipRenderState) == 0)
			{
				bgfx::setState(state, desc.rgba);
			}

			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()), 0,
			             primitivePreserveState ? BGFX_DISCARD_NONE : BGFX_DISCARD_ALL);
		}
		lastPreserveState = primitivePreserveState;
	}
}

namespace
{
/// The land light mode and haze of a mesh's models (RenderContext::meshLandLight, land_light::ObjectLight)
void ApplyLandLightMode(const RenderContext& context, entt::id_type meshId, RendererInterface::L3DMeshSubmitDesc& desc)
{
	const auto mode = context.meshLandLight.find(meshId);
	desc.landLightMode = 0;
	if (mode != context.meshLandLight.end())
	{
		desc.landLightMode = static_cast<uint8_t>(mode->second.mode);
		desc.noHaze = desc.noHaze || !mode->second.haze;
	}
}
} // namespace

namespace
{
/// The on-screen region tests, shared (Graphics/RegionOnScreen.h)
using openblack::graphics::region_on_screen::BoxInView;
using openblack::graphics::region_on_screen::SphereInView;

/// OPENBLACK_ZSORTER_TRACE's frame count, in the debug hooks' store (Locator::debugHooks)
struct RendererDebugHooksState
{
	uint32_t zsorterTraceFrame {0};
};

RendererDebugHooksState& RendererDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("renderer: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<RendererDebugHooksState>();
}
} // namespace

const graphics::ShaderProgram* Renderer::BonesVariant32(const graphics::ShaderProgram* program) const
{
	if (_bonesVariants32.empty())
	{
		for (const auto* name :
		     {"ObjectInstanced", "ObjectHeightMapInstanced", "ObjectShadowInstanced", "ObjectHeightMapShadowInstanced"})
		{
			_bonesVariants32.emplace(_shaderManager->GetShader(name), _shaderManager->GetShader(std::string(name) + "B32"));
		}
	}
	const auto found = _bonesVariants32.find(program);
	return found != _bonesVariants32.end() ? found->second : program;
}

const graphics::ShaderProgram* Renderer::StaticVariant(const graphics::ShaderProgram* program) const
{
	if (_staticVariants.empty())
	{
		for (const auto* name :
		     {"ObjectInstanced", "ObjectHeightMapInstanced", "ObjectShadowInstanced", "ObjectHeightMapShadowInstanced"})
		{
			_staticVariants.emplace(_shaderManager->GetShader(name), _shaderManager->GetShader(std::string(name) + "Static"));
		}
	}
	const auto found = _staticVariants.find(program);
	return found != _staticVariants.end() ? found->second : program;
}

void Renderer::DrawMesh(const graphics::L3DMesh& mesh, const L3DMeshSubmitDesc& desc, uint8_t subMeshIndex) const noexcept
{
	if (mesh.GetNumSubMeshes() == 0)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Mesh {} has no submeshes to draw", mesh.GetDebugName());
		return;
	}

	const auto& subMeshes = mesh.GetSubMeshes();
	// worked out once for every sub-mesh and primitive of this draw: nothing they read changes during it
	std::optional<MeshDrawValues> values;
	std::optional<uint32_t> transformCache;

	if (subMeshIndex != std::numeric_limits<uint8_t>::max())
	{
		if (subMeshIndex >= mesh.GetNumSubMeshes())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "tried to draw submesh out of range ({}/{})", subMeshIndex,
			                   mesh.GetNumSubMeshes());
		}

		DrawSubMesh(mesh, *subMeshes[subMeshIndex], desc, false, values, transformCache);
		return;
	}

	for (auto it = subMeshes.begin(); it != subMeshes.end(); ++it)
	{
		const L3DSubMesh& subMesh = **it;
		DrawSubMesh(mesh, subMesh, desc, std::next(it) != subMeshes.end(), values, transformCache);
	}
}

void Renderer::DrawFootprintPass(const DrawSceneDesc& drawDesc) const
{
	const auto viewId = graphics::RenderPass::Footprint;
	auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::FootprintPass);
	if (drawDesc.drawIsland)
	{
		const auto& island = Locator::terrainSystem::value();
		island.GetFootprintFramebuffer().Bind(viewId);

		// This dummy draw call is here to make sure that view is cleared if no
		// other draw calls are submitted to view
		bgfx::touch(static_cast<bgfx::ViewId>(viewId));

		// _shaderManager->SetCamera(viewId, *drawDesc.camera); // TODO

		auto view = island.GetOrthoView();
		auto proj = island.GetOrthoProj();
		bgfx::setViewTransform(static_cast<bgfx::ViewId>(viewId), &view, &proj);

		const auto& meshManager = Locator::resources::value().GetMeshes();
		const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
		const auto* footprintShaderInstanced = _shaderManager->GetShader("FootprintInstanced");
		const auto drawFootprints = [&](const auto& descs) {
			for (const auto& [meshId, placers] : descs)
			{
				// (openblack guard) a mesh that did not load (the test's mock data); never in the original
				if (!meshManager.Contains(meshId))
				{
					continue;
				}
				auto mesh = meshManager.Handle(meshId);
				if (!mesh->ContainsLandscapeFeature() || mesh->GetFootprints().empty())
				{
					continue;
				}
				const auto& footprint = mesh->GetFootprints()[0];
				footprintShaderInstanced->SetTextureSampler("s_footprint", 0, *footprint.texture);
				footprint.mesh->GetVertexBuffer().Bind();
				bgfx::setInstanceDataBuffer(toBgfx(renderCtx.instanceUniformBuffer), placers.offset, placers.count);
				const uint64_t state = 0u                       //
				                       | BGFX_STATE_WRITE_RGB   //
				                       | BGFX_STATE_WRITE_A     //
				                       | BGFX_STATE_BLEND_ALPHA //
				                       | BGFX_STATE_CULL_CW     //
				                       | BGFX_STATE_MSAA;
				bgfx::setState(state);
				bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(footprintShaderInstanced->GetRawHandle()));
			}
		};
		drawFootprints(renderCtx.instancedDrawDescs);
		// the objects drawn through the Z-sorter (components::NeedsSorting): their footprint as any other's
		// (SetFootPrintOnTexture is not a drawing path). (pending) their place in the original's footprint order
		drawFootprints(renderCtx.sortedOpaqueDrawDescs);
		// the buildings at 0 % (components::NotDrawn): not drawn, but their footprint is (SetFootPrintOnTexture)
		drawFootprints(renderCtx.footprintOnlyDrawDescs);
		DrawRiverFootprints(static_cast<bgfx::ViewId>(viewId), false);
	}
}

void Renderer::DrawRiverFootprints(bgfx::ViewId viewId, bool channel) const
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	const entt::id_type meshId = entt::hashed_string(channel ? "river" : "river2").value();
	if (!meshes.Contains(meshId))
	{
		return;
	}
	const auto mesh = meshes.Handle(meshId);
	if (mesh->GetFootprints().empty())
	{
		return;
	}
	std::vector<glm::mat4> matrices;
	Locator::entitiesRegistry::value().Each<const ecs::components::StreamFootprint, const ecs::components::Transform>(
	    [&matrices, channel](const ecs::components::StreamFootprint& footprint, const ecs::components::Transform& transform) {
		    if (footprint.channel == channel)
		    {
			    matrices.push_back(affine::Model(transform));
		    }
	    },
	    entt::exclude<ecs::components::Unavailable>);
	const auto count = static_cast<uint32_t>(matrices.size());
	constexpr uint16_t k_Stride = sizeof(glm::mat4);
	if (count == 0 || bgfx::getAvailInstanceDataBuffer(count, k_Stride) < count)
	{
		return;
	}
	bgfx::InstanceDataBuffer instances;
	bgfx::allocInstanceDataBuffer(&instances, count, k_Stride);
	std::memcpy(instances.data, matrices.data(), matrices.size() * sizeof(glm::mat4));

	const auto& footprint = mesh->GetFootprints()[0];
	const auto* program = _shaderManager->GetShader(channel ? "LandAlphaInstanced" : "FootprintInstanced");
	program->SetTextureSampler("s_footprint", 0, *footprint.texture);
	if (channel)
	{
		const auto size = footprint.texture->GetResolution();
		const glm::vec4 u_footprintSize(size.x, size.y, 0.0f, 0.0f);
		program->SetUniformValue("u_footprintSize", &u_footprintSize);
	}
	footprint.mesh->GetVertexBuffer().Bind();
	bgfx::setInstanceDataBuffer(&instances);
	// the bed blends its colour like any footprint; the channel keeps the lowest alpha
	const uint64_t state = channel ? BGFX_STATE_WRITE_R | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE) |
	                                     BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_MIN)
	                               : BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA;
	bgfx::setState(state);
	bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
}

void Renderer::DrawLandAlphaPass(const DrawSceneDesc& drawDesc) const
{
	if (!drawDesc.drawIsland)
	{
		return;
	}
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::LandAlpha);
	const auto& island = Locator::terrainSystem::value();
	const auto& frameBuffer = island.GetLandAlphaFramebuffer();
	frameBuffer.Bind(graphics::RenderPass::LandAlpha);
	bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0xFFFFFFFF);
	bgfx::setViewRect(viewId, 0, 0, frameBuffer.GetColorAttachment().GetResolution().x,
	                  frameBuffer.GetColorAttachment().GetResolution().y);
	bgfx::touch(viewId);
	const auto view = island.GetOrthoView();
	const auto proj = island.GetOrthoProj();
	bgfx::setViewTransform(viewId, &view, &proj);
	DrawRiverFootprints(viewId, true);
}

void Renderer::UpdateLandLight() const
{
	if (!_landLight)
	{
		_landLight = std::make_unique<LandLightTable>();
		try
		{
			auto& fileSystem = Locator::filesystem::value();
			const auto path = fileSystem.GetPath<filesystem::Path::WeatherSystem>() / "palette.raw";
			auto& palettes = Locator::resources::value().GetLandLightPalettes();
			if (!palettes.Contains(LandLightPalette::k_Id.value()))
			{
				palettes.Load(LandLightPalette::k_Id.value(), resources::LandLightPaletteLoader::FromDiskTag {},
				              fileSystem.FindPath(path));
			}
			_landLightPalette = palettes.Handle(LandLightPalette::k_Id.value()).handle();
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "No landscape light table (palette.raw): {}", e.what());
		}
		_landLightTexture.Reset(bgfx::createTexture2D(LandLightTable::k_Size, 1, false, 1, bgfx::TextureFormat::RGBA8,
		                                              BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP));
		bgfx::setName(_landLightTexture.Get(), "LandLightTable");
		_landLightBuilt.reset();
	}
	if (!IsLandLit())
	{
		return;
	}
	// The overcast at the camera caps the base colour (Clouds::WeatherOvercastAtCamera); the lightning flash at the
	// camera lerps the table to white (sky_weather::LightningFlash -> weather::LightningFlashAtCamera) with this frame's
	// sky type: the table is built right after the sky type is updated, so its sky type is sky_type::Frame(), the value
	// the haze reads.
	// The inputs are read every frame (the overcast's lookup refreshes the weather cells it reads, which the others do not
	// read). The table is a function of them and of the palette, which never changes once loaded, and the texture keeps
	// what was last uploaded: with the same inputs as the last build, the table, the copy of it the game reads
	// (RenderFrameSystemInterface::GetLandLightTable) and the texture already hold what a new build would give
	const float skyType = sky_type::Frame();
	const float alignment = _skyAlignment.Get();
	const float overcast = Clouds::WeatherOvercastAtCamera();
	const uint8_t flash = sky_weather::LightningFlash();
	const LandLightInputs inputs {
	    .skyType = std::bit_cast<uint32_t>(skyType),
	    .alignment = std::bit_cast<uint32_t>(alignment),
	    .overcast = std::bit_cast<uint32_t>(overcast),
	    .flash = flash,
	};
	if (_landLightBuilt == inputs)
	{
		return;
	}
	_landLightBuilt = inputs;
	_landLight->Build(*_landLightPalette, skyType, alignment, overcast, flash);
	// the game's copy, read when something is coloured by the land's light outside the renderer
	Locator::renderFrameSystem::value().SetLandLightTable(*_landLight);
	const auto& texels = _landLight->GetTexels();
	bgfx::updateTexture2D(_landLightTexture.Get(), 0, 0, 0, 0, LandLightTable::k_Size, 1,
	                      bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(texels[0]))));
}

void Renderer::DrawStaticShadowPass(const DrawSceneDesc& drawDesc) const
{
	if (!drawDesc.drawIsland || !drawDesc.drawEntities)
	{
		return;
	}
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::StaticShadow);
	const auto& island = Locator::terrainSystem::value();
	const auto& frameBuffer = island.GetStaticShadowFramebuffer();
	frameBuffer.Bind(graphics::RenderPass::StaticShadow);
	bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0x00000000);
	bgfx::setViewRect(viewId, 0, 0, frameBuffer.GetColorAttachment().GetResolution().x,
	                  frameBuffer.GetColorAttachment().GetResolution().y);
	bgfx::touch(viewId);
	const auto view = island.GetOrthoView();
	const auto proj = island.GetOrthoProj();
	bgfx::setViewTransform(viewId, &view, &proj);

	const auto& meshManager = Locator::resources::value().GetMeshes();
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto* boned = _shaderManager->GetShader("StaticShadowInstanced");
	const auto* single = _shaderManager->GetShader("StaticShadowInstancedStatic");
	constexpr uint64_t k_State = BGFX_STATE_WRITE_R | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE) |
	                             BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_MAX);
	for (const auto& [meshId, placers] : renderCtx.shadowCasterDrawDescs)
	{
		if (!meshManager.Contains(meshId)) // (openblack guard) a mesh that did not load; never in the original
		{
			continue;
		}
		const auto mesh = meshManager.Handle(meshId);
		const auto* program = mesh->IsBoned() ? boned : single;
		const glm::mat4 identity(1.0f);
		const auto* matrices = mesh->IsBoned() ? mesh->GetBoneMatrices().data() : &identity;
		const auto matrixCount = mesh->IsBoned() ? static_cast<uint16_t>(mesh->GetBoneMatrices().size()) : uint16_t {1};
		for (const auto& subMesh : mesh->GetSubMeshes())
		{
			// LOD 0 only, like the original's static shadows
			if (subMesh->IsPhysics() || subMesh->GetFlags().status != 0 || (subMesh->GetFlags().lodMask & 1) != 1)
			{
				continue;
			}
			for (const auto& prim : subMesh->GetPrimitives())
			{
				const auto* texture = world_triangles::PrimitiveTexture(*mesh, prim.skinID);
				// x: ALPHAREF / 255 of the primitive's mode, -1 without alpha test (inferred: the normal table)
				const auto alpha = render_modes::PrimitiveAlpha(
				    static_cast<render_modes::Mode>(prim.materialType), render_modes::Table::Normal,
				    static_cast<uint8_t>(std::lround(prim.alphaCutoutThreshold * 255.0f)));
				const glm::vec4 u_shadowParams = {alpha.ref, texture != nullptr ? 1.0f : 0.0f, 0.0f, 0.0f};
				program->SetUniformValue("u_shadowParams", &u_shadowParams);
				if (texture != nullptr)
				{
					program->SetTextureSampler("s_diffuse", 0, *texture);
				}
				bgfx::setTransform(matrices, matrixCount);
				bgfx::setInstanceDataBuffer(toBgfx(renderCtx.instanceUniformBuffer), placers.offset, placers.count);
				if (subMesh->GetMesh().IsIndexed())
				{
					subMesh->GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
				}
				subMesh->GetMesh().GetVertexBuffer().Bind();
				bgfx::setState(k_State);
				bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
			}
		}
	}
}

void Renderer::DrawCelestialMesh(graphics::RenderPass viewId, const L3DMesh& mesh, const glm::mat4& model,
                                 const Texture2D& texture, const glm::vec4& colour, uint64_t state, const glm::vec4& celestial,
                                 const Texture2D* alpha) const
{
	const auto* program = _shaderManager->GetShader("Celestial");
	for (const auto& subMesh : mesh.GetSubMeshes())
	{
		for (const auto& prim : subMesh->GetPrimitives())
		{
			bgfx::setTransform(&model);
			program->SetTextureSampler("s_diffuse", 0, texture);
			program->SetUniformValue("u_colour", &colour);
			program->SetUniformValue("u_celestial", &celestial);
			program->SetTextureSampler("s_alpha", 1, alpha != nullptr ? *alpha : texture);
			if (subMesh->GetMesh().IsIndexed())
			{
				subMesh->GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
			}
			subMesh->GetMesh().GetVertexBuffer().Bind();
			bgfx::setState(state);
			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
		}
	}
}

void Renderer::DrawSun(graphics::RenderPass viewId, const Camera& camera, bool glare, uint32_t frameGameMs) const
{
	const auto& sky = Locator::skySystem::value();
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_SunTexture = entt::hashed_string("raw/sun").value();
	if (!textures.Contains(k_SunTexture))
	{
		return;
	}
	// height by game hour T, alpha A fading in 3..6 h and out 18..21 h
	const float t = sky.GetTime();
	const float height = 7500.0f * (std::clamp(std::min(t, 24.0f - t), 6.0f, 12.0f) - 6.0f) / 6.0f;
	float alpha = 255.0f;
	if (t < 3.0f || t > 21.0f)
	{
		alpha = 0.0f;
	}
	else if (t < 6.0f)
	{
		alpha = (t - 3.0f) * 85.0f;
	}
	else if (t > 18.0f)
	{
		alpha = 255.0f - (t - 18.0f) * 85.0f;
	}
	if (alpha <= 0.0f)
	{
		return;
	}
	// A vertical quad at (-30000, y, -30000) turned by 3*pi/4 about Y, i.e. facing the island
	const glm::vec3 position(-30000.0f, height, -30000.0f);
	// (inferred) the turn (+3 pi / 4 in the original's sense = glm's -3 pi / 4) is not read
	auto model = glm::translate(position) * glm::rotate(-3.0f * glm::pi<float>() / 4.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	const auto& texture = *textures.Handle(k_SunTexture);
	// mode 13: additive SRCALPHA / ONE, colour and alpha = texture x diffuse, no Z write, cull none; the glare with
	// ZFUNC ALWAYS
	const uint64_t additive =
	    render_modes::State(render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite, {.zFunc = render_modes::ZFunc::Always});
	if (!glare)
	{
		const glm::vec4 colour(glm::vec3(0x95, 0x7C, 0x63) / 255.0f, alpha / 255.0f);
		DrawCelestialMesh(viewId, sky.GetSunMesh(), model, texture, colour,
		                  render_modes::State(render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite));
		return;
	}

	// Glare: 5 samples around the sun, each hidden when the landscape is in the way; the
	// visibility eases toward (1 - 0.2 * hidden) * 255 by 1 % per ms
	int hidden = 0;
	const auto origin = camera.GetOrigin();
	const auto& island = Locator::terrainSystem::value();
	const auto right = glm::vec3(model * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	for (const auto& offset : {glm::vec2(0.0f), glm::vec2(500.0f, 500.0f), glm::vec2(-500.0f, 500.0f),
	                           glm::vec2(500.0f, -500.0f), glm::vec2(-500.0f, -500.0f)})
	{
		auto sample = position + right * offset.x + glm::vec3(0.0f, offset.y, 0.0f);
		sample.y = std::max(sample.y, 10.0f);
		const auto direction = sample - origin;
		// march over the island (the landscape is at most a few thousand units across)
		constexpr int k_Steps = 256;
		for (int i = 1; i <= k_Steps; ++i)
		{
			const auto p = origin + direction * (static_cast<float>(i) / k_Steps * 0.25f);
			if (p.y < island.GetHeightAt(glm::vec2(p.x, p.z)))
			{
				++hidden;
				break;
			}
		}
	}
	const float target = (1.0f - 0.2f * static_cast<float>(hidden)) * 255.0f;
	// (target - glare) x (frame game ms x 0.01) + glare, clamped to 0..255; the factor is not capped (the frame's game
	// ms stay below 200) and the glare stays put in pause (0 ms)
	const float factor = static_cast<float>(frameGameMs) * game_clock::k_FractionPerMs; // the frame's (DrawClock)
	const float eased = (target - _sunGlare) * factor;
	_sunGlare = std::clamp(eased + _sunGlare, 0.0f, 255.0f);
	if (_sunGlare <= 0.0f)
	{
		return;
	}
	model = model * glm::scale(glm::vec3(1.8f));
	const glm::vec4 colour(glm::vec3(0xA0, 0x6A, 0x35) / 255.0f, _sunGlare * alpha / 255.0f / 255.0f);
	DrawCelestialMesh(viewId, sky.GetSunMesh(), model, texture, colour, additive);
}

void Renderer::DrawMoon(graphics::RenderPass viewId, const Camera& camera) const
{
	const auto pass = sea_pass::ForPass(viewId);
	const bool mirrored = pass.mirrored;
	const auto& sky = Locator::skySystem::value();
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Weather = entt::hashed_string("raw/weather").value();
	constexpr entt::id_type k_WeatherAlpha = entt::hashed_string("raw/weathera").value();
	constexpr entt::id_type k_Atmos = entt::hashed_string("raw/ATMOS").value();
	constexpr entt::id_type k_AtmosAlpha = entt::hashed_string("raw/ATMOSA").value();
	if (!textures.Contains(k_Weather) || !textures.Contains(k_WeatherAlpha) || !textures.Contains(k_Atmos) ||
	    !textures.Contains(k_AtmosAlpha))
	{
		return;
	}
	// Position relative to the camera, by game hour T; alpha m = min(200, 0.5 y - 110), so it shows about +-4.7 h
	// around midnight
	const float theta = sky.GetTime() * glm::pi<float>() / 12.0f;
	const glm::vec3 offset(4000.0f, 1100.0f * std::cos(theta) - 150.0f, 800.0f * std::sin(theta));
	const float m = std::min(200.0f, std::floor(0.5f * offset.y - 110.0f));
	if (m <= 0.0f)
	{
		return;
	}
	// (the reflection camera has the main camera's origin)
	const auto centre = camera.GetOrigin() + offset;
	const auto frame = billboard::CameraFrame::From(camera);
	// The moon is drawn twice for the reflection, the second time at (x, -y, z) under water. That second call only
	// draws its glow (it skips the moon object): drawn here with the mirrored camera at (x, y, z), it is that glow,
	// built from the mirrored view (billboard::MoonBasis). The moon in the reflection is the first call's
	// DrawUnderWater, its matrix mirrored in y = 0: drawn with the mirrored camera, that is the main camera's moon
	// matrix itself (the mirrored view x mirror(y) is the main view, ReflectionXZCamera::GetViewMatrix), so the mesh
	// comes out flipped and its winding with it.
	const auto colour = IsLandLit() ? LandLightTable::ToColour(_landLight->GetMoonColour()) : glm::vec3(1.0f);

	// Glow: a 4000 x 4000 quad on the moon's basis (billboard::MoonHalo), atmos.raw UV
	// 0.25..0.49375, additive (mode 13), colour (R/6, G/5, B/4, m)
	{
		struct Vertex
		{
			float x, y, z, u, v;
		};
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .end();
		if (bgfx::getAvailTransientVertexBuffer(6, layout) == 6)
		{
			bgfx::TransientVertexBuffer buffer;
			bgfx::allocTransientVertexBuffer(&buffer, 6, layout);
			auto* vertices = reinterpret_cast<Vertex*>(buffer.data);
			const auto halo = billboard::MoonHalo(billboard::MoonBasis(frame.view, frame.inverseView, centre), centre);
			for (size_t i = 0; i < billboard::k_MoonHaloTriangles.size(); ++i)
			{
				const auto k = static_cast<size_t>(billboard::k_MoonHaloTriangles.at(i));
				const auto& p = halo.corners.at(k);
				vertices[i] = {p.x, p.y, p.z, halo.uv.at(k).x, halo.uv.at(k).y};
			}
			const auto* program = _shaderManager->GetShader("Celestial");
			const glm::mat4 identity(1.0f);
			const glm::vec4 glowColour(colour.r / 6.0f, colour.g / 5.0f, colour.b / 4.0f, m / 255.0f);
			const glm::vec4 celestial(0.0f, 0.0f, 0.0f, 1.0f);
			bgfx::setTransform(&identity);
			program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Atmos));
			program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_AtmosAlpha));
			program->SetUniformValue("u_colour", &glowColour);
			program->SetUniformValue("u_celestial", &celestial);
			bgfx::setVertexBuffer(0, &buffer);
			bgfx::setState(render_modes::State(render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite));
			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
		}
	}

	// The moon (mode 4: SRCALPHA / INVSRCALPHA): the moon's basis x4, its tilt, RotateY(phase + pi), x0.65
	// (billboard::MoonModel). The phase follows the real clock (audio::guidance::MoonPhase, whole days)
	const auto phase = audio::guidance::MoonPhase(audio::spooky::UnixTime());
	const auto mainView = mirrored ? sea_pass::UnmirrorView(frame.view) : frame.view;
	const auto mainInverseView = mirrored ? glm::inverse(mainView) : frame.inverseView;
	const auto model = billboard::MoonModel(billboard::MoonBasis(mainView, mainInverseView, centre), centre, phase);
	const glm::vec4 moonColour(colour, m / 255.0f);
	const glm::vec4 celestial(std::cos(phase), std::sin(phase), 1.0f, 1.0f);
	// (inferred) without the Z write of mode 4: nothing farther is drawn after it in the sky.
	// The moon object is flagged to use the alternate mode table before its Draw and its DrawUnderWater, so both draw
	// mode 4 as 5 (the same blend and Z write, ALPHAOP MODULATE(TEXTURE, DIFFUSE)). fs_celestial always modulates the
	// alpha by u_colour, so the state of mode 4 here already draws as mode 5, in both passes
	DrawCelestialMesh(viewId, sky.GetMoonMesh(), model, *textures.Handle(k_Weather), moonColour,
	                  render_modes::State(render_modes::Mode::AlphaTextured,
	                                      {.cull = pass.FaceCull(sea_pass::Surface::Model, false, false), .zWrite = false}),
	                  celestial, &*textures.Handle(k_WeatherAlpha));
}

void Renderer::UpdateClouds() const
{
	const auto& detail = GetDetailLevel(Locator::config::value().detailLevel);
	// opening a land opens the sky's clouds: a new layout for every land
	if (!_clouds || _cloudsGeneration != Clouds::GetLandscapeGeneration())
	{
		_clouds = std::make_unique<Clouds>();
		_cloudsGeneration = Clouds::GetLandscapeGeneration();
	}
	if (_cloudShadowImage.empty())
	{
		// the file system resolves the path when it reads: a missing file is reported by LoadOptionalBlob, not thrown
		_cloudShadowImage = resources::LoadOptionalBlob(
		    Locator::resources::value().GetBlobs(),
		    Locator::filesystem::value().GetPath<filesystem::Path::Textures>() / "sclouds.raw", "cloud shadows");
	}
	// game_clock::FrameGameMs: the game ms of this frame, whole, 0 while paused, faster or slower with the game speed.
	// The clouds and their animation stop while the game is paused. Its readers here: the sky's alignment, the clouds,
	// and for the night lights the jitter and the flames
	const auto milliseconds = static_cast<float>(game_clock::FrameGameMs());
	const bool running = Locator::time::has_value() && !game_clock::IsPaused();
	if (running)
	{
		_clouds->Update(milliseconds);
	}
	// CollectClouds advances the animation counters of the clouds it queues by this step
	_cloudMilliseconds = running ? milliseconds : 0.0f;
	// the sky's alignment moves towards the most influential player's
	_skyAlignment.Update(Clouds::InfluentialPlayerAlignment(), running ? milliseconds : 0.0f);

	// the colour and the alpha byte from the sky's alignment and light table[255]
	const uint32_t table255 = IsLandLit() ? land_light::FullLight(*_landLight) : 0xFFFFFFFFu;
	const uint32_t colour = Clouds::Colour(_skyAlignment.Get(), table255);
	_cloudRgb = argb_colour::ToVec3(colour);
	const auto alignAlpha = static_cast<int>(colour >> 24);
	_cloudAlpha.resize(_clouds->GetClouds().size());
	for (size_t i = 0; i < _cloudAlpha.size(); ++i)
	{
		// alpha = edge * A / 255 in integers, drawn only when it is not 0
		_cloudAlpha[i] =
		    detail.clouds ? static_cast<float>(Clouds::EdgeAlpha(_clouds->GetClouds()[i]) * alignAlpha / 255) : 0.0f;
	}

	// This frame's land cells (land_light, the cell map's layout): the loaded ones back, then the map clouds' stamps
	// ("CloudShadows"), every stamp of this frame in the stamp list (PSys light maps, the storms, the flashes, the
	// fires), then the hand's and the village lights
	if (!Locator::terrainSystem::has_value())
	{
		land_light::ClearStamps();
		return;
	}
	const auto& island = Locator::terrainSystem::value();
	const auto size = island.GetCellMap().GetResolution();
	if (size != _landCellsSize)
	{
		_landCellsTexture.Reset(); // the old one before the new one is made
		_landCellsTexture.Reset(bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::RGBA8,
		                                              BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP));
		_landCellsSize = size;
	}
	land_light::BeginFrame(island, Clouds::GetLandscapeGeneration());
	if (detail.clouds)
	{
		_clouds->StampShadows(_cloudShadowImage, _cloudAlpha);
	}
	land_light::ApplyStamps();
	land_light::ClearStamps();
	// Night lights: the hand light and the village lights into this frame's luminosities, after the stamps: they read
	// the cell's luminosity byte and write it directly, no min with the loaded one
	if (IsLandLit() && Locator::time::has_value() && Locator::dayNightClock::has_value())
	{
		auto luminosity = land_light::Luminosity();
		night_lights::LightCells cells {
		    .firstCell = land_light::GetCells().firstCell,
		    .size = glm::ivec2(size),
		    .cap = &luminosity,
		    .fullLightGreen = static_cast<uint8_t>((land_light::FullLight(*_landLight) >> 8) & 0xFFu),
		};
		night_lights::Update(game_clock::IsPaused() ? 0.0f : milliseconds,
		                     Locator::dayNightClock::value().Clock().GetScriptTime(),
		                     LandLightTable::ToColour(_landLight->GetLandColour()), cells);
		land_light::SetLuminosity(luminosity);
	}
	const auto texels = land_light::Texels();
	if (texels.size() == static_cast<size_t>(size.x) * size.y * 4)
	{
		bgfx::updateTexture2D(_landCellsTexture.Get(), 0, 0, 0, 0, size.x, size.y,
		                      bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size())));
	}
}

std::vector<std::pair<float, uint32_t>> Renderer::CollectClouds(const Camera& camera) const
{
	std::vector<std::pair<float, uint32_t>> order;
	if (!_clouds || !GetDetailLevel(Locator::config::value().detailLevel).clouds)
	{
		return order;
	}
	const auto& mesh = Locator::skySystem::value().GetCloudMesh();
	if (mesh.GetNumSubMeshes() == 0)
	{
		return order;
	}
	const auto origin = camera.GetOrigin();
	// as for the mists: only a cloud whose sphere
	// (the mesh's bounding-box half diagonal x size x 0.55) touches the screen is queued and advances its counter
	const float meshRadius = glm::length(mesh.GetBoundingBox().Size()) * 0.5f;
	const auto viewProjection = camera.GetViewProjectionMatrix(Camera::Interpolation::Current);
	const float milliseconds = _cloudMilliseconds;
	_cloudMilliseconds = 0.0f;
	// mist.l3d is loaded without skins; the mist draw uses the smoke material instead
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
	constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();
	if (!textures.Contains(k_Smoke) || !textures.Contains(k_SmokeAlpha))
	{
		return order;
	}
	order.reserve(_clouds->GetClouds().size());
	for (size_t index = 0; index < _clouds->GetClouds().size(); ++index)
	{
		const auto& cloud = _clouds->GetClouds()[index];
		const auto position = Clouds::WorldPosition(cloud);
		if (_cloudAlpha[index] / 255.0f <= 0.0f || !SphereInView(viewProjection, position, meshRadius * cloud.size * 0.55f))
		{
			continue;
		}
		_clouds->AdvanceAnimation(index, milliseconds);
		// the Z-sorter key: |position - camera|^2, (x^2 + y^2) + z^2
		order.emplace_back(zsort::Key(position, origin), static_cast<uint32_t>(index));
	}
	return order;
}

void Renderer::DrawCloud(graphics::RenderPass viewId, const Camera& camera, uint32_t index) const
{
	if (!_clouds || index >= _clouds->GetClouds().size())
	{
		return;
	}
	const auto rgb = _cloudRgb;
	const auto& mesh = Locator::skySystem::value().GetCloudMesh();
	const auto origin = camera.GetOrigin();
	// the same billboard as the map mists (Renderer::DrawMist): the inverse camera rotation, in glm
	// mat3(right, -forward, up): local X = screen right, local Y (the dome's axis) towards the camera, local Z =
	// screen up (billboard::MistBasis)
	const auto cameraFrame = billboard::CameraFrame::From(camera);
	const auto& rotation = billboard::MistBasis(cameraFrame);
	// The clouds are mists too (with a size and a shrink), so their draw is the effect branch of the same mist draw:
	// no specular (it is never written) and the temporary light straight above.
	const glm::vec4 u_cloudSpecular(0.0f);
	const auto* program = _shaderManager->GetShader("Cloud");
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
	constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();
	const auto& smoke = *textures.Handle(k_Smoke);
	const auto& smokeAlpha = *textures.Handle(k_SmokeAlpha);
	const auto& cloud = _clouds->GetClouds()[index];
	const auto position = Clouds::WorldPosition(cloud);
	// row 0 (local X, the screen width) is scaled by the size and rows 1-2 (local Y = depth, local Z = screen
	// height) by the shrunk one, so a cloud is round only straight overhead and near the horizon it is about k (2.5
	// to 5, set when the clouds open) times wider than tall (billboard::MistShrunkSize)
	const float shrunk = billboard::MistShrunkSize(cloud.size, cloud.k, position - origin);
	const auto model = glm::translate(position) * glm::mat4(rotation) * glm::scale(glm::vec3(cloud.size, shrunk, shrunk));
	const glm::vec4 u_cloudColour(rgb, _cloudAlpha[index] / 255.0f);
	// one whole atlas cell, rows 2-3 (the frame after this frame's step;
	// frame_anim::MistCellUv of the effect branch)
	const auto cell = frame_anim::MistCellUv(Clouds::GetFrame(cloud), true);
	// the mist draw saves the engine light and moves it straight above while it draws the clouds, with the ambient at
	// 210; both go back afterwards. model_light::LightInMeshSpace brings the light into the mesh's own space, normalised
	const model_light::ScopedLight cloudLight(glm::vec3(0.0f, 500000.0f, 0.0f));
	const model_light::ScopedAmbient cloudAmbient(model_light::k_MistAmbient);
	const glm::vec4 u_cloud(cell.x, cell.y, static_cast<float>(model_light::Ambient()) / 256.0f, 0.0f);
	const glm::vec4 u_cloudLight(model_light::LightInMeshSpace(model), 0.0f);
	for (const auto& subMesh : mesh.GetSubMeshes())
	{
		for (const auto& prim : subMesh->GetPrimitives())
		{
			bgfx::setTransform(&model);
			program->SetTextureSampler("s_diffuse", 0, smoke);
			program->SetTextureSampler("s_alpha", 1, smokeAlpha);
			program->SetUniformValue("u_cloud", &u_cloud);
			program->SetUniformValue("u_cloudColour", &u_cloudColour);
			program->SetUniformValue("u_cloudLight", &u_cloudLight);
			program->SetUniformValue("u_cloudSpecular", &u_cloudSpecular);
			if (subMesh->GetMesh().IsIndexed())
			{
				subMesh->GetMesh().GetIndexBuffer().Bind(prim.indicesCount, prim.indicesOffset);
			}
			subMesh->GetMesh().GetVertexBuffer().Bind();
			// the smoke material: mode 6, two-sided
			bgfx::setState(render_modes::State(render_modes::materials::k_Smoke));
			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
		}
	}
}

void Renderer::DrawObjectReflections(graphics::RenderPass viewId) const
{
	if (!Locator::handSystem::has_value())
	{
		return;
	}
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& hand = Locator::handSystem::value();
	// DrawUnderWater of the held object (by the hand, after the hand, in its own colour) and of the physics objects
	// (while not wholly under water: y > -r, r = the farthest vertex). "Own colour" is the object colour and specular
	// as the last Draw left them: the land light and cell specular of the physics objects' draw.
	// The physics objects are the awake entries of the physics list (the asleep ones are skipped: no resting proxy, so
	// no broken house); r is the body's radius (the farthest vertex). (pending) a fragment's own 3D object is a rock in
	// the default colour (white, no specular): the original mirrors that rock, openblack the piece. The hand's thrown
	// objects that are not in the physics keep the bounding box radius.
	struct Reflected
	{
		entt::entity entity;
		float radius; ///< < 0: always drawn (the held object); 0: from the bounding box
		float centreY;
	};
	std::vector<Reflected> objects;
	// the render hand's object, held from the press: it casts no dynamic shadow while held, restored when the hand
	// throws it
	if (const auto held = hand.GetRenderHandObject(); held.has_value())
	{
		objects.push_back({*held, -1.0f, 0.0f});
	}
	ecs::physics::PhysicsObjects::ForEach([&objects](const ecs::physics::PhysicsObject& po) {
		if (po.entity != entt::null && !po.body.resting)
		{
			objects.push_back({po.entity, po.body.Radius(), po.body.Centre().y});
		}
	});
	for (const auto entity : hand.GetThrownObjects())
	{
		if (ecs::physics::PhysicsObjects::Find(entity) == nullptr)
		{
			objects.push_back({entity, 0.0f, 0.0f});
		}
	}
	for (const auto& [entity, bodyRadius, centreY] : objects)
	{
		const auto instance = renderCtx.entityInstances.find(entity);
		if (!registry.Valid(entity) || instance == renderCtx.entityInstances.end() || !meshes.Contains(instance->second.meshId))
		{
			continue;
		}
		if (bodyRadius >= 0.0f)
		{
			const auto mesh = meshes.Handle(instance->second.meshId);
			const auto& transform = registry.Get<ecs::components::Transform>(entity);
			// the scale it is drawn at (a creature is drawn smaller in its pen)
			const float radius = bodyRadius > 0.0f ? bodyRadius
			                                       : 0.5f * glm::length(mesh->GetBoundingBox().Size()) *
			                                             ecs::creature_pose::DrawnScale(registry, entity).x;
			if ((bodyRadius > 0.0f ? centreY : transform.position.y) <= -radius)
			{
				continue;
			}
		}
		// DrawUnderWater in the colour the last Draw left (the held object, the physics objects)
		DrawUnderWater(viewId, entity, sea_pass::UnderWaterLastDraw());
	}
}

void Renderer::DrawFishShoals(graphics::RenderPass viewId) const
{
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Texture = entt::hashed_string("raw/misc0").value();
	constexpr entt::id_type k_Alpha = entt::hashed_string("raw/misc0a").value();
	if (!textures.Contains(k_Texture) || !textures.Contains(k_Alpha))
	{
		return;
	}
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	std::vector<Vertex> vertices;
	Locator::entitiesRegistry::value().Each<const ecs::components::FishFarm>(
	    [&vertices](const ecs::components::FishFarm& farm) {
		    if (!farm.shoal.has_value() || !farm.shoal->visible)
		    {
			    return;
		    }
		    const uint32_t colour = (static_cast<uint32_t>(farm.shoal->alpha) << 24) | 0x00FFFFFFu;
		    for (size_t i = 0; i < std::min(farm.shoal->shown, farm.shoal->fish.size()); ++i)
		    {
			    const auto& fish = farm.shoal->fish[i];
			    // a flat sprite: a quad turned about Y, its local x along the heading (billboard::Horizontal); cells 8..23 of
			    // the 8 x 8 sheet
			    billboard::Sprite sprite {
			        // drawn before the sea without a mirror: mirrored back in the reflection target (see
			        // DrawPass; on the CPU, vs_blob is shared by 6 programs)
			        .position = sea_pass::Unmirror(fish.position),
			        .size = fish.halfSize,
			        .angle = fish.heading,
			        .cell = fish.cell, // taken before the frame's wrap (frame_anim::FishFrame)
			        .horizontal = true,
			    };
			    const auto quad = billboard::Horizontal(sprite);
			    for (const int k : billboard::k_SpriteTriangles)
			    {
				    const auto& p = quad.corners.at(static_cast<size_t>(k));
				    const auto& uv = quad.uv.at(static_cast<size_t>(k));
				    vertices.push_back({p.x, p.y, p.z, uv.x, uv.y, colour});
			    }
		    }
	    },
	    entt::exclude<ecs::components::Unavailable>);
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
	const auto* program = _shaderManager->GetShader("WorldQuad");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Texture));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_Alpha));
	bgfx::setVertexBuffer(0, &buffer);
	// the misc0 material (inferred), mode 6: SRCALPHA / INVSRCALPHA, no Z write; two-sided. ZFUNC ALWAYS (approximate:
	// the original keeps LESSEQUAL; equivalent because the mirrored land under them wrote no Z)
	bgfx::setState(render_modes::State(render_modes::materials::k_Misc0, {.zFunc = render_modes::ZFunc::Always}));
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
}

void Renderer::PreloadForLand() const noexcept
{
	// (openblack engine) what the draw would load lazily, loaded with the land: the chimney smoke's alpha texture
	LoadChimneySmokeAlpha();
	// the help text's three fonts, which the original loads when it makes its help text, not on the first text drawn
	for (const auto font : {help::TextFont::J0, help::TextFont::F1, help::TextFont::F3})
	{
		[[maybe_unused]] const auto* loaded = GameFontAt(font);
	}
	// Textures and meshes no longer call bgfx::frame() when they are made: two frames here hand everything the land
	// made to the render thread and wait for its upload, so the upload stays in the load and not in the first frames
	bgfx::frame();
	bgfx::frame();
}

void Renderer::DrawWaterRings(graphics::RenderPass viewId) const
{
	const auto& rings = ecs::GetWaterRings();
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Texture = entt::hashed_string("raw/smoke").value();
	constexpr entt::id_type k_Alpha = entt::hashed_string("raw/smokea").value();
	if (rings.empty() || !textures.Contains(k_Texture) || !textures.Contains(k_Alpha))
	{
		return;
	}
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	std::vector<Vertex> vertices;
	vertices.reserve(rings.size() * 6);
	for (const auto& ring : rings)
	{
		// half size max(age * growth / 700, 0.0001), the z half size x aspect; alpha (255 - 0.364286 age) * A >> 8
		const float half = std::max(static_cast<float>(ring.age) * ring.growth * 0.00142857f, 0.0001f);
		const auto alpha = static_cast<uint32_t>(static_cast<int>((255.0f - static_cast<float>(ring.age % 700) * 0.364286f) *
		                                                          static_cast<float>(ring.argb >> 24)) >>
		                                         8) &
		                   0xFFu;
		// the colour as the creator left it (the light colour was fixed at creation, ecs::AddWaterRing)
		const uint32_t abgr = argb_colour::ToAbgr(ring.argb, alpha);
		// a flat sprite: a quad turned about Y (billboard::Horizontal), the z half size x the aspect
		billboard::Sprite sprite {
		    .position = ring.position,
		    .size = half,
		    .height = ring.aspect,
		    .angle = ring.angle,
		    .cell = frame_anim::SpriteCell(ring.cell), // fixed at creation
		    .horizontal = true,
		};
		const auto quad = billboard::Horizontal(sprite);
		for (const int k : billboard::k_SpriteTriangles)
		{
			const auto& p = quad.corners.at(static_cast<size_t>(k));
			const auto& uv = quad.uv.at(static_cast<size_t>(k));
			vertices.push_back({p.x, p.y, p.z, uv.x, uv.y, abgr});
		}
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
	const auto* program = _shaderManager->GetShader("WorldQuad");
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Texture));
	program->SetTextureSampler("s_alpha", 1, *textures.Handle(k_Alpha));
	bgfx::setVertexBuffer(0, &buffer);
	// smoke.raw in mode 13 (inferred): SRCALPHA / ONE, no Z write
	bgfx::setState(render_modes::State(render_modes::materials::k_SmokeAdditive));
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
}

namespace
{
/// A shadow blob's point: the local point `local` of bone `bone` of the drawn pose, through the drawn model matrix
/// (the blobs read the frame's skinned bone buffer). Shared by the villagers' feet and the animals' EBone points.
/// (approximate) The glm chain: the original's, BlobBones + BlobPointExact below, needs the frame's locals
glm::vec3 BlobPointApprox(const glm::mat4& model, const glm::mat4& bone, const glm::vec3& local)
{
	return glm::vec3(model * bone * glm::vec4(local, 1.0f));
}

/// The frame's skinned bone buffer, in clipping space: SkinBones of the frame's locals
/// (SkeletalAnimation::locals, SampleLocal's) under the root BoneRootToClip(object matrix, W); (approximate)
/// the object matrix is FromModel(DrawnModel), openblack's cells. Without a clip, the mesh's rest locals, as the
/// original skins them without an animation. (approximate) a clip of another skeleton also falls back to the rest
/// locals: the original skins any clip. Empty only for a mesh without bones
std::vector<affine::AffineMatrix> BlobBones(const glm::mat4& model, const ecs::components::SkeletalAnimation* animation,
                                            const L3DMesh& mesh, const affine::CameraMatrices& lh)
{
	std::vector<affine::AffineMatrix> skinned;
	const auto& parents = mesh.GetBoneParents();
	const bool clip = animation != nullptr && !animation->locals.empty() && animation->locals.size() == parents.size();
	const auto& source = clip ? animation->locals : mesh.GetBoneLocals();
	if (source.empty() || source.size() != parents.size())
	{
		return skinned;
	}
	std::vector<affine::AffineMatrix> locals;
	locals.reserve(source.size());
	for (const auto& local : source)
	{
		locals.push_back(affine::FromModel(local));
	}
	const auto root = affine::BoneRootToClip(affine::FromModel(model), lh.worldToClipping);
	affine::SkinBones(locals, parents, root, skinned);
	return skinned;
}

/// A blob's point of a skinned bone: M = Mul(bone, clippingToWorld) (the feet and the EBone points), for an EBone
/// point then MultiplyReversed(M, E), whose translation reads only E's position; the point is the translation row (its y then
/// goes onto the ground)
glm::vec3 BlobPointExact(const affine::AffineMatrix& bone, const affine::AffineMatrix& clippingToWorld,
                         const std::optional<glm::vec3>& eBone)
{
	auto m = affine::Mul(bone, clippingToWorld);
	if (eBone.has_value())
	{
		affine::AffineMatrix e;
		e.m[9] = eBone->x;
		e.m[10] = eBone->y;
		e.m[11] = eBone->z;
		m = affine::MultiplyReversed(m, e);
	}
	return {m.m[9], m.m[10], m.m[11]};
}
} // namespace

void Renderer::DrawHumanShadows(graphics::RenderPass viewId, const graphics::SuperVillagerFrame& superVillagers,
                                const affine::CameraMatrices& lh) const
{
	const auto& textures = Locator::resources::value().GetTextures();
	constexpr entt::id_type k_Texture = entt::hashed_string("raw/human_shadow").value();
	if (!textures.Contains(k_Texture) || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& island = Locator::terrainSystem::value();
	// each point on the ground + the blob lift (land_morph, algorithm D)
	const auto ground = land_morph::Altitude(island);
	const auto& meshes = Locator::resources::value().GetMeshes();
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	std::vector<Vertex> vertices;
	// the blobs' constants: half width U = 0.2 * norm(1, 0, -1), the light offset O = 2 * norm(1, 0, 1)
	const glm::vec3 u(0.14142136f, 0.0f, -0.14142136f);
	const glm::vec3 w = -u;
	const glm::vec3 o(1.41421356f, 0.0f, 1.41421356f);
	const auto addQuad = [&vertices, &u, &w](const glm::vec3& c, const glm::vec3& v) {
		// v0 = C - 0.02V + U, v1 = C - 0.02V + W, v2 = C + V + W, v3 = C + V + U; opaque at the feet
		const std::array<glm::vec3, 4> p = {c - 0.02f * v + u, c - 0.02f * v + w, c + v + w, c + v + u};
		const std::array<glm::vec2, 4> uv = {glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1)};
		const std::array<uint32_t, 4> colour = {0xFFFFFFFFu, 0xFFFFFFFFu, 0x00FFFFFFu, 0x00FFFFFFu};
		for (const int i : {0, 1, 2, 0, 2, 3})
		{
			vertices.push_back({p[i].x, p[i].y, p[i].z, uv[i].x, uv[i].y, colour[i]});
		}
	};
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const ecs::components::Villager, const ecs::components::Transform, const ecs::components::Mesh>(
	    [&](entt::entity entity, const ecs::components::Villager&, const ecs::components::Transform& transform,
	        const ecs::components::Mesh& mesh) {
		    // none for villagers in the water (y <= 0.2), nor for a SuperVillager (its temporary shadow turns the human
		    // shadow off)
		    if (transform.position.y <= 0.2f || !meshes.Contains(mesh.id) ||
		        std::ranges::find(superVillagers.notHumanShadowed, entity) != superVillagers.notHumanShadowed.end())
		    {
			    return;
		    }
		    const auto l3d = meshes.Handle(mesh.id);
		    // the feet of the drawn pose (ecs/Animations.h) where the villager is drawn (ecs/MobileDrawing.h)
		    const auto* animation = registry.TryGet<const ecs::components::SkeletalAnimation>(entity);
		    const auto& bones = animation != nullptr && animation->pose.size() == l3d->GetBoneMatrices().size()
		                            ? animation->pose
		                            : l3d->GetBoneMatrices();
		    if (bones.size() <= 21)
		    {
			    return;
		    }
		    // the two feet: bone matrix slots 21 and 18 (ends of the leg chains), on the ground + 0.2, of the drawn matrix
		    // (ecs::DrawnModel: in the physics the drawn pose between its last two turns) WITH the slope shear: the villager's
		    // draw applies MultiplyReversed(object matrix, S) with S = identity + the shear cells m1 / m7 before the 3D draw:
		    // the skin's root, and so the feet, are sheared
		    const auto model = ecs::DrawnModel(registry, entity, true);
		    // BlobPointExact on the skinned bones (the clip's locals, or the rest ones without a clip); BlobPointApprox, the
		    // glm chain (approximate), only for a mesh without bones
		    const auto skinned = BlobBones(model, animation, *l3d, lh);
		    const auto foot = [&](size_t bone) {
			    auto p = skinned.empty() ? BlobPointApprox(model, bones[bone], glm::vec3(0.0f))
			                             : BlobPointExact(skinned[bone], lh.clippingToWorld, std::nullopt);
			    p.y = land_morph::OnGround(ground, glm::vec2(p.x, p.z), land_morph::k_BlobLift);
			    return p;
		    };
		    const auto a = foot(21);
		    const auto b = foot(18);
		    // the light offset projected onto the land's plane: D = O s - ((O s) . n) n, n = the land normal there
		    const auto n = island.GetNormalAt(glm::vec2(transform.position.x, transform.position.z));
		    const auto os = o * transform.scale.x;
		    const auto d = os - glm::dot(os, n) * n;
		    addQuad(a, d + (b - a) * 0.5f);
		    addQuad(b, d + (a - b) * 0.5f);
	    },
	    entt::exclude<ecs::components::Unavailable>);
	// Animals (with the human shadow flag): the points of their mesh's EBone block, 2 or 4 quads. The original passes
	// the first quad of each pair V = D (it builds D + (P1 - P0) / 2 but hands over &D); the second gets D + (P0 - P1) / 2.
	registry.Each<const ecs::components::Animal, const ecs::components::Transform, const ecs::components::Mesh>(
	    [&](entt::entity entity, const ecs::components::Animal& animal, const ecs::components::Transform& transform,
	        const ecs::components::Mesh& mesh) {
		    if (!animal.humanShadowed || transform.position.y <= 0.2f || !meshes.Contains(mesh.id))
		    {
			    return;
		    }
		    const auto l3d = meshes.Handle(mesh.id);
		    const auto& points = l3d->GetBlobPoints();
		    // the blobs read the bone buffer filled with the frame's skinning: the animated pose, as the villagers above;
		    // the bind pose only without a matching SkeletalAnimation
		    const auto* animation = registry.TryGet<const ecs::components::SkeletalAnimation>(entity);
		    const auto& bones = animation != nullptr && animation->pose.size() == l3d->GetBoneMatrices().size()
		                            ? animation->pose
		                            : l3d->GetBoneMatrices();
		    if (points.empty())
		    {
			    return;
		    }
		    // the matrix the mesh is drawn with this frame (the root bone x object matrix x world-to-clip):
		    // ecs::DrawnModel (HandDrawPose in the render hand, then PhysicsDrawPose, DrawPosition, Transform) WITH the slope
		    // shear: the animal's draw applies MultiplyReversed(object matrix, S), which shears the matrix the skin's root and
		    // so the blobs come from
		    const auto model = ecs::DrawnModel(registry, entity, true);
		    // the land normal at the drawn matrix's translation x, z
		    const auto n = island.GetNormalAt(glm::vec2(model[3].x, model[3].z));
		    const auto os = o * transform.scale.x;
		    const auto d = os - glm::dot(os, n) * n;
		    // BlobPointExact on the skinned bones (the clip's locals, or the rest ones without a clip); BlobPointApprox, the
		    // glm chain (approximate), only for a point whose bone the mesh does not have
		    const auto skinned = BlobBones(model, animation, *l3d, lh);
		    std::array<glm::vec3, 4> p {};
		    for (size_t k = 0; k < points.size(); ++k)
		    {
			    const auto& [bone, position] = points[k];
			    const auto boneMatrix = bone < bones.size() ? bones[bone] : glm::mat4(1.0f);
			    p[k] = bone < skinned.size() ? BlobPointExact(skinned[bone], lh.clippingToWorld, position)
			                                 : BlobPointApprox(model, boneMatrix, position);
			    p[k].y = land_morph::OnGround(ground, glm::vec2(p[k].x, p[k].z), land_morph::k_BlobLift);
		    }
		    for (size_t k = 0; k + 1 < points.size(); k += 2)
		    {
			    addQuad(p[k], d);
			    addQuad(p[k + 1], d + (p[k] - p[k + 1]) * 0.5f);
		    }
	    },
	    entt::exclude<ecs::components::Unavailable>);
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
	program->SetTextureSampler("s_diffuse", 0, *textures.Handle(k_Texture));
	bgfx::setVertexBuffer(0, &buffer);
	// mode 6, no Z write, cull none
	bgfx::setState(render_modes::State(render_modes::Mode::AlphaTexturedAlphaNoZWrite));
	bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(program->GetRawHandle()));
}

void Renderer::PreDraw(const DrawSceneDesc& drawDesc) const noexcept
{
	// (openblack engine) what DrawScene wrote before it drew, in the same order: the sky type of the frame, the dome,
	// the land light table, the frame's light, the dynamic shadows and the clouds (CRT draws in UpdateClouds). Kept on
	// the logic side so that the draw only reads (docs/bw1-notes/engine-loop.md §6). Not with the full screen film
	// (DrawScene's first test), as before
	if (video::Get().CoversScreen() || video::GetFallingSpell().HidesWorld())
	{
		// no PSys list of an earlier frame (its pointers into the effects) survives a frame
		// without PreDraw
		ClearFrameParticles();
		return;
	}
	// The sky, once a frame from the landscape draw: it samples the sky type of the visual time, then rebuilds the land
	// light table (UpdateLandLight below) and advances the dome. Here once per DrawScene, so the reflection pass does not
	// advance the dome a second time.
	if (Locator::dayNightClock::has_value())
	{
		sky_type::SampleFrame(Locator::dayNightClock::value().Clock().GetVisualTime());
	}
	if (Locator::skySystem::has_value())
	{
		Locator::skySystem::value().UpdateDome();
	}
	UpdateLandLight();
	// Before the models, the landscape draw sets the engine's one light for this frame. The focus is the player hand's
	// model position, used even while the hand is hidden, and not the point under the cursor of GetPlayerHandPositions.
	// Known difference: with the cursor off the land the original still moves the hand along the mouse ray at its
	// distance from view (it keeps |camera - position| when the ray misses), while HandSystem::Place leaves the hand
	// where it was last placed, so at night the light stays there.
	bool placed = false;
	// Inside the temple the hand carries no light, and the light stays the default sun
	if (!InTemple() && Locator::dayNightClock::has_value() && Locator::handSystem::has_value() &&
	    Locator::entitiesRegistry::has_value() && drawDesc.camera != nullptr)
	{
		const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
		const auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(hand))
		{
			// the sky type of the visual time computed there, not the frame's sample (the same value: the sky samples the
			// same visual time right after)
			model_light::UpdateFrameLight(registry.Get<const ecs::components::Transform>(hand).position,
			                              drawDesc.camera->GetOrigin(),
			                              sky_type::At(Locator::dayNightClock::value().Clock().GetVisualTime()));
			placed = true;
		}
	}
	if (!placed)
	{
		// The original sets the light every frame; with no hand or camera to place it here, the light falls back to
		// its day branch, the default sun, instead of keeping an old frame's (inferred)
		model_light::SetLight(model_light::k_DefaultSun);
	}
	UpdateShadows(drawDesc);
	if (drawDesc.drawIsland)
	{
		UpdateClouds();
	}
	// Distance haze of this frame (graphics::haze::Frame and the "Fog" detail key), from the
	// land light table just built: the same for the reflection and the main pass
	_haze = IsLandLit() ? haze::Frame() : haze::Params {};
	_hazeUniforms = haze::Uniforms(_haze);
	// the clouds the main view draws (each is queued with the blended things); only the
	// main pass with the sky collects them (the reflection does not)
	_preClouds.clear();
	if (drawDesc.drawSky)
	{
		_preClouds = CollectClouds(*drawDesc.camera);
	}
	// The main pass's writes that the logic shares, in the order and under the tests the main pass made them (the
	// reflection pass makes none of them): with the entities, the creators of the surfaces of revolution that are
	// gone, then the mists (with the sky, with or without the entities: Mist.counter, mists::Submit's list), the
	// chimney smoke (with the sprites: after the clouds' and the night lights' CRT draws, as before), the rain
	// (rain::MarkDrawn) and the influence border's alpha
	if (drawDesc.drawEntities)
	{
		psys::surf_revol::PruneCreators();
	}
	_preMists.clear();
	if (drawDesc.drawSky)
	{
		_preMists = CollectMists(*drawDesc.camera);
	}
	_preSmoke.clear();
	if (drawDesc.drawEntities && drawDesc.drawSprites)
	{
		_preSmoke = CollectChimneySmoke(*drawDesc.camera);
	}
	_preRain.clear();
	_preInfluenceScroll.reset();
	if (drawDesc.drawEntities)
	{
		_preRain = CollectRain(*drawDesc.camera);
		UpdateInfluenceCurtain(*drawDesc.camera);
	}
	// the main pass's PSys collects, made here once with what the draw used to pass them (the desc's turn fraction,
	// camera origin and hand) and under the same tests (the main view; the Sorted pieces with
	// the entities). After PruneCreators above, as the draw's surf_revol::Collect was
	CollectFrameParticles(drawDesc);
}

void Renderer::ClearFrameParticles() const
{
	auto& frame = _frameParticles;
	frame.sorted = {};
	frame.queued.clear();
	frame.hand.clear();
	frame.surfaces.clear();
	frame.sortedPieces.Clear();
	frame.orderedPieces.Clear();
}

void Renderer::CollectFrameParticles(const DrawSceneDesc& drawDesc) const
{
	ClearFrameParticles();
	auto& frame = _frameParticles;
	// the draw collected them in the main view and with the entities only (DrawPass's `if (desc.drawEntities)`)
	if (drawDesc.viewId != graphics::RenderPass::Main || drawDesc.camera == nullptr || !drawDesc.drawEntities)
	{
		return;
	}
	const psys::manager::FrameInputs psysInputs {drawDesc.clock.turnFraction, drawDesc.camera->GetOrigin(),
	                                             drawDesc.hand.position};
	// the draw's order: the Sorted pieces (the exploded pieces' block), then the effects by path, the surfaces, the Queued
	// and Immediate pieces
	if (drawDesc.drawEntities)
	{
		psys::mesh_pieces::Build(frame.sortedPieces, psys::DrawPath::Sorted, psysInputs);
	}
	frame.sorted = psys::manager::CollectSorted(psysInputs);
	frame.queued = psys::manager::CollectQueued(psysInputs);
	frame.hand = psys::manager::HandEffects(psysInputs);
	frame.surfaces = psys::surf_revol::Collect(psysInputs);
	psys::mesh_pieces::Build(frame.orderedPieces, psys::DrawPath::Queued, psysInputs);
	psys::mesh_pieces::Build(frame.orderedPieces, psys::DrawPath::Immediate, psysInputs);
}

void Renderer::DrawScene(const DrawSceneDesc& drawDesc) const noexcept
{
	// With the full screen film, alpha == 1.0 and not the falling spell's film, the 3D world is not drawn; the film, the
	// script fade and the help system's bars still are, in the end of frame's order: bars, film, fade
	// (DrawFinishFrameOverlays). (approximate) the main view is cleared to openblack's colour as always (the original
	// does not clear: the film covers the screen).
	// In mode 2 (the falling spell, Video/FallingSpellVideo.h) no land is drawn either: the film with base alpha 0x50
	// and, not ported, the falling creature, its sprites and the liquid particles. (inferred) what lies under that 31 %
	// film in the original (no clear read): here openblack's clear colour
	if (video::Get().CoversScreen() || video::GetFallingSpell().HidesWorld())
	{
		bgfx::touch(static_cast<bgfx::ViewId>(graphics::RenderPass::Main));
		DrawFinishFrameOverlays(drawDesc.overlay, drawDesc.camera);
		return;
	}
	// the creatures drawn this frame, their shapes and their painted skins; none on a land without one
	CollectCreatures();
	// (Renderer::PreDraw: the sky type, the dome, the land light, the frame's light, the shadows and the clouds of this
	// frame were made just before this, in this order, then the haze, the clouds, the foliage, the surfaces' creators,
	// the mists, the chimney smoke, the rain and the influence border of the main view)
	{
		auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::FootprintPass);
		DrawStaticShadowPass(drawDesc);
	}
	// TODO(bwrsandman): Footprint framebuffer doesn't need to be updated each frame
	DrawFootprintPass(drawDesc);
	DrawLandAlphaPass(drawDesc);
	// The land from above for the temple's map, once a visit; nothing outside the temple
	DrawTempleMapPass(drawDesc);
	// Reflection Pass
	{
		auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::ReflectionPass);
		if (drawDesc.drawWater)
		{
			UpdateReflectionTarget();
			DrawSceneDesc drawPassDesc = drawDesc;
			// the mirrored land is drawn without the small bump map (sea_pass::SeaPassState::smallBump)
			if (!sea_pass::ForPass(graphics::RenderPass::Reflection).smallBump)
			{
				drawPassDesc.smallBumpMapStrength = 0.0f;
			}

			const auto& frameBuffer = Locator::oceanSystem::value().GetReflectionFramebuffer();
			auto reflectionCamera = drawDesc.camera->Reflect();

			drawPassDesc.viewId = graphics::RenderPass::Reflection;
			drawPassDesc.camera = reflectionCamera.get();
			drawPassDesc.frameBuffer = &frameBuffer;
			drawPassDesc.drawWater = false;
			drawPassDesc.drawBoundingBoxes = false;
			// "LandRef" detail key: without it only the sky is mirrored
			drawPassDesc.drawIsland = GetDetailLevel(Locator::config::value().detailLevel).landReflection;
			// "LandRef": the mirrored scene under the sea is only the sky and the land; models and sprites are not
			// reflected
			drawPassDesc.drawEntities = false;
			drawPassDesc.drawSprites = false;
			// (the culling of the mirrored camera comes from sea_pass::ForPass(Reflection))

			DrawPass(drawPassDesc);
		}
		else if (InTemple())
		{
			// The temple has no sea: the reflection is its main room's, under its floor
			DrawTempleReflection(drawDesc);
		}
	}

	// Main Draw Pass
	{
		auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::MainPass);
		DrawPass(drawDesc);
	}
	// The end of the frame: the "before" callbacks after the Z-sorter's flush, the spirits' trails (priority 100); the
	// help system's 3D draw: the spirits in the world; then the overlay spirits (priority 100) after the Z reset, before
	// the 2D rectangles (10000) and the texts (20000)
	DrawSpiritTrails(*drawDesc.camera, drawDesc.overlay);
	DrawSpirits(*drawDesc.camera, drawDesc.overlay, false);
	DrawSpirits(*drawDesc.camera, drawDesc.overlay, true);
	DrawFinishFrameOverlays(drawDesc.overlay, drawDesc.camera);
}

void Renderer::DrawFinishFrameOverlays(const OverlayFrame& overlay, const Camera* camera) const
{
	// The end of the frame, all in the Sequential ScreenOverlay view: the bars, then the last callbacks, among them the
	// video player's draw, so the film is drawn over the bars and fits between them at 100 % (FullScreenRect's letterbox
	// is barH at pct 1); the fade last, over the film
	// The input prompt icons' first draw, the end of the help system's 3D draw: in the 3D frame, before the end of the frame,
	// so under the bars and the overlay spirits: its own view FinishFrameIcons (before FinishFrame3D and ScreenOverlay)
	DrawInputPrompts(overlay, 0);
	if (video::GetFallingSpell().HidesWorld())
	{
		// mode 2: the falling spell draws the film itself (clearing the player's pending flag so its end-of-frame callback
		// draws nothing), before the end of the frame: then the Z-sorter (its sparks), its callback (its bursts), the bars
		// and the fade
		DrawVideoOverlay();
		DrawFallingSpellOverlay(camera);
		DrawScreenOverlay(overlay, false);
		DrawHelpText(overlay);        // HelpText's callback, as in the other branch
		DrawInputPrompts(overlay, 1); // the input prompt icons' second draw, at the end of HelpText's
		DrawScreenOverlay(overlay, true);
		return;
	}
	DrawScreenOverlay(overlay, false);
	// HelpText's callback (priority 20000) runs after the bars and before the film (one of the last callbacks)
	DrawHelpText(overlay);
	// (approximate) the did-you-know bubble: a Z object in the world in the original, here over the help text
	DrawDidYouKnowBubble(overlay);
	// the input prompt icons' second draw, the end of HelpText's (also on its early exit): every InputPromptIcon again
	DrawInputPrompts(overlay, 1);
	DrawVideoOverlay();
	DrawScreenOverlay(overlay, true);
}

void Renderer::DrawVideoOverlay() const
{
	const auto frame = video::Get().GetFrame();
	if (!frame || frame->width == 0 || frame->height == 0 || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const glm::u16vec2 size(static_cast<uint16_t>(frame->width), static_cast<uint16_t>(frame->height));
	if (!_videoTexture.IsValid() || _videoTextureSize != size)
	{
		_videoTexture.Reset(); // the old one before the new one is made
		// the tiles are clamped: the material's tiling bit is cleared.
		// (inferred) the driver's bilinear filter
		_videoTexture.Reset(bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::RGBA8,
		                                          BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP));
		bgfx::setName(_videoTexture.Get(), "Video");
		_videoTextureSize = size;
		_videoSerial.reset();
	}
	if (_videoSerial != frame->serial)
	{
		// uploaded after each picture decoded
		bgfx::updateTexture2D(_videoTexture.Get(), 0, 0, 0, 0, size.x, size.y,
		                      bgfx::copy(frame->rgba.data(), static_cast<uint32_t>(frame->rgba.size())));
		_videoSerial = frame->serial;
	}
	if (!_videoAlphaTexture.IsValid())
	{
		const uint8_t white = 0xFF;
		_videoAlphaTexture.Reset(bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::R8,
		                                               BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, bgfx::copy(&white, 1)));
	}

	// drawn to the screen at (colour, 0, bars, W + 1, H - 2 bars + 1) with W, H the screen size
	const int width = _resolution.x;
	const int height = _resolution.y;
	const auto rect = video::FullScreenRect(width, height);
	// the size of a texel on screen, w / film width and h / film height
	const float sx = static_cast<float>(rect.width) / static_cast<float>(frame->width);
	const float sy = static_cast<float>(rect.height) / static_cast<float>(frame->height);
	// the film's mosaic: ceil(size / 256) tiles a side
	constexpr uint32_t k_Tile = 0x100;
	const uint32_t tilesX = (frame->width + k_Tile - 1) / k_Tile;
	const uint32_t tilesY = (frame->height + k_Tile - 1) / k_Tile;
	const uint32_t abgr = argb_colour::ToAbgr(frame->colour); // the diffuse of the four vertices
	struct Vertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	std::vector<Vertex> vertices;
	vertices.reserve(static_cast<size_t>(tilesX) * tilesY * 6);
	const auto toClipX = [width](float px) { return 2.0f * px / static_cast<float>(width) - 1.0f; };
	const auto toClipY = [height](float py) { return 1.0f - 2.0f * py / static_cast<float>(height); };
	for (uint32_t ty = 0; ty < tilesY; ++ty)
	{
		// the last row has height & 0xFF texels (256 if that is 0)
		const uint32_t rows = (ty == tilesY - 1 && (frame->height & 0xFFu) != 0) ? (frame->height & 0xFFu) : k_Tile;
		for (uint32_t tx = 0; tx < tilesX; ++tx)
		{
			// the last column width & 0xFF
			const uint32_t columns = (tx == tilesX - 1 && (frame->width & 0xFFu) != 0) ? (frame->width & 0xFFu) : k_Tile;
			// x0 = x + tx * 256 * sx, x1 = x0 + columns * sx, likewise y (pre-transformed)
			const float x0 = static_cast<float>(rect.x) + static_cast<float>(tx * k_Tile) * sx;
			const float x1 = x0 + static_cast<float>(columns) * sx;
			const float y0 = static_cast<float>(rect.y) + static_cast<float>(ty * k_Tile) * sy;
			const float y1 = y0 + static_cast<float>(rows) * sy;
			// u, v from 1/512 to n/256 - 1/512 of the 256x256 tile, half a texel inside each edge;
			// the same texels of the one texture here (clamped, so the filter never reaches the next tile)
			const float u0 = (static_cast<float>(tx * k_Tile) + 0.5f) / static_cast<float>(frame->width);
			const float u1 = (static_cast<float>(tx * k_Tile + columns) - 0.5f) / static_cast<float>(frame->width);
			const float v0 = (static_cast<float>(ty * k_Tile) + 0.5f) / static_cast<float>(frame->height);
			const float v1 = (static_cast<float>(ty * k_Tile + rows) - 0.5f) / static_cast<float>(frame->height);
			const Vertex a {toClipX(x0), toClipY(y0), 0.5f, u0, v0, abgr};
			const Vertex b {toClipX(x1), toClipY(y0), 0.5f, u1, v0, abgr};
			const Vertex c {toClipX(x1), toClipY(y1), 0.5f, u1, v1, abgr};
			const Vertex d {toClipX(x0), toClipY(y1), 0.5f, u0, v1, abgr};
			for (const auto& vertex : {a, b, c, a, c, d})
			{
				vertices.push_back(vertex);
			}
		}
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
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	// mode 6 (MODULATE colour and alpha): colour = texture x diffuse, alpha = texture alpha (1) x diffuse alpha
	const auto* program = _shaderManager->GetShader("WorldQuad");
	program->SetTextureSampler("s_diffuse", 0, fromBgfx(_videoTexture.Get()));
	program->SetTextureSampler("s_alpha", 1, fromBgfx(_videoAlphaTexture.Get()));
	bgfx::setVertexBuffer(0, &buffer);
	// a mode 6 material, two-sided (CULLMODE NONE): SRCALPHA / INVSRCALPHA, no Z write; ZFUNC ALWAYS and
	// ZWRITEENABLE 0 from the screen draw's two false flags
	constexpr render_modes::Material k_VideoMaterial {render_modes::Mode::AlphaTexturedAlphaNoZWrite, render_modes::k_TwoSided};
	bgfx::setState(render_modes::State(k_VideoMaterial, {.zFunc = render_modes::ZFunc::Always, .zWrite = false}));
	bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
}

namespace
{
// atmos.raw / atmosa.raw (the atmosphere's) and mousehelp.raw / mousehelpa.raw (the mouse picture's), loaded by
// Game.cpp as raw/<file stem>
constexpr entt::id_type k_KeyOrMouseAtmos = entt::hashed_string("raw/ATMOS").value();
constexpr entt::id_type k_KeyOrMouseAtmosAlpha = entt::hashed_string("raw/ATMOSA").value();
constexpr entt::id_type k_KeyOrMouseMouse = entt::hashed_string("raw/mousehelp").value();
constexpr entt::id_type k_KeyOrMouseMouseAlpha = entt::hashed_string("raw/mousehelpa").value();

/// A text colour (argb) as GameFont's rgba
glm::vec4 TextRgba(uint32_t argb)
{
	return glm::vec4(static_cast<float>((argb >> 16) & 0xFFu), static_cast<float>((argb >> 8) & 0xFFu),
	                 static_cast<float>(argb & 0xFFu), static_cast<float>(argb >> 24)) /
	       255.0f;
}
} // namespace

void Renderer::DrawInputPrompts(const OverlayFrame& overlay, size_t pass) const
{
	// The input prompt icons' draw half, twice a frame as the original: pass 0 from the help system's 3D draw, pass 1 from
	// HelpText's end-of-frame callback, each icon with the alpha its own update left (input_prompt::Frame twice with the same
	// dt). Only the frame's copy is read (OverlayFrame::inputPrompts, newest first as the original's list); the hand's point
	// and the tick count are the copy's too. (not ported) an active setup box skips the draw: openblack has no setup box
	const auto& frame = overlay.inputPrompts;
	_keyOrMouseView = pass == 0 ? graphics::RenderPass::FinishFrameIcons : graphics::RenderPass::ScreenOverlay;
	_keyOrMouseHand = frame.hand;
	_keyOrMouseTicks = frame.tickCount;
	for (const auto& icon : frame.icons)
	{
		// c1.a = c2.a = int(a x 255); the panels at int(alpha8 x a)
		const float alpha = icon.alpha.at(pass);
		const auto byte = static_cast<uint32_t>(TruncateToInt(alpha * 255.0f)) & 0xFFu;
		const uint32_t c1 = (icon.c1 & 0x00FFFFFFu) | (byte << 24);
		const uint32_t c2 = (icon.c2 & 0x00FFFFFFu) | (byte << 24);
		// the row: a key's name for a key with no codes (animType -1, clickType 0), an int otherwise
		const KeyOrMouseRow row = icon.animType == -1 && icon.clickType == 0 ? KeyOrMouseRow(std::u16string_view(icon.keyName))
		                                                                     : KeyOrMouseRow(icon.row);
		DrawKeyOrMouse(icon.animType, icon.clickType, row, std::u16string_view(icon.text), icon.x, icon.y, icon.size,
		               icon.align, &c1, &c2, TruncateToInt(static_cast<float>(icon.alpha8) * alpha));
	}
}

void Renderer::AddScreenQuad(std::vector<SpiritQuadVertex>& out, float x0, float y0, float x1, float y1, float u0, float v0,
                             float u1, float v1, uint32_t argb) const
{
	// a pre-transformed textured rectangle in pixels, here straight to clip space. (approximate) the original box
	// draw's clip (+-0xA000) and inv_w 100.0 are left out
	const float width = _resolution.x;
	const float height = _resolution.y;
	const uint32_t abgr = argb_colour::ToAbgr(argb);
	const float l = 2.0f * x0 / width - 1.0f;
	const float r = 2.0f * x1 / width - 1.0f;
	const float t = 1.0f - 2.0f * y0 / height;
	const float b = 1.0f - 2.0f * y1 / height;
	const SpiritQuadVertex a {l, t, 0.5f, u0, v0, abgr};
	const SpiritQuadVertex bq {r, t, 0.5f, u1, v0, abgr};
	const SpiritQuadVertex c {r, b, 0.5f, u1, v1, abgr};
	const SpiritQuadVertex d {l, b, 0.5f, u0, v1, abgr};
	for (const auto& vertex : {a, bq, c, a, c, d})
	{
		out.push_back(vertex);
	}
}

void Renderer::AddKeyOrMousePanel(std::vector<SpiritQuadVertex>& out, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                                  uint32_t argb) const
{
	// A 9-slice of the soft blob (0, 0.25)-(0.24609375, 0.49609375) of atmos.raw, the draw alpha = argb >> 24 and the
	// rgb of argb. b = (y1 - y0) / 4; each corner is a 2b square centred on the rect's corner, the edges and the centre
	// between them
	const int32_t b = (y1 - y0) / 4;
	const std::array<float, 4> xs {static_cast<float>(x0 - b), static_cast<float>(x0 + b), static_cast<float>(x1 - b),
	                               static_cast<float>(x1 + b)};
	const std::array<float, 4> ys {static_cast<float>(y0 - b), static_cast<float>(y0 + b), static_cast<float>(y1 - b),
	                               static_cast<float>(y1 + b)};
	// the breaks: u 0, 0.078125, 0.16796875, 0.24609375; v 0.25, 0.328125, 0.41796875, 0.49609375
	constexpr std::array<float, 4> k_U {0.0f, 0.078125f, 0.16796875f, 0.24609375f};
	constexpr std::array<float, 4> k_V {0.25f, 0.328125f, 0.41796875f, 0.49609375f};
	for (size_t row = 0; row < 3; ++row)
	{
		for (size_t column = 0; column < 3; ++column)
		{
			AddScreenQuad(out, xs.at(column), ys.at(row), xs.at(column + 1), ys.at(row + 1), k_U.at(column), k_V.at(row),
			              k_U.at(column + 1), k_V.at(row + 1), argb);
		}
	}
}

void Renderer::DrawKeyOrMouseIn(graphics::RenderPass view, const InputPromptFrame& frame, int32_t animType, int32_t clickType,
                                KeyOrMouseRow row, std::optional<std::u16string_view> text, int32_t x, int32_t y, int32_t s,
                                uint32_t align, const uint32_t* c1, const uint32_t* c2, int32_t alpha8) const
{
	const auto savedView = _keyOrMouseView;
	const auto savedHand = _keyOrMouseHand;
	const auto savedTicks = _keyOrMouseTicks;
	_keyOrMouseView = view;
	_keyOrMouseHand = frame.hand;
	_keyOrMouseTicks = frame.tickCount;
	DrawKeyOrMouse(animType, clickType, row, text, x, y, s, align, c1, c2, alpha8);
	_keyOrMouseView = savedView;
	_keyOrMouseHand = savedHand;
	_keyOrMouseTicks = savedTicks;
}

void Renderer::DrawKeyOrMouse(int32_t animType, int32_t clickType, KeyOrMouseRow row, std::optional<std::u16string_view> text,
                              int32_t x, int32_t y, int32_t s, uint32_t align, const uint32_t* c1In, const uint32_t* c2In,
                              int32_t alpha8) const
{
	// A key or mouse button icon with its text, panels and arrows
	const GameFont* font = GameFontAt(help::TextFont::J0);
	// (openblack) no font or no view yet: nothing
	if (font == nullptr || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const int32_t width = _resolution.x;
	const int32_t height = _resolution.y;
	int32_t boxX = x;
	int32_t boxY = y;
	if ((align & 6) == 6)
	{
		// the hand's point (projected, the frame's copy) plus (x, y); Y clamped to [0, H - S], then X to [0, W - S]
		if (!_keyOrMouseHand)
		{
			return;
		}
		boxX = static_cast<int32_t>(_keyOrMouseHand->x) + x;
		boxY = static_cast<int32_t>(_keyOrMouseHand->y) + y;
		boxY = boxY < 0 ? 0 : boxY;
		boxY = boxY > height - s ? height - s : boxY;
		boxX = boxX < 0 ? 0 : boxX;
		boxX = boxX > width - s ? width - s : boxX;
	}
	else
	{
		boxY = y - s / 2; // y is the box's vertical centre
	}

	// a null c1 is white, a null c2 is c1; nothing when both alphas are under 4
	const uint32_t c1 = c1In != nullptr ? *c1In : 0xFFFFFFFFu;
	const uint32_t c2 = c2In != nullptr ? *c2In : c1;
	if ((c1 >> 24) < 4 && (c2 >> 24) < 4)
	{
		return;
	}

	// the icon's width. A key (animType -1): its text is the row's string when clickType is 0; with key codes the key
	// names (GetKeyNameText-style, inferred), "%s + %s" of the row's and clickType's when the row is not 0. Its width is
	// max(1.0, GetStringWidth(text, 0.4 S) + int(2S / 3))
	std::u16string keyText;
	float iconWidth = 0.0f;
	if (animType == -1)
	{
		if (clickType == 0)
		{
			if (const auto* name = std::get_if<std::u16string_view>(&row); name != nullptr)
			{
				keyText = *name;
			}
		}
		// (not ported) the key codes' names: no caller of openblack passes codes (input_prompt::ResolveAction
		// gives a name)
		iconWidth = std::max(1.0f, font->GetStringWidth(keyText, static_cast<float>(s) * 0.4f) + static_cast<float>(2 * s / 3));
	}
	else
	{
		// a mouse button: S wide, none for the row 3
		const auto* mouseRow = std::get_if<int32_t>(&row);
		iconWidth = (mouseRow != nullptr && *mouseRow == 3) ? 0.0f : static_cast<float>(s);
	}
	// keyW': + S / 2 for each stub 0x400 / 0x800
	float keyWidth = iconWidth;
	if ((align & 0x400) != 0)
	{
		keyWidth += static_cast<float>(s / 2);
	}
	if ((align & 0x800) != 0)
	{
		keyWidth += static_cast<float>(s / 2);
	}
	// textSize int(2S / 3); (pending) int(4S / 5) for languages that need bigger text: openblack has no language
	const float textSize = static_cast<float>(2 * s / 3);
	// total = textW + keyW' + 2.0; a null text gives textW 0 and total keyW'
	const std::u16string textString = text ? std::u16string(*text) : std::u16string();
	const float textWidth = text ? font->GetStringWidth(textString, textSize) : 0.0f;
	const float total = text ? textWidth + keyWidth + 2.0f : keyWidth;

	// the horizontal layout. align & 1 right-aligns; align & 2 picks the side, with the hysteresis _keyOrMouseSide: 1
	// past 2W / 3, 0 below W / 3; side 1 ends S / 2 right of X, side 0 starts S / 2 left of it
	bool rightAligned = (align & 1) != 0;
	if ((align & 2) != 0)
	{
		if (boxX > 2 * width / 3)
		{
			_keyOrMouseSide = 1;
		}
		if (boxX < width / 3)
		{
			_keyOrMouseSide = 0;
		}
		if (_keyOrMouseSide != 0)
		{
			boxX = TruncateToInt(static_cast<float>(boxX + s / 2) - total);
			rightAligned = true;
		}
		else
		{
			boxX -= s / 2;
			rightAligned = false;
		}
	}
	else if (rightAligned)
	{
		boxX = TruncateToInt(static_cast<float>(boxX) - total);
	}
	// the order: align & 0x10 puts the text first; with 0x20, the side / right-align flag instead
	bool textFirst = (align & 0x10) != 0;
	if ((align & 0x20) != 0)
	{
		textFirst = rightAligned;
	}
	int32_t iconX = textFirst ? TruncateToInt(static_cast<float>(boxX) + textWidth + 2.0f) : boxX;
	const int32_t textX = textFirst ? boxX : TruncateToInt(static_cast<float>(boxX) + keyWidth + 2.0f);
	if ((align & 0x400) != 0)
	{
		iconX += s / 2;
	}

	// every part is its own submit in the pass's Sequential view (FinishFrameIcons / ScreenOverlay), in the original's
	// order
	const auto viewId = _keyOrMouseView;
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(static_cast<bgfx::ViewId>(viewId), glm::value_ptr(identity), glm::value_ptr(identity));
	// the box draw has no depth test (ZFUNC ALWAYS), the Atmos materials have no Z write (modes 6 and 13)
	const uint64_t atmosState = render_modes::State(render_modes::materials::k_Atmos, {.zFunc = render_modes::ZFunc::Always});
	const uint64_t additiveState =
	    render_modes::State(render_modes::materials::k_AtmosAdditive, {.zFunc = render_modes::ZFunc::Always});

	// the panels, only with alpha8 != 0: AddKeyOrMousePanel in the additive material
	if (alpha8 != 0)
	{
		std::vector<SpiritQuadVertex> panels;
		// behind the icon, when its width is not 0: c2's rgb at alpha8
		if (iconWidth != 0.0f)
		{
			AddKeyOrMousePanel(panels, iconX, boxY, TruncateToInt(static_cast<float>(iconX) + iconWidth), boxY + s,
			                   (c2 & 0x00FFFFFFu) | (static_cast<uint32_t>(alpha8) << 24));
		}
		// the rest in c1's rgb at alpha8 / 3
		const uint32_t dim = (c1 & 0x00FFFFFFu) | (static_cast<uint32_t>(alpha8 / 3) << 24);
		// behind the text, when it is not empty: o = (S - textSize) * 0.5
		if (!textString.empty())
		{
			const float o = (static_cast<float>(s) - textSize) * 0.5f;
			AddKeyOrMousePanel(panels, textX, TruncateToInt(static_cast<float>(boxY) + o),
			                   TruncateToInt(static_cast<float>(textX) + textWidth),
			                   TruncateToInt(static_cast<float>(boxY + s) - o), dim);
		}
		// the arrow stubs: 0x400 left, 0x800 right, 0x100 up, 0x200 down
		if ((align & 0x400) != 0)
		{
			AddKeyOrMousePanel(panels, iconX - s / 2, boxY + s / 4, iconX, boxY + 3 * s / 4, dim);
		}
		if ((align & 0x800) != 0)
		{
			AddKeyOrMousePanel(panels, TruncateToInt(static_cast<float>(iconX) + iconWidth), boxY + s / 4,
			                   TruncateToInt(static_cast<float>(s / 2) + static_cast<float>(iconX) + iconWidth),
			                   boxY + 3 * s / 4, dim);
		}
		if ((align & 0x100) != 0)
		{
			AddKeyOrMousePanel(panels, iconX + s / 4, boxY - s / 2, iconX + 3 * s / 4, boxY, dim);
		}
		if ((align & 0x200) != 0)
		{
			AddKeyOrMousePanel(panels, iconX + s / 4, boxY + s, iconX + 3 * s / 4, boxY + s / 2 + s, dim);
		}
		SubmitSpiritQuads(viewId, panels, k_KeyOrMouseAtmos, k_KeyOrMouseAtmosAlpha, additiveState);
	}

	if (animType == -1)
	{
		// the key cap from iconX to int(iconX + iconW), Y to Y + S: a 3-slice in the atmos material, b = (y1 - y0) / 4
		// wide ends, white with the draw alpha = c2.a, v 0.001953125..0.248046875, u 0.501953125 / 0.564453125 /
		// 0.685546875 / 0.748046875
		const int32_t x0 = iconX;
		const int32_t y0 = boxY;
		const int32_t x1 = TruncateToInt(static_cast<float>(iconX) + iconWidth);
		const int32_t y1 = boxY + s;
		const int32_t b = (y1 - y0) / 4;
		const uint32_t capAlpha = c2 >> 24;
		const uint32_t white = 0x00FFFFFFu | (capAlpha << 24);
		constexpr float k_V0 = 0.001953125f;
		constexpr float k_V1 = 0.248046875f;
		std::vector<SpiritQuadVertex> cap;
		AddScreenQuad(cap, static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x0 + b), static_cast<float>(y1),
		              0.501953125f, k_V0, 0.564453125f, k_V1, white);
		AddScreenQuad(cap, static_cast<float>(x0 + b), static_cast<float>(y0), static_cast<float>(x1 - b),
		              static_cast<float>(y1), 0.564453125f, k_V0, 0.685546875f, k_V1, white);
		AddScreenQuad(cap, static_cast<float>(x1 - b), static_cast<float>(y0), static_cast<float>(x1), static_cast<float>(y1),
		              0.685546875f, k_V0, 0.748046875f, k_V1, white);
		SubmitSpiritQuads(viewId, cap, k_KeyOrMouseAtmos, k_KeyOrMouseAtmosAlpha, atmosState);
		// its text at (x0 + h / 5, y0 + h / 7), h = y1 - y0, size h x 1.0 x 0.4, black with the cap's alpha. (inferred,
		// static) checked on the original's code, not at run time
		std::vector<GameFont::Vertex> glyphs;
		const int32_t h = y1 - y0;
		font->AddText(glyphs, keyText, static_cast<float>(x0 + h / 5), static_cast<float>(y0 + h / 7),
		              static_cast<float>(h) * 1.0f * 0.4f, TextRgba(capAlpha << 24));
		SubmitScreenText(*font, glyphs, _keyOrMouseView);
	}
	else if (const auto* mouseRow = std::get_if<int32_t>(&row); mouseRow != nullptr && *mouseRow != 3)
	{
		DrawKeyOrMouseMouse(clickType, *mouseRow, animType, iconX, boxY, s, c2);
	}

	// the arrows: the atmos material, colour c2 with the draw alpha = c2.a, grown by g = S x 0.1111111 (S / 9) on
	// each side
	if ((align & 0xF00) != 0)
	{
		const float g = static_cast<float>(s) * 0.1111111f;
		const float fx = static_cast<float>(iconX);
		const float fy = static_cast<float>(boxY);
		std::vector<SpiritQuadVertex> arrows;
		const auto addArrow = [&](float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
			AddScreenQuad(arrows, static_cast<float>(TruncateToInt(x0)), static_cast<float>(TruncateToInt(y0)),
			              static_cast<float>(TruncateToInt(x1)), static_cast<float>(TruncateToInt(y1)), u0, v0, u1, v1, c2);
		};
		const float quarter = static_cast<float>(s / 4);
		const float threeQuarters = static_cast<float>(3 * s / 4);
		const float half = static_cast<float>(s / 2);
		if ((align & 0x400) != 0)
		{
			// left
			addArrow(fx - half - g, fy + quarter - g, fx + g, fy + threeQuarters + g, 0.75390625f, 0.12890625f, 0.87109375f,
			         0.24609375f);
		}
		if ((align & 0x800) != 0)
		{
			// right
			addArrow(fx - g + iconWidth, fy + quarter - g, fx + g + half + iconWidth, fy + threeQuarters + g, 0.87890625f,
			         0.00390625f, 0.99609375f, 0.12109375f);
		}
		if ((align & 0x100) != 0)
		{
			// up
			addArrow(fx + quarter - g, fy - half - g, fx + threeQuarters + g, fy + g, 0.75390625f, 0.00390625f, 0.87109375f,
			         0.12109375f);
		}
		if ((align & 0x200) != 0)
		{
			// down
			addArrow(fx + quarter - g, fy + static_cast<float>(s) - g, fx + threeQuarters + g,
			         fy + half + static_cast<float>(s) + g, 0.87890625f, 0.12890625f, 0.99609375f, 0.24609375f);
		}
		SubmitSpiritQuads(viewId, arrows, k_KeyOrMouseAtmos, k_KeyOrMouseAtmosAlpha, atmosState);
	}

	// the text, only when not null: drawn three times with j0 at textSize, y = Y + o with o a float (not floored);
	// black (c1's alpha) at (textX - 1, Y + o - 1) and (textX + 1, Y + o + 1), then c1 at (textX, Y + o). (not ported)
	// the text draw's fifth argument, 1.1 x the near plane
	if (text)
	{
		const float o = (static_cast<float>(s) - textSize) * 0.5f;
		const float textY = static_cast<float>(boxY) + o;
		const auto shadow = TextRgba(c1 & 0xFF000000u);
		std::vector<GameFont::Vertex> glyphs;
		font->AddText(glyphs, textString, static_cast<float>(textX - 1), textY - 1.0f, textSize, shadow);
		font->AddText(glyphs, textString, static_cast<float>(textX + 1), textY + 1.0f, textSize, shadow);
		font->AddText(glyphs, textString, static_cast<float>(textX), textY, textSize, TextRgba(c1));
		SubmitScreenText(*font, glyphs, _keyOrMouseView);
	}
}

void Renderer::DrawKeyOrMouseMouse(int32_t clickType, int32_t row, int32_t animType, int32_t x, int32_t y, int32_t s,
                                   uint32_t argb) const
{
	// An S x S quad of mousehelp.raw (256 x 256, 4 x 4 cells of 64 px) with mousehelpa.raw, material mode 5 (made on
	// the first draw)
	if (row == 3)
	{
		return;
	}
	// the lit state from the tick count: animType 1 (T / 100) % 5 < 2; 2 (T / 80) % 12 in {0, 1, 4, 5}; any other
	// always. The frame's game_clock::TickCount, read before the draw: both passes
	// use the same one (the original's two calls read it twice, the same but on a tick boundary)
	const uint32_t ticks = _keyOrMouseTicks;
	const int32_t d = static_cast<int32_t>((ticks / 100) % 5);
	bool lit = true;
	if (animType == 1)
	{
		lit = d < 2;
	}
	else if (animType == 2)
	{
		const uint32_t phase = (ticks / 80) % 12;
		lit = phase < 2 || (phase >= 4 && phase < 6);
	}
	// the cell by clickType, r the row; 1 and anything else
	// is the right button's cell mirrored, the left button
	int32_t cell = 0;
	bool mirrored = false;
	switch (clickType)
	{
	case 0:
		cell = 4 * row;
		break;
	case 2:
		cell = 4 * row + (lit ? 1 : 0);
		break;
	case 3:
		cell = 4 * row + (lit ? 2 : 0);
		break;
	case 4:
		cell = 4 * row + (lit ? 3 : 0);
		break;
	case 8:
		cell = d <= 3 ? 15 - d : 11; // the wheel down frames
		break;
	case 0x10:
		cell = d <= 3 ? 12 + d : 11; // the wheel up frames
		break;
	default:
		mirrored = true;
		cell = 4 * row + (lit ? 1 : 0);
		break;
	}
	// nothing for a negative cell or a colour of 0
	if (cell < 0 || argb == 0)
	{
		return;
	}
	// u = (c & 3) x 0.25, v = (c >> 2) x 0.25, mirrored: u + 0.25 to u; the vertices (x, y), (x + S, y), (x + S, y + S),
	// (x, y + S)
	const float u0 = static_cast<float>(cell & 3) * 0.25f + (mirrored ? 0.25f : 0.0f);
	const float u1 = u0 + (mirrored ? -0.25f : 0.25f);
	const float v0 = static_cast<float>(cell / 4) * 0.25f;
	const float v1 = v0 + 0.25f;
	std::vector<SpiritQuadVertex> quad;
	AddScreenQuad(quad, static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + s), static_cast<float>(y + s), u0,
	              v0, u1, v1, argb);
	// mode 5, tiling off, ZFUNC ALWAYS and ZWRITEENABLE 0 around the 2D draw
	constexpr render_modes::Material k_MouseHelp {render_modes::Mode::AlphaTexturedAlpha, 0};
	SubmitSpiritQuads(_keyOrMouseView, quad, k_KeyOrMouseMouse, k_KeyOrMouseMouseAlpha,
	                  render_modes::State(k_MouseHelp, {.zFunc = render_modes::ZFunc::Always, .zWrite = false}));
}

const GameFont* Renderer::GameFontAt(help::TextFont font) const
{
	// as HelpText loads them: j0, f1 if loaded, else j0, f3 if loaded, else j0
	const auto index = static_cast<size_t>(font);
	if (!_fontLoadTried.at(index))
	{
		_fontLoadTried.at(index) = true;
		static constexpr std::array<std::string_view, 3> k_Names {"j0.met", "f1.met", "f3.met"};
		auto base = Locator::filesystem::value().GetPath<filesystem::Path::Data>() / k_Names.at(index);
		_fonts.at(index) = resources::LoadGameFont(Locator::resources::value().GetFonts(), base.replace_extension());
	}
	if (_fonts.at(index))
	{
		return _fonts.at(index).get();
	}
	return font == help::TextFont::J0 ? nullptr : GameFontAt(help::TextFont::J0);
}

float Renderer::MeasureText(help::TextFont font, std::u16string_view text, float size) const noexcept
{
	// The string width in the font HelpText would draw it with. Logic side, read-only, no bgfx calls; GameFontAt's
	// first-use font load must move before the game or through gpu::Submit (pending)
	const GameFont* gameFont = GameFontAt(font);
	return gameFont != nullptr ? gameFont->GetStringWidth(text, size) : 0.0f;
}

void Renderer::SubmitScreenText(const GameFont& font, const std::vector<GameFont::Vertex>& glyphs,
                                graphics::RenderPass view) const
{
	if (glyphs.empty() || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const float width = _resolution.x;
	const float height = _resolution.y;
	const auto toClip = [width, height](float px, float py) {
		return glm::vec2(2.0f * px / width - 1.0f, 1.0f - 2.0f * py / height);
	};
	const auto viewId = static_cast<bgfx::ViewId>(view);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	struct TextVertex
	{
		float x, y, z, u, v;
		uint32_t abgr;
	};
	std::vector<TextVertex> vertices;
	vertices.reserve(glyphs.size());
	for (const auto& g : glyphs)
	{
		const auto p = toClip(g.x, g.y);
		vertices.push_back({p.x, p.y, 0.5f, g.u, g.v, g.abgr});
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
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(TextVertex));
	const auto* program = _shaderManager->GetShader("Text");
	program->SetTextureSampler("s_diffuse", 0, font.GetTexture());
	bgfx::setVertexBuffer(0, &buffer);
	// mode 16 (the font pages' material): SRCALPHA / INVSRCALPHA, no Z write, ZFUNC ALWAYS (no depth test); its alpha
	// test is fs_text's
	bgfx::setState(
	    render_modes::State(render_modes::Mode::TexturedChromaAlphaNoZWrite, {.zFunc = render_modes::ZFunc::Always}));
	bgfx::submit(viewId, toBgfx(program->GetRawHandle()));
}

void Renderer::DrawScreenOverlay(const OverlayFrame& overlay, bool drawFade) const
{
	if (_resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	// ScreenFade's colour and its bar (ScreenFade::LetterboxHeight), read before the draw (FillOverlayFrame)
	const uint32_t colour = overlay.fade.colour;
	const int width = _resolution.x;
	const int height = _resolution.y;
	const int bar = overlay.fade.barPixels;
	if (drawFade ? (colour >> 24) == 0 : bar == 0)
	{
		return;
	}
	std::vector<ScreenRectVertex> vertices;
	const auto addRect = [this, &vertices](int x0, int y0, int x1, int y1, uint32_t argb) {
		AddScreenRect(vertices, x0, y0, x1, y1, argb);
	};
	// the bars, 0xFF000000, at the top and the bottom
	const auto addBars = [&]() {
		if (bar > 0)
		{
			addRect(0, 0, width, bar, 0xFF000000u);
			addRect(0, height - bar, width, height, 0xFF000000u);
		}
	};
	if (drawFade)
	{
		// the fade: x 0..W-1, y h'..H-1-h' with h' = h ? h - 1 : 0, then the bars again so the fade never tints them
		const int inset = bar > 0 ? bar - 1 : 0;
		addRect(0, inset, width - 1, height - 1 - inset, colour);
	}
	addBars();
	SubmitScreenRects(vertices);
}

void Renderer::SubmitScreenRects(const std::vector<ScreenRectVertex>& vertices) const
{
	if (vertices.empty())
	{
		return;
	}
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), vertices.size() * sizeof(ScreenRectVertex));
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::ScreenOverlay);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	bgfx::setVertexBuffer(0, &buffer);
	// mode 1 (untextured, SRCALPHA / INVSRCALPHA), ZFUNC ALWAYS and ZWRITEENABLE 0 by hand (the 2D rectangle queue the
	// same)
	bgfx::setState(
	    render_modes::State(render_modes::Mode::SmoothAlpha, {.zFunc = render_modes::ZFunc::Always, .zWrite = false}));
	bgfx::submit(viewId, toBgfx(_shaderManager->GetShader("DebugLine")->GetRawHandle()));
}

void Renderer::AddScreenRect(std::vector<ScreenRectVertex>& out, int x0, int y0, int x1, int y1, uint32_t argb) const
{
	// pre-transformed rectangles in pixels (rhw 1), here straight to clip space
	const float width = _resolution.x;
	const float height = _resolution.y;
	const uint32_t abgr = argb_colour::ToAbgr(argb);
	const float l = 2.0f * static_cast<float>(x0) / width - 1.0f;
	const float r = 2.0f * static_cast<float>(x1) / width - 1.0f;
	const float t = 1.0f - 2.0f * static_cast<float>(y0) / height;
	const float b = 1.0f - 2.0f * static_cast<float>(y1) / height;
	for (const auto& [x, y] : {std::pair {l, t}, {r, t}, {r, b}, {l, t}, {r, b}, {l, b}})
	{
		out.push_back({x, y, 0.5f, abgr});
	}
}

void Renderer::DrawDidYouKnowBubble(const OverlayFrame& overlay) const
{
	if (!overlay.didYouKnow.active)
	{
		return;
	}
	// gatheringtext.raw in a mode 6 material, with its alpha gatheringtexta.raw
	constexpr entt::id_type k_Texture = entt::hashed_string("raw/gatheringtext").value();
	constexpr entt::id_type k_Alpha = entt::hashed_string("raw/gatheringtexta").value();
	const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA;
	// the frame's vertices are pixels: to clip space for the ScreenOverlay view, as AddScreenQuad does
	const auto toClip = [this](std::vector<SpiritQuadVertex> vertices) {
		for (auto& v : vertices)
		{
			v.x = 2.0f * v.x / _resolution.x - 1.0f;
			v.y = 1.0f - 2.0f * v.y / _resolution.y;
		}
		return vertices;
	};
	SubmitSpiritQuads(graphics::RenderPass::ScreenOverlay, toClip(overlay.didYouKnow.shape), k_Texture, k_Alpha, state);
	// the scroll arrows: boxes in the atmos material, yellow at the draw alpha
	std::vector<SpiritQuadVertex> arrows;
	for (const auto& arrow : overlay.didYouKnow.arrows)
	{
		AddScreenQuad(arrows, arrow.x0, arrow.y0, arrow.x1, arrow.y1, arrow.u0, arrow.v0, arrow.u1, arrow.v1,
		              (static_cast<uint32_t>(overlay.didYouKnow.arrowAlpha) << 24) | 0x00FFFF00u);
	}
	constexpr entt::id_type k_Atmos = entt::hashed_string("raw/ATMOS").value();
	constexpr entt::id_type k_AtmosAlpha = entt::hashed_string("raw/ATMOSA").value();
	SubmitSpiritQuads(graphics::RenderPass::ScreenOverlay, arrows, k_Atmos, k_AtmosAlpha, state);
	// the $g gestures: a mode 6 material of S_Gesture<page>.raw with its alpha
	constexpr std::array<std::pair<entt::id_type, entt::id_type>, 2> k_Gestures = {{
	    {entt::hashed_string("raw/S_Gesture0").value(), entt::hashed_string("raw/S_Gesture0a").value()},
	    {entt::hashed_string("raw/S_Gesture1").value(), entt::hashed_string("raw/S_Gesture1a").value()},
	}};
	for (size_t page = 0; page < k_Gestures.size(); ++page)
	{
		SubmitSpiritQuads(graphics::RenderPass::ScreenOverlay, toClip(overlay.didYouKnow.gestures.at(page)),
		                  k_Gestures.at(page).first, k_Gestures.at(page).second, state);
	}
	// the $m icons: DrawKeyOrMouse through DrawKeyOrMouseIn (the frame's hand and ticks)
	for (const auto& icon : overlay.didYouKnow.keyIcons)
	{
		const uint32_t c1 = (static_cast<uint32_t>(icon.alpha) << 24) | 0x00FFFFFFu;
		const KeyOrMouseRow row = icon.animType == -1 && icon.clickType == 0 ? KeyOrMouseRow(std::u16string_view(icon.keyName))
		                                                                     : KeyOrMouseRow(icon.row);
		DrawKeyOrMouseIn(graphics::RenderPass::ScreenOverlay, overlay.inputPrompts, icon.animType, icon.clickType, row,
		                 std::nullopt, icon.x, icon.y, icon.size, 1, &c1, nullptr, icon.alpha);
	}
	// the lines (their shadow and text), one submit per font as DrawHelpText
	if (const GameFont* font = GameFontAt(help::TextFont::J0); font != nullptr && !overlay.didYouKnow.lines.empty())
	{
		std::vector<GameFont::Vertex> glyphs;
		for (const auto& line : overlay.didYouKnow.lines)
		{
			const auto& run = line.run;
			const size_t first = glyphs.size();
			font->AddText(glyphs, run.text, run.x, run.y, run.size, glm::vec4(run.r, run.g, run.b, run.a) / 255.0f, run.clipTop,
			              run.clipBottom);
			// the text draw's two colours: the alpha from the line's top edge to its bottom edge
			for (size_t i = first; i < glyphs.size(); ++i)
			{
				const float t = std::clamp((glyphs[i].y - run.y) / run.size, 0.0f, 1.0f);
				const auto alphaByte = static_cast<uint32_t>(
				    static_cast<float>(run.a) + (static_cast<float>(line.alphaBottom) - static_cast<float>(run.a)) * t);
				glyphs[i].abgr = (glyphs[i].abgr & 0x00FFFFFFu) | (std::min(alphaByte, 255u) << 24);
			}
		}
		SubmitScreenText(*font, glyphs);
	}
}

void Renderer::DrawHelpText(const OverlayFrame& overlay) const
{
	// HelpText's draw (its game gate is not ported) with HelpTextDisplay::Layout's frame, laid out before the draw at
	// the same resolution (FillOverlayFrame in Game.cpp)
	const auto& helpText = overlay.helpText;
	if (!helpText.active || _resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	const auto& frame = helpText.text;

	// the box, {b, g, r of the box colour, a 0x80} in the 2D rectangle queue (flushed before this callback runs)
	if (frame.boxShown)
	{
		std::vector<ScreenRectVertex> box;
		AddScreenRect(box, frame.box.left, frame.box.top, frame.box.right, frame.box.bottom,
		              static_cast<uint32_t>(frame.boxAlpha) << 24);
		SubmitScreenRects(box);
	}
	// the dialogue texts: one text draw per word, here one submit per font
	for (const auto font : {help::TextFont::J0, help::TextFont::F1, help::TextFont::F3})
	{
		const GameFont* gameFont = GameFontAt(font);
		if (gameFont == nullptr)
		{
			continue;
		}
		std::vector<GameFont::Vertex> glyphs;
		for (const auto& run : frame.runs)
		{
			if (run.font == font)
			{
				gameFont->AddText(glyphs, run.text, run.x, run.y, run.size, glm::vec4(run.r, run.g, run.b, run.a) / 255.0f,
				                  run.clipTop, run.clipBottom);
			}
		}
		SubmitScreenText(*gameFont, glyphs);
	}
}

namespace
{
/// One entry of the frame's transparency queue (zsort::Queue): what DrawPass needs to draw it when the queue is
/// drained, in place of the original's object and callback. One of the indices is set; with none, an instance of the
/// models (a mesh flagged to be sorted; a fading one; the hand).
/// (approximate) the arrival order, which breaks ties between equal keys, is not the original's: here clouds, then the
/// models mesh by mesh (a map), fading ones, the PSys sprites, meshes, chains and Queued effects, sprites, mists,
/// smoke, rain, boat; there the order in which the frame queues them
/// (docs/bw1-notes/original-frame.md)
struct ZObject
{
	entt::id_type meshId {0};
	uint32_t index {0};
	bool morphWithTerrain {false};
	bool fading {false};
	entt::entity sprite {entt::null};
	int dust {-1}; ///< a puff of the pass's dust list (ecs::physics::Dust::Snapshot), drawn as the sprites
	/// a sprite of a Sorted effect (manager::SortedFrame::sprites)
	int psysSprite {-1};
	/// a mesh atom of a Sorted effect (RenderContext::psysAtoms), opaque or not
	int psysMesh {-1};
	/// a chain of a Sorted effect (manager::SortedFrame::chains)
	int psysChain {-1};
	/// a Queued effect (manager::CollectQueued), all of it, as one Z object
	int queuedEffect {-1};
	int mist {-1};   ///< an index of _frameMists
	int smoke {-1};  ///< an index of _frameSmoke (one Z object per chimney)
	int cloud {-1};  ///< a cloud of _clouds
	int rain {-1};   ///< an index of _frameRain (one per raining tile)
	int boat {-1};   ///< an index of _frameBoatSprites (one per sprite)
	int ripple {-1}; ///< an index of influence::Ripples()
	/// the intro light, DrawSceneDesc::overlay.introLight
	int introLight {-1};

	/// a model instance (meshId / index): none of the other kinds is set. The drain draws it with drawInstance, its
	/// shadows inside it (DrawShadowsOnObject); a new kind must be added here too
	[[nodiscard]] bool IsModel() const
	{
		return sprite == entt::null && dust < 0 && psysSprite < 0 && psysMesh < 0 && psysChain < 0 && queuedEffect < 0 &&
		       mist < 0 && smoke < 0 && cloud < 0 && rain < 0 && boat < 0 && ripple < 0 && introLight < 0;
	}
};

// The renderer draws the PSys effects by their draw path (manager::CollectSorted / CollectQueued / HandEffects and
// RenderContext::psysAtoms): with k_DrawByPath false the mesh atoms would be drawn by the old loops as well
static_assert(psys::manager::k_DrawByPath, "Renderer::DrawPass draws the PSys effects by path");
} // namespace

void Renderer::DrawPass(const DrawSceneDesc& desc) const
{
	// Inside the temple the pass draws the temple in place of the world (RendererTemple.cpp)
	if (InTemple())
	{
		DrawTemplePass(desc);
		return;
	}
	const auto& meshManager = Locator::resources::value().GetMeshes();
	auto& profiler = Locator::profiler::value();

	if (desc.frameBuffer != nullptr)
	{
		desc.frameBuffer->Bind(desc.viewId);
	}
	// This dummy draw call is here to make sure that view is cleared if no
	// other draw calls are submitted to view
	bgfx::touch(static_cast<bgfx::ViewId>(desc.viewId));

	_shaderManager->SetCamera(desc.viewId, *desc.camera);

	const auto* skyShader = _shaderManager->GetShader("Sky");
	const auto* terrainShader = _shaderManager->GetShader("Terrain");
	const auto* debugShader = _shaderManager->GetShader("DebugLine");
	const auto* spriteShader = _shaderManager->GetShader("Sprite");
	const auto* debugShaderInstanced = _shaderManager->GetShader("DebugLineInstanced");
	const auto* objectShaderInstanced = _shaderManager->GetShader("ObjectInstanced");

	// Distance haze of this frame (PreDraw)
	const glm::vec4 u_haze = _hazeUniforms[0];
	const glm::vec4 u_hazeColour = _hazeUniforms[1];

	// the frame's single queue of everything blended, begun at the start of the frame, filled while the main view is
	// drawn and drained once, far to near, after all of it (at the end of the frame)
	zsort::Queue<ZObject> sorted;
	sorted.Begin();

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawSky
		                                                                          : Profiler::Stage::MainPassDrawSky);
		if (desc.drawSky)
		{
			const auto modelMatrix = glm::mat4(1.0f);
			// x unused: the sky type is blended into the dome textures by Sky::UpdateDome (sky_type::DomeBlend)
			const glm::vec4 u_typeAlignment = {0.0f, _skyAlignment.Get() + 1.0f, 0.0f, 0.0f};

			skyShader->SetTextureSampler("s_diffuse", 0, Locator::skySystem::value().GetTexture());
			skyShader->SetUniformValue("u_typeAlignment", &u_typeAlignment);

			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = desc.viewId;
			submitDesc.program = skyShader;
			// (inferred) the sky meshes' own modes, culled as a whole (flipped by the mirrored camera)
			submitDesc.options = {.cull = sea_pass::ForPass(desc.viewId).FaceCull(sea_pass::Surface::Sky, false, false),
			                      .writeAlpha = true,
			                      .msaa = true};
			submitDesc.modelMatrices = &modelMatrix;
			submitDesc.matrixCount = 1;
			submitDesc.isSky = true;

			DrawMesh(Locator::skySystem::value().GetMesh(), submitDesc, 0);
			if (desc.viewId == graphics::RenderPass::Main)
			{
				DrawMoon(desc.viewId, *desc.camera);
				DrawSun(desc.viewId, *desc.camera, false, desc.clock.frameGameMs);
				// each cloud is queued with the blended things of the frame (it used to be sorted on its own and drawn after
				// all of them)
				for (const auto& [key, index] : _preClouds) // collected by PreDraw
				{
					sorted.Submit({.cloud = static_cast<int>(index)}, key);
				}
			}
			else if (desc.viewId == graphics::RenderPass::Reflection)
			{
				// the moon is reflected (its halo and DrawUnderWater; mirrored: sea_pass::ForPass), the sun is not
				// (it is drawn once)
				DrawMoon(desc.viewId, *desc.camera);
			}
		}
	}

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawWater
		                                                                          : Profiler::Stage::MainPassDrawWater);
		if (desc.drawWater)
		{
			// RendererSea.cpp
			DrawSea(desc);
		}
	}

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawIsland
		                                                                          : Profiler::Stage::MainPassDrawIsland);
		if (desc.drawIsland)
		{
			auto& island = Locator::terrainSystem::value();
			auto islandExtent = glm::vec4(island.GetExtent().minimum, island.GetExtent().maximum);

			// Small bump fade line: the plane perpendicular to the camera forward, 50 units ahead, meets
			// the horizontal plane y = min(camera y, 0.67 * 165); the detail is full up to 20 units before that line and
			// gone 20 units past it.
			const auto cameraOrigin = desc.camera->GetOrigin();
			const auto cameraForward = desc.camera->GetForward();
			const glm::vec2 forwardXZ(cameraForward.x, cameraForward.z);
			const float forwardLength = std::max(glm::length(forwardXZ), 1e-4f);
			const float lineDistance =
			    (50.0f + cameraForward.y * std::max(0.0f, cameraOrigin.y - 0.67f * 165.0f)) / forwardLength;
			const glm::vec4 u_smallBumpLine = {cameraOrigin.x, cameraOrigin.z, forwardXZ / forwardLength};
			// x unused: the sky type reaches the land through the land light table and the haze (vs_terrain)
			const glm::vec4 u_skyAndBump = {0.0f, desc.bumpMapStrength, desc.smallBumpMapStrength, lineDistance};

			terrainShader->SetTextureSampler("s0_materials", 0, island.GetAlbedoArray());
			terrainShader->SetTextureSampler("s1_bump", 1, island.GetBump());
			terrainShader->SetTextureSampler("s2_smallBump", 2, island.GetSmallBump());
			terrainShader->SetTextureSampler("s3_footprints", 3, island.GetFootprintFramebuffer().GetColorAttachment());
			terrainShader->SetTextureSampler("s_landLightTable", 4, fromBgfx(_landLightTexture.Get())); // vs
			const auto cellMapSize = glm::vec2(island.GetCellMap().GetResolution());
			if (_landCellsTexture.IsValid() && glm::vec2(_landCellsSize) == cellMapSize)
			{
				terrainShader->SetTextureSampler("s_landCells", 6, fromBgfx(_landCellsTexture.Get())); // vs
			}
			else
			{
				terrainShader->SetTextureSampler("s_landCells", 6, island.GetCellMap()); // vs
			}
			const glm::vec4 u_cellMap = {island.GetExtent().minimum, cellMapSize};
			terrainShader->SetUniformValue("u_cellMap", &u_cellMap); // vs
			terrainShader->SetTextureSampler("s5_staticShadow", 5, island.GetStaticShadowFramebuffer().GetColorAttachment());
			terrainShader->SetTextureSampler("s8_landAlpha", 8, island.GetLandAlphaFramebuffer().GetColorAttachment());
			// x: 1 = the colour comes from the block texture (the original's, BlockTexture.h); 0 = the per-vertex materials
			// (an island without a block texture)
			glm::vec4 u_blockTexture {0.0f};
			if (const auto* blockTexture = island.GetBlockTexture(); blockTexture != nullptr)
			{
				terrainShader->SetTextureSampler("s10_blockTexture", 10, *blockTexture);
				u_blockTexture.x = 1.0f;
				u_blockTexture.y = 1.0f; // its alpha is the coast alpha
			}
			else
			{
				terrainShader->SetTextureSampler("s10_blockTexture", 10, island.GetLandAlphaFramebuffer().GetColorAttachment());
			}
			terrainShader->SetUniformValue("u_blockTexture", &u_blockTexture);
			terrainShader->SetUniformValue("u_skyAndBump", &u_skyAndBump);
			terrainShader->SetUniformValue("u_smallBumpLine", &u_smallBumpLine);
			terrainShader->SetUniformValue("u_haze", &u_haze);
			terrainShader->SetUniformValue("u_hazeColour", &u_hazeColour);
			// Static shadows darken the block texture by up to x0.5 (x0.75 with 128 px textures)
			const float staticShadowStrength =
			    GetDetailLevel(Locator::config::value().detailLevel).useHighTexture ? 0.5f : 0.25f;
			// x: the light table >> 1 of the mirrored land (sea_pass::SeaPassState::landLightScale)
			const glm::vec4 u_terrainPass = {sea_pass::ForPass(desc.viewId).landLightScale, 0.0f, staticShadowStrength, 0.0f};
			terrainShader->SetUniformValue("u_terrainPass", &u_terrainPass);
			terrainShader->SetUniformValue("u_islandExtent", &islandExtent);

			// clang-format off
			// fs_terrain writes the land and small bump passes premultiplied (the coast alpha does not fade the small
			// bump, as in the original); Z is written even where the land is transparent (render mode 14)
			// (the land blocks' material: the function of mode 5)
			const auto defaultState = render_modes::State(render_modes::Mode::Landscape,
				{.writeAlpha = true, .msaa = true, .premultiplied = true});

			constexpr auto discard = 0u
				| BGFX_DISCARD_INSTANCE_DATA
				| BGFX_DISCARD_INDEX_BUFFER
				| BGFX_DISCARD_TRANSFORM
				| BGFX_DISCARD_VERTEX_STREAMS
				| BGFX_DISCARD_STATE
			;
			// clang-format on

			// The mirrored land is drawn with ZWRITEENABLE off, so it
			// hides nothing drawn after it under the sea (the hand's, the objects' and the boats' reflections, the fish);
			// here it wrote Z, and a mirrored hill hid the reflections behind it.
			const auto seaPass = sea_pass::ForPass(desc.viewId);
			const auto state = seaPass.landWriteZ ? defaultState : defaultState & ~BGFX_STATE_WRITE_Z;
			// openblack's blocks wind the other way round from the models (sea_pass::Surface::Land)
			const uint64_t landCull = seaPass.FaceCull(sea_pass::Surface::Land, false, false) == render_modes::Cull::Ccw
			                              ? BGFX_STATE_CULL_CCW
			                              : BGFX_STATE_CULL_CW;

			// Block order of both land passes: the list the island builds before drawing, ascending by the distance from the
			// camera to the block centre (x + 80, 0, z + 80) with LandRef on: nearest first. Without Z, it is what decides
			// which mirrored hill covers which.
			const auto& blocks = island.GetBlocks();
			const auto& viewOrigin = desc.camera->GetOrigin();
			std::vector<std::pair<float, size_t>> blockOrder;
			blockOrder.reserve(blocks.size());
			for (size_t i = 0; i < blocks.size(); ++i)
			{
				const glm::vec2 centre = blocks[i].GetMapPosition() + glm::vec2(80.0f);
				blockOrder.emplace_back(glm::length(glm::vec3(centre.x - viewOrigin.x, viewOrigin.y, centre.y - viewOrigin.z)),
				                        i);
			}
			std::stable_sort(blockOrder.begin(), blockOrder.end(),
			                 [](const auto& a, const auto& b) { return a.first < b.first; });

			// the block's haze class from its box (graphics::haze::BlockClassOf), with the LandRef detail (inferred)
			const auto view = desc.camera->GetViewMatrix(Camera::Interpolation::Current);
			const bool landRef = GetDetailLevel(Locator::config::value().detailLevel).landReflection;
			for (const auto& [distance, blockIndex] : blockOrder)
			{
				const auto& block = blocks[blockIndex];
				// pack uniforms
				const glm::vec4 mapPositionAndSize = glm::vec4(block.GetMapPosition(), 160.0f, 160.0f);
				terrainShader->SetUniformValue("u_blockPositionAndSize", &mapPositionAndSize);
				const glm::vec4 u_hazeBlock = {static_cast<float>(haze::BlockClassOf(_haze, view, block, landRef)), 0.0f, 0.0f,
				                               0.0f};
				terrainShader->SetUniformValue("u_hazeBlock", &u_hazeBlock);

				block.GetMesh().GetVertexBuffer().Bind();

				bgfx::setState(state | landCull, 0);
				bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(terrainShader->GetRawHandle()), 0, discard);
				// the shadows over the block just drawn, only in the main land (the mirrored one draws none)
				if (desc.viewId == graphics::RenderPass::Main && desc.drawEntities)
				{
					DrawLandShadows(desc.viewId, block, landCull);
				}
			}
			bgfx::discard(BGFX_DISCARD_BINDINGS);
		}
	}

	bool mistsSorted = false; ///< the mists went through the back-to-front list of the blended models
	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawModels
		                                                                          : Profiler::Stage::MainPassDrawModels);
		// The original's "underwater" stage: the hand mirrored in the sea, unlit in colour 0x65A0A0A0 with no specular, only
		// its part above the water (the hand's DrawUnderWater with its own bones; sea_pass::UnderWater). Around it one of
		// the hand object's flags is cleared and put back afterwards: (not ported) what that flag does in this draw is not
		// identified (the under-water draw has no test of it)
		if (!desc.drawEntities && desc.viewId == graphics::RenderPass::Reflection)
		{
			const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
			const auto handDesc = renderCtx.instancedDrawDescs.find(ecs::components::Hand::k_MeshId);
			const auto* bones = Locator::handSystem::has_value() ? Locator::handSystem::value().GetBoneMatrices() : nullptr;
			if (handDesc != renderCtx.instancedDrawDescs.end() && bones != nullptr)
			{
				const auto mesh = meshManager.Handle(ecs::components::Hand::k_MeshId);
				if (mesh->GetBoneMatrices().size() == bones->size())
				{
					DrawUnderWater(desc.viewId, *mesh,
					               std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer,
					                                                        handDesc->second.offset, handDesc->second.count),
					               bones->data(), static_cast<uint8_t>(bones->size()), false,
					               sea_pass::UnderWater(sea_pass::k_HandColour, sea_pass::k_HandSpecular));
				}
			}
			DrawObjectReflections(desc.viewId);
			// TODO(sea_pass): the creature's DrawUnderWater goes here, after the held and physics objects, in
			// sea_pass::k_CreatureColour / k_CreatureSpecular, only while the tests of sea_pass::k_CreatureMaxBlockDistance /
			// k_CreatureMaxY / k_CreatureMaxA0 pass. There is no creature in openblack yet
			DrawBoatReflection(desc.viewId);
		}
		if (desc.viewId == graphics::RenderPass::Reflection)
		{
			// The parts under the water go into the frame before the sea, over the mirrored
			// land. Here that frame is the reflection target, drawn with the mirrored camera, so they are mirrored too.
			// the sharks (and whatever else is cut by the plane), then the fish (4d-4e)
			// the sharks
			DrawCutBelowWater(desc.viewId);
			// each shoal of a bait, then its net under the net's own plane (sea_pass::k_NetPlane, put back afterwards)
			DrawFishShoals(desc.viewId);
			DrawFishPlots(desc.viewId, sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_NetPlane));
			// the swimming SuperVillagers, after the fish and the nets and before the hand's glow: each SuperVillager whose
			// animation is "M_P_Swim2" (the frame's SuperVillagerFrame::swimmers, filled before the draw), after its own draw
			// and shadow: the plane sea_pass::k_SwimPlane, colour k_SwimmerColour / k_SwimmerSpecular, DrawCutByPlane, then the
			// default plane again. The swim rings every 1000 ms are made by ecs::super_villager::Update (ecs::AddWaterRing)
			for (const auto swimmer : desc.superVillagers.swimmers)
			{
				DrawCutByPlane(desc.viewId, swimmer, sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_SwimPlane),
				               sea_pass::k_SwimmerColour, sea_pass::k_SwimmerSpecular);
			}
			// the hand's glow on the water, the last thing before the sea
			DrawHandWaterGlow(desc.viewId);
		}
		// The screen-facing sprite (billboard::Screen) on the GPU: vs_sprite adds u_invView x (model x (x, y, 0,
		// 0)) to the model's translation on the plane -1..1 with v = 0 at the top, so the half width / half height are
		// the Transform's scale x / y. components::Sprite has no angle nor origin (every user, NightLights, FireFlies,
		// Dust, HandEffects, Glow, CameraBookmark, HandDebugHooks, gives an identity rotation), so it is Screen with angle
		// 0 and ox = oy = 0. The sprite's near test is made here on the CPU
		const auto spriteFrame = billboard::CameraFrame::From(*desc.camera);
		const auto drawSprite = [this, &spriteShader, &spriteFrame](const ecs::components::Sprite& sprite,
		                                                            const ecs::components::Transform& transform,
		                                                            RenderPass viewId) {
			if (!billboard::InFrontOfNear(transform.position, spriteFrame))
			{
				return;
			}
			const glm::mat4 modelMatrix = billboard::ScreenSpriteModel(transform.position, glm::vec2(transform.scale), 0.0f);

			glm::vec4 u_sampleRect(sprite.uvExtent, sprite.uvMin);

			bgfx::setTransform(glm::value_ptr(modelMatrix));
			spriteShader->SetUniformValue("u_sampleRect", glm::value_ptr(u_sampleRect));
			spriteShader->SetUniformValue("u_tint", glm::value_ptr(sprite.tint));
			spriteShader->SetTextureSampler("s_diffuse", 0, sprite.texture);

			_plane->GetVertexBuffer().Bind();

			// mode 13, or mode 6 with the tint premultiplied (fs_sprite) (inferred: the materials of the sprites' owners)
			bgfx::setState(render_modes::State(sprite.additive ? render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite
			                                                   : render_modes::Mode::AlphaTexturedAlphaNoZWrite,
			                                   {.writeAlpha = true, .premultiplied = true}));

			bgfx::submit(static_cast<bgfx::ViewId>(viewId), toBgfx(spriteShader->GetRawHandle()));
		};
		// the physics' dust puffs: a list taken from the game (ecs::physics::Dust::Snapshot), each drawn as the
		// components::Sprite + Transform it was (identity rotation, the half size in x / y, normal blending)
		ecs::physics::Dust::Snapshot(_frameDust);
		const auto drawDust = [&drawSprite](const ecs::physics::DustParticleDraw& puff, RenderPass viewId) {
			const ecs::components::Sprite sprite {puff.texture, puff.uvMin, puff.uvExtent, puff.tint, false};
			const ecs::components::Transform transform {puff.position, glm::mat3(1.0f), glm::vec3(puff.halfSize)};
			drawSprite(sprite, transform, viewId);
		};
		// the sprites also go to the Z-sorter: in the main pass they are sorted with the blended models
		const bool spritesSorted = desc.drawEntities && desc.drawSprites && desc.viewId == graphics::RenderPass::Main;
		// every mist on screen goes to the same Z-sorter (key |pos - camera|^2)
		mistsSorted = desc.drawEntities && desc.drawSky && desc.viewId == graphics::RenderPass::Main;

		if (desc.drawEntities)
		{
			if (desc.viewId == graphics::RenderPass::Main)
			{
				_shaderManager->SetCamera(graphics::RenderPass::MainBlended, *desc.camera);
			}
			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = desc.viewId;
			submitDesc.program = objectShaderInstanced;
			submitDesc.options = render_modes::k_ModelPass;
			const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
			// the objects under a shadow that falls on objects: each one gets it right after its own draw
			// (RendererShadows.cpp: the tail loop of the objects' Draw)
			CollectShadowReceivers(desc.viewId == graphics::RenderPass::Main);

			if (desc.viewId == graphics::RenderPass::Main)
			{
				DrawWaterRings(desc.viewId);
				DrawHumanShadows(desc.viewId, desc.superVillagers, billboard::CameraFrame::From(*desc.camera).clipMatrices);
				// the nets' part over the water, with the default plane
				DrawFishPlots(desc.viewId, sea_pass::Kept(sea_pass::Mechanism::CutByPlane, sea_pass::k_DefaultPlane));
			}
			const auto setMatrices = [&submitDesc](entt::id_type meshId, const L3DMesh& mesh) {
				const auto* handBones =
				    meshId == ecs::components::Hand::k_MeshId ? Locator::handSystem::value().GetBoneMatrices() : nullptr;
				if (mesh.IsBoned() && handBones != nullptr && handBones->size() == mesh.GetBoneMatrices().size())
				{
					// Player hand: animated pose from hh.HBN
					submitDesc.modelMatrices = handBones->data();
					submitDesc.matrixCount = static_cast<uint8_t>(handBones->size());
				}
				else if (mesh.IsBoned())
				{
					submitDesc.modelMatrices = mesh.GetBoneMatrices().data();
					submitDesc.matrixCount = static_cast<uint8_t>(mesh.GetBoneMatrices().size());
					// TODO(bwrsandman): Get animation frame instead of default
				}
				else
				{
					const static auto identity = glm::mat4(1.0f);
					submitDesc.modelMatrices = &identity;
					submitDesc.matrixCount = 1;
				}
			};
			// An object (static, animated or morphing) is queued only by its Z-sort flag: the whole object goes to the
			// Z-sorter, else it is drawn at once, blended primitives and all. The flag comes from the mesh's flag 0x200
			// (L3DMesh::IsZSorted); the one-shot orb forces it. The global alpha's flag is not looked at here (only one other
			// object class queues by it), so a fading object whose mesh lacks 0x200 is drawn at once through the alternate
			// mode table. (inferred) that every model instance is drawn this way and none is of that other class; in the main
			// view only, the queue's (the reflection draws everything at once)
			const bool sortBlended = desc.viewId == graphics::RenderPass::Main;
			const auto cameraOrigin = desc.camera->GetOrigin();
			const auto opaqueOptions = submitDesc.options;

			// the poses of the animated boned meshes (ecs/Animations.h), by instance; the PSys mesh atoms' too
			const auto poses = ecs::PosesByInstance(renderCtx);
			// the sharks (components::CutByPlane::drawAbove): their owner draws them cut by the water instead
			const auto cutAbove =
			    desc.viewId == graphics::RenderPass::Main ? CutAboveInstances() : std::unordered_set<uint32_t>();

			// Instance meshes, one range of a mesh at a time: forceQueued for the held objects' (sortedOpaqueDrawDescs)
			const auto drawRange = [&](entt::id_type meshId, const auto& placers, bool forceQueued) {
				if (!meshManager.Contains(meshId)) // (openblack guard) a mesh that did not load; never in the original
				{
					return;
				}
				auto mesh = meshManager.Handle(meshId);
				// a SuperVillager's HD body and its eyes are drawn with the light at the default sun
				// (SuperVillagerFrame::litByDefaultSun), not the frame's light
				std::optional<model_light::ScopedLight> superVillagerSun;
				if (desc.superVillagers.litByDefaultSun.contains(meshId))
				{
					superVillagerSun.emplace(model_light::k_DefaultSun);
				}

				submitDesc.instanceDesc =
				    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, placers.offset, placers.count);
				setMatrices(meshId, *mesh);
				submitDesc.isSky = false;
				submitDesc.lightBoost = meshId == ecs::components::Hand::k_MeshId ? 1.5f : 1.0f;
				submitDesc.noHaze = meshId == ecs::components::Hand::k_MeshId;
				ApplyLandLightMode(renderCtx, meshId, submitDesc);
				submitDesc.morphWithTerrain = placers.morphWithTerrain;
				submitDesc.program = land_morph::ObjectProgram(*_shaderManager, submitDesc.morphWithTerrain);
				submitDesc.blendFilter = 0;

				// the hand always queues the whole hand (no test); any other model whole by its mesh's flag 0x200. In the main
				// view a queued one is drawn only from the queue, opaque primitives included; the others at once with their
				// blended primitives
				const bool hand = meshId == ecs::components::Hand::k_MeshId;
				// and a held object by its Z-sort flag (components::NeedsSorting, the range drawn with forceQueued)
				const bool queued = sortBlended && (forceQueued || hand || mesh->IsZSorted());
				// TODO(bwrsandman): choose the correct LOD
				// a creature too, posed or not: its body is drawn with its own shape and skins
				if (!queued && mesh->IsBoned() &&
				    (ecs::HasPose(poses, placers.offset, placers.count) || HasCreature(placers.offset, placers.count)))
				{
					// animated (ecs/Animations.h): each instance on its own, with its pose. Only the ones in the view: every
					// draw copies the bones into the backend's per-frame uniform buffer (see vs_object.sc)
					const auto viewProjection = desc.camera->GetViewProjectionMatrix();
					const auto box = mesh->GetBoundingBox();
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						const auto& model = renderCtx.instanceUniforms[placers.offset + i];
						if (cutAbove.contains(placers.offset + i))
						{
							continue;
						}
						if (!BoxInView(viewProjection, box, model))
						{
							continue;
						}
						submitDesc.instanceDesc =
						    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, placers.offset + i, 1);
						setMatrices(meshId, *mesh);
						ecs::UsePose(poses, placers.offset + i, *mesh, submitDesc.modelMatrices, submitDesc.matrixCount);
						const auto* creature = creature_draw::Find(_frameCreatures, placers.offset + i);
						const auto* bodyProgram = submitDesc.program;
						if (creature != nullptr)
						{
							UseCreatureBody(*creature, submitDesc);
						}
						DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
						if (creature != nullptr)
						{
							submitDesc.program = bodyProgram;
							submitDesc.morphTargets = nullptr;
							submitDesc.paintedSkins = entt::null;
						}
						// drawn at once: the shadows over it right after, the tail of its Draw; a queued one gets them in its
						// Z object
						DrawShadowsOnObject(desc.viewId, placers.offset + i, submitDesc.modelMatrices, submitDesc.matrixCount);
						if (creature != nullptr)
						{
							DrawCreatureParts(desc.viewId, *desc.camera, renderCtx, *creature);
						}
					}
				}
				else if (!queued)
				{
					DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						DrawShadowsOnObject(desc.viewId, placers.offset + i, submitDesc.modelMatrices, submitDesc.matrixCount);
					}
				}
				if (queued)
				{
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						if (cutAbove.contains(placers.offset + i))
						{
							continue;
						}
						// the object's key (its origin, (x^2 + z^2) + y^2) or, for the hand, the origin of the hand's 3D
						// object, (x^2 + y^2) + z^2. (inferred) that the hand instance's translation is that object's origin
						const auto origin = glm::vec3(renderCtx.instanceUniforms[placers.offset + i][3]);
						const auto order = hand ? zsort::SumOrder::XYZ : zsort::SumOrder::XZY;
						sorted.Submit(
						    {.meshId = meshId, .index = placers.offset + i, .morphWithTerrain = placers.morphWithTerrain},
						    zsort::Key(origin, cameraOrigin, order));
					}
				}
			};
			for (const auto& [meshId, placers] : renderCtx.instancedDrawDescs)
			{
				drawRange(meshId, placers, false);
			}
			for (const auto& [meshId, placers] : renderCtx.sortedOpaqueDrawDescs)
			{
				drawRange(meshId, placers, true);
			}
			// the sharks' parts above the water, in the normal object list, and the shadows over
			// them (RendererShadows.cpp)
			if (desc.viewId == graphics::RenderPass::Main)
			{
				DrawCutAboveWater(desc.viewId);
				DrawShadowsOnCutObjects(desc.viewId, cutAbove);
			}
			// One model instance drawn on its own: from the queue, or a fading one at once (in the main view). The alternate
			// mode table with its alpha byte for a fading object (components::Alpha, in the object colour's alpha), every
			// primitive in its own mode for the others
			const auto drawInstance = [&](const ZObject& instance, RenderPass viewId) {
				auto mesh = meshManager.Handle(instance.meshId);
				// the SuperVillager's default sun, as in the instance loop above
				std::optional<model_light::ScopedLight> superVillagerSun;
				if (desc.superVillagers.litByDefaultSun.contains(instance.meshId))
				{
					superVillagerSun.emplace(model_light::k_DefaultSun);
				}
				submitDesc.viewId = viewId;
				submitDesc.instanceDesc =
				    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, instance.index, 1);
				setMatrices(instance.meshId, *mesh);
				ecs::UsePose(poses, instance.index, *mesh, submitDesc.modelMatrices, submitDesc.matrixCount);
				submitDesc.isSky = false;
				submitDesc.lightBoost = instance.meshId == ecs::components::Hand::k_MeshId ? 1.5f : 1.0f;
				submitDesc.noHaze = instance.meshId == ecs::components::Hand::k_MeshId;
				ApplyLandLightMode(renderCtx, instance.meshId, submitDesc);
				submitDesc.morphWithTerrain = instance.morphWithTerrain;
				submitDesc.program = land_morph::ObjectProgram(*_shaderManager, instance.morphWithTerrain);
				// the whole object (its Draw; the hand's own draw)
				submitDesc.blendFilter = 0;
				submitDesc.options = instance.fading ? render_modes::StateOptions {.msaa = true} : opaqueOptions;
				submitDesc.table = instance.fading ? render_modes::Table::GlobalAlpha : render_modes::Table::Normal;
				submitDesc.globalAlpha = render_modes::AlphaByte(1.0f - renderCtx.instanceUniforms[instance.index][0][3]);
				submitDesc.mode = std::nullopt;
				submitDesc.sea = {};
				// a creature's body, with its own shape and skins, then its hair and eyes
				const auto* creature = creature_draw::Find(_frameCreatures, instance.index);
				if (creature != nullptr)
				{
					UseCreatureBody(*creature, submitDesc);
				}
				DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
				// the tail loop of its Draw: the shadows over it right after it, inside its Z object for a queued one, in the
				// main view for a fading one drawn at once. (inferred) that the hand's mesh draw runs that tail too, so its
				// shadows come before the held object and its effects
				DrawShadowsOnObject(viewId, instance.index, submitDesc.modelMatrices, submitDesc.matrixCount);
				// a creature's hair and eyes after those shadows, as when it is drawn at once. (approximate) the original
				// draws a queued creature's eyes at once, outside its Z object; a creature's body is never queued today
				if (creature != nullptr)
				{
					submitDesc.morphTargets = nullptr;
					submitDesc.paintedSkins = entt::null;
					DrawCreatureParts(viewId, *desc.camera, renderCtx, *creature);
				}
				submitDesc.options = opaqueOptions;
				submitDesc.table = render_modes::Table::Normal;
				submitDesc.globalAlpha = 255;
				submitDesc.viewId = desc.viewId;
			};
			// One PSys mesh atom (RenderContext::psysAtoms): the callback of a Sorted atom's Z object and the draw of a
			// Queued / Immediate one inside its effect: drawn as is, or cut by the default plane with DrawCutByPlane
			// (sea_pass::CutAtoms). A translucent one (global alpha or additive) through the alternate mode table, an
			// additive one in mode 13 (UseAdditiveAlpha, Creators/Mesh.h); an opaque one in its materials' own modes
			const auto drawAtom = [&](const RenderContext::ParticleInstance& atom, RenderPass viewId) {
				// fully faded out (alpha 0, kept in [0][3] as 1 - alpha): not drawn, it would still write depth
				if (atom.translucent && renderCtx.instanceUniforms[atom.index][0][3] >= 1.0f)
				{
					return;
				}
				auto mesh = meshManager.Handle(atom.meshId);
				submitDesc.viewId = viewId;
				submitDesc.instanceDesc =
				    std::make_unique<graphics::InstanceDesc>(renderCtx.instanceUniformBuffer, atom.index, 1);
				setMatrices(atom.meshId, *mesh);
				// a ParticleAnimCreator atom has its own pose
				ecs::UsePose(poses, atom.index, *mesh, submitDesc.modelMatrices, submitDesc.matrixCount);
				submitDesc.isSky = false;
				submitDesc.lightBoost = 1.0f;
				submitDesc.noHaze = false;
				ApplyLandLightMode(renderCtx, atom.meshId, submitDesc);
				submitDesc.morphWithTerrain = false;
				submitDesc.program = land_morph::ObjectProgram(*_shaderManager, false);
				submitDesc.blendFilter = 0;
				submitDesc.options = atom.translucent ? render_modes::StateOptions {.msaa = true} : opaqueOptions;
				submitDesc.table = atom.translucent ? render_modes::Table::GlobalAlpha : render_modes::Table::Normal;
				submitDesc.globalAlpha = render_modes::AlphaByte(1.0f - renderCtx.instanceUniforms[atom.index][0][3]);
				// a mode of its own (an object's ghost, the second draw) for every material, else mode 13 for an additive
				// atom, else the materials' own
				submitDesc.mode = atom.mode ? atom.mode
				                  : atom.translucent && atom.additive
				                      ? std::optional(render_modes::Mode::AlphaTexturedAlphaAdditiveNoZWrite)
				                      : std::nullopt;
				submitDesc.sea = atom.cut ? sea_pass::CutAtoms(viewId) : sea_pass::SeaDraw {};
				DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
				submitDesc.sea = {};
				submitDesc.options = opaqueOptions;
				submitDesc.table = render_modes::Table::Normal;
				submitDesc.globalAlpha = 255;
				submitDesc.mode = std::nullopt;
				submitDesc.viewId = desc.viewId;
			};
			if (sortBlended)
			{
				for (const auto& [meshId, placers] : renderCtx.translucentDrawDescs)
				{
					if (!meshManager.Contains(meshId)) // (openblack guard) a mesh that did not load; never in the original
					{
						continue;
					}
					const bool zSorted = meshManager.Handle(meshId)->IsZSorted();
					for (uint32_t i = 0; i < placers.count; ++i)
					{
						// fully faded out (alpha 0, kept in [0][3] as 1 - alpha): not drawn, it would still write depth
						if (renderCtx.instanceUniforms[placers.offset + i][0][3] >= 1.0f)
						{
							continue;
						}
						const ZObject instance {.meshId = meshId,
						                        .index = placers.offset + i,
						                        .morphWithTerrain = placers.morphWithTerrain,
						                        .fading = true};
						// the sort point of the one-shot orb: only the orbs have one, and their object is always queued
						const auto point = renderCtx.sortPoints.find(placers.offset + i);
						// a fading held object too (components::NeedsSorting, the Z-sort flag)
						if (!zSorted && point == renderCtx.sortPoints.end() &&
						    !renderCtx.sortedInstances.contains(placers.offset + i))
						{
							// without the Z-sort flag: drawn at once, through the alternate mode table. The physical shield too
							// when its mesh lacks 0x200 (it sets its global alpha, then is drawn as any object)
							drawInstance(instance, desc.viewId);
							continue;
						}
						// the object's key ((x^2 + z^2) + y^2) at the orb's sort point or the translation
						const auto origin = point != renderCtx.sortPoints.end()
						                        ? point->second
						                        : glm::vec3(renderCtx.instanceUniforms[placers.offset + i][3]);
						sorted.Submit(instance, zsort::Key(origin, cameraOrigin, zsort::SumOrder::XZY));
					}
				}
			}

			// The broken buildings and their fragments (FragMesh): lit per face on the CPU and drawn at once in the object
			// loop, no Z object. (approximate) here after every model instead of in the loop's order
			if (desc.viewId == graphics::RenderPass::Main && desc.drawEntities && desc.camera != nullptr)
			{
				_frameFragMeshes.Clear();
				ecs::physics::FragMesh::FrameLight frameLight {
				    .lit = IsLandLit(),
				    .light = model_light::Light(),
				    .ambient = model_light::Ambient(),
				    .haze = _haze,
				    .view = desc.camera->GetViewMatrix(Camera::Interpolation::Current),
				};
				// the fire glow's clock: the frame's turn and its fraction (DrawClock)
				const float turnTime = static_cast<float>(desc.clock.turn) + desc.clock.turnFraction;
				ecs::physics::Buildings::AppendFragMeshes(_frameFragMeshes, frameLight, turnTime);
				// (approximate) every primitive, blended ones too, goes in Main (docs/bw1-notes/rendering-objects.md,
				// FragMesh light)
				world_triangles::Submit(graphics::RenderPass::Main, _frameFragMeshes, *_shaderManager);
			}
			// The leashes, drawn at once right after the land and its models, as the original does: before the exploded
			// pieces, the hand and the particle effects drawn at once
			if (sortBlended)
			{
				DrawLeashes(desc.viewId, *desc.camera);
			}
			// ---- the exploded pieces ----
			// The exploded pieces are drawn at once, no Z object (they ignore the queue flag), after the models drawn at once
			// and before the queue's drain (at the end of the frame); drawn below with the Sorted surfaces, in the walk's order
			// (SortedFrame::atOnce); built by PreDraw
			// ---- end of the exploded pieces ----
			// The particle effects by their draw path (psys::DrawPath)
			// collected by PreDraw (CollectFrameParticles); the other views draw none of them, as before
			static const FrameParticles k_NoPSys {};
			const auto& framePSys = sortBlended ? _frameParticles : k_NoPSys;
			const auto& psysSorted = framePSys.sorted;
			const auto& psysQueued = framePSys.queued;
			const auto& psysHand = framePSys.hand;
			const auto& psysSurfaces = framePSys.surfaces;
			std::unordered_map<const psys::Atom*, size_t> surfaceOf;
			for (size_t i = 0; i < psysSurfaces.size(); ++i)
			{
				surfaceOf.insert_or_assign(psysSurfaces[i].atom, i);
			}
			// The pieces (Kind::MeshPiece) of the Queued and Immediate effects, built once for the frame and tagged with their
			// atom: each is drawn by drawOrderedEffect at its atom's place (world_triangles::Submit's `only`)
			const auto& orderedPieces = framePSys.orderedPieces; // built by PreDraw (CollectFrameParticles)
			// A Queued or Immediate effect drawn all at once (the queue flag off): its items in the effect's order
			// (manager::OrderedEffect). Sprites through DrawParticleSprites, meshes through drawAtom, mists through
			// DrawEffectMist, surfaces through DrawParticleSurface, pieces through world_triangles::Submit, chains through
			// DrawParticleChain; the other kinds draw nothing here
			const auto drawOrderedEffect = [&](const psys::manager::OrderedEffect& effect, RenderPass viewId) {
				std::vector<psys::Effect::DrawAtom> sprites;
				const auto flushSprites = [&]() {
					if (!sprites.empty())
					{
						DrawParticleSprites(sprites, *desc.camera, viewId);
						sprites.clear();
					}
				};
				for (const auto& item : effect.items)
				{
					const auto* creator = item.atom.creator;
					if (item.chain < 0 && creator != nullptr && creator->kind == psys::Creator::Kind::Sprite)
					{
						sprites.push_back(item.atom);
						continue;
					}
					flushSprites();
					if (item.chain >= 0)
					{
						if (static_cast<size_t>(item.chain) < effect.chains.size())
						{
							DrawParticleChain(viewId, *desc.camera, effect.chains[static_cast<size_t>(item.chain)]);
						}
						continue;
					}
					if (creator == nullptr)
					{
						continue;
					}
					if (creator->kind == psys::Creator::Kind::MeshPiece)
					{
						// a piece atom: its world triangles at once, at the atom's place in the effect's order (it ignores the
						// queue flag). No data uses it today: the pieces' effect, EXPLODE_OBJECT, is drawn Sorted (the block
						// above, in Main)
						world_triangles::Submit(viewId, orderedPieces, *_shaderManager, item.atom.atom);
						continue;
					}
					if (creator->kind == psys::Creator::Kind::Mesh)
					{
						const auto found = renderCtx.psysAtomIndex.find(item.atom.atom);
						if (found != renderCtx.psysAtomIndex.end())
						{
							drawAtom(renderCtx.psysAtoms[found->second], viewId);
						}
						continue;
					}
					if (dynamic_cast<const psys::MistCreator*>(creator) != nullptr)
					{
						mists::MistDesc mist {};
						if (psys::mist_atoms::Describe(item.atom, mist))
						{
							DrawEffectMist(viewId, *desc.camera, mist);
						}
						continue;
					}
					if (creator->className == "ZR_SurfRevol")
					{
						const auto found = surfaceOf.find(item.atom.atom);
						if (found != surfaceOf.end())
						{
							DrawParticleSurface(viewId, psysSurfaces[found->second]);
						}
					}
				}
				flushSprites();
			};
			if (sortBlended)
			{
				// A Sorted effect (the spells and the other Sorted sites): the effect has no Z object, each element goes into
				// the queue with its own key, (x^2 + y^2) + z^2 (zsort::SumOrder::XYZ). Its MeshPiece pieces (the exploded
				// pieces above) and ZR_SurfRevol surfaces ignore the queue flag: drawn at once, here after the models, in the
				// walk's order, effect by effect (RendererRevolvedSurface.cpp)
				if (desc.drawEntities)
				{
					for (const auto* atom : psysSorted.atOnce)
					{
						if (const auto found = surfaceOf.find(atom); found != surfaceOf.end())
						{
							if (psysSurfaces[found->second].path == psys::DrawPath::Sorted)
							{
								DrawParticleSurface(desc.viewId, psysSurfaces[found->second]);
							}
							continue;
						}
						// one Submit per piece (its own transient buffer): the same draws, but the (openblack) guard of a full
						// transient buffer now drops whole later pieces instead of cutting the last one
						world_triangles::Submit(graphics::RenderPass::Main, _frameParticles.sortedPieces, *_shaderManager,
						                        atom);
					}
				}
				// each sprite, keyed at the sprite's position (manager::SortedFrame::sprites)
				for (size_t i = 0; i < psysSorted.sprites.size(); ++i)
				{
					sorted.Submit({.psysSprite = static_cast<int>(i)}, zsort::Key(psysSorted.sprites[i].key, cameraOrigin));
				}
				// each mesh atom, opaque ones too, after the on-screen test (here the sphere around the mesh's box,
				// (approximate) not the box), keyed at the object's origin
				const auto viewProjection = desc.camera->GetViewProjectionMatrix();
				for (size_t i = 0; i < renderCtx.psysAtoms.size(); ++i)
				{
					const auto& atom = renderCtx.psysAtoms[i];
					if (atom.path != psys::DrawPath::Sorted)
					{
						continue;
					}
					const auto& model = renderCtx.instanceUniforms[atom.index];
					if (!BoxInView(viewProjection, meshManager.Handle(atom.meshId)->GetBoundingBox(), model))
					{
						continue;
					}
					sorted.Submit({.psysMesh = static_cast<int>(i)}, zsort::Key(atom.key, cameraOrigin));
				}
				// each mist, its own Z object at the mist's position: they come through mists::Submit
				// (mist_atoms::SubmitFrame) and CollectMists (PreDraw).
				// each chain, keyed at the joint n / 2
				for (size_t i = 0; i < psysSorted.chains.size(); ++i)
				{
					sorted.Submit({.psysChain = static_cast<int>(i)}, zsort::Key(psysSorted.chains[i].key, cameraOrigin));
				}
				// the Queued effects (the seed graphic on a ball or an icon, the containers; the fire's graphic likewise):
				// one Z object per effect at its origin, drawn all at once from the drain
				for (size_t i = 0; i < psysQueued.size(); ++i)
				{
					sorted.Submit({.queuedEffect = static_cast<int>(i)}, zsort::Key(psysQueued[i].origin, cameraOrigin));
				}
			}
			if (spritesSorted)
			{
				Locator::entitiesRegistry::value().Each<const ecs::components::Sprite, const ecs::components::Transform>(
				    [&sorted, &cameraOrigin](entt::entity entity, const auto&, const ecs::components::Transform& transform) {
					    // the sprite's position, (x^2 + y^2) + z^2
					    sorted.Submit({.sprite = entity}, zsort::Key(transform.position, cameraOrigin));
				    },
				    entt::exclude<ecs::components::Unavailable>);
				// the dust, the same key (it came in the loop above while its puffs were entities)
				for (size_t i = 0; i < _frameDust.size(); ++i)
				{
					sorted.Submit({.dust = static_cast<int>(i)}, zsort::Key(_frameDust[i].position, cameraOrigin));
				}
			}
			if (mistsSorted)
			{
				for (const auto& [key, index] : _preMists) // collected by PreDraw
				{
					sorted.Submit({.mist = static_cast<int>(index)}, key);
				}
			}
			if (spritesSorted)
			{
				for (const auto& [key, index] : _preSmoke) // collected by PreDraw
				{
					sorted.Submit({.smoke = static_cast<int>(index)}, key);
				}
			}
			if (sortBlended)
			{
				// one Z object per raining tile; they used to be drawn as one group after the queue
				for (const auto& [key, index] : _preRain) // collected by PreDraw
				{
					sorted.Submit({.rain = static_cast<int>(index)}, key);
				}
				// the boat's sprites, each one on its own; they used to be one batch after the queue
				for (const auto& [key, index] : CollectBoatSprites(*desc.camera))
				{
					sorted.Submit({.boat = static_cast<int>(index)}, key);
				}
				// the hand's ripples on the influence border, one Z object each (RendererInfluence.cpp)
				for (const auto& [key, index] : CollectInfluenceRipples(*desc.camera))
				{
					sorted.Submit({.ripple = static_cast<int>(index)}, key);
				}
				// the intro light, one Z object, keyed at its head ((x^2 + y^2) + z^2); RendererIntroLight.cpp
				if (desc.overlay.introLight.active)
				{
					const auto& introLight = desc.overlay.introLight;
					sorted.Submit({.introLight = 0}, zsort::Key(introLight.keyPoint, desc.camera->GetOrigin()));
				}
			}

			// OPENBLACK_ZSORTER_TRACE=1: once a second, what the frame's queue holds (docs/bw1-notes/openblack-internals.md)
			static const bool k_ZSorterTrace = std::getenv("OPENBLACK_ZSORTER_TRACE") != nullptr;
			if (k_ZSorterTrace && sortBlended && ++RendererDebugHooksData().zsorterTraceFrame % 60 == 0)
			{
				std::array<int, 14> counts {};
				const auto ordered = sorted.Ordered();
				for (const auto& entry : ordered)
				{
					const auto& z = *entry.item;
					const size_t kind = z.cloud >= 0                                               ? 1
					                    : z.rain >= 0                                              ? 2
					                    : z.boat >= 0                                              ? 3
					                    : z.psysSprite >= 0                                        ? 4
					                    : z.psysMesh >= 0                                          ? 5
					                    : z.psysChain >= 0                                         ? 6
					                    : z.queuedEffect >= 0                                      ? 7
					                    : z.mist >= 0                                              ? 8
					                    : z.smoke >= 0                                             ? 9
					                    : z.sprite != entt::null || z.dust >= 0                    ? 10
					                    : z.meshId == ecs::components::Hand::k_MeshId && !z.fading ? 11
					                    : z.fading                                                 ? 12
					                    : z.IsModel()                                              ? 0
					                                                                               : 13;
					++counts.at(kind);
				}
				SPDLOG_LOGGER_INFO(spdlog::get("graphics"),
				                   "ZSorter trace: {} entries ({} dropped): models {}, fading {}, clouds {}, rain tiles {}, "
				                   "boat sprites {}, PSys sprites {}, PSys meshes {}, PSys chains {}, queued effects {}, "
				                   "mists {}, smoke {}, sprites {}, hand {}; farthest key {:.1f}, nearest {:.1f}",
				                   ordered.size(), sorted.Dropped(), counts[0], counts[12], counts[1], counts[2], counts[3],
				                   counts[4], counts[5], counts[6], counts[7], counts[8], counts[9], counts[10], counts[11],
				                   ordered.empty() ? 0.0f : ordered.front().key, ordered.empty() ? 0.0f : ordered.back().key);
			}

			// OPENBLACK_ORB_TRACE=1: one line a drawn frame per one-shot orb (docs/bw1-notes/openblack-internals.md), to
			// follow the bubble across the 15 -> 0 wrap of its 4 x 4 sheet
			static const bool k_OrbTrace = std::getenv("OPENBLACK_ORB_TRACE") != nullptr;
			if (k_OrbTrace && sortBlended && Locator::entitiesRegistry::has_value())
			{
				const auto viewProjection = desc.camera->GetViewProjectionMatrix();
				// the queue in draw order; the key printed is the distance (the root of the queue's key) as before
				const auto ordered = sorted.Ordered();
				// the ZR_SurfRevol discs: a Sorted effect's at once, before the whole queue (so before every bubble); a
				// Queued one's inside its effect's entry
				for (const auto& surface : psysSurfaces)
				{
					int at = -1;
					for (size_t k = 0; k < ordered.size() && surface.path != psys::DrawPath::Sorted; ++k)
					{
						const int effect = ordered[k].item->queuedEffect;
						if (effect >= 0 && psysQueued[static_cast<size_t>(effect)].effect == surface.effect)
						{
							at = static_cast<int>(k);
							break;
						}
					}
					SPDLOG_LOGGER_INFO(spdlog::get("graphics"),
					                   "Orb trace: surface ({}) path {} sorted {}/{} origin ({:.1f}, {:.1f}, {:.1f})",
					                   surface.texture, static_cast<int>(surface.path), at, static_cast<int>(ordered.size()),
					                   surface.origin.x, surface.origin.y, surface.origin.z);
				}
				Locator::entitiesRegistry::value().Each<const ecs::components::OneOffSpellSeed, const ecs::components::Mesh>(
				    [&](entt::entity entity, const ecs::components::OneOffSpellSeed& orb, const ecs::components::Mesh& mesh) {
					    const auto found = renderCtx.entityInstances.find(entity);
					    if (found == renderCtx.entityInstances.end())
					    {
						    SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Orb trace: orb {} has no instance this frame",
						                       static_cast<uint32_t>(entity));
						    return;
					    }
					    const auto index = found->second.index;
					    const auto& model = renderCtx.instanceUniforms[index];
					    int at = -1;
					    float key = -1.0f;
					    for (size_t k = 0; k < ordered.size(); ++k)
					    {
						    if (ordered[k].item->fading && ordered[k].item->index == index &&
						        ordered[k].item->meshId == mesh.id)
						    {
							    at = static_cast<int>(k);
							    key = std::sqrt(ordered[k].key);
							    break;
						    }
					    }
					    // the gate of the sorted list: a fully faded instance ([0][3] >= 1) is left out; nothing culls the
					    // translucent instances by frustum, so SphereInView is only reported
					    const float alpha = 1.0f - model[0][3];
					    auto l3d = meshManager.Handle(mesh.id);
					    const auto box = l3d->GetBoundingBox();
					    const float radius = glm::length(box.Size()) * 0.5f;
					    const bool inView =
					        SphereInView(viewProjection, glm::vec3(model * glm::vec4(box.Center(), 1.0f)), radius);
					    SPDLOG_LOGGER_INFO(
					        spdlog::get("graphics"),
					        "Orb trace: orb {} phase {:.4f} frame {} packed[1][3] {:.6f} uv ({:.3f}, {:.3f}) alpha {:.3f} "
					        "sorted {}/{} key {:.2f} sortPoint ({:.1f}, {:.1f}, {:.1f}) inView {}",
					        static_cast<uint32_t>(entity), orb.phase, static_cast<int>(orb.phase), model[1][3],
					        static_cast<float>(static_cast<int>(orb.phase) % 4) * 0.25f,
					        static_cast<float>(static_cast<int>(orb.phase) / 4) * 0.25f, alpha, at,
					        static_cast<int>(ordered.size()), key, orb.sortPoint.x, orb.sortPoint.y, orb.sortPoint.z, inView);
				    });
			}

			// The influence circles: after the power spins, so after everything drawn at once, and before the drain at the
			// end of the frame, every frame of the world view. (inferred) not in the reflection: they are drawn once, after
			// the whole world draw (RendererInfluence.cpp; docs/bw1-notes/original-frame.md)
			if (sortBlended)
			{
				DrawInfluenceCircles(desc.viewId); // its camera gate, scroll and alpha: PreDraw
			}

			// The drain, far to near (a full queue dropped the entries over 2048), in
			// its own view right after the main pass (same target and camera, no clear), so that nothing drawn at once in
			// the main pass is drawn over them
			if (!sorted.Empty())
			{
				constexpr auto k_Blended = graphics::RenderPass::MainBlended;
				auto& spriteRegistry = Locator::entitiesRegistry::value();
				const auto drained = sorted.Drain();
				bool handEffectsDrawn = false;
				std::vector<psys::Effect::DrawAtom> sprites;
				for (size_t e = 0; e < drained.size(); ++e)
				{
					const auto& instance = *drained[e].item;
					if (instance.cloud >= 0)
					{
						DrawCloud(k_Blended, *desc.camera, static_cast<uint32_t>(instance.cloud));
						continue;
					}
					if (instance.rain >= 0)
					{
						DrawRainTile(k_Blended, static_cast<uint32_t>(instance.rain));
						continue;
					}
					if (instance.boat >= 0)
					{
						DrawBoatSprite(k_Blended, static_cast<uint32_t>(instance.boat));
						continue;
					}
					if (instance.ripple >= 0)
					{
						DrawInfluenceRipple(k_Blended, static_cast<uint32_t>(instance.ripple), desc.clock.frameGameMs);
						continue;
					}
					if (instance.introLight >= 0)
					{
						DrawIntroLight(k_Blended, *desc.camera, desc.overlay.introLight);
						continue;
					}
					if (instance.psysSprite >= 0)
					{
						// a Sorted effect's sprite, drawn from its Z object; the next entries that are sprites too
						// go in the same call (DrawParticleSprites keeps their order and batches only equal materials)
						sprites.clear();
						sprites.push_back(psysSorted.sprites[static_cast<size_t>(instance.psysSprite)].atom);
						while (e + 1 < drained.size() && drained[e + 1].item->psysSprite >= 0)
						{
							++e;
							sprites.push_back(psysSorted.sprites[static_cast<size_t>(drained[e].item->psysSprite)].atom);
						}
						DrawParticleSprites(sprites, *desc.camera, k_Blended);
						continue;
					}
					if (instance.psysMesh >= 0)
					{
						drawAtom(renderCtx.psysAtoms[static_cast<size_t>(instance.psysMesh)], k_Blended);
						continue;
					}
					if (instance.psysChain >= 0)
					{
						// the chain's Z object callback
						DrawParticleChain(k_Blended, *desc.camera,
						                  psysSorted.chains[static_cast<size_t>(instance.psysChain)].chain);
						continue;
					}
					if (instance.queuedEffect >= 0)
					{
						// the Queued effect's Z object callback
						drawOrderedEffect(psysQueued[static_cast<size_t>(instance.queuedEffect)], k_Blended);
						continue;
					}
					if (instance.mist >= 0)
					{
						DrawMist(k_Blended, *desc.camera, static_cast<uint32_t>(instance.mist));
						continue;
					}
					if (instance.smoke >= 0)
					{
						DrawChimneySmoke(k_Blended, *desc.camera, static_cast<uint32_t>(instance.smoke));
						continue;
					}
					if (instance.sprite != entt::null)
					{
						const auto& [sprite, transform] =
						    spriteRegistry.Get<const ecs::components::Sprite, const ecs::components::Transform>(
						        instance.sprite);
						drawSprite(sprite, transform, k_Blended);
						continue;
					}
					if (instance.dust >= 0)
					{
						drawDust(_frameDust[static_cast<size_t>(instance.dust)], k_Blended);
						continue;
					}
					if (!instance.IsModel())
					{
						continue;
					}
					drawInstance(instance, k_Blended);
					// the hand's draw: after the hand's mesh and the held object, the spell in the hand draws the hand's
					// effects at once, in their own order (manager::HandEffects, DrawPath::Immediate). The held object is not
					// drawn from the hand's entry here (inferred: no visible difference, it is opaque and drawn before)
					if (instance.meshId == ecs::components::Hand::k_MeshId && !instance.fading && !handEffectsDrawn)
					{
						handEffectsDrawn = true;
						for (const auto& effect : psysHand)
						{
							drawOrderedEffect(effect, k_Blended);
						}
					}
				}
			}
			// The effects' ribbons and surfaces, the rain tiles, the boat's sprites and the
			// clouds are no longer groups of their own: they all went at once or through the queue above. The shadows on the
			// objects went with their objects: right after each one drawn at once, inside the Z object of each queued one
			// (RendererShadows.cpp). A receiver not drawn this frame (out of view, faded out, past the queue's 2048) gets none,
			// as the tail of a Draw that did not run
			ClearShadowReceivers();

			// Debug
			if (desc.viewId == graphics::RenderPass::Main)
			{
				if (renderCtx.boundingBox)
				{
					const auto boundBoxOffset = static_cast<uint32_t>(renderCtx.instanceUniforms.size() / 2);
					const auto boundBoxCount = static_cast<uint32_t>(renderCtx.instanceUniforms.size() / 2);
					renderCtx.boundingBox->GetVertexBuffer().Bind();
					bgfx::setInstanceDataBuffer(toBgfx(renderCtx.instanceUniformBuffer), boundBoxOffset, boundBoxCount);
					bgfx::setState(k_BgfxDefaultStateInvertedZ | BGFX_STATE_PT_LINES);
					bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(debugShaderInstanced->GetRawHandle()));
				}
				if (renderCtx.footpaths)
				{
					renderCtx.footpaths->GetVertexBuffer().Bind();
					bgfx::setState(k_BgfxDefaultStateInvertedZ | BGFX_STATE_PT_LINES);
					bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(debugShader->GetRawHandle()));
				}
				if (renderCtx.streams)
				{
					renderCtx.streams->GetVertexBuffer().Bind();
					bgfx::setState(k_BgfxDefaultStateInvertedZ | BGFX_STATE_PT_LINES);
					bgfx::submit(static_cast<bgfx::ViewId>(desc.viewId), toBgfx(debugShader->GetRawHandle()));
				}
			}
		}

		{
			auto subSection =
			    profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawSprites
			                                                               : Profiler::Stage::MainPassDrawSprites);

			// In the main pass the sprites went through the back-to-front list with the blended models
			if (desc.drawSprites && !spritesSorted)
			{
				using namespace ecs::components;

				auto& registry = Locator::entitiesRegistry::value();
				registry.Each<const Sprite, const Transform>(
				    [&drawSprite, &desc](const Sprite& sprite, const Transform& transform) {
					    drawSprite(sprite, transform, desc.viewId);
				    },
				    entt::exclude<ecs::components::Unavailable>);
				for (const auto& puff : _frameDust)
				{
					drawDust(puff, desc.viewId);
				}
			}
		}
	}

	if (desc.drawSky && desc.viewId == graphics::RenderPass::Main)
	{
		if (!mistsSorted)
		{
			// without the entities the models' section did not drain the queue: the mists join the clouds in it here and
			// it is drained; nothing else can be in it
			for (const auto& [key, index] : _preMists) // collected by PreDraw
			{
				sorted.Submit({.mist = static_cast<int>(index)}, key);
			}
			for (const auto& entry : sorted.Drain())
			{
				if (entry.item->cloud >= 0)
				{
					DrawCloud(graphics::RenderPass::MainBlended, *desc.camera, static_cast<uint32_t>(entry.item->cloud));
				}
				else if (entry.item->mist >= 0)
				{
					DrawMist(graphics::RenderPass::MainBlended, *desc.camera, static_cast<uint32_t>(entry.item->mist));
				}
			}
		}
		DrawSun(graphics::RenderPass::MainBlended, *desc.camera, true, desc.clock.frameGameMs);
	}

	// Enable stats or debug text.
	auto debugMode = BGFX_DEBUG_NONE;
	if (_bgfxDebug)
	{
		debugMode |= BGFX_DEBUG_STATS;
	}
	if (desc.wireframe)
	{
		debugMode |= BGFX_DEBUG_WIREFRAME;
	}
	if (_bgfxProfile)
	{
		debugMode |= BGFX_DEBUG_PROFILER;
	}
	bgfx::setDebug(debugMode);
}

void Renderer::Frame() noexcept
{
	SubmitOpaqueAlpha();
	// Advance to next frame. Process submitted rendering primitives.
	bgfx::frame();
}

void Renderer::SubmitOpaqueAlpha() const
{
	if (_resolution.x == 0 || _resolution.y == 0)
	{
		return;
	}
	// (openblack engine) every view before this one may leave the backbuffer's alpha below 1 (the blended models,
	// sprites, ImGui), and bgfx's Vulkan swapchain is composited with it when the driver offers it
	// (VK_COMPOSITE_ALPHA_INHERIT / PRE_MULTIPLIED, renderer_vk.cpp:7305): the desktop then shows through. A full-screen
	// quad writing only the alpha (no RGB, no depth test or write, no blending) makes it 1 without changing the image
	const auto viewId = static_cast<bgfx::ViewId>(graphics::RenderPass::OpaqueAlpha);
	bgfx::setViewClear(viewId, BGFX_CLEAR_NONE);
	bgfx::setViewRect(viewId, 0, 0, _resolution.x, _resolution.y);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(identity));
	constexpr uint32_t k_OpaqueWhite = 0xFFFFFFFFu;
	const std::array<ScreenRectVertex, 6> quad {{{-1.0f, 1.0f, 0.5f, k_OpaqueWhite},
	                                             {1.0f, 1.0f, 0.5f, k_OpaqueWhite},
	                                             {1.0f, -1.0f, 0.5f, k_OpaqueWhite},
	                                             {-1.0f, 1.0f, 0.5f, k_OpaqueWhite},
	                                             {1.0f, -1.0f, 0.5f, k_OpaqueWhite},
	                                             {-1.0f, -1.0f, 0.5f, k_OpaqueWhite}}};
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(quad.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, quad.data(), sizeof(quad));
	bgfx::setVertexBuffer(0, &buffer);
	bgfx::setState(BGFX_STATE_WRITE_A);
	bgfx::submit(viewId, toBgfx(_shaderManager->GetShader("DebugLine")->GetRawHandle()));
}

void Renderer::RequestScreenshot(const std::filesystem::path& filepath) noexcept
{
	const bgfx::FrameBufferHandle mainBackbuffer = BGFX_INVALID_HANDLE;
	bgfx::requestScreenShot(mainBackbuffer, filepath.string().c_str());
}
