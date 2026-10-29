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

#include "3D/MapCoords.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "Enums.h"
#include "Magic/DispenserRules.h"
#include "Magic/FlockMiracleRules.h"
#include "Magic/MiracleVisuals.h"
#include "Magic/ResourcePiles.h"
#include "Magic/ShieldRules.h"
#include "Magic/SpellChants.h"
#include "Magic/TownBelief.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_m(entt::meta_ctx& context);
}

namespace
{

// The names of the enumerations indexing lists, which the lists' elements are read and set by
constexpr std::array<std::string_view, 8> k_PlayerNamesNames {"PLAYER_ONE",  "PLAYER_TWO", "PLAYER_THREE", "PLAYER_FOUR",
                                                              "PLAYER_FIVE", "PLAYER_SIX", "PLAYER_SEVEN", "NEUTRAL"};
static_assert(static_cast<size_t>(openblack::PlayerNames::_COUNT) == k_PlayerNamesNames.size());

} // namespace

void openblack::ecs::reflection::RegisterFields_Values_m(entt::meta_ctx& context)
{
	Reflect<openblack::magic::DispenserTimer>(context, ValueOnly {})
	    .Field<&openblack::magic::DispenserTimer::tick>("tick")
	    .Field<&openblack::magic::DispenserTimer::period>("period")
	    .Field<&openblack::magic::DispenserTimer::active>("active");
	Reflect<openblack::magic::SpellChants>(context, ValueOnly {})
	    .Field<&openblack::magic::SpellChants::chants>("chants")
	    .Field<&openblack::magic::SpellChants::initialChants>("initialChants")
	    .Field<&openblack::magic::SpellChants::strengthMultiplier>("strengthMultiplier")
	    .Field<&openblack::magic::SpellChants::free>("free");
	Reflect<openblack::magic::flock::Corridor>(context, ValueOnly {})
	    .Field<&openblack::magic::flock::Corridor::normal>("normal")
	    .Field<&openblack::magic::flock::Corridor::offset>("offset")
	    .Field<&openblack::magic::flock::Corridor::halfWidth>("halfWidth")
	    .Field<&openblack::magic::flock::Corridor::along>("along");
	Reflect<openblack::magic::piles::Rise>(context, ValueOnly {})
	    .Field<&openblack::magic::piles::Rise::start>("start")
	    .Field<&openblack::magic::piles::Rise::speed>("speed")
	    .Field<&openblack::magic::piles::Rise::acceleration>("acceleration")
	    .Field<&openblack::magic::piles::Rise::jerk>("jerk")
	    .Field<&openblack::magic::piles::Rise::snap>("snap")
	    .Field<&openblack::magic::piles::Rise::target>("target")
	    .Field<&openblack::magic::piles::Rise::time>("time")
	    .Field<&openblack::magic::piles::Rise::duration>("duration")
	    .Field<&openblack::magic::piles::Rise::offset>("offset")
	    .Field<&openblack::magic::piles::Rise::currentSpeed>("currentSpeed");
	Reflect<openblack::magic::shield::DomePose>(context, ValueOnly {})
	    .Field<&openblack::magic::shield::DomePose::scale>("scale")
	    .Field<&openblack::magic::shield::DomePose::angle>("angle")
	    .Field<&openblack::magic::shield::DomePose::height>("height")
	    .Field<&openblack::magic::shield::DomePose::drawn>("drawn")
	    .Field<&openblack::magic::shield::DomePose::gone>("gone");
	Reflect<openblack::magic::shield::DomeShape>(context, ValueOnly {})
	    .Field<&openblack::magic::shield::DomeShape::finalScale>("finalScale")
	    .Field<&openblack::magic::shield::DomeShape::startScale>("startScale")
	    .Field<&openblack::magic::shield::DomeShape::startSpin>("startSpin")
	    .Field<&openblack::magic::shield::DomeShape::endSpin>("endSpin")
	    .Field<&openblack::magic::shield::DomeShape::raiseWithScale>("raiseWithScale")
	    .Field<&openblack::magic::shield::DomeShape::shieldHeight>("shieldHeight")
	    .Field<&openblack::magic::shield::DomeShape::bobMagnitude>("bobMagnitude");
	Reflect<openblack::magic::shield::DomeState>(context, ValueOnly {})
	    .Field<&openblack::magic::shield::DomeState::angle>("angle")
	    .Field<&openblack::magic::shield::DomeState::bob>("bob")
	    .Field<&openblack::magic::shield::DomeState::dieTime>("dieTime")
	    .Field<&openblack::magic::shield::DomeState::dying>("dying");
	Reflect<openblack::magic::town_belief::Belief>(context, ValueOnly {})
	    .Field<&openblack::magic::town_belief::Belief::belief>("belief", {k_PlayerNamesNames})
	    .Field<&openblack::magic::town_belief::Belief::pending>("pending", {k_PlayerNamesNames})
	    .Field<&openblack::magic::town_belief::Belief::recent>("recent", {k_PlayerNamesNames})
	    .Field<&openblack::magic::town_belief::Belief::gainedSinceShown>("gainedSinceShown", {k_PlayerNamesNames})
	    .Field<&openblack::magic::town_belief::Belief::cap>("cap", {k_PlayerNamesNames})
	    .Field<&openblack::magic::town_belief::Belief::scale>("scale")
	    .Field<&openblack::magic::town_belief::Belief::neutral>("neutral")
	    .Field<&openblack::magic::town_belief::Belief::desireLevel>("desireLevel");
	Reflect<openblack::magic::visuals::HandBand>(context, ValueOnly {})
	    .Field<&openblack::magic::visuals::HandBand::kind>("kind")
	    .Field<&openblack::magic::visuals::HandBand::index>("index")
	    .Field<&openblack::magic::visuals::HandBand::delay>("delay")
	    .Field<&openblack::magic::visuals::HandBand::age>("age")
	    .Field<&openblack::magic::visuals::HandBand::duration>("duration")
	    .Field<&openblack::magic::visuals::HandBand::alphaFrom>("alphaFrom")
	    .Field<&openblack::magic::visuals::HandBand::alphaTo>("alphaTo")
	    .Field<&openblack::magic::visuals::HandBand::spin>("spin")
	    .Field<&openblack::magic::visuals::HandBand::done>("done");
	Reflect<openblack::magic::visuals::HandBands>(context, ValueOnly {})
	    .Field<&openblack::magic::visuals::HandBands::bracelets>("bracelets")
	    .Field<&openblack::magic::visuals::HandBands::flying>("flying");
	Reflect<openblack::map_coords::MapCoords>(context, ValueOnly {})
	    .Field<&openblack::map_coords::MapCoords::x>("x")
	    .Field<&openblack::map_coords::MapCoords::z>("z")
	    .Field<&openblack::map_coords::MapCoords::altitude>("altitude");
}
