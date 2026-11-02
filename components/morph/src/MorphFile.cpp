/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

/*
 * The layout of a Morph Spec Text File is as follows:
 *
 * - 1 line with an int representing the spec version
 * - 1 line category name per line starting with the '=' character
 * - 1 line with the character E indicating the end of the categories
 * - 1 line animation per remaining line where:
 *         the first character is the type
 *         the following characters form the name
 *         each animation belongs to the latest category line parsed
 *
 * The layout of a Morph Block is as follows:
 *
 * - 236 byte header
 *         1 unknown 32-bit int, TODO: is 0 for hands and 21 for creatures
 *         version of the morph spec file to be parsed
 *         binary version
 *         base mesh name
 *         6 optional variant mesh names (evil, good, thin, fat, weak, strong)
 *
 * ------------------------ start of animation offsets block ------------------
 *
 * - 4 bytes * number of animations * (1 + min(4, number of variants)) from
 *   specs file, each record contains:
 *         offset into animation block (see below)
 * - 4 byte offset to the next block (animation or hair groups when last)
 *
 * ------------------------ start of animation block --------------------------
 *
 * - 44 bytes header containing:
 *         duration in milliseconds
 *         1 when the animation is a cycle (type C), 0 for a pose range (type L)
 *         5 unknown floats, TODO: likely same role as as in ANMHeader
 *         frame count as a 32-bit int
 *         mesh bone count as a 32-bit int
 *         count of joint rotation joints as a 32-bit int
 *         count of joint translation joints as a 32-bit int
 * - 4 bytes * count of joint rotation joints containing:
 *         joint index of the target mesh
 * - 4 bytes * count of joint translation joints containing:
 *         joint index of the target mesh
 * - frame count Animation Frames, each containing:
 *          count of joint rotation joints * 12 byte euler angles
 *          count of joint translation joints * 12 byte vector
 *
 * ------------------------ start of hair group block -------------------------
 *
 * - 4 bytes 1 unknown int used if the binary version is greater than 4
 * - 4 bytes containing the number of hair groups
 * - for each group, a 100 byte header containing:
 *         segments version
 *         hair count
 *         segment count (points per strand)
 *         mapping index (1 when drawn with the hair texture)
 *         neutral, evil and good looks of 28 bytes each: red, green, blue
 *         as ints 0 to 255, then length, damping, stiffness and thickness
 *   followed by its 76 byte hairs, each containing:
 *         flags (bit 0: turned by its angles)
 *         36 byte point on a triangle of the base mesh
 *         neutral, evil and good x, y and z angles
 * - 4 byte offset to the next block (extra)
 *
 * ------------------------ start of extra animation block --------------------
 *
 * - 16 byte structure * number of non-zero offsets in the base animation
 *   block, containing:
 *         4 unknown 32-bit data, TODO
 *
 */

#include "MorphFile.h"

#include <cassert>
#include <cstring>

#include <algorithm>
#include <fstream>
#include <optional>
#include <utility>
#include <vector>

using namespace openblack::morph;

namespace
{
// Adapted from https://stackoverflow.com/a/13059195/10604387
//          and https://stackoverflow.com/a/46069245/10604387
struct membuf: std::streambuf
{
	membuf(char const* base, size_t size)
	{
		char* p(const_cast<char*>(base));
		this->setg(p, p, p + size);
	}
	std::streampos seekoff(off_type off, std::ios_base::seekdir way, [[maybe_unused]] std::ios_base::openmode which) override
	{
		if (way == std::ios_base::cur)
		{
			gbump(static_cast<int>(off));
		}
		else if (way == std::ios_base::end)
		{
			setg(eback(), egptr() + off, egptr());
		}
		else if (way == std::ios_base::beg)
		{
			setg(eback(), eback() + off, egptr());
		}
		return gptr() - eback();
	}

