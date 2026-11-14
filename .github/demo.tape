# SPDX-License-Identifier: Apache-2.0 OR MIT
#
# Rendered by `make demo` (vhs, https://github.com/charmbracelet/vhs) from
# the repository root, after it has built examples/basic.cpp into
# build-demo/. Off screen, it starts the pain001 server named by
# PAIN001_SERVER on 127.0.0.1:18088 and copies two conformance fixtures
# into a throwaway directory. On screen, the example generates a
# pain.001.001.09 file from the credit-transfer fixture, then the server
# refuses the fixture with a bad IBAN. Nothing leaves loopback.
Output .github/demo.gif

Set Shell "bash"
Set FontSize 14
Set Width 1200
Set Height 640
Set Padding 20
Set Theme "Catppuccin Mocha"
Set WaitTimeout 60s
Set WaitPattern /^\$ ?$/

Hide
Type `export PAIN001_BASE_URL=http://127.0.0.1:18088 PATH="$PWD/build-demo:$PATH" fx="$PWD/conformance/fixtures" srvdir="$(mktemp -d /tmp/pain001-sdk-cpp-serve.XXXX)"`
Enter
Type `(cd "$srvdir" && exec "$PAIN001_SERVER" serve --host 127.0.0.1 --port 18088 > serve.log 2>&1) & srv=$!`
Enter
Type `cd "$(mktemp -d /tmp/pain001-sdk-cpp-demo.XXXX)" && cp "$fx/credit-transfer.pain.001.001.09.csv" payments.csv && cp "$fx/bad-iban.pain.001.001.09.csv" bad-iban.csv`
Enter
Type `until curl -fsS "$PAIN001_BASE_URL/api/v1/health" > /dev/null 2>&1; do sleep 1; done; echo server" "up`
Enter
Wait+Screen@90s /server up/
Type `export PS1='$ ' && clear`
Enter
Sleep 500ms
Show

Type "curl -s http://127.0.0.1:18088/api/v1/health"
Sleep 500ms
Enter
Wait
Sleep 4s

Type "pain001_basic < payments.csv > payments.xml"
Sleep 500ms
Enter
Wait
Sleep 4s

Type "head -n 12 payments.xml"
Sleep 500ms
Enter
Wait
Sleep 8s

Type "pain001_basic < bad-iban.csv > refused.xml"
Sleep 500ms
Enter
Wait
Sleep 10s

Hide
Type `kill "$srv"; wait "$srv" 2> /dev/null; rm -rf "$srvdir"`
Enter
Sleep 1s
