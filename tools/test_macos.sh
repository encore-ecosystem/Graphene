#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if [ "$(uname -s)" != "Darwin" ]; then
    echo "tools/test_macos.sh is only needed on macOS" >&2
    exit 2
fi

# Encore's standalone-test linker does not currently honor target.runtime-sources.
# Put a narrow clang wrapper first in PATH so only target/tests links receive the
# Objective-C Metal backend and Apple framework arguments.
PATH="$script_dir/macos-test-bin:$PATH" exec encore test "$@"
