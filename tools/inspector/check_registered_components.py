#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Fails if the game can put a component on an entity that the reflection does not know.

A component with no registration is left out of the sync checksum and out of the saves, so a change in it between two
computers is never seen. The check reads the sources: every type that the game assigns to an entity (Assign,
AssignOrReplace, emplace, emplace_or_replace, get_or_emplace, storage) must have a registration (Reflect<Type> in
src/ECS/Reflection), or be on the short list of types that are not components. It is the second check beside
generate_component_fields.py --check, which is about the structs in src/ECS/Components. The game checks the same at
run time too: it logs "unregistered component storage: <names>" and the tools fail on it.

    python tools/inspector/check_registered_components.py          # exit code 1 and the names if one is missing
    python tools/inspector/check_registered_components.py --test   # the checks of this script
"""

import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCES = (ROOT / "src", ROOT / "apps")
REFLECTION = ROOT / "src" / "ECS" / "Reflection"

# Types that these calls take but that are not components on entities: values in the registry's context, and
# the services that the locator holds (its Emplace takes the same word)
NOT_COMPONENTS = {
    "LandShadowCasterChanges",
    "RegistryContext",
    "VillageLightFlames",
    "MapScriptGlobals",
    # The name of a template parameter, and the entity storage of the registry
    "Component",
    "entity",
    # An alias of the component Mesh
    "MeshComponent",
    # The rendering services that the locator holds
    "RenderingSystemTemple",
}
# Files whose calls are not about entity components: the service locator (its game part and its part with the devices
# that use no hardware too) and the template that the registry itself uses
SKIPPED_FILES = {
    "src/Locator.cpp",
    "src/LocatorClient.cpp",
    "src/LocatorHeadless.cpp",
    "src/ECS/Registry.h",
    "src/ECS/Reflection/ComponentReflection.h",
    "src/ECS/Reflection/BinaryArchive.h",
}

REGISTERED = re.compile(r"Reflect\w*<\s*([A-Za-z_][\w:]*)\s*>\s*[({]")
ASSIGNED = re.compile(r"(?:\.|->|\b)(?:Assign|AssignOrReplace|emplace|emplace_or_replace|get_or_emplace|storage)"
                      r"<\s*([A-Za-z_][\w:]*)\s*>\s*\(")
# A call on the registry's context is not a component, nor is a call on a locator or a context object
CONTEXT_CALL = re.compile(r"(ctx\(\)|context|Context\(\))\s*\.\s*$")


def last_name(name):
    return name.split("::")[-1]


def registered_names(root=REFLECTION):
    names = set()
    for path in root.rglob("*.cpp"):
        names.update(last_name(match.group(1)) for match in REGISTERED.finditer(path.read_text(errors="ignore")))
    return names


def assigned_names(roots=SOURCES, base=ROOT):
    """Each type that is assigned to an entity, with the first file that does it"""
    found = {}
    for root in roots:
        for path in sorted([*root.rglob("*.cpp"), *root.rglob("*.h")]):
            relative = path.relative_to(base).as_posix()
            if relative in SKIPPED_FILES:
                continue
            text = path.read_text(errors="ignore")
            for match in ASSIGNED.finditer(text):
                if CONTEXT_CALL.search(text[max(0, match.start() - 24):match.start()]):
                    continue
                found.setdefault(last_name(match.group(1)), relative)
    return found


def missing(registered, assigned):
    """The assigned types with no registration, with a file that assigns each"""
    return {name: where for name, where in sorted(assigned.items())
            if name not in registered and name not in NOT_COMPONENTS and not name.endswith("System")}


class Tests(unittest.TestCase):
    def test_a_registered_type_is_not_missing(self):
        self.assertEqual(missing({"Abode"}, {"Abode": "a.cpp"}), {})

    def test_an_unregistered_type_is_missing(self):
        self.assertEqual(missing({"Abode"}, {"Abode": "a.cpp", "NewThing": "b.cpp"}), {"NewThing": "b.cpp"})

    def test_a_value_of_the_context_is_not_a_component(self):
        self.assertEqual(missing(set(), {"RegistryContext": "a.cpp"}), {})

    def test_the_patterns_find_the_calls(self):
        text = "registry.Assign<ecs::components::Foo>(e); registry.storage<Bar>(); x.ctx().emplace<Baz>();"
        found = [last_name(match.group(1)) for match in ASSIGNED.finditer(text)]
        self.assertEqual(found, ["Foo", "Bar", "Baz"])
        self.assertTrue(CONTEXT_CALL.search("x.ctx()."))

    def test_the_game_has_no_missing_component(self):
        self.assertEqual(missing(registered_names(), assigned_names()), {})


def main(argv):
    if "--test" in argv:
        return 0 if unittest.main(argv=[argv[0]], exit=False).result.wasSuccessful() else 1
    absent = missing(registered_names(), assigned_names())
    for name, where in absent.items():
        print(f"unregistered component storage: {name} (assigned in {where}); register it with the reflection")
    return 1 if absent else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
