/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFriendAgendas.h"

#include <memory>

#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;
using creature_face::Cue;

namespace
{
Step PlayOnce(size_t animation)
{
	return {.kind = Step::Kind::Action, .animation = animation};
}

/// Turning to face a thing, holding still a while once it does
Step FaceThing(uint32_t thing, float seconds, Cue face = Cue::None, std::optional<Gaze> gaze = std::nullopt)
{
	return {.kind = Step::Kind::Move,
	        .seconds = seconds,
	        .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = thing},
	        .face = face,
	        .gaze = gaze};
}

Step PointAt(uint32_t thing, float seconds, Cue face)
{
	return {.kind = Step::Kind::Object,
	        .order = {.kind = ObjectOrder::Kind::PointAt, .object = thing, .seconds = seconds},
	        .face = face,
	        .gaze = Gaze {.object = thing}};
}

/// An order to the partner. The partner does the step with no face of its own and looks about as it does it.
Step Order(uint32_t partner, Step ordered)
{
	ordered.face = Cue::None;
	ordered.gaze.reset();
	return {.kind = Step::Kind::Order, .object = partner, .ordered = std::make_shared<const Step>(std::move(ordered))};
}
} // namespace

std::vector<Step> creature_mind::WaveAtFriend(uint32_t self, uint32_t friendly)
{
	auto wave = PlayOnce(animations::k_FriendlyWave);
	wave.main = true;
	return {
	    {.kind = Step::Kind::RequestPartner,
	     .object = friendly,
	     .face = Cue::Playfulness,
	     .gaze = Gaze {.object = friendly},
	     .request = creature_friendship::RequestKind::Play},
	    {.kind = Step::Kind::Demobilise, .object = friendly},
	    Order(friendly, FaceThing(self, k_FriendFacesWaverSeconds)),
	    FaceThing(friendly, k_FaceFriendToWaveSeconds, Cue::Smile, Gaze {.object = friendly}),
	    Order(friendly, PlayOnce(animations::k_FriendlyWave)),
	    wave,
	    {.kind = Step::Kind::Remobilise, .object = friendly},
	};
}

std::vector<Step> creature_mind::HowlAtFriend(uint32_t friendly)
{
	return {
	    FaceThing(friendly, k_FaceToHowlSeconds, Cue::None, Gaze {.object = friendly}),
	    PlayOnce(k_Howl),
	};
}

std::vector<Step> creature_mind::PineForFriend(uint32_t friendly)
{
	auto pine = PlayOnce(animations::k_Sad);
	pine.main = true;
	return {
	    FaceThing(friendly, k_PineSeconds, Cue::Sad, Gaze {.object = friendly}),
	    PointAt(friendly, k_PineSeconds, Cue::Sad),
	    pine,
	};
}

std::vector<Step> creature_mind::LookForFriend(uint32_t friendly, float height)
{
	auto pine = PlayOnce(animations::k_Sad);
	pine.main = true;
	return {
	    PointAt(friendly, k_PineSeconds, Cue::Sad),
	    pine,
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = friendly, .maxDistance = k_LookForFriendHeights * height},
	     .gaze = Gaze {.object = friendly, .whileGoing = true}},
	};
}
