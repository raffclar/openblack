/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleSaveGameRoom.h"

#include <algorithm>
#include <ranges>

#include <SavePictures.h>
#include <fmt/format.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "Graphics/GraphicsDeviceInterface.h"
#include "Locator.h"

using namespace openblack;
namespace rules = openblack::temple::save_game_room;

namespace
{
/// The empty picture's side, in texels
constexpr uint32_t k_EmptySide = 256;

/// A texture of compressed blocks of a picture's cell, none when there are no blocks
std::optional<graphics::TextureHandle> CompressedTexture(std::span<const uint8_t> blocks, uint32_t width, uint32_t height,
                                                         std::string_view name)
{
	const size_t needed = static_cast<size_t>(width / gamesave::k_PictureBlockSide) * (height / gamesave::k_PictureBlockSide) *
	                      gamesave::k_PictureBlockSize;
	if (blocks.size() < needed)
	{
		return std::nullopt;
	}
	const auto& device = Locator::graphicsDevice::value();
	const auto* memory = device.Copy(blocks.data(), static_cast<uint32_t>(needed));
	const auto handle = device.CreateTexture2D(static_cast<uint16_t>(width), static_cast<uint16_t>(height), 1,
	                                           graphics::TextureFormat::BlockCompression1,
	                                           graphics::texture_flags::k_ClampU | graphics::texture_flags::k_ClampV, memory);
	device.SetName(handle, name);
	return handle;
}

/// A texture of the columns of a square RGBA picture from first, as wide as given
graphics::TextureHandle ColumnsTexture(std::span<const uint8_t> rgba, uint32_t first, uint32_t width, std::string_view name)
{
	const auto& device = Locator::graphicsDevice::value();
	const auto* memory = device.Alloc(width * k_EmptySide * 4);
	auto* bytes = device.Bytes(memory).data();
	for (uint32_t y = 0; y < k_EmptySide; ++y)
	{
		const auto from = rgba.subspan(((static_cast<size_t>(y) * k_EmptySide) + first) * 4, static_cast<size_t>(width) * 4);
		std::ranges::copy(from, bytes + (static_cast<size_t>(y) * width * 4));
	}
	const auto handle = device.CreateTexture2D(static_cast<uint16_t>(width), static_cast<uint16_t>(k_EmptySide), 1,
	                                           graphics::TextureFormat::RGBA8,
	                                           graphics::texture_flags::k_ClampU | graphics::texture_flags::k_ClampV, memory);
	device.SetName(handle, name);
	return handle;
}
} // namespace

TempleSaveGameRoom::~TempleSaveGameRoom()
{
	Destroy(_emptyEvil);
	Destroy(_emptyGood);
	Destroy(_large);
	for (auto& texture : _small)
	{
		Destroy(texture);
	}
}

void TempleSaveGameRoom::Destroy(std::optional<graphics::TextureHandle>& texture)
{
	if (texture.has_value())
	{
		Locator::graphicsDevice::value().Destroy(*texture);
		texture.reset();
	}
}

void TempleSaveGameRoom::Find(const graphics::L3DMesh& mesh)
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

void TempleSaveGameRoom::SetEmptyPicture(std::span<const uint8_t> rgba)
{
	Destroy(_emptyEvil);
	Destroy(_emptyGood);
	if (rgba.size() < static_cast<size_t>(k_EmptySide) * k_EmptySide * 4)
	{
		return;
	}
	constexpr uint32_t k_Half = k_EmptySide / 2;
	_emptyEvil = ColumnsTexture(rgba, 0, k_Half, "Empty saved game, evil");
	_emptyGood = ColumnsTexture(rgba, k_Half, k_Half, "Empty saved game, good");
}

