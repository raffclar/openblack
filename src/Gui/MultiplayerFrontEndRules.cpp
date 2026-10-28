/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MultiplayerFrontEndRules.h"

#include <charconv>

#include <algorithm>

#include <fmt/format.h>

#include "Online/OnlineMapLand.h"
#include "PingDisplay.h"

namespace openblack::gui::multiplayer
{
namespace
{

using ecs::systems::LobbyErrorKind;

std::u16string Widen(std::string_view text)
{
	std::u16string out;
	out.reserve(text.size());
	for (const auto c : text)
	{
		out.push_back(static_cast<char16_t>(static_cast<unsigned char>(c)));
	}
	return out;
}

std::u16string Number(int64_t value, int width)
{
	std::array<char, 32> digits {};
	const auto [end, error] = std::to_chars(digits.data(), digits.data() + digits.size(), value);
	auto text = Widen(std::string_view(digits.data(), static_cast<size_t>(end - digits.data())));
	while (static_cast<int>(text.size()) < width)
	{
		text.insert(text.begin(), u'0');
	}
	return text;
}

char16_t Lower(char16_t c)
{
	return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c - u'A' + u'a') : c;
}

/// The observers' grey, as 0xAARRGGBB
constexpr uint32_t k_ObserverColour = 0xFF808080;
/// The chat colour of each of the 32 gods, as 0xAARRGGBB. The first four are the game room's team colours: red, green,
/// blue and yellow. The others are far from each other, from the grey of the observers and from the player's own lines,
/// and bright enough to read on the dark chat box. A god earlier in the list is further from the colours before it.
constexpr std::array<uint32_t, 32> k_GodColours = {
    0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFF00, 0xFFFF33BB, 0xFF3377EE, 0xFF11FFBB, 0xFF559900,
    0xFFDD5566, 0xFFEEFF99, 0xFFEEAAFF, 0xFF00BBFF, 0xFFDD11FF, 0xFF119977, 0xFF66FFFF, 0xFFAA55DD,
    0xFFAAFF55, 0xFF778844, 0xFFFF7700, 0xFFEEAA88, 0xFFFFCC33, 0xFF00FF77, 0xFFBB7799, 0xFFAAEEBB,
    0xFF2299AA, 0xFFFF0077, 0xFF7788CC, 0xFFFF77BB, 0xFF00AA55, 0xFFBBCC33, 0xFFFF0044, 0xFF00BB00,
};
/// The player's own chat lines
constexpr uint32_t k_OwnLine = 0xFFC8C8C8;

glm::vec4 Colour(uint32_t argb)
{
	return {static_cast<float>((argb >> 16u) & 0xFFu) / 255.0f, static_cast<float>((argb >> 8u) & 0xFFu) / 255.0f,
	        static_cast<float>(argb & 0xFFu) / 255.0f, static_cast<float>((argb >> 24u) & 0xFFu) / 255.0f};
}

/// The texts that describe the first three local lands
constexpr std::array<std::string_view, 3> k_LandInfoTexts = {"HELP_TEXT_DYK_RULES_02", "HELP_TEXT_DYK_RULES_10",
                                                             "HELP_TEXT_DYK_RULES_11"};

/// The conditions' names in the template file and the names of their texts, in the order of the game
struct ConditionName
{
	std::string_view key;
	std::string_view text;
};
constexpr std::array<ConditionName, k_ConditionCount> k_Conditions = {{
    {"WC_KILL_NUM_VILLAGERS", "HELP_TEXT_MULTIPLAYER_01"},
    {"WC_CREATURE_KILL_VILLAGERS", "HELP_TEXT_MULTIPLAYER_02"},
    {"WC_VILLAGERS_BORN_IN_TOWN", "HELP_TEXT_MULTIPLAYER_03"},
    {"WC_HOUSES_BUILT", "HELP_TEXT_MULTIPLAYER_04"},
    {"WC_WONDERS_BUILT", "HELP_TEXT_MULTIPLAYER_05"},
    {"WC_FOOD_IN_PITS", "HELP_TEXT_MULTIPLAYER_06"},
    {"WC_WOOD_IN_PITS", "HELP_TEXT_MULTIPLAYER_07"},
    {"WC_TAKEOVER_NUM_TOWNS", "HELP_TEXT_MULTIPLAYER_08"},
    {"WC_PRAYER_POWER_GENERATED", "HELP_TEXT_MULTIPLAYER_09"},
    {"WC_BUILDINGS_PLAYER_SMASHED", "HELP_TEXT_MULTIPLAYER_10"},
    {"WC_BUILDINGS_CREATURE_SMASHED", "HELP_TEXT_MULTIPLAYER_11"},
    {"WC_BELIEF_IN_WORLD", "HELP_TEXT_MULTIPLAYER_12"},
    {"WC_BUILD_COMPLETE_NEW_TOWN", "HELP_TEXT_MULTIPLAYER_14"},
    {"WC_VILLAGERS_CONVERTED", "HELP_TEXT_MULTIPLAYER_15"},
    {"WC_GROW_TREES", "HELP_TEXT_MULTIPLAYER_16"},
}};

std::string_view Trim(std::string_view text)
{
	while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r'))
	{
		text.remove_prefix(1);
	}
	while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
	{
		text.remove_suffix(1);
	}
	return text;
}

/// The next number of a line of the template, which goes past it
std::optional<int32_t> NextNumber(std::string_view& text)
{
	text = Trim(text);
	int32_t value = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc())
	{
		return std::nullopt;
	}
	text.remove_prefix(static_cast<size_t>(end - text.data()));
	return value;
}