	std::streampos seekpos([[maybe_unused]] pos_type pos, [[maybe_unused]] std::ios_base::openmode which) override
	{
		return seekoff(pos - static_cast<off_type>(0), std::ios_base::beg, which);
	}
};
struct imemstream: virtual membuf, std::istream
{
	imemstream(char const* base, size_t size)
	    : membuf(base, size)
	    , std::istream(dynamic_cast<std::streambuf*>(this))
	{
	}
};

// https://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
std::istream& safe_getline(std::istream& is, std::string& t)
{
	t.clear();

	// The characters in the stream are read one-by-one using a std::streambuf.
	// That is faster than reading them one-by-one using the std::istream.
	// Code that uses streambuf this way must be guarded by a sentry object.
	// The sentry object performs various tasks,
	// such as thread synchronization and updating the stream state.

	std::istream::sentry se(is, true);
	std::streambuf* sb = is.rdbuf();

	for (;;)
	{
		const int c = sb->sbumpc();
		switch (c)
		{
		case '\n':
			return is;
		case '\r':
			if (sb->sgetc() == '\n')
			{
				sb->sbumpc();
			}
			return is;
		case std::streambuf::traits_type::eof():
			// Also handle the case when the last line has no line ending
			if (t.empty())
			{
				is.setstate(std::ios::eofbit);
			}
			return is;
		default:
			t += static_cast<char>(c);
		}
	}
}
} // namespace

std::string_view openblack::morph::ResultToStr(MorphResult result)
{
	switch (result)
	{
	case MorphResult::Success:
		return "Success";
	case MorphResult::ErrCantOpen:
		return "Could not open file.";
	case MorphResult::ErrFileTooSmall:
		return "File too small to be a valid Morph file.";
	case MorphResult::ErrSpecFileCantOpen:
		return "Could not open file spec file.";
	case MorphResult::ErrSpecFileVersionMismatch:
		return "Spec file version mismatch.";
	case MorphResult::ErrSpecFileAnimationsBeforeCategories:
		return "Spec file has animations before categories.";
	}
	std::unreachable();
}

MorphFile::MorphFile() noexcept = default;

MorphFile::~MorphFile() noexcept = default;

MorphResult MorphFile::ReadSpecFile(const std::filesystem::path& specFilePath) noexcept
{
	assert(!_isLoaded);

	_animationSpecs = {};
	_animationSpecs.path = specFilePath;
	std::ifstream spec(_animationSpecs.path);
	if (!spec.good())
	{
		return MorphResult::ErrSpecFileCantOpen;
	}
	std::string line;
	spec >> _animationSpecs.version >> std::ws;
	if (_header.specFileVersion != _animationSpecs.version)
	{
		return MorphResult::ErrSpecFileVersionMismatch;
	}

	[[maybe_unused]] bool reachedEnd = false;
	while (!spec.eof())
	{
		char type;
		spec.get(type);
		// End of file
		if (type == 'E')
		{
			reachedEnd = true;
			break;
		}
		safe_getline(spec, line);
		if (type == '=')
		{
			_animationSpecs.animationSets.emplace_back().name = line;
		}
		else
		{
			if (_animationSpecs.animationSets.empty())
			{
				return MorphResult::ErrSpecFileAnimationsBeforeCategories;
			}
			_animationSpecs.animationSets.back().animations.emplace_back(
			    AnimationDesc {line, static_cast<AnimationType>(type)});
		}
	}
	assert(reachedEnd);

	return MorphResult::Success;
}

std::vector<Animation> MorphFile::ReadAnimations(std::istream& stream, const std::vector<uint32_t>& offsets) noexcept
{
	assert(!_isLoaded);

	std::vector<Animation> animations;

	uint32_t i = 0;
	for (auto& animSet : _animationSpecs.animationSets)
	{
		for (const auto& animDesc : animSet.animations)
		{
			if (offsets[i] > 0)
			{
				stream.seekg(offsets[i]);
				auto& animation = animations.emplace_back();
				// The spec stores the node type (C/L) as the first character, followed by the name.
				animation.name = static_cast<char>(animDesc.type) + animDesc.name;
				animation.setName = animSet.name;
				stream.read(reinterpret_cast<char*>(&animation.header), sizeof(animation.header));

				animation.rotatedJointIndices.resize(animation.header.rotatedJointCount);
				stream.read(reinterpret_cast<char*>(animation.rotatedJointIndices.data()),
				            animation.rotatedJointIndices.size() * sizeof(animation.rotatedJointIndices[0]));

				animation.translatedJointIndices.resize(animation.header.translatedJointCount);
				stream.read(reinterpret_cast<char*>(animation.translatedJointIndices.data()),
				            animation.translatedJointIndices.size() * sizeof(animation.translatedJointIndices[0]));

				animation.keyframes.resize(animation.header.frameCount);
				for (auto& frame : animation.keyframes)
				{
					frame.eulerAngles.resize(animation.header.rotatedJointCount);
					stream.read(reinterpret_cast<char*>(frame.eulerAngles.data()),
					            frame.eulerAngles.size() * sizeof(frame.eulerAngles[0]));

					frame.translations.resize(animation.header.translatedJointCount);
					stream.read(reinterpret_cast<char*>(frame.translations.data()),
					            frame.translations.size() * sizeof(frame.translations[0]));
				}
			}
			i++;
		}
	}

	return animations;
}

