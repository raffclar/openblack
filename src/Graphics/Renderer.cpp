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

/// How a sprite of the land's things draws: tested against depth, added by its alpha or blended over what is behind
constexpr uint64_t SpriteState(bool additive)
{
	const auto blend = additive ? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE)
	                            : BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
	return BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | blend |
	       BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_ADD);
}
} // namespace

RenderPass Renderer::TranslucentView(RenderPass scene)
{
	// In the temple the scene's own pass draws everything in the order it comes
	return Locator::temple::has_value() && Locator::temple::value().Active() ? scene : TranslucentPassOf(scene);
}

/// How far back, as a fraction of their depth, the temple's rooms the player isn't in are drawn: a few millimetres at the
/// doorways, past the rounding of the copies of their arches
constexpr float k_OtherTempleRoomDepthBias = 5e-5f;

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

namespace
{
/// Whether the ring of an order is of the player of this computer, who holds the leash of the creature. Only that player
/// sees the ring.
bool IsOwnLeashMarker(const ecs::Registry& registry, const ecs::components::LeashMarker& marker)
{
	const auto* leashes =
	    registry.Valid(marker.creature) ? registry.TryGet<const ecs::components::CreatureLeash>(marker.creature) : nullptr;
	const auto own = Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
	return leashes != nullptr && leashes->worn.has_value() && leashes->worn->holder == own;
}

/// Whether the game mirrors a creature's body in the sea this frame (see creature_reflection). The block under the
/// creature is tested against the main camera's view. Its box reaches below the water when the land is mirrored.
bool CreatureMirrored(entt::entity entity, glm::vec3 position, float fizz)
{
	if (!Locator::playerSystem::has_value() || !Locator::terrainSystem::has_value() || !Locator::camera::has_value())
	{
		return false;
	}
	const auto& players = Locator::playerSystem::value();
	creature_reflection::Situation situation {
	    .primaryCreature =
	        players.GetPrimaryCreature((Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer()
	                                                                     : PlayerNames::PLAYER_ONE)) == entity,
	    .height = position.y,
	    .fizz = fizz,
	};
	const auto under = creature_reflection::BlockUnder(position);
	if (situation.primaryCreature && under.has_value())
	{
		const auto& blocks = Locator::terrainSystem::value().GetBlocks();
		const auto block = std::ranges::find_if(blocks, [&under](const LandBlock& candidate) {
			return candidate.GetLndBlock() != nullptr && candidate.GetBlockPosition() == *under;
		});
		if (block != blocks.end())
		{
			const object_draw_list::Block box {
			    .corner = block->GetMapPosition(),
			    .highestAltitude = block->GetHighestAltitudeNow(),
			    .reflected = Locator::config::value().detail.landReflection,
			};
			const auto& camera = Locator::camera::value();
			situation.onBlock = true;
			situation.blockInView = object_draw_list::BlockInView(camera.GetViewProjectionMatrix(), camera.GetNearClip(), box);
			situation.blockDistance = object_draw_list::BlockDistance(camera.GetOrigin(), box);
		}
	}
	return creature_reflection::Shown(situation);
}

/// The colour a recognised gesture's flash adds to the hand's light, 0 to 1 a channel
glm::vec3 HandGlowColour()
{
	if (!Locator::handSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return glm::vec3(0.0f);
	}
	const auto hand =
	    Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	const auto* glow = Locator::entitiesRegistry::value().TryGet<ecs::components::HandGlow>(hand);
	if (glow == nullptr)
	{
		return glm::vec3(0.0f);
	}
	constexpr float k_ByteMax = 255.0f;
	return glm::vec3(static_cast<float>((glow->rgb >> 16u) & 0xFFu), static_cast<float>((glow->rgb >> 8u) & 0xFFu),
	                 static_cast<float>(glow->rgb & 0xFFu)) /
	       k_ByteMax;
}
} // namespace

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