void TempleSaveGameRoom::ReadSmallPictures(const ReadPicture& read)
{
	for (auto& texture : _small)
	{
		Destroy(texture);
	}
	_small.assign(rules::k_Slots, std::nullopt);
	std::optional<std::string> lastName;
	std::optional<gamesave::PictureFile> file;
	for (uint32_t slot = 0; slot < rules::k_Slots; ++slot)
	{
		// Eight slots share a file of small pictures
		const auto name = gamesave::SmallPictureFileName(slot);
		if (name != lastName)
		{
			lastName = name;
			const auto bytes = read ? read(name) : std::nullopt;
			file = bytes.has_value() ? gamesave::ReadPictureFile(*bytes) : std::nullopt;
		}
		if (!file.has_value())
		{
			continue;
		}
		const auto cell = gamesave::SmallPictureCell(slot);
		_small[slot] = CompressedTexture(gamesave::CellBlocks(*file, cell), cell.width, cell.height,
		                                 fmt::format("Saved game {} low detail", slot));
	}
}

void TempleSaveGameRoom::ReadPicturesAfterSave(uint32_t slot, const ReadPicture& read)
{
	const auto pictures = rules::PicturesAfterSave(slot, _highDetail);
	if (pictures.smallPictures)
	{
		ReadSmallPictures(read);
	}
	// The picture in high detail shows the new save at once, not the picture that the slot had before the save
	if (pictures.largePicture)
	{
		ReadLargePicture(slot, read);
	}
}

void TempleSaveGameRoom::ReadLargePicture(uint32_t slot, const ReadPicture& read)
{
	Destroy(_large);
	const auto bytes = read ? read(gamesave::LargePictureFileName(slot)) : std::nullopt;
	const auto file = bytes.has_value() ? gamesave::ReadPictureFile(*bytes) : std::nullopt;
	if (!file.has_value())
	{
		return;
	}
	const auto cell = gamesave::LargePictureCell(slot);
	_large = CompressedTexture(gamesave::CellBlocks(*file, cell), cell.width, cell.height,
	                           fmt::format("Saved game {} high detail", slot));
}

void TempleSaveGameRoom::Enter()
{
	// The large picture goes, and the room forgets that the camera was last sent to a picture
	_highDetail.reset();
	_pictureLookedAt = false;
	Destroy(_large);
}

void TempleSaveGameRoom::Leave()
{
	Enter();
	_alignment = 0.0f;
	_held.reset();
	_wasPressed = false;
}

void TempleSaveGameRoom::SetSaves(std::vector<Save> saves)
{
	_saves = std::move(saves);
}

bool TempleSaveGameRoom::HasSave(uint32_t slot) const
{
	return SaveName(slot).has_value();
}

std::optional<std::u16string_view> TempleSaveGameRoom::SaveName(uint32_t slot) const
{
	const auto found = std::ranges::find(_saves, slot, &Save::slot);
	return found != _saves.end() ? std::optional<std::u16string_view>(found->name) : std::nullopt;
}

const TempleSaveGameRoom::Save* TempleSaveGameRoom::LastSaveOf(uint32_t slot) const
{
	const auto backwards = _saves | std::views::reverse;
	const auto found = std::ranges::find(backwards, slot, &Save::slot);
	return found != backwards.end() ? &*found : nullptr;
}

const TempleSaveGameRoom::Part* TempleSaveGameRoom::FindPart(uint32_t subMesh) const
{
	const auto found = std::ranges::find(_parts, subMesh, &Part::subMesh);
	return found != _parts.end() ? &*found : nullptr;
}

std::vector<TempleSubMeshTexture> TempleSaveGameRoom::GetTextures() const
{
	std::vector<TempleSubMeshTexture> textures;
	const auto& empty = rules::EmptyPicture(_alignment) == rules::EmptyPicture(1.0f) ? _emptyGood : _emptyEvil;
	for (const auto& part : _parts)
	{
		if (part.control.part != rules::Part::Picture)
		{
			continue;
		}
		const auto slot = part.control.slot;
		// A slot with a save shows its picture, the one looked at closer in high detail. A slot with no save, or whose
		// picture cannot be read, shows the empty picture.
		std::optional<graphics::TextureHandle> texture;
		if (HasSave(slot))
		{
			texture = _highDetail == slot ? _large : (slot < _small.size() ? _small[slot] : std::nullopt);
		}
		else
		{
			texture = empty;
		}
		if (texture.has_value())
		{
			textures.push_back({.subMesh = part.subMesh, .texture = *texture, .picture = true});
		}
	}
	return textures;
}

