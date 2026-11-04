/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <LNDFile.h>
#include <SDL_video.h>
#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <bx/file.h>
#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/ChimneySmoke.h"
#include "3D/Clouds.h"
#include "3D/DayNightClock.h"
#include "3D/HandMorph.h"
#include "3D/InfluenceCircle.h"
#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSkinTexels.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandColourStamps.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightFrame.h"
#include "3D/LandLightTable.h"
#include "3D/Lightning.h"
#include "3D/Mists.h"
#include "3D/OceanInterface.h"
#include "3D/OrientedText.h"
#include "3D/Rain.h"
#include "3D/SnowCover.h"
#include "3D/StormClouds.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleMap.h"
#include "3D/WaterRings.h"
#include "Blast/RootsPile.h"
#include "Camera/Camera.h"
#include "Common/CrashHandler.h"
#include "Common/MainThreadQueue.h"
#include "Common/Platform.h"
#include "Common/UserData.h"
#include "Common/WorkerPool.h"
#include "Creature/CreatureEnvironmentMap.h"
#include "Creature/CreatureFizzLook.h"
#include "Creature/CreatureHair.h"
#include "Creature/CreatureHairLight.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureSkin.h"
#include "Creature/CreatureSpells.h"
#include "Creature/LeashOrders.h"
#include "ECS/AbodeKnock.h"
#include "ECS/BoatLaunchRules.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/DestructionGhost.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGlow.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HandMorph.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/LightBeam.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/MistDome.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/Weather.h"
#include "ECS/DrawnOnScreen.h"
#include "ECS/DrawnPlace.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/MovingShadowCasters.h"
#include "ECS/PlayerColour.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/SwimmerRules.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LandShadeSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/RainSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/SnowfallSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Graphics/BoneBudget.h"
#include "Graphics/CreatureReflection.h"
#include "Graphics/DebugLines.h"
#include "Graphics/DrawOrder.h"
#include "Graphics/DrawWindow.h"
#include "Graphics/FizzDrawOrder.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsDeviceInterface.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/GroundBlobs.h"
#include "Graphics/HandLight.h"
#include "Graphics/HandReflection.h"
#include "Graphics/HandWaterGlow.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/LandFootprints.h"
#include "Graphics/LandShadowBake.h"
#include "Graphics/LightShine.h"
#include "Graphics/MaterialStages.h"
#include "Graphics/Mesh.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/ModelLight.h"
#include "Graphics/Moon.h"
#include "Graphics/ObjectShadows.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/PosedDraws.h"
#include "Graphics/Primitive.h"
#include "Graphics/RendererDetail.h"
#include "Graphics/ScreenshotCapture.h"
#include "Graphics/SeaReflection.h"
#include "Graphics/SeaRows.h"
#include "Graphics/SeeThroughState.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShadowTexture.h"
#include "Graphics/SpriteTextures.h"
#include "Graphics/Sun.h"
#include "Graphics/TreeBrightness.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/ViewFrustum.h"
#include "Graphics/WarmUpViews.h"
#include "Graphics/ZSort.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/VortexRules.h"
#include "Profiler.h"
#include "Renderer.h"
#include "Resources/Rereads.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/ObjectDrawList.h"
#include "Visual/DetailLevel.h"
#include "Visual/LightBeams.h"
#include "Windowing/WindowingInterface.h"
#if defined(__EMSCRIPTEN__)
#include "Web/WebPlatform.h"
#endif
#include "entt/locator/locator.hpp"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::ecs::systems;
using namespace openblack::graphics::detail;

namespace openblack
{

namespace
{
/// How many buffers the frames are drawn into in turn
constexpr uint8_t k_BackBuffers = 3;
} // namespace

RenderPass Renderer::TranslucentView(RenderPass scene)
{
	// In the temple the scene's own pass draws everything in the order it comes
	return Locator::temple::has_value() && Locator::temple::value().Active() ? scene : TranslucentPassOf(scene);
}

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
		// After a lost device, bgfx makes the device again with all its resources at the start of the next frame
		if (graphics::device_restore::ContinuesAfter(code))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "bgfx: {}:{}: {}: {}. The renderer makes the device again.", filePath,
			                    line, codeStr, str);
			return;
		}
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "bgfx: {}:{}: FATAL ({}): {}", filePath, line, codeStr, str);

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_CRITICAL
		spdlog::get("graphics")
		    ->log(spdlog::source_loc {filePath, line, SPDLOG_FUNCTION}, spdlog::level::critical, "FATAL ({}): {}", codeStr,
		          str);
