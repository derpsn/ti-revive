# The 0.7.x wrapper, kept only as the reference for measurements and screenshots of the original under Wine
# (scripts/latency-wine.sh, native/tools/uicompare ORIG mode) and the VST3 test host they and scripts/latency-orig.sh
# run: make -C src/wrapper. The other 0.7.x targets were removed in 0.8.45 (git history).
CC = x86_64-w64-mingw32-gcc
CFLAGS = -O2 -Wall -Wextra
OUT = ../../work/build

all: $(OUT)/linux-wine/VirusTI_TIRevive.vst3 $(OUT)/vtihost.exe

# Linux (findings/design-linux.md, L2): the same wrapper for Wine/yabridge, device access through the NATIVE Linux
# vtibroker over its Unix socket (vticore_sock.c) instead of the vtibroker.exe pipe
$(OUT)/linux-wine/VirusTI_TIRevive.vst3: vtiwrap.c vtiemu.c vtiaudio.c ../broker/vtiusbaudio.c ../broker/vtiusbaudio.h replay_table.h ../broker/vticore_sock.c ../broker/vticore_midi.c ../broker/vticore.h ../broker/vtiproto.h ../broker/vtiaudioshm.h
	mkdir -p $(OUT)/linux-wine
	$(CC) $(CFLAGS) -DVTIEMU_NO_DLLMAIN -DVTIEMU_VIRTUAL_DEVICE -shared -o $@ vtiwrap.c vtiemu.c vtiaudio.c ../broker/vtiusbaudio.c ../broker/vticore_sock.c \
		../broker/vticore_midi.c -lwinusb -lsetupapi -lshlwapi -lshell32 -lole32 -luuid -lws2_32 -lwinmm -static -lpthread

# VST3 test host for Windows / Wine (process loop, test notes, WAV): vtihost <plugin.vst3> [seconds] [out.wav] [quiet]
VST3SDK ?= ../../native/external/vst3sdk
$(OUT)/vtihost.exe: ../../native/tools/vtihost.cpp
	mkdir -p $(OUT)
	x86_64-w64-mingw32-g++ -std=c++17 -O2 -Wall -Wno-format -I$(VST3SDK) -o $@ ../../native/tools/vtihost.cpp $(VST3SDK)/pluginterfaces/base/funknown.cpp \
		-static -lole32 -luuid -lgdi32 -luser32

clean:
	rm -f $(OUT)/linux-wine/VirusTI_TIRevive.vst3 $(OUT)/vtihost.exe
