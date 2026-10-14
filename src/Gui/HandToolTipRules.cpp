/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandToolTipRules.h"

#include <algorithm>

namespace openblack::gui::hand_tooltip
{

Offer OverNothing(const CameraFacts& camera)
{
	// The tilt comes before the turn
	if ((camera.tricons & k_PitchHint) != 0 && (camera.features & k_PitchFeature) != 0)
	{
		return {.index = k_Pitch, .action = ToolTipAction::Select, .arrows = ToolTipArrows::k_UpDown};
	}
	if ((camera.tricons & k_RotateHint) != 0 && (camera.features & k_RotateFeature) != 0)
	{
		return {.index = k_Rotate, .action = ToolTipAction::Select, .arrows = ToolTipArrows::k_All};
	}
	// The zoom tooltip shows the mouse of the zoom in control and no arrows
	if (camera.centreDistance.has_value() && *camera.centreDistance < k_ZoomOutDistance && camera.pitch > k_ZoomOutPitch &&
	    (camera.features & k_ZoomFeature) != 0)
	{
		return {.index = k_ZoomOut, .action = ToolTipAction::ZoomIn, .arrows = ToolTipArrows::k_None};
	}
	return {.index = k_Move, .action = ToolTipAction::Select, .arrows = ToolTipArrows::k_All};
}

std::vector<Offer> OverTakeable(bool moving, std::optional<uint32_t> tapToolTip)
{
	if (moving)
	{
		return {{.index = k_Catch, .action = ToolTipAction::Apply}};
	}
	std::vector<Offer> offers {{.index = k_PickUp, .action = ToolTipAction::Apply}};
	if (tapToolTip.has_value())
	{
		offers.push_back({.index = *tapToolTip, .action = ToolTipAction::Apply});
	}
	return offers;
}

Offer TapOffer(TapKind kind)
{
	switch (kind)
	{
	case TapKind::Break:
		return {.index = k_TapToBreak, .action = ToolTipAction::Apply};
	case TapKind::Open:
		return {.index = k_TapToOpen, .action = ToolTipAction::Apply};
	case TapKind::Knock:
		break;
	}
	return {.index = k_Tap, .action = ToolTipAction::Apply};
}

Offer ScrollOffer(bool started)
{
	return {.index = started ? k_Reminder : k_Activate, .action = ToolTipAction::Apply};
}

std::optional<std::vector<Offer>> Holding(bool handInInfluence, HeldUse use, bool tree)
{
	if (!handInInfluence)
	{
		return std::nullopt;
	}
	switch (use)
	{
	case HeldUse::Give:
		return std::vector<Offer> {{.index = k_Give, .action = ToolTipAction::Apply}};
	case HeldUse::Supply:
		return std::vector<Offer> {{.index = k_Supply, .action = ToolTipAction::Apply}};
	case HeldUse::None:
		break;
	}
	return std::vector<Offer> {
	    {.index = tree ? k_Plant : k_Drop, .action = ToolTipAction::Apply},
	    {.index = k_Throw, .action = ToolTipAction::Apply, .arrows = ToolTipArrows::k_All},
	};
}

bool OffersBuildingToolTips(bool whole, bool ownOrNoPlayer, uint32_t features)
{
	return !whole && ownOrNoPlayer && (features & k_PitchFeature) != 0;
}

BuildingWords WoodRequired(std::optional<float> woodNeeded, double completed)
{
	if (!woodNeeded.has_value())
	{
		return {.words = k_BuildingCompleted, .number = completed};
	}
	return {.words = k_WoodRequired, .number = static_cast<double>(std::max(*woodNeeded, 0.0f))};
}

double BuildingCompleted(bool built, float builtShare, float life)
{
	return static_cast<double>(built ? life : builtShare) * 100.0;
}

} // namespace openblack::gui::hand_tooltip
