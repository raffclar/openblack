/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptAudioState.h"

#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"

namespace openblack::audio
{

void ScriptAudioState::Reset()
{
	creatureSound = 1;
	musicLine = 0;
	musicBeat = 0;
	gameSoundOff = 0;
	alignmentMusic = 1;
}

void ScriptAudioState::EndDialogue()
{
	creatureSound = 1;
	musicBeat = 0;
}

ScriptAudioState& GetScriptAudioState()
{
	return Locator::audioState::value().Get<ScriptAudioState>();
}

} // namespace openblack::audio