#endif

		// Must terminate, continuing will cause crash anyway. This can come from bgfx's render thread, where nothing
		// would catch an exception, so it is reported and the game exits from here.
		if (crash_handler::IsInstalled())
		{
			crash_handler::ReportFatal(crash_report::CrashKind::GraphicsFatal, str, filePath, line,
			                           std::string("bgfx ") + codeStr);
		}
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
			// bgfx only says so in its notes when it had no screen to read for a picture, which is then never written
			if (const auto uncaptured = graphics::UncapturedScreenshotPath(out); uncaptured.has_value())
			{
				failures.Record(std::string(*uncaptured),
				                "the renderer had no screen to read (a Vulkan window has none while it is minimised)");
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
	// The driver's compiled pipelines, kept between runs
	uint32_t cacheReadSize(uint64_t id) override
	{
		// The renderer asks for the cache once for each pipeline that it makes. The Vulkan renderer also asks one time for
		// all the kept pipelines, as number 0, when it starts.
		if (id != 0)
		{
			++pipelinesMade;
		}
		return pipelines.has_value() ? pipelines->ReadSize(id) : 0;
	}
	bool cacheRead(uint64_t id, void* data, uint32_t size) override
	{
		return pipelines.has_value() && pipelines->Read(id, std::span(static_cast<std::byte*>(data), size));
	}
	void cacheWrite(uint64_t id, const void* data, uint32_t size) override
	{
		if (pipelines.has_value())
		{
			pipelines->Write(id, std::span(static_cast<const std::byte*>(data), size));
		}
	}
	// Saving a screenshot
	void screenShot(const char* filePath, uint32_t width, uint32_t height, uint32_t pitch, bgfx::TextureFormat::Enum format,
	                const void* data, uint32_t size, bool yflip) override
	{
		// The pictures are BGRA8. A renderer gives the pixels in the format of its window, which can be RGBA8.
		auto bgra = std::span(static_cast<const uint8_t*>(data), size);
		std::vector<uint8_t> swapped;
		if (format == bgfx::TextureFormat::RGBA8)
		{
			swapped = graphics::RgbaToBgra(bgra);
			bgra = swapped;
		}
		else if (format != bgfx::TextureFormat::BGRA8)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Screenshot at {} is in a format that can't be written", filePath);
			failures.Record(filePath, "the window's pixels are in a format that can't be written");
			return;
		}
		if (graphics::ScreenPictures::InMemory(filePath))
		{
			// Make the picture now from the pixels of bgfx, and do not copy the full frame
			pictures.Make(filePath, {.width = width, .height = height, .pitch = pitch, .yflip = yflip, .bgra = bgra});
			return;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Taking a screenshot...");

		const auto path = std::filesystem::path(filePath);
		if (path.extension() != ".png")
		{
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "Not Implemented: {} screenshot ({}x{}) requested at {}",
			                   path.extension().string(), width, height, filePath);
			failures.Record(filePath, "only .png pictures can be written");
			return;
		}
		if (const auto why = graphics::WriteScreenshotPng(path, width, height, pitch, bgra, yflip); !why.empty())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Failed to save Screenshot ({}x{}) at {}: {}", width, height, filePath,
			                    why);
			failures.Record(filePath, "couldn't be written: " + why);
			return;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Screenshot ({}x{}) saved at {}", width, height, filePath);
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

	/// The pictures bgfx gave up or couldn't write, told from its render thread
	graphics::ScreenshotFailures failures;
	/// The pictures that the game asked for in memory
	graphics::ScreenPictures pictures;
	/// The pipelines that the renderer made since it started, counted on its render thread
	std::atomic<uint32_t> pipelinesMade {0};
	/// The driver's compiled pipelines on disk. Only the Vulkan and Direct3D 12 renderers keep them, and only when the
	/// system gives a user folder.
	std::optional<graphics::PipelineCache> pipelines;
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
	case GraphicsBackend::OpenGLES:
		init.type = bgfx::RendererType::OpenGLES;
		break;
	case GraphicsBackend::WebGPU:
		init.type = bgfx::RendererType::WebGPU;
		break;
	case GraphicsBackend::Direct3D11:
		init.type = bgfx::RendererType::Direct3D11;
		break;
	case GraphicsBackend::OpenGL:
		init.type = bgfx::RendererType::OpenGL;
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
		init.swapChain.width = static_cast<uint32_t>(drawableSize.x);
		init.swapChain.height = static_cast<uint32_t>(drawableSize.y);

		// Get Native Handles from SDL window
		const auto handles = window.GetNativeHandles();
		init.swapChain.nwh = handles.nativeWindow;
		init.swapChain.ndt = handles.nativeDisplay;
	}

	uint32_t bgfxReset = BGFX_RESET_NONE;
	auto bgfxCallback = std::make_unique<BgfxCallback>();
	// The Vulkan renderer gives back one cache for all its pipelines, the Direct3D 12 renderer one cache for each
	if (const auto folder = user_data::Folder(); folder.has_value())
	{
		if (backend == GraphicsBackend::Vulkan)
		{
			bgfxCallback->pipelines.emplace(*folder / "cache" / "vulkan_pipelines.bin", graphics::PipelineCache::Kind::Shared);
		}
		else if (backend == GraphicsBackend::Direct3D12)
		{
			bgfxCallback->pipelines.emplace(*folder / "cache" / "d3d12_pipelines.bin",
			                                graphics::PipelineCache::Kind::PerPipeline);
		}
	}
	if (vsync)
	{
		bgfxReset |= BGFX_RESET_VSYNC;
	}
	init.reset = bgfxReset;
	// The window's colours are 8-bit RGBA, the format that the screenshots and the other readers of the window expect
	init.swapChain.formatColor = bgfx::TextureFormat::RGBA8;
	// Three buffers to draw into: with two, a frame waits for the one on screen to be let go of, a whole refresh or two
	// whenever the window is composited, as Direct3D 12 windows often are
	init.swapChain.numBackBuffers = k_BackBuffers;
	init.callback = dynamic_cast<bgfx::CallbackI*>(bgfxCallback.get());

