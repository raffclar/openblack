#!/usr/bin/env python3
"""Writes src/Inspector/ComponentFields.cpp: every component of src/ECS/Components and its data members registered
with the inspector's reflection, so that the inspector writes them out as JSON and sets them by name.

Run it again after adding or changing a component (with clang-format on the PATH, it formats the file too):

    python tools/inspector/generate_component_fields.py

With --check it writes nothing and fails if the file is out of date (spaces aside), as ctest's
test_inspector_component_fields does, so that a branch adding a component without registering it is told so.

Only plain structs in openblack::ecs::components are read. Members that are functions, static, references or C arrays
are left out, as are templates. A struct with no members is still registered, so that it can be added to an entity.

The values the components' fields hold are registered too, for their fields alone (not as components): every plain
struct of src/ (public members, no base, not a template) that a field holds, directly or in a list or an option, and
those their own fields hold in turn. A list indexed by an enumeration (std::array<T, static_cast<size_t>(E::_Count)>, or
a constant of that) is given its enumeration's names, which the inspector reads and writes its elements by.

Types whose data is private are registered by hand in src/Inspector/ComponentFieldsByHand.cpp (BY_HAND below says
which types they give, to be registered here). With --report it lists the types fields hold that it can't register,
and why.
"""

import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
COMPONENTS = ROOT / "src" / "ECS" / "Components"
OUTPUT = ROOT / "src" / "Inspector" / "ComponentFields.cpp"
NAMESPACE = "openblack::ecs::components"

# Types that aren't components, though they are declared beside them
SKIPPED = {
    # Held in the registry's context, not on entities
    "VillageLightFlames",
    "MapScriptGlobals",
}

SOURCES = ROOT / "src"

# Types with private data, registered by hand in src/Inspector/ComponentFieldsByHand.cpp, and the types their functions
# give, which are registered here
BY_HAND = {
    "openblack::creature_fight::MoveQueue": ["openblack::creature_fight::QueuedMove"],
    "openblack::DayNightClock": [],
    "openblack::Zoomer": [],
    "openblack::animals::Zoomer": [],
    "openblack::hand_grab::HandSpring": [],
    "openblack::sky_dome::Follow": [],
    "openblack::creature_route::Planner": [],
}

# Namespaces of libraries, whose types aren't followed
EXTERNAL = ("std::", "glm::", "entt::", "bgfx::", "bx::", "spdlog::")
# Holders of a value elsewhere, which are written as what they are, not followed
INDIRECT = re.compile(r"\*|\b(shared_ptr|unique_ptr|weak_ptr|function|reference_wrapper|span|string_view)\b")

HEADER = """/******************************************************************************
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

"""


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def strip_preprocessor(text):
    return "\n".join(line for line in text.splitlines() if not line.lstrip().startswith("#"))


def matching(text, start, open_char, close_char):
    """The index just past the bracket closing the one at start"""
    depth = 0
    for i in range(start, len(text)):
        if text[i] == open_char:
            depth += 1
        elif text[i] == close_char:
            depth -= 1
            if depth == 0:
                return i + 1
    return len(text)


def split_top(text, separator):
    """Splits on a character outside any brackets"""
    parts, depth, current = [], 0, []
    for char in text:
        if char in "<({[":
            depth += 1
        elif char in ">)}]":
            depth -= 1
        if char == separator and depth == 0:
            parts.append("".join(current))
            current = []
        else:
            current.append(char)
    parts.append("".join(current))
    return parts


def statements(body):
    """The body's statements at its own level, each with any braces it holds"""
    result, depth, current = [], 0, []
    i = 0
    while i < len(body):
        char = body[i]
        current.append(char)
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                head = "".join(current).strip()
                # A nested type's or a function's body ends its statement without a semicolon
                if re.match(r"^(enum|struct|class|union)\b", head) or re.search(r"\)\s*(const\s*)?(noexcept\s*)?\{", head):
                    rest = body[i + 1 :].lstrip()
                    if not rest.startswith(";") or re.match(r"^(enum|struct|class|union)\b", head):
                        result.append(head)
                        current = []
        elif char == ";" and depth == 0:
            result.append("".join(current).strip())
            current = []
        i += 1
    return [s for s in result if s and s != ";"]


