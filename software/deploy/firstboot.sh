#!/bin/bash
# Pi-Gaze first boot: build + install from /opt/pigaze/src, then reboot once.
# Run by pigaze-firstboot.service until it succeeds (needs the network only if g++ is missing).
set -uo pipefail
exec > >(tee -a /var/log/pigaze-setup.log) 2>&1
echo "=== pigaze first boot $(date -Is) ==="

if ! command -v g++ >/dev/null; then
  for _ in $(seq 1 60); do
    getent hosts deb.debian.org >/dev/null && break
    sleep 5
  done
fi

if bash /opt/pigaze/src/deploy/pi-setup.sh; then
  touch /var/lib/pigaze-installed
  systemctl disable pigaze-firstboot.service
  echo "=== installed, rebooting ==="
  systemctl reboot --no-block
else
  echo "=== setup failed; it will be retried on the next boot (see above) ==="
  exit 1
fi
