"""src/installer-mac/scripts/postinstall with no user at the Mac's screen (audit 2026-11-17 #9): at the login
window, over ssh or from MDM, /dev/console belongs to root, USER_ID stayed empty and the libusb package fell through to
the branch that removes the libusb service, then reported success. It must stop with an error and leave the service
alone. Runs the real script with stubbed macOS commands (on Linux the /Library paths do not exist, nothing is
touched). Run: python3 -I -m unittest discover -s scripts/tests"""
import os
import shutil
import stat
import subprocess
import tempfile
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
POSTINSTALL = os.path.join(HERE, "..", "..", "src", "installer-mac", "scripts", "postinstall")


def run(console_user, libusb, bootstrap_fails=False):
    """Run postinstall from a temp folder (with the libusb marker if `libusb`); return (exit code, command log).
    rm, mkdir and touch are stubbed too, so the script never deletes anything of this machine."""
    d = tempfile.mkdtemp()
    try:
        shutil.copy(POSTINSTALL, os.path.join(d, "postinstall"))
        if libusb:
            open(os.path.join(d, "libusb-audio"), "w").close()
        bin_ = os.path.join(d, "bin")
        os.mkdir(bin_)
        log = os.path.join(d, "log")
        fail = '[ "$1" != bootstrap ]' if bootstrap_fails else "true"
        stubs = {"stat": f'echo "{console_user}"', "id": 'echo 501',
                 "launchctl": f'echo "launchctl $*" >> "{log}"; {fail}',
                 "killall": "true", "pkill": "true", "sleep": "true", "chown": "true", "chmod": "true",
                 "install": "true", "find": "true", "rm": "true", "mkdir": "true", "touch": "true"}
        for name, body in stubs.items():
            p = os.path.join(bin_, name)
            with open(p, "w") as f:
                f.write(f"#!/bin/sh\n{body}\n")
            os.chmod(p, os.stat(p).st_mode | stat.S_IEXEC)
        env = dict(os.environ, PATH=bin_ + os.pathsep + os.environ["PATH"])
        r = subprocess.run(["bash", os.path.join(d, "postinstall")], env=env, capture_output=True, text=True)
        calls = open(log).read() if os.path.exists(log) else ""
        return r.returncode, calls + r.stderr
    finally:
        shutil.rmtree(d)


@unittest.skipIf(sys.platform == "win32", "the Mac package scripts, not used on Windows")
class MacPostinstall(unittest.TestCase):
    def test_libusb_package_without_a_console_user_stops(self):
        code, out = run("root", libusb=True)
        self.assertEqual(code, 1)
        self.assertNotIn("bootout system", out)  # the service is left as it is
        self.assertIn("log in", out.lower())

    # audit 5: a service launchd refused must fail the install, after one retry, not report success
    def test_libusb_bootstrap_failure_is_retried_then_fails(self):
        code, out = run("alice", libusb=True, bootstrap_fails=True)
        self.assertNotEqual(code, 0)
        self.assertEqual(out.count("launchctl bootstrap system"), 2)
        self.assertIn("could not start", out)

    def test_libusb_bootstrap_success(self):
        code, out = run("alice", libusb=True)
        self.assertEqual(code, 0)
        self.assertEqual(out.count("launchctl bootstrap system"), 1)

    def test_normal_package_without_a_console_user_still_succeeds(self):
        code, _ = run("root", libusb=False)
        self.assertEqual(code, 0)


if __name__ == "__main__":
    unittest.main()