uint16_t ReadU16(std::span<const uint8_t> bytes, size_t at)
{
	return static_cast<uint16_t>(bytes[at] | (bytes[at + 1] << 8u));
}

uint32_t ReadU32(std::span<const uint8_t> bytes, size_t at)
{
	return static_cast<uint32_t>(bytes[at]) | (static_cast<uint32_t>(bytes[at + 1]) << 8u) |
	       (static_cast<uint32_t>(bytes[at + 2]) << 16u) | (static_cast<uint32_t>(bytes[at + 3]) << 24u);
}

/// The grey that Windows gives a colour when it copies the colour into a picture of 256 greys. Each part goes to the
/// middle of its step of eight, then the grey is the mean of the three parts, rounded.
uint8_t NearestGrey(uint32_t red, uint32_t green, uint32_t blue)
{
	constexpr uint32_t k_StepMask = 0xF8;
	constexpr uint32_t k_StepMiddles = 12;
	const auto sum = (red & k_StepMask) + (green & k_StepMask) + (blue & k_StepMask) + k_StepMiddles;
	return static_cast<uint8_t>((sum + 1) / 3);
}

} // namespace

MessageButtons ButtonsOf(Message message) noexcept
{
	switch (message)
	{
	case Message::NotRegistered:
	case Message::EmailRequired:
		return MessageButtons::YesNo;
	case Message::WaitingForTransfer:
	case Message::SendingStartData:
	case Message::None:
		return MessageButtons::None;
	default:
		return MessageButtons::Ok;
	}
}

Message TicketFailureMessage(ecs::systems::LobbyErrorKind /*error*/, std::string_view code) noexcept
{
	if (code == lobby_api::problems::k_EmailRequired)
	{
		return Message::EmailRequired;
	}
	return Message::ConnectionFailed;
}

Message JoinFailureMessage(std::string_view refusal, LobbyKind kind) noexcept
{
	// The server's word for an account that is in its room already
	if (refusal == "already")
	{
		return Message::AlreadyInRoom;
	}
	return kind == LobbyKind::Lan ? Message::CouldNotConnect : Message::ConnectionFailed;
}

LoginProblem CheckLogin(std::u16string_view name, std::u16string_view password)
{
	std::string ascii;
	for (const auto c : name)
	{
		if (c > 0x7F)
		{
			return LoginProblem::InvalidDetails;
		}
		ascii.push_back(static_cast<char>(c));
	}
	if (!lobby_api::IsValidAccountName(ascii))
	{
		return LoginProblem::InvalidDetails;
	}
	// An empty password gets the answer of a wrong password, as the game gives it
	if (password.empty() || password.size() > k_MostLoginCharacters)
	{
		return LoginProblem::IncorrectPassword;
	}
	return LoginProblem::None;
}

