/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleChallengeRoom.h"

#include <algorithm>
#include <span>

#include <bgfx/bgfx.h>
#include <fmt/format.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/ChallengePicture.h"
#include "ECS/Components/ChallengeRecord.h"
#include "ECS/Registry.h"
#include "Graphics/GraphicsHandleBgfx.h"

using namespace openblack;
namespace rules = openblack::temple::challenge_room;

namespace
{
/// The picture's sizes in high and low detail
constexpr uint16_t k_FullWidth = 256;
constexpr uint16_t k_FullHeight = 128;
constexpr uint16_t k_SmallWidth = 128;
constexpr uint16_t k_SmallHeight = 64;

/// Writes a picture, RGB with its top row first, to its texture, made the first time
void Upload(std::optional<graphics::TextureHandle>& texture, std::span<const uint8_t> rgb, uint16_t width, uint16_t height,
            std::string_view name)
{
	const size_t texels = static_cast<size_t>(width) * height;
	if (rgb.size() < texels * 3)
	{
		return;
	}
	if (!texture.has_value())
	{
		texture = graphics::fromBgfx(bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8,
		                                                   BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP));
		bgfx::setName(graphics::toBgfx(*texture), name.data(), static_cast<int32_t>(name.size()));
	}
	const auto* memory = bgfx::alloc(static_cast<uint32_t>(texels * 4));
	for (size_t i = 0; i < texels; ++i)
	{
		memory->data[(i * 4) + 0] = rgb[(i * 3) + 0];
		memory->data[(i * 4) + 1] = rgb[(i * 3) + 1];
		memory->data[(i * 4) + 2] = rgb[(i * 3) + 2];
		memory->data[(i * 4) + 3] = 0xFF;
	}
	bgfx::updateTexture2D(graphics::toBgfx(*texture), 0, 0, 0, 0, width, height, memory);
}
} // namespace

TempleChallengeRoom::~TempleChallengeRoom()
{
	for (auto& place : _places)
	{
		Destroy(place);
	}
}

void TempleChallengeRoom::Destroy(Place& place)
{
	for (auto* texture : {&place.small, &place.full})
	{
		if (texture->has_value())
		{
			bgfx::destroy(graphics::toBgfx(**texture));
			texture->reset();
		}
	}
	place.smallTaken.reset();
	place.fullTaken.reset();
}

void TempleChallengeRoom::Find(const graphics::L3DMesh& mesh)
{
	_parts.clear();
	const auto& subMeshes = mesh.GetSubMeshes();
	for (uint32_t i = 0; i < subMeshes.size(); ++i)
	{
		if (const auto control = rules::ParseControl(subMeshes[i]->GetName()); control.has_value())
		{
			const auto& frame = subMeshes[i]->GetFrame();
			_parts.push_back({.subMesh = i, .control = *control, .frame = frame.toMesh, .min = frame.min, .max = frame.max});
		}
	}
}

void TempleChallengeRoom::Enter()
{
	_highDetail.reset();
	_pictureLookedAt = false;
	for (auto& place : _places)
	{
		if (place.full.has_value())
		{
			bgfx::destroy(graphics::toBgfx(*place.full));
			place.full.reset();
			place.fullTaken.reset();
		}
	}
}

void TempleChallengeRoom::Update(const ecs::Registry& registry)
{
	const auto records = ecs::challenge_records::InOrder(registry);
	// Places past the records' count go, with their textures
	while (_places.size() > records.size())
	{
		Destroy(_places.back());
		_places.pop_back();
	}
	for (size_t i = 0; i < records.size(); ++i)
	{
		const auto& record = registry.Get<ecs::components::ChallengeRecord>(records[i]);
		if (i == _places.size())
		{
			_places.push_back({.record = records[i], .shown = {}});
		}
		auto& place = _places[i];
		if (place.record != records[i])
		{
			Destroy(place);
			place.record = records[i];
		}
		place.shown = {.done = ecs::challenge_records::IsDone(record), .quest = record.quest};
		const auto* picture = registry.TryGet<ecs::components::ChallengePicture>(records[i]);
		if (picture == nullptr || picture->taken == 0)
		{
			continue;
		}
		// Each picture is written to its texture again once the record has a new one
		if (place.smallTaken != picture->taken)
		{
			Upload(place.small, picture->small, k_SmallWidth, k_SmallHeight, fmt::format("Challenge {} low detail", i));
			place.smallTaken = picture->taken;
		}
		const bool highDetail = _highDetail == i;
		if (highDetail && place.fullTaken != picture->taken)
		{
			Upload(place.full, picture->full, k_FullWidth, k_FullHeight, fmt::format("Challenge {} high detail", i));
			place.fullTaken = picture->taken;
		}
		else if (!highDetail && place.full.has_value())
		{
			// Only the picture looked at closer is kept in high detail
			bgfx::destroy(graphics::toBgfx(*place.full));
			place.full.reset();
			place.fullTaken.reset();
		}
	}
}

