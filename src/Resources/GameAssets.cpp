/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameAssets.h"

#include <array>
#include <exception>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <MorphFile.h>
#include <PackFile.h>
#include <SDL.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/HandAnimation.h"
#include "3D/L3DMesh.h"
#include "3D/LowResolutionTextures.h"
#include "3D/PlayersSymbols.h"
#include "Creature/CreatureTattoo.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandMorph.h"
#include "ECS/GamePlayers.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "EngineConfig.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Visual/TexelDepth.h"
#if defined(__EMSCRIPTEN__)
#include "Web/WebPlatform.h"
#endif

namespace openblack::game_assets
{

namespace
{

/// The version file next to the game. The web build has it in its working folder, where the page puts it.
std::filesystem::path BuildVersionPath()
{
#if defined(__EMSCRIPTEN__)
	return std::filesystem::path(web::k_WorkingFolder) / build_version::k_FileName;
#else
	const std::unique_ptr<char, decltype(&SDL_free)> base(SDL_GetBasePath(), SDL_free);
	return (base ? std::filesystem::path(base.get()) : std::filesystem::current_path()) / build_version::k_FileName;
#endif
}

/// The meshes the hand is pulled towards as its player turns evil or good, the first two of its morph file's variants,
/// and the files of the base and those two for their skins. Black & White would also blend the hand's bones and
/// animations towards theirs, but its own meshes share the base's bones and the morph file has no animations for
/// them, so that changes nothing; a warning says when other data would.
void LoadHandLooks(const morph::MorphFile& morphFile)
{
	using ecs::components::HandMorph;
	auto& fileSystem = Locator::filesystem::value();
	auto& resources = Locator::resources::value();
	auto& meshes = resources.GetMeshes();
	auto& files = resources.GetL3DFiles();
	const auto& header = morphFile.GetHeader();
	const auto pathOf = [&fileSystem](const std::array<char, 0x20>& name) {
		return fileSystem.GetPath<filesystem::Path::CreatureMesh>() / (std::string(name.data()) + ".l3d");
	};
	const auto load = [&files](entt::id_type id, const std::filesystem::path& path) {
		try
		{
			files.Load(id, resources::L3DFileLoader::FromDiskTag {}, path);
		}
		catch (std::exception& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't read the hand's skins from {}: {}", path.string(), err.what());
		}
	};
	load(HandMorph::k_SkinFileIds[0], pathOf(header.baseMeshName));
	const auto base = meshes.Handle(ecs::components::Hand::k_MeshId);
	for (size_t look = 0; look < HandMorph::k_LookMeshIds.size(); ++look)
	{
		const auto& name = header.variantMeshNames.at(look);
		if (name[0] == '\0')
		{
			continue;
		}
		const auto path = pathOf(name);
		try
		{
			meshes.Load(HandMorph::k_LookMeshIds.at(look), resources::L3DLoader::FromDiskTag {}, path);
		}
		catch (std::exception& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't load the hand mesh {}: {}", path.string(), err.what());
			continue;
		}
		load(HandMorph::k_SkinFileIds.at(look + 1), path);
		const auto variant = meshes.Handle(HandMorph::k_LookMeshIds.at(look));
		if (base && variant->GetBoneMatrices() != base->GetBoneMatrices())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The bones of {} differ from the hand's: they are not blended",
			                   path.string());
		}
		if (!morphFile.GetVariantAnimationSet(static_cast<uint32_t>(look)).empty())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The animations of {} in the hand's morph file are not blended",
			                   path.string());
		}
	}
}
} // namespace

/// The openblack commit of this build. A build without the version file has an unknown version.
build_version::BuildVersion ReadBuildVersion(filesystem::FileSystemInterface& fileSystem)
{
	const auto path = BuildVersionPath();
	if (!fileSystem.Exists(path))
	{
		return {};
	}
	try
	{
		const auto bytes = fileSystem.ReadAll(path);
		return build_version::Parse(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
	}
	catch (const std::exception&)
	{
		return {};
	}
}

/// The language of the install from Country.txt in the game folder, or UK English when the game can not read the file
GameLanguage ReadGameLanguage(filesystem::FileSystemInterface& fileSystem)
{
	std::optional<std::vector<uint8_t>> contents;
	if (const auto path =
	        filesystem::FileSystemInterface::FindIgnoringCase(fileSystem.GetGamePath(), game_language::k_CountryFile);
	    path.has_value())
	{
		try
		{
			contents = fileSystem.ReadAll(*path);
		}
		catch (const std::exception& error)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Could not read {}: {}", game_language::k_CountryFile, error.what());
		}
	}
	const auto language = contents.has_value() ? game_language::FromCountryFile(std::span<const uint8_t>(*contents))
	                                           : game_language::FromCountryFile(std::nullopt);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The language of the install is \"{}\"", game_language::Code(language));
	return language;
}