bool AsksAboutName(LobbyErrorKind error) noexcept
{
	return error == LobbyErrorKind::Refused || error == LobbyErrorKind::NotLoggedIn;
}

Message LoginFailureMessage(LobbyErrorKind error, std::optional<bool> nameAvailable) noexcept
{
	if (error == LobbyErrorKind::InvalidName)
	{
		return Message::InvalidDetails;
	}
	if (AsksAboutName(error) && nameAvailable.value_or(false))
	{
		return Message::NotRegistered;
	}
	// Every other failure gets the answer of a wrong password, as the game gives it
	return Message::IncorrectPassword;
}

std::u16string FormatText(std::u16string_view format, std::span<const TextArgument> arguments)
{
	std::u16string out;
	size_t next = 0;
	for (size_t i = 0; i < format.size(); ++i)
	{
		if (format[i] != u'%' || i + 1 >= format.size())
		{
			out.push_back(format[i]);
			continue;
		}
		if (format[i + 1] == u'%')
		{
			out.push_back(u'%');
			++i;
			continue;
		}
		// A width of the form 0N, then the kind of the code
		size_t at = i + 1;
		int width = 0;
		if (format[at] == u'0')
		{
			++at;
			while (at < format.size() && format[at] >= u'0' && format[at] <= u'9')
			{
				width = (width * 10) + (format[at] - u'0');
				++at;
			}
		}
		if (at >= format.size() || next >= arguments.size())
		{
			out.push_back(format[i]);
			continue;
		}
		const auto kind = format[at];
		const auto& argument = arguments[next];
		if (kind == u's' && std::holds_alternative<std::u16string_view>(argument))
		{
			out.append(std::get<std::u16string_view>(argument));
		}
		else if ((kind == u'd' || kind == u'u') && std::holds_alternative<int64_t>(argument))
		{
			out.append(Number(std::get<int64_t>(argument), width));
		}
		else
		{
			out.push_back(format[i]);
			continue;
		}
		++next;
		i = at;
	}
	return out;
}

std::u16string GameLine(const lobby_api::ServerEntry& server, const GameLineTexts& texts, std::string_view gameVersion,
                        LobbyKind kind)
{
	std::u16string line = Widen(server.hostname);
	line += u" (";
	line += Number(server.numplayers, 0);
	if (server.maxplayers > 0)
	{
		line += u'/';
		line += Number(server.maxplayers, 0);
	}
	line += u')';
	// A server is in play once its room has closed for the start
	if (server.gamemode == "closedplaying")
	{
		line += texts.playing;
	}
	if (server.gamever != gameVersion)
	{
		line += texts.wrongVersion;
	}
	if (server.password)
	{
		line += texts.needsPassword;
	}
	if (kind == LobbyKind::Internet && server.locked)
	{
		line += u'-';
		line += texts.locked;
	}
	return line;
}

bool MatchesFilter(std::u16string_view name, std::u16string_view filter)
{
	if (filter.empty())
	{
		return true;
	}
	return !std::ranges::search(name, filter, {}, Lower, Lower).empty();
}

std::vector<size_t> FilteredGames(std::span<const lobby_api::ServerEntry> servers, std::u16string_view filter, bool filterOn)
{
	std::vector<size_t> shown;
	for (size_t i = 0; i < servers.size(); ++i)
	{
		if (!filterOn || MatchesFilter(Widen(servers[i].hostname), filter))
		{
			shown.push_back(i);
		}
	}
	return shown;
}

std::vector<std::u16string> ConnectionLines(bool lan, std::optional<std::u16string_view> internet, std::u16string_view lanText,
                                            std::u16string_view defaultText)
{
	std::vector<std::u16string> lines;
	if (lan)
	{
		lines.push_back(Number(static_cast<int64_t>(lines.size()) + 1, 0) + u"." + std::u16string(lanText));
	}
	if (internet.has_value())
	{
		lines.push_back(Number(static_cast<int64_t>(lines.size()) + 1, 0) + u". " + std::u16string(*internet) + u" " +
		                std::u16string(defaultText));
	}
	return lines;
}

