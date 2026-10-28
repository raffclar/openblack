/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldStart.h"

#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <MorphFile.h>
#include <PackFile.h>
#include <fmt/format.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "Simulation/ProfileMenu.h"
#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif

#include "3D/AllMeshes.h"
#include "3D/CreatureCaveTrophies.h"
#include "3D/L3DMesh.h"
#include "3D/LandLightTable.h"
#include "3D/LowResolutionTextures.h"
#include "3D/SeaTexture.h"
#include "3D/SkyDome.h"
#include "3D/SnowCover.h"
#include "3D/TempleRoomMeshes.h"
#include "Audio/AnimEffectTable.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/ClipSounds.h"
#include "Audio/HelpSpeech.h"
#include "CHLApi.h"
#include "Common/BufferedLogSink.h"
#include "Common/GameRandom.h"
#include "Common/RandomNumberManager.h"
#include "Common/StringUtils.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"
#include "Creature/TattooPictures.h"
#include "ECS/Archetypes/SkyArchetype.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/TexelDepth.h"
#include "Gui/TextDatabase.h"
#include "Level.h"
#include "Locator.h"
#include "Parsers/InfoFile.h"
#include "Physics/Materials.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::simulation
{
namespace
{

/// How many bytes a game file has, for the loading budget, or none when it isn't there
std::optional<size_t> GameFileBytes(const std::filesystem::path& path)
{
	try
	{
		std::error_code error;
		const auto size = std::filesystem::file_size(Locator::filesystem::value().FindPath(path), error);
		if (!error)
		{
			return static_cast<size_t>(size);
		}
	}
	catch (const std::exception&)
	{
		// Not found: there is nothing to load
	}
	return std::nullopt;
}

/// The bytes of a raw picture file at half its size, when the lowest detail levels load it so; none otherwise
std::optional<std::vector<uint8_t>> HalvedAtThisDetail(const std::filesystem::path& path)
{
	if (!Locator::config::value().detail.lowResTextures || !low_resolution_textures::HalvedAtLowDetail(path.stem().string()))
	{
		return std::nullopt;
	}
	return low_resolution_textures::Halve(Locator::filesystem::value().ReadAll(path));
}

/// Loads a raw picture file into the texture cache, at half its size when the lowest detail levels load it so
template <typename Id>
void LoadRawTexture(resources::TextureManager& textures, Id id, const std::filesystem::path& path,
                    graphics::texel_depth::Depth depth)
{
	if (auto halved = HalvedAtThisDetail(path))
	{
		textures.Load(id, resources::Texture2DLoader::FromBufferTag {}, ("raw" / path.stem()).string(), std::move(*halved),
		              depth);
		return;
	}
	textures.Load(id, resources::Texture2DLoader::FromDiskTag {}, path, depth);
}

/// Registers a resource read from a game file, to be loaded by its cache's loader with these arguments when it is first
/// needed or prefetched. One whose file isn't there is left out, as loading it would fail.
template <typename Manager, typename Id, typename... Args>
void RegisterFile(Manager& manager, Id id, const std::filesystem::path& path, Args... args)
{
	if (const auto bytes = GameFileBytes(path))
	{
		manager.RegisterLoad(id, *bytes, std::move(args)...);
	}
	else
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't find {}", path.generic_string());
	}
}

/// The sound banks whose samples are decoded as soon as they are read, as they are wanted at once and often: the hand's,
/// the miracles' and the interface's
constexpr std::array<std::string_view, 1> k_DecodedAheadBanks = {"InGame.sad"};
} // namespace

void StartLogs(const std::string& logFile, const LogLevels& levels)
{
	std::function<std::shared_ptr<spdlog::logger>(const std::string&)> createLogger;
	bool fileLog = false;
#ifdef __ANDROID__
	if (!logFile.empty() && logFile == "logcat")
	{
		createLogger = [](const std::string& name) { return spdlog::android_logger_mt(name, "spdlog-android"); };
	}
	else
#endif // __ANDROID__
	{
		if (!logFile.empty() && logFile != "stdout")
		{
			// Every subsystem's logger writes through the one file: each opening the file for itself, their writes
			// overwrote each other's and lines went missing. The sink writes the file on its own thread each second, so
			// that a thread that logs a line does not wait for the disk.
			fileLog = true;
			auto fileSink = logging::BufferedLogSink::ToFile(logFile, std::chrono::seconds(1));
			createLogger = [fileSink](const std::string& name) {
				auto logger = std::make_shared<spdlog::logger>(name, fileSink);
				spdlog::register_logger(logger);
				return logger;
			};
		}
		else
		{
			createLogger = [](const std::string& name) { return spdlog::stdout_color_mt(name); };
		}
	}
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; const auto& subsystem : k_LoggingSubsystemStrs)
	{
		// A logger of this name that an earlier user left in the registry would make the new one fail. A test program
		// that runs many tests in one process leaves such loggers.
		spdlog::drop(subsystem.data());
		auto logger = createLogger(subsystem.data());
		logger->set_level(levels.at(i));
		// An error is written out at once, so it is in the file even if the process is then ended from outside, where no
		// crash report can be written
		logger->flush_on(spdlog::level::err);
		++i;
	}
	// Everything else is written out within a second. The file sink does this itself.
	if (!fileLog)
	{
		spdlog::flush_every(std::chrono::seconds(1));
	}
}