#if defined(__EMSCRIPTEN__)
	// In the browser, only the thread that owns the canvas can draw into it. The web platform gives the canvas to a
	// render thread of its own, which then draws the frames of bgfx while this thread prepares the next frame. When that
	// thread does not start, a call to renderFrame before init makes bgfx draw on this thread.
	if (!web::StartRenderThread())
	{
		bgfx::renderFrame();
	}
#endif
	if (!bgfx::init(init))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Failed to initialize bgfx.");
		return nullptr;
	}
	if (bgfx::getRendererType() != init.type)
	{
		// bgfx tries the next renderer when the renderer that the game asks for does not start. In a web browser with no
		// WebGPU device, this is WebGL 2.
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "The {} renderer did not start. The game draws with {}.",
		                   bgfx::getRendererName(init.type), bgfx::getRendererName(bgfx::getRendererType()));
	}

	const bgfx::Caps* caps = bgfx::getCaps();
	if (caps->limits.maxTextureLayers < 9)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Graphics device must support texture layers.");
		return nullptr;
	}

	return std::make_unique<Renderer>(bgfxReset, std::move(bgfxCallback));
}

Renderer::Renderer(uint32_t bgfxReset, std::unique_ptr<BgfxCallback>&& bgfxCallback) noexcept
    : _shaderManager(std::make_unique<ShaderManager>())
    , _bgfxCallback(std::move(bgfxCallback))
    , _bgfxReset(bgfxReset)
    , _offscreenShots(std::make_unique<OffscreenScreenshots>())
    , _workers(std::make_unique<WorkerPool>(WorkerPool::DefaultThreadCount()))
{
	_shaderManager->LoadShaders();
	// Each program's mesh uniforms are found now, so that the threads that encode draws only read them
	_shaderManager->ForEachProgram([this](const ShaderProgram& program) { static_cast<void>(MeshUniformsOf(program)); });
	_groundShadowScratch.resize(_workers->GetLaneCount());
	_plane = Primitive::CreatePlane();
	_morphStreamLayouts = std::make_unique<MorphStreamLayouts>();
	{
		constexpr uint32_t k_White = 0xFFFFFFFF;
		_whiteTexture = fromBgfx(bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_NONE,
		                                               bgfx::copy(&k_White, sizeof(k_White))));
		bgfx::setName(toBgfx(*_whiteTexture), "White");
		// The renderer gives the white texel again after a restore of the device, thus the device keeps no copy
		bgfx::setDeviceRestoreByApplication(toBgfx(*_whiteTexture));
	}
	_readBackTexture =
	    fromBgfx(bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_READ_BACK | BGFX_SAMPLER_NONE,
	                                   bgfx::copy(_readBackTexel.data(), static_cast<uint32_t>(_readBackTexel.size()))));
	bgfx::setName(toBgfx(*_readBackTexture), "Read Back");
	// Only the reads of the texture are of use, not its texel, thus the device keeps no copy
	bgfx::setDeviceRestoreByApplication(toBgfx(*_readBackTexture));
	// The moving shadows' data, grown as more are cast
	_groundShadowData = std::make_unique<Texture2D>("Ground Shadow Data");
	_groundShadowData->Create(k_GroundShadowDataWidth, 1, 1, TextureFormat::RGBA32F, Wrapping::ClampEdge, Filter::Nearest,
	                          nullptr);
	// The ground shadow pass fills the texture before its draws, thus the device keeps no copy after a restore
	_groundShadowData->SetTexelsGivenAgain();

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
	_offscreenShots.reset();
	_creatureSkins.clear();
	_roomCreatureSkins.clear();
	_drawnCreatureSkins.clear();
	_spareCreatureSkins.clear();
	_animalBoneTexture.reset();
	_handSkins.clear();
	_handSkinTextures.clear();
	_particleLightMaps.clear();
	_glowShells.clear();
	_snowDepth.reset();
	_plane.reset();
	_morphStreamLayouts.reset();
	_handFlowTexture.reset();
	_groundShadowData.reset();
	_objectShadowFrameBuffer.reset();
	_landShadowBake.reset();
	_landFootprints.reset();
	_templeMapFrameBuffer.reset();
	_templeMapHalfFrameBuffer.reset();
	if (_landLightTexture)
	{
		bgfx::destroy(toBgfx(*_landLightTexture));
	}
	ReleaseLightOfFrameBefore();
	if (_iconsTexture)
	{
		bgfx::destroy(toBgfx(*_iconsTexture));
	}
	if (_whiteTexture)
	{
		bgfx::destroy(toBgfx(*_whiteTexture));
	}
	if (_readBackTexture)
	{
		bgfx::destroy(toBgfx(*_readBackTexture));
	}
	for (auto& texture : _landLuminosity)
	{
		texture.reset();
	}
	_landColourFrameBuffer.reset();
	_hairLightKept.reset();
	_skyDomeFrameBuffer.reset();
	if (_lightningGlowTexture)
	{
		bgfx::destroy(toBgfx(*_lightningGlowTexture));
	}
	if (_warmUpInstances.has_value())
	{
		bgfx::destroy(toBgfx(*_warmUpInstances));
	}
	if (_interfaceCreatureInstances.has_value())
	{
		bgfx::destroy(toBgfx(*_interfaceCreatureInstances));
	}
	if (_menuCreatureInstance.has_value())
	{
		bgfx::destroy(toBgfx(*_menuCreatureInstance));
	}
	if (_filmCreatureInstance.has_value())
	{
		bgfx::destroy(toBgfx(*_filmCreatureInstance));
	}
	for (auto& query : _glareQueries)
	{
		if (bgfx::isValid(query))
		{
			bgfx::destroy(query);
			query = BGFX_INVALID_HANDLE;
		}
	}
	_shaderManager.reset();
	bgfx::frame();
	bgfx::shutdown();
