/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// Any value that the archive can keep, for the files that write more than the values of the rules. The rules' files do
// not include this, so that they stay small.

#include "ECS/Reflection/BinaryArchive.h"
#include "ParticleArchive.h"

template <typename Type>
void openblack::particles::EffectWriter::WriteValue(const Type& value)
{
	ecs::reflection::Save(*_out, value);
}

template <typename Type>
void openblack::particles::EffectReader::ReadValue(Type& value)
{
	ecs::reflection::Load(*_in, value);
}