bool RegisterWorldData(WorldDataViewInterface& view)
{
	using filesystem::Path;
	auto& fileSystem = Locator::filesystem::value();
	const auto& config = Locator::config::value();
	auto& resources = Locator::resources::value();
	auto& meshManager = resources.GetMeshes();
	auto& textureManager = resources.GetTextures();
	auto& animationManager = resources.GetAnimations();
	auto& levelManager = resources.GetLevels();
	auto& soundManager = resources.GetSounds();
	auto& glowManager = resources.GetGlows();
	auto& camPathManager = resources.GetCameraPaths();

	// The main room's markers of the temples, creatures and challenges on the map, and the creature's room's belts and
	// medals
	std::vector<std::string> icons {"I_citadel_on_map", "I_creature_on_map", "I_challenge_on_map"};
	for (uint32_t i = 0; i < CreatureCaveTrophies::k_IconCount; ++i)
	{
		icons.push_back(CreatureCaveTrophies::IconName(i));
	}
	for (const auto& icon : icons)
	{
		const auto path = fileSystem.GetPath<Path::Citadel>() / "icons" / fmt::format("{}.l3d", icon);
		try
		{
			RegisterFile(meshManager, fmt::format("temple/icons/{}", icon), path, resources::L3DLoader::FromDiskTag {}, path);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	fileSystem.Iterate(fileSystem.GetPath<Path::Citadel>() / "OutsideMeshes", false,
	                   [&meshManager, &resources](const std::filesystem::path& f) {
		                   const auto extension = string_utils::LowerCase(f.extension().string());
		                   const auto name = fmt::format("temple/{}", string_utils::LowerCase(f.stem().string()));
		                   try
		                   {
			                   if (extension == ".zzz")
			                   {
				                   SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading temple mesh: {}", f.stem().string());
				                   RegisterFile(meshManager, name, f, resources::L3DLoader::FromDiskTag {}, f);
				                   // The temple's outside is blended from the temple meshes, into the first temple's,
				                   // and its worship sites wear its skin
				                   if (name.starts_with("temple/b_temple") || name.starts_with("temple/b_first_temple") ||
				                       name == "temple/b_worship_l3d")
				                   {
					                   RegisterFile(resources.GetL3DFiles(), name, f, resources::L3DFileLoader::FromDiskTag {},
					                                f);
				                   }
			                   }
			                   else if (extension == ".16b")
			                   {
				                   // And its texture from these, from evil to neutral to good
				                   RegisterFile(resources.GetBitmaps(), name, f, resources::Bitmap16BLoader::FromDiskTag {}, f);
			                   }
		                   }
		                   catch (std::runtime_error& err)
		                   {
			                   SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		                   }
	                   });

	view.StartupStep("temple outside meshes");

	// What things are made of, for the physics; without the file every material is zero, as in the game
	if (const auto constants = fileSystem.GetPath<filesystem::Path::Data>() / "PhysicsConstants.txt";
	    fileSystem.Exists(constants))
	{
		resources.GetPhysicsMaterials().Load(physics::k_MaterialsId.value(), resources::PhysicsMaterialsLoader::FromDiskTag {},
		                                     constants);
	}
	else
	{
		resources.GetPhysicsMaterials().Load(physics::k_MaterialsId.value(), resources::PhysicsMaterialsLoader::EmptyTag {});
	}
	// The sheet the physics' dust puffs are drawn from
	for (const auto* name : {"blobs", "blobsa"})
	{
		if (const auto sheet = fileSystem.GetPath<filesystem::Path::Data>() / fmt::format("{}.raw", name);
		    fileSystem.Exists(sheet))
		{
			// Loaded with its alpha by the game, so both keep four bits a channel
			LoadRawTexture(textureManager, fmt::format("raw/{}", name), sheet, graphics::texel_depth::Depth::FourBits);
		}
	}

	// The land's light is built from this every frame
	if (const auto palette = fileSystem.GetPath<filesystem::Path::WeatherSystem>() / "palette.raw"; fileSystem.Exists(palette))
	{
		resources.GetLandLightPalettes().Load(LandLightPalette::k_Id.value(), resources::LandLightPaletteLoader::FromDiskTag {},
		                                      palette);
	}

	view.StartupStep("physics materials, dust sheets and land palette");
	fileSystem.Iterate( //
	    fileSystem.GetPath<filesystem::Path::Citadel>() / "engine", false,
	    [&meshManager, &glowManager, &fileSystem, &config](const std::filesystem::path& f) {
		    if (f.extension() == ".zzz")
		    {
			    // At the lowest detail the rooms are built from their low meshes, which have no light maps. Only one of
			    // the two is loaded: both together are more meshes than the renderer holds.
			    const auto stem = f.stem().string();
			    const bool lightmaps = config.detail.templeLightmaps;
			    const auto lowMesh = f.parent_path() / (temple_room_meshes::LowName(stem) + f.extension().string());
			    const auto name = temple_room_meshes::LoadedName(stem, lightmaps, fileSystem.Exists(lowMesh));
			    if (!name.has_value())
			    {
				    return;
			    }
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple mesh: {} as {}", stem, *name);
			    try
			    {
				    RegisterFile(meshManager, fmt::format("temple/interior/{}", *name), f, resources::L3DLoader::FromDiskTag {},
				                 f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
		    else if (f.extension() == ".glw")
		    {
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple glows: {}", f.stem().string());
			    try
			    {
				    RegisterFile(glowManager, fmt::format("temple/interior/glow/{}", f.stem().string()), f,
				                 resources::LightLoader::FromDiskTag {}, f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
	    });

	view.StartupStep("temple interior meshes and glows");
	// The pack is kept by the loads of its meshes and textures until the last of them has run
	const auto pack = std::make_shared<pack::PackFile>();

	auto packResult = pack->ReadFile(*fileSystem.GetData(fileSystem.GetPath<Path::Data>() / "AllMeshes.g3d"));
	if (packResult != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Unable to load AllMeshes.g3d: {}", pack::ResultToStr(packResult));
		return false;
	}

	view.StartupStep("mesh pack read");
	const auto& meshes = pack->GetMeshes();
	for (uint32_t i = 0; i < meshes.size(); ++i)
	{
		meshManager.Register(
		    static_cast<MeshId>(i),
		    [pack, i] {
			    return resources::L3DLoader {}(resources::L3DLoader::FromBufferTag {}, std::string(k_MeshNames.at(i)),
			                                   pack->GetMesh(i));
		    },
		    meshes[i].size());
	}

	view.StartupStep("mesh pack meshes");
	// The lowest detail levels draw each texture's own low resolution picture, where the pack has one
	const bool lowTextures = Locator::config::value().detail.lowResTextures;
	for (const auto& [name, g3dTexture] : pack->GetTextures())
	{
		const auto low = pack->GetLowTextures().find(name);
		const auto* picture = lowTextures && low != pack->GetLowTextures().end() ? &low->second : &g3dTexture;
		textureManager.Register(
		    g3dTexture.header.id,
		    [pack, name, picture] {
			    return resources::Texture2DLoader {}(resources::Texture2DLoader::FromPackTag {}, name, *picture);
		    },
		    picture->ddsData.size());
	}

	view.StartupStep("mesh pack textures");
	pack::PackFile animationPack;
	packResult = animationPack.ReadFile(*fileSystem.GetData(fileSystem.GetPath<Path::Data>() / "AllAnims.anm"));
	if (packResult != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Unable to load AllAnims.anm: {}", pack::ResultToStr(packResult));
		return false;
	}

	const auto& animations = animationPack.GetAnimations();
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; i < animations.size(); i++)
	{
		animationManager.Load(i, resources::L3DAnimLoader::FromBufferTag {}, animations[i]);
	}
	view.StartupStep("animation pack");
	// The game stops one of the packed clips playing round and round as it loads them
	if (constexpr uint32_t k_HeldOnceClip = 323; animationManager.Contains(resources::HashIdentifier(k_HeldOnceClip)))
	{
		animationManager.Handle(resources::HashIdentifier(k_HeldOnceClip))->StopLooping();
	}
	// The sounds placed on the frames of the people's, animals' and birds' clips
	if (const auto sounds = fileSystem.GetPath<Path::Data>() / "SmallSounds.SAS"; fileSystem.Exists(sounds))
	{
		resources.GetClipSounds().Load(audio::clip_sounds::k_TableId.value(), resources::ClipSoundsLoader::FromDiskTag {},
		                               sounds);
	}
	else
	{
		resources.GetClipSounds().Load(audio::clip_sounds::k_TableId.value(), resources::ClipSoundsLoader::EmptyTag {});
	}
	{
		// Each name's sounds go to the first packed clip bearing it
		std::vector<std::string_view> packNames;
		packNames.reserve(animations.size());
		for (size_t i = 0; i < animations.size(); ++i)
		{
			const auto clip = animationManager.Handle(resources::HashIdentifier(i));
			packNames.emplace_back(clip ? std::string_view(clip->GetName().c_str()) : std::string_view {});
		}
		resources.GetClipSounds().Handle(audio::clip_sounds::k_TableId.value())->Attach(packNames);
	}

	view.StartupStep("clip sounds");
	fileSystem.Iterate(fileSystem.GetPath<Path::CreatureMesh>(), false, [&meshManager](const std::filesystem::path& f) {
		const auto& fileName = f.stem().string();
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading creature mesh: {}", fileName);
		try
		{
			if (string_utils::BeginsWith(fileName, "Hand"))
			{
				return;
			}

			const auto meshId = creature::GetIdFromMeshName(fileName);
			RegisterFile(meshManager, meshId, f, resources::L3DLoader::FromDiskTag {}, f);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	view.StartupStep("creature meshes");
	fileSystem.Iterate(fileSystem.GetPath<Path::Symbols>(), false, [&camPathManager](const std::filesystem::path& f) {
		if (f.extension() != ".cam")
		{
			return;
		}
		const auto& fileName = f.stem().string();
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading symbol cam: {}", fileName);
		const auto pathId = fmt::format("symbol/{}", fileName);
		try
		{
			camPathManager.Load(pathId, resources::CameraPathLoader::FromDiskTag {}, f);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	fileSystem.Iterate(fileSystem.GetPath<Path::CitadelEngine>(), false, [&camPathManager](const std::filesystem::path& f) {
		if (f.extension() != ".cam")
		{
			return;
		}
		const auto& fileName = f.stem().string();
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple cam: {}", fileName);
		const auto pathId = fmt::format("temple/{}", fileName);
		try
		{
			camPathManager.Load(pathId, resources::CameraPathLoader::FromDiskTag {}, f);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	view.StartupStep("camera paths");
	// Load loose one-off assets
	{
		using AFromDiskTag = resources::L3DAnimLoader::FromDiskTag;
		animationManager.Load("coffre", AFromDiskTag {}, fileSystem.GetPath<Path::Misc>() / "coffre.anm");

		using LFromDiskTag = resources::L3DLoader::FromDiskTag;
		meshManager.Load("hand", LFromDiskTag {}, fileSystem.GetPath<Path::CreatureMesh>() / "Hand_Boned_Base2.l3d");
		view.StartupStep("hand mesh");
		view.LoadHandAnimation();
		view.StartupStep("hand animations");
		LoadCreatureRigs();
		view.StartupStep("creature rigs");
		Locator::advisorSystem::value().Load();
		const auto registerMesh = [&meshManager](auto id, const std::filesystem::path& path) {
			RegisterFile(meshManager, id, path, LFromDiskTag {}, path);
		};
		registerMesh("coffre", fileSystem.GetPath<Path::Misc>() / "coffre.l3d");
		// The closed reward chest
		if (const auto path = fileSystem.GetPath<Path::Misc>() / "chest0.l3d"; fileSystem.Exists(path))
		{
			registerMesh("misc/chest0", path);
		}
		// The collar the citadel's leash posts are drawn with
		if (const auto path = fileSystem.GetPath<Path::Misc>() / "leash.l3d"; fileSystem.Exists(path))
		{
			registerMesh("misc/leash", path);
		}
		// The eyes every creature is drawn with
		for (const auto& [id, file] : {std::pair {ecs::components::CreatureEyes::k_EyeballMeshId, "Eyeball.l3d"},
		                               std::pair {ecs::components::CreatureEyes::k_EyelidMeshId, "Eyelid.l3d"}})
		{
			if (const auto path = fileSystem.GetPath<Path::Data>() / file; fileSystem.Exists(path))
			{
				registerMesh(id, path);
			}
		}
		registerMesh("cone", fileSystem.GetPath<Path::Data>() / "cone.l3d");
		registerMesh("marker", fileSystem.GetPath<Path::Data>() / "marker.l3d");
		registerMesh("river", fileSystem.GetPath<Path::Data>() / "river.l3d");
		registerMesh("river2", fileSystem.GetPath<Path::Data>() / "river2.l3d");
		registerMesh("metre_sphere", fileSystem.GetPath<Path::Data>() / "metre_sphere.l3d");
		// The sky: its dome, with the dome's pictures, the sun and the moon
		using SkyArchetype = ecs::archetypes::SkyArchetype;
		registerMesh(SkyArchetype::k_DomeMeshId.value(), fileSystem.GetPath<Path::WeatherSystem>() / "sky.l3d");
		{
			std::vector<std::filesystem::path> pictures;
			for (const auto& file : sky_dome::PictureFiles())
			{
				pictures.push_back(fileSystem.GetPath<Path::WeatherSystem>() / file);
			}
			textureManager.Load(SkyArchetype::k_DomeTextureId.value(), resources::Texture2DLoader::FromBitmapLayersTag {},
			                    "Sky", pictures, sky_dome::k_Rows);
		}
		registerMesh(SkyArchetype::k_SunMeshId.value(), fileSystem.GetPath<Path::WeatherSystem>() / "sun.l3d");
		registerMesh(SkyArchetype::k_MoonMeshId.value(), fileSystem.GetPath<Path::WeatherSystem>() / "moon.l3d");
		registerMesh(ecs::components::Mist::k_MeshId, fileSystem.GetPath<Path::Landscape>() / "mist.l3d");

		using CFromDiskTag = resources::CameraPathLoader::FromDiskTag;
		camPathManager.Load("cam", CFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "cam.cam");
		camPathManager.Load("flying", CFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "flying.cam");
	}

	view.StartupStep("one-off meshes");
	// The info tables come before the game's interface, whose tooltips take their priorities and times from them
	{
		InfoFile infoFile;
		auto result = infoFile.LoadFromFile(Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / "info.dat");
		if (!result)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to load game info data.");
			return false;
		}
		Locator::infoConstants::reset(result.release());
	}

	view.StartupStep("info tables");
	// The current profile's own pictures take the places of their designs from the start
	std::ignore = ReadTattooPictures();
	// The game's menu, made by the view, which gives the texts of the game; none with no view
	const auto* texts = view.MakeInterface();

	view.StartupStep("game interface");
	// TODO(raffclar): #400: Parse level files within the resource loader
	// TODO(raffclar): #405: Determine campaign levels from the challenge script file
	// Load the campaign levels
	fileSystem.Iterate(fileSystem.GetPath<Path::Scripts>(), false, [&levelManager](const std::filesystem::path& f) {
		const auto& name = f.stem().string();
		if (f.extension() != ".txt" || name.rfind("InfoScript", 0) != std::string::npos)
		{
			return;
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading campaign level: {}", f.stem().string());
		try
		{
			if (Level::IsLevelFile(f))
			{
				levelManager.Load(fmt::format("campaign/{}", name), resources::LevelLoader::FromDiskTag {}, f,
				                  Level::LandType::Campaign);
			}
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});
	// Load Playgrounds
	// Attempt to load additional levels as playgrounds
	fileSystem.Iterate(fileSystem.GetPath<Path::Playgrounds>(), false, [&levelManager](const std::filesystem::path& f) {
		if (f.extension() != ".txt")
		{
			return;
		}
		const auto& name = f.stem().string();
		if (levelManager.Contains(fmt::format("playgrounds/{}", name)))
		{
			// Already added
			return;
		}

		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading custom level: {}", f.stem().string());
		try
		{
			if (Level::IsLevelFile(f))
			{
				levelManager.Load(fmt::format("playgrounds/{}", name), resources::LevelLoader::FromDiskTag {}, f,
				                  Level::LandType::Skirmish);
			}
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	view.StartupStep("level list");
	// Load all sound packs in the Audio directory
	auto& audioManager = Locator::audio::value();
	fileSystem.Iterate(
	    fileSystem.GetPath<Path::Audio>(), true, [&audioManager, &soundManager, &fileSystem](const std::filesystem::path& f) {
		    if (f.extension() != ".sad")
		    {
			    return;
		    }

		    // Only the samples' headers are read now: each sample is read from the file when it is wanted
		    pack::PackFile soundPack;
		    SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Opening sound pack {}", f.filename().string());
		    auto result = soundPack.ReadFile(*fileSystem.GetData(f), {"LHAudioWaveData"});
		    const auto waveData = soundPack.GetUnreadBlock("LHAudioWaveData");
		    if (result == pack::PackResult::Success && soundPack.HasBlock("LHAudioBankSampleTable") && !waveData.has_value())
		    {
			    result = pack::PackResult::ErrMissingAudioWaveDataBlock;
		    }
		    const auto& audioHeaders = soundPack.GetAudioSampleHeaders();
		    // As when the whole pack is read, a sample beyond the wave data spoils the pack
		    if (result == pack::PackResult::Success && waveData.has_value() &&
		        std::ranges::any_of(audioHeaders, [&waveData](const pack::AudioBankSampleHeader& header) {
			        return static_cast<uint64_t>(header.offset) + header.size > waveData->size;
		        }))
		    {
			    result = pack::PackResult::ErrFileTooSmall;
		    }
		    if (result != pack::PackResult::Success)
		    {
			    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to load sound pack {}: {}", f.filename().string(),
			                        pack::ResultToStr(result));
			    return;
		    }

		    if (audioHeaders.empty())
		    {
			    SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Empty sound pack found for {}. Skipping", f.filename().string());
			    return;
		    }
		    auto soundName = std::filesystem::path(audioHeaders[0].name.data());

		    auto groupName = f.filename().string();

		    // A hacky way of detecting if the sound is music as all music sounds end with "mpg"
		    if (soundName.extension() == ".mpg")
		    {
			    auto buffers = std::queue<std::vector<uint8_t>>();
			    auto packName = f.string();
			    audioManager.AddMusicEntry(packName);
		    }
		    else
		    {
			    audioManager.CreateSoundGroup(groupName);
			    const bool decodeAhead = std::ranges::find(k_DecodedAheadBanks, groupName) != k_DecodedAheadBanks.end();
			    for (size_t i = 0; i < audioHeaders.size(); i++)
			    {
				    soundName = std::filesystem::path(audioHeaders[i].name.data());
				    // Banks have gaps between their samples, which are skipped without skipping the samples after them
				    if (audioHeaders[i].size == 0)
				    {
					    SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Empty sound buffer found for {}/{}. Skipping", groupName,
					                        audioHeaders[i].id);
					    continue;
				    }

				    const auto stringId = fmt::format("{}/{}", groupName, audioHeaders[i].id);
				    const entt::id_type id = entt::hashed_string(stringId.c_str());
				    SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Registering sound {}: {}", stringId,
				                        audioHeaders[i].name.data());
				    soundManager.RegisterLoad(id, audioHeaders[i].size, resources::SoundLoader::FromBankFileTag {}, f,
				                              waveData->offset, audioHeaders[i], decodeAhead);
				    audioManager.AddToSoundGroup(groupName, id);
			    }

			    // What the bank plays for things happening in the game, such as trees rustling
			    if (soundPack.HasBlock("LHAudioAnimArrayTable") && soundPack.HasBlock("LHAudioWaveNumTable"))
			    {
				    if (auto effects = audio::AnimEffectTable::Parse(soundPack.GetBlock("LHAudioAnimArrayTable"),
				                                                     soundPack.GetBlock("LHAudioWaveNumTable")))
				    {
					    audioManager.AddAnimEffects(groupName, std::move(*effects));
				    }
				    else
				    {
					    SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Malformed animation effects in sound pack {}",
					                       f.filename().string());
				    }
			    }
		    }
	    });

	// Which sample of the speech banks says each help text, for the lines the scripts have spoken
	if (texts != nullptr)
	{
		auto& sounds = Locator::resources::value().GetSounds();
		std::array<std::vector<audio::SpeechBankSample>, audio::k_SpeechBankFiles.size()> banks;
		for (const auto& [groupName, group] : audioManager.GetSoundGroups())
		{
			const auto bank = std::ranges::find_if(audio::k_SpeechBankFiles, [&groupName](std::string_view file) {
				return string_utils::LowerCase(std::string(file)) == string_utils::LowerCase(groupName);
			});
			if (bank == audio::k_SpeechBankFiles.end())
			{
				continue;
			}
			auto& samples = banks.at(static_cast<size_t>(std::distance(audio::k_SpeechBankFiles.begin(), bank)));
			for (const auto id : group.sounds)
			{
				const auto& sound = *sounds.Handle(id);
				samples.push_back({.sample = static_cast<uint32_t>(sound.id), .file = sound.name, .sound = id});
			}
		}
		Locator::helpSpeechSystem::value().SetTable(audio::HelpSpeechTable(texts->GetHelpNames(), banks));
		// The scripts' dialogue, its text sized for the screen it starts on
		const int screenHeight = Locator::windowing::has_value() ? Locator::windowing::value().GetSize().y : 0;
		Locator::helpTextSystem::value().Start(*texts, screenHeight);
		// What the dialogue changing hands does to the advisors and the texts
		Locator::dialogueControlSystem::value().SetHooks({
		    .sendSpiritsHome = [](bool helpScript) { ecs::systems::SendAdvisorsHome(helpScript); },
		    .taken = []() { Locator::helpTextSystem::value().ClearAllText(); },
		    .released =
		        [](bool helpScript) {
			        // They fly home, are cut short in what they say, are sent home as the script would, and the texts go
			        ecs::systems::SendAdvisorsHome(false);
			        Locator::helpTextSystem::value().InterruptAdvisors();
			        ecs::systems::SendAdvisorsHome(helpScript);
			        Locator::helpTextSystem::value().ClearAllText();
		        },
		});
	}

	view.StartupStep("sound and music banks");
	// The temple's leashes are drawn with the leash texture and its alpha
	if (const auto leash = fileSystem.GetPath<Path::Textures>() / "leash.raw",
	    alpha = fileSystem.GetPath<Path::Textures>() / "leasha.raw";
	    fileSystem.Exists(leash) && fileSystem.Exists(alpha))
	{
		constexpr uint16_t k_LeashTextureSide = 256;
		try
		{
			textureManager.Load(ecs::components::LeashPost::k_TextureId, resources::Texture2DLoader::FromDiskWithAlphaTag {},
			                    leash, alpha, k_LeashTextureSide);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}
	// The marks the vortices leave on the ground, with their alphas
	for (const auto& textures : ecs::components::Vortex::k_GroundTextures)
	{
		for (const auto& [id, name] :
		     {std::pair {textures.hole, textures.holeFile}, std::pair {textures.ring, textures.ringFile}})
		{
			const auto colours = fileSystem.GetPath<Path::Textures>() / fmt::format("{}.raw", name);
			const auto alpha = fileSystem.GetPath<Path::Textures>() / fmt::format("{}a.raw", name);
			if (!fileSystem.Exists(colours) || !fileSystem.Exists(alpha))
			{
				continue;
			}
			constexpr uint16_t k_VortexTextureSide = 256;
			try
			{
				textureManager.Load(id, resources::Texture2DLoader::FromDiskWithAlphaTag {}, colours, alpha,
				                    k_VortexTextureSide);
			}
			catch (std::runtime_error& err)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			}
		}
	}
	// The sea's texture, its colours and alpha together
	try
	{
		// The lowest detail levels load the sea's picture at half its size
		textureManager.Load(sea_texture::k_Id.value(), resources::Texture2DLoader::FromDiskWithAlphaTag {},
		                    fileSystem.GetPath<Path::Textures>() / sea_texture::k_ColourFile,
		                    fileSystem.GetPath<Path::Textures>() / sea_texture::k_AlphaFile, sea_texture::k_Side,
		                    Locator::config::value().detail.lowResTextures);
	}
	catch (std::runtime_error& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
	}
	// Each texture is kept at the depth the game keeps it: colours with an alpha file and the alphas four bits a channel,
	// colours without one five bits a channel, and the small pictures the game reads as numbers as they are
	std::vector<std::filesystem::path> rawFiles;
	std::unordered_set<std::string> rawStems;
	fileSystem.Iterate(fileSystem.GetPath<Path::Textures>(), false, [&rawFiles, &rawStems](const std::filesystem::path& f) {
		// The game ships a grey ice map it never loads, cut short of a whole texture
		constexpr std::string_view k_UnusedTexture = "s_iceenvmapgrey.raw";
		if (string_utils::LowerCase(f.extension().string()) == ".raw" &&
		    string_utils::LowerCase(f.filename().string()) != k_UnusedTexture)
		{
			rawFiles.push_back(f);
			rawStems.insert(string_utils::LowerCase(f.stem().string()));
		}
	});
	for (const auto& f : rawFiles)
	{
		// The people's shadow is a grey picture the game draws as an alpha
		constexpr std::string_view k_GreyAlpha = "human_shadow";
		const auto stem = string_utils::LowerCase(f.stem().string());
		const bool isAlpha = stem == k_GreyAlpha || (stem.ends_with('a') && rawStems.contains(stem.substr(0, stem.size() - 1)));
		const auto bytes = GameFileBytes(f);
		if (!bytes)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't find {}", f.generic_string());
			continue;
		}
		const auto kind = graphics::texel_depth::Classify(*bytes, rawStems.contains(stem + "a"), isAlpha);
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading raw texture: {}", f.stem().string());
		try
		{
			const auto name = fmt::format("raw/{}", f.stem().string());
			const auto depth = graphics::texel_depth::DepthOf(kind);
			if (auto halved = HalvedAtThisDetail(f))
			{
				textureManager.RegisterLoad(name, halved->size(), resources::Texture2DLoader::FromBufferTag {}, name,
				                            std::move(*halved), depth);
			}
			else
			{
				textureManager.RegisterLoad(name, *bytes, resources::Texture2DLoader::FromDiskTag {}, f, depth);
			}
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	view.StartupStep("raw textures");
	// The texture every creature's hair is drawn with, and its alpha beside it
	for (const auto& [id, name] : {std::pair {ecs::components::CreatureHair::k_TextureId, "C_Ape_Hair.raw"},
	                               std::pair {ecs::components::CreatureHair::k_AlphaTextureId, "C_Ape_Haira.raw"}})
	{
		const auto path = fileSystem.GetPath<Path::Data>() / name;
		if (!fileSystem.Exists(path))
		{
			continue;
		}
		try
		{
			// The game loads the hair with its alpha, so both keep four bits a channel
			LoadRawTexture(textureManager, id, path, graphics::texel_depth::Depth::FourBits);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	// What creatures' tattoos and marks are painted with
	LoadCreatureSkinArt();

	// The noise that makes the snow's edges on the land ragged
	try
	{
		textureManager.Load(snow_cover::k_NoiseTextureId.value(), resources::Texture2DLoader::FromDiskTag {},
		                    fileSystem.GetPath<Path::WeatherSystem>() / "snowmap.raw");
	}
	catch (std::runtime_error& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
	}
	view.StartupStep("hair, skin art and snow textures");

	// Everything registered is loaded on the loading threads from now on, while the first land is made: what the land
	// needs before its turn comes is loaded there and then
	resources.PrefetchAll();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "{} resources registered to be loaded", resources.PendingCount());

	return true;
}

void LoadCreatureRigs()
{
	auto& fileSystem = Locator::filesystem::value();
	const auto directory = fileSystem.GetPath<filesystem::Path::Data>() / "CTR";
	const auto specPath = fileSystem.GetPath<filesystem::Path::Data>() / "ctrspec27.txt";
	if (!fileSystem.Exists(specPath))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The creatures are not animated: {} is missing", specPath.string());
		return;
	}
	const auto specDirectory = fileSystem.FindPath(specPath).parent_path();
	const auto meshDirectory = fileSystem.GetPath<filesystem::Path::CreatureMesh>();
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	fileSystem.Iterate(directory, false, [&](const std::filesystem::path& path) {
		if (string_utils::LowerCase(path.extension().string()) != ".cbn")
		{
			return;
		}
		// Only the header of the species' animations is read now, for which species they are; the rest when the species
		// is first wanted
		pack::PackFile pack;
		const auto stream = fileSystem.GetData(path);
		const auto block =
		    pack.ReadFile(*stream, {"Creature"}) == pack::PackResult::Success ? pack.GetUnreadBlock("Creature") : std::nullopt;
		if (!block.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the Creature block of {}", path.string());
			return;
		}
		if (block->size < sizeof(morph::MorphHeader))
		{
			return;
		}
		// The species is the one the base mesh named in the header is of
		morph::MorphHeader header {};
		stream->clear();
		stream->seekg(static_cast<std::streamoff>(block->offset));
		stream->read(reinterpret_cast<char*>(&header), sizeof(header));
		const auto species = creature::GetSpeciesFromMeshName(header.baseMeshName.data());
		if (!*stream || species == CreatureType::Unknown)
		{
			return;
		}
		// The creature paints its skins from the files of its base, evil and good meshes when it first shows.
		// Register them, so that the game loads them behind the loading screen.
		auto& files = Locator::resources::value().GetL3DFiles();
		const auto registerSkins = [&files, &meshDirectory](const std::array<char, 0x20>& field) {
			const std::string_view raw(field.data(), field.size());
			const std::string name(raw.substr(0, raw.find(char {})));
			if (!name.empty())
			{
				const auto skinPath = meshDirectory / (name + ".l3d");
				RegisterFile(files, creature_skin::MeshFileId(name), skinPath, resources::L3DFileLoader::FromDiskTag {},
				             skinPath);
			}
		};
		registerSkins(header.baseMeshName);
		registerSkins(header.variantMeshNames.at(static_cast<size_t>(creature::CreatureRig::Mesh::Evil) - 1));
		registerSkins(header.variantMeshNames.at(static_cast<size_t>(creature::CreatureRig::Mesh::Good) - 1));
		rigs.Register(
		    creature::GetRigId(species),
		    [path, block = *block, specDirectory, meshDirectory] {
			    const auto file = Locator::filesystem::value().GetData(path);
			    std::vector<uint8_t> bytes(block.size);
			    file->seekg(static_cast<std::streamoff>(block.offset));
			    file->read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
			    if (!*file)
			    {
				    throw std::runtime_error("Unable to read the Creature block of " + path.string());
			    }
			    return resources::CreatureRigLoader {}(resources::CreatureRigLoader::FromBufferTag {}, bytes, specDirectory,
			                                           meshDirectory);
		    },
		    block->size);
	});
}

void LoadCreatureSkinArt()
{
	using filesystem::Path;
	auto& fileSystem = Locator::filesystem::value();
	try
	{
		const auto data = fileSystem.GetPath<Path::Data>();
		auto& arts = Locator::resources::value().GetCreatureSkinArt();
		arts.Erase(creature_skin::k_ArtId);
		arts.Load(creature_skin::k_ArtId, resources::CreatureSkinArtLoader::FromDiskTag {},
		          resources::CreatureSkinArtLoader::Paths {
		              .symbols = fileSystem.GetPath<Path::Textures>() / "OriginalChooseSymbol.raw",
		              .freshDamage = data / "damage_new256.raw",
		              .freshDamageAlpha = data / "damage_new256A.raw",
		              .oldDamage = data / "damage_old256.raw",
		              .oldDamageAlpha = data / "damage_old256A.raw",
		              .palette = data / "tattoocols.raw",
		              .pictures = Locator::tattooEditorSystem::value().GetPictures(),
		              .lowDetail = Locator::config::value().detail.lowResTextures,
		          });
	}
	catch (std::runtime_error& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
	}
}

std::optional<creature_tattoo::Pictures> ReadTattooPictures()
{
	if (!Locator::tattooEditorSystem::has_value() || !Locator::profileFolderSystem::has_value() ||
	    !Locator::playerProfileSystem::has_value())
	{
		return std::nullopt;
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value())
	{
		return std::nullopt;
	}
	auto& editor = Locator::tattooEditorSystem::value();
	auto pictures = editor.GetPictures();
	const auto player = gui::ToUtf16(profile->name);
	const auto& folder = Locator::profileFolderSystem::value();
	tattoo_pictures::Read(pictures,
	                      [&folder, &player](const std::string& file) { return folder.ReadPlayerFile(player, file); });
	if (pictures == editor.GetPictures())
	{
		return std::nullopt;
	}
	editor.SetPictures(pictures);
	if (Locator::resources::value().GetCreatureSkinArt().Contains(creature_skin::k_ArtId))
	{
		LoadCreatureSkinArt();
		// The masks of the creature of the player at this computer take the new pictures, and every creature is painted
		// again
		const auto own =
		    Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
		if (const auto creature = Locator::playerSystem::value().GetPrimaryCreature(own))
		{
			Locator::creatureSkinSystem::value().RemakeTattooMasks(*creature, creature_tattoo::Sheet::OwnPictures);
		}
		Locator::entitiesRegistry::value().Each<ecs::components::CreatureTattoos>(
		    [](ecs::components::CreatureTattoos& tattoos) { ++tattoos.revision; });
	}
	return pictures;
}

ProfileMenu ProfileMenuSettings()
{
	const auto* user = std::getenv("USERNAME");
	user = user != nullptr ? user : std::getenv("USER");
	auto playerName = gui::ToUtf16(user != nullptr ? user : "Player");
	// The settings the menu starts with are the game's
	gui::MenuSettings settings;
	settings.leftHandedHand = !Locator::config::value().rightHandedHand;
	// The options the player keeps with their profile, the game's own without one
	if (const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile(); profile.has_value())
	{
		playerName = gui::ToUtf16(profile->name);
		settings.creatureName = gui::ToUtf16(profile->creatureName);
		settings.symbol = profile->symbol;
		gui::TakeProfileSettings(settings, profile->settings);
	}
	auto& audio = Locator::audio::value();
	settings.sfxVolume = audio.GetSfxVolume();
	settings.musicVolume = audio.GetMusicVolume();
	const auto& config = Locator::config::value();
	settings.detailLevels = config.maxDetailLevel;
	settings.detail = graphics::detail_level::MenuLevel(config.detailLevel, config.maxDetailLevel);
	return {.playerName = std::move(playerName), .settings = std::move(settings)};
}

void ApplyPlayerSettings(const player_profiles::Settings& settings)
{
	Locator::config::value().rightHandedHand = !settings.leftHanded;
	Locator::config::value().pushScrolling = settings.pushScrolling;
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().SetHelpOn(settings.creatureHelp);
	}
	if (Locator::chlapi::has_value())
	{
		Locator::chlapi::value().SetPlayerHelpLevel(static_cast<uint32_t>(std::max(settings.helpLevel, 0)));
	}
	if (Locator::helpTextSystem::has_value())
	{
		Locator::helpTextSystem::value().SetTextChoices(
		    {.textDraw = settings.storyText, .topToBottom = settings.textFromBottom, .readSpeed = settings.readSpeed});
	}
}

void PrepareNetworkLand(uint32_t seed)
{
	Locator::entitiesRegistry::value().SetFreshIdsAtNextReset();
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().SetFreshIdsAtNextReset();
	}
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().SetFreshIdsAtNextReset();
	}
	if (Locator::gameRandom::has_value())
	{
		Locator::gameRandom::value().Restart();
	}
	Locator::rng::value().SetRunSeed(seed);
}

std::optional<std::string_view> ApplySoakTools(const developer_tools::SoakTools& tools)
{
	if (const auto refusal = developer_tools::SoakRefusalOf(developer_tools::k_Built, tools); refusal.has_value())
	{
		return refusal;
	}
	if (Locator::landSystem::has_value())
	{
		Locator::landSystem::value().SetAllComputerGods(tools.allComputerGods);
	}
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetAiSeed(tools.aiSeed);
	}
	return std::nullopt;
}

void LoadEveryResource()
{
	auto& loading = Locator::resources::value();
	const auto loadStart = std::chrono::steady_clock::now();
	while (loading.PendingCount() > 0)
	{
		loading.UpdateLoading(k_LoadingScreenLoadBudget, k_FrameUploads);
		// The graphics card takes what the loading made, as on the loading screen
		Locator::rendererInterface::value().Frame();
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Every resource loaded in {:.0f} ms",
	                   std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - loadStart).count());
}

void BeginPlay(bool continued, bool loadAllFirst, PlayStartViewInterface& view)
{
	// Everything the map made goes into the map's cells, in the order it was made
	Locator::entitiesMap::value().Sync();

	view.SizeSceneToWindow();

	// A saved game keeps its own time of day
	if (!continued)
	{
		Locator::skySystem::value().SetTime(Locator::config::value().timeOfDay);
	}
	Locator::time::value().Start();

	view.StartPlayView();

	// The models of the miracles' globes come with the world: the physics of a thrown globe needs them, also on a computer
	// that draws no frame
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().LoadMeshes();
	}

	// A simulation run with a fixed frame time loads every resource before its first frame. A thing that waits for its
	// picture or its model then starts after the same frames each run, at any speed.
	if (loadAllFirst)
	{
		LoadEveryResource();
	}
	view.BeforeFirstFrame(continued);
	// Just before its first frame, the game puts the turn timer back to the start of the current turn. Thus the time of
	// the load, the logo and the menu is not game time. A game that continued from a save keeps the time of the save.
	if (!continued)
	{
		Locator::time::value().ResetTimerToTurn();
	}
}

} // namespace openblack::simulation