#if defined(__EMSCRIPTEN__)
	web::WaitForRenderThreadStop();
#endif
}

namespace
{
/// The rectangles of a scene's views over the whole of a target `resolution` pixels big: its sky's, its own, what is
/// drawn over it and what blends in it
void SetSceneViewRects(RenderPass viewId, glm::u16vec2 resolution) noexcept
{
	const auto setRect = [resolution](RenderPass pass) {
		bgfx::setViewRect(static_cast<bgfx::ViewId>(pass), 0, 0, resolution.x, resolution.y);
	};
	setRect(SkyPassOf(viewId));
	if (HasSkyPass(viewId))
	{
		setRect(viewId);
	}
	if (const auto landId = LandPassOf(viewId); landId != viewId)
	{
		setRect(landId);
	}
	if (viewId == RenderPass::Main)
	{
		setRect(RenderPass::Advisors);
	}
	if (viewId == RenderPass::Reflection)
	{
		setRect(RenderPass::ReflectionMirrored);
		setRect(RenderPass::UnderSea);
	}
	if (const auto translucentId = TranslucentPassOf(viewId); translucentId != viewId)
	{
		setRect(translucentId);
	}
}
} // namespace

void Renderer::ConfigureView(graphics::RenderPass viewId, glm::u16vec2 resolution, uint32_t clearColor) const noexcept
{
	if (viewId == RenderPass::Main)
	{
		_mainClearColour = clearColor;
	}
	// A scene with a sky of its own is cleared by its sky's pass, which comes first, and drawn over it
	bgfx::setViewClear(static_cast<bgfx::ViewId>(SkyPassOf(viewId)), BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor, 0.0f, 0);
	if (HasSkyPass(viewId))
	{
		bgfx::setViewClear(static_cast<bgfx::ViewId>(viewId), BGFX_CLEAR_NONE);
	}
	// Its land is drawn over the sky and the sea, before the rest of the scene
	if (const auto landId = LandPassOf(viewId); landId != viewId)
	{
		bgfx::setViewClear(static_cast<bgfx::ViewId>(landId), BGFX_CLEAR_NONE);
	}
	// The advisors near the screen are drawn over the main view, its depth cleared
	if (viewId == RenderPass::Main)
	{
		bgfx::setViewClear(static_cast<bgfx::ViewId>(RenderPass::Advisors), BGFX_CLEAR_DEPTH, 0, 0.0f, 0);
	}
	// The hand and the creature that the game mirrors are drawn over the mirrored world with its depth cleared, as the
	// game draws its mirrored land without depth. What is under the sea's surface is then drawn over it all.
	if (viewId == RenderPass::Reflection)
	{
		bgfx::setViewClear(static_cast<bgfx::ViewId>(RenderPass::ReflectionMirrored), BGFX_CLEAR_DEPTH, 0, 0.0f, 0);
		bgfx::setViewClear(static_cast<bgfx::ViewId>(RenderPass::UnderSea), BGFX_CLEAR_NONE);
	}
	// And what blends in it is drawn over it after
	if (const auto translucentId = TranslucentPassOf(viewId); translucentId != viewId)
	{
		bgfx::setViewClear(static_cast<bgfx::ViewId>(translucentId), BGFX_CLEAR_NONE);
	}
	SetSceneViewRects(viewId, resolution);
}

