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

#include "ECS/Reflection/ComponentReflection.h"
#include "MindFile.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creaturem_2(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_creaturem_2(entt::meta_ctx& context)
{
	Reflect<openblack::creaturemind::MindBelief>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindBelief::type>("type")
	    .Field<&openblack::creaturemind::MindBelief::x>("x")
	    .Field<&openblack::creaturemind::MindBelief::z>("z")
	    .Field<&openblack::creaturemind::MindBelief::attributes>("attributes");
	Reflect<openblack::creaturemind::MindDatabaseEntry>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindDatabaseEntry::id>("id")
	    .Field<&openblack::creaturemind::MindDatabaseEntry::time>("time")
	    .Field<&openblack::creaturemind::MindDatabaseEntry::count>("count")
	    .Field<&openblack::creaturemind::MindDatabaseEntry::pairs>("pairs");
	Reflect<openblack::creaturemind::MindDatabaseEntry::Pair>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindDatabaseEntry::Pair::number>("number")
	    .Field<&openblack::creaturemind::MindDatabaseEntry::Pair::mood>("mood")
	    .Field<&openblack::creaturemind::MindDatabaseEntry::Pair::fear>("fear");
	Reflect<openblack::creaturemind::MindDesire>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindDesire::activated>("activated")
	    .Field<&openblack::creaturemind::MindDesire::value>("value")
	    .Field<&openblack::creaturemind::MindDesire::max>("max")
	    .Field<&openblack::creaturemind::MindDesire::increaseSeconds>("increaseSeconds")
	    .Field<&openblack::creaturemind::MindDesire::before7>("before7")
	    .Field<&openblack::creaturemind::MindDesire::from6To9>("from6To9")
	    .Field<&openblack::creaturemind::MindDesire::sources>("sources")
	    .Field<&openblack::creaturemind::MindDesire::before15>("before15");
	Reflect<openblack::creaturemind::MindEpisode>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindEpisode::kind>("kind")
	    .Field<&openblack::creaturemind::MindEpisode::desire>("desire")
	    .Field<&openblack::creaturemind::MindEpisode::action>("action")
	    .Field<&openblack::creaturemind::MindEpisode::belief>("belief")
	    .Field<&openblack::creaturemind::MindEpisode::feedback>("feedback");
	Reflect<openblack::creaturemind::MindFileData>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindFileData::version>("version")
	    .Field<&openblack::creaturemind::MindFileData::speciesRow>("speciesRow")
	    .Field<&openblack::creaturemind::MindFileData::savedAs>("savedAs")
	    .Field<&openblack::creaturemind::MindFileData::profile>("profile")
	    .Field<&openblack::creaturemind::MindFileData::name>("name")
	    .Field<&openblack::creaturemind::MindFileData::counters>("counters")
	    .Field<&openblack::creaturemind::MindFileData::desires>("desires")
	    .Field<&openblack::creaturemind::MindFileData::from6To9>("from6To9")
	    .Field<&openblack::creaturemind::MindFileData::treeDesireCount>("treeDesireCount")
	    .Field<&openblack::creaturemind::MindFileData::trees>("trees")
	    .Field<&openblack::creaturemind::MindFileData::opinions>("opinions")
	    .Field<&openblack::creaturemind::MindFileData::desireMemories>("desireMemories")
	    .Field<&openblack::creaturemind::MindFileData::actionsSeen>("actionsSeen")
	    .Field<&openblack::creaturemind::MindFileData::miraclesSeen>("miraclesSeen")
	    .Field<&openblack::creaturemind::MindFileData::turnsLookedAt>("turnsLookedAt")
	    .Field<&openblack::creaturemind::MindFileData::attitudeToPlayer>("attitudeToPlayer")
	    .Field<&openblack::creaturemind::MindFileData::playerDesires>("playerDesires")
	    .Field<&openblack::creaturemind::MindFileData::townDesires>("townDesires")
	    .Field<&openblack::creaturemind::MindFileData::known>("known")
	    .Field<&openblack::creaturemind::MindFileData::unknown1>("unknown1")
	    .Field<&openblack::creaturemind::MindFileData::before11>("before11")
	    .Field<&openblack::creaturemind::MindFileData::alignment>("alignment")
	    .Field<&openblack::creaturemind::MindFileData::developmentTimer>("developmentTimer")
	    .Field<&openblack::creaturemind::MindFileData::developmentPhase>("developmentPhase")
	    .Field<&openblack::creaturemind::MindFileData::physique>("physique")
	    .Field<&openblack::creaturemind::MindFileData::database>("database")
	    .Field<&openblack::creaturemind::MindFileData::unknown2>("unknown2")
	    .Field<&openblack::creaturemind::MindFileData::leashWords>("leashWords")
	    .Field<&openblack::creaturemind::MindFileData::tattooHeader>("tattooHeader")
	    .Field<&openblack::creaturemind::MindFileData::tattoo>("tattoo")
	    .Field<&openblack::creaturemind::MindFileData::learningCounts>("learningCounts")
	    .Field<&openblack::creaturemind::MindFileData::drawnScale>("drawnScale")
	    .Field<&openblack::creaturemind::MindFileData::unknown4>("unknown4")
	    .Field<&openblack::creaturemind::MindFileData::unknown5>("unknown5")
	    .Field<&openblack::creaturemind::MindFileData::trailing>("trailing");
	Reflect<openblack::creaturemind::MindFileData::DesireMemory>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindFileData::DesireMemory::value>("value")
	    .Field<&openblack::creaturemind::MindFileData::DesireMemory::count>("count");
	Reflect<openblack::creaturemind::MindKnownAction>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindKnownAction::id>("id")
	    .Field<&openblack::creaturemind::MindKnownAction::before20>("before20");
	Reflect<openblack::creaturemind::MindLearningCount>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindLearningCount::a>("a")
	    .Field<&openblack::creaturemind::MindLearningCount::b>("b")
	    .Field<&openblack::creaturemind::MindLearningCount::c>("c");
	Reflect<openblack::creaturemind::MindPhysique>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindPhysique::turns>("turns")
	    .Field<&openblack::creaturemind::MindPhysique::age>("age")
	    .Field<&openblack::creaturemind::MindPhysique::strength>("strength")
	    .Field<&openblack::creaturemind::MindPhysique::fatness>("fatness")
	    .Field<&openblack::creaturemind::MindPhysique::shownFatness>("shownFatness")
	    .Field<&openblack::creaturemind::MindPhysique::before14>("before14")
	    .Field<&openblack::creaturemind::MindPhysique::energy>("energy")
	    .Field<&openblack::creaturemind::MindPhysique::scratch>("scratch")
	    .Field<&openblack::creaturemind::MindPhysique::flags>("flags")
	    .Field<&openblack::creaturemind::MindPhysique::needs>("needs")
	    .Field<&openblack::creaturemind::MindPhysique::unknown3>("unknown3")
	    .Field<&openblack::creaturemind::MindPhysique::size>("size")
	    .Field<&openblack::creaturemind::MindPhysique::blood>("blood")
	    .Field<&openblack::creaturemind::MindPhysique::wounds>("wounds");
	Reflect<openblack::creaturemind::MindSighting>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindSighting::count>("count")
	    .Field<&openblack::creaturemind::MindSighting::turn>("turn");
	Reflect<openblack::creaturemind::MindSource>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindSource::value>("value")
	    .Field<&openblack::creaturemind::MindSource::threshold>("threshold")
	    .Field<&openblack::creaturemind::MindSource::type>("type");
	Reflect<openblack::creaturemind::MindTree>(context, ValueOnly {})
	    .Field<&openblack::creaturemind::MindTree::type>("type")
	    .Field<&openblack::creaturemind::MindTree::desire>("desire")
	    .Field<&openblack::creaturemind::MindTree::desireAgain>("desireAgain")
	    .Field<&openblack::creaturemind::MindTree::episodes>("episodes");
}
