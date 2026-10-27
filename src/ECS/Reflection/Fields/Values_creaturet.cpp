/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Written by tools/inspector/generate_component_fields.py: run it again rather than editing this file.

#include <array>
#include <span>
#include <string_view>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureTattoo.h"
#include "Creature/CreatureTownCompassion.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creaturet(entt::meta_ctx& context);
}

namespace
{

// The names of the enumerations indexing lists, which the lists' elements are read and set by
constexpr std::array<std::string_view, 23> k_CreatureTreeAttributeNames {"Allegiance",
                                                                         "Origin",
                                                                         "Animate",
                                                                         "PlayerNumber",
                                                                         "HarderThanMe",
                                                                         "CreatureType",
                                                                         "Type",
                                                                         "Life",
                                                                         "Tribe",
                                                                         "TownReligiousBelief",
                                                                         "TownNeedsMost",
                                                                         "TownSize",
                                                                         "DominantDesire",
                                                                         "Height",
                                                                         "SpellKnowledge",
                                                                         "Carrying",
                                                                         "ForestSize",
                                                                         "VillagerJob",
                                                                         "Sex",
                                                                         "MobileObjectType",
                                                                         "AbodeType",
                                                                         "AbodeBeingBuilt",
                                                                         "OnFire"};
static_assert(static_cast<size_t>(openblack::creature_tree::Attribute::_Count) == k_CreatureTreeAttributeNames.size());

} // namespace

void openblack::ecs::reflection::RegisterFields_Values_creaturet(entt::meta_ctx& context)
{
	Reflect<openblack::creature_tattoo::Mask>(context, ValueOnly {})
	    .Field<&openblack::creature_tattoo::Mask::size>("size")
	    .Field<&openblack::creature_tattoo::Mask::levels>("levels");
	Reflect<openblack::creature_tattoo::Slot>(context, ValueOnly {})
	    .Field<&openblack::creature_tattoo::Slot::design>("design")
	    .Field<&openblack::creature_tattoo::Slot::site>("site")
	    .Field<&openblack::creature_tattoo::Slot::colour>("colour");
	Reflect<openblack::creature_town_compassion::State>(context, ValueOnly {})
	    .Field<&openblack::creature_town_compassion::State::desire>("desire")
	    .Field<&openblack::creature_town_compassion::State::index>("index")
	    .Field<&openblack::creature_town_compassion::State::remembered>("remembered")
	    .Field<&openblack::creature_town_compassion::State::lastTurn>("lastTurn")
	    .Field<&openblack::creature_town_compassion::State::timesKept>("timesKept")
	    .Field<&openblack::creature_town_compassion::State::choosesFreely>("choosesFreely");
	Reflect<openblack::creature_tree::Belief>(context, ValueOnly {})
	    .Field<&openblack::creature_tree::Belief::type>("type")
	    .Field<&openblack::creature_tree::Belief::values>("values", {k_CreatureTreeAttributeNames})
	    .Field<&openblack::creature_tree::Belief::x>("x")
	    .Field<&openblack::creature_tree::Belief::z>("z");
	Reflect<openblack::creature_tree::Episode>(context, ValueOnly {})
	    .Field<&openblack::creature_tree::Episode::belief>("belief")
	    .Field<&openblack::creature_tree::Episode::feedback>("feedback")
	    .Field<&openblack::creature_tree::Episode::lessonKind>("lessonKind")
	    .Field<&openblack::creature_tree::Episode::action>("action")
	    .Field<&openblack::creature_tree::Episode::savedDesire>("savedDesire");
	Reflect<openblack::creature_tree::Node>(context, ValueOnly {})
	    .Field<&openblack::creature_tree::Node::test>("test")
	    .Field<&openblack::creature_tree::Node::children>("children")
	    .Field<&openblack::creature_tree::Node::bucket>("bucket")
	    .Field<&openblack::creature_tree::Node::examples>("examples");
	Reflect<openblack::creature_tree::Tree>(context, ValueOnly {}).Field<&openblack::creature_tree::Tree::nodes>("nodes");
}
