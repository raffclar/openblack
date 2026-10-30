/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "NoInputDevice.h"

#include <cctype>

#include <algorithm>
#include <array>

#include "Locator.h"

using namespace openblack::input;

namespace
{

/// The names of the keys by their scancodes, before the keyboard of the window system starts. These are the names of
/// SDL 2. The test test_input_device compares them with the names of SDL. An empty name is a scancode with no name.
// clang-format off
constexpr std::array<std::string_view, 291> k_ScancodeNames {
	"", // 0
	"", // 1
	"", // 2
	"", // 3
	"A", // 4
	"B", // 5
	"C", // 6
	"D", // 7
	"E", // 8
	"F", // 9
	"G", // 10
	"H", // 11
	"I", // 12
	"J", // 13
	"K", // 14
	"L", // 15
	"M", // 16
	"N", // 17
	"O", // 18
	"P", // 19
	"Q", // 20
	"R", // 21
	"S", // 22
	"T", // 23
	"U", // 24
	"V", // 25
	"W", // 26
	"X", // 27
	"Y", // 28
	"Z", // 29
	"1", // 30
	"2", // 31
	"3", // 32
	"4", // 33
	"5", // 34
	"6", // 35
	"7", // 36
	"8", // 37
	"9", // 38
	"0", // 39
	"Return", // 40
	"Escape", // 41
	"Backspace", // 42
	"Tab", // 43
	"Space", // 44
	"-", // 45
	"=", // 46
	"[", // 47
	"]", // 48
	"\\", // 49
	"#", // 50
	";", // 51
	"'", // 52
	"`", // 53
	",", // 54
	".", // 55
	"/", // 56
	"CapsLock", // 57
	"F1", // 58
	"F2", // 59
	"F3", // 60
	"F4", // 61
	"F5", // 62
	"F6", // 63
	"F7", // 64
	"F8", // 65
	"F9", // 66
	"F10", // 67
	"F11", // 68
	"F12", // 69
	"PrintScreen", // 70
	"ScrollLock", // 71
	"Pause", // 72
	"Insert", // 73
	"Home", // 74
	"PageUp", // 75
	"Delete", // 76
	"End", // 77
	"PageDown", // 78
	"Right", // 79
	"Left", // 80
	"Down", // 81
	"Up", // 82
	"Numlock", // 83
	"Keypad /", // 84
	"Keypad *", // 85
	"Keypad -", // 86
	"Keypad +", // 87
	"Keypad Enter", // 88
	"Keypad 1", // 89
	"Keypad 2", // 90
	"Keypad 3", // 91
	"Keypad 4", // 92
	"Keypad 5", // 93
	"Keypad 6", // 94
	"Keypad 7", // 95
	"Keypad 8", // 96
	"Keypad 9", // 97
	"Keypad 0", // 98
	"Keypad .", // 99
	"", // 100
	"Application", // 101
	"Power", // 102
	"Keypad =", // 103
	"F13", // 104
	"F14", // 105
	"F15", // 106
	"F16", // 107
	"F17", // 108
	"F18", // 109
	"F19", // 110
	"F20", // 111
	"F21", // 112
	"F22", // 113
	"F23", // 114
	"F24", // 115
	"Execute", // 116
	"Help", // 117
	"Menu", // 118
	"Select", // 119
	"Stop", // 120
	"Again", // 121
	"Undo", // 122
	"Cut", // 123
	"Copy", // 124
	"Paste", // 125
	"Find", // 126
	"Mute", // 127
	"VolumeUp", // 128
	"VolumeDown", // 129
	"", // 130
	"", // 131
	"", // 132
	"Keypad ,", // 133
	"Keypad = (AS400)", // 134
	"", // 135
	"", // 136
	"", // 137
	"", // 138
	"", // 139
	"", // 140
	"", // 141
	"", // 142
	"", // 143
	"", // 144
	"", // 145
	"", // 146
	"", // 147
	"", // 148
	"", // 149
	"", // 150
	"", // 151
	"", // 152
	"AltErase", // 153
	"SysReq", // 154
	"Cancel", // 155
	"Clear", // 156
	"Prior", // 157
	"Return", // 158
	"Separator", // 159
	"Out", // 160
	"Oper", // 161
	"Clear / Again", // 162
	"CrSel", // 163
	"ExSel", // 164
	"", // 165
	"", // 166
	"", // 167
	"", // 168
	"", // 169
	"", // 170
	"", // 171
	"", // 172
	"", // 173
	"", // 174
	"", // 175
	"Keypad 00", // 176
	"Keypad 000", // 177
	"ThousandsSeparator", // 178
	"DecimalSeparator", // 179
	"CurrencyUnit", // 180
	"CurrencySubUnit", // 181
	"Keypad (", // 182
	"Keypad )", // 183
	"Keypad {", // 184
	"Keypad }", // 185
	"Keypad Tab", // 186
	"Keypad Backspace", // 187
	"Keypad A", // 188
	"Keypad B", // 189
	"Keypad C", // 190
	"Keypad D", // 191
	"Keypad E", // 192
	"Keypad F", // 193
	"Keypad XOR", // 194
	"Keypad ^", // 195
	"Keypad %", // 196
	"Keypad <", // 197
	"Keypad >", // 198
	"Keypad &", // 199
	"Keypad &&", // 200
	"Keypad |", // 201
	"Keypad ||", // 202
	"Keypad :", // 203
	"Keypad #", // 204
	"Keypad Space", // 205
	"Keypad @", // 206
	"Keypad !", // 207
	"Keypad MemStore", // 208
	"Keypad MemRecall", // 209
	"Keypad MemClear", // 210
	"Keypad MemAdd", // 211
	"Keypad MemSubtract", // 212
	"Keypad MemMultiply", // 213
	"Keypad MemDivide", // 214
	"Keypad +/-", // 215
	"Keypad Clear", // 216
	"Keypad ClearEntry", // 217
	"Keypad Binary", // 218
	"Keypad Octal", // 219
	"Keypad Decimal", // 220
	"Keypad Hexadecimal", // 221
	"", // 222
	"", // 223
	"Left Ctrl", // 224
	"Left Shift", // 225
	"Left Alt", // 226
	"Left GUI", // 227
	"Right Ctrl", // 228
	"Right Shift", // 229
	"Right Alt", // 230
	"Right GUI", // 231
	"", // 232
	"", // 233
	"", // 234
	"", // 235
	"", // 236
	"", // 237
	"", // 238
	"", // 239
	"", // 240
	"", // 241
	"", // 242
	"", // 243
	"", // 244
	"", // 245
	"", // 246
	"", // 247
	"", // 248
	"", // 249
	"", // 250
	"", // 251
	"", // 252
	"", // 253
	"", // 254
	"", // 255
	"", // 256
	"ModeSwitch", // 257
	"AudioNext", // 258
	"AudioPrev", // 259
	"AudioStop", // 260
	"AudioPlay", // 261
	"AudioMute", // 262
	"MediaSelect", // 263
	"WWW", // 264
	"Mail", // 265
	"Calculator", // 266
	"Computer", // 267
	"AC Search", // 268
	"AC Home", // 269
	"AC Back", // 270
	"AC Forward", // 271
	"AC Stop", // 272
	"AC Refresh", // 273
	"AC Bookmarks", // 274
	"BrightnessDown", // 275
	"BrightnessUp", // 276
	"DisplaySwitch", // 277
	"KBDIllumToggle", // 278
	"KBDIllumDown", // 279
	"KBDIllumUp", // 280
	"Eject", // 281
	"Sleep", // 282
	"App1", // 283
	"App2", // 284
	"AudioRewind", // 285
	"AudioFastForward", // 286
	"SoftLeft", // 287
	"SoftRight", // 288
	"Call", // 289
	"EndCall", // 290
};
// clang-format on

/// Whether two names are the same, with no difference between the upper and the lower case of a letter
[[nodiscard]] bool SameName(std::string_view a, std::string_view b)
{
	return std::ranges::equal(a, b, [](unsigned char x, unsigned char y) { return std::toupper(x) == std::toupper(y); });
}

} // namespace