std::vector<rules::Challenge> TempleChallengeRoom::Challenges() const
{
	std::vector<rules::Challenge> challenges;
	challenges.reserve(_places.size());
	std::ranges::transform(_places, std::back_inserter(challenges), &Place::shown);
	return challenges;
}

bool TempleChallengeRoom::IsShown(const Part& part) const
{
	const auto challenges = Challenges();
	return rules::IsShown(part.control, challenges);
}

const TempleChallengeRoom::Part* TempleChallengeRoom::FindPart(uint32_t subMesh) const
{
	const auto found = std::ranges::find(_parts, subMesh, &Part::subMesh);
	return found != _parts.end() ? &*found : nullptr;
}

std::vector<TempleSubMeshTexture> TempleChallengeRoom::GetTextures() const
{
	std::vector<TempleSubMeshTexture> textures;
	for (const auto& part : _parts)
	{
		if (part.control.part != rules::Part::Picture || part.control.place >= _places.size())
		{
			continue;
		}
		// The picture looked at closer is drawn in high detail, the others in low detail
		const auto& place = _places[part.control.place];
		const auto& texture = place.full.has_value() ? place.full : place.small;
		if (texture.has_value())
		{
			textures.push_back({.subMesh = part.subMesh, .texture = *texture});
		}
	}
	return textures;
}

std::vector<uint32_t> TempleChallengeRoom::GetHidden() const
{
	const auto challenges = Challenges();
	std::vector<uint32_t> hidden;
	for (const auto& part : _parts)
	{
		if (!rules::IsShown(part.control, challenges))
		{
			hidden.push_back(part.subMesh);
		}
	}
	return hidden;
}

bool TempleChallengeRoom::IsControl(uint32_t subMesh) const
{
	const auto* part = FindPart(subMesh);
	return part != nullptr && IsShown(*part);
}

std::vector<TempleChallengeControl> TempleChallengeRoom::GetToolTipControls() const
{
	std::vector<TempleChallengeControl> controls;
	for (const auto& part : _parts)
	{
		const bool button = part.control.part == rules::Part::ReplayButton;
		if ((button || part.control.part == rules::Part::Picture) && part.control.place < _places.size())
		{
			controls.push_back({.subMesh = part.subMesh,
			                    .button = button,
			                    .place = part.control.place,
			                    .done = _places[part.control.place].shown.done});
		}
	}
	return controls;
}

std::optional<entt::entity> TempleChallengeRoom::GetLookedAtRecord() const
{
	if (!_pictureLookedAt || !_highDetail.has_value() || *_highDetail >= _places.size())
	{
		return std::nullopt;
	}
	return _places[*_highDetail].record;
}

bool TempleChallengeRoom::Hold(bool pressed, std::optional<uint32_t> hoveredSubMesh, bool closeUp, Outcome& outcome)
{
	const bool pressing = pressed && !_wasPressed;
	_wasPressed = pressed;
	if (pressing && hoveredSubMesh.has_value())
	{
		if (const auto* part = FindPart(*hoveredSubMesh);
		    part != nullptr && IsShown(*part) &&
		    (part->control.part == rules::Part::Picture || part->control.part == rules::Part::ReplayButton))
		{
			_held = *hoveredSubMesh;
		}
	}
	if (pressed || !_held.has_value())
	{
		return _held.has_value();
	}
	// Let go: over the part pressed, it is clicked
	const auto subMesh = *_held;
	_held.reset();
	if (hoveredSubMesh != subMesh)
	{
		return false;
	}
	const auto* part = FindPart(subMesh);
	if (part == nullptr)
	{
		return false;
	}
	const auto challenges = Challenges();
	switch (rules::Clicked(part->control, closeUp, challenges))
	{
	case rules::Click::LookCloser:
	{
		// The camera looks at the middle of the picture's box from in front of it along its frame's z axis, and the
		// picture is shown in high detail
		const auto middle = (part->min + part->max) * 0.5f;
		outcome.lookAt = Focus {
		    .position = glm::vec3(part->frame * glm::vec4(middle + glm::vec3(0.0f, 0.0f, rules::k_LookDistance), 1.0f)),
		    .lookAt = glm::vec3(part->frame * glm::vec4(middle, 1.0f)),
		};
		_highDetail = part->control.place;
		_pictureLookedAt = true;
		break;
	}
	case rules::Click::Replay:
		outcome.replay = _places[part->control.place].record;
		break;
	case rules::Click::Nothing:
		break;
	}
	return false;
}
