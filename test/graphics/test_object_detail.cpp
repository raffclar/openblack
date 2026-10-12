/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/GroundMark.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/NeverFades.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Vortex.h"
#include "ECS/ObjectDetail.h"
#include "ECS/Registry.h"

namespace
{
namespace object_detail = openblack::ecs::object_detail;
using namespace openblack::ecs::components;
} // namespace

TEST(ObjectDetail, ThingsOnTheLandAreBandedButNotTheCreaturesHandOrTemple)
{
	openblack::ecs::Registry registry;
	const auto rock = registry.Create();
	EXPECT_TRUE(object_detail::Banded(registry, rock));
	const auto creature = registry.Create();
	registry.Assign<CreatureMorph>(creature);
	EXPECT_FALSE(object_detail::Banded(registry, creature));
	const auto hand = registry.Create();
	registry.Assign<Hand>(hand);
	EXPECT_FALSE(object_detail::Banded(registry, hand));
	const auto temple = registry.Create();
	registry.Assign<Temple>(temple);
	EXPECT_FALSE(object_detail::Banded(registry, temple));
}

TEST(ObjectDetail, HeldThingsAreDrawnWhole)
{
	openblack::ecs::Registry registry;
	const auto rock = registry.Create();
	EXPECT_TRUE(object_detail::BandedNow(registry, rock));
	registry.Assign<InHand>(rock);
	EXPECT_FALSE(object_detail::BandedNow(registry, rock));
	const auto log = registry.Create();
	registry.Assign<HeldByCreature>(log);
	EXPECT_FALSE(object_detail::BandedNow(registry, log));
	const auto person = registry.Create();
	registry.Assign<HighDetail>(person);
	EXPECT_FALSE(object_detail::BandedNow(registry, person));
	// Rubble is banded until it fades by its own alpha
	const auto rubble = registry.Create();
	registry.Assign<GroundMark>(rubble);
	EXPECT_TRUE(object_detail::BandedNow(registry, rubble));
	registry.Get<GroundMark>(rubble).alpha = 200;
	EXPECT_FALSE(object_detail::BandedNow(registry, rubble));
}

TEST(ObjectDetail, MostThingsFadeButSomeAreKeptInView)
{
	openblack::ecs::Registry registry;
	const auto rock = registry.Create();
	EXPECT_TRUE(object_detail::Disappears(registry, rock, false));
	// A village centre
	EXPECT_FALSE(object_detail::Disappears(registry, rock, true));
	// A script keeps it in view, or it flies in the physics
	registry.Assign<NeverFades>(rock);
	EXPECT_FALSE(object_detail::Disappears(registry, rock, false));
	const auto thrown = registry.Create();
	registry.Assign<InPhysics>(thrown);
	EXPECT_FALSE(object_detail::Disappears(registry, thrown, false));
	const auto ball = registry.Create();
	registry.Assign<Ball>(ball);
	EXPECT_FALSE(object_detail::Disappears(registry, ball, false));
	const auto vortex = registry.Create();
	registry.Assign<Vortex>(vortex);
	EXPECT_FALSE(object_detail::Disappears(registry, vortex, false));
	const auto scroll = registry.Create();
	registry.Assign<ScriptHighlight>(scroll);
	EXPECT_FALSE(object_detail::Disappears(registry, scroll, false));
	// The physical shield's dome, not the spiritual shield's marker
	const auto dome = registry.Create();
	registry.Assign<MagicShield>(dome).kind = MagicShield::Kind::Physical;
	EXPECT_FALSE(object_detail::Disappears(registry, dome, false));
	const auto marker = registry.Create();
	registry.Assign<MagicShield>(marker).kind = MagicShield::Kind::Spiritual;
	EXPECT_TRUE(object_detail::Disappears(registry, marker, false));
	// Birds, but not the animals on the ground
	const auto crow = registry.Create();
	registry.Assign<Animal>(crow).type = openblack::AnimalInfo::Crow;
	EXPECT_FALSE(object_detail::Disappears(registry, crow, false));
	const auto cow = registry.Create();
	registry.Assign<Animal>(cow).type = openblack::AnimalInfo::Cow;
	EXPECT_TRUE(object_detail::Disappears(registry, cow, false));
}
