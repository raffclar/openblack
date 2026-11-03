/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DataReport.h"

#include <charconv>
#include <cstring>

#include <algorithm>
#include <array>
#include <exception>
#include <format>
#include <limits>
#include <memory>
#include <utility>

#include <EnumHeader.h>
#include <SaveGameInfo.h>
#include <VanillaSaveFile.h>

#include "Common/PlayerProfiles.h"
#include "Common/Zip.h"

namespace openblack::datatool
{

namespace
{
/// Text of the characters a report may hold: anything else shows as a question mark
std::string Narrow(std::u16string_view text)
{
	std::string narrow;
	for (const auto c : text)
	{
		narrow.push_back(c >= 0x20 && c < 0x7F ? static_cast<char>(c) : '?');
	}
	return narrow;
}

std::string_view PropertyTypeName(psys::Property::Type type)
{
	switch (type)
	{
	case psys::Property::Type::Bool:
		return "BOOL";
	case psys::Property::Type::Integer:
		return "INT";
	case psys::Property::Type::Float:
		return "FLOAT";
	case psys::Property::Type::String:
		return "STRING";
	case psys::Property::Type::Reference:
		return "PERSIS_PNTR";
	case psys::Property::Type::Array:
		return "ARRAY";
	case psys::Property::Type::SoundAction:
		return "SOUND_ACTION";
	}
	return "?";
}

std::string PropertyValue(const psys::Property& property)
{
	switch (property.type)
	{
	case psys::Property::Type::Bool:
		return property.integer != 0 ? "true" : "false";
	case psys::Property::Type::Integer:
		return std::to_string(property.integer);
	case psys::Property::Type::Float:
		return std::format("{}", property.number);
	case psys::Property::Type::String:
	case psys::Property::Type::Reference:
		return property.text;
	case psys::Property::Type::Array:
	{
		std::string text;
		for (const auto value : property.numbers)
		{
			text += std::format("{}{}", text.empty() ? "" : " ", value);
		}
		return text;
	}
	case psys::Property::Type::SoundAction:
		return std::format("{}{}{}{}", property.sound.sound, property.sound.looping ? " looping" : "",
		                   property.sound.softRelease ? " soft-release" : "", property.sound.useSurface ? " surface" : "");
	}
	return {};
}

struct ChannelRange
{
	uint8_t lowest {std::numeric_limits<uint8_t>::max()};
	uint8_t highest {0};
	uint64_t sum {0};
};

std::string ChannelLine(std::string_view name, const ChannelRange& range, size_t pixels)
{
	const auto mean = pixels == 0 ? 0.0 : static_cast<double>(range.sum) / static_cast<double>(pixels);
	return std::format("{}: lowest {}, highest {}, mean {:.2f}\n", name, pixels == 0 ? 0 : range.lowest, range.highest, mean);
}

/// One table of the info block
struct TableEntry
{
	std::string_view name;
	size_t offset;
	size_t rows;
	size_t rowSize;
};

template <typename T>
struct TableShape
{
	static constexpr size_t k_Rows = 1;
	static constexpr size_t k_RowSize = sizeof(T);
};

template <typename T, size_t N>
struct TableShape<std::array<T, N>>
{
	static constexpr size_t k_Rows = N;
	static constexpr size_t k_RowSize = sizeof(T);
};

std::vector<TableEntry> InfoTables()
{
	std::vector<TableEntry> tables;
	size_t offset = 0;
	using Info = InfoConstants;
	const auto add = [&](std::string_view name, size_t rows, size_t rowSize) {
		tables.push_back({name, offset, rows, rowSize});
		offset += rows * rowSize;
	};
#define X(member) add(#member, TableShape<decltype(Info::member)>::k_Rows, TableShape<decltype(Info::member)>::k_RowSize);
	X(magicGeneral)
	X(magicHeal)
	X(magicTeleport)
	X(magicForest)
	X(magicFood)
	X(magicStormAndTornado)
	X(magicShield)
	X(magicWood)
	X(magicWater)
	X(magicFlockFlying)
	X(magicFlockGround)
	X(magicCreatureSpell)
	X(magicEffect)
	X(spellSeed)
	X(animal)
	X(creature)
	X(differentCreature)
	X(creatureDesireForType)
	X(creatureDevelopmentPhaseEntry)
	X(creatureDevelopmentDurationEntry)
	X(citadel)
	X(citadelHeart)
	X(creaturePen)
	X(worshipSite)
	X(spellIcon)
	X(abode)
	X(villager)
	X(specialVillager)
	X(tree)
	X(singleMapFixed)
	X(scriptHighlight)
	X(mapShield)
	X(ball)
	X(town)
	X(job)
	X(feature)
	X(flowers)
	X(animatedStatic)
	X(mobileObject)
	X(scaffold)
	X(mobileStatic)
	X(pot)
	X(prayerIcon)
	X(prayerSite)
	X(showNeeds)
	X(townDesire)
	X(worshipSiteUpgrade)
	X(helpSpirit)
	X(arrow)
	X(spotVisual)
	X(effect)
	X(field)
	X(fieldType)
	X(fishFarm)
	X(footballPosition)
	X(playtime)
	X(player)
	X(sound)
	X(belief)
	X(helpSpritesGuidance)
	X(influence)
	X(helpSystem)
	X(alignment)
	X(reaction)
	X(creatureAction)
	X(creatureDesireAction1)
	X(creatureDesireAction2)
	X(creatureDesireAction3)
	X(creatureDesireDependency)
	X(creatureInitialDesire)
	X(desireSourceTable)
	X(creatureInitialSource1)
	X(creatureInitialSource2)
	X(creatureSourceBounds)
	X(creatureDesireAttributeEntry)
	X(creatireActionKnownAboutEntry)
	X(creatureMagicActionKnownAboutEntry)
	X(creatureMimic)
	X(terrainMaterial)
	X(tribe)
	X(speedThreshold)
	X(pBall)
	X(pFootball)
	X(pFootballPosition)
	X(dance)
	X(villagerStateTable)
	X(animalStateTable)
	X(totemStatue)
	X(bigForest)
	X(furniture)
	X(weather)
	X(climate)
	X(spellSystem)
	X(vortex)
	X(magicFireBall)
	X(reward)
	X(leashSelector)
	X(rewardProgressGood)
	X(rewardProgressEvil)
	X(spookyVoice)
	X(scriptOpposingCreature)
	X(toolTips)
#undef X
	return tables;
}
} // namespace

std::string DescribeGestures(const gestures::GestureFile& file)
{
	std::string text = std::format("{} templates\n", file.GetTemplates().size());
	for (size_t i = 0; i < file.GetTemplates().size(); ++i)
	{
		const auto& entry = file.GetTemplates()[i];
		text += std::format("[{}] gesture {}, {} points, position mode {}, direction {}, mirror {}, aspect ratio {}{}\n", i,
		                    entry.Gesture(), entry.PointCount(), entry.PositionModeValue(),
		                    entry.ChecksDirection() ? "checked" : "free", entry.AllowsMirror() ? "allowed" : "no",
		                    entry.ChecksAspectRatio() ? "checked " : "free ", entry.aspectRatio);
		for (const auto& point : entry.Points())
		{
			text +=
			    std::format("    ({:.3f}, {:.3f}) turn {:.3f} direction {}\n", point.x, point.z, point.turn, point.direction);
		}
	}
	return text;
}

std::string DescribeParticleFile(const psys::ParticleFile& file, bool properties)
{
	std::string text = std::format("header: {} properties\n{} objects\n", file.header.properties.size(), file.objects.size());
	for (const auto& object : file.objects)
	{
		text += std::format("{} {}: {} properties\n", object.className, object.name, object.properties.size());
		if (properties)
		{
			for (const auto& [key, property] : object.properties)
			{
				text += std::format("    {} {} {}\n", key, PropertyTypeName(property.type), PropertyValue(property));
			}
		}
	}
	return text;
}

std::optional<std::string> ParticleText(std::span<const uint8_t> bytes, bool compressed)
{
	if (!compressed)
	{
		return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	}
	const auto parts = psys::SplitCompressed(bytes);
	if (!parts.has_value())
	{
		return std::nullopt;
	}
	try
	{
		const auto inflated =
		    zip::Inflate(std::vector<uint8_t>(parts->deflated.begin(), parts->deflated.end()), parts->textSize);
		return std::string(inflated.begin(), inflated.end());
	}
	catch (const std::exception&)
	{
		return std::nullopt;
	}
}

std::string DescribeRgbImage(const rawimage::RgbImage& image)
{
	std::array<ChannelRange, 3> ranges {};
	for (const auto& pixel : image.pixels)
	{
		for (size_t c = 0; c < 3; ++c)
		{
			ranges[c].lowest = std::min(ranges[c].lowest, pixel[c]);
			ranges[c].highest = std::max(ranges[c].highest, pixel[c]);
			ranges[c].sum += pixel[c];
		}
	}
	std::string text = std::format("{} x {} pixels, 3 bytes a pixel\n", image.width, image.height);
	text += ChannelLine("red", ranges[0], image.pixels.size());
	text += ChannelLine("green", ranges[1], image.pixels.size());
	text += ChannelLine("blue", ranges[2], image.pixels.size());
	return text;
}

std::string DescribeGreyImage(const rawimage::GreyImage& image)
{
	ChannelRange range;
	for (const auto pixel : image.pixels)
	{
		range.lowest = std::min(range.lowest, pixel);
		range.highest = std::max(range.highest, pixel);
		range.sum += pixel;
	}
	std::string text = std::format("{} x {} pixels, 1 byte a pixel\n", image.width, image.height);
	text += ChannelLine("level", range, image.pixels.size());
	return text;
}

std::optional<uint32_t> SquareSide(size_t byteCount, uint32_t channels)
{
	if (channels == 0 || byteCount == 0 || byteCount % channels != 0)
	{
		return std::nullopt;
	}
	const auto pixels = byteCount / channels;
	auto side = static_cast<size_t>(0);
	while ((side + 1) * (side + 1) <= pixels)
	{
		++side;
	}
	if (side * side != pixels)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(side);
}

std::optional<std::pair<uint32_t, uint32_t>> ParseSize(std::string_view text)
{
	const auto cross = text.find('x');
	if (cross == std::string_view::npos)
	{
		return std::nullopt;
	}
	const auto number = [](std::string_view part) -> std::optional<uint32_t> {
		uint32_t value = 0;
		const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
		if (part.empty() || error != std::errc() || end != part.data() + part.size() || value == 0)
		{
			return std::nullopt;
		}
		return value;
	};
	const auto width = number(text.substr(0, cross));
	const auto height = number(text.substr(cross + 1));
	if (!width.has_value() || !height.has_value())
	{
		return std::nullopt;
	}
	return std::pair {*width, *height};
}

namespace
{
/// The block as the newest layout, or a note of why it cannot be
std::optional<std::vector<uint8_t>> BlockAsPatchLayout(std::span<const uint8_t> block, std::string& note)
{
	if (block.size() == sizeof(v120::InfoConstants))
	{
		note = "layout of the patch 1.20";
		return std::vector<uint8_t>(block.begin(), block.end());
	}
	if (block.size() == sizeof(v100::InfoConstants))
	{
		auto old = std::make_unique<v100::InfoConstants>();
		std::memcpy(old.get(), block.data(), block.size());
		auto converted = std::make_unique<InfoConstants>();
		std::memset(converted.get(), 0, sizeof(InfoConstants));
		UpdateInfo(*converted, *old);
		note = "layout of version 1.0, shown as the patch 1.20 layout";
		const auto* bytes = reinterpret_cast<const uint8_t*>(converted.get());
		return std::vector<uint8_t>(bytes, bytes + sizeof(InfoConstants));
	}
	note = std::format("{} bytes: neither {} (version 1.0) nor {} (patch 1.20)", block.size(), sizeof(v100::InfoConstants),
	                   sizeof(v120::InfoConstants));
	return std::nullopt;
}
} // namespace

std::string DescribeInfoBlock(std::span<const uint8_t> block)
{
	std::string note;
	if (!BlockAsPatchLayout(block, note).has_value())
	{
		return std::format("info block: {}\n", note);
	}
	std::string text = std::format("info block: {} bytes, {}\n", block.size(), note);
	const auto tables = InfoTables();
	text += std::format("{} tables\n", tables.size());
	for (const auto& table : tables)
	{
		text += std::format("{:<36} offset {:>7} rows {:>4} row size {:>5} bytes\n", table.name, table.offset, table.rows,
		                    table.rowSize);
	}
	const auto& last = tables.back();
	text += std::format("the tables cover {} of {} bytes of the layout\n", last.offset + (last.rows * last.rowSize),
	                    sizeof(InfoConstants));
	return text;
}

std::optional<std::string> DumpInfoTable(std::span<const uint8_t> block, std::string_view table, size_t firstRow,
                                         size_t rowCount)
{
	std::string note;
	const auto bytes = BlockAsPatchLayout(block, note);
	if (!bytes.has_value())
	{
		return std::nullopt;
	}
	const auto tables = InfoTables();
	const auto found = std::ranges::find(tables, table, &TableEntry::name);
	if (found == tables.end() || firstRow >= found->rows)
	{
		return std::nullopt;
	}
	const auto last = std::min(found->rows, firstRow + rowCount);
	std::string text;
	for (size_t row = firstRow; row < last; ++row)
	{
		text += std::format("{}[{}]\n", found->name, row);
		const auto start = found->offset + (row * found->rowSize);
		for (size_t at = 0; at < found->rowSize; at += 16)
		{
			text += std::format("  {:04x}:", at);
			for (size_t i = at; i < std::min(at + 16, found->rowSize); ++i)
			{
				text += std::format(" {:02x}", (*bytes)[start + i]);
			}
			text += "\n";
		}
	}
	return text;
}

std::string DescribeFont(const gui::GameFont& font, bool glyphs)
{
	std::string text = std::format("font \"{}\": bitmap height {}, {} glyphs, atlas {} x {}, full atlas {} x {}\n",
	                               font.GetName(), font.GetHeight(), font.GetGlyphs().size(), font.GetAtlasSize().x,
	                               font.GetAtlasSize().y, font.GetFullAtlasSize().x, font.GetFullAtlasSize().y);
	if (glyphs)
	{
		for (const auto& glyph : font.GetGlyphs())
		{
			text += std::format("U+{:04X} width {} left {} ink {} right {}\n", static_cast<uint32_t>(glyph.character),
			                    glyph.width, glyph.left, glyph.ink, glyph.right);
		}
	}
	return text;
}

std::string DrawGlyph(const gui::GameFont& font, char16_t character)
{
	const auto* glyph = font.Find(character);
	if (glyph == nullptr)
	{
		return {};
	}
	const auto& atlas = font.GetFullAtlas();
	const auto stride = font.GetFullAtlasSize().x;
	std::string text;
	for (uint32_t y = glyph->fullMin.y; y < glyph->fullMax.y; ++y)
	{
		for (uint32_t x = glyph->fullMin.x; x < glyph->fullMax.x; ++x)
		{
			text.push_back(atlas[(static_cast<size_t>(y) * stride) + x] != 0 ? '#' : '.');
		}
		text.push_back('\n');
	}
	return text;
}

std::string DescribeProfiles(std::string_view text)
{
	const auto profiles = player_profiles::Read(text);
	std::string report = std::format("{} profiles, playing as \"{}\"\n", profiles.profiles.size(), profiles.current);
	for (const auto& profile : profiles.profiles)
	{
		report += std::format("\"{}\": creature \"{}\" in {}, symbol {}, alignment {}\n", profile.name, profile.creatureName,
		                      profile.creatureFile, profile.symbol, profile.alignment);
		report += std::format("    tool tips {}, story text {}, help level {}, creature help {}, read speed {}, text from "
		                      "bottom {}, left handed {}, push scrolling {}, auto save {}\n",
		                      profile.settings.toolTips, profile.settings.storyText, profile.settings.helpLevel,
		                      profile.settings.creatureHelp, profile.settings.readSpeed, profile.settings.textFromBottom,
		                      profile.settings.leftHanded, profile.settings.pushScrolling, profile.settings.autoSave);
		report += std::format("    played {} s, {} saves, {} loads, {} keys kept, autosaves on {} lands\n",
		                      profile.statistics.secondsPlayed, profile.statistics.saves, profile.statistics.loads,
		                      profile.keys.size(), profile.landAutosaves.size());
	}
	return report;
}

std::string DescribeSaveInfo(std::span<const uint8_t> bytes)
{
	const auto info = gamesave::ReadSaveGameInfo(bytes);
	if (!info.has_value())
	{
		return std::format("save info: {} bytes, fewer than {}\n", bytes.size(), gamesave::k_SaveGameInfoSize);
	}
	return std::format("save info: slot {}, saved \"{}\", named \"{}\"\n", info->slot, Narrow(info->date), Narrow(info->name));
}

std::string DescribeSavedWorld(std::span<const uint8_t> bytes)
{
	const auto framing = gamesave::ReadVanillaFraming(bytes);
	if (!framing.has_value())
	{
		return std::format("saved world: {} bytes, the start is not a saved world\n", bytes.size());
	}
	std::string text = std::format("saved world: {} bytes, revision \"{}\", land \"{}\", {} write attempts\n", bytes.size(),
	                               framing->revision, framing->landscape, framing->attempts);
	text += std::format("game object at byte {}, running sum {:08x}\n", framing->body, framing->checksum);
	const auto creatures = gamesave::FindCreatures(bytes);
	text += std::format("{} creatures\n", creatures.size());
	for (const auto& found : creatures)
	{
		text += std::format("    object {} at bytes {} to {}\n", found.number, found.at, found.end);
	}
	const auto leashes = gamesave::FindLeashes(bytes);
	text += std::format("{} leashes\n", leashes.size());
	for (const auto& found : leashes)
	{
		text += std::format("    object {} at bytes {} to {}\n", found.number, found.at, found.end);
	}
	return text;
}

std::string DescribeSounds(const sas::SASFile& file, bool frames)
{
	size_t sounds = 0;
	for (const auto& clip : file.clips)
	{
		sounds += clip.sounds.size();
	}
	std::string text = std::format("version {}, {} clips, {} sounds\n", file.version, file.clips.size(), sounds);
	for (const auto& clip : file.clips)
	{
		text += std::format("{} (kind {}): {} sounds\n", clip.clip, clip.soundType, clip.sounds.size());
		if (frames)
		{
			for (const auto& sound : clip.sounds)
			{
				text += std::format("    at {} ms: sound {} mode {}\n", sound.time, sound.action, sound.mode);
			}
		}
	}
	return text;
}

std::string DescribeEnumHeader(std::string_view header)
{
	const auto names = psys::ParseEnumHeader(header);
	std::vector<std::pair<int32_t, std::string_view>> ordered;
	ordered.reserve(names.size());
	for (const auto& [name, value] : names)
	{
		ordered.emplace_back(value, name);
	}
	std::ranges::sort(ordered);
	std::string text = std::format("{} names\n", ordered.size());
	for (const auto& [value, name] : ordered)
	{
		text += std::format("{:>6} {}\n", value, name);
	}
	return text;
}

std::string DescribeDance(const dance::DanceFile& file)
{
	std::string text = std::format("version {}, {} key frames, {} groups, loops {}, beat {}\n", file.version,
	                               file.keyFrames.size(), file.groupCount, file.loops, file.beat);
	if (file.angle.has_value())
	{
		text += std::format("angle {}\n", *file.angle);
	}
	for (size_t i = 0; i < file.groupNames.size(); ++i)
	{
		text += std::format("group {}: {}\n", i, file.groupNames[i]);
	}
	for (size_t i = 0; i < file.keyFrames.size(); ++i)
	{
		const auto& frame = file.keyFrames[i];
		text += std::format("key frame {} at beat {}: {} actions\n", i, frame.time, frame.actions.size());
		for (const auto& action : frame.actions)
		{
			text += std::format("    action {} on {} groups\n", action.type, action.groups.size());
		}
	}
	return text;
}

std::string DescribeDanceShape(const dance::DanceShapeFile& file)
{
	std::string text = std::format("{} points\n", file.points.size());
	for (size_t i = 0; i < file.points.size(); ++i)
	{
		text += std::format("[{}] ({}, {})\n", i, file.points[i].x, file.points[i].z);
	}
	return text;
}

std::string DescribeFootpaths(std::span<const serializer::GameThingSerializer::FootpathLinkSave> links,
                              std::span<const serializer::GameThingSerializer::Footpath> paths)
{
	std::string text = std::format("{} links, {} paths\n", links.size(), paths.size());
	for (size_t i = 0; i < links.size(); ++i)
	{
		text += std::format("link {} at ({}, {}) holds {} paths\n", i, links[i].coords.x, links[i].coords.z,
		                    links[i].link.footpaths.size());
	}
	for (size_t i = 0; i < paths.size(); ++i)
	{
		text += std::format("path {}: {} nodes\n", i, paths[i].nodes.size());
		for (const auto& node : paths[i].nodes)
		{
			text += std::format("    ({}, {}) altitude {}\n", node.coords.x, node.coords.z, node.coords.altitude);
		}
	}
	return text;
}

} // namespace openblack::datatool