std::vector<Seat> Seats(std::span<const lobby_api::RoomPlayer> players)
{
	std::vector<Seat> seats;
	int32_t god = 0;
	for (const auto& player : players)
	{
		const bool isGod = player.role == "god";
		seats.push_back({.account = player.account, .god = isGod ? ++god : 0, .clan = player.clan});
	}
	return seats;
}

std::u16string RosterLine(const Seat& seat, std::u16string_view name, bool own, const RosterTexts& texts,
                          const std::optional<std::u16string>& ping)
{
	std::u16string line;
	if (seat.god > 0)
	{
		const std::array<TextArgument, 1> god = {int64_t {seat.god}};
		line = FormatText(texts.god, god);
	}
	else
	{
		line = texts.observers;
	}
	line += u": ";
	line += name;
	if (own)
	{
		line += texts.current;
		line = ping_display::AfterOwnName(std::move(line), ping);
	}
	return line;
}

uint32_t GodColourBits(int32_t god) noexcept
{
	if (god <= 0 || god > static_cast<int32_t>(k_GodColours.size()))
	{
		return k_ObserverColour;
	}
	return k_GodColours.at(static_cast<size_t>(god - 1));
}

glm::vec4 GodColour(int32_t god) noexcept
{
	return Colour(GodColourBits(god));
}

uint32_t OwnLineColourBits() noexcept
{
	return k_OwnLine;
}

glm::vec4 OwnLineColour() noexcept
{
	return Colour(k_OwnLine);
}

ChatItem ChatLine(std::u16string_view from, std::u16string_view text, bool own, int32_t god)
{
	ChatItem item;
	item.text = std::u16string(from) + (own ? u"> " : u": ") + std::u16string(text);
	item.colour = own ? OwnLineColour() : GodColour(god);
	return item;
}

std::u16string ClampChat(std::u16string_view text)
{
	return std::u16string(text.substr(0, std::min(text.size(), k_MostChatCharacters)));
}

std::u16string TimeLimitText(uint32_t minutes)
{
	const std::array<TextArgument, 2> arguments = {static_cast<int64_t>(minutes / 60u), static_cast<int64_t>(minutes % 60u)};
	return FormatText(u"%u:%02u", arguments);
}

std::vector<LocalMap> ParseLocalMaps(std::string_view text)
{
	std::vector<LocalMap> maps;
	while (!text.empty())
	{
		const auto end = text.find('\n');
		auto line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
		while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
		{
			line.remove_suffix(1);
		}
		// Each line starts with its number: "1:name:file:players:"
		const auto number = line.find(':');
		if (number == std::string_view::npos)
		{
			continue;
		}
		line.remove_prefix(number + 1);
		const auto first = line.find(':');
		if (first == std::string_view::npos)
		{
			continue;
		}
		const auto second = line.find(':', first + 1);
		if (second == std::string_view::npos)
		{
			continue;
		}
		const auto third = line.find(':', second + 1);
		const auto players =
		    line.substr(second + 1, third == std::string_view::npos ? std::string_view::npos : third - second - 1);
		LocalMap map {.name = Widen(line.substr(0, first)), .file = std::string(line.substr(first + 1, second - first - 1))};
		std::from_chars(players.data(), players.data() + players.size(), map.players);
		maps.push_back(std::move(map));
	}
	return maps;
}

std::optional<size_t> FindLand(std::span<const LocalMap> maps, std::string_view land)
{
	// The server sends the path of a network land, such as "Online Maps/mpm_2p_1.map". The list of the lands has the
	// file name with no folder and no extension, in any case.
	const auto mapFile = online_map_land::MapName(land);
	for (size_t i = 0; i < maps.size(); ++i)
	{
		if (maps[i].file == land || maps[i].name == Widen(land) ||
		    (!mapFile.empty() && online_map_land::MapName(maps[i].file + std::string(online_map_land::k_Extension)) == mapFile))
		{
			return i;
		}
	}
	return std::nullopt;
}

