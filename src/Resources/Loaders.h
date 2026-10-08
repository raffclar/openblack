/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <map>
#include <optional>
#include <queue>
#include <span>
#include <string>

#include <PackFile.h>

#include "3D/CameraPath.h"
#include "3D/CameraTracks.h"
#include "3D/L3DAnim.h"
#include "3D/L3DSubMesh.h"
#include "3D/Light.h"
#include "Audio/Device/Sound.h"
#include "Common/HelpText.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"
#include "Graphics/Texture2D.h"
#include "Level.h"

namespace openblack
{
class LandLightPalette;
} // namespace openblack

namespace openblack::graphics
{
class GameFont;
class L3DMesh;
class Texture2D;
} // namespace openblack::graphics

namespace openblack::l3d
{
class L3DFile;
} // namespace openblack::l3d

namespace openblack::ecs::super_villager
{
struct HdModel;
} // namespace openblack::ecs::super_villager

namespace openblack::psys
{
struct File;
struct EnumNames;
} // namespace openblack::psys

namespace openblack::graphics::frame_anim
{
struct StackedFrames;
} // namespace openblack::graphics::frame_anim

namespace openblack::psys::explode_object
{
struct SourceMesh;
} // namespace openblack::psys::explode_object

namespace openblack::magic::gestures
{
struct GestureData;
struct Shape;
} // namespace openblack::magic::gestures

namespace openblack::pack
{
struct AudioBankSampleHeader;
struct G3DTexture;
} // namespace openblack::pack

namespace openblack::resources
{

template <typename Resource>
struct BaseLoader
{
	using result_type = std::shared_ptr<Resource>;
	using ResourceType = Resource;
	struct FromBufferTag
	{
	};
	struct FromDiskTag
	{
	};
};

struct L3DLoader final: BaseLoader<graphics::L3DMesh>
{
	struct FromGeneratedTag
	{
	};
	/// A mesh made from a file already read, whose skins can take new texels later (a temple's outside)
	struct FromDynamicFileTag
	{
	};

	[[nodiscard]] result_type operator()(FromGeneratedTag, const std::string& debugName,
	                                     const std::vector<graphics::L3DSubMesh::GeneratedPrimitive>& primitives) const;
	[[nodiscard]] result_type operator()(FromDynamicFileTag, const std::string& debugName, const l3d::L3DFile& file) const;
	[[nodiscard]] result_type operator()(FromBufferTag, const std::string& debugName, const std::vector<uint8_t>& data) const;
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

/// The data of an L3D file, .l3d or zipped .zzz, for what changes meshes on the CPU
struct L3DFileLoader final: BaseLoader<l3d::L3DFile>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

struct Texture2DLoader final: BaseLoader<graphics::Texture2D>
{
	struct FromPackTag
	{
	};
	/// A texture made of a colour file (8-bit RGB) and an alpha file (8 bits a pixel), as the game's x.raw and xa.raw
	struct FromColourAlphaTag
	{
	};
	/// How the colour and alpha bytes become RGBA8
	enum class ColourAlpha : uint8_t
	{
		Interleaved, ///< colour and alpha side by side as they are; both files are needed
		Argb4444,    ///< both cut to 4 bits (argb4444::PackRaw); the alpha file may be missing
	};
	struct ColourAlphaDesc
	{
		std::filesystem::path colour;
		std::filesystem::path alpha;
		uint16_t size {256}; ///< square
		ColourAlpha packing {ColourAlpha::Interleaved};
		graphics::Wrapping wrap {graphics::Wrapping::ClampEdge};
		graphics::Filter filter {graphics::Filter::Linear};
	};
	/// The RGBA8 pixels of a colour and an alpha file, or nothing when their sizes do not fit `desc`
	[[nodiscard]] static std::optional<std::vector<uint8_t>>
	PackColourAlpha(const std::vector<uint8_t>& colour, const std::vector<uint8_t>& alpha, const ColourAlphaDesc& desc);

	[[nodiscard]] result_type operator()(FromPackTag, const std::string& name, const pack::G3DTexture& g3dTexture) const;
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& rawTexturePath) const;
	[[nodiscard]] result_type operator()(FromColourAlphaTag, const std::string& name, const ColourAlphaDesc& desc) const;
};

struct L3DAnimLoader final: BaseLoader<L3DAnim>
{
	[[nodiscard]] result_type operator()(FromBufferTag, const std::vector<uint8_t>& data) const;
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

struct LevelLoader final: BaseLoader<Level>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path, Level::LandType landType) const;
};

struct CreatureMindLoader final: BaseLoader<creature::CreatureMind>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& creatureMindPath) const;
};

/// What moves a species' body, from the Creature block of its .cbn file. The species' meshes are read from
/// meshDirectory for where its eyes sit; the animations are named by the creature spec file in specDirectory.
struct CreatureRigLoader final: BaseLoader<creature::CreatureRig>
{
	[[nodiscard]] result_type operator()(FromBufferTag, const std::vector<uint8_t>& block,
	                                     const std::filesystem::path& specDirectory,
	                                     const std::filesystem::path& meshDirectory) const;
};

/// What creatures' tattoos and marks are painted with, read from raw images: the tattoo designs from the atlas of
/// players' symbols, the fresh and old damage atlases with their alphas, and the tattoo palette
struct CreatureSkinArtLoader final: BaseLoader<creature_skin::Art>
{
	struct Paths
	{
		/// The players' symbols as the game last wrote them, and the symbols it ships with, for the cells no player's
		/// symbol has been written into
		std::filesystem::path symbols;
		std::filesystem::path defaultSymbols;
		std::filesystem::path freshDamage;
		std::filesystem::path freshDamageAlpha;
		std::filesystem::path oldDamage;
		std::filesystem::path oldDamageAlpha;
		std::filesystem::path palette;
	};
	[[nodiscard]] result_type operator()(FromDiskTag, const Paths& paths) const;
};