void Renderer::DrawPass(const DrawSceneDesc& desc) const
{
	const auto& meshManager = Locator::resources::value().GetMeshes();
	auto& profiler = Locator::profiler::value();

	// The sky is drawn first, in its own pass into the same target, in the order the game draws it: the dome, the sun
	// and the moon. The scene's pass then draws everything else over it.
	const auto skyViewId = SkyPassOf(desc.viewId);
	// The land is drawn next, in a pass of its own, the nearest block first: after the sea and before everything that
	// stands on it or is under the sea behind it, which its depth hides
	const auto landViewId = LandPassOf(desc.viewId);
	if (desc.frameBuffer != nullptr)
	{
		desc.frameBuffer->Bind(desc.viewId);
		desc.frameBuffer->Bind(skyViewId);
		desc.frameBuffer->Bind(landViewId);
	}
	// This dummy draw call is here to make sure that view is cleared if no
	// other draw calls are submitted to view
	bgfx::touch(static_cast<bgfx::ViewId>(skyViewId));
	bgfx::touch(static_cast<bgfx::ViewId>(desc.viewId));
	_shaderManager->SetViewOrder(skyViewId, ViewOrder::Sequential);
	if (landViewId != desc.viewId)
	{
		_shaderManager->SetViewOrder(landViewId, ViewOrder::Sequential);
	}
	// bgfx sorts a view's blended draws by their programs unless told to keep them in order. The game draws the temple
	// in the order it is submitted, and its blended parts must be too: the pool's water, drawn with the lightmap
	// program, would otherwise land after the hand, whose faded wrist writes depth, and leave a hole in the water
	// where the wrist should show it through.
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	_shaderManager->SetViewOrder(desc.viewId, inTemple ? ViewOrder::Sequential : ViewOrder::ByProgram);
	// Outside the temple, what blends in the world is drawn after the rest of the scene in a pass of its own, all of it
	// together and the farthest first, as the game sorts it
	const auto translucentViewId = TranslucentView(desc.viewId);
	if (translucentViewId != desc.viewId)
	{
		if (desc.frameBuffer != nullptr)
		{
			desc.frameBuffer->Bind(translucentViewId);
		}
		_shaderManager->SetViewOrder(translucentViewId, ViewOrder::DepthDescending);
		_shaderManager->SetCamera(translucentViewId, *desc.camera);
	}
	// Outside the temple, the hand, the thing that it holds and the creature that the game mirrors in the sea are drawn
	// over all of the mirrored world, in their order
	const auto mirroredViewId =
	    desc.viewId == RenderPass::Reflection && !inTemple ? RenderPass::ReflectionMirrored : desc.viewId;
	if (mirroredViewId != desc.viewId)
	{
		if (desc.frameBuffer != nullptr)
		{
			desc.frameBuffer->Bind(mirroredViewId);
		}
		bgfx::touch(static_cast<bgfx::ViewId>(mirroredViewId));
		_shaderManager->SetViewOrder(mirroredViewId, ViewOrder::DepthAscending);
		_shaderManager->SetCamera(mirroredViewId, *desc.camera);
	}
	const auto cameraOrigin = desc.camera->GetOrigin();

	_shaderManager->SetCamera(skyViewId, *desc.camera);
	_shaderManager->SetCamera(landViewId, *desc.camera);
	_shaderManager->SetCamera(desc.viewId, *desc.camera);
	_modelLight = GetModelLight();
	if (Locator::camera::has_value())
	{
		const auto& camera = Locator::camera::value();
		_treeBrightness =
		    static_cast<float>(tree_brightness::Factor(camera.GetFocus(), camera.GetForward(), glm::vec3(_modelLight))) /
		    256.0f;
		_treeBrightnessMade = true;
	}

	const auto* skyShader = _shaderManager->GetShader("Sky");
	const auto* waterShader = _shaderManager->GetShader("Water");
	const auto* terrainShader = _shaderManager->GetShader("Terrain");
	const auto* debugShader = _shaderManager->GetShader("DebugLine");
	// Trees are placed on the land already, so they are drawn where they are rather than moved onto the height map
	const auto* vegetationShaderInstanced = _shaderManager->GetShader("Vegetation");
	const auto* spriteShader = _shaderManager->GetShader("Sprite");
	const auto* debugShaderInstanced = _shaderManager->GetShader("DebugLineInstanced");
	const auto* objectShaderInstanced = _shaderManager->GetShader("ObjectInstanced");
	const auto* objectShaderFewBonesInstanced = _shaderManager->GetShader("ObjectFewBonesInstanced");
	const auto* objectShaderMorphInstanced = _shaderManager->GetShader("ObjectMorphInstanced");
	const auto* objectShaderMorphEnvironmentInstanced = _shaderManager->GetShader("ObjectMorphEnvironmentInstanced");
	const auto* objectShaderPosedInstanced = _shaderManager->GetShader("ObjectPosedInstanced");
	const auto* objectShaderStaticInstanced = _shaderManager->GetShader("ObjectStaticInstanced");
	const auto* objectShaderEnvironmentStaticInstanced = _shaderManager->GetShader("ObjectEnvironmentStaticInstanced");
	const auto& sceneTextures = Locator::resources::value().GetTextures();
	const auto firstEnvironmentMap = entt::hashed_string("raw/envmap").value();
	const auto* environmentMap =
	    sceneTextures.Contains(firstEnvironmentMap) ? &*sceneTextures.Handle(firstEnvironmentMap) : nullptr;
	const auto* objectShaderHeightMapInstanced = _shaderManager->GetShader("ObjectHeightMapInstanced");
	const auto* objectShaderHeightMapStaticInstanced = _shaderManager->GetShader("ObjectHeightMapStaticInstanced");
	// Each mesh is drawn with the shader declaring the fewest bones that hold its skeleton
	const auto objectProgramFor = [&](uint8_t boneCount, bool morphWithTerrain) {
		const auto budget = bone_budget::For(boneCount);
		if (morphWithTerrain)
		{
			return budget == bone_budget::Budget::Single ? objectShaderHeightMapStaticInstanced
			                                             : objectShaderHeightMapInstanced;
		}
		switch (budget)
		{
		case bone_budget::Budget::Single:
			return objectShaderStaticInstanced;
		case bone_budget::Budget::Few:
			return objectShaderFewBonesInstanced;
		case bone_budget::Budget::All:
			break;
		}
		return objectShaderInstanced;
	};
	const auto* objectShaderPaletteInstanced = _shaderManager->GetShader("ObjectPaletteInstanced");
	const auto* objectShaderLightmapInstanced = _shaderManager->GetShader("ObjectLightmapInstanced");
	const auto* objectShaderReflectiveLightmapInstanced = _shaderManager->GetShader("ObjectReflectiveLightmapInstanced");

	const auto skyType = Locator::skyClockSystem::value().GetCurrentSkyType();

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawSky
		                                                                          : Profiler::Stage::MainPassDrawSky);
		if (desc.drawSky && _skyDomeFrameBuffer)
		{
			const auto modelMatrix = sky_dome::Placement();
			// The sky's alignment from 0, evil, to 2, good, mixes two of the alignments' domes
			const auto alignments = sky_dome::AlignmentPair(Locator::alignmentSystem::value().GetSkyAlignment() + 1.0f);
			const glm::vec4 u_skyAlignment {alignments.lower, alignments.upper, alignments.weight,
			                                static_cast<float>(_skyDomeRows)};

			skyShader->SetTextureSampler("s_diffuse", 0, _skyDomeFrameBuffer->GetColorAttachment());
			skyShader->SetUniformValue("u_skyAlignment", &u_skyAlignment);
			const glm::vec4 u_skyModulate {glm::vec3(_skyTint.modulate) / 255.0f, 1.0f};
			const glm::vec4 u_skyAdd {glm::vec3(_skyTint.add) / 255.0f, 0.0f};
			skyShader->SetUniformValue("u_skyModulate", &u_skyModulate);
			skyShader->SetUniformValue("u_skyAdd", &u_skyAdd);

			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = skyViewId;
			submitDesc.program = skyShader;
			submitDesc.state = k_BgfxDefaultStateInvertedZ;
			if (!desc.cullBack)
			{
				submitDesc.state &= ~BGFX_STATE_CULL_MASK;
				submitDesc.state |= BGFX_STATE_CULL_CCW;
			}
			submitDesc.modelMatrices = &modelMatrix;
			submitDesc.matrixCount = 1;
			submitDesc.isSky = true;

			const auto* dome =
			    Locator::entitiesRegistry::value().TryGet<ecs::components::SkyDome>(Locator::skySystem::value().GetDome());
			if (const auto* mesh = dome != nullptr ? Locator::resources::value().GetMeshes().Find(dome->meshId) : nullptr)
			{
				DrawMesh(*mesh, submitDesc, 0);
			}
			DrawSun(skyViewId);
			DrawMoon(skyViewId);
		}
	}

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawWater
		                                                                          : Profiler::Stage::MainPassDrawWater);
		if (desc.drawWater)
		{
			const auto& ocean = Locator::oceanSystem::value();
			const auto& mesh = ocean.GetMesh();
			mesh.GetIndexBuffer().Bind(mesh.GetIndexBuffer().GetCount(), 0);
			mesh.GetVertexBuffer().Bind();
			// The sea is a backdrop, as the game draws it: over whatever was there and leaving no depth, so the land under
			// the water, blended by its coast alpha, and a vortex's funnel down through a hole in the land show over it. It
			// is one triangle over the screen, the sea's plane worked out to the horizon for each pixel.
			constexpr auto k_SeaState =
			    k_BgfxDefaultStateInvertedZ & ~(BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_MASK | BGFX_STATE_CULL_MASK);
			bgfx::setState(k_SeaState);
			auto texture = Locator::resources::value().GetTextures().Handle(ocean.GetTexture());
			waterShader->SetTextureSampler("s_diffuse", 0, *texture);
			waterShader->SetTextureSampler("s_reflection", 1, ocean.GetReflectionFramebuffer().GetColorAttachment());
			SetSeaUniforms(*waterShader, *desc.camera);
			// The sea is drawn in the sky's pass, which keeps its order, so that what is drawn before the land (a vortex's
			// swirl) goes over the sea and under the land, as the game draws them in turn
			waterShader->Submit(static_cast<bgfx::ViewId>(skyViewId));
		}
		if (desc.viewId != RenderPass::Reflection)
		{
			DrawVortexDepthWalls(desc, skyViewId);
		}
	}

	// The land mirrored under the sea is lit at half
	const float landLightScale = desc.viewId == RenderPass::Reflection ? 0.5f : 1.0f;
	const glm::vec4 landSkyAndBump = {skyType, landLightScale, desc.smallBumpMapStrength, 0.0f};
	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawIsland
		                                                                          : Profiler::Stage::MainPassDrawIsland);
		// Below High detail the sea doesn't mirror the land
		const bool landShown = desc.viewId != RenderPass::Reflection || Locator::config::value().detail.landReflection;
		if (desc.drawIsland && landShown)
		{
			auto& island = Locator::terrainSystem::value();
			auto islandExtent = glm::vec4(island.GetExtent().minimum, island.GetExtent().maximum);

			const auto& textures = Locator::resources::value().GetTextures();
			auto smallBump = textures.Handle(LandIslandInterface::k_SmallBumpTextureId);
			auto smallBumpAlpha = textures.Handle(LandIslandInterface::k_SmallBumpAlphaTextureId);
			const auto& u_skyAndBump = landSkyAndBump;

			// The small bump detail fades out about a line across the ground: where the plane square to the camera's
			// view, 50 units ahead of it, meets the ground at the camera's height, or at 110.55 if the camera is higher
			const auto cameraForward = desc.camera->GetForward();
			const glm::vec2 forwardAlongGround {cameraForward.x, cameraForward.z};
			const float forwardLength = std::max(glm::length(forwardAlongGround), 1e-4f);
			constexpr float k_SmallBumpDistance = 50.0f;
			constexpr float k_SmallBumpHighestGround = 0.67f * 165.0f;
			const float lineDistance =
			    (k_SmallBumpDistance + cameraForward.y * std::max(0.0f, cameraOrigin.y - k_SmallBumpHighestGround)) /
			    forwardLength;
			const glm::vec4 u_smallBumpLine {cameraOrigin.x, cameraOrigin.z, forwardAlongGround / forwardLength};
			const glm::vec4 u_smallBump {lineDistance, 0.0f, 0.0f, 0.0f};
			// The land's detail lines this frame, from the view's camera (see land_detail): xy the camera's x and z, zw the
			// way it faces along the ground; then how far ahead the two mesh lines and the texture line lie, each 0 when
			// the camera looks straight down, and the bands
			const auto lineAhead = [](const land_detail::ViewLine& line) { return line.valid ? line.ahead : 0.0f; };
			const glm::vec4 u_landLine {_landDetail.meshNear.camera, _landDetail.meshNear.forward};
			const glm::vec4 u_landLineAhead {lineAhead(_landDetail.meshNear), lineAhead(_landDetail.meshFar),
			                                 lineAhead(_landDetail.texture), _landDetail.meshNear.valid ? 1.0f : 0.0f};
			const glm::vec4 u_landBands {_landDetail.lines.meshBand, _landDetail.lines.textureBand, 0.0f, 0.0f};

			// The hand's light is in the land's own lighting, so the reflection shows it too

			// clang-format off
			// The land and its small bump detail come premultiplied over the sea
			constexpr auto defaultState = 0u
				| BGFX_STATE_WRITE_MASK
				| BGFX_STATE_DEPTH_TEST_GREATER
				| BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA)
				| BGFX_STATE_MSAA
			;

			constexpr auto discard = 0u
				| BGFX_DISCARD_INSTANCE_DATA
				| BGFX_DISCARD_INDEX_BUFFER
				| BGFX_DISCARD_TRANSFORM
				| BGFX_DISCARD_VERTEX_STREAMS
				| BGFX_DISCARD_STATE
			;
			// clang-format on

			// The snow lying on the land, when there is its noise to draw it with
			const auto setTerrainSnow = [&]() {
				const auto& textures = Locator::resources::value().GetTextures();
				const auto* depth = SnowDepth(_snowDepth, _snowRevision);
				const bool snowed = depth != nullptr && textures.Contains(snow_cover::k_NoiseTextureId.value());
				const glm::vec4 u_snow {snowed ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
				terrainShader->SetUniformValue("u_snow", &u_snow);
				if (snowed)
				{
					terrainShader->SetTextureSampler("s_snowDepth", 11, *depth);
					terrainShader->SetTextureSampler("s12_snowNoise", 12,
					                                 *textures.Handle(snow_cover::k_NoiseTextureId.value()));
				}
			};
			// What every block shares. bgfx gives a draw the uniforms set since the draw before it was submitted, and
			// they stay for the draws after it in the order the view sorts them. The blocks come the nearest first,
			// in that order in the land's own pass and sorted by their distance in the reflection's. The nearest block
			// is submitted first with all of these, so they reach every block after it; the textures stay bound from
			// one block to the next.
			const auto landTexels = LandTexelsUniform(island.GetBlockTextureSide());
			const auto setTerrainUniforms = [&]() {
				terrainShader->SetTextureSampler("s0_blockTextures", 0, island.GetBlockTextures());
				terrainShader->SetTextureSampler("s9_landLuminosity", 9, GetLandLuminosity());
				terrainShader->SetTextureSampler("s10_landColour", 10, GetLandColour());
				terrainShader->SetTextureSampler("s1_smallBumpAlpha", 1, *smallBumpAlpha);
				terrainShader->SetTextureSampler("s2_smallBump", 2, *smallBump);
				terrainShader->SetUniformValue("u_smallBumpLine", &u_smallBumpLine);
				terrainShader->SetUniformValue("u_smallBump", &u_smallBump);
				terrainShader->SetUniformValue("u_landLine", &u_landLine);
				terrainShader->SetUniformValue("u_landLineAhead", &u_landLineAhead);
				terrainShader->SetUniformValue("u_landBands", &u_landBands);
				// Without pages of its own the land binds its block textures in their place, never read
				const auto* coarsePages = island.GetCoarsePages();
				terrainShader->SetTextureSampler("s15_coarsePages", 15,
				                                 coarsePages != nullptr ? *coarsePages : island.GetBlockTextures());
				SetObjectShadowUniforms(*terrainShader);
				terrainShader->SetTextureSampler("s7_landLight", 7, GetLandLightTexture());
				terrainShader->SetUniformValue("u_haze", &_haze[0]);
				terrainShader->SetUniformValue("u_hazeColour", &_haze[1]);
				terrainShader->SetUniformValue("u_skyAndBump", &u_skyAndBump);
				terrainShader->SetUniformValue("u_islandExtent", &islandExtent);
				terrainShader->SetUniformValue("u_landTexels", &landTexels);
				// The moving shadows fall on the land, not on its reflection
				SetGroundShadowUniforms(*terrainShader, desc.viewId == RenderPass::Main);
				setTerrainSnow();
			};

			// The game draws the land's blocks the nearest first, which bgfx keeps by their distance
			const auto& blocks = island.GetBlocks();
			const auto blockDepth = [&](const auto& block) {
				const auto centre = block.GetMapPosition() + glm::vec2(80.0f);
				return zsort::Depth(glm::vec3(centre.x, 0.0f, centre.y), cameraOrigin);
			};
			const ProgramUniform blockPositionUniform {*terrainShader, "u_blockPositionAndSize"};
			const ProgramUniform blockUniform {*terrainShader, "u_block"};
			const ProgramUniform blockNeighboursUniform {*terrainShader, "u_blockNeighbours"};
			const ProgramUniform coarsePageUniform {*terrainShader, "u_coarsePage"};
			const bool coarseLand = Locator::config::value().detail.coarseLand;
			const ProgramUniform vortexGroundUniform {*terrainShader, "u_vortexGround"};
			const ProgramUniform groundShadowBlockUniform {*terrainShader, "u_groundShadowBlock"};
			const bool groundShadowed = desc.viewId == RenderPass::Main;
			// An open vortex opens a hole in the land block under its middle and lays a ring on it; the last vortex on a
			// block is the one it shows
			const std::span<const ecs::systems::VortexSystemInterface::GroundMark> groundMarks =
			    Locator::vortexSystem::has_value() ? Locator::vortexSystem::value().GetGroundMarks()
			                                       : std::span<const ecs::systems::VortexSystemInterface::GroundMark> {};
			using GroundTextures = ecs::components::Vortex::GroundTextures;
			const auto groundTexturesLoaded = [&](const GroundTextures& ground) {
				return textures.Contains(ground.hole) && textures.Contains(ground.ring);
			};
			// Every block binds the same hole and ring textures, whether a vortex marks it or not: its u_vortexGround
			// alone says whether it shows them. bgfx draws the blocks by their distance, not in the order they were
			// submitted, so a binding left over from the block submitted before would split the blocks into two sets
			// of bindings that alternate down the sorted list, and Vulkan needs a descriptor set at every change. Only
			// a block marked by a vortex of another kind (the volcano's beside a land vortex) binds its own textures.
			const auto sharedMark = std::ranges::find_if(groundMarks, [&](const auto& mark) {
				return groundTexturesLoaded(ecs::components::Vortex::GroundTexturesOf(mark.type));
			});
			const GroundTextures* sharedGround =
			    sharedMark != groundMarks.end() ? &ecs::components::Vortex::GroundTexturesOf(sharedMark->type) : nullptr;
			const auto submitBlock = [&](size_t index) {
				const auto& block = blocks[index];
				// The block's mesh and texture this frame: y how its corners morph, z its texture
				const auto detail = index < _landDetail.blocks.size() ? _landDetail.blocks[index] : land_detail::BlockDetail {};
				const uint8_t coarserSides = index < _landDetail.coarserSides.size() ? _landDetail.coarserSides[index] : 0;
				// The game leaves out open sea only from blocks wholly before the first line
				const bool dropOpenSea = detail.mesh == land_detail::Mesh::Full && detail.morph == land_detail::Morph::None;
				// A block of open sea only then has no triangles, and nothing to draw
				const auto* triangles = block.GetTriangles(detail.mesh, coarserSides, dropOpenSea);
				if (triangles == nullptr)
				{
					return;
				}
				const glm::vec4 mapPositionAndSize = glm::vec4(block.GetMapPosition(), 160.0f, 160.0f);
				blockPositionUniform.Set(&mapPositionAndSize);
				const glm::vec4 u_block {static_cast<float>(index), static_cast<float>(detail.morph),
				                         static_cast<float>(detail.texture), 0.0f};
				blockUniform.Set(&u_block);
				const glm::vec4 u_blockNeighbours =
				    index < _landDetail.neighbours.size() ? _landDetail.neighbours[index] : glm::vec4(-1.0f);
				blockNeighboursUniform.Set(&u_blockNeighbours);
				// The coarse land draws every block from the landscape file's pages, where it has them
				glm::vec4 u_coarsePage = island.GetCoarsePageSlot(index);
				u_coarsePage.w = coarseLand && island.GetCoarsePages() != nullptr ? u_coarsePage.w : 0.0f;
				coarsePageUniform.Set(&u_coarsePage);
				glm::vec4 vortexGround {0.0f, 0.0f, 0.0f, -1.0f};
				const GroundTextures* blockGround = sharedGround;
				const auto mark = std::ranges::find(groundMarks | std::views::reverse, block.GetBlockPosition(),
				                                    &ecs::systems::VortexSystemInterface::GroundMark::block);
				if (mark != (groundMarks | std::views::reverse).end())
				{
					const auto& marked = ecs::components::Vortex::GroundTexturesOf(mark->type);
					if (groundTexturesLoaded(marked))
					{
						vortexGround = {mark->centre, 1.0f / (vortex::k_GroundTextureSpan * mark->baseScale),
						                static_cast<float>(mark->holeThreshold)};
						blockGround = &marked;
					}
				}
				if (blockGround != nullptr)
				{
					terrainShader->SetTextureSampler("s6_vortexHole", 6, *textures.Handle(blockGround->hole));
					terrainShader->SetTextureSampler("s13_vortexRing", 13, *textures.Handle(blockGround->ring));
				}
				vortexGroundUniform.Set(&vortexGround);
				// The moving shadows reaching the block
				const auto shadows =
				    groundShadowed && index < _groundShadowBlocks.size() ? _groundShadowBlocks[index] : glm::vec2(0.0f);
				const glm::vec4 u_groundShadowBlock {shadows, 0.0f, 0.0f};
				groundShadowBlockUniform.Set(&u_groundShadowBlock);
				block.BindVertices();
				triangles->Bind(triangles->GetCount());
				bgfx::setState(defaultState | (desc.cullBack ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW), 0);
				terrainShader->Submit(static_cast<bgfx::ViewId>(landViewId), blockDepth(block), discard);
			};
			if (!blocks.empty())
			{
				// Only the blocks in view of the scene's own camera are drawn, also when they are mirrored
				const bool culled = desc.viewId == RenderPass::Main || desc.viewId == RenderPass::Reflection;
				// The land's pass keeps the order the blocks come in, so they come the nearest first. Each block's depth is
				// found one time, and only the blocks that are drawn are put in order.
				std::vector<std::pair<decltype(blockDepth(blocks.front())), size_t>> order;
				order.reserve(blocks.size());
				for (size_t index = 0; index < blocks.size(); ++index)
				{
					if (!culled || index >= _landDetail.inView.size() || _landDetail.inView[index])
					{
						order.emplace_back(blockDepth(blocks[index]), index);
					}
				}
				std::ranges::stable_sort(order, {}, [](const auto& entry) { return entry.first; });
				setTerrainUniforms();
				for (const auto& entry : order)
				{
					submitBlock(entry.second);
				}
			}
			_shaderManager->DiscardBindings();
			DrawCreatureFootprints(desc);
		}
	}

	{
		auto section = profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawModels
		                                                                          : Profiler::Stage::MainPassDrawModels);
		if (desc.drawEntities)
		{
			L3DMeshSubmitDesc submitDesc = {};
			submitDesc.viewId = desc.viewId;
			submitDesc.program = objectShaderInstanced;
			submitDesc.state = 0u                              //
			                   | BGFX_STATE_WRITE_MASK         //
			                   | BGFX_STATE_DEPTH_TEST_GREATER //
			                   | BGFX_STATE_MSAA               //
			    ;
			const auto& renderCtx = Locator::rendereringSystem::value().GetContext();

			// Instance meshes
			// A creature is drawn with its own pose, and its body blended from its meshes
			struct EntityPose
			{
				std::span<const glm::mat4> bones;
				const L3DMeshSubmitDesc::MorphTargets* morphTargets;
				/// Each instance's bones are read from the bone palette, where its instance says they start
				bool bonePalette {false};
				/// The body draws with the first environment map added over every part
				bool environmentMapped {false};
			};
			// The creatures are drawn with a light of their own, at every detail level
			bool drawingCreatures = false;
			// A thing that the game mirrors in the sea by itself is drawn in one flat colour with a colour added, and with no
			// light, haze, snow or shadows
			struct FlatLook
			{
				glm::vec3 colour;
				glm::vec3 added;
			};
			std::optional<FlatLook> flatReflection;
			// The thing that the hand holds, mirrored in the sea: flat in the land's light, the land's colour and the haze that
			// it kept the frame before, with nothing of its own added
			std::optional<hand_reflection::HeldLook> heldReflection;
			const auto drawInstancesWith = [&](L3DMeshSubmitDesc& sd, entt::id_type meshId,
			                                   const RenderContext::InstancedDrawDesc& placers, bool useMaterialBlending,
			                                   uint32_t first, uint32_t count, const EntityPose* pose = nullptr,
			                                   const L3DMeshSubmitDesc::MorphTargets* handMorph = nullptr,
			                                   std::optional<graphics::DynamicVertexBufferHandle> instances = std::nullopt,
			                                   const AnimalBoneGroup* posedGroup = nullptr) {
				// A mesh with none of its things placed this frame, such as the temple's leashes before a creature wears
				// one, has nothing to draw
				if (count == 0)
				{
					return;
				}
				auto mesh = meshManager.Handle(meshId);

				// The materials of a creature's body set its blending and culling. Thus the edges of the lion's mane
				// strands blend, and only the strands show their back faces. The passes of a fizzing creature keep
				// their own blending and culling.
				const bool creatureMaterials = drawingCreatures && !sd.fizz.has_value();
				sd.useMaterialBlending = useMaterialBlending || creatureMaterials;
				sd.alphaTexturedAdditive = placers.alphaTexturedAdditive;
				sd.shineColour = placers.shineColour;
				sd.additiveShare = placers.additiveShare;
				sd.instanceAlpha = placers.instanceAlpha;
				sd.changedMaterials = placers.changedMaterials;
				// A thing see-through by its alpha draws in the game's second table of modes. A thing whose materials the
				// game changed keeps its own rule.
				sd.seeThrough = placers.instanceAlpha && !placers.changedMaterials.has_value();
				sd.seeThroughRef.reset();
				if (const auto ref = renderCtx.seeThroughRefs.find(first);
				    sd.seeThrough && count == 1 && !instances.has_value() && ref != renderCtx.seeThroughRefs.end())
				{
					sd.seeThroughRef = ref->second;
				}
				// The game gives the rooms the temple's light; the hand keeps its own
				const bool templeLit = Locator::temple::has_value() && Locator::temple::value().Active() &&
				                       meshId != ecs::components::Hand::k_MeshId;
				const auto light = templeLit ? Locator::temple::value().GetLight() : TempleLight {};
				sd.tint = glm::vec4(light.multiply, 0.0f);
				// The temple's walking worshippers are drawn in a grey of their own, darker in the reflection
				if (placers.templeGrey.has_value())
				{
					const float grey = desc.viewId == RenderPass::Reflection ? placers.templeGrey->y : placers.templeGrey->x;
					sd.tint = glm::vec4(light.Colour(glm::vec3(grey)), 0.0f);
				}
				sd.lightMultiply = light.multiply;
				// The hand glows as a recognised gesture flashes
				sd.lightAdd = light.add + (meshId == ecs::components::Hand::k_MeshId ? HandGlowColour() : glm::vec3(0.0f));
				// Everything in the temple but the hand fades with its depth fog. The walls are fogged with the light's
				// colour still added; the worshippers have only the fog's added, which takes the light's place.
				sd.templeFog = templeLit;
				sd.fogFromFirstVertex = std::nullopt;
				if (templeLit && placers.templeGrey.has_value())
				{
					sd.lightAdd = glm::vec3(0.0f);
				}
				else if (templeLit)
				{
					sd.fogFromFirstVertex = Locator::temple::value().GetPosition();
				}
				// The fog scales the colour in the temple's light in whole bytes, before the model light shades it
				if (templeLit)
				{
					sd.objectLook =
					    L3DMeshSubmitDesc::ObjectLook {.colour = glm::floor(glm::vec3(sd.tint) * 255.0f + 0.5f), .alpha = 1.0f};
					sd.tint = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
				}
				sd.landLightScale = meshId == ecs::components::Hand::k_MeshId ? 1.5f : 1.0f;
				sd.snow = !inTemple && meshId != ecs::components::Hand::k_MeshId;
				sd.snowByFace = placers.snowByFace;
				// The villagers a script draws in high detail take no moving shadows, nor does the hand
				const bool highDetail =
				    std::ranges::any_of(renderCtx.highDetailDraws, [meshId](const auto& draw) { return draw.mesh == meshId; });
				sd.groundShadows = meshId != ecs::components::Hand::k_MeshId && !highDetail;
				sd.unlit = placers.unlit;
				// The creatures, the hand and the villagers a script draws in high detail are lit at every detail level
				sd.alwaysLit = drawingCreatures || highDetail || meshId == ecs::components::Hand::k_MeshId;
				sd.instanceDesc =
				    std::make_unique<graphics::InstanceDesc>(instances.value_or(renderCtx.instanceUniformBuffer), first, count);
				if (mesh->IsBoned())
				{
					const auto animated = renderCtx.animatedBoneMatrices.find(meshId);
					const auto& bones = animated != renderCtx.animatedBoneMatrices.end() &&
					                            animated->second.size() == mesh->GetBoneMatrices().size()
					                        ? animated->second
					                        : mesh->GetBoneMatrices();
					sd.modelMatrices = bones.data();
					sd.matrixCount = static_cast<uint8_t>(bones.size());
					// TODO(bwrsandman): Get animation frame instead of default for the other boned meshes
				}
				else
				{
					const static auto identity = glm::mat4(1.0f);
					sd.modelMatrices = &identity;
					sd.matrixCount = 1;
				}
				sd.isSky = false;
				sd.onlyJoints = placers.onlyJoints;
				sd.hideShutJoints = placers.hideShutJoints;
				// The temple's meshes and the creatures, which are drawn as their materials say
				sd.useMaterialCulling = placers.materialBlending || creatureMaterials;
				sd.useMaterialAddressing = creatureMaterials;
				sd.mirrored = desc.cullBack;
				sd.uvOffset = placers.uvOffset;
				sd.uvOffsetByMaterial = placers.uvOffsetByMaterial;
				sd.subMeshTextures = placers.subMeshTextures;
				sd.pictureSubMeshes = placers.pictureSubMeshes;
				sd.controlSubMeshes = placers.controlSubMeshes;
				sd.subMeshGlows = placers.subMeshGlows;
				sd.subMeshOffsets = placers.subMeshOffsets;
				sd.hiddenSubMeshes = placers.hiddenSubMeshes;
				// The rooms meet at their doorways, whose arches each room has a copy of: the player's room's is seen
				sd.depthBias = placers.behindCurrentRoom ? k_OtherTempleRoomDepthBias : 0.0f;
				sd.joints = {};
				if (Locator::temple::has_value() && Locator::temple::value().Active())
				{
					sd.joints = Locator::temple::value().GetDoors().GetJoints();
				}
				sd.morphWithTerrain = placers.morphWithTerrain;
				sd.program = objectProgramFor(sd.matrixCount, sd.morphWithTerrain);
				// Only the temple's meshes have lightmaps, and they don't stand on the land
				sd.lightmapProgram = sd.morphWithTerrain       ? nullptr
				                     : placers.showsReflection ? objectShaderReflectiveLightmapInstanced
				                                               : objectShaderLightmapInstanced;
				if (placers.showsReflection)
				{
					objectShaderReflectiveLightmapInstanced->SetTextureSampler(
					    sd.encoder != nullptr ? *sd.encoder : _shaderManager->GetMainEncoder(), "s_reflection", 4,
					    Locator::oceanSystem::value().GetReflectionFramebuffer().GetColorAttachment());
				}

				sd.morphTargets = nullptr;
				sd.bonePalette = nullptr;
				if (pose != nullptr)
				{
					sd.landLightScale = CreatureLandLightScale(desc.viewId);
					sd.modelMatrices = pose->bones.data();
					sd.matrixCount = static_cast<uint8_t>(pose->bones.size());
					sd.program = objectProgramFor(sd.matrixCount, sd.morphWithTerrain);
					if (pose->bonePalette)
					{
						const static auto identity = glm::mat4(1.0f);
						sd.modelMatrices = &identity;
						sd.matrixCount = 1;
						sd.program = objectShaderPaletteInstanced;
						sd.lightmapProgram = nullptr;
						sd.bonePalette = renderCtx.bonePaletteTexture.get();
					}
					if (pose->morphTargets != nullptr)
					{
						sd.program = objectShaderMorphInstanced;
						sd.morphTargets = pose->morphTargets;
					}
				}
				// The hand pulled towards its evil or good mesh, keeping its own light
				if (handMorph != nullptr)
				{
					sd.program = objectShaderMorphInstanced;
					sd.morphTargets = handMorph;
				}
				// The animals of a model all at once, each posed by its own bones from the texture
				sd.boneTexture = nullptr;
				if (posedGroup != nullptr && _animalBoneTexture)
				{
					sd.landLightScale = CreatureLandLightScale(desc.viewId);
					sd.program = objectShaderPosedInstanced;
					const static auto identity = glm::mat4(1.0f);
					sd.modelMatrices = &identity;
					sd.matrixCount = 1;
					const auto size = _animalBoneTexture->GetResolution();
					sd.boneTexture = _animalBoneTexture.get();
					sd.bones = {static_cast<float>(posedGroup->firstMatrix), static_cast<float>(posedGroup->bones),
					            static_cast<float>(size.x), static_cast<float>(size.y)};
				}

				if (flatReflection.has_value())
				{
					sd.tint = glm::vec4(flatReflection->colour, 1.0f);
					sd.lightAdd = flatReflection->added;
					sd.landLightAndHaze = false;
					sd.snow = false;
					sd.groundShadows = false;
				}
				// The scripts' scrolls and signs take the first environment map, each primitive as its material says. The
				// sea mirrors them without it.
				const bool reflected = desc.viewId == RenderPass::Reflection ||
				                       desc.viewId == RenderPass::ReflectionTranslucent ||
				                       desc.viewId == RenderPass::ReflectionMirrored || flatReflection.has_value();
				const bool environmentMapped = placers.environmentMapped && environmentMap != nullptr && !reflected &&
				                               sd.program == objectShaderStaticInstanced;
				sd.environment = environmentMapped ? environmentMap : nullptr;
				sd.environmentStageForAll.reset();
				if (environmentMapped)
				{
					sd.program = objectShaderEnvironmentStaticInstanced;
				}
				// A creature's body with the environment map takes it on every part, added over the lit texture. The sea
				// mirrors the body without it.
				if (pose != nullptr && pose->environmentMapped && environmentMap != nullptr && !reflected &&
				    sd.program == objectShaderMorphInstanced)
				{
					sd.program = objectShaderMorphEnvironmentInstanced;
					sd.environment = environmentMap;
					sd.environmentStageForAll = material_stages::k_CreatureBodyStage;
				}
				if (heldReflection.has_value())
				{
					sd.shadeAt = glm::vec4(heldReflection->point, hand_reflection::k_HeldShade);
					sd.heldMirror = heldReflection;
					sd.unlit = true;
					sd.landLightScale = 1.0f;
					sd.objectLook.reset();
					sd.snow = false;
					sd.groundShadows = false;
				}

				// TODO(bwrsandman): choose the correct LOD
				DrawMesh(*mesh, sd, std::numeric_limits<uint8_t>::max());
				sd.morphTargets = nullptr;
				sd.environment = nullptr;
				sd.environmentStageForAll.reset();
				sd.landLightAndHaze = true;
				sd.shadeAt = glm::vec4(0.0f);
				sd.heldMirror.reset();
				if (templeLit)
				{
					sd.objectLook.reset();
				}
			};
			// Most draws use the one submit desc of the pass
			const auto drawInstances = [&](auto&&... args) {
				drawInstancesWith(submitDesc, std::forward<decltype(args)>(args)...);
			};
			// The solid things after a fizzing creature in the land's draw list come after the passes of the creature.
			// They go in the sorted view, each segment of the list at the depth after the creature before it. Thus they
			// do not show where the creature wrote its depth, as in the game. Segment 0 stays in this view.
			const bool splitByFizz =
			    desc.viewId == RenderPass::Main && translucentViewId != desc.viewId && !renderCtx.fizzOrder.Empty();
			const auto inSegmentWith = [&](L3DMeshSubmitDesc& sd, uint32_t segment, const auto& draw) {
				if (!splitByFizz || segment == 0)
				{
					draw();
					return;
				}
				const auto view = sd.viewId;
				const auto depth = sd.sortDepth;
				sd.viewId = translucentViewId;
				sd.sortDepth = graphics::fizz_draw_order::SegmentDepth(segment);
				draw();
				sd.viewId = view;
				sd.sortDepth = depth;
			};
			const auto inSegment = [&](uint32_t segment, const auto& draw) { inSegmentWith(submitDesc, segment, draw); };
			// What the hand holds is mirrored with the hand, after it, so the rest of the mirrored world leaves it out
			const auto mirroredWithHand = [&](uint32_t instance) {
				return mirroredViewId != desc.viewId &&
				       std::ranges::any_of(renderCtx.inHandDraws,
				                           [instance](const auto& held) { return held.instance == instance; });
			};
			// Call `draw(first, count)` for each run of instances that the hand does not hold
			const auto withoutHeld = [&](uint32_t first, uint32_t count, const auto& draw) {
				if (mirroredViewId == desc.viewId || renderCtx.inHandDraws.empty())
				{
					draw(first, count);
					return;
				}
				uint32_t start = first;
				for (uint32_t instance = first; instance < first + count; ++instance)
				{
					if (mirroredWithHand(instance))
					{
						if (instance > start)
						{
							draw(start, instance - start);
						}
						start = instance + 1;
					}
				}
				if (first + count > start)
				{
					draw(start, first + count - start);
				}
			};
			// Call `draw(first, count)` for the placed instances of each segment, each in the place of its segment
			const auto inSegmentsWith = [&](L3DMeshSubmitDesc& sd, const RenderContext::InstancedDrawDesc& placers,
			                                const auto& draw) {
				const auto starts =
				    splitByFizz ? std::span<const uint32_t>(placers.segmentStarts) : std::span<const uint32_t>();
				graphics::fizz_draw_order::ForEachSegment(
				    placers.offset, placers.filled, starts, [&](uint32_t first, uint32_t count, uint32_t segment) {
					    inSegmentWith(sd, segment, [&] { withoutHeld(first, count, draw); });
				    });
			};
			const auto inSegments = [&](const RenderContext::InstancedDrawDesc& placers, const auto& draw) {
				inSegmentsWith(submitDesc, placers, draw);
			};
			// The view of this pass, as the pass sets it. An instance out of it puts no pixel on the screen, so the pass
			// does not draw it. A warm-up draws all instances, so that each draw makes its pipeline.
			const auto seenBy = view_frustum::FromViewProjection(desc.camera->GetProjectionMatrix() *
			                                                     desc.camera->GetViewMatrix(Camera::Interpolation::Current));
			const auto instanceSeen = [&](const L3DMesh& mesh, const RenderContext::InstancedDrawDesc& placers,
			                              uint32_t instance) {
				// The bones of a model can move its parts out of its box, and so can the offsets of its submeshes
				if (_warmingUp || mesh.IsBoned() || placers.bonePalette || placers.perEntity ||
				    !placers.subMeshOffsets.empty() || instance >= renderCtx.instanceUniforms.size())
				{
					return true;
				}
				const auto& model = renderCtx.instanceUniforms[instance].model;
				const auto& box = mesh.GetBoundingBox();
				const bool boxSeen = view_frustum::SeesBox(seenBy, model, box.minima, box.maxima);
				if (!placers.morphWithTerrain || boxSeen)
				{
					return boxSeen;
				}
				// A model that goes with the land moves its points along its up, by no more than the highest land under
				// its box. The test reads the land only when the highest land that a cell can have puts the box in view.
				const auto up = view_frustum::LandRiseSweep(model, 1.0f);
				if (!up.has_value())
				{
					return true;
				}
				if (!view_frustum::SeesSweptBox(seenBy, model, box.minima, box.maxima, *up * view_frustum::k_HighestLand))
				{
					return false;
				}
				const auto& land = Locator::terrainSystem::value();
				const float highestRise =
				    view_frustum::HighestLandUnder(model, box.minima, box.maxima, [&land](glm::ivec2 corner) {
					    return land.GetCell(glm::u16vec2(corner)).altitude;
				    });
				return view_frustum::SeesSweptBox(seenBy, model, box.minima, box.maxima, *up * highestRise);
			};
			// A model whose parts differ by detail is drawn one instance at a time, each with the parts of its own. A
			// draw of all the other models' instances starts at the first instance in view and stops after the last.
			const auto drawPlaced = [&](L3DMeshSubmitDesc& sd, entt::id_type meshId, const L3DMesh& mesh,
			                            const RenderContext::InstancedDrawDesc& placers, bool useMaterialBlending) {
				inSegmentsWith(sd, placers, [&](uint32_t first, uint32_t count) {
					const auto seen = [&](uint32_t instance) { return instanceSeen(mesh, placers, instance); };
					if (!placers.partsByLevel)
					{
						const auto instances = std::views::iota(first, first + count);
						const auto start = std::ranges::find_if(instances, seen);
						if (start == instances.end())
						{
							return;
						}
						const auto last = *std::ranges::find_if(instances | std::views::reverse, seen);
						drawInstancesWith(sd, meshId, placers, useMaterialBlending, *start, last + 1 - *start);
						return;
					}
					for (uint32_t instance = first; instance < first + count; ++instance)
					{
						if (seen(instance))
						{
							sd.detailLevel = renderCtx.instanceLevels.at(instance);
							drawInstancesWith(sd, meshId, placers, useMaterialBlending, instance, 1);
						}
					}
					sd.detailLevel = 1;
				});
			};
			// The things placed this frame. The things too far away to draw come after them. Each model starts from the
			// submit desc of the pass, thus the threads can draw the models at the same time. The draws keep the order of
			// the models. This loop loads the models, thus the threads only read them.
			struct Placed
			{
				entt::id_type meshId;
				const L3DMesh* mesh;
				const RenderContext::InstancedDrawDesc* placers;
			};
			std::vector<Placed> placed;
			placed.reserve(renderCtx.instancedDrawDescs.size());
			for (const auto& [meshId, placers] : renderCtx.instancedDrawDescs)
			{
				if (meshId != ecs::components::Hand::k_MeshId && !placers.translucent && !placers.perEntity &&
				    !(desc.viewId == RenderPass::Reflection && placers.hiddenFromReflection))
				{
					placed.push_back({.meshId = meshId, .mesh = &*meshManager.Handle(meshId), .placers = &placers});
				}
			}
			EncodeInOrder(placed.size(), [&](size_t index, DrawEncoder& to) {
				const auto& [meshId, mesh, placers] = placed[index];
				L3DMeshSubmitDesc own = {};
				own.viewId = submitDesc.viewId;
				own.encoder = &to;
				own.program = submitDesc.program;
				own.state = submitDesc.state;
				drawPlaced(own, meshId, *mesh, *placers, placers->materialBlending);
			});
			// A broken or unfinished building's whole model, as far up as it stands, from both sides and with its inner
			// walls, and its scaffold sunk into the land or cut down from the top
			// One fading out in the distance is drawn with the blended things, by its alpha
			// A warm-up draws it with an instance of its own at no size
			const auto drawPartialBuild = [&](const RenderContext::PartialBuildDraw& build,
			                                  std::optional<graphics::DynamicVertexBufferHandle> hidden = std::nullopt) {
				if (!meshManager.Contains(build.meshId))
				{
					return;
				}
				RenderContext::InstancedDrawDesc placers {0, 1, build.morphWithTerrain};
				placers.instanceAlpha = build.fading;
				// A model that follows the land is cut and capped where its corners rise to with the land
				const auto& instances = renderCtx.partialBuildInstances;
				const auto landPlacement = build.morphWithTerrain && !hidden.has_value() && build.instance < instances.size()
				                               ? std::optional(instances.at(build.instance).model)
				                               : std::nullopt;
				// A part cut at a height is drawn from both sides; a scaffold standing whole, only sunk into the land, is
				// drawn as the building's own model is
				const auto drawPart = [&](uint32_t instance, std::optional<float> cut, std::optional<uint32_t> status,
				                          bool innerWalls) {
					submitDesc.cutAbove = cut;
					const bool landCut = build.morphWithTerrain && !landPlacement.has_value();
					submitDesc.modelCutHeight = status.has_value() || landCut ? std::nullopt : build.capHeight;
					submitDesc.landPlacement = status.has_value() ? std::nullopt : landPlacement;
					submitDesc.twoSided = cut.has_value();
					submitDesc.innerWalls = innerWalls;
					submitDesc.innerWallInset = build.innerWallInset;
					submitDesc.onlyStatus = status;
					drawInstances(build.meshId, placers, false, hidden.has_value() ? 0 : instance, 1, nullptr, nullptr,
					              hidden.value_or(renderCtx.partialBuildInstanceBuffer));
					submitDesc.cutAbove.reset();
					submitDesc.modelCutHeight.reset();
					submitDesc.landPlacement.reset();
					submitDesc.twoSided = false;
					submitDesc.innerWalls = false;
					submitDesc.innerWallInset.reset();
					submitDesc.onlyStatus.reset();
				};
				if (build.modelCut.has_value())
				{
					drawPart(build.instance, build.modelCut, std::nullopt, false);
					drawPart(build.instance, build.modelCut, std::nullopt, true);
					// Then the cap over the cut walls, joining the outer to the inner. The cap of a model that follows the
					// land already rises with the land, thus the renderer draws it as the cap of a model that does not.
					if (!build.morphWithTerrain || landPlacement.has_value())
					{
						const bool followsLand = placers.morphWithTerrain;
						placers.morphWithTerrain = false;
						submitDesc.cap = true;
						drawPart(build.instance, build.modelCut, std::nullopt, false);
						submitDesc.cap = false;
						placers.morphWithTerrain = followsLand;
					}
				}
				if (build.scaffoldStatus.has_value())
				{
					drawPart(build.scaffoldInstance, build.scaffoldCut, build.scaffoldStatus, false);
				}
				// A graveyard's graves, as many as its burials show, over its model drawn with the others
				if (build.gravesShown.has_value())
				{
					submitDesc.statusesUpTo = build.gravesShown;
					drawInstances(build.meshId, placers, false, hidden.has_value() ? 0 : build.instance, 1, nullptr, nullptr,
					              hidden.value_or(renderCtx.partialBuildInstanceBuffer));
					submitDesc.statusesUpTo.reset();
				}
			};
			for (const auto& build : renderCtx.partialBuilds)
			{
				if (!build.fading)
				{
					inSegment(build.segment, [&] { drawPartialBuild(build); });
				}
			}
			// The creatures, each posed and shaped as it is, then its eyes
			drawingCreatures = true;
			// A creature fizzing in or out of sight blends with what is behind it, but in its place among the solid
			// models: after them in this view, before all that blends, the nearest fizzing creature first. First its depth
			// alone, where the static scrolling over its skin is strong enough, then its body blended over just that
			// depth. Frozen, the ice is then added over all of its mesh where nothing nearer hides it, and lays no depth.
			// Returns the sort depth of its last pass.
			const auto drawFizzing = [&](entt::id_type meshId, const RenderContext::InstancedDrawDesc& placers,
			                             uint32_t instance, std::optional<graphics::DynamicVertexBufferHandle> instances,
			                             const EntityPose& pose, uint8_t level, glm::vec2 scroll, uint32_t depth,
			                             float freeze) {
				const auto mainState = submitDesc.state;
				submitDesc.viewId = translucentViewId;
				submitDesc.fizz =
				    L3DMeshSubmitDesc::Fizz {.pass = L3DMeshSubmitDesc::Fizz::Pass::Depth, .level = level, .scroll = scroll};
				submitDesc.state = BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
				submitDesc.sortDepth = depth;
				drawInstances(meshId, placers, false, instance, 1, &pose, nullptr, instances);
				submitDesc.fizz->pass = L3DMeshSubmitDesc::Fizz::Pass::Body;
				submitDesc.state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_EQUAL |
				                   BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;
				submitDesc.sortDepth = depth > 0 ? depth - 1 : 0;
				drawInstances(meshId, placers, false, instance, 1, &pose, nullptr, instances);
				if (creature_spells::IceAlpha(freeze) > 0)
				{
					submitDesc.fizz->pass = L3DMeshSubmitDesc::Fizz::Pass::Ice;
					submitDesc.freeze = freeze;
					submitDesc.state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_GEQUAL |
					                   BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE) |
					                   BGFX_STATE_MSAA;
					submitDesc.sortDepth = submitDesc.sortDepth > 0 ? submitDesc.sortDepth - 1 : 0;
					drawInstances(meshId, placers, false, instance, 1, &pose, nullptr, instances);
					submitDesc.freeze = 0.0f;
				}
				const auto bodyDepth = submitDesc.sortDepth;
				submitDesc.fizz.reset();
				submitDesc.state = mainState;
				submitDesc.sortDepth = 0;
				submitDesc.viewId = desc.viewId;
				return bodyDepth;
			};
			for (const auto& drawn : _drawnCreatures)
			{
				const auto entity = drawn.entity;
				const auto instance = drawn.instance;
				// A copy drawn elsewhere looks as its creature does, in a pose of its own
				const bool copy = drawn.posedBy != entt::null;
				const auto poser = copy ? drawn.posedBy : entity;
				// The copy is a thing of the temple's rooms
				const auto& posers = copy && Locator::temple::has_value() ? Locator::temple::value().GetRooms() : desc.entities;
				const auto* mesh = desc.entities.TryGet<const ecs::components::Mesh>(entity);
				const auto* creature = desc.entities.TryGet<const ecs::components::Creature>(entity);
				const auto* morph = desc.entities.TryGet<const ecs::components::CreatureMorph>(entity);
				const auto* animation =
				    posers.Valid(poser) ? posers.TryGet<const ecs::components::CreatureAnimation>(poser) : nullptr;
				if (mesh == nullptr || !meshManager.Contains(mesh->id) || !posers.Valid(poser))
				{
					continue;
				}
				const auto placers = renderCtx.instancedDrawDescs.find(mesh->id);
				if (placers == renderCtx.instancedDrawDescs.end() ||
				    (desc.viewId == RenderPass::Reflection && placers->second.hiddenFromReflection))
				{
					continue;
				}
				const auto bodyMesh = meshManager.Handle(mesh->id);
				std::optional<L3DMeshSubmitDesc::MorphTargets> targets;
				if (creature != nullptr && morph != nullptr)
				{
					const auto ids = creature_morph::MeshesOf(
					    creature->species, morph->drawn, [&meshManager](entt::id_type id) { return meshManager.Contains(id); });
					targets = L3DMeshSubmitDesc::MorphTargets {
					    .meshes = {&*meshManager.Handle(ids.evilGood), &*meshManager.Handle(ids.thinFat),
					               &*meshManager.Handle(ids.weakStrong)},
					    .weights = glm::abs(glm::vec3(morph->drawn.evilGood, morph->drawn.thinFat, morph->drawn.weakStrong)),
					    .skins = {},
					    .blendSeams = Locator::config::value().blendCreatureSeams,
					};
					// A copy with a skin of its own, as the Creature Cave's copy has, shows that skin
					if (const auto own = copy ? _roomCreatureSkins.find(poser) : _roomCreatureSkins.end();
					    own != _roomCreatureSkins.end())
					{
						targets->skins = own->second.drawn;
					}
					else if (const auto skins = _creatureSkins.find(entity); skins != _creatureSkins.end())
					{
						targets->skins = skins->second.drawn;
					}
				}
				const bool posed = animation != nullptr && animation->boneMatrices.size() == bodyMesh->GetBoneMatrices().size();
				// The environment map belongs to the creature's own body: a copy drawn elsewhere has its own body, without it
				const uint32_t environmentWord = creature != nullptr && !copy ? creature->environmentMap : 0;
				const EntityPose pose {
				    .bones = posed ? std::span<const glm::mat4>(animation->boneMatrices)
				                   : std::span<const glm::mat4>(bodyMesh->GetBoneMatrices()),
				    .morphTargets = targets ? &*targets : nullptr,
				    .environmentMapped = creature_environment_map::DrawnWithMap(environmentWord),
				};
				if (pose.bones.empty())
				{
					continue;
				}
				// A copy shows none of its creature's frozen or fizzing look, and is lit by a light of its own
				const auto* spells = posers.TryGet<const ecs::components::CreatureSpells>(poser);
				const float fizz = spells != nullptr ? spells->fizz : 0.0f;
				submitDesc.modelLight = drawn.light;
				if (desc.viewId == RenderPass::Reflection)
				{
					// The sea reflects the body of the local player's creature alone, without its eyes or hair, whole.
					// It does so only while the creature stands low on a near land block in view, and until it has fizzed
					// a fifth of the way out.
					const auto position = instance < renderCtx.instanceUniforms.size()
					                          ? glm::vec3(renderCtx.instanceUniforms.at(instance).model[3])
					                          : glm::vec3(0.0f);
					// It shows none of the creature's frozen look. It comes after the hand and what the hand holds.
					if (!copy && CreatureMirrored(entity, position, fizz))
					{
						flatReflection =
						    FlatLook {.colour = creature_reflection::Colour(), .added = creature_reflection::Added()};
						submitDesc.iceShown = false;
						submitDesc.viewId = mirroredViewId;
						submitDesc.sortDepth = hand_reflection::SortKey(hand_reflection::MirroredThing::Creature);
						drawInstances(mesh->id, placers->second, placers->second.materialBlending, instance, 1, &pose);
						submitDesc.sortDepth = 0;
						submitDesc.viewId = desc.viewId;
						submitDesc.iceShown = true;
						flatReflection.reset();
					}
					submitDesc.modelLight.reset();
					continue;
				}
				uint32_t eyesSortDepth = 0;
				// The game lights the body as it poses it, with the posed models' rule, also while the body fizzes. The
				// eyes keep the usual model light.
				submitDesc.posedModelLight = true;
				if (creature_environment_map::BodyFizzes(fizz, environmentWord) && instance < renderCtx.instanceUniforms.size())
				{
					// A creature in the land's draw list takes its place in the order of the list. Any other creature
					// takes its place by its distance, after the creatures of the list.
					const auto position = glm::vec3(renderCtx.instanceUniforms.at(instance).model[3]);
					const auto rank = copy ? std::nullopt : renderCtx.fizzOrder.RankOf(entity);
					const auto depth = rank.has_value() ? graphics::fizz_draw_order::CreatureDepth(*rank)
					                                    : zsort::BeforeSorted(position, cameraOrigin);
					eyesSortDepth = drawFizzing(mesh->id, placers->second, instance, std::nullopt, pose,
					                            creature_fizz_look::Level(fizz), spells->staticScroll, depth, spells->freeze);
				}
				else if (creature_environment_map::BodyDrawn(fizz, environmentWord))
				{
					inSegment(copy ? 0 : renderCtx.fizzOrder.SegmentOf(entity), [&] {
						drawInstances(mesh->id, placers->second, placers->second.materialBlending, instance, 1, &pose);
					});
				}
				submitDesc.posedModelLight = false;
				const float freeze = spells != nullptr ? spells->freeze : 0.0f;
				if (const auto* eyes = posers.TryGet<const ecs::components::CreatureEyes>(poser); eyes != nullptr)
				{
					DrawCreatureEyes(desc, *eyes, submitDesc, fizz, freeze, eyesSortDepth);
				}
				submitDesc.modelLight.reset();
				// The hair is drawn as it is until the creature has fizzed or frozen a fifth of the way, then not at all. With
				// the environment map, the hair shows at all times. A copy draws the hair of its own, which hangs from its
				// own pose.
				const auto* hair = posers.TryGet<const ecs::components::CreatureHair>(poser);
				const auto* transform = posers.TryGet<const ecs::components::Transform>(poser);
				if (creature_environment_map::HairShown(fizz, freeze, environmentWord) && hair != nullptr &&
				    transform != nullptr && Locator::creatureHairSystem::has_value() &&
				    Locator::creatureHairSystem::value().IsShown())
				{
					// The hair hangs from the body where the frame draws it. The hair of a body with the environment map
					// takes the colours of the last usual hair before it.
					const auto origin = ecs::components::DrawnTransform(
					                        *transform, desc.entities.TryGet<const ecs::components::CreatureAnimation>(poser))
					                        .position;
					if (creature_environment_map::DrawnWithMap(environmentWord))
					{
						DrawEnvironmentHair(desc, entity, *hair, origin);
					}
					else
					{
						DrawCreatureHair(desc, *hair, origin);
					}
				}
				// The glows on its body, where its pose put them
				if (const auto* glows = posers.TryGet<const ecs::components::CreatureHandGlows>(poser);
				    glows != nullptr && transform != nullptr)
				{
					DrawCreatureHandGlows(static_cast<uint16_t>(TranslucentView(desc.viewId)), *glows,
					                      desc.camera->GetProjectionMatrix(), desc.viewId == RenderPass::Reflection,
					                      zsort::Depth(transform->position, cameraOrigin));
				}
			}
			// The creatures that the interface shows over itself, each moved for its draw to its place before the
			// interface's own camera, in a view of their own
			if (desc.viewId == RenderPass::Main && _interfaceCreatures.has_value())
			{
				DrawInterfaceCreatures(desc, [&](entt::id_type meshId, uint32_t instance, std::span<const glm::mat4> bones,
				                                 const L3DMeshSubmitDesc::MorphTargets* targets,
				                                 const ecs::components::CreatureEyes* eyes) {
					const auto placers = renderCtx.instancedDrawDescs.find(meshId);
					if (placers == renderCtx.instancedDrawDescs.end())
					{
						return;
					}
					const EntityPose pose {.bones = bones, .morphTargets = targets};
					submitDesc.viewId = RenderPass::InterfaceCreatures;
					// The body takes the posed models' light, as in the world
					submitDesc.posedModelLight = true;
					drawInstances(meshId, placers->second, placers->second.materialBlending, instance, 1, &pose, nullptr,
					              _interfaceCreatureInstances);
					submitDesc.posedModelLight = false;
					if (eyes != nullptr)
					{
						DrawCreatureEyes(desc, *eyes, submitDesc, 0.0f, 0.0f, 0);
					}
					submitDesc.viewId = desc.viewId;
				});
			}
			// The creature that the main menu's tattoo editor shows, in the editor's view of its own. Its mesh can be a
			// mesh that the scene does not have, so it has an instance of its own.
			if (desc.viewId == RenderPass::Main && _menuCreature.has_value())
			{
				DrawMenuCreature(desc, [&](entt::id_type meshId, uint32_t instance, std::span<const glm::mat4> bones,
				                           const L3DMeshSubmitDesc::MorphTargets* targets,
				                           const ecs::components::CreatureEyes* eyes) {
					if (!meshManager.Contains(meshId) || !_menuCreatureInstance.has_value())
					{
						return;
					}
					const RenderContext::InstancedDrawDesc placers {0, 1, false};
					const EntityPose pose {.bones = bones, .morphTargets = targets};
					submitDesc.viewId = RenderPass::MenuCreature;
					// The body takes the posed models' light, as in the world
					submitDesc.posedModelLight = true;
					drawInstances(meshId, placers, false, instance, 1, &pose, nullptr, _menuCreatureInstance);
					submitDesc.posedModelLight = false;
					if (eyes != nullptr)
					{
						DrawCreatureEyes(desc, *eyes, submitDesc, 0.0f, 0.0f, 0);
					}
					submitDesc.viewId = desc.viewId;
				});
			}
			// A warm-up draws a body of each species that is loaded, with its eyes and hair, as the first frame of a creature
			// draws them. They are drawn at no size, and cover no pixel.
			if (_warmingUp && _warmUpInstances.has_value())
			{
				const RenderContext::InstancedDrawDesc placers {0, 1, false};
				const auto loaded = [&meshManager](entt::id_type id) { return meshManager.Contains(id); };
				for (int32_t type = static_cast<int32_t>(CreatureType::Cow); type < static_cast<int32_t>(CreatureType::_COUNT);
				     ++type)
				{
					const auto ids =
					    creature_morph::MeshesOf(static_cast<CreatureType>(type), creature_morph::Morph {}, loaded);
					if (!meshManager.Contains(ids.base))
					{
						continue;
					}
					const auto body = meshManager.Handle(ids.base);
					const L3DMeshSubmitDesc::MorphTargets targets {
					    .meshes = {&*meshManager.Handle(ids.evilGood), &*meshManager.Handle(ids.thinFat),
					               &*meshManager.Handle(ids.weakStrong)},
					    .weights = glm::vec3(0.0f),
					    .skins = {},
					    .blendSeams = Locator::config::value().blendCreatureSeams,
					};
					const EntityPose pose {.bones = body->GetBoneMatrices(), .morphTargets = &targets};
					if (pose.bones.empty())
					{
						continue;
					}
					// The body takes the posed models' light, as in the world, but not in the reflection
					submitDesc.posedModelLight = desc.viewId != RenderPass::Reflection;
					drawInstances(ids.base, placers, false, 0, 1, &pose, nullptr, *_warmUpInstances);
					// And fizzing, as it comes into sight
					if (desc.viewId != RenderPass::Reflection)
					{
						drawFizzing(ids.base, placers, 0, *_warmUpInstances, pose,
						            creature_fizz_look::Level(warm_up::k_CreatureFizz), glm::vec2(0.0f), 0,
						            warm_up::k_CreatureFreeze);
					}
					submitDesc.posedModelLight = false;
				}
				if (desc.viewId != RenderPass::Reflection)
				{
					ecs::components::CreatureEyes eyes;
					eyes.drawn.at(0) = {.eyeball = glm::mat4(0.0f), .eyelid = glm::mat4(0.0f)};
					DrawCreatureEyes(desc, eyes, submitDesc, 0.0f, 0.0f, 0);
					DrawCreatureEyes(desc, eyes, submitDesc, warm_up::k_CreatureFizz, 0.0f, 0);
					ecs::components::CreatureHair hair;
					hair.groups.push_back(
					    {.strands = {{.positions = {glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)}, .velocities = {}}},
					     .colour = glm::ivec3(0),
					     .halfWidth = 0.0f,
					     .textured = true});
					DrawCreatureHair(desc, hair, glm::vec3(0.0f));
				}
			}
			drawingCreatures = false;
			// The animals, each posed by its own clip and in its own light, as the mesh its distance chose; one fading out,
			// by itself or in the distance, blends over the rest
			const auto drawAnimal = [&](entt::entity entity, uint32_t instance, bool translucent) {
				const auto* pose = desc.entities.TryGet<const ecs::components::AnimalPose>(entity);
				const auto holder = HolderOf(renderCtx, instance);
				if (pose == nullptr || !holder.has_value() || pose->bones.empty() ||
				    (pose->alpha < 255 || holder->fading) != translucent || !meshManager.Contains(holder->mesh) ||
				    (!holder->fading && _animalBoneGroups.contains(holder->mesh)))
				{
					return;
				}
				const auto* mesh = &holder->mesh;
				// How opaque it is drawn: its own alpha, faded further in the distance
				const float distanceAlpha =
				    holder->fading ? 1.0f + std::min(renderCtx.instanceUniforms.at(instance).look.z, 0.0f) : 1.0f;
				const float alpha = (static_cast<float>(pose->alpha) / 255.0f) * distanceAlpha;
				// Blended by that alpha alone, not by its instance's as well
				auto drawn = std::make_pair(holder->mesh, *holder->placers);
				drawn.second.instanceAlpha = false;
				const auto* placers = &drawn;
				// A swimmer drawn under the sea as it is isn't mirrored in it, as the game mirrors only the land
				if (desc.viewId == RenderPass::Reflection &&
				    (placers->second.hiddenFromReflection || pose->underSea.has_value()))
				{
					return;
				}
				const EntityPose entityPose {.bones = pose->bones, .morphTargets = nullptr};
				// What of a swimmer is under the sea is drawn beneath the sea's surface, which the sea is then blended
				// over: in its colour under the sea, and nothing of it above the sea's level
				if (desc.viewId == RenderPass::Main && desc.drawWater && pose->underSea.has_value() && alpha >= 1.0f)
				{
					const auto view = std::exchange(submitDesc.viewId, RenderPass::UnderSea);
					submitDesc.objectLook = L3DMeshSubmitDesc::ObjectLook {.colour = *pose->underSea, .alpha = 1.0f};
					submitDesc.cutAbove = pose->cutBelow.value_or(0.0f);
					drawInstances(*mesh, placers->second, placers->second.materialBlending, instance, 1, &entityPose);
					submitDesc.cutAbove.reset();
					submitDesc.objectLook.reset();
					submitDesc.viewId = view;
				}
				if (!pose->aboveCutShown)
				{
					return;
				}
				glm::vec3 colour(255.0f);
				if (pose->light == ecs::components::AnimalLight::BrightestLand && _landLightTable)
				{
					const auto texel = _landLightTable->GetTexels().back();
					colour = glm::vec3(static_cast<float>(texel & 0xFFu), static_cast<float>((texel >> 8) & 0xFFu),
					                   static_cast<float>((texel >> 16) & 0xFFu));
				}
				else if (pose->light == ecs::components::AnimalLight::Own)
				{
					colour = pose->colour;
				}
				// One lit by the land where it stands takes no colour of its own, only its alpha when it fades
				if (pose->light != ecs::components::AnimalLight::Land || alpha < 1.0f)
				{
					submitDesc.objectLook = L3DMeshSubmitDesc::ObjectLook {.colour = colour, .alpha = alpha};
				}
				// An animal cut by a plane shows nothing below it
				submitDesc.cutBelow = pose->cutBelow;
				drawInstances(*mesh, placers->second, translucent || placers->second.materialBlending, instance, 1,
				              &entityPose);
				submitDesc.objectLook.reset();
				submitDesc.cutBelow.reset();
			};
			// The mesh, and its draw, whose instances hold this one: a villager's is the one its distance chose
			const auto drawHolding =
			    [&renderCtx](
			        uint32_t instance) -> std::optional<std::pair<entt::id_type, const RenderContext::InstancedDrawDesc*>> {
				for (const auto* descs : {&renderCtx.instancedDrawDescs, &renderCtx.fadingDrawDescs})
				{
					for (const auto& [meshId, placers] : *descs)
					{
						if (instance >= placers.offset && instance < placers.offset + placers.count)
						{
							return std::make_pair(meshId, &placers);
						}
					}
				}
				return std::nullopt;
			};
			// The villagers, each posed by its state's clip, or in its model's own pose while its state plays none. The
			// sea reflects only those flying in the physics, and one held in the hand while the hand is reflected.
			const auto handDraw = renderCtx.instancedDrawDescs.find(ecs::components::Hand::k_MeshId);
			const bool handReflected = desc.drawHand && handDraw != renderCtx.instancedDrawDescs.end() &&
			                           handDraw->second.count > 0 && !handDraw->second.hiddenFromReflection;
			const auto drawVillager = [&](entt::entity entity, uint32_t instance) {
				const bool reflection = desc.viewId == RenderPass::Reflection;
				const auto* pose = desc.entities.TryGet<const ecs::components::VillagerPose>(entity);
				if (pose == nullptr || (reflection && !graphics::sea_reflection::ReflectsVillager(
				                                          {.inPhysics = desc.entities.AllOf<ecs::components::InPhysics>(entity),
				                                           .inHand = desc.entities.AllOf<ecs::components::InHand>(entity),
				                                           .handReflected = handReflected})))
				{
					return;
				}
				const auto holding = drawHolding(instance);
				if (!holding.has_value() || !meshManager.Contains(holding->first))
				{
					return;
				}
				const auto& [meshId, placers] = *holding;
				const bool palette = placers->bonePalette && renderCtx.bonePaletteTexture;
				const auto model = meshManager.Handle(meshId);
				const bool posed = pose->bones.size() == model->GetBoneMatrices().size();
				const EntityPose entityPose =
				    palette ? EntityPose {.bones = {}, .morphTargets = nullptr, .bonePalette = true}
				            : EntityPose {.bones = posed ? std::span<const glm::mat4>(pose->bones)
				                                         : std::span<const glm::mat4>(model->GetBoneMatrices()),
				                          .morphTargets = nullptr};
				// A villager that swims in high detail is also drawn under the sea's surface, so the sea blends over it:
				// in the dark blue of a swimmer, and nothing of it above the sea's level
				if (desc.viewId == RenderPass::Main && desc.drawWater &&
				    ecs::swimmer_rules::Swims(desc.entities.AllOf<ecs::components::HighDetail>(entity), pose->clip))
				{
					const auto view = std::exchange(submitDesc.viewId, RenderPass::UnderSea);
					submitDesc.objectLook =
					    L3DMeshSubmitDesc::ObjectLook {.colour = ecs::swimmer_rules::k_UnderSeaColour, .alpha = 1.0f};
					submitDesc.cutAbove = 0.0f;
					drawInstances(meshId, *placers, placers->materialBlending, instance, 1, &entityPose);
					submitDesc.cutAbove.reset();
					submitDesc.objectLook.reset();
					submitDesc.viewId = view;
				}
				// The villagers of a mesh are drawn together from the bone palette; in the reflection one is drawn alone,
				// still with its bones from the palette
				if (palette && !reflection)
				{
					return;
				}
				drawInstances(meshId, *placers, placers->materialBlending, instance, 1, &entityPose);
			};
			// Whether an instance is of a thing fading out in the distance (not a villager, which the palette draws)
			const auto fadingHolds = [&renderCtx](uint32_t instance) {
				return std::ranges::any_of(renderCtx.fadingDrawDescs, [instance](const auto& entry) {
					const auto& placers = entry.second;
					return !placers.bonePalette && instance >= placers.offset && instance < placers.offset + placers.count;
				});
			};
			// The gates and other scenery the scripts open and close, each posed by its own clip: those fading out in the
			// distance and those that blend by their materials (an opened reward chest) with the blended things. `from`
			// is the registry of the thing: the land's, or the drawn registry.
			const auto drawPosedStillOf = [&](const ecs::Registry& from, entt::entity entity, uint32_t instance, bool blended) {
				const auto* still = from.TryGet<const ecs::components::AnimatedStaticPose>(entity);
				if (still == nullptr)
				{
					return;
				}
				const bool fading = fadingHolds(instance);
				// The sea mirrors none of the explorers' boat film this way, even one sharing its model with things it
				// mirrors
				const bool reflection = desc.viewId == RenderPass::Reflection;
				if (reflection && from.AllOf<ecs::components::BoatFilmPart>(entity))
				{
					return;
				}
				const auto holding = drawHolding(instance);
				if (!holding.has_value() || !meshManager.Contains(holding->first) ||
				    (reflection && holding->second->hiddenFromReflection))
				{
					return;
				}
				const auto& [meshId, placers] = *holding;
				if (graphics::posed_draws::PosedThingIsBlended(
				        fading, {.translucent = placers->translucent, .perEntity = placers->perEntity}) != blended)
				{
					return;
				}
				const auto model = meshManager.Handle(meshId);
				const bool posed = still->bones.size() == model->GetBoneMatrices().size();
				const EntityPose entityPose {.bones = posed ? std::span<const glm::mat4>(still->bones)
				                                            : std::span<const glm::mat4>(model->GetBoneMatrices()),
				                             .morphTargets = nullptr};
				if (placers->partsByLevel)
				{
					submitDesc.detailLevel = renderCtx.instanceLevels.at(instance);
				}
				drawInstances(meshId, *placers, fading ? false : placers->materialBlending, instance, 1, &entityPose);
				submitDesc.detailLevel = 1;
			};
			const auto drawPosedStill = [&](entt::entity entity, uint32_t instance, bool blended) {
				drawPosedStillOf(desc.entities, entity, instance, blended);
			};
			// The posed things that only the frames make, as the worm on the apple barrel, are in the first segment
			const auto drawDrawnPosedStills = [&](bool blended) {
				if (!Locator::drawnRegistry::has_value())
				{
					return;
				}
				const auto& drawn = Locator::drawnRegistry::value();
				for (const auto& [entity, instance] : renderCtx.drawnEntityDraws)
				{
					if (instance >= renderCtx.instanceUniforms.size() || mirroredWithHand(instance))
					{
						continue;
					}
					if (blended)
					{
						submitDesc.sortDepth =
						    zsort::Depth(glm::vec3(renderCtx.instanceUniforms.at(instance).model[3]), cameraOrigin);
					}
					drawPosedStillOf(drawn, entity, instance, blended);
				}
			};
			// The animals of a model drawn at once, in their light: the brightest of the land's, or white
			for (const auto& [meshId, group] : _animalBoneGroups)
			{
				const auto placers = renderCtx.instancedDrawDescs.find(meshId);
				if (placers == renderCtx.instancedDrawDescs.end() ||
				    (desc.viewId == RenderPass::Reflection && placers->second.hiddenFromReflection))
				{
					continue;
				}
				glm::vec3 colour(255.0f);
				if (group.brightestLand && _landLightTable)
				{
					const auto texel = _landLightTable->GetTexels().back();
					colour = glm::vec3(static_cast<float>(texel & 0xFFu), static_cast<float>((texel >> 8) & 0xFFu),
					                   static_cast<float>((texel >> 16) & 0xFFu));
				}
				// Lit by the land where each stands, as other things are, they take no colour of their own
				if (!group.landWhereItStands)
				{
					submitDesc.objectLook = L3DMeshSubmitDesc::ObjectLook {.colour = colour, .alpha = 1.0f};
				}
				inSegments(placers->second, [&](uint32_t first, uint32_t count) {
					drawInstances(meshId, placers->second, placers->second.materialBlending, first, count, nullptr, nullptr,
					              std::nullopt, &group);
				});
				submitDesc.objectLook.reset();
				submitDesc.boneTexture = nullptr;
			}
			drawDrawnPosedStills(false);
			for (const auto& [entity, instance] : renderCtx.entityDraws)
			{
				if (mirroredWithHand(instance))
				{
					continue;
				}
				inSegment(renderCtx.fizzOrder.SegmentOf(entity), [&] {
					drawAnimal(entity, instance, false);
					drawVillager(entity, instance);
					drawPosedStill(entity, instance, false);
				});
			}
			// The game mirrors what the hand holds just after the hand, flat in the light that it kept
			if (mirroredViewId != desc.viewId && handReflected)
			{
				submitDesc.viewId = mirroredViewId;
				submitDesc.sortDepth = hand_reflection::SortKey(hand_reflection::MirroredThing::Held);
				for (const auto& held : renderCtx.inHandDraws)
				{
					heldReflection = held.heldLight;
					const auto posed =
					    std::ranges::find(renderCtx.entityDraws, held.entity, &RenderContext::EntityDraw::entity);
					if (posed != renderCtx.entityDraws.end())
					{
						drawAnimal(held.entity, held.instance, false);
						drawVillager(held.entity, held.instance);
						drawPosedStill(held.entity, held.instance, false);
						continue;
					}
					const auto placers = renderCtx.instancedDrawDescs.find(held.mesh);
					if (placers != renderCtx.instancedDrawDescs.end() && held.instance >= placers->second.offset &&
					    held.instance < placers->second.offset + placers->second.filled)
					{
						drawInstances(held.mesh, placers->second,
						              placers->second.translucent || placers->second.materialBlending, held.instance, 1);
					}
				}
				heldReflection.reset();
				submitDesc.sortDepth = 0;
				submitDesc.viewId = desc.viewId;
			}
			// The villagers, a draw for each of their meshes, each instance posed by its own bones in the bone palette
			if (renderCtx.bonePaletteTexture)
			{
				const EntityPose palettePose {.bones = {}, .morphTargets = nullptr, .bonePalette = true};
				for (const auto& [meshId, placers] : renderCtx.instancedDrawDescs)
				{
					if (placers.bonePalette && placers.filled > 0 &&
					    !(desc.viewId == RenderPass::Reflection && placers.hiddenFromReflection))
					{
						inSegments(placers, [&](uint32_t first, uint32_t count) {
							drawInstances(meshId, placers, placers.materialBlending, first, count, &palettePose);
						});
					}
				}
			}
			// What the villagers hold in their hands, a draw for each model: the opaque ones here, those fading out with
			// the blended things. The sea reflects only what the villagers flying in the physics hold.
			const auto drawHeld = [&](bool fading) {
				for (const auto& held : renderCtx.heldDraws)
				{
					if (held.fading != fading || (desc.viewId == RenderPass::Reflection && !held.reflected) ||
					    !meshManager.Contains(held.meshId))
					{
						continue;
					}
					RenderContext::InstancedDrawDesc placers {held.first, held.count, false};
					placers.instanceAlpha = fading;
					if (fading)
					{
						submitDesc.sortDepth =
						    zsort::Depth(glm::vec3(renderCtx.heldInstances.at(held.first).model[3]), cameraOrigin);
					}
					drawInstances(held.meshId, placers, false, held.first, held.count, nullptr, nullptr,
					              renderCtx.heldInstanceBuffer);
				}
			};
			// The piles of roots, which only this computer draws: the opaque piles here, the piles that fade with the
			// blended things, each in its own place in the sort
			const auto rootsPileMesh = resources::HashIdentifier(roots_pile::k_Mesh);
			const auto drawRootsPiles = [&](bool fading) {
				const auto first = fading ? renderCtx.rootsPileOpaqueCount : 0;
				const auto end =
				    fading ? static_cast<uint32_t>(renderCtx.rootsPileInstances.size()) : renderCtx.rootsPileOpaqueCount;
				if (first == end || !meshManager.Contains(rootsPileMesh))
				{
					return;
				}
				RenderContext::InstancedDrawDesc placers {first, end - first, true};
				placers.instanceAlpha = fading;
				if (!fading)
				{
					drawInstances(rootsPileMesh, placers, false, first, end - first, nullptr, nullptr,
					              renderCtx.rootsPileInstanceBuffer);
					return;
				}
				for (uint32_t instance = first; instance < end; ++instance)
				{
					submitDesc.sortDepth =
					    zsort::Depth(glm::vec3(renderCtx.rootsPileInstances.at(instance).model[3]), cameraOrigin);
					drawInstances(rootsPileMesh, placers, false, instance, 1, nullptr, nullptr,
					              renderCtx.rootsPileInstanceBuffer);
				}
			};
			drawHeld(false);
			drawRootsPiles(false);
			DrawTempleUnderside(desc);
			// The translucent meshes blend over the opaque ones, each in its own place in the sort
			submitDesc.viewId = translucentViewId;
			// The things fading out in the distance, each blended by its alpha in its own place in the sort: the
			// villagers posed from the bone palette, the things posed one by one with them
			{
				const EntityPose palettePose {.bones = {}, .morphTargets = nullptr, .bonePalette = true};
				for (const auto& [meshId, placers] : renderCtx.fadingDrawDescs)
				{
					if (placers.filled == 0 || (desc.viewId == RenderPass::Reflection && placers.hiddenFromReflection) ||
					    !meshManager.Contains(meshId) || (placers.bonePalette && !renderCtx.bonePaletteTexture) ||
					    (placers.perEntity && !placers.bonePalette))
					{
						continue;
					}
					for (uint32_t instance = placers.offset; instance < placers.offset + placers.filled; ++instance)
					{
						if (mirroredWithHand(instance) || !instanceSeen(*meshManager.Handle(meshId), placers, instance))
						{
							continue;
						}
						const auto position = glm::vec3(renderCtx.instanceUniforms.at(instance).model[3]);
						submitDesc.sortDepth = zsort::Depth(position, cameraOrigin);
						if (placers.partsByLevel)
						{
							submitDesc.detailLevel = renderCtx.instanceLevels.at(instance);
						}
						drawInstances(meshId, placers, false, instance, 1, placers.bonePalette ? &palettePose : nullptr);
					}
					submitDesc.detailLevel = 1;
				}
				for (const auto& build : renderCtx.partialBuilds)
				{
					if (build.fading && build.instance < renderCtx.partialBuildInstances.size())
					{
						submitDesc.sortDepth =
						    zsort::Depth(glm::vec3(renderCtx.partialBuildInstances.at(build.instance).model[3]), cameraOrigin);
						drawPartialBuild(build);
					}
				}
				drawDrawnPosedStills(true);
				for (const auto& [entity, instance] : renderCtx.entityDraws)
				{
					if (instance < renderCtx.instanceUniforms.size() && !mirroredWithHand(instance))
					{
						submitDesc.sortDepth =
						    zsort::Depth(glm::vec3(renderCtx.instanceUniforms.at(instance).model[3]), cameraOrigin);
						drawPosedStill(entity, instance, true);
					}
				}
			}
			drawHeld(true);
			drawRootsPiles(true);
			// The souls of the dead rising or sinking, each white and see-through, posed by its own clip; the sea doesn't
			// reflect them
			if (desc.viewId != RenderPass::Reflection)
			{
				for (const auto& soul : renderCtx.soulDraws)
				{
					if (!meshManager.Contains(soul.meshId) || soul.instance >= renderCtx.soulInstances.size())
					{
						continue;
					}
					RenderContext::InstancedDrawDesc placers {soul.instance, 1, false};
					const EntityPose soulPose {.bones = soul.bones, .morphTargets = nullptr};
					submitDesc.sortDepth =
					    zsort::Depth(glm::vec3(renderCtx.soulInstances.at(soul.instance).model[3]), cameraOrigin);
					submitDesc.objectLook = L3DMeshSubmitDesc::ObjectLook {.colour = glm::vec3(255.0f), .alpha = soul.alpha};
					drawInstances(soul.meshId, placers, true, soul.instance, 1, &soulPose, nullptr,
					              renderCtx.soulInstanceBuffer);
					submitDesc.objectLook.reset();
				}
			}
			// Each thing that is posed by itself (an animal, a villager or posed scenery) is drawn above, in its own pose.
			// It is not drawn here, because it would show again in the model's own pose.
			for (const auto& [meshId, placers] : renderCtx.instancedDrawDescs)
			{
				if (graphics::posed_draws::BlendedPassDrawsAllPlaced(
				        {.translucent = placers.translucent, .perEntity = placers.perEntity}) &&
				    !(desc.viewId == RenderPass::Reflection && placers.hiddenFromReflection))
				{
					for (uint32_t instance = placers.offset; instance < placers.offset + placers.filled; ++instance)
					{
						if (mirroredWithHand(instance) || !instanceSeen(*meshManager.Handle(meshId), placers, instance))
						{
							continue;
						}
						const auto position = glm::vec3(renderCtx.instanceUniforms.at(instance).model[3]);
						submitDesc.sortDepth = zsort::Depth(position, cameraOrigin);
						drawInstances(meshId, placers, true, instance, 1);
					}
				}
			}
			for (const auto& [entity, instance] : renderCtx.entityDraws)
			{
				if (instance < renderCtx.instanceUniforms.size() && !mirroredWithHand(instance))
				{
					const auto position = glm::vec3(renderCtx.instanceUniforms.at(instance).model[3]);
					submitDesc.sortDepth = zsort::Depth(position, cameraOrigin);
					drawAnimal(entity, instance, true);
				}
			}
			// A warm-up draws each model of the land once in each way that the frames draw it: solid as it is placed, blended
			// by its materials and fading by its alpha. It draws them at no size, and covers no pixel. Thus a model that
			// comes into sight or fades out later makes no pipeline then.
			if (_warmingUp && _warmUpInstances.has_value())
			{
				const EntityPose palettePose {.bones = {}, .morphTargets = nullptr, .bonePalette = true};
				const auto warmUpModel = [&](entt::id_type meshId, RenderContext::InstancedDrawDesc placers) {
					if (meshId == ecs::components::Hand::k_MeshId || !meshManager.Contains(meshId) ||
					    (desc.viewId == RenderPass::Reflection && placers.hiddenFromReflection))
					{
						return;
					}
					const auto* pose = placers.bonePalette ? &palettePose : nullptr;
					placers.instanceAlpha = false;
					submitDesc.viewId = desc.viewId;
					drawInstances(meshId, placers, placers.materialBlending, 0, 1, pose, nullptr, *_warmUpInstances);
					submitDesc.viewId = translucentViewId;
					drawInstances(meshId, placers, true, 0, 1, pose, nullptr, *_warmUpInstances);
					placers.instanceAlpha = true;
					drawInstances(meshId, placers, false, 0, 1, pose, nullptr, *_warmUpInstances);
				};
				for (const auto& [meshId, placers] : renderCtx.instancedDrawDescs)
				{
					warmUpModel(meshId, placers);
				}
				for (const auto& [meshId, placers] : renderCtx.fadingDrawDescs)
				{
					warmUpModel(meshId, placers);
				}
				// And each building part built, both standing and fading
				for (auto build : renderCtx.partialBuilds)
				{
					for (const bool fading : {false, true})
					{
						build.fading = fading;
						submitDesc.viewId = fading ? translucentViewId : desc.viewId;
						drawPartialBuild(build, *_warmUpInstances);
					}
				}
			}
			submitDesc.viewId = desc.viewId;
			submitDesc.sortDepth = 0;
			// The main room's pool, the map over it and its markers, ahead of the hand, whose faded wrist they show
			// through
			DrawTemplePool(desc);
			DrawTempleMap(desc);
			DrawTempleMapMarkers(desc);
			DrawCaveTrophies(desc);
			DrawCaveSeeds(desc);
			DrawMapSpellSeeds(desc);
			DrawGroundBlobs(desc);
			DrawFarVillagerSmudges(desc);
			DrawVillagerEyes(desc);
			DrawGlobes(desc);
			DrawIconSeeds(desc);
			DrawRewardSeeds(desc);
			DrawHandMiracleBands(desc);
			DrawTribalPower(desc);
			DrawLightColumns(desc);
			DrawScrollBeams(desc);
			DrawIconPulses(desc);
			DrawLeashes(desc);
			DrawWaterRings(desc);
			DrawRain(desc);
			DrawSnowfall(desc);
			DrawKnockReadout(desc);
			DrawChimneySmoke(desc);
			DrawShieldDomes(desc);
			DrawDestructionGhosts(desc);
			DrawFilmReflections(desc);
			DrawParticles(desc);
			DrawIntroLight(desc);
			DrawInfluenceBorder(desc);
			DrawInfluenceRipples(desc);
			// The mists blend over the rest, the farthest first
			DrawMists(desc);
			DrawPauseText(desc);

			// The game mirrors the hand in the sea while the hand shows, wherever it is: in one flat grey, with no light
			// and no glow, and only its front faces. Its texture alone gives its alpha.
			if (const auto hand = renderCtx.instancedDrawDescs.find(ecs::components::Hand::k_MeshId);
			    mirroredViewId != desc.viewId && desc.drawHand && hand != renderCtx.instancedDrawDescs.end() &&
			    !hand->second.hiddenFromReflection &&
			    hand_reflection::Shown(hand->second.count > 0, meshManager.Contains(hand->first)))
			{
				// L3D meshes face clockwise, which the mirrored reflection pass and a mirrored hand each turn around
				const bool facesTurned = desc.cullBack != renderCtx.handMirrored;
				const auto opaqueState = submitDesc.state;
				submitDesc.viewId = mirroredViewId;
				submitDesc.sortDepth = hand_reflection::SortKey(hand_reflection::MirroredThing::Hand);
				submitDesc.state =
				    (opaqueState & ~BGFX_STATE_CULL_MASK) | (facesTurned ? BGFX_STATE_CULL_CW : BGFX_STATE_CULL_CCW);
				auto mirrored = hand->second;
				mirrored.additiveShare.reset();
				mirrored.instanceAlpha = false;
				const auto handMorph = HandMorphTargets();
				flatReflection = FlatLook {.colour = hand_reflection::Colour(), .added = hand_reflection::Added()};
				drawInstances(hand->first, mirrored, true, mirrored.offset, mirrored.count, nullptr,
				              handMorph ? &*handMorph : nullptr);
				flatReflection.reset();
				submitDesc.state = opaqueState;
				submitDesc.viewId = desc.viewId;
				submitDesc.sortDepth = 0;
			}
			// The game draws the hand after the rest of the scene, blended by its translucent texture. Its material
			// shows only the front faces, so the inside of the hand never shows through it.
			// The game's reflection of the main room has no hand in it
			if (const auto hand = renderCtx.instancedDrawDescs.find(ecs::components::Hand::k_MeshId);
			    mirroredViewId == desc.viewId && desc.drawHand && hand != renderCtx.instancedDrawDescs.end() &&
			    !(desc.viewId == RenderPass::Reflection && hand->second.hiddenFromReflection))
			{
				// L3D meshes face clockwise, which the mirrored reflection pass and a mirrored hand each turn around
				const bool facesTurned = desc.cullBack != renderCtx.handMirrored;
				// It takes its place in the sort by where it is
				submitDesc.viewId = translucentViewId;
				if (hand->second.count > 0)
				{
					const auto position = glm::vec3(renderCtx.instanceUniforms.at(hand->second.offset).model[3]);
					submitDesc.sortDepth = zsort::Depth(position, cameraOrigin);
				}
				const auto cullFront = facesTurned ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW;
				const auto cullBack = facesTurned ? BGFX_STATE_CULL_CW : BGFX_STATE_CULL_CCW;
				const auto opaqueState = submitDesc.state;
				const auto handMorph = HandMorphTargets();
				const auto* morph = handMorph ? &*handMorph : nullptr;

				// The game lights the hand as it poses it, with the posed models' rule rather than the usual model light
				submitDesc.posedModelLight = true;
				const auto outsideState = [opaqueState](uint64_t cull) { return (opaqueState & ~BGFX_STATE_CULL_MASK) | cull; };
				// The glow is added over the hand without writing depth
				const auto glowState = [opaqueState](uint64_t cull) {
					constexpr uint64_t k_Kept = ~(BGFX_STATE_CULL_MASK | BGFX_STATE_WRITE_Z | BGFX_STATE_WRITE_A |
					                              BGFX_STATE_DEPTH_TEST_MASK | BGFX_STATE_BLEND_MASK);
					return (opaqueState & k_Kept) | BGFX_STATE_DEPTH_TEST_GEQUAL |
					       BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE) | cull;
				};
				submitDesc.state = outsideState(cullBack);
				drawInstances(hand->first, hand->second, true, hand->second.offset, hand->second.count, nullptr, morph);
				// Holding a miracle, the game draws the hand once more over itself: the same hand at the same depth, in the
				// flow's sheet and its player's colour, shaded as it is posed and added over it without writing depth
				if (const auto glow = HandGlowOf(desc))
				{
					auto glowPlacers = hand->second;
					glowPlacers.uvOffset = glow->uvOffset;
					glowPlacers.additiveShare.reset();
					glowPlacers.instanceAlpha = false;
					submitDesc.state = glowState(cullBack);
					// Just after the hand in the sort
					submitDesc.sortDepth = submitDesc.sortDepth > 0 ? submitDesc.sortDepth - 1 : 0;
					submitDesc.skinTexture = &glow->flow->GetNativeHandle();
					submitDesc.uvScale = glow->uvScale;
					submitDesc.objectLook = L3DMeshSubmitDesc::ObjectLook {.colour = glow->colour, .alpha = glow->alpha};
					// Nothing of it is left out by its alpha
					submitDesc.alphaThreshold = 0.0f;
					drawInstances(hand->first, glowPlacers, false, glowPlacers.offset, glowPlacers.count, nullptr, morph);
					submitDesc.alphaThreshold.reset();
					submitDesc.objectLook.reset();
					submitDesc.uvScale = 1.0f;
					submitDesc.skinTexture = nullptr;
				}
				// A warm-up draws the hand again at no size in each other way that the frames draw it: turned round, as a
				// mirrored hand is, and glowing with a miracle
				if (_warmingUp && _warmUpInstances.has_value())
				{
					auto hidden = hand->second;
					hidden.additiveShare.reset();
					hidden.instanceAlpha = false;
					for (const auto cull : {cullBack, cullFront})
					{
						submitDesc.state = outsideState(cull);
						drawInstances(hand->first, hidden, true, 0, 1, nullptr, morph, *_warmUpInstances);
						submitDesc.state = glowState(cull);
						drawInstances(hand->first, hidden, false, 0, 1, nullptr, morph, *_warmUpInstances);
					}
				}
				submitDesc.posedModelLight = false;
				submitDesc.state = opaqueState;
				submitDesc.viewId = desc.viewId;
				submitDesc.sortDepth = 0;
			}

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
					debugShaderInstanced->Submit(static_cast<bgfx::ViewId>(desc.viewId));
				}
				if (renderCtx.footpaths)
				{
					renderCtx.footpaths->GetVertexBuffer().Bind();
					bgfx::setState(k_BgfxDefaultStateInvertedZ | BGFX_STATE_PT_LINES);
					debugShader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
				}
				if (renderCtx.streams)
				{
					renderCtx.streams->GetVertexBuffer().Bind();
					bgfx::setState(k_BgfxDefaultStateInvertedZ | BGFX_STATE_PT_LINES);
					debugShader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
				}
			}
		}

		{
			auto subSection =
			    profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawVegetation
			                                                               : Profiler::Stage::MainPassDrawVegetation);
			const auto& renderCtx = Locator::rendereringSystem::value().GetContext();

			if (desc.drawVegetation)
			{
				L3DMeshSubmitDesc submitDesc = {};
				submitDesc.viewId = desc.viewId;
				submitDesc.program = vegetationShaderInstanced;
				submitDesc.state = 0u                              //
				                   | BGFX_STATE_WRITE_MASK         //
				                   | BGFX_STATE_DEPTH_TEST_GREATER //
				                   | BGFX_STATE_MSAA               //
				    ;

				// The trees of each mesh, a draw for each detail they are drawn in: each with the parts of its detail. Those
				// fading out in the distance are blended by each one's alpha with the blended things.
				const auto opaqueState = submitDesc.state;
				for (const auto& run : renderCtx.treeRuns)
				{
					if (run.gone || run.count == 0 || !meshManager.Contains(run.mesh))
					{
						continue;
					}
					auto mesh = meshManager.Handle(run.mesh);
					submitDesc.instanceDesc =
					    std::make_unique<graphics::InstanceDesc>(renderCtx.treeInstanceUniformBuffer, run.first, run.count);
					const static auto identity = glm::mat4(1.0f);
					submitDesc.modelMatrices = &identity;
					submitDesc.matrixCount = 1;
					submitDesc.isSky = false;
					submitDesc.morphWithTerrain = false;
					submitDesc.landLightScale = _treeBrightness;
					submitDesc.snow = !inTemple;
					submitDesc.program = vegetationShaderInstanced;
					submitDesc.detailLevel = run.level;
					submitDesc.viewId = run.fading ? TranslucentView(desc.viewId) : desc.viewId;
					submitDesc.state = run.fading ? opaqueState | BGFX_STATE_BLEND_ALPHA : opaqueState;
					submitDesc.sortDepth =
					    run.fading ? zsort::Depth(glm::vec3(renderCtx.treeInstanceData.at(run.first).modelMatrix[3]),
					                              desc.camera->GetOrigin())
					               : 0;
					// The solid trees after a fizzing creature in the land's draw list come after the passes of the
					// creature
					if (!run.fading && run.segment > 0 && desc.viewId == RenderPass::Main &&
					    TranslucentView(desc.viewId) != desc.viewId)
					{
						submitDesc.viewId = TranslucentView(desc.viewId);
						submitDesc.sortDepth = graphics::fizz_draw_order::SegmentDepth(run.segment);
					}
					// The sea mirrors a tree in a hand only with the hand, not with the land
					if (desc.viewId == RenderPass::Reflection && !renderCtx.heldTreeDraws.empty())
					{
						uint32_t from = run.first;
						const uint32_t end = run.first + run.count;
						while (from < end)
						{
							uint32_t to = end;
							for (const auto& held : renderCtx.heldTreeDraws)
							{
								if (held.instance >= from && held.instance < to)
								{
									to = held.instance;
								}
							}
							if (to > from)
							{
								submitDesc.instanceDesc = std::make_unique<graphics::InstanceDesc>(
								    renderCtx.treeInstanceUniformBuffer, from, to - from);
								DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
							}
							from = to + 1;
						}
						continue;
					}
					DrawMesh(*mesh, submitDesc, std::numeric_limits<uint8_t>::max());
				}
				// The game mirrors a tree in the hand right after the hand, flat in the light that the tree kept: the land's
				// light at its place on the map, the haze there and the grey of a fire on it, of the frame before
				if (const auto hand = renderCtx.instancedDrawDescs.find(ecs::components::Hand::k_MeshId);
				    desc.viewId == RenderPass::Reflection && !inTemple && desc.drawHand &&
				    hand != renderCtx.instancedDrawDescs.end() && hand->second.count > 0 && !hand->second.hiddenFromReflection)
				{
					for (const auto& held : renderCtx.heldTreeDraws)
					{
						const auto run = std::ranges::find_if(renderCtx.treeRuns, [&held](const auto& each) {
							return each.mesh == held.mesh && held.instance >= each.first &&
							       held.instance < each.first + each.count;
						});
						if (!held.heldLight.has_value() || !meshManager.Contains(held.mesh) || run == renderCtx.treeRuns.end())
						{
							continue;
						}
						submitDesc.instanceDesc =
						    std::make_unique<graphics::InstanceDesc>(renderCtx.treeInstanceUniformBuffer, held.instance, 1);
						submitDesc.detailLevel = run->level;
						submitDesc.viewId = RenderPass::ReflectionMirrored;
						submitDesc.sortDepth = hand_reflection::SortKey(hand_reflection::MirroredThing::Held);
						submitDesc.state = opaqueState;
						submitDesc.snow = false;
						submitDesc.landLightScale = _before.treeBrightness.Or(_treeBrightness);
						submitDesc.shadeAt = glm::vec4(held.heldLight->point, hand_reflection::k_HeldShade);
						submitDesc.heldMirror = held.heldLight;
						submitDesc.heldTreeWidth = hand_reflection::MirroredWidthFactor(held.widthShare);
						DrawMesh(*meshManager.Handle(held.mesh), submitDesc, std::numeric_limits<uint8_t>::max());
						submitDesc.shadeAt = glm::vec4(0.0f);
						submitDesc.heldMirror.reset();
						submitDesc.heldTreeWidth = 1.0f;
					}
				}
				submitDesc.detailLevel = 1;
				submitDesc.viewId = desc.viewId;
				submitDesc.state = opaqueState;
				submitDesc.sortDepth = 0;

				// Draw tree bounding boxes if enabled
				if (renderCtx.boundingBox && renderCtx.treeInstanceCount > 0)
				{
					const auto boundBoxOffset = renderCtx.treeInstanceCount;
					const auto boundBoxCount = renderCtx.treeInstanceCount;
					renderCtx.boundingBox->GetVertexBuffer().Bind();
					bgfx::setInstanceDataBuffer(toBgfx(renderCtx.treeInstanceUniformBuffer), boundBoxOffset, boundBoxCount);
					bgfx::setState(k_BgfxDefaultStateInvertedZ | BGFX_STATE_PT_LINES);
					debugShaderInstanced->Submit(static_cast<bgfx::ViewId>(desc.viewId));
				}
			}
		}

		if (desc.drawEntities)
		{
			DrawLightBeams(desc);
			DrawTempleExitGlow(desc);
			DrawTempleCreedShells(desc);
			DrawMistDomes(desc);
			DrawTempleText(desc);
		}

		{
			auto subSection =
			    profiler.BeginScoped(desc.viewId == RenderPass::Reflection ? Profiler::Stage::ReflectionDrawSprites
			                                                               : Profiler::Stage::MainPassDrawSprites);

			if (desc.drawSprites)
			{
				using namespace ecs::components;

				const auto& registry = Locator::entitiesRegistry::value();
				// The camera's bookmarks are drawn unless a script put them away or holds the cinema bars
				const bool bookmarksShown =
				    (!Locator::cameraBookmarkSystem::has_value() || Locator::cameraBookmarkSystem::value().IsEnabled()) &&
				    (!Locator::cinematicDirectorSystem::has_value() ||
				     Locator::cinematicDirectorSystem::value().IsInterfaceActive());
				// The markers of leash orders show only while the land shows
				const bool leashMarkersShown = creature_leash_orders::MarkerShown(
				    Locator::temple::has_value() && Locator::temple::value().Active(),
				    Locator::videoSystem::has_value() &&
				        (Locator::videoSystem::value().CoversScreen() || Locator::videoSystem::value().HidesWorld()));
				// A warm-up draws a sprite of no size for each way that sprites blend, as the first sprites draw
				if (_warmingUp)
				{
					const auto hidden = glm::mat4(0.0f);
					for (const bool additive : {false, true})
					{
						bgfx::setTransform(glm::value_ptr(hidden));
						_plane->GetVertexBuffer().Bind();
						bgfx::setState(SpriteState(additive));
						spriteShader->Submit(static_cast<bgfx::ViewId>(translucentViewId));
					}
				}
				// The sprites name their textures by their ids in the cache
				const auto& textures = Locator::resources::value().GetTextures();
				// The view of this pass. A sprite out of it puts no pixel on the screen, so the pass does not draw it.
				const auto seenBy = view_frustum::FromViewProjection(
				    desc.camera->GetProjectionMatrix() * desc.camera->GetViewMatrix(Camera::Interpolation::Current));
				const auto drawSprites = [this, &spriteShader, &desc, &textures, translucentViewId, cameraOrigin,
				                          bookmarksShown, leashMarkersShown, &seenBy](const ecs::Registry& from) {
					// The walk makes no component, so the stores of the components stay the same for all of it
					const ecs::drawn_place::DrawnPlaces drawnPlaces(from);
					const ecs::ComponentLookup<CameraBookmark> bookmarks(from);
					const ecs::ComponentLookup<LeashMarker> markers(from);
					const ecs::ComponentLookup<TempleInteriorPart> templeParts(from);
					from.Each<const Sprite, const Transform>(
					    [this, &spriteShader, &desc, &textures, translucentViewId, cameraOrigin, &from, bookmarksShown,
					     leashMarkersShown, &seenBy, &drawnPlaces, &bookmarks, &markers,
					     &templeParts](entt::entity entity, const Sprite& sprite, const Transform& transform) {
						    if (!bookmarksShown && bookmarks.Has(entity))
						    {
							    return;
						    }
						    if (const auto* marker = markers.TryGet(entity);
						        marker != nullptr && (!leashMarkersShown || !IsOwnLeashMarker(from, *marker)))
						    {
							    return;
						    }
						    // The temple draws the glows of the rooms it draws whole, and the main room reflects its own glows
						    // alone in its floor
						    if (const auto* templePart = templeParts.TryGet(entity); templePart != nullptr)
						    {
							    const bool inMainRoom = templePart->room == TempleRoom::Main;
							    const bool drawn = Locator::temple::value().IsRoomDrawn(templePart->room);
							    const bool inReflection = desc.viewId == RenderPass::Reflection;
							    if ((inReflection ? !inMainRoom : !drawn) ||
							        !detail_level::TempleGlowsShown(Locator::config::value().detail, inReflection))
							    {
								    return;
							    }
						    }
						    // A sprite whose texture the cache does not have is not drawn
						    const auto found = sprite_textures::FindTextures(
						        sprite, [&textures](entt::id_type id) { return textures.Find(id); });
						    if (!found.has_value())
						    {
							    return;
						    }
						    const auto* texture = found->texture;
						    const auto* alpha = found->alpha;
						    // A sprite that moves between the turns is drawn at its drawn place
						    const auto placed = drawnPlaces.Of(entity, transform);
						    glm::mat4 modelMatrix = glm::mat4(1.0f);
						    modelMatrix = glm::translate(modelMatrix, placed.position);
						    modelMatrix *= glm::mat4(placed.rotation);
						    modelMatrix = glm::scale(modelMatrix, placed.scale);
						    if (!_warmingUp && !view_frustum::SeesSphere(seenBy, placed.position,
						                                                 view_frustum::QuadRadius(glm::mat3(modelMatrix)) +
						                                                     view_frustum::k_BoxMargin))
						    {
							    return;
						    }

						    glm::vec4 u_sampleRect(sprite.uvExtent, sprite.uvMin);

						    bgfx::setTransform(glm::value_ptr(modelMatrix));
						    spriteShader->SetUniformValue("u_sampleRect", glm::value_ptr(u_sampleRect));
						    const glm::vec4 u_spriteParams {sprite.facesCamera ? 1.0f : 0.0f,
						                                    sprite.alpha.has_value() ? 1.0f : 0.0f,
						                                    sprite.additive ? 1.0f : 0.0f, 0.0f};
						    spriteShader->SetUniformValue("u_spriteParams", glm::value_ptr(u_spriteParams));
						    spriteShader->SetTextureSampler("s_alpha", 1, *alpha);
						    // The shader multiplies the tint by the texture's alpha, which for alpha blending gives colours
						    // premultiplied by alpha when the tint is
						    const auto tint = sprite.additive
						                          ? sprite.tint
						                          : glm::vec4(glm::vec3(sprite.tint) * sprite.tint.a, sprite.tint.a);
						    spriteShader->SetUniformValue("u_tint", glm::value_ptr(tint));
						    spriteShader->SetTextureSampler("s_diffuse", 0, *texture);

						    _plane->GetVertexBuffer().Bind();

						    bgfx::setState(SpriteState(sprite.additive));

						    spriteShader->Submit(static_cast<bgfx::ViewId>(translucentViewId),
						                         zsort::Depth(placed.position, cameraOrigin));
					    });
				};
				// The temple's rooms have sprites of their own: their glows, and the flames and smoke of the creature's room
				if (Locator::temple::has_value() && Locator::temple::value().Active())
				{
					drawSprites(Locator::temple::value().GetRooms());
				}
				drawSprites(registry);
				// The things that only the frames make, as the marks over the disciples, have sprites too
				if (Locator::drawnRegistry::has_value())
				{
					drawSprites(Locator::drawnRegistry::value());
				}
				DrawTempleLeashes(desc);
			}
		}
	}

	if (desc.viewId == RenderPass::Main && desc.drawEntities)
	{
		DrawAdvisors(desc);
		DrawGestureHintCollars(desc);
		DrawGestureHintSeeds(desc);
	}

	// The sun's glare over everything else in the view
	if (desc.viewId == RenderPass::Main && desc.drawSky)
	{
		DrawSunGlare(*desc.camera);
	}

	// The hand's glow lies on the water, under the sea that is blended over it
	if (desc.viewId == RenderPass::Reflection && (desc.drawHand || _warmingUp))
	{
		DrawHandWaterGlow(desc);
	}
	// So do the fish farms' shoals and the shoals of the fish puzzle, swimming under it
	DrawFishShoals(desc);
	// And the parts of the floats of the fish puzzle under the sea's level
	DrawFishPuzzleFloats(desc);

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
