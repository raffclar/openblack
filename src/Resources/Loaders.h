/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <queue>
#include <span>
#include <string>

#include <PackFile.h>

#include "3D/CameraPath.h"
#include "3D/L3DAnim.h"
#include "3D/L3DSubMesh.h"
#include "3D/Light.h"
#include "Audio/Sound.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"
#include "Level.h"

namespace openblack
{
class Bitmap16B;
class LandLightPalette;
} // namespace openblack

namespace openblack::graphics
{
class L3DMesh;
class Texture2D;
} // namespace openblack::graphics

namespace openblack::l3d
{
class L3DFile;
}

namespace openblack::psys
{
struct ParticleFile;
struct StackedBitmap;
} // namespace openblack::psys

namespace openblack::edt
{
class EDTFile;
} // namespace openblack::edt

namespace openblack::exc
{
class EXCFile;
} // namespace openblack::exc

namespace openblack::dance
{
struct DanceFile;
} // namespace openblack::dance
namespace openblack::hnd
{
struct HNDFile;
} // namespace openblack::hnd
namespace openblack::bink
{
class BinkFile;
} // namespace openblack::bink

namespace openblack::gestures
{
class GestureFile;
}

namespace openblack::pack
{
struct AudioBankSampleHeader;
struct G3DTexture;
} // namespace openblack::pack

namespace openblack::help::spirits
{
struct AdvisorModel;
}

namespace openblack::physics
{
class MaterialTable;
}

namespace openblack::audio::clip_sounds
{
class ClipSoundTable;
}

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
	/// From a file already read, as a mesh whose vertices and skins can be changed after
	struct FromDynamicFileTag
	{
	};

	[[nodiscard]] result_type operator()(FromBufferTag, const std::string& debugName, const std::vector<uint8_t>& data) const;
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
	[[nodiscard]] result_type operator()(FromDynamicFileTag, const std::string& debugName, const l3d::L3DFile& file) const;
	/// Made while the game runs from triangles of another model, drawn with its skins (a broken building's)
	struct FromMadeTag
	{
	};
	[[nodiscard]] result_type operator()(FromMadeTag, const std::string& debugName, const graphics::L3DMesh& skinSource,
	                                     std::span<const std::vector<graphics::L3DSubMesh::MadePrimitive>> subMeshes) const;
	/// From a file already read, drawn with the skins of another model (which must outlive it) as they change
	struct FromFileWithSkinsOfTag
	{
	};
	[[nodiscard]] result_type operator()(FromFileWithSkinsOfTag, const std::string& debugName, const l3d::L3DFile& file,
	                                     const graphics::L3DMesh& skinSource) const;
};

/// The data of an L3D file, .l3d or zipped .zzz, for what changes meshes on the CPU
struct L3DFileLoader final: BaseLoader<l3d::L3DFile>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
	/// A copy of a file made in the game, such as a blended mesh
	struct FromFileTag
	{
	};
	[[nodiscard]] result_type operator()(FromFileTag, const l3d::L3DFile& file) const;
};

/// A 16 bit image, .16B
struct Bitmap16BLoader final: BaseLoader<Bitmap16B>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

/// The sounds placed on the people's, animals' and birds' clips, from Data/SmallSounds.SAS; none when there is no file
struct ClipSoundsLoader final: BaseLoader<audio::clip_sounds::ClipSoundTable>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
	struct EmptyTag
	{
	};
	[[nodiscard]] result_type operator()(EmptyTag) const;
};

/// The physics materials of Data/PhysicsConstants.txt; every row zero when there is no file
struct PhysicsMaterialsLoader final: BaseLoader<physics::MaterialTable>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
	/// No file to read: every material is zero, as the game's are without one
	struct EmptyTag
	{
	};
	[[nodiscard]] result_type operator()(EmptyTag) const;
};

struct LandLightPaletteLoader final: BaseLoader<LandLightPalette>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

struct Texture2DLoader final: BaseLoader<graphics::Texture2D>
{
	struct FromPackTag
	{
	};

	/// A texture of colours with its alpha from the file beside it, "<name>a.raw"
	struct FromDiskWithAlphaTag
	{
	};

	/// A texture of layers, each a square 16-bit bitmap file of 5 bits a colour
	struct FromBitmapLayersTag
	{
	};

	[[nodiscard]] result_type operator()(FromPackTag, const std::string& name, const pack::G3DTexture& g3dTexture) const;
	[[nodiscard]] result_type operator()(FromBitmapLayersTag, const std::string& name,
	                                     std::span<const std::filesystem::path> layerPaths, uint16_t side) const;
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& rawTexturePath) const;
	[[nodiscard]] result_type operator()(FromDiskWithAlphaTag, const std::filesystem::path& rawTexturePath,
	                                     const std::filesystem::path& alphaPath, uint16_t side) const;
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
		/// The symbols the game ships with, which the tattoos are cut from whatever symbol the players have chosen
		std::filesystem::path symbols;
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
	                                     const std::vector<std::vector<uint8_t>>& buffer) const;

	/// A sample of a sound bank read from the bank's file, from where the bank's wave data starts plus the sample's
	/// offset; decoded too when `decode` is set, so its first play needn't
	struct FromBankFileTag
	{
	};
	[[nodiscard]] result_type operator()(FromBankFileTag, const std::filesystem::path& bank, uint64_t waveData,
	                                     const pack::AudioBankSampleHeader& header, bool decode) const;
};

struct LightLoader final: BaseLoader<Lights>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

/// A particle effect file by its name in a folder: <name>.txt when there is one, else the compressed <name>_txt.zzz
struct ParticleFileLoader final: BaseLoader<psys::ParticleFile>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& directory, const std::string& name) const;
};

/// A particle light map: a file of frames of pitch by pitch texels of `channels` bytes, laid out in a grid
struct ParticleBitmapLoader final: BaseLoader<psys::StackedBitmap>
{
	struct Layout
	{
		int pitch;
		int channels;
		int framesInFile;
		int framesInUse;
	};
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path, const Layout& layout) const;
};

/// A Bink video, .bik: its container, whose frames are decoded as it plays. None when it can't be read
struct VideoLoader final: BaseLoader<bink::BinkFile>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

/// The templates the hand's drawn gestures are matched against
struct GestureTemplatesLoader final: BaseLoader<gestures::GestureFile>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

/// The camera editor's file of the scripts' numbered cameras and tracks
struct CameraEditLoader final: BaseLoader<edt::EDTFile>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

/// A land's camera zones, from the files under Data/Zones
struct CameraZoneLoader final: BaseLoader<exc::EXCFile>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

/// A dance's choreography, from the files under the scripts' Dance folder
struct DanceFileLoader final: BaseLoader<dance::DanceFile>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

/// A hand demonstration, from the files under Data/HandDemo
struct HandDemoLoader final: BaseLoader<hnd::HNDFile>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};

/// An advisor from its .hd file: the file, its skeleton and its mesh
struct AdvisorModelLoader final: BaseLoader<help::spirits::AdvisorModel>
{
	[[nodiscard]] result_type operator()(FromBufferTag, const std::string& debugName, const std::vector<uint8_t>& data) const;
};

struct CameraPathLoader final: BaseLoader<CameraPath>
{
	[[nodiscard]] result_type operator()(FromDiskTag, const std::filesystem::path& path) const;
};
} // namespace openblack::resources
