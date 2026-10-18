#!/usr/bin/env python3
"""Reads the hourly battery/scan diary the Pet app keeps in NVS (pet:tlm) and prints it.

Usage (ESP-IDF environment active):
    python tools/read_log.py [--port /dev/tty.usbmodem1101]   # reads the NVS partition from the watch
    python tools/read_log.py --file nvs.bin                    # a dump made earlier

Reading the partition resets the chip (it boots the Launcher afterwards); nothing is written.
Entry layout = components/pet_app/pet_log.h: u32 t, u16 mv, u16 scans, i8 pct, u8 flags.
mV/h only means something between two consecutive on-battery entries, so rows after an
entry flagged USB (u) are not used for the average.
"""
import argparse, glob, os, re, struct, subprocess, sys, tempfile, time

NVS_OFFSET, NVS_SIZE = "0x9000", "0x6000"
ENTRY = struct.Struct("<IHHbB")


def read_partition(port, out):
    cmd = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "-p", port, "-b", "921600",
           "--after", "hard_reset", "read_flash", NVS_OFFSET, NVS_SIZE, out]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)


def blob_from_dump(nvs_bin):
    idf = os.environ.get("IDF_PATH")
    if not idf:
        sys.exit("IDF_PATH not set: source the ESP-IDF activation script first")
    tool = os.path.join(idf, "components/nvs_flash/nvs_partition_tool/nvs_tool.py")
    text = subprocess.run([sys.executable, tool, nvs_bin, "-d", "blobs", "--color", "never"],
                          check=True, capture_output=True, text=True).stdout
    data, cur, size = None, False, 0
    for line in text.splitlines():
        if line.startswith("pet:tlm"):
            data, cur = bytearray(), True
            size = int(re.search(r"Size: (\d+)", line).group(1))
        elif cur and line.startswith("  0x"):
            cols = line[10:].split("  ")
            data += bytes(int(b, 16) for b in (cols[0].split() + cols[1].split())[:32])
        elif cur and not line.strip():
            break
    return bytes(data[:size]) if data is not None else None   # the dump pads to 32-byte lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port")
    ap.add_argument("--file")
    args = ap.parse_args()
    if args.file:
        path = args.file
    else:
        port = args.port or (sorted(glob.glob("/dev/tty.usbmodem*")) or sorted(glob.glob("/dev/ttyACM*")) or [None])[0]
        if not port:
            sys.exit("no serial port found; pass --port or --file")
        path = os.path.join(tempfile.mkdtemp(), "nvs.bin")
        read_partition(port, path)
    blob = blob_from_dump(path)
    if not blob:
        sys.exit("no pet:tlm entry in that NVS (the app has not logged an hour yet)")
    n = len(blob) // ENTRY.size
    rows = [ENTRY.unpack_from(blob, i * ENTRY.size) for i in range(n)]
    print(f"{len(rows)} hourly entries")
    print("hour (local)       mV    %   scans  flags   mV/h   scans/h")
    prev, drops, hours = None, 0, 0
    for t, mv, scans, pct, flags in rows:
        when = time.strftime("%Y-%m-%d %H:%M", time.gmtime(t))
        fl = ("u" if flags & 1 else "-") + ("s" if flags & 2 else "-") + ("b" if flags & 4 else "-")
        extra = ""
        if prev:
            pt, pmv, pscans, _, pflags = prev
            dh = (t - pt) / 3600
            ds = (scans - pscans) & 0xFFFF
            rate = ""
            if dh > 0:
                rate = f"{(pmv - mv) / dh:6.1f}" if not (flags | pflags) & 1 and mv and pmv else "     -"
                if not (flags | pflags) & 1 and mv and pmv and 0.5 < dh < 1.5:
                    drops += pmv - mv
                    hours += dh
                extra = f"{rate}  {ds / dh:6.1f}"
        print(f"{when}  {mv:5d} {pct:4d} {scans:7d}  {fl}   {extra}")
        prev = (t, mv, scans, pct, flags)
    if hours:
        print(f"\non-battery average over {hours:.0f} h of consecutive hours: {drops / hours:.1f} mV/h")
    print("flags: u = USB, s = screen on, b = first entry after boot")


if __name__ == "__main__":
    main()