void MorphFile::ReadCreatureBlock(std::istream& stream) noexcept
{
	// The creature block's version is the header's first field. Each field below came in at a version, and the eyes
	// at version 14.
	const auto version = _header.unknown0x0;
	constexpr uint32_t k_EyesVersion = 14;

	// Older morph data ends with a name
	if (_header.binaryVersion <= 5)
	{
		_creaturePrefix.resize(0x20);
		stream.read(reinterpret_cast<char*>(_creaturePrefix.data()), static_cast<std::streamsize>(_creaturePrefix.size()));
	}
	// Every number after the last field read is kept, for writing the file back
	const auto readRest = [this, &stream]() {
		_creatureRest.clear();
		for (int32_t value = 0; stream.read(reinterpret_cast<char*>(&value), sizeof(value)).good();)
		{
			_creatureRest.push_back(value);
		}
	};
	// The bones it acts with and the moments of its object animations, as many as the version has
	CreatureActionPoints points {};
	const auto read = [&stream](int32_t& value) { stream.read(reinterpret_cast<char*>(&value), sizeof(value)); };
	read(points.rightHand);
	read(points.rightFoot);
	if (version > 2)
	{
		read(points.rightArmpit);
		read(points.belly);
	}
	read(points.head);
	if (version > 11)
	{
		read(points.unknownBone);
	}
	read(points.groin);
	if (version > 4)
	{
		read(points.leashBone);
		read(points.rightEye);
	}
	read(points.pickUpTime);
	if (version > 8)
	{
		read(points.catchTimes[0]);
		read(points.catchTimes[1]);
	}
	if (version > 9)
	{
		read(points.pointTime);
	}
	read(points.destroyTime);
	read(points.discardTime);
	read(points.eatTime);
	read(points.throwTime);
	read(points.putDownTime);
	if (version > 15)
	{
		read(points.kickTime);
	}
	if (version > 7)
	{
		read(points.kissTime);
	}
	if (stream.good())
	{
		_creatureActionPoints = points;
	}
	if (version < k_EyesVersion)
	{
		readRest();
		return;
	}
	// Then up to two points on the body, each after whether it is there
	const auto readOptionalPoint = [&stream]() -> std::optional<MeshIntersect> {
		uint32_t present = 0;
		stream.read(reinterpret_cast<char*>(&present), sizeof(present));
		if (present == 0)
		{
			return std::nullopt;
		}
		MeshIntersect point {};
		stream.read(reinterpret_cast<char*>(&point), sizeof(point));
		return stream.good() ? std::optional(point) : std::nullopt;
	};
	// The first point is the point that kisses, the second the point that sick comes out of
	if (version > 7)
	{
		_creatureKissPoint = readOptionalPoint();
	}
	if (version > 17)
	{
		_creaturePukePoint = readOptionalPoint();
	}

	CreatureEyes eyes {};
	stream.read(reinterpret_cast<char*>(&eyes.scale), sizeof(eyes.scale));
	for (auto& point : eyes.points)
	{
		uint32_t enabled = 0;
		stream.read(reinterpret_cast<char*>(&point.intersect), sizeof(point.intersect));
		stream.read(reinterpret_cast<char*>(&enabled), sizeof(enabled));
		stream.read(reinterpret_cast<char*>(&point.depth), sizeof(point.depth));
		point.enabled = enabled != 0;
	}
	// Stored a column at a time
	for (size_t k = 0; k < 3; ++k)
	{
		for (auto& row : eyes.lidAngles)
		{
			stream.read(reinterpret_cast<char*>(&row.at(k)), sizeof(float));
		}
	}
	if (stream.good())
	{
		_creatureEyes = eyes;
		ReadTattooSites(stream, version);
	}
	// The block ends with each bone's mirror bone. Its last numbers are kept, the table among them, for the bones are
	// counted in the mesh rather than here.
	constexpr size_t k_MostTail = 512;
	_creatureVersion = version;
	readRest();
	const auto keep = std::min(_creatureRest.size(), k_MostTail);
	_creatureTail.assign(_creatureRest.end() - static_cast<std::ptrdiff_t>(keep), _creatureRest.end());
}

