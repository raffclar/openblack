/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SoundTags.h"

#include <cstdlib>

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Audio/Device/Sound.h"
#include "Audio/Services/Guidance.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// A sound tag: a sample replayed on a thing or a point
struct Tag
{
	tags::TagId id {tags::k_NoTag};
	/// Its thing; entt::null = none (a point tag, or a tag of a dead object)
	entt::entity thing {entt::null};
	/// (openblack) a tag of a ScriptMarker (the older sound_tags names): a thing that is always there, at `position`
	bool marker {false};
	/// The point (the thing's when it was made)
	glm::vec3 position {0.0f};
	/// Added by the channel
	glm::vec3 offset {0.0f};
	int sample {0};
	/// The sample's bank: every ported Create looks it up among the effect banks; only the ambient point tag (the
	/// weather's thunder, not ported: Audio.h) uses the ambient banks
	SfxBank bank {SfxBank::None};
	bool track {false}; ///< Only with a thing
	int mode {3};
	int loops {0};
	bool extra3DFlag {false};
	bool is3D {false};
	int delay {0}; ///< Only when is3D
	bool active {true};
	uint16_t turns {0}; ///< ProcessSoundTags calls since it was made
	bool gone {false};  ///< (openblack) deleted during a pass of the list

	[[nodiscard]] Owner ChannelOwner() const { return Owner::Tag(id); }
	[[nodiscard]] bool HasThing() const { return thing != entt::null || marker; }
};

/// What this module keeps between calls (Locator::audioState)
struct SoundTagsState
{
	/// The tags in creation order (the original's head is the newest: ProcessSoundTags walks it from the back)
	std::vector<Tag> tags {};
	tags::TagId nextId {1};
};

SoundTagsState& SoundTagsData()
{
	return openblack::Locator::audioState::value().Get<SoundTagsState>();
}

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_SOUND_TAG_TRACE") != nullptr;
	return k_Trace;
}

Tag* Find(tags::TagId id)
{
	for (auto& tag : SoundTagsData().tags)
	{
		if (tag.id == id && !tag.gone)
		{
			return &tag;
		}
	}
	return nullptr;
}

/// The sound position of the tag's thing: a ScriptMarker's point, else the thing's (GameQueries); nullopt = gone
std::optional<glm::vec3> ThingPosition(const Tag& tag)
{
	if (tag.marker)
	{
		return tag.position;
	}
	if (tag.thing == entt::null)
	{
		return std::nullopt;
	}
	return OwnerSoundPosition(Owner::Thing(tag.thing));
}

/// The tag deleted: it leaves the list; its sample, if any, plays on
void Destroy(Tag& tag)
{
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: deleted (sample {})", tag.id, tag.sample);
	}
	tag.gone = true;
}

/// The thing of the tag is gone: true when the tag lives on (its loop released)
bool ForDeadObject(Tag& tag)
{
	tag.thing = entt::null;
	tag.marker = false;
	const auto bank = Bank(tag.bank);
	const auto sound = SampleId(bank, tag.sample);
	// a looping sample that still plays
	if (sample_play::Loops(sound, tag.ChannelOwner()) != 0 && IsPlaying(tag.ChannelOwner(), tag.sample, bank))
	{
		ReleaseLoop(tag.ChannelOwner(), tag.sample, bank);
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: thing gone, loop of {} released", tag.id, tag.sample);
		}
		return true;
	}
	return false;
}

void ToBeDeleted(Tag& tag)
{
	if (!ForDeadObject(tag))
	{
		Destroy(tag);
	}
}

/// The time since it was made (turns x game_clock::MsPerTurn() ms, 100 by default), at 347 m a second (the speed of
/// sound), against the camera's distance
void CheckDelay(Tag& tag)
{
	const auto camera = ListenerPoint();
	if (!camera)
	{
		return;
	}
	constexpr float k_SoundSpeed = 347.0f; // metres a second
	// turns x ms per turn, in seconds
	const float seconds = static_cast<float>(tag.turns) * static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
	const auto d = tag.position - *camera;
	const float distanceSq = glm::dot(d, d);
	const auto bank = Bank(tag.bank);
	const float maxDistance = MaxDistance({bank, tag.sample});
	const float reach = k_SoundSpeed * seconds;
	// the sound has not reached the camera yet
	if (reach * reach < distanceSq)
	{
		return;
	}
	// within the sample's max distance and active: played at the tag's point
	if (distanceSq < maxDistance * maxDistance && tag.active)
	{
		PlaySoundEffectAt(tag.ChannelOwner(), tag.position, tag.sample, tag.mode, tag.loops, tag.extra3DFlag, tag.is3D, bank);
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: delayed {} heard after {} turns", tag.id, tag.sample,
			                   tag.turns);
		}
	}
	// the delay is over, heard or not
	tag.delay = 0;
}