void Renderer::Reset(glm::u16vec2 resolution) const noexcept
{
	// The other values of the window's buffers stay as bgfx::init set them
	bgfx::SwapChain swapChain;
	swapChain.width = resolution.x;
	swapChain.height = resolution.y;
	bgfx::reset(_bgfxReset, &swapChain);
	// The scene and its mirror in the sea cover the window, each keeping how it is cleared
	SetSceneViewRects(RenderPass::Main, resolution);
	SetSceneViewRects(RenderPass::Reflection, resolution);
}

graphics::ShaderManager* Renderer::GetShaderManager() const noexcept
{
	return _shaderManager.get();
}

bool Renderer::WarmUpScene(const DrawSceneDesc& drawDesc, uint32_t part) const noexcept
{
	MainThreadQueue::CheckDirect(MainThreadApi::Renderer, "Renderer::WarmUpScene");
	// The draws into the window keep to one pixel. The renderer makes a pipeline for each draw that has pixels.
	for (const auto view : warm_up::k_WindowSceneViews)
	{
		bgfx::setViewScissor(static_cast<bgfx::ViewId>(view), 0, 0, 1, 1);
	}
	if (!_warmUpInstances.has_value())
	{
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
		    .end();
		_warmUpInstances = fromBgfx(bgfx::createDynamicVertexBuffer(1, layout));
		// Each warm-up gives the instance, thus the device keeps no copy after a restore
		bgfx::setDeviceRestoreByApplication(toBgfx(*_warmUpInstances));
	}
	const RenderContext::ObjectInstance hidden {.model = glm::mat4(0.0f), .look = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)};
	bgfx::update(toBgfx(*_warmUpInstances), 0, bgfx::copy(&hidden, sizeof(hidden)));
	// Under Vulkan, one frame with all the draws can use all of the descriptor sets, so each part does some of them
	constexpr auto k_PartedViews = [] {
		std::array<uint16_t, warm_up::k_PartedViews.size()> views {};
		std::ranges::transform(warm_up::k_PartedViews, views.begin(),
		                       [](RenderPass view) { return static_cast<uint16_t>(view); });
		return views;
	}();
	const auto window = WarmUpPartWindow(part, warm_up::k_DrawsPerPart);
	const bool parted = _shaderManager->OpenDrawWindow(window.first, window.count, k_PartedViews);
	_warmingUp = true;
	DrawScene(drawDesc);
	_warmingUp = false;
	_warmUpInFrame = true;
	const auto draws = _shaderManager->CloseDrawWindow();
	return parted && draws.has_value() && part + 1 < warm_up::k_MaxParts &&
	       WarmUpPartsLeft(part, *draws, warm_up::k_DrawsPerPart);
}

