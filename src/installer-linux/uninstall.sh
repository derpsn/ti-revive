#!/bin/sh
# TI Revive for Linux - uninstaller. Removes the broker, its systemd user units, the token, the plug-in, the unpacked
# Access files and the classic plug-in of TI Revive 0.7 from the Wine prefix; the udev rule is removed with sudo.
set -u
PREFIX="${WINEPREFIX:-$HOME/.wine}"
UNITS="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
if command -v systemctl >/dev/null 2>&1; then
    SOCKETS=tirevive-broker.socket # Virus 2-4 (#45): instances of the template unit
    for n in 2 3 4; do SOCKETS="$SOCKETS tirevive-broker@$n.socket"; done
    systemctl --user disable --now $SOCKETS 2>/dev/null
    systemctl --user stop tirevive-broker.service 'tirevive-broker@*.service' 2>/dev/null
fi
rm -f "$UNITS/tirevive-broker.socket" "$UNITS/tirevive-broker.service" "$UNITS/tirevive-broker@.socket" \
    "$UNITS/tirevive-broker@.service"
rm -f "$HOME/.local/bin/tirevive-cc" "$HOME/.local/bin/tirevive-control-center"
rm -f "${XDG_DATA_HOME:-$HOME/.local/share}/applications/tirevive-control-center.desktop"
command -v systemctl >/dev/null 2>&1 && systemctl --user daemon-reload 2>/dev/null
rm -rf "$HOME/.local/lib/tirevive"
rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/tirevive/broker.token"
rm -f "$PREFIX/drive_c/Program Files/Common Files/VST3/Virus TI (TI Revive).vst3"
# the old bundle names are also listed in src/installer-linux/install.sh, src/installer-mac/scripts/postinstall,
# "src/installer-mac/Uninstall TI Revive.command", native/packaging/windows/native.wxs: change them together
rm -rf "$HOME/.vst3/TI Revive.vst3" "$HOME/.vst3/TI Revive Virus Control.vst3" "$HOME/.vst3/VirusTINative.vst3"
rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/tirevive/access-dir"
rm -rf "$HOME/.local/share/tirevive/access" # the unpacked Access files (your patch libraries in your documents folder stay)
rmdir "${XDG_CONFIG_HOME:-$HOME/.config}/tirevive" "$HOME/.local/share/tirevive" 2>/dev/null # only when empty
command -v yabridgectl >/dev/null 2>&1 && yabridgectl sync --prune >/dev/null 2>&1
RULE=${TIREVIVE_UDEV_RULE:-/etc/udev/rules.d/70-tirevive-virus-ti.rules} # the variable: tests only
if [ -f "$RULE" ]; then
    sudo rm -f "$RULE" && sudo udevadm control --reload
fi
echo "TI Revive removed. Your patch libraries in Access Music (documents folder) stay."
