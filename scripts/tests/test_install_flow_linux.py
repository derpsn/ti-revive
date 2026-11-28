"""The whole Linux install and uninstall (src/installer-linux/install.sh, uninstall.sh) in a temporary HOME. sudo,
systemctl, udevadm, msiextract, ldd and xdg-user-dir are stand-ins on PATH that only record their arguments, and the
udev rule path points into the temp folder (TIREVIVE_UDEV_RULE), so nothing of this machine is touched.
Run: python3 -I -m unittest discover -s scripts/tests"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "..", "src", "installer-linux")
sys.path.insert(0, HERE)
from test_install_linux import pe_header  # noqa: E402

STUBS = {
    # privileged steps: record only. sudo runs nothing, so the rule never reaches /etc.
    "sudo": 'echo "sudo $*" >> "$T/calls"',
    "udevadm": 'echo "udevadm $*" >> "$T/calls"',
    "systemctl": 'echo "systemctl $*" >> "$T/calls"',
    "ldd": "true",
    "xdg-user-dir": 'echo "$HOME/Documents"',
    # an unpacked 64-bit Access .msi: Common/ with the DLL, the ROM file and the Control Center, manuals, libraries
    "msiextract": '[ -f "$1" ] || exit 1; c="Access Music/Virus TI/Common"; mkdir -p "$c" "Access Music/Virus TI/Documentation" '
                  '"PersonalFiles/Access Music/Virus TI/Patches"; cp "$T/dll64" "$c/Virus Control.dll"; '
                  ': > "$c/firmware.bin"; : > "$c/Virus Control Center.exe"; : > "Access Music/Virus TI/Documentation/m.pdf"; '
                  ': > "PersonalFiles/Access Music/Virus TI/Patches/Factory.mdb"',
}


@unittest.skipIf(sys.platform == "win32", "the Linux installer, not used on Windows")
class InstallFlow(unittest.TestCase):
    def setUp(self):
        self.t = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.t)
        self.home = os.path.join(self.t, "home")
        os.makedirs(os.path.join(self.home, "Documents"))
        with open(os.path.join(self.t, "dll64"), "wb") as f:
            f.write(pe_header(64))
        # the release package as linux-build-release.sh lays it out
        self.pkg = os.path.join(self.t, "pkg")
        for d in ("bin", "systemd", "udev", "native/TI Revive.vst3/Contents/x86_64-linux"):
            os.makedirs(os.path.join(self.pkg, d))
        for s in ("install.sh", "uninstall.sh"):
            shutil.copy(os.path.join(SRC, s), self.pkg)
        for u in ("tirevive-broker.socket", "tirevive-broker.service", "tirevive-broker@.socket",
                  "tirevive-broker@.service"):
            shutil.copy(os.path.join(SRC, u), os.path.join(self.pkg, "systemd"))
        shutil.copy(os.path.join(SRC, "70-tirevive-virus-ti.rules"), os.path.join(self.pkg, "udev"))
        for b in ("vtibroker", "tirevive-cc", "tirevive-control-center"):
            open(os.path.join(self.pkg, "bin", b), "w").close()
        open(os.path.join(self.pkg, "native/TI Revive.vst3/Contents/x86_64-linux/TI Revive.so"), "w").close()
        open(os.path.join(self.t, "Virus TI Software Suite 5.1.7.00 64-bit.msi"), "w").close()
        stubs = os.path.join(self.t, "stubs")
        os.mkdir(stubs)
        for name, body in STUBS.items():
            p = os.path.join(stubs, name)
            with open(p, "w") as f:
                f.write(f"#!/bin/sh\n{body}\n")
            os.chmod(p, 0o755)
        self.rule = os.path.join(self.t, "etc-rule")
        self.env = {"HOME": self.home, "T": self.t, "PATH": stubs + ":/usr/bin:/bin", "LANG": "C",
                    "TIREVIVE_UDEV_RULE": self.rule, "WINEPREFIX": os.path.join(self.home, ".wine")}

    def sh(self, script, *args):
        r = subprocess.run(["sh", os.path.join(self.pkg, script), *args], env=self.env, cwd=self.t,
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return r.stdout

    def calls(self):
        p = os.path.join(self.t, "calls")
        if not os.path.exists(p):
            return ""
        with open(p) as f:
            return f.read()

    def files(self):
        out = []
        for root, dirs, fs in os.walk(self.home):
            out += [os.path.relpath(os.path.join(root, x), self.home) for x in dirs + fs]
        return sorted(out)

    # audit 12: an installed rule that differs from the package's is replaced, even when the Virus is usable now
    def test_a_different_rule_is_replaced(self):
        with open(self.rule, "w") as f:
            f.write("# an older rule\n")
        self.sh("install.sh", "Virus TI Software Suite 5.1.7.00 64-bit.msi")
        self.assertIn(f"sudo install -m 0644 {self.pkg}/udev/70-tirevive-virus-ti.rules {self.rule}", self.calls())

    def test_the_same_rule_is_left_alone(self):
        shutil.copy(os.path.join(SRC, "70-tirevive-virus-ti.rules"), self.rule)
        self.sh("install.sh", "Virus TI Software Suite 5.1.7.00 64-bit.msi")
        self.assertNotIn("sudo install", self.calls())