void Renderer::DrawScene(const DrawSceneDesc& drawDesc) const noexcept
{
	MainThreadQueue::CheckDirect(MainThreadApi::Renderer, "Renderer::DrawScene");
	// The draw only reads the world. Thus side threads find the changed footprints and test the casters of the shadows
	// while the draw continues, and the passes that paint them take the results.
	if (drawDesc.drawIsland)
	{
		if (!_landFootprints)
		{
			_landFootprints = std::make_unique<LandFootprints>();
		}
		_landFootprints->StartSearch();
		if (drawDesc.drawEntities)
		{
			if (!_landShadowBake)
			{
				_landShadowBake = std::make_unique<LandShadowBake>();
			}
			_landShadowBake->StartWatchedTest();
		}
	}
	SelectDrawnCreatures(drawDesc);
	// A warm-up gives no creature a skin texture: each creature gets them in its first frame
	if (!_warmingUp)
	{
		PlanCreatureHairLight(drawDesc);
		UploadCreatureSkins(drawDesc.entities, _creatureSkins);
		// The copy of the creature in the temple's creature room has a skin of its own
		if (Locator::temple::has_value())
		{
			UploadCreatureSkins(Locator::temple::value().GetRooms(), _roomCreatureSkins);
		}
		// The creature of the tattoo menu is a thing of the drawn registry
		if (Locator::drawnRegistry::has_value())
		{
			UploadCreatureSkins(Locator::drawnRegistry::value(), _drawnCreatureSkins);
		}
	}
	UploadHandSkins();
	UploadAnimalBones(drawDesc);
	{
		const auto& textures = Locator::resources::value().GetTextures();
		_snowTexture = textures.Find(snow_cover::k_TextureId.value());
		_snowAlphaTexture = textures.Find(snow_cover::k_AlphaTextureId.value());
	}
	// What the particle effects draw, once for the land's colours and every pass
	CollectParticles();
	// The land's light of the frame that ended, before this frame makes its own
	KeepLightOfFrameBefore();
	DrawLandLuminosityPass(drawDesc);
	DrawLandColourPass(drawDesc);
	DrawSkyDomePass(drawDesc);
	PaintLandFootprints(drawDesc);
	UpdateLandLight();
	UpdateLandDetail(drawDesc);
	DrawObjectShadowPass(drawDesc);
	// The passes above took the results of the side threads
	if (_landFootprints)
	{
		_landFootprints->ForgetSearch();
	}
	if (_landShadowBake)
	{
		_landShadowBake->ForgetWatchedTest();
	}
	DrawTempleMapPass();
	DrawGroundShadowPass(drawDesc);
	// Reflection Pass
	{
		auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::ReflectionPass);
		const auto& instancedDrawDescs = Locator::rendereringSystem::value().GetContext().instancedDrawDescs;
		const bool showsReflection = drawDesc.drawEntities && std::ranges::any_of(instancedDrawDescs, [](const auto& entry) {
			                             return entry.second.showsReflection;
		                             });
		if (drawDesc.drawWater || showsReflection)
		{
			DrawSceneDesc drawPassDesc = drawDesc;

			const auto& frameBuffer = Locator::oceanSystem::value().GetReflectionFramebuffer();
			auto reflectionCamera = drawDesc.camera->Reflect();

			drawPassDesc.viewId = graphics::RenderPass::Reflection;
			drawPassDesc.camera = reflectionCamera.get();
			drawPassDesc.frameBuffer = &frameBuffer;
			drawPassDesc.drawWater = false;
			drawPassDesc.drawBoundingBoxes = false;
			drawPassDesc.cullBack = true;
			// The game mirrors the land under the sea without its small bump detail. Unlike the game, which mirrors
			// only the sky, the land and a few moving things, everything is mirrored here.
			drawPassDesc.smallBumpMapStrength = 0.0f;

			DrawPass(drawPassDesc);

			// Then what is under the sea's surface, seen from the view itself, into the same target and against its
			// depth: the main pass submits it there (the sharks' bodies) before blending the sea over the target
			frameBuffer.Bind(RenderPass::UnderSea);
			_shaderManager->SetCamera(RenderPass::UnderSea, *drawDesc.camera);
			bgfx::touch(static_cast<bgfx::ViewId>(RenderPass::UnderSea));
		}
	}

	// Main Draw Pass
	{
		auto section = Locator::profiler::value().BeginScoped(Profiler::Stage::MainPass);
		DrawPass(drawDesc);
	}
	// The colours of the last hair for the next frame. A warm-up makes the target that keeps them.
	KeepCreatureHairLight();
}

uint32_t Renderer::GetPipelinesMade() const noexcept
{
	return _bgfxCallback->pipelinesMade.load();
}

void Renderer::RequestDeviceRestore(device_restore::Request request) noexcept
{
	MainThreadQueue::CheckDirect(MainThreadApi::Renderer, "Renderer::RequestDeviceRestore");
	switch (request)
	{
	case device_restore::Request::MakeAgain:
		bgfx::requestDeviceRestore();
		break;
	case device_restore::Request::Lose:
		bgfx::requestDeviceLoss();
		break;
	}
}

uint32_t Renderer::GetDeviceRestores() const noexcept
{
	return bgfx::getDeviceRestoreCount();
}