std::string_view openblack::input::DefaultScancodeName(SDL_Scancode scancode)
{
	const auto index = static_cast<size_t>(scancode);
	if (scancode < SDL_SCANCODE_UNKNOWN || index >= k_ScancodeNames.size())
	{
		return {};
	}
	return k_ScancodeNames.at(index);
}

SDL_Scancode openblack::input::DefaultScancodeFromName(std::string_view name)
{
	if (name.empty())
	{
		return SDL_SCANCODE_UNKNOWN;
	}
	const auto found = std::ranges::find_if(k_ScancodeNames,
	                                        [name](std::string_view known) { return !known.empty() && SameName(name, known); });
	return found != k_ScancodeNames.end() ? static_cast<SDL_Scancode>(found - k_ScancodeNames.begin()) : SDL_SCANCODE_UNKNOWN;
}

uint32_t NoInputDevice::Ticks() const
{
	using namespace std::chrono;
	return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now() - _start).count());
}

std::string_view NoInputDevice::ScancodeName(SDL_Scancode scancode) const
{
	return DefaultScancodeName(scancode);
}

SDL_Scancode NoInputDevice::ScancodeFromName(std::string_view name) const
{
	return DefaultScancodeFromName(name);
}

InputDeviceInterface& openblack::input::Device()
{
	if (Locator::inputDevice::has_value())
	{
		return Locator::inputDevice::value();
	}
	// The window system keeps one keyboard for the whole program, also before it starts. Thus one device serves all.
	static NoInputDevice none;
	return none;
}
