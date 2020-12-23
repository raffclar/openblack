/*****************************************************************************
 * Copyright (c) 2018-2020 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <istream>
#include <map>
#include <memory>
#include <streambuf>
#include <string>
#include <vector>

namespace openblack::sad
{

struct SadAudioMetaDataHeader
{
	char unknown[27];
	uint32_t entries;
};

struct SadAudioBankSampleTable
{
	uint16_t numOfEntries;
	uint16_t numOfEntries2;

};

enum class AudioBankLoop : uint16_t
{
	None,
	Restart,
	Once,
	Overlap,
};


// TODO: Look for channel count (e.g 2)
// TODO: Look for word length (e.g 16);
struct AudioBankSample
{
	char audioSampleName[255];
	int32_t unknown0;
	int32_t id;
	int32_t isBank;
	uint32_t size;
	uint32_t relativeOffset;
	int32_t clone;
	int16_t group;
	int16_t atmosGroup;
	int32_t unknown4;
	int32_t unknown5;
	int16_t unknown6a;
	int16_t unknown6b;
	uint32_t sampleRate;
	int16_t unknownOthera;
	int16_t unknownOtherb;
	int16_t unknown7a;
	int16_t unknown7b;
	int32_t unknown8;
	int32_t lStart;
	int32_t lEnd;
	char sampleDescription[255];
	uint16_t priority;            // 0-9999
	uint16_t unknown9;            //
	uint16_t unknown10;           //
	uint16_t unknown11;           //
	int16_t loop;                 //
	uint16_t start;               //
	uint8_t pan;                  //
	uint16_t unknown12;           //
	float xPos;                   // -9999 to 9999
	float yPos;                   // -9999 to 9999
	float zPos;                   // -9999 to 9999
	uint8_t volume;               //
	uint16_t userParam;           //
	uint16_t pitch;               //
	uint16_t unknown18;           //
	uint16_t pitchDeviation;      //
	uint16_t unknown20;           //
	float minDist;                //
	float maxDist;                //
	float scale;                  // 0-50 (multiply by 10)
	AudioBankLoop loopType;       //
	uint16_t unknown21;           //
	uint16_t unknown22;           //
	uint16_t unknown23;           //
	uint16_t atmos;               //
};

static_assert(sizeof(AudioBankSample) == 640, "Audio sample header has incorrect size");

struct SadBlockHeader
{
	char name[31];
	uint32_t size;
};

using SoundData = std::vector<uint8_t>;
using SoundEntry = std::tuple<AudioBankSample, std::vector<uint8_t>>;
using Sounds = std::vector<SoundEntry>;

/**
  This class is used to read LionHead sad files
 */
class SadFile
{
protected:
	static constexpr const char kMagic[8] = {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};

	/// True when a file has been loaded
	bool _isLoaded;

	std::string _filename;
	std::string _name;

	std::map<std::string, std::vector<uint8_t>> _blocks;
	Sounds _sounds;
	bool _isMusic;

	/// Error handling
	void Fail(const std::string& msg);

	void ResolveFileSegmentDataBlock();
	void ResolveAudioWaveDataBlock();
public:
	SadFile();

	virtual ~SadFile() = default;

	/// Read sad file from the filesystem
	void Open(const std::string& file);

	/// Read blocks from pack
	virtual void ReadBlocks();

	[[nodiscard]] const std::string& GetFilename() const { return _filename; }
	[[nodiscard]] const std::string& GetName() const { return _name; }
	[[nodiscard]] const std::map<std::string, std::vector<uint8_t>>& GetBlocks() const { return _blocks; }
	[[nodiscard]] bool HasBlock(const std::string& name) const { return _blocks.count(name); }
	[[nodiscard]] const std::vector<uint8_t>& GetBlock(const std::string& name) const { return _blocks.at(name); }
	[[nodiscard]] const Sounds GetSounds() const { return _sounds; }
};
}