void MorphFile::ReadTattooSites(std::istream& stream, uint32_t version) noexcept
{
	// Each field below came in at a version, and the tattoo sites at version 15
	constexpr uint32_t k_TattooVersion = 15;
	if (version < k_TattooVersion)
	{
		return;
	}
	// The sound bank's name, then seven sizes, then a pair or triple of numbers for each of the twelve fight blows
	if (version > 18)
	{
		stream.read(_soundBankRaw.data(), _soundBankRaw.size());
		_soundBankName.assign(_soundBankRaw.data(), strnlen(_soundBankRaw.data(), _soundBankRaw.size()));
	}
	_creatureSizes.resize(version < 11 ? 1 : 7);
	stream.read(reinterpret_cast<char*>(_creatureSizes.data()),
	            static_cast<std::streamsize>(_creatureSizes.size() * sizeof(uint32_t)));
	CreatureBlows blows {};
	for (auto& blow : blows)
	{
		stream.read(reinterpret_cast<char*>(&blow.landTime), sizeof(blow.landTime));
		stream.read(reinterpret_cast<char*>(&blow.strikingBone), sizeof(blow.strikingBone));
		if (version > 12)
		{
			stream.read(reinterpret_cast<char*>(&blow.woundType), sizeof(blow.woundType));
		}
	}
	if (stream.good())
	{
		_creatureBlows = blows;
	}

	TattooSites sites {};
	for (size_t siteIndex = 0; siteIndex < sites.size(); ++siteIndex)
	{
		auto& site = sites[siteIndex];
		site = {};
		uint32_t enabled = 0;
		stream.read(reinterpret_cast<char*>(&enabled), sizeof(enabled));
		if (enabled == 0)
		{
			continue;
		}
		// The last byte of the packed place is left over
		std::array<uint8_t, 4> packed {};
		stream.read(reinterpret_cast<char*>(packed.data()), packed.size());
		stream.read(reinterpret_cast<char*>(&site.size), sizeof(site.size));
		site.enabled = true;
		_tattooRaw[siteIndex].packed = packed;
		site.u = packed[0];
		site.v = packed[1];
		site.skin = static_cast<uint8_t>(packed[2] >> 6u);
		if (version > 16)
		{
			uint32_t mirror = 0;
			stream.read(reinterpret_cast<char*>(&mirror), sizeof(mirror));
			stream.read(reinterpret_cast<char*>(&site.rotation), sizeof(site.rotation));
			_tattooRaw[siteIndex].mirror = mirror;
			_tattooRaw[siteIndex].rotation = site.rotation;
			site.mirror = mirror != 0;
			site.rotation &= 3u;
		}
	}
	if (stream.good())
	{
		_tattooSites = sites;
	}
}

HairGroup MorphFile::ReadHairGroup(std::istream& stream) noexcept
{
	HairGroup hairGroup;
	stream.read(reinterpret_cast<char*>(&hairGroup.header), sizeof(hairGroup.header));
	hairGroup.hairs.resize(hairGroup.header.hairCount);
	stream.read(reinterpret_cast<char*>(hairGroup.hairs.data()), hairGroup.hairs.size() * sizeof(hairGroup.hairs[0]));
	return hairGroup;
}

