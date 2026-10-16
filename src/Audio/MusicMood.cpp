/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MusicMood.h"

#include <cmath>

#include <algorithm>
#include <bit>

#include <MindFile.h>

using namespace openblack;
using namespace openblack::music_mood;

namespace
{
/// The song name is read into a buffer of this many characters, the last kept for the terminating zero
constexpr size_t k_LongestSongName = 1023;
} // namespace

uint32_t music_mood::TrackId(std::string_view songName)
{
	const auto name = songName.substr(0, std::min(songName.size(), k_LongestSongName));
	uint32_t id = 0;
	uint8_t count = 1;
	for (const char c : name)
	{
		// A zero ends the name
		if (c == '\0')
		{
			break;
		}
		const auto product = static_cast<int32_t>(static_cast<int8_t>(c)) * static_cast<int32_t>(count | 0x10u);
		id = std::rotl(id, 3) ^ static_cast<uint32_t>(product);
		++count;
	}
	return id | 1u;
}

Emotion music_mood::AddFeelings(Emotion record, Emotion now)
{
	record.mood += std::clamp(now.mood, -1.0f, 1.0f);
	record.fear += std::clamp(now.fear, -1.0f, 1.0f);
	return record;
}

std::optional<Tint> music_mood::ButtonTint(std::optional<Emotion> current)
{
	if (!current.has_value())
	{
		return std::nullopt;
	}
	const auto size = std::abs(std::trunc(current->mood * -255.0f));
	return Tint {
	    .colour = current->mood >= 0.0f ? k_GoodMoodColour : k_BadMoodColour,
	    .amount = size >= 255.0f ? 255u : static_cast<uint32_t>(size),
	};
}

std::vector<openblack::creaturemind::MindDatabaseEntry> music_mood::ToMindFile(std::span<const Record> records)
{
	std::vector<creaturemind::MindDatabaseEntry> entries;
	entries.reserve(records.size());
	for (const auto& record : records)
	{
		auto& entry = entries.emplace_back();
		entry.id = record.id;
		entry.time = static_cast<uint32_t>(record.time);
		entry.count = static_cast<int32_t>(record.emotions.size());
		for (size_t i = 0; i < record.emotions.size(); ++i)
		{
			const auto& emotion = record.emotions.at(i);
			if (emotion.mood != 0.0f || emotion.fear != 0.0f)
			{
				entry.pairs.push_back({.number = static_cast<int32_t>(i + 1), .mood = emotion.mood, .fear = emotion.fear});
			}
		}
	}
	return entries;
}

std::vector<Record> music_mood::FromMindFile(std::span<const creaturemind::MindDatabaseEntry> entries)
{
	std::vector<Record> records;
	records.reserve(entries.size());
	for (const auto& entry : entries)
	{
		auto& record = records.emplace_back();
		record.id = entry.id;
		record.time = entry.time;
		record.emotions.resize(static_cast<size_t>(std::max(entry.count, 0)));
		for (const auto& pair : entry.pairs)
		{
			if (pair.number >= 1 && static_cast<size_t>(pair.number) <= record.emotions.size())
			{
				record.emotions.at(static_cast<size_t>(pair.number) - 1) = {.mood = pair.mood, .fear = pair.fear};
			}
		}
	}
	return records;
}

std::optional<Packet> Controller::Turn(float seconds, bool reset, const Listen& listen, std::optional<Emotion> feelings,
                                       int64_t now)
{
	if (reset)
	{
		_countdown = k_ResetCountdown;
	}
	_countdown -= seconds;
	if (_countdown >= 0.0f)
	{
		return std::nullopt;
	}
	const auto track = listen ? listen() : std::nullopt;
	if (!track.has_value())
	{
		_countdown = k_InactiveSeconds;
		return SetNoTrack(feelings);
	}
	_countdown = k_PlayingSeconds;
	if (*track == 0)
	{
		_countdown = k_SilentSeconds;
		return SetNoTrack(feelings);
	}
	// The same track plays on with its feelings: nothing changes
	if (*track == _id && _index == 1 && _current != nullptr && _current->emotions.size() == 1)
	{
		return std::nullopt;
	}
	// The record of the last track gets the creature's feelings now, and the new track's record becomes current
	UpdateCurrent(feelings);
	_index = 1;
	_id = *track;
	const auto found = std::ranges::find(_records, *track, &Record::id);
	Record* record = nullptr;
	if (found != _records.end())
	{
		record = &*found;
	}
	else
	{
		record = &NewRecord();
		record->id = *track;
	}
	_current = record;
	// A record in use keeps one set of feelings; a record made or reused here starts with no feelings
	_current->emotions.resize(1);
	_current->time = now;
	return MakePacket();
}

std::optional<Emotion> Controller::Current() const
{
	if (_current == nullptr)
	{
		return std::nullopt;
	}
	if (_index != 0 && _index <= _current->emotions.size())
	{
		return _current->emotions.at(_index - 1);
	}
	return _spare;
}

void Controller::Clear()
{
	_records.clear();
	_current = nullptr;
	_index = 0;
	_id = 0;
	_countdown = 0.0f;
}

Packet Controller::ReadRecords(const std::optional<std::vector<Record>>& records, bool own, std::optional<Emotion> feelings)
{
	UpdateCurrent(feelings);
	if (records.has_value() && own)
	{
		Clear();
		// Each record read goes to the front, so the records end in the reverse order of the file
		for (const auto& read : *records)
		{
			auto& record = NewRecord();
			record.id = read.id;
			record.time = read.time;
			record.emotions = read.emotions;
		}
	}
	return MakePacket();
}

std::vector<Record> Controller::WriteRecords(std::optional<Emotion> feelings)
{
	UpdateCurrent(feelings);
	return {_records.begin(), _records.end()};
}

void Controller::UpdateCurrent(std::optional<Emotion> feelings)
{
	if (_current == nullptr || !feelings.has_value())
	{
		return;
	}
	auto& emotion = CurrentEmotion();
	emotion = AddFeelings(emotion, *feelings);
}

std::optional<Packet> Controller::SetNoTrack(std::optional<Emotion> feelings)
{
	if (_current == nullptr)
	{
		return std::nullopt;
	}
	UpdateCurrent(feelings);
	_current = nullptr;
	_index = 0;
	_id = 0;
	return MakePacket();
}

Record& Controller::NewRecord()
{
	if (_records.size() >= k_MostRecords)
	{
		// The record of the track that started longest ago, the first of equals, is cleared and used again
		auto oldest = _records.begin();
		for (auto it = _records.begin(); it != _records.end(); ++it)
		{
			if (it->time < oldest->time)
			{
				oldest = it;
			}
		}
		oldest->emotions.clear();
		return *oldest;
	}
	return _records.emplace_front();
}

Packet Controller::MakePacket() const
{
	return {
	    .index = _index,
	    .count = _current != nullptr ? static_cast<uint32_t>(_current->emotions.size()) : 0u,
	    .id = _id,
	    .emotion = Current().value_or(_spare),
	};
}

Emotion& Controller::CurrentEmotion()
{
	if (_current != nullptr && _index != 0 && _index <= _current->emotions.size())
	{
		return _current->emotions.at(_index - 1);
	}
	return _spare;
}