std::optional<std::string_view> LandInfoText(size_t index) noexcept
{
	if (index < k_LandInfoTexts.size())
	{
		return k_LandInfoTexts.at(index);
	}
	return std::nullopt;
}

std::vector<std::u16string> ClanInfoLines(const lobby_api::ClanDetails& clan, const ClanInfoTexts& texts)
{
	std::vector<std::u16string> lines;
	lines.push_back(texts.description + u" " + Widen(clan.description));
	lines.push_back(texts.leader + u" " + Widen(clan.leader));
	lines.push_back(texts.url + u" " + Widen(clan.url));
	// The credits have two places after the point
	const auto hundredths = static_cast<int64_t>((clan.points * 100.0) + (clan.points < 0.0 ? -0.5 : 0.5));
	const auto whole = hundredths / 100;
	const auto part = hundredths < 0 ? -(hundredths % 100) : hundredths % 100;
	auto value = Number(whole, 0) + u"." + Number(part, 2);
	if (hundredths < 0 && whole == 0)
	{
		value.insert(value.begin(), u'-');
	}
	lines.push_back(texts.credits + u" " + value);
	return lines;
}

std::optional<LogoOpacities> DecodeClanLogo(std::span<const uint8_t> bmp)
{
	// The file header (14 bytes) and a header of the picture of at least 40 bytes
	if (bmp.size() < 54 || bmp[0] != 'B' || bmp[1] != 'M')
	{
		return std::nullopt;
	}
	const auto pixels = ReadU32(bmp, 10);
	const auto headerSize = ReadU32(bmp, 14);
	if (headerSize < 40)
	{
		return std::nullopt;
	}
	const auto width = static_cast<int32_t>(ReadU32(bmp, 18));
	const auto rawHeight = static_cast<int32_t>(ReadU32(bmp, 22));
	const auto bits = ReadU16(bmp, 28);
	const auto compression = ReadU32(bmp, 30);
	if (width <= 0 || rawHeight == 0 || compression != 0 || (bits != 8 && bits != 24 && bits != 32))
	{
		return std::nullopt;
	}
	const bool bottomUp = rawHeight > 0;
	const auto height = bottomUp ? rawHeight : -rawHeight;
	const auto stride = static_cast<size_t>(((static_cast<int64_t>(width) * bits + 31) / 32) * 4);
	// The palette follows the header: 4 bytes a colour (blue, green, red, unused)
	const auto paletteAt = static_cast<size_t>(14) + headerSize;
	auto paletteCount = static_cast<size_t>(ReadU32(bmp, 46));
	if (bits == 8 && paletteCount == 0)
	{
		paletteCount = 256;
	}
	if (bits == 8 && paletteAt + (paletteCount * 4) > bmp.size())
	{
		return std::nullopt;
	}
	if (static_cast<size_t>(pixels) + (stride * static_cast<size_t>(height)) > bmp.size())
	{
		return std::nullopt;
	}
	LogoOpacities logo {};
	const auto rows = std::min(height, k_LogoSize);
	const auto columns = std::min(width, k_LogoSize);
	for (int32_t y = 0; y < rows; ++y)
	{
		const auto fileRow = bottomUp ? height - 1 - y : y;
		const auto row = static_cast<size_t>(pixels) + (stride * static_cast<size_t>(fileRow));
		for (int32_t x = 0; x < columns; ++x)
		{
			uint32_t red = 0;
			uint32_t green = 0;
			uint32_t blue = 0;
			if (bits == 8)
			{
				const auto index = static_cast<size_t>(bmp[row + static_cast<size_t>(x)]);
				if (index >= paletteCount)
				{
					continue;
				}
				const auto colour = paletteAt + (index * 4);
				blue = bmp[colour];
				green = bmp[colour + 1];
				red = bmp[colour + 2];
			}
			else
			{
				const auto at = row + (static_cast<size_t>(x) * (bits / 8u));
				blue = bmp[at];
				green = bmp[at + 1];
				red = bmp[at + 2];
			}
			logo.at(static_cast<size_t>((y * k_LogoSize) + x)) = static_cast<uint8_t>(NearestGrey(red, green, blue) >> 4u);
		}
	}
	return logo;
}

