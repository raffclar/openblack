/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <MorphFile.h>
#include <glm/vec3.hpp>

/// Synthetic Creature blocks of .cbn files and Hand blocks of .hbn files, laid out as the morph parser reads them, and
/// the spec files that name their animations
namespace openblack::test::creature_block
{
/// One animation of a block
struct Clip
{
	uint32_t durationMs {1000};
	uint32_t looping {1};
	float strideRate {0.0f};
	float strideLength {0.0f};
	std::array<float, 3> displacement {0.0f, 0.0f, 0.0f};
	uint32_t meshBoneCount {2};
	std::vector<uint32_t> rotated {0};
	std::vector<uint32_t> translated {};
	/// For each frame, the rotated joints' angles, then the translated joints' moves
	std::vector<std::vector<glm::vec3>> frames {{glm::vec3(0.0f)}};
	/// The sounds and hair groups on moments of the animation
	std::vector<morph::ExtraData> events;
};

struct Block
{
	/// The header's first field: 0 for the hand's file, else the creature block's version
	uint32_t version {21};
	uint32_t specVersion {90};
	std::string baseMesh {"A_Ape_Base"};
	/// One for each animation of the spec file, in its order; none where the file has no animation there
	std::vector<std::optional<Clip>> clips;
	uint32_t soundObject {3};
	morph::CreatureActionPoints points {
	    .rightHand = 4,
	    .rightFoot = 5,
	    .rightArmpit = 6,
	    .belly = 7,
	    .head = 8,
	    .unknownBone = 9,
	    .groin = 10,
	    .leashBone = 11,
	    .unknownBone2 = 12,
	    .pickUpTime = 300,
	    .catchTimes = {310, 320},
	    .unknownTime = 330,
	    .destroyTime = 400,
	    .discardTime = 500,
	    .eatTime = 600,
	    .throwTime = 700,
	    .putDownTime = 800,
	    .unknownTimes = {900, 910},
	};
	morph::CreatureEyes eyes {};
	std::string soundBank {"ape_voice"};
	morph::TattooSites sites {};
};

class Writer
{
public:
	template <typename T>
	void Put(const T& value)
	{
		const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
		_bytes.insert(_bytes.end(), bytes, bytes + sizeof(T));
	}
	void Zeros(size_t count) { _bytes.insert(_bytes.end(), count, 0); }
	void PutAt(size_t offset, uint32_t value) { std::memcpy(_bytes.data() + offset, &value, sizeof(value)); }
	[[nodiscard]] uint32_t Size() const { return static_cast<uint32_t>(_bytes.size()); }
	[[nodiscard]] std::vector<uint8_t> Take() { return std::move(_bytes); }

private:
	std::vector<uint8_t> _bytes;
};

/// The creature block after the morph data, with the fields of the block's version only, as the parser reads them
inline void WriteCreatureBlock(Writer& w, const Block& block)
{
	const auto v = block.version;
	const auto& p = block.points;
	w.Put(p.rightHand);
	w.Put(p.rightFoot);
	if (v > 2)
	{
		w.Put(p.rightArmpit);
		w.Put(p.belly);
	}
	w.Put(p.head);
	if (v > 11)
	{
		w.Put(p.unknownBone);
	}
	w.Put(p.groin);
	if (v > 4)
	{
		w.Put(p.leashBone);
		w.Put(p.unknownBone2);
	}
	w.Put(p.pickUpTime);
	if (v > 8)
	{
		w.Put(p.catchTimes[0]);
		w.Put(p.catchTimes[1]);
	}
	if (v > 9)
	{
		w.Put(p.unknownTime);
	}
	w.Put(p.destroyTime);
	w.Put(p.discardTime);
	w.Put(p.eatTime);
	w.Put(p.throwTime);
	w.Put(p.putDownTime);
	if (v > 15)
	{
		w.Put(p.unknownTimes[0]);
	}
	if (v > 7)
	{
		w.Put(p.unknownTimes[1]);
	}
	if (v < 14)
	{
		return;
	}
	// two points on the body that are not there
	if (v > 7)
	{
		w.Put(uint32_t {0});
	}
	if (v > 17)
	{
		w.Put(uint32_t {0});
	}
	w.Put(block.eyes.scale);
	for (const auto& point : block.eyes.points)
	{
		w.Put(point.intersect);
		w.Put(static_cast<uint32_t>(point.enabled ? 1 : 0));
		w.Put(point.depth);
	}
	for (size_t k = 0; k < 3; ++k)
	{
		for (const auto& row : block.eyes.lidAngles)
		{
			w.Put(row.at(k));
		}
	}
	if (v < 15)
	{
		return;
	}
	if (v > 18)
	{
		std::array<char, 0x20> bank {};
		std::copy_n(block.soundBank.begin(), std::min(block.soundBank.size(), bank.size() - 1), bank.begin());
		w.Put(bank);
	}
	w.Zeros((v < 11 ? 1 : 7) * sizeof(uint32_t));
	w.Zeros(12 * (v > 12 ? 3 : 2) * sizeof(uint32_t));
	for (const auto& site : block.sites)
	{
		w.Put(static_cast<uint32_t>(site.enabled ? 1 : 0));
		if (!site.enabled)
		{
			continue;
		}
		w.Put(std::array<uint8_t, 4> {site.u, site.v, static_cast<uint8_t>(site.skin << 6u), 0});
		w.Put(site.size);
		if (v > 16)
		{
			w.Put(static_cast<uint32_t>(site.mirror ? 1 : 0));
			w.Put(site.rotation);
		}
	}
}

/// The block's bytes: the header, the base animations' offsets and animations, no variant animations, no hair groups,
/// the animations' moments, then for a creature its own block
inline std::vector<uint8_t> Build(const Block& block)
{
	Writer w;
	morph::MorphHeader header {};
	header.unknown0x0 = block.version;
	header.specFileVersion = block.specVersion;
	header.binaryVersion = 6;
	std::copy_n(block.baseMesh.begin(), std::min(block.baseMesh.size(), header.baseMeshName.size() - 1),
	            header.baseMeshName.begin());
	w.Put(header);
	const auto offsetsAt = w.Size();
	w.Zeros(block.clips.size() * sizeof(uint32_t));
	const auto nextAt = w.Size();
	w.Put(uint32_t {0});
	for (size_t i = 0; i < block.clips.size(); ++i)
	{
		if (!block.clips[i].has_value())
		{
			continue;
		}
		const auto& clip = *block.clips[i];
		w.PutAt(offsetsAt + static_cast<uint32_t>(i * sizeof(uint32_t)), w.Size());
		morph::AnimationHeader animation {};
		animation.duration = clip.durationMs;
		animation.looping = clip.looping;
		animation.strideRate = clip.strideRate;
		animation.strideLength = clip.strideLength;
		animation.displacement = clip.displacement;
		animation.frameCount = static_cast<uint32_t>(clip.frames.size());
		animation.meshBoneCount = clip.meshBoneCount;
		animation.rotatedJointCount = static_cast<uint32_t>(clip.rotated.size());
		animation.translatedJointCount = static_cast<uint32_t>(clip.translated.size());
		w.Put(animation);
		for (const auto joint : clip.rotated)
		{
			w.Put(joint);
		}
		for (const auto joint : clip.translated)
		{
			w.Put(joint);
		}
		for (const auto& frame : clip.frames)
		{
			for (const auto& value : frame)
			{
				w.Put(std::array<float, 3> {value.x, value.y, value.z});
			}
		}
	}
	w.PutAt(nextAt, w.Size());
	w.Put(morph::HairHeader {.soundObject = block.soundObject, .hairGroupCount = 0});
	for (const auto& clip : block.clips)
	{
		if (!clip.has_value())
		{
			continue;
		}
		for (const auto& event : clip->events)
		{
			w.Put(uint32_t {1});
			w.Put(event);
		}
		w.Put(uint32_t {0});
	}
	if (block.version != 0)
	{
		WriteCreatureBlock(w, block);
	}
	// a little left over, as the files have
	w.Zeros(sizeof(uint32_t));
	return w.Take();
}

/// A spec file in `directory`: ctrspec<version>.txt for a creature, hndspec<version>.txt for the hand. Each set is its
/// name and its animations, each animation's type letter first
inline void WriteSpec(const std::filesystem::path& directory, bool creature, uint32_t version,
                      const std::vector<std::pair<std::string, std::vector<std::string>>>& sets)
{
	std::ofstream spec(directory / ((creature ? "ctrspec" : "hndspec") + std::to_string(version) + ".txt"));
	spec << version << "\n";
	for (const auto& [name, animations] : sets)
	{
		spec << "=" << name << "\n";
		for (const auto& animation : animations)
		{
			spec << animation << "\n";
		}
	}
	spec << "E\n";
}

/// A fresh folder under the temporary directory, removed with everything in it when it goes
class TempFolder
{
public:
	explicit TempFolder(const std::string& name)
	    : _path(std::filesystem::temp_directory_path() / name)
	{
		std::error_code ec;
		std::filesystem::remove_all(_path, ec);
		std::filesystem::create_directories(_path);
	}
	~TempFolder()
	{
		std::error_code ec;
		std::filesystem::remove_all(_path, ec);
	}
	TempFolder(const TempFolder&) = delete;
	TempFolder& operator=(const TempFolder&) = delete;
	TempFolder(TempFolder&&) = delete;
	TempFolder& operator=(TempFolder&&) = delete;

	[[nodiscard]] const std::filesystem::path& Path() const { return _path; }

	void Write(const std::filesystem::path& relative, const std::vector<uint8_t>& bytes) const
	{
		const auto path = _path / relative;
		std::filesystem::create_directories(path.parent_path());
		std::ofstream(path, std::ios::binary)
		    .write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}

private:
	std::filesystem::path _path;
};

} // namespace openblack::test::creature_block