MorphResult MorphFile::ReadFile(std::istream& stream, const std::filesystem::path& specsDirectory) noexcept
{
	assert(!_isLoaded);

	// Total file size
	std::size_t fsize = 0;
	if (stream.seekg(0, std::ios_base::end))
	{
		fsize = static_cast<std::size_t>(stream.tellg());
		stream.seekg(0);
	}

	if (fsize < sizeof(MorphHeader))
	{
		return MorphResult::ErrFileTooSmall;
	}

	// First 236 bytes
	stream.read(reinterpret_cast<char*>(&_header), sizeof(MorphHeader));

	assert(_header.binaryVersion > 4); // structure is much different below v5

	// Parse spec file (a separate text file) using the version
	std::string specName;
	// this field is a good guess for hand or not, but a better choice might be
	// getting the segment name from pack
	if (_header.unknown0x0 != 0u)
	{
		specName = "ctrspec" + std::to_string(_header.specFileVersion) + ".txt";
	}
	else
	{
		specName = "hndspec" + std::to_string(_header.specFileVersion) + ".txt";
	}
	const auto specResult = ReadSpecFile(specsDirectory / specName);
	if (specResult != MorphResult::Success)
	{
		return specResult;
	}
	size_t numAnimations = 0;
	for (auto& animSet : _animationSpecs.animationSets)
	{
		numAnimations += animSet.animations.size();
	}

	// After the header is the anim set, a variable length array of offsets relative to the section offset
	std::vector<uint32_t> animationOffsets(numAnimations);
	stream.read(reinterpret_cast<char*>(animationOffsets.data()), animationOffsets.size() * sizeof(animationOffsets[0]));

	// Following the animation offsets are chained offsets which can lead to extra data
	uint32_t extraOffset = 0;
	stream.read(reinterpret_cast<char*>(&extraOffset), sizeof(extraOffset));

	// Read in the base animations using those offsets
	_baseAnimation = ReadAnimations(stream, animationOffsets);
	_baseAnimationIndices.resize(numAnimations, -1);
	int32_t baseIndex = 0;
	for (size_t i = 0; i < numAnimations; ++i)
	{
		if (animationOffsets[i] > 0)
		{
			_baseAnimationIndices[i] = baseIndex++;
		}
	}

	// Creature files have different animations for the morph meshes (evil, good, thin, fat) weak, strong are skipped
	for (uint32_t i = 0; i < 4; ++i)
	{
		if (std::strlen(_header.variantMeshNames.at(i).data()) > 0)
		{
			// Set file to next animation set
			stream.seekg(extraOffset);

			std::vector<uint32_t> variantAnimationOffsets(numAnimations);
			stream.read(reinterpret_cast<char*>(variantAnimationOffsets.data()),
			            variantAnimationOffsets.size() * sizeof(variantAnimationOffsets[0]));

			// Again, the get pointer to the next part
			stream.read(reinterpret_cast<char*>(&extraOffset), sizeof(extraOffset));

			_variantAnimations.at(i) = ReadAnimations(stream, variantAnimationOffsets);
			auto& indices = _variantAnimationIndices.at(i);
			indices.assign(numAnimations, -1);
			int32_t variantIndex = 0;
			for (size_t j = 0; j < numAnimations; ++j)
			{
				if (variantAnimationOffsets[j] > 0)
				{
					indices[j] = variantIndex++;
				}
			}
		}
	}

	// Once all the animation sets are loaded, the extra offset points to hair groups data (even if there are none)
	stream.seekg(extraOffset);
	stream.read(reinterpret_cast<char*>(&_hairHeader), sizeof(_hairHeader));
	for (uint32_t i = 0; i < _hairHeader.hairGroupCount; ++i)
	{
		_hairGroups.emplace_back(ReadHairGroup(stream));
	}

	// The extra data segment is in relation to the number of animations in the base animation set
	_extraData.resize(numAnimations);
	for (size_t i = 0; i < numAnimations; ++i)
	{
		if (animationOffsets[i] == 0)
		{
			continue;
		}
		uint32_t hasData; // TODO(#467): unknown if this serves another function
		while (stream.read(reinterpret_cast<char*>(&hasData), sizeof(hasData)).good() && hasData != 0u)
		{
			auto& data = _extraData[i].emplace_back();
			stream.read(reinterpret_cast<char*>(&data), sizeof(data));
		}
	}

	// Creature files go on with the creature's own block
	if (_header.unknown0x0 != 0u)
	{
		ReadCreatureBlock(stream);
	}

	_isLoaded = true;

	return MorphResult::Success;
}

