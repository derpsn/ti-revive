"""Tests for .claude/hooks/github-identity.sh (PreToolUse: commands that reach GitHub or make commits run only as
derpsn). gh is a stand-in on PATH that answers from environment variables; git is the real one on a temp repo.
Run: python3 -I -m unittest discover -s scripts/tests"""
import json
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
HOOK = os.path.join(HERE, "..", "..", ".claude", "hooks", "github-identity.sh")
GH = """#!/bin/sh
case "$1 $2" in
  "config get") echo "$FAKE_GH_ACTIVE" ;;
  "api user") [ -n "$FAKE_GH_TOKEN_USER" ] && echo "$FAKE_GH_TOKEN_USER" || exit 1 ;;
esac
"""


@unittest.skipIf(sys.platform == "win32", "bash hook, not used on Windows")
class GithubIdentityHook(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        t = self.tmp.name
        self.repo = os.path.join(t, "repo")
        subprocess.run(["git", "init", "-q", self.repo], check=True)
        self.name("derpsn")
        bin_ = os.path.join(t, "bin")
        os.mkdir(bin_)
        with open(os.path.join(bin_, "gh"), "w") as f:
            f.write(GH)
        os.chmod(os.path.join(bin_, "gh"), 0o755)
        self.env = {k: v for k, v in os.environ.items() if k not in ("GH_TOKEN", "GITHUB_TOKEN")}
        self.env.update(PATH=bin_ + os.pathsep + os.environ["PATH"], CLAUDE_PROJECT_DIR=self.repo,
                        FAKE_GH_ACTIVE="derpsn", FAKE_GH_TOKEN_USER="", GIT_CONFIG_GLOBAL=os.devnull)

    def name(self, n):
        subprocess.run(["git", "-C", self.repo, "config", "user.name", n], check=True)

    def hook(self, command, **env):
        event = {"tool_name": "Bash", "tool_input": {"command": command}}
        r = subprocess.run(["bash", HOOK], input=json.dumps(event), capture_output=True, text=True,
                           env=dict(self.env, **env))
        return r.returncode, r.stderr

    def test_commands_without_github_pass_unchecked(self):
        self.assertEqual(self.hook("ls -la", FAKE_GH_ACTIVE="other")[0], 0)

    def test_push_as_derpsn_passes(self):
        self.assertEqual(self.hook("git push origin HEAD:refs/heads/x")[0], 0)

    def test_other_active_gh_account_blocks(self):
        for cmd in ("git push", "gh pr list", "cd x && git commit -m y", "curl https://api.github.com/x"):
            code, err = self.hook(cmd, FAKE_GH_ACTIVE="other")
            self.assertEqual(code, 2, cmd)
            self.assertIn("'other'", err)

    def test_wrong_git_user_name_blocks_a_commit(self):
        self.name("someone")
        code, err = self.hook("git commit -m x")
        self.assertEqual(code, 2)
        self.assertIn("user.name", err)

    def test_token_of_another_account_blocks(self):
        code, err = self.hook("gh issue list", GH_TOKEN="x", FAKE_GH_TOKEN_USER="other")
        self.assertEqual(code, 2)
        self.assertIn("GH_TOKEN", err)
        self.assertEqual(self.hook("gh issue list", GH_TOKEN="x", FAKE_GH_TOKEN_USER="derpsn")[0], 0)

    def test_unreadable_token_blocks(self):
        self.assertEqual(self.hook("gh issue list", GH_TOKEN="x")[0], 2)


if __name__ == "__main__":
    unittest.main()
