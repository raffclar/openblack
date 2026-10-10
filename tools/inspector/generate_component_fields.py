#!/usr/bin/env python3
"""Writes src/Inspector/ComponentFields.cpp: every component of src/ECS/Components and its data members registered
with the inspector's reflection, so that the inspector writes them out as JSON and sets them by name.

Run it again after adding or changing a component (with clang-format on the PATH, it formats the file too):

    python tools/inspector/generate_component_fields.py

With --check it writes nothing and fails if the file is out of date (spaces aside), as ctest's
test_inspector_component_fields does, so that a branch adding a component without registering it is told so.

Only plain structs in openblack::ecs::components are read. Members that are functions, static, references or C arrays
are left out, as are templates. A struct with no members is still registered, so that it can be added to an entity.
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

HEADER = """/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Written by tools/inspector/generate_component_fields.py: run it again rather than editing this file.

#include "ComponentReflection.h"

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
    if statement.startswith("[["):
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
    # By name as the bytes sort, as clang-format orders the includes: Windows paths sort without case
    headers = sorted(COMPONENTS.glob("*.h"), key=lambda path: path.name)
    lines = [HEADER]
    for header in headers:
        lines.append(f'#include "ECS/Components/{header.name}"\n')
    lines.append("\nnamespace components = openblack::ecs::components;\n\n")
    lines.append("void openblack::inspector::reflection::RegisterComponentFields(entt::meta_ctx& context)\n{\n")
    count = 0
    for header in headers:
        for name, members in components_of(header):
            count += 1
            if not members:
                lines.append(f"\tReflect<components::{name}> {{context}};\n")
                continue
            lines.append(f"\tReflect<components::{name}>(context)")
            for member in members:
                lines.append(f'\n\t    .Field<&components::{name}::{member}>("{member}")')
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
        print(f"{OUTPUT.relative_to(ROOT)} is up to date ({count} components)")
        return 0
    OUTPUT.write_text("".join(lines), encoding="utf-8", newline="\n")
    # Formatted as the rest of the code is, when clang-format is there
    clang_format = shutil.which("clang-format")
    if clang_format is not None:
        subprocess.run([clang_format, "-i", str(OUTPUT)], check=True)
    print(f"{count} components written to {OUTPUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