def member_names(statement):
    statement = statement.strip().rstrip(";").strip()
    statement = re.sub(r"^public:|^private:|^protected:", "", statement).strip()
    if not statement:
        return []
    if re.match(r"^(static|using|typedef|friend|enum|struct|class|union|template|constexpr|virtual|explicit|inline|"
                r"\[\[|~|operator)\b", statement):
        return []
    if statement.startswith("[[") or re.search(r"\boperator\b", statement):
        return []
    # Up to the first initialiser at the top level
    cut, depth = len(statement), 0
    for i, char in enumerate(statement):
        if char in "<([":
            depth += 1
        elif char in ">)]":
            depth -= 1
        elif depth == 0 and char in "{=":
            cut = i
            break
    declaration = statement[:cut].strip()
    # A function (a constructor, an operator, a method) has parentheses outside any template's brackets
    angle = 0
    for char in declaration:
        if char == "<":
            angle += 1
        elif char == ">":
            angle -= 1
        elif char == "(" and angle == 0:
            return []
    declarators = split_top(declaration, ",")
    first = declarators[0].strip()
    match = re.match(r"^(.*?)([A-Za-z_]\w*)\s*(:\s*\d+)?$", first, flags=re.S)
    if match is None:
        return []
    type_text = match.group(1).strip()
    if not type_text or "&" in type_text or match.group(3):
        return []
    names = [match.group(2)]
    for other in declarators[1:]:
        other = other.strip()
        found = re.match(r"^\**([A-Za-z_]\w*)$", other)
        if found is None:
            return []
        names.append(found.group(1))
    if "[" in declaration:
        return []
    return names


def member_types(statement):
    """The members a statement declares, each with its type as written"""
    names = member_names(statement)
    if not names:
        return []
    statement = re.sub(r"^public:|^private:|^protected:", "", statement.strip().rstrip(";").strip()).strip()
    cut, depth = len(statement), 0
    for i, char in enumerate(statement):
        if char in "<([":
            depth += 1
        elif char in ">)]":
            depth -= 1
        elif depth == 0 and char in "{=":
            cut = i
            break
    first = split_top(statement[:cut].strip(), ",")[0].strip()
    type_text = re.sub(r"\s+", " ", re.match(r"^(.*?)([A-Za-z_]\w*)\s*$", first, flags=re.S).group(1).strip())
    return [(name, type_text) for name in names]


def scope_statements(body):
    """A namespace's or a type's statements at its own level: those ending in a semicolon, and namespaces and function
    bodies, which don't"""
    result, depth, current = [], 0, []
    for char in body:
        current.append(char)
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                head = "".join(current).strip()
                head_only = re.sub(r"^(public|private|protected)\s*:", "", head).strip()
                if re.match(r"^(inline\s+)?namespace\b|^extern\b", head_only) or (
                        re.search(r"\)\s*(const\s*)?(noexcept\s*)?(override\s*)?(final\s*)?(->[^{;]*)?\{", head_only)
                        and not re.match(r"^(template\s*<.*?>\s*)?(enum|struct|class|union)\b", head_only, flags=re.S)):
                    result.append(head)
                    current = []
        elif char == ";" and depth == 0:
            result.append("".join(current).strip())
            current = []
    return [statement for statement in result if statement and statement != ";"]


class TypeDef:
    def __init__(self, name, kind, header, scope):
        self.name = name
        self.kind = kind
        self.header = header
        self.scope = scope
        self.members = []
        self.why_not = None
        self.enumerators = None