/// One tag of ProcessSoundTags
void Process(Tag& tag)
{
	++tag.turns;
	if (tag.HasThing())
	{
		// a thing that is no longer functional, or has no sound position, deletes the tag (openblack: the thing's
		// position is gone, inferred to be both)
		const auto at = ThingPosition(tag);
		if (!at)
		{
			ToBeDeleted(tag);
			return;
		}
		if (!tag.active)
		{
			return;
		}
		// replayed at the thing every turn
		const auto channel = PlaySoundEffectAt(tag.ChannelOwner(), *at, tag.offset, tag.sample, tag.track, tag.mode, tag.loops,
		                                       tag.extra3DFlag, tag.is3D, Bank(tag.bank));
		if (Trace() && channel != k_NoChannel && tag.turns % 50 == 1)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Sound tag {}: {} on channel {:#x} at ({:.1f}, {:.1f}, {:.1f})", tag.id,
			                   tag.sample, channel, at->x, at->y, at->z);
		}
		return;
	}
	// no thing: the delay, else gone once its sample stops
	if (tag.delay != 0)
	{
		CheckDelay(tag);
		return;
	}
	if (!IsPlaying(tag.ChannelOwner(), tag.sample, Bank(tag.bank)))
	{
		Destroy(tag);
	}
}

void Compact()
{
	std::erase_if(SoundTagsData().tags, [](const Tag& tag) { return tag.gone; });
}

tags::TagId Add(Tag tag)
{
	auto& state = SoundTagsData();
	tag.id = state.nextId++;
	if (state.nextId == tags::k_NoTag)
	{
		state.nextId = 1;
	}
	// the delay only for a 3D one, the track only with a thing; active, no turns yet
	if (!tag.is3D)
	{
		tag.delay = 0;
	}
	if (!tag.HasThing())
	{
		tag.track = false;
	}
	state.tags.push_back(tag);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"),
		                   "Sound tag {}: made, sample {} bank {} mode {} loops {} {} {}, at ({:.1f}, {:.1f}, {:.1f})", tag.id,
		                   tag.sample, static_cast<int>(tag.bank), tag.mode, tag.loops, tag.is3D ? "3D" : "2D",
		                   tag.HasThing() ? "on a thing" : "on a point", tag.position.x, tag.position.y, tag.position.z);
	}
	return tag.id;
}
} // namespace

// ---- Create -------------------------------------------------------------------------------------------------------

tags::TagId tags::Create(entt::entity thing, int sample, bool track, int mode, int loops, bool extra3DFlag, bool is3D,
                         SfxBank bank, int delay)
{
	// the offset is (0, 0, 0)
	return Create(thing, glm::vec3(0.0f), sample, track, mode, loops, extra3DFlag, is3D, bank, delay);
}

tags::TagId tags::Create(entt::entity thing, glm::vec3 offset, int sample, bool track, int mode, int loops, bool extra3DFlag,
                         bool is3D, SfxBank bank, int delay)
{
	// the point is the thing's (x, ground + y, z)
	Tag tag {
	    .thing = thing,
	    .position = OwnerSoundPosition(Owner::Thing(thing)).value_or(glm::vec3(0.0f)),
	    .offset = offset,
	    .sample = sample,
	    .bank = bank,
	    .track = track,
	    .mode = mode,
	    .loops = loops,
	    .extra3DFlag = extra3DFlag,
	    .is3D = is3D,
	    .delay = delay,
	};
	return Add(tag);
}

tags::TagId tags::Create(glm::vec3 point, int sample, bool track, int mode, int loops, bool extra3DFlag, bool is3D,
                         SfxBank bank, int delay)
{
	// no thing, no offset
	Tag tag {
	    .position = point,
	    .sample = sample,
	    .bank = bank,
	    .track = track,
	    .mode = mode,
	    .loops = loops,
	    .extra3DFlag = extra3DFlag,
	    .is3D = is3D,
	    .delay = delay,
	};
	const auto id = Add(tag);
	// played at once unless 3D with a delay
	if (is3D && delay != 0)
	{
		return id;
	}
	// at the point, owned by the tag, without tracking
	PlayOptions options;
	options.sample = {Bank(bank), sample};
	options.position = point;
	options.loops = loops;
	options.owner = Owner::Tag(id);
	options.is3D = is3D;
	options.track = false;
	options.mode = mode;
	PlaySoundEffect(options);
	return id;
}

tags::TagId tags::CreateAtMapCoords(const map_coords::MapCoords& coords, int sample, bool track, int mode, int loops,
                                    bool extra3DFlag, bool is3D, SfxBank bank, int delay)
{
	// (x, ground + altitude, z), x and z in metres (map_coords::ToMetres)
	return CreateAtMapCoords(map_coords::ToMetres(coords.x), map_coords::ToMetres(coords.z), coords.altitude, sample, track,
	                         mode, loops, extra3DFlag, is3D, bank, delay);
}

