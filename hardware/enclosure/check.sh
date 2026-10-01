#!/bin/bash
# delegated: the counter-model now lives in hardware/check.sh (covers clamp, bar, modified case, real Pi)
exec "$(dirname "$0")/../check.sh" "$@"