class Index:
    """The types, enumerations, constants and aliases declared in the game's headers, by their full names"""

    def __init__(self):
        self.types = {}
        self.constants = {}
        self.aliases = {}
        for header in sorted(SOURCES.rglob("*.h")):
            text = strip_preprocessor(strip_comments(header.read_text(encoding="utf-8", errors="replace")))
            self.read_scope(text, [], header, None)

    def read_scope(self, body, scope, header, owner):
        for statement in scope_statements(body):
            bare = re.sub(r"^((public|private|protected)\s*:\s*)+", "", statement).strip()
            namespace = re.match(r"^(?:inline\s+)?namespace\s*([\w:]*)\s*\{", bare)
            if namespace is not None:
                names = namespace.group(1).split("::") if namespace.group(1) else ["(anonymous)"]
                inner = bare[namespace.end():bare.rfind("}")]
                self.read_scope(inner, scope + names, header, None)
                continue
            if bare.startswith("extern"):
                brace = bare.find("{")
                if brace >= 0:
                    self.read_scope(bare[brace + 1:bare.rfind("}")], scope, header, owner)
                continue
            declared = re.match(r"^(template\s*<.*?>\s*)?(struct|class|union)\s+(?:\[\[[^\]]*\]\]\s*)?([A-Za-z_]\w*)\s*"
                                r"(final\s*)?(:[^{;]*)?\{", bare, flags=re.S)
            if declared is not None:
                self.read_type(declared, bare, scope, header, owner)
                continue
            enum = re.match(r"^enum\s+(?:class\s+|struct\s+)?([A-Za-z_]\w*)\s*(:[^{;]*)?\{", bare)
            if enum is not None:
                definition = TypeDef("::".join(scope + [enum.group(1)]), "enum", header, scope)
                definition.enumerators = enumerators(bare[enum.end():bare.rfind("}")])
                self.types[definition.name] = definition
                continue
            constant = re.search(r"\b(k_\w+)\s*=\s*static_cast\s*<\s*(?:std::)?size_t\s*>\s*\(\s*([\w:]+)::_(?:Count|COUNT)\s*\)",
                                 bare)
            if constant is not None:
                self.constants["::".join(scope + [constant.group(1)])] = (constant.group(2), scope)
                continue
            alias = re.match(r"^using\s+([A-Za-z_]\w*)\s*=\s*(.+?)\s*;?$", bare, flags=re.S)
            if alias is not None:
                self.aliases["::".join(scope + [alias.group(1)])] = (alias.group(2), scope)
                continue
            if owner is not None:
                owner.members.extend(member_types(statement))

    def read_type(self, declared, text, scope, header, owner):
        name = declared.group(3)
        kind = declared.group(2)
        definition = TypeDef("::".join(scope + [name]), kind, header, scope)
        open_brace = declared.end() - 1
        body = text[open_brace + 1:matching(text, open_brace, "{", "}") - 1]
        if "(anonymous)" in scope:
            definition.why_not = "in an anonymous namespace"
        elif owner is not None and owner.kind != "struct":
            definition.why_not = f"inside {owner.kind} {owner.name}"
        elif owner is not None and owner.why_not is not None:
            definition.why_not = f"inside {owner.name}, which isn't registered"
        elif declared.group(1):
            definition.why_not = "a template"
        elif kind != "struct":
            definition.why_not = f"a {kind}" + (" (its data is private)" if kind == "class" else "")
        elif declared.group(5):
            definition.why_not = "derived from another type"
        elif re.search(r"(^|[;{}\s])(private|protected)\s*:", body):
            definition.why_not = "it has private data"
        self.types[definition.name] = definition
        self.read_scope(body, scope + [name], header, definition)

    def resolve(self, name, scope, seen=None):
        """A type's full name, looked up from a scope as the compiler does: the innermost scope first"""
        name = name.lstrip(":")
        seen = set() if seen is None else seen
        for depth in range(len(scope), -1, -1):
            candidate = "::".join(scope[:depth] + [name])
            if candidate in self.types:
                return candidate
            if candidate in self.aliases and candidate not in seen:
                seen.add(candidate)
                target, target_scope = self.aliases[candidate]
                target = target.strip()
                if re.fullmatch(r"(::)?[\w:]+", target):
                    return self.resolve(target, target_scope, seen)
                return None
        return None

    def enum_of_size(self, size, scope):
        """The enumeration a list's size counts, when it is one's count"""
        counted = re.fullmatch(r"static_cast\s*<\s*(?:std::)?size_t\s*>\s*\(\s*([\w:]+)::_(?:Count|COUNT)\s*\)", size)
        if counted is not None:
            found = self.resolve(counted.group(1), scope)
        elif re.fullmatch(r"[\w:]*k_\w+", size):
            found = None
            for depth in range(len(scope), -1, -1):
                candidate = "::".join(scope[:depth] + [size.lstrip(":")])
                if candidate in self.constants:
                    enum_name, enum_scope = self.constants[candidate]
                    found = self.resolve(enum_name, enum_scope)
                    break
        else:
            found = None
        if found is None or self.types[found].enumerators is None:
            return None
        return found

    def index_enums(self, type_text, scope):
        """For a list (of lists) indexed by enumerations, the enumeration of each level, None for one by number"""
        levels = []
        text = type_text.strip()
        while True:
            alias = self.alias_of(text, scope)
            if alias is not None:
                text, scope = alias[0].strip(), alias[1]
            array = re.fullmatch(r"(?:const\s+)?std::array\s*<(.*)>", text, flags=re.S)
            if array is None:
                break
            parts = split_top(array.group(1), ",")
            if len(parts) != 2:
                break
            levels.append(self.enum_of_size(parts[1].strip(), scope))
            text = parts[0].strip()
        return levels if any(level is not None for level in levels) else []

    def held_types(self, type_text, scope):
        """The game's types a member's type holds, directly or in lists, maps and options; not through pointers"""
        if INDIRECT.search(type_text):
            return []
        found = []
        for token in re.findall(r"(?<![\w:])((?:::)?(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*)", type_text):
            if token.lstrip(":").startswith(EXTERNAL) or token in ("const", "unsigned", "signed", "volatile"):
                continue
            resolved = self.resolve(token, scope)
            if resolved is not None and self.types[resolved].kind != "enum":
                found.append(resolved)
            alias = self.alias_of(token, scope) if resolved is None else None
            if alias is not None:
                found.extend(self.held_types(alias[0], alias[1]))
        return found

    def alias_of(self, name, scope):
        """What an alias of a type made of others (using Slots = std::array<Slot, 8>) stands for, and where"""
        if not re.fullmatch(r"(::)?[\w:]+", name):
            return None
        for depth in range(len(scope), -1, -1):
            candidate = "::".join(scope[:depth] + [name.lstrip(":")])
            if candidate in self.types:
                return None
            if candidate in self.aliases:
                return self.aliases[candidate]
        return None


