import json
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import tidy_gate  # noqa: E402

DIFF = """diff --git a/src/A.cpp b/src/A.cpp
index 1..2 100644
--- a/src/A.cpp
+++ b/src/A.cpp
@@ -3,0 +4,2 @@ void f()
+  int a;
+  int b;
@@ -10 +12 @@ void g()
-  old
+  new
@@ -20,2 +21,0 @@ void h()
-  gone
-  gone
diff --git a/src/B.h b/src/B.h
--- /dev/null
+++ b/src/B.h
@@ -0,0 +1,3 @@
+one
+two
+three
diff --git a/src/C.cpp b/src/C.cpp
--- a/src/C.cpp
+++ /dev/null
@@ -1,2 +0,0 @@
-x
-y
"""


class DiffToLines(unittest.TestCase):
    def test_added_and_changed_lines_are_ranges(self):
        changed = tidy_gate.parse_diff_lines(DIFF)
        self.assertEqual(changed["src/A.cpp"], [(4, 5), (12, 12)])

    def test_a_pure_deletion_has_no_line(self):
        changed = tidy_gate.parse_diff_lines(DIFF)
        self.assertNotIn((21, 20), changed["src/A.cpp"])
        self.assertEqual(len(changed["src/A.cpp"]), 2)

    def test_a_new_file_is_all_lines(self):
        self.assertEqual(tidy_gate.parse_diff_lines(DIFF)["src/B.h"], [(1, 3)])

    def test_a_deleted_file_is_not_listed(self):
        self.assertNotIn("src/C.cpp", tidy_gate.parse_diff_lines(DIFF))

    def test_crlf_diff(self):
        self.assertEqual(tidy_gate.parse_diff_lines(DIFF.replace("\n", "\r\n"))["src/B.h"], [(1, 3)])

    def test_line_filter_json(self):
        text = tidy_gate.line_filter({"src/A.cpp": [(4, 5), (12, 12)]})
        expected = [{"name": os.path.normpath("src/A.cpp"), "lines": [[4, 5], [12, 12]]}]
        self.assertEqual(json.loads(text), expected)
        self.assertNotIn(" ", text)

    def test_checked_files(self):
        self.assertTrue(tidy_gate.is_checked_file("src/Game.cpp"))
        self.assertTrue(tidy_gate.is_checked_file("components/cam/include/Cam.h"))
        self.assertFalse(tidy_gate.is_checked_file("docs/x.md"))
        self.assertFalse(tidy_gate.is_checked_file("externals/a.cpp"))
        self.assertFalse(tidy_gate.is_checked_file("src/shader.sc"))


class Output(unittest.TestCase):
    ROOT = os.path.abspath("work")

    def line(self, path, number, check="misc-x"):
        return f"{path}:{number}:5: warning: a finding [{check}]"

    def test_only_changed_lines_count(self):
        changed = {"src/A.cpp": [(4, 5)]}
        text = "\n".join([self.line("src/A.cpp", 4), self.line("src/A.cpp", 9), self.line("src/Z.cpp", 4)])
        found = tidy_gate.parse_tidy_output(text, changed, self.ROOT)
        self.assertEqual(found, [("src/A.cpp", 4, "misc-x", "a finding")])

    def test_absolute_path_and_notes(self):
        changed = {"src/A.cpp": [(4, 5)]}
        absolute = os.path.join(self.ROOT, "src", "A.cpp")
        text = self.line(absolute, 5) + "\n" + f"{absolute}:5:1: note: see here"
        found = tidy_gate.parse_tidy_output(text, changed, self.ROOT)
        self.assertEqual([f[1] for f in found], [5])

    def test_duplicates_collapse(self):
        changed = {"src/A.h": [(1, 9)]}
        text = "\n".join([self.line("src/A.h", 2)] * 3)
        self.assertEqual(len(tidy_gate.parse_tidy_output(text, changed, self.ROOT)), 1)


class Nolint(unittest.TestCase):
    def check(self, lines, ranges=None):
        return tidy_gate.nolint_findings(lines, ranges or [(1, len(lines))])

    def test_name_and_reason_on_the_same_line_pass(self):
        self.assertEqual(self.check(["f(); // NOLINT(misc-x) the API needs it"]), [])

    def test_reason_before_the_marker_passes(self):
        self.assertEqual(self.check(["f(); // the API needs it NOLINT(misc-x)"]), [])

    def test_reason_on_the_line_before_passes(self):
        self.assertEqual(self.check(["// The API needs a raw pointer.", "f(); // NOLINT(misc-x)"]), [])

    def test_nextline_with_reason_before_passes(self):
        lines = ["// A raw pointer is part of the API.", "// NOLINTNEXTLINE(misc-x)", "f();"]
        self.assertEqual(self.check(lines), [])

    def test_no_check_name_fails(self):
        self.assertEqual(len(self.check(["f(); // NOLINT the API needs it"])), 1)

    def test_empty_and_star_names_fail(self):
        self.assertEqual(len(self.check(["f(); // NOLINT() the API needs it"])), 1)
        self.assertEqual(len(self.check(["f(); // NOLINT(*) the API needs it"])), 1)

    def test_no_reason_fails(self):
        found = self.check(["f(); // NOLINT(misc-x)"])
        self.assertEqual([n for n, _ in found], [1])

    def test_another_nolint_line_is_not_a_reason(self):
        found = self.check(["// NOLINT(misc-y)", "f(); // NOLINT(misc-x)"], [(2, 2)])
        self.assertEqual([n for n, _ in found], [2])

    def test_a_line_that_did_not_change_is_ignored(self):
        self.assertEqual(self.check(["f(); // NOLINT", "g();"], [(2, 2)]), [])

    def test_end_needs_nothing(self):
        self.assertEqual(self.check(["// NOLINTEND(misc-x)"]), [])

    def test_begin_needs_name_and_reason(self):
        self.assertEqual(len(self.check(["// NOLINTBEGIN(misc-x)"])), 1)
        self.assertEqual(self.check(["// Generated code, left as it is.", "// NOLINTBEGIN(misc-x)"]), [])

    def test_code_text_is_not_a_comment(self):
        self.assertEqual(self.check(['const char* s = "NOLINT";']), [])


class Header(unittest.TestCase):
    def test_a_header_borrows_the_command_of_a_source_in_its_folder(self):
        root = os.path.abspath("work")
        database = [
            {"file": os.path.join(root, "src", "A.cpp"), "command": "cl a"},
            {"file": os.path.join(root, "src", "Sub", "B.cpp"), "command": "cl b"},
        ]
        picked = tidy_gate.pick_template_entry(database, "src/Sub/B.h", root)
        self.assertEqual(picked["command"], "cl b")
        picked = tidy_gate.pick_template_entry(database, "src/Other/C.h", root)
        self.assertEqual(picked["command"], "cl a")


if __name__ == "__main__":
    unittest.main()