std::vector<std::u16string> PlayerDetailLines(std::span<const PlayerDetail> details)
{
	std::vector<std::u16string> lines;
	lines.reserve(details.size());
	for (const auto& detail : details)
	{
		lines.push_back(detail.label + u":" + detail.value);
	}
	return lines;
}

std::string_view ConditionText(size_t index) noexcept
{
	return index < k_Conditions.size() ? k_Conditions.at(index).text : std::string_view();
}

std::optional<size_t> ConditionIndex(std::string_view key) noexcept
{
	const auto place = std::ranges::find(k_Conditions, key, &ConditionName::key);
	if (place == k_Conditions.end())
	{
		return std::nullopt;
	}
	return static_cast<size_t>(place - k_Conditions.begin());
}

std::optional<std::string> ConditionsRefusal(const std::optional<ConditionTemplate>& limits,
                                             std::span<const game_end::ConditionPick> picks)
{
	if (!limits.has_value())
	{
		return "the template of the winning conditions has no block for the land";
	}
	for (const auto& pick : picks)
	{
		const auto index = static_cast<size_t>(pick.condition);
		const auto& each = limits->at(index);
		if (pick.value < each.least || pick.value > each.most)
		{
			return fmt::format("the value {} of {} is not from {} to {}", pick.value, k_Conditions.at(index).key, each.least,
			                   each.most);
		}
	}
	return std::nullopt;
}

std::optional<ConditionTemplate> FindConditions(std::string_view text, std::string_view landFile)
{
	const auto wanted = std::string(landFile) + ".txt";
	std::optional<ConditionTemplate> found;
	bool inBlock = false;
	while (!text.empty())
	{
		const auto end = text.find('\n');
		auto line = Trim(text.substr(0, end));
		text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
		if (line.starts_with("FORMAP"))
		{
			if (found.has_value())
			{
				break;
			}
			inBlock = Trim(line.substr(6)) == wanted;
			if (inBlock)
			{
				found.emplace();
			}
			continue;
		}
		if (!inBlock || line.empty() || line.starts_with("//"))
		{
			continue;
		}
		const auto colon = line.find(':');
		if (colon == std::string_view::npos)
		{
			continue;
		}
		const auto key = Trim(line.substr(0, colon));
		const auto place = std::ranges::find(k_Conditions, key, &ConditionName::key);
		if (place == k_Conditions.end())
		{
			continue;
		}
		auto values = line.substr(colon + 1);
		auto& limits = found->at(static_cast<size_t>(place - k_Conditions.begin()));
		const auto value = NextNumber(values);
		const auto least = NextNumber(values);
		const auto most = NextNumber(values);
		const auto left = NextNumber(values);
		const auto right = NextNumber(values);
		if (value && least && most && left && right)
		{
			limits = {.value = *value, .least = *least, .most = *most, .leftStep = *left, .rightStep = *right};
		}
	}
	return found;
}

void Next(Flow& flow)
{
	switch (flow.box)
	{
	case Box::ChooseConnection:
		flow.box = Box::FindGames;
		break;
	case Box::FindGames:
		flow.box = flow.kind == LobbyKind::Internet ? Box::Login : Box::GameList;
		break;
	case Box::Room:
		flow.box = Box::Conditions;
		break;
	case Box::Login:
	case Box::GameList:
	case Box::Conditions:
		break;
	}
}

void Back(Flow& flow)
{
	switch (flow.box)
	{
	case Box::ChooseConnection:
		flow.left = true;
		break;
	case Box::FindGames:
		flow.box = Box::ChooseConnection;
		break;
	case Box::Login:
		flow.box = Box::FindGames;
		break;
	case Box::GameList:
		flow.box = flow.kind == LobbyKind::Internet ? Box::Login : Box::FindGames;
		break;
	case Box::Room:
	case Box::Conditions:
		flow.box = Box::GameList;
		break;
	}
}

} // namespace openblack::gui::multiplayer