def enumerators(body):
    """An enumeration's names in order, when they count up from 0 (a sentinel starting with _ ends them); None when
    their values are anything else"""
    names = []
    for item in split_top(body, ","):
        item = item.strip()
        if not item:
            continue
        found = re.fullmatch(r"([A-Za-z_]\w*)\s*(?:=\s*(.+))?", item, flags=re.S)
        if found is None:
            return None
        if found.group(2) is not None:
            try:
                if int(found.group(2).strip().rstrip("uU"), 0) != len(names):
                    return None
            except ValueError:
                return None
        names.append(found.group(1))
    return names


def names_array(enum_name):
    words = [word for part in enum_name.split("::")[1:] for word in part.split("_") if word]
    return "k_" + "".join(word[:1].upper() + word[1:] for word in words) + "Names"


def field_line(owner, member, levels):
    """A field's registration, with the names of the enumerations indexing it"""
    if not levels:
        return f'\n\t    .Field<&{owner}::{member}>("{member}")'
    names = ", ".join(names_array(level) if level is not None else "std::span<const std::string_view> {}"
                      for level in levels)
    return f'\n\t    .Field<&{owner}::{member}>("{member}", {{{names}}})'


def components_of(path):
    text = strip_preprocessor(strip_comments(path.read_text(encoding="utf-8")))
    found = []
    for namespace in re.finditer(r"namespace\s+" + re.escape(NAMESPACE) + r"\s*\{", text):
        start = namespace.end() - 1
        end = matching(text, start, "{", "}")
        body = text[start + 1 : end - 1]
        i = 0
        while i < len(body):
            match = re.compile(r"(template\s*<[^>]*>\s*)?\bstruct\s+([A-Za-z_]\w*)\s*(final\s*)?(:[^{;]*)?([{;])").search(body, i)
            if match is None:
                break
            if match.group(5) == ";":
                i = match.end()
                continue
            open_brace = match.end() - 1
            close = matching(body, open_brace, "{", "}")
            # Only structs at the namespace's own level
            before = body[:match.start()]
            if before.count("{") == before.count("}") and match.group(1) is None and not match.group(4):
                name = match.group(2)
                if name not in SKIPPED:
                    members = []
                    for statement in statements(body[open_brace + 1 : close - 1]):
                        members.extend(member_names(statement))
                    found.append((name, members))
            i = close
    return found


