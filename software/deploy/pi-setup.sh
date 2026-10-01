#!/bin/bash
# Pi-Gaze installer: build, run the unit tests, install. Safe to re-run:
#   sudo bash deploy/pi-setup.sh
set -euo pipefail
SRC="$(cd "$(dirname "$0")/.." && pwd)"
BUILD=/opt/pigaze/build
log() { echo "[pigaze-setup] $*"; }

need=()
command -v g++ >/dev/null || need+=(g++)
command -v rpicam-vid >/dev/null || need+=(rpicam-apps-lite)
if ((${#need[@]})); then
  log "installing: ${need[*]}"
  apt-get update
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${need[@]}"
  apt-get clean
fi

mkdir -p "$BUILD"
FLAGS=(-std=c++17 -O2 -pthread -Wall -Wextra -I"$SRC/include")
log "building + running unit tests"
g++ "${FLAGS[@]}" "$SRC"/src/core/*.cpp "$SRC"/tests/*.cpp -o "$BUILD/pigaze_tests"
(cd "$BUILD" && ./pigaze_tests)
log "building pigaze"
g++ "${FLAGS[@]}" "$SRC"/src/core/*.cpp "$SRC"/src/linux/*.cpp "$SRC"/src/main.cpp -o "$BUILD/pigaze"
"$BUILD/pigaze" --selftest

log "installing"
install -m 755 "$BUILD/pigaze" /usr/local/bin/pigaze
install -m 755 "$SRC/deploy/pigaze-ctl" /usr/local/bin/pigaze-ctl
install -d -m 755 /etc/pigaze
[ -f /etc/pigaze/pigaze.conf ] || install -m 644 "$SRC/config/pigaze.conf" /etc/pigaze/pigaze.conf
install -D -m 644 "$SRC/deploy/logind-pigaze.conf" /etc/systemd/logind.conf.d/pigaze.conf
install -m 644 "$SRC/deploy/pigaze.service" /etc/systemd/system/pigaze.service

# the service runs as the login user 'pigaze' (created by cloud-init); make one if missing
id pigaze >/dev/null 2>&1 || useradd --system --no-create-home --shell /usr/sbin/nologin pigaze
for g in video dialout input gpio; do
  getent group "$g" >/dev/null && usermod -aG "$g" pigaze
done

# UART0 on GPIO14/15 for the CH9329
CFG=/boot/firmware/config.txt
if ! grep -q '^dtparam=uart0=on' "$CFG"; then
  printf '\n# Pi-Gaze: CH9329 on GPIO14 (TX) / GPIO15 (RX)\ndtparam=uart0=on\n' >> "$CFG"
  log "enabled UART0 in $CFG (active after reboot)"
fi

systemctl daemon-reload
systemctl enable pigaze.service
log "done. After a reboot pigaze starts by itself: journalctl -u pigaze -f"
