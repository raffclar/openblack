/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CameraHelpSystem final: public CameraHelpSystemInterface
{
public:
	[[nodiscard]] const camera_help::CameraHelp& Get() const final { return _help; }
	[[nodiscard]] camera_help::CameraHelp& Get() final { return _help; }

	/// A save keeps what the camera lets the player do, its automatic pitch and the interface level that a script set
	void SaveState(reflection::BinaryWriter& out) const final
	{
		reflection::Save(out, _help.features);
		reflection::Save(out, _help.autoPitch);
		reflection::Save(out, _help.autoPitchHeight);
		reflection::Save(out, _help.interfaceLevel);
	}

	/// As the game loads, it sets the saved interface level again, or the normal level when no script set one. This
	/// gives the keys and the reach of the hand. Then the saved features and the saved pitch replace those of the level.
	bool LoadState(reflection::BinaryReader& in) final
	{
		auto features = _help.features;
		auto autoPitch = _help.autoPitch;
		auto autoPitchHeight = _help.autoPitchHeight;
		std::optional<int32_t> level;
		reflection::Load(in, features);
		reflection::Load(in, autoPitch);
		reflection::Load(in, autoPitchHeight);
		// A save from an earlier version has no level
		if (in.Remaining() != 0)
		{
			reflection::Load(in, level);
		}
		if (in.Failed())
		{
			return false;
		}
		_help.SetInterfaceLevel(level.value_or(0));
		_help.interfaceLevel = level;
		_help.features = features;
		_help.autoPitch = autoPitch;
		_help.autoPitchHeight = autoPitchHeight;
		return true;
	}

private:
	camera_help::CameraHelp _help;
};

} // namespace openblack::ecs::systems