std::unique_ptr<HandAnimation> LoadHandAnimation()
{
	auto& fileSystem = Locator::filesystem::value();
	const auto path = fileSystem.GetPath<filesystem::Path::Data>() / "CTR" / "hh.hbn";
	const auto specPath = fileSystem.GetPath<filesystem::Path::Data>() / "hndspec5.txt";
	if (!fileSystem.Exists(path) || !fileSystem.Exists(specPath))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The hand is not animated: {} or {} is missing", path.string(),
		                   specPath.string());
		return nullptr;
	}

	pack::PackFile pack;
	const auto packResult = pack.ReadFile(*fileSystem.GetData(path));
	if (packResult != pack::PackResult::Success || !pack.HasBlock("Hand"))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the Hand block of {}: {}", path.string(),
		                    pack::ResultToStr(packResult));
		return nullptr;
	}
	morph::MorphFile morphFile;
	const auto morphResult = morphFile.Open(pack.GetBlock("Hand"), fileSystem.FindPath(specPath).parent_path());
	if (morphResult != morph::MorphResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the hand animations of {}: {}", path.string(),
		                    morph::ResultToStr(morphResult));
		return nullptr;
	}

	LoadHandLooks(morphFile);

	const auto mesh = Locator::resources::value().GetMeshes().Handle(entt::hashed_string("hand"));
	auto animation = std::make_unique<HandAnimation>();
	if (!mesh || !animation->Load(morphFile, mesh->GetBoneParents(), mesh->GetBoneMatrices()))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The hand animations of {} do not fit the hand mesh", path.string());
		return nullptr;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loaded the hand animations of {}", path.string());
	return animation;
}

void MakePlayersSymbols(std::optional<uint32_t> symbolInPlace)
{
	using filesystem::Path;
	if (!Locator::filesystem::has_value() || !Locator::playerProfileSystem::has_value())
	{
		return;
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto textures = fileSystem.GetPath<Path::Textures>();
	const auto read = [&fileSystem](const std::filesystem::path& path) -> std::vector<uint8_t> {
		try
		{
			return fileSystem.ReadAll(path);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			return {};
		}
	};
	players_symbols::Sheet file {.colours = read(textures / "PlayersSymbols.raw"),
	                             .alpha = read(textures / "PlayersSymbolsa.raw")};
	const players_symbols::Sheet designs {.colours = read(textures / "OriginalChooseSymbol.raw"),
	                                      .alpha = read(textures / "OriginalChooseSymbola.raw")};
	if (!file.Whole() || !designs.Whole())
	{
		return;
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	const auto symbol =
	    players_symbols::ChosenSymbol(profile.has_value() ? std::optional(profile->symbol) : std::nullopt, symbolInPlace);
	std::optional<std::span<const uint8_t>> ownPicture;
	std::optional<creature_tattoo::Picture> picture;
	if (symbol.has_value() && Locator::tattooEditorSystem::has_value() && *symbol < creature_tattoo::k_DesignCount)
	{
		picture = Locator::tattooEditorSystem::value().GetPictures().at(*symbol);
		if (picture.has_value())
		{
			ownPicture = std::span<const uint8_t>(*picture);
		}
	}
	const auto localPlayer =
	    Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
	// A game with more player numbers than the squares of the file has a square for each player number
	auto wide = players_symbols::Widen(std::move(file), players_symbols::SquaresInARowFor(ecs::game_players::Count()));
	auto players = players_symbols::MakeSheet(std::move(wide), designs, symbol, static_cast<uint32_t>(localPlayer), ownPicture);
	// The lowest detail levels keep the sheet at half its size: each symbol takes every second texel of its full one
	const bool halve = Locator::config::value().detail.lowResTextures;
	if (halve)
	{
		for (auto* bytes : {&players.colours, &players.alpha})
		{
			if (auto halved = low_resolution_textures::Halve(*bytes))
			{
				*bytes = std::move(*halved);
			}
		}
	}
	// The square of each computer player takes the picture of its god. At the low detail levels the picture comes from
	// the god's file of half size. A computer god after the seventh takes the last picture (deviation 222).
	const auto landNumber = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
	for (const auto player : ecs::game_players::Gods())
	{
		if (!Locator::playerSystem::has_value() || !Locator::playerSystem::value().IsComputerPlayer(player))
		{
			continue;
		}
		const auto number = static_cast<uint32_t>(player);
		const auto god = players_symbols::k_GodPictures.at(players_symbols::GodPictureOf(number, landNumber));
		const auto name = fmt::format("{}{}.cps", god, halve ? players_symbols::k_LowDetailLetter : "");
		players_symbols::PutPicture(players, read(textures / name), number);
	}
	// The sheet takes the place of the file's, kept as the game keeps a texture with an alpha
	auto& textureManager = Locator::resources::value().GetTextures();
	const auto replace = [&textureManager](const std::string& name, std::vector<uint8_t> bytes) {
		textureManager.Erase(name);
		try
		{
			textureManager.Load(name, resources::Texture2DLoader::FromMadeTag {}, name, std::move(bytes),
			                    graphics::texel_depth::Depth::FourBits);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	};
	replace("raw/PlayersSymbols", std::move(players.colours));
	replace("raw/PlayersSymbolsa", std::move(players.alpha));
}

} // namespace openblack::game_assets