device_restore::Kept Renderer::GetDeviceRestoreKept() const noexcept
{
	const auto kept = bgfx::getDeviceRestoreKept();
	return {
	    .buffers = kept.buffers,
	    .dynamicBuffers = kept.dynamicBuffers,
	    .shaders = kept.shaders,
	    .textures = kept.textures,
	    .textureChanges = kept.textureChanges,
	};
}

uint32_t Renderer::GetDrawsOfLastFrame() const noexcept
{
	return bgfx::getStats()->numDraw;
}

bool Renderer::SetPipelinePrefetch(bool enabled) noexcept
{
	MainThreadQueue::CheckDirect(MainThreadApi::Renderer, "Renderer::SetPipelinePrefetch");
	// Only the WebGPU backend makes its pipelines in the background
	if (bgfx::getRendererType() != bgfx::RendererType::WebGPU)
	{
		return false;
	}
	bgfx::setPipelinePrefetch(enabled);
	return true;
}

uint32_t Renderer::GetPipelinePrefetchPending() const noexcept
{
	return bgfx::getPipelinePrefetchPending();
}

void Renderer::ReleaseGpuWorkMemory() noexcept
{
	// The read of a texture waits until the GPU did all the work before it, and then the backend releases the memory
	// of that work
	if (_readBackTexture)
	{
		bgfx::TextureRegion texel;
		texel.init(toBgfx(*_readBackTexture));
		bgfx::read(texel, _readBackTexel.data());
	}
}

void Renderer::Frame() noexcept
{
	MainThreadQueue::CheckDirect(MainThreadApi::Renderer, "Renderer::Frame");
	// In a warm-up's frame the loading screen's view first clears all of the window as the main view clears it. This is
	// set after every draw of the frame, as the loading screen sets its view too.
	const bool warmUp = std::exchange(_warmUpInFrame, false);
	_landCaps.clear();
	if (warmUp)
	{
		const auto cover = static_cast<bgfx::ViewId>(warm_up::k_CoverView);
		bgfx::setViewClear(cover, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, _mainClearColour, 0.0f, 0);
		bgfx::setViewRect(cover, 0, 0, bgfx::BackbufferRatio::Equal);
		bgfx::touch(cover);
	}
#if defined(__EMSCRIPTEN__)
	web::BeforeFrame();
#endif
	// Advance to next frame. Process submitted rendering primitives.
	const auto frame = bgfx::frame();
	// The game thread's encoder starts each frame at submit order 0
	_submitOrder = 0;
#if defined(__EMSCRIPTEN__)
	web::PresentFrame();
#endif
	// After a warm-up frame, set the views back
	if (warmUp)
	{
		for (const auto view : warm_up::k_WindowSceneViews)
		{
			bgfx::setViewScissor(static_cast<bgfx::ViewId>(view));
		}
		bgfx::setViewClear(static_cast<bgfx::ViewId>(warm_up::k_CoverView), BGFX_CLEAR_NONE);
	}
	_shaderManager->FrameEnded();
	_offscreenShots->FrameEnded(frame, _bgfxCallback->failures, _bgfxCallback->pictures);
	// After a restore of the device, the render targets that keep their pictures from frame to frame are empty. The
	// next frames draw the sky's dome and the temple's map again.
	if (_deviceRestores.Saw(bgfx::getDeviceRestoreCount()))
	{
		const auto kept = GetDeviceRestoreKept();
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"),
		                   "The renderer made the graphics device again ({} restores, {} kept bytes: buffers {}, dynamic "
		                   "buffers {}, shaders {}, textures {}, texture changes {})",
		                   bgfx::getDeviceRestoreCount(), kept.Total(), kept.buffers, kept.dynamicBuffers, kept.shaders,
		                   kept.textures, kept.textureChanges);
		_skyDomeLost = true;
		_templeMapVisit.reset();
		// The next frame keeps the colours of the last hair again
		if (std::holds_alternative<creature_hair_light::Kept>(_hairLightCarried))
		{
			_hairLightKept.reset();
		}
		// The graphics device keeps no copy of the textures, the vertices and the triangles of the land's blocks: the
		// land gives them again, and then the footprints paint the textures
		if (Locator::terrainSystem::has_value())
		{
			Locator::terrainSystem::value().GiveDeviceDataAgain();
		}
		if (_landFootprints)
		{
			_landFootprints->PaintAgain();
		}
		// The renderer's own textures that it fills from the world get their texels again: the land's shadows are baked
		// again, and the textures of the land's light, the snow and the painted skins are filled again in the next frame
		if (_landShadowBake)
		{
			_landShadowBake->BakeAgain();
		}
		for (auto& texture : _landLuminosity)
		{
			texture.reset();
		}
		_snowRevision.reset();
		for (auto* skins : {&_creatureSkins, &_roomCreatureSkins, &_drawnCreatureSkins})
		{
			for (auto& [entity, entry] : *skins)
			{
				entry.stamp.reset();
			}
		}
		_handSkinStamp.reset();
		_handFlowTexture.reset();
		_handFlowLoaded = false;
		// The renderer's textures of fixed texels get them again
		if (_whiteTexture)
		{
			constexpr uint32_t k_White = 0xFFFFFFFF;
			bgfx::updateTexture2D(toBgfx(*_whiteTexture), 0, 0, 0, 0, 1, 1, bgfx::copy(&k_White, sizeof(k_White)));
		}
		if (_lightningGlowTexture)
		{
			const auto image = land_colour_stamps::LightningImage();
			constexpr auto k_Side = static_cast<uint16_t>(land_colour_stamps::k_LightningSide);
			bgfx::updateTexture2D(toBgfx(*_lightningGlowTexture), 0, 0, 0, 0, k_Side, k_Side,
			                      bgfx::copy(image.data(), static_cast<uint32_t>(image.size())));
		}
		_shaderManager->GiveDefaultTexelsAgain();
		// The meshes and textures of the resource caches read their files again and give their data to the device
		if (Locator::graphicsDevice::has_value() && Locator::filesystem::has_value())
		{
			resources::Rereads rereads(
			    [](const std::filesystem::path& path) { return Locator::filesystem::value().ReadAll(path); });
			auto& givers = Locator::graphicsDevice::value().GetDataGivers();
			givers.GiveAll(rereads);
			SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "{} meshes and textures gave their data again from {} files",
			                   givers.Count(), rereads.ReadCount());
		}
	}
}

