"""Tests of local_act.py. The workflow files come from a clone of bwgame-service when this computer has one."""
import os
import subprocess
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import local_act  # noqa: E402

SERVICE = os.environ.get("OB_SERVICE_CLONE", "C:/projects/bwgame-service")
SHA = "bc6727302e73e5b2ca715118fec7792f0c224ab7"


def workflow(name):
    done = subprocess.run(["git", "-C", SERVICE, "show", "origin/master:.github/workflows/" + name], capture_output=True,
                          text=True, encoding="utf-8")
    return done.stdout if done.returncode == 0 else None


class LocalActTest(unittest.TestCase):
    def setUp(self):
        self.build = workflow("build-game.yml")
        self.deploy = workflow("deploy-game.yml")
        if self.build is None or self.deploy is None:
            self.skipTest("no clone of bwgame-service with origin/master")

    def test_the_build_keeps_the_tar_and_the_debug_file_on_the_host(self):
        text = local_act.patch_build_game(self.build)
        self.assertNotIn("uses: actions/upload-artifact", text)
        self.assertIn("/out/web-openblack.debug.wasm", text)
        self.assertIn('"/out/${{ inputs.kind }}-build.tar"', text)
        self.assertNotIn("Add swap", text)

    def test_the_local_deploy_reads_no_build_job_and_takes_the_debug_file(self):
        text = local_act.make_local_deploy(self.deploy, SHA)
        deploy_job = text.split("\n  deploy:\n", 1)[1]
        self.assertNotIn("needs.build-", deploy_job)
        self.assertNotIn("uses: actions/download-artifact", text)
        self.assertIn("/out/web-openblack.debug.wasm", text)
        self.assertIn("steps.host.outputs.found == 'true'", text)
        self.assertIn(f"WEB_SHA: {SHA}", text)

    def test_the_patches_are_stable_when_the_file_changes_in_an_unexpected_way(self):
        with self.assertRaises(ValueError):
            local_act.patch_build_game(self.build.replace("Add swap", "Add memory"))
        with self.assertRaises(ValueError):
            local_act.make_local_deploy(self.deploy.replace("download-artifact", "fetch-artifact"), SHA)


class CommitTest(unittest.TestCase):
    def test_a_word_that_is_not_a_commit_is_refused(self):
        with self.assertRaises(ValueError):
            local_act.make_local_deploy("name: Deploy game\n", "main")


if __name__ == "__main__":
    unittest.main()