def main():
    check = "--check" in sys.argv[1:]
    report = "--report" in sys.argv[1:]
    # By name, as clang-format sorts the includes: a Windows path sorts without regard to case
    headers = sorted(COMPONENTS.glob("*.h"), key=lambda header: header.name)
    index = Index()
    component_scope = NAMESPACE.split("::")
    components = [(header, name, members) for header in headers for name, members in components_of(header)]
    component_names = {"::".join(component_scope + [name]) for _, name, _ in components}

    # The values the components' fields hold, and those theirs hold in turn
    values, refused, enums = {}, {}, set()
    pending = []

    def follow(type_text, scope, holder):
        for held in index.held_types(type_text, scope):
            pending.append((held, holder))
        for level in index.index_enums(type_text, scope):
            if level is not None:
                enums.add(level)

    def registered_members(definition, members):
        result = []
        for member in members:
            type_text = dict(definition.members).get(member, "")
            result.append((member, index.index_enums(type_text, definition.scope + [definition.name.split("::")[-1]])))
        return result

    component_fields = {}
    for header, name, members in components:
        full = "::".join(component_scope + [name])
        definition = index.types.get(full)
        if definition is None:
            component_fields[name] = [(member, []) for member in members]
            continue
        for member, type_text in definition.members:
            if member in members:
                follow(type_text, component_scope + [name], full)
        component_fields[name] = registered_members(definition, members)
    while pending:
        held, holder = pending.pop()
        if held in component_names or held in values or held in refused:
            continue
        if held in BY_HAND:
            refused[held] = (f"registered by hand in src/Inspector/ComponentFieldsByHand.cpp", holder)
            for given in BY_HAND[held]:
                pending.append((given, held))
            continue
        definition = index.types[held]
        if definition.why_not is not None:
            refused[held] = (definition.why_not, holder)
            continue
        own_scope = definition.scope + [held.split("::")[-1]]
        members = [member for member, _ in definition.members]
        if not members:
            refused[held] = ("it has no data members", holder)
            continue
        values[held] = definition
        for _, type_text in definition.members:
            follow(type_text, own_scope, held)

    if report:
        for name, (why, holder) in sorted(refused.items()):
            print(f"{name}: {why} (held by {holder})")
        return 0

    includes = {"ComponentReflection.h"} | {f"ECS/Components/{header.name}" for header in headers}
    for name in list(values) + sorted(enums):
        includes.add(index.types[name].header.relative_to(SOURCES).as_posix())
    lines = [HEADER]
    for include in sorted(includes):
        lines.append(f'#include "{include}"\n')
    lines.append("\nnamespace components = openblack::ecs::components;\n\n")
    if enums:
        lines.append("namespace\n{\n\n")
        lines.append("// The names of the enumerations indexing lists, which the lists' elements are read and set by\n")
        for enum in sorted(enums):
            names = [name for name in index.types[enum].enumerators if not name.startswith("_")]
            quoted = ", ".join(f'"{name}"' for name in names)
            lines.append(f"constexpr std::array<std::string_view, {len(names)}> {names_array(enum)} {{{quoted}}};\n")
            if len(names) != len(index.types[enum].enumerators):
                lines.append(f"static_assert(static_cast<size_t>({enum}::{index.types[enum].enumerators[-1]}) == "
                             f"{names_array(enum)}.size());\n")
        lines.append("\n} // namespace\n\n")
    lines.append("void openblack::inspector::reflection::RegisterComponentFields(entt::meta_ctx& context)\n{\n")
    count = 0
    for header, name, members in components:
        count += 1
        if not members:
            lines.append(f"\tReflect<components::{name}> {{context}};\n")
            continue
        lines.append(f"\tReflect<components::{name}>(context)")
        for member, levels in component_fields[name]:
            lines.append(field_line(f"components::{name}", member, levels))
        lines.append(";\n")
    if values:
        lines.append("\n\t// The values the components' fields hold\n")
    for name in sorted(values):
        definition = values[name]
        lines.append(f"\tReflect<{name}>(context, ValueOnly {{}})")
        for member, levels in registered_members(definition, [member for member, _ in definition.members]):
            lines.append(field_line(name, member, levels))
        lines.append(";\n")
    lines.append("}\n")
    if check:
        def squeezed(text):
            return re.sub(r"\s+", "", text)
        current = OUTPUT.read_text(encoding="utf-8") if OUTPUT.exists() else ""
        if squeezed(current) != squeezed("".join(lines)):
            print(f"{OUTPUT.relative_to(ROOT)} is out of date with src/ECS/Components: run "
                  f"python tools/inspector/generate_component_fields.py", file=sys.stderr)
            return 1
        print(f"{OUTPUT.relative_to(ROOT)} is up to date ({count} components, {len(values)} values)")
        return 0
    OUTPUT.write_text("".join(lines), encoding="utf-8", newline="\n")
    # Formatted as the rest of the code is, when clang-format is there
    clang_format = shutil.which("clang-format")
    if clang_format is not None:
        subprocess.run([clang_format, "-i", str(OUTPUT)], check=True)
    print(f"{count} components and {len(values)} values written to {OUTPUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