void Renderer::RequestScreenshot(const std::filesystem::path& filepath) noexcept
{
	MainThreadQueue::CheckDirect(MainThreadApi::Renderer, "Renderer::RequestScreenshot");
	const bool minimised = Locator::windowing::has_value() && Locator::windowing::value().IsMinimised();
	const auto size = Locator::windowing::has_value() ? glm::u16vec2(Locator::windowing::value().GetSize()) : glm::u16vec2(0);
	// All the renderers of bgfx can copy and read back textures
	constexpr bool k_CanReadBack = true;
	// Vulkan releases the window's buffers while the window is minimised. OpenGL on the desktop reads the window's own
	// pixels, which have no defined value where other windows cover the window. This often occurs in background runs.
	// In a browser, WebGPU gets a new canvas texture for each frame and releases it after it shows the frame. The other
	// renderers keep the whole frame in the window's buffers.
	auto buffers = graphics::WindowBuffers::Kept;
	switch (bgfx::getRendererType())
	{
	case bgfx::RendererType::Vulkan:
		buffers = graphics::WindowBuffers::DroppedWhenMinimised;
		break;
	case bgfx::RendererType::OpenGL:
		buffers = graphics::WindowBuffers::UncoveredOnly;
		break;
	case bgfx::RendererType::WebGPU:
		buffers = graphics::WindowBuffers::ReleasedAfterPresent;
		break;
	default:
		break;
	}
	switch (const auto plan = PlanScreenshot(buffers, minimised, k_CanReadBack, size); plan.route)
	{
	case ScreenshotRoute::Screen:
	{
		const bgfx::FrameBufferHandle mainBackbuffer = BGFX_INVALID_HANDLE;
		bgfx::requestScreenShot(mainBackbuffer, filepath.string().c_str());
		break;
	}
	case ScreenshotRoute::Offscreen:
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Drawing the frame aside for {}", filepath.string());
		_offscreenShots->Begin(filepath, size);
		break;
	case ScreenshotRoute::Refused:
		SPDLOG_LOGGER_WARN(spdlog::get("graphics"), "No screenshot at {}: {}", filepath.string(), plan.why);
		_bgfxCallback->failures.Record(filepath.string(), plan.why);
		break;
	}
}

std::optional<std::string> Renderer::TakeScreenshotFailure(const std::filesystem::path& filepath) noexcept
{
	auto failure = _bgfxCallback->failures.Take(filepath.string());
	if (failure.has_value())
	{
		_bgfxCallback->pictures.Forget(filepath.string());
	}
	return failure;
}

void Renderer::RequestScreenPicture(const std::string& name, graphics::ScreenPicturer picturer) noexcept
{
	MainThreadQueue::CheckDirect(MainThreadApi::Renderer, "Renderer::RequestScreenPicture");
	_bgfxCallback->pictures.Ask(name, std::move(picturer));
	RequestScreenshot(name);
}

std::optional<std::vector<uint8_t>> Renderer::TakeScreenPicture(const std::string& name) noexcept
{
	return _bgfxCallback->pictures.Take(name);
}