MorphResult MorphFile::Open(const std::filesystem::path& filepath, const std::filesystem::path& specsDirectory) noexcept
{
	assert(!_isLoaded);

	std::ifstream stream(filepath, std::ios::binary);

	if (!stream.is_open())
	{
		return MorphResult::ErrCantOpen;
	}

	return ReadFile(stream, specsDirectory);
}

MorphResult MorphFile::Open(const std::vector<uint8_t>& buffer, const std::filesystem::path& specsDirectory) noexcept
{
	assert(!_isLoaded);

	imemstream stream(reinterpret_cast<const char*>(buffer.data()), buffer.size() * sizeof(buffer[0]));

	return ReadFile(stream, specsDirectory);
}

namespace
{
template <typename T>
void Put(std::vector<uint8_t>& out, const T& value)
{
	const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
	out.insert(out.end(), bytes, bytes + sizeof(T));
}

template <typename T>
void PutAll(std::vector<uint8_t>& out, const std::vector<T>& values)
{
	for (const auto& value : values)
	{
		Put(out, value);
	}
}

void PatchU32(std::vector<uint8_t>& out, size_t at, uint32_t value)
{
	std::memcpy(out.data() + at, &value, sizeof(value));
}

void PutAnimation(std::vector<uint8_t>& out, const Animation& animation)
{
	Put(out, animation.header);
	PutAll(out, animation.rotatedJointIndices);
	PutAll(out, animation.translatedJointIndices);
	for (const auto& frame : animation.keyframes)
	{
		PutAll(out, frame.eulerAngles);
		PutAll(out, frame.translations);
	}
}

/// A set of animations: an offset for each animation of the spec file (0 for none), the offset of the block after the
/// set, then the animations that are present, one after the other
void PutAnimationSet(std::vector<uint8_t>& out, const std::vector<Animation>& animations, const std::vector<int32_t>& indices,
                     size_t specCount)
{
	const auto offsetsAt = out.size();
	out.resize(out.size() + ((specCount + 1) * sizeof(uint32_t)), 0);
	for (size_t i = 0; i < specCount; ++i)
	{
		if (i < indices.size() && indices[i] >= 0)
		{
			PatchU32(out, offsetsAt + (i * sizeof(uint32_t)), static_cast<uint32_t>(out.size()));
			PutAnimation(out, animations[static_cast<size_t>(indices[i])]);
		}
	}
	PatchU32(out, offsetsAt + (specCount * sizeof(uint32_t)), static_cast<uint32_t>(out.size()));
}
} // namespace