tags::TagId tags::CreateAtMapCoords(float x, float z, float heightAboveLand, int sample, bool track, int mode, int loops,
                                    bool extra3DFlag, bool is3D, SfxBank bank, int delay)
{
	// On map coordinates already in metres (x, z = ToMetres of its 16.16 values: magic::ToMap): not quantised again
	// (a second round trip may lose a unit). The point is (x, ground + altitude, z)
	return Create(glm::vec3(x, IslandAltitude(x, z) + heightAboveLand, z), sample, track, mode, loops, extra3DFlag, is3D, bank,
	              delay);
}

// ---- the others ---------------------------------------------------------------------------------------------------

void tags::SetActive(TagId id, bool active)
{
	auto* tag = Find(id);
	if (tag == nullptr)
	{
		return;
	}
	// an active tag turned off stops its sample
	if (tag->active && !active)
	{
		StopSoundEffect(tag->sample, tag->ChannelOwner(), tag->bank);
	}
	tag->active = active;
}

void tags::Remove(entt::entity thing, int sample, SfxBank bank)
{
	// every tag of that thing, sample and bank is deleted
	for (auto& tag : SoundTagsData().tags)
	{
		if (!tag.gone && tag.thing == thing && tag.sample == sample && tag.bank == bank)
		{
			ToBeDeleted(tag);
		}
	}
	Compact();
}

void tags::Remove(entt::entity thing, int sample, SfxBank bank, bool stop)
{
	// the same, stopping the sample first when stop
	for (auto& tag : SoundTagsData().tags)
	{
		if (!tag.gone && tag.thing == thing && tag.sample == sample && tag.bank == bank)
		{
			if (stop)
			{
				StopSoundEffect(tag.sample, tag.ChannelOwner(), tag.bank);
			}
			ToBeDeleted(tag);
		}
	}
	Compact();
}

void tags::Delete(TagId id)
{
	if (auto* tag = Find(id); tag != nullptr)
	{
		ToBeDeleted(*tag);
		Compact();
	}
}

int tags::RandomSample(int first, int count)
{
	// first + a local random number below count (0 for 0): the one local random of src/Audio, guidance::LocalRand
	// (game_random's local stream)
	if (count <= 0)
	{
		return first;
	}
	return first + static_cast<int>(guidance::LocalRand(static_cast<uint32_t>(count)));
}

bool tags::Exists(TagId id)
{
	return Find(id) != nullptr;
}

void tags::ProcessSoundTags()
{
	auto& state = SoundTagsData();
	// from the head (the newest); a tag may delete itself, the next one is read before
	for (size_t i = state.tags.size(); i-- > 0;)
	{
		if (!state.tags[i].gone)
		{
			Process(state.tags[i]);
		}
	}
	Compact();
}

void tags::Clear()
{
	auto& state = SoundTagsData();
	for (auto& tag : state.tags)
	{
		if (!tag.gone)
		{
			StopSoundEffect(tag.sample, tag.ChannelOwner(), tag.bank);
		}
	}
	state.tags.clear();
}

std::optional<glm::vec3> tags::TagSoundPoint(TagId id)
{
	const auto* tag = Find(id);
	if (tag == nullptr)
	{
		return std::nullopt;
	}
	// the thing's sound position; no thing: none
	return ThingPosition(*tag);
}

std::optional<glm::vec3> tags::Point(TagId id)
{
	const auto* tag = Find(id);
	return tag != nullptr ? std::optional<glm::vec3>(tag->position) : std::nullopt;
}

// ---- the sound_tags names -----------------------------------------------------------------------------------------

sound_tags::TagId sound_tags::Create(const TagDesc& desc)
{
	// as the waterfall's tag (marker, sample 12 G_WaterFlow, mode 2, for ever, 3D, InGame); the sound id is
	// an InGame.sad one
	const auto* sound = sample_play::GetSound(desc.sample);
	const int sample = sound != nullptr ? sound->id : 0;
	const int loops = desc.loop ? -1 : 0;
	TagId id = k_NoTag;
	if (desc.thing != entt::null)
	{
		id = tags::Create(desc.thing, desc.point, sample, false, 2, loops, false, true, SfxBank::InGame, 0);
	}
	else
	{
		Tag tag {
		    .marker = true,
		    .position = desc.point,
		    .sample = sample,
		    .bank = SfxBank::InGame,
		    .mode = 2,
		    .loops = loops,
		    .is3D = true,
		};
		id = Add(tag);
	}
	if (!desc.active)
	{
		tags::SetActive(id, false);
	}
	return id;
}

void sound_tags::SetActive(TagId tag, bool active)
{
	tags::SetActive(tag, active);
}

void sound_tags::Delete(TagId tag)
{
	tags::Delete(tag);
}
