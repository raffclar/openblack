#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Tests of the comparison of two saved worlds: the blocks, the entities block and the systems block"""

import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import save_diff  # noqa: E402


def u32(value):
    return struct.pack("<I", value)


def name(text):
    data = text.encode()
    return u32(len(data)) + data


def entities_block(entities, components):
    """An entities block: components is [(name, [fields], [(entity, [field bytes])])]"""
    data = b"OBEW" + u32(1) + u32(len(entities)) + u32(len(entities)) + b"".join(u32(e) for e in entities)
    data += u32(len(components))
    for component, fields, rows in components:
        block = u32(len(fields)) + b"".join(name(f) for f in fields) + u32(len(rows))
        for entity, values in rows:
            block += u32(entity) + b"".join(u32(len(v)) + v for v in values)
        data += name(component) + u32(len(block)) + block
    return data


def sections_block(sections):
    return u32(len(sections)) + b"".join(name(n) + u32(len(b)) + b for n, b in sections)


def world(blocks, attempts=0):
    data = b"OBSV" + u32(4) + u32(attempts) + u32(len(blocks))
    for tag, block in blocks:
        data += tag.encode() + u32(len(block)) + block
    return data


class SaveDiffTests(unittest.TestCase):
    def test_same_files_give_nothing(self):
        data = world([("TIME", b"\x01\x02"), ("SYST", sections_block([("fire", b"\x00")]))])
        self.assertEqual(save_diff.compare_files(data, data), {})

    def test_field_that_differs_is_named_with_its_entity(self):
        first = entities_block([1, 2], [("Transform", ["position", "scale"], [(1, [b"\x00" * 4, b"\x01"]),
                                                                              (2, [b"\x00" * 4, b"\x01"])])])
        second = entities_block([1, 2], [("Transform", ["position", "scale"], [(1, [b"\x00" * 4, b"\x01"]),
                                                                               (2, [b"\x00" * 4, b"\x02"])])])
        result = save_diff.compare_files(world([("ECSW", first)]), world([("ECSW", second)]))
        values = result["ECSW"]["detail"]["components"]["Transform"]["values"]
        self.assertEqual(list(values), ["scale"])
        self.assertEqual(values["scale"]["entities"], 1)
        self.assertEqual(values["scale"]["examples"][0]["entity"], "2v0")

    def test_rows_in_another_order_are_told(self):
        rows = [(1, [b"\x00"]), (2, [b"\x00"])]
        first = entities_block([1, 2], [("Tag", ["value"], rows)])
        second = entities_block([1, 2], [("Tag", ["value"], list(reversed(rows)))])
        result = save_diff.compare_files(world([("ECSW", first)]), world([("ECSW", second)]))
        self.assertTrue(result["ECSW"]["detail"]["components"]["Tag"]["rows"]["same_set_other_order"])

    def test_section_that_differs_is_named(self):
        first = sections_block([("fire", b"\x00\x00"), ("snow", b"\x01")])
        second = sections_block([("fire", b"\x00\x00\x05"), ("snow", b"\x01")])
        result = save_diff.compare_files(world([("SYST", first)]), world([("SYST", second)]))
        detail = result["SYST"]["detail"]
        self.assertEqual(list(detail), ["fire"])
        self.assertEqual(detail["fire"]["sizes"], [2, 3])

    def test_other_block_gives_the_first_place(self):
        result = save_diff.compare_files(world([("TIME", b"\x00\x00\x00\x00\x07")]),
                                         world([("TIME", b"\x00\x00\x00\x00\x08")]))
        self.assertEqual(result["TIME"]["bytes"]["first"], 4)

    def test_attempts_differ_in_the_head(self):
        result = save_diff.compare_files(world([], attempts=1), world([], attempts=0))
        self.assertIn("head", result)

    def test_parts_name_the_components_sections_and_blocks(self):
        first = entities_block([1], [("Tag", ["value"], [(1, [b"\x00"])])])
        second = entities_block([1], [("Tag", ["value"], [(1, [b"\x01"])])])
        result = save_diff.compare_files(
            world([("ECSW", first), ("SYST", sections_block([("fire", b"\x00")])), ("TIME", b"\x00")]),
            world([("ECSW", second), ("SYST", sections_block([("fire", b"\x01")])), ("TIME", b"\x01")]))
        self.assertEqual(save_diff.differing_parts(result), ["ECSW:Tag", "SYST:fire", "TIME"])
        self.assertEqual(save_diff.differing_parts({}), [])

    def test_the_start_of_a_game_explains_the_alignment_and_the_towns_at_the_start(self):
        first = entities_block([1], [("Alignment", ["value"], [(1, [b"\x00\x00\x80\x3f"])])])
        second = entities_block([1], [("Alignment", ["value"], [(1, [b"\x00\x00\x00\x00"])])])
        context_a = b"\x07" * 8 + b"\x00\x00\x00\x00"
        context_b = b"\x07" * 8 + b"\x01\x00\x00\x00"
        result = save_diff.compare_files(world([("ECSW", first), ("RCTX", context_a)]),
                                         world([("ECSW", second), ("RCTX", context_b)]))
        self.assertEqual(save_diff.not_game_start(result), [])

    def test_the_start_of_a_game_does_not_explain_other_values(self):
        first = entities_block([1], [("Transform", ["position"], [(1, [b"\x00"])])])
        second = entities_block([1], [("Transform", ["position"], [(1, [b"\x01"])])])
        result = save_diff.compare_files(world([("ECSW", first), ("RCTX", b"\x00" * 12), ("TIME", b"\x00")]),
                                         world([("ECSW", second), ("RCTX", b"\x01" + b"\x00" * 11), ("TIME", b"\x01")]))
        self.assertEqual(save_diff.not_game_start(result), ["ECSW:Transform", "RCTX", "TIME"])

    def test_the_sounds_that_go_at_a_load_are_the_start_of_a_game(self):
        tag = [(2, [b"\x05"])]
        first = entities_block([1, 2], [("SoundTag", ["sound"], tag), ("Tag", ["value"], [(1, [b"\x00"])])])
        second = entities_block([1, 3], [("Tag", ["value"], [(1, [b"\x00"])])])
        result = save_diff.compare_files(world([("ECSW", first)]), world([("ECSW", second)]))
        self.assertEqual(save_diff.not_game_start(result), [])
        # A new entity of another kind is not a sound that went
        third = entities_block([1, 3], [("SoundTag", ["sound"], tag),
                                        ("Tag", ["value"], [(1, [b"\x00"]), (3, [b"\x00"])])])
        result = save_diff.compare_files(world([("ECSW", first)]), world([("ECSW", third)]))
        self.assertIn("ECSW:Tag", save_diff.not_game_start(result))

    def test_the_start_of_a_game_explains_the_mind_of_a_computer_god(self):
        first = entities_block([1], [("ComputerPlayer", ["mind", "speed"], [(1, [b"\x01", b"\x00"])])])
        second = entities_block([1], [("ComputerPlayer", ["mind", "speed"], [(1, [b"\x02", b"\x00"])])])
        result = save_diff.compare_files(world([("ECSW", first)]), world([("ECSW", second)]))
        self.assertEqual(save_diff.not_game_start(result), [])
        third = entities_block([1], [("ComputerPlayer", ["mind", "speed"], [(1, [b"\x01", b"\x01"])])])
        result = save_diff.compare_files(world([("ECSW", first)]), world([("ECSW", third)]))
        self.assertEqual(save_diff.not_game_start(result), ["ECSW:ComputerPlayer"])

    def test_the_end_of_a_temple_visit_explains_only_the_temple_sections(self):
        first = sections_block([("temple", b"\x01"), ("helpText", b"\x01"), ("fire", b"\x00")])
        second = sections_block([("temple", b"\x00"), ("helpText", b"\x00"), ("fire", b"\x00")])
        result = save_diff.compare_files(world([("SYST", first)]), world([("SYST", second)]))
        self.assertEqual(save_diff.not_temple_visit(result), [])
        third = sections_block([("temple", b"\x00"), ("helpText", b"\x01"), ("fire", b"\x01")])
        result = save_diff.compare_files(world([("SYST", first)]), world([("SYST", third)]))
        self.assertEqual(save_diff.not_temple_visit(result), ["SYST:fire"])

    def test_the_count_of_the_changed_altitudes_comes_from_the_altitudes_block(self):
        self.assertEqual(save_diff.changed_altitudes(world([("LALT", u32(3) + b"\x00" * 15)])), 3)
        self.assertEqual(save_diff.changed_altitudes(world([("TIME", b"\x00")])), 0)

    def test_the_land_of_the_file_explains_the_creature_records_only_when_the_land_changed(self):
        result = save_diff.compare_files(world([("OBJS", b"\x00")]), world([("OBJS", b"\x01")]))
        self.assertEqual(save_diff.not_game_start(result), ["OBJS"])
        self.assertEqual(save_diff.not_game_start(result, land_from_file=True), [])


if __name__ == "__main__":
    unittest.main()