struct SoundLoader final: BaseLoader<audio::Sound>
{
	[[nodiscard]] result_type operator()(FromBufferTag, const pack::AudioBankSampleHeader& header,
	                                     std::vector<std::vector<uint8_t>> buffer) const;
};

struct LightLoader final: BaseLoader<Lights>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

/// A game font from its two files, <base>.met and <base>.fnt
struct GameFontLoader final: BaseLoader<graphics::GameFont>
{
	[[nodiscard]] result_type operator()(FromBufferTag, const std::vector<uint8_t>& met, const std::vector<uint8_t>& fnt,
	                                     std::string_view name) const;
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& base) const;
};

/// The bytes of a data file that its user parses itself (shadow images, tables, recorded input)
struct HelpTextLoader final: BaseLoader<std::vector<helptext::Entry>>
{
	/// The texts of an InfoScript*.txt (UTF-16)
	[[nodiscard]] result_type operator()(FromBufferTag, const std::vector<uint8_t>& utf16) const;
	/// The same, its bytes from the byte cache (the interface reads the same scripts)
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

struct CameraTrackLoader final: BaseLoader<CameraTrack>
{
	/// A track segment of camera.edt; throws when it is not a track
	[[nodiscard]] result_type operator()(FromBufferTag, std::span<const uint8_t> segment) const;
};

struct SourceMeshLoader final: BaseLoader<psys::explode_object::SourceMesh>
{
	/// Mesh `index` of the pack as the exploding objects split it; throws when the bytes are not an L3D
	[[nodiscard]] result_type operator()(FromBufferTag, uint32_t index, const std::vector<uint8_t>& bytes) const;
};

struct HdModelLoader final: BaseLoader<ecs::super_villager::HdModel>
{
	/// Data\MISC\<relative>.l3d and its eye data; a model with no mesh when it cannot be read
	[[nodiscard]] result_type operator()(FromDiskTag, const std::string& relative, const std::string& name) const;
};

struct PSysFileLoader final: BaseLoader<psys::File>
{
	/// Data\Spells\ZSpellFiles\<name>.txt if it exists (the original reads a loose .txt first), else <name>_txt.zzz
	/// (u32 size, then zlib), parsed (psys::File::Parse); null, cached as such, when it is missing or not a spell file
	[[nodiscard]] result_type operator()(FromDiskTag, const std::string& name) const;
};

struct LightBitmapLoader final: BaseLoader<graphics::frame_anim::StackedFrames>
{
	/// Data\<name> decoded as land light frames (land_light::ReadBitmapFile); null, cached as such, when it is missing or
	/// of another size
	[[nodiscard]] result_type operator()(FromDiskTag, const std::string& name, int pitch, int bpp, int framesInFile,
	                                     int framesInUse) const;
};

struct EnumHeaderLoader final: BaseLoader<psys::EnumNames>
{
	/// Data\<file>'s enum (psys::ReadEnumHeader); empty, cached as such, when it cannot be read
	[[nodiscard]] result_type operator()(FromDiskTag, const std::string& file) const;
};

struct TextureStemsLoader final: BaseLoader<std::map<std::string, std::string, std::less<>>>
{
	/// The lower-case stem of every .raw in Data\Textures -> its real stem (psys::ReadTextureStems)
	[[nodiscard]] result_type operator()(FromDiskTag) const;
};

struct GestureTemplatesLoader final: BaseLoader<std::vector<magic::gestures::GestureData>>
{
	/// Gestures.jty's records (magic::gestures::LoadTemplates); an empty list when the bytes are short
	[[nodiscard]] result_type operator()(FromBufferTag, const std::vector<uint8_t>& bytes) const;
	/// Data\Gestures.jty, its bytes from the byte cache; an empty list, logged, when it cannot be read
	[[nodiscard]] result_type operator()(FromDiskTag) const;
};

struct GestureShapeLoader final: BaseLoader<magic::gestures::Shape>
{
	/// A PathSymbol .cam file's shape (magic::gestures::ParseShape); null when the bytes are not a shape
	[[nodiscard]] result_type operator()(FromBufferTag, const std::vector<uint8_t>& bytes) const;
	/// Data\Symbols\PathSymbol<number>.cam; null, cached as such, when it is missing or cannot be read
	[[nodiscard]] result_type operator()(FromDiskTag, int number) const;
};

struct CameraPathLoader final: BaseLoader<CameraPath>
{
	/// A .cam file's path, named for the debug tools
	[[nodiscard]] result_type operator()(FromBufferTag, const std::string& debugName, const std::vector<uint8_t>& data) const;
	/// A .cam file through the file system; throws when it cannot be read
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

struct LandLightPaletteLoader final: BaseLoader<LandLightPalette>
{
	/// A palette.raw's bytes; throws for the wrong size
	[[nodiscard]] result_type operator()(FromBufferTag, std::span<const uint8_t> bytes) const;
	/// The weather system's palette.raw through the file system; throws when it cannot be read or is the wrong size
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

struct BlobLoader final: BaseLoader<std::vector<uint8_t>>
{
	[[nodiscard]] result_type operator()(FromBufferTag, std::vector<uint8_t> data) const;
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};
} // namespace openblack::resources
