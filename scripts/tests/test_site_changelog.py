"""Tests for the changelog part of scripts/site/build.py: each version shows only its own release text.
Run: python3 -I -m unittest discover -s scripts/tests"""
import importlib.util
import os
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("sitebuild", os.path.join(HERE, "..", "site", "build.py"))
sitebuild = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sitebuild)

HEAD = "**TI Revive 0.9.0** - Total Integration for the Access Virus TI.\n\n"


class Summary(unittest.TestCase):
    def test_older_texts_repeat_earlier_versions(self):
        body = HEAD + "**New in 0.9.0: drops.**\n\n- Drop a file.\n\n**New in 0.8.9: knobs.**\n\n- Knobs.\n"
        self.assertEqual(sitebuild.summary({"tag_name": "v0.9.0", "body": body}), "- Drop a file.")

    def test_current_texts_end_with_links_and_the_notice(self):
        # since 0.8.35 a release text holds only its own version, then a pointer to earlier ones and the notice
        body = (HEAD + "**New in 0.9.0: drops.**\n\n- Drop a file.\n\nTested on a Virus TI2 Desktop.\n\n"
                "Earlier versions: their own release pages, and the [changelog](x).\n\n"
                "Not affiliated with Access Music or Kemper. Contains no Access software.\n")
        self.assertEqual(sitebuild.summary({"tag_name": "v0.9.0", "body": body}),
                         "- Drop a file.\n\nTested on a Virus TI2 Desktop.")


class LoadReleases(unittest.TestCase):  # audit 2026-11-28 #8: more than 100 releases need every page
    def test_all_pages_flattened(self):
        import json
        import subprocess
        from unittest import mock
        pages = [[{"tag_name": "v%d" % i} for i in range(100)], [{"tag_name": "v100"}]]
        done = subprocess.CompletedProcess([], 0, stdout=json.dumps(pages))
        with mock.patch.object(sitebuild.subprocess, "run", return_value=done) as run:
            rels = sitebuild.load_releases(None)  # a stand-in gh: nothing reaches GitHub
        self.assertIn("--paginate", run.call_args.args[0])
        self.assertIn("--slurp", run.call_args.args[0])
        self.assertEqual([r["tag_name"] for r in rels][-2:], ["v99", "v100"])
        self.assertEqual(len(rels), 101)

    def test_saved_file_flat_or_paged(self):
        import json
        import tempfile
        for data in ([{"tag_name": "v1"}], [[{"tag_name": "v1"}]], []):
            with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
                json.dump(data, f)
            try:
                self.assertEqual(sitebuild.load_releases(f.name), [{"tag_name": "v1"}] if data else [])
            finally:
                os.remove(f.name)


if __name__ == "__main__":
    unittest.main()
