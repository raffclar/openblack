import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import write_settings  # noqa: E402


class WriteSettingsTest(unittest.TestCase):
    def read(self, path):
        with open(path, encoding="utf-8") as handle:
            return json.load(handle)

    def test_file_is_inside_the_profile_folder(self):
        with tempfile.TemporaryDirectory() as root:
            folder = os.path.join(root, "profile")
            path = write_settings.write_settings(folder, {"vsync": False})
            self.assertEqual(path, os.path.join(folder, "settings.json"))
            self.assertEqual(self.read(path), {"vsync": False})

    def test_custom_detail_selects_the_custom_level(self):
        with tempfile.TemporaryDirectory() as root:
            path = write_settings.write_settings(root, {"custom_detail": {"clouds": False}})
            self.assertEqual(self.read(path)["detail_level"], 5)

    def test_given_level_is_kept(self):
        with tempfile.TemporaryDirectory() as root:
            path = write_settings.write_settings(root, {"detail_level": 2, "custom_detail": {}})
            self.assertEqual(self.read(path)["detail_level"], 2)


if __name__ == "__main__":
    unittest.main()