void MorphFile::WriteCreatureBlock(std::vector<uint8_t>& out) const
{
	const auto version = _header.unknown0x0;
	out.insert(out.end(), _creaturePrefix.begin(), _creaturePrefix.end());
	// What the reader reads, in its order, field for field
	if (_creatureActionPoints.has_value())
	{
		const auto& points = *_creatureActionPoints;
		Put(out, points.rightHand);
		Put(out, points.rightFoot);
		if (version > 2)
		{
			Put(out, points.rightArmpit);
			Put(out, points.belly);
		}
		Put(out, points.head);
		if (version > 11)
		{
			Put(out, points.unknownBone);
		}
		Put(out, points.groin);
		if (version > 4)
		{
			Put(out, points.leashBone);
			Put(out, points.rightEye);
		}
		Put(out, points.pickUpTime);
		if (version > 8)
		{
			Put(out, points.catchTimes[0]);
			Put(out, points.catchTimes[1]);
		}
		if (version > 9)
		{
			Put(out, points.pointTime);
		}
		Put(out, points.destroyTime);
		Put(out, points.discardTime);
		Put(out, points.eatTime);
		Put(out, points.throwTime);
		Put(out, points.putDownTime);
		if (version > 15)
		{
			Put(out, points.kickTime);
		}
		if (version > 7)
		{
			Put(out, points.kissTime);
		}
	}
	constexpr uint32_t k_EyesVersion = 14;
	constexpr uint32_t k_TattooVersion = 15;
	if (version >= k_EyesVersion && _creatureActionPoints.has_value())
	{
		const auto putOptionalPoint = [&out](const std::optional<MeshIntersect>& point) {
			Put(out, static_cast<uint32_t>(point.has_value() ? 1 : 0));
			if (point.has_value())
			{
				Put(out, *point);
			}
		};
		if (version > 7)
		{
			putOptionalPoint(_creatureKissPoint);
		}
		if (version > 17)
		{
			putOptionalPoint(_creaturePukePoint);
		}
		if (_creatureEyes.has_value())
		{
			const auto& eyes = *_creatureEyes;
			Put(out, eyes.scale);
			for (const auto& point : eyes.points)
			{
				Put(out, point.intersect);
				Put(out, static_cast<uint32_t>(point.enabled ? 1 : 0));
				Put(out, point.depth);
			}
			for (size_t k = 0; k < 3; ++k)
			{
				for (const auto& row : eyes.lidAngles)
				{
					Put(out, row.at(k));
				}
			}
			if (version >= k_TattooVersion)
			{
				if (version > 18)
				{
					out.insert(out.end(), reinterpret_cast<const uint8_t*>(_soundBankRaw.data()),
					           reinterpret_cast<const uint8_t*>(_soundBankRaw.data()) + _soundBankRaw.size());
				}
				PutAll(out, _creatureSizes);
				if (_creatureBlows.has_value())
				{
					for (const auto& blow : *_creatureBlows)
					{
						Put(out, blow.landTime);
						Put(out, blow.strikingBone);
						if (version > 12)
						{
							Put(out, blow.woundType);
						}
					}
				}
				if (_tattooSites.has_value())
				{
					for (size_t i = 0; i < _tattooSites->size(); ++i)
					{
						const auto& site = (*_tattooSites)[i];
						Put(out, static_cast<uint32_t>(site.enabled ? 1 : 0));
						if (!site.enabled)
						{
							continue;
						}
						auto packed = _tattooRaw[i].packed;
						packed[0] = site.u;
						packed[1] = site.v;
						packed[2] = static_cast<uint8_t>((packed[2] & 0x3Fu) | (static_cast<uint32_t>(site.skin) << 6u));
						out.insert(out.end(), packed.begin(), packed.end());
						Put(out, site.size);
						if (version > 16)
						{
							const auto mirror = site.mirror ? std::max<uint32_t>(_tattooRaw[i].mirror, 1u) : 0u;
							Put(out, mirror);
							Put(out, (_tattooRaw[i].rotation & ~3u) | (site.rotation & 3u));
						}
					}
				}
			}
		}
	}
	PutAll(out, _creatureRest);
}

std::vector<uint8_t> MorphFile::Write() const
{
	assert(_isLoaded);

	std::vector<uint8_t> out;
	Put(out, _header);

	size_t specCount = 0;
	for (const auto& set : _animationSpecs.animationSets)
	{
		specCount += set.animations.size();
	}
	PutAnimationSet(out, _baseAnimation, _baseAnimationIndices, specCount);
	// Creature files have an animation set for each variant mesh that is named, in the order of the variants
	for (size_t i = 0; i < _variantAnimations.size(); ++i)
	{
		if (std::strlen(_header.variantMeshNames.at(i).data()) > 0)
		{
			PutAnimationSet(out, _variantAnimations.at(i), _variantAnimationIndices.at(i), specCount);
		}
	}

	Put(out, _hairHeader);
	for (const auto& group : _hairGroups)
	{
		Put(out, group.header);
		PutAll(out, group.hairs);
	}

	// What happens at moments of each base animation, each after a flag, and a flag of 0 ends the list
	for (size_t i = 0; i < specCount; ++i)
	{
		if (i >= _baseAnimationIndices.size() || _baseAnimationIndices[i] < 0)
		{
			continue;
		}
		if (i < _extraData.size())
		{
			for (const auto& data : _extraData[i])
			{
				Put(out, static_cast<uint32_t>(1));
				Put(out, data);
			}
		}
		Put(out, static_cast<uint32_t>(0));
	}

	if (_header.unknown0x0 != 0u)
	{
		WriteCreatureBlock(out);
	}
	return out;
}

MorphResult MorphFile::Write(const std::filesystem::path& filepath) const
{
	std::ofstream stream(filepath, std::ios::binary);
	if (!stream.is_open())
	{
		return MorphResult::ErrCantOpen;
	}
	const auto bytes = Write();
	stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	return stream.good() ? MorphResult::Success : MorphResult::ErrCantOpen;
}