std::vector<uint32_t> TempleSaveGameRoom::GetHidden(const rules::View& view) const
{
	std::vector<uint32_t> hidden;
	for (const auto& part : _parts)
	{
		if (!rules::IsShown(part.control, view, HasSave(part.control.slot)))
		{
			hidden.push_back(part.subMesh);
		}
	}
	return hidden;
}

std::vector<TempleSubMeshOffset> TempleSaveGameRoom::GetOffsets() const
{
	const auto* part = _held.has_value() ? FindPart(*_held) : nullptr;
	if (part == nullptr)
	{
		return {};
	}
	const auto offset = rules::HeldOffset(part->control.part, glm::vec3(part->frame[2]));
	if (offset == glm::vec3(0.0f))
	{
		return {};
	}
	return {{.subMesh = part->subMesh, .offset = offset}};
}

bool TempleSaveGameRoom::IsControl(uint32_t subMesh, const rules::View& view) const
{
	const auto* part = FindPart(subMesh);
	return part != nullptr && rules::IsShown(part->control, view, HasSave(part->control.slot));
}

std::vector<TempleSaveGameControl> TempleSaveGameRoom::GetToolTipControls(const rules::View& view) const
{
	std::vector<TempleSaveGameControl> controls;
	for (const auto& part : _parts)
	{
		if (rules::IsShown(part.control, view, HasSave(part.control.slot)))
		{
			const auto name = SaveName(part.control.slot);
			controls.push_back({.subMesh = part.subMesh,
			                    .control = part.control,
			                    .saveName = name.has_value() ? std::optional<std::u16string>(*name) : std::nullopt});
		}
	}
	return controls;
}

bool TempleSaveGameRoom::Hold(bool pressed, std::optional<uint32_t> hoveredSubMesh, const rules::View& view,
                              const ReadPicture& read, Outcome& outcome)
{
	const bool pressing = pressed && !_wasPressed;
	_wasPressed = pressed;
	if (pressing && hoveredSubMesh.has_value())
	{
		if (const auto* part = FindPart(*hoveredSubMesh);
		    part != nullptr && rules::IsShown(part->control, view, HasSave(part->control.slot)))
		{
			_held = *hoveredSubMesh;
		}
	}
	if (pressed || !_held.has_value())
	{
		return _held.has_value();
	}
	// Let go: over the control pressed, it is clicked
	const auto subMesh = *_held;
	_held.reset();
	const auto* part = FindPart(subMesh);
	if (hoveredSubMesh != subMesh || part == nullptr)
	{
		return false;
	}
	const auto slot = part->control.slot;
	const auto click = rules::Clicked(part->control, view, HasSave(slot));
	if (click == rules::Click::LookCloser)
	{
		// The camera looks at the middle of the picture's box from in front of it along its frame's z axis, and the
		// picture is shown in high detail
		const auto middle = (part->min + part->max) * 0.5f;
		outcome.lookAt = Focus {
		    .position = glm::vec3(part->frame * glm::vec4(middle + glm::vec3(0.0f, 0.0f, rules::k_LookDistance), 1.0f)),
		    .lookAt = glm::vec3(part->frame * glm::vec4(middle, 1.0f)),
		};
		_highDetail = slot;
		_pictureLookedAt = true;
		ReadLargePicture(slot, read);
		return false;
	}
	outcome.click = click;
	outcome.slot = slot;
	return false;
}
