#!/bin/bash -e

# run this script from within the _tools_ directory

# clone the webthing-tester
if [ -d "webthing-tester" ]; then
    cd "webthing-tester"
    git pull origin time-regex-option
    cd ".."
else
    git clone -b time-regex-option https://github.com/bw-hro/webthing-tester.git
fi

pip3 install --user -r webthing-tester/requirements.txt

# build Webthing-CPP with examples when they do not exist
SINGLE_BIN="../build/examples/single-thing"
MULTIS_BIN="../build/examples/multiple-things"
if [ ! -e "$SINGLE_BIN" ] || [ ! -e "$MULTIS_BIN" ]; then
    cd ".."
    ./build.sh clean release
    cd "tools"
fi

# configure time regex to match timestamps with milliseconds
TIME_REGEX="^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}.\d{3}[+-]\d{2}:\d{2}$"

# test the single-thing example
"./$SINGLE_BIN" &
EXAMPLE_PID=$!
sleep 15
./webthing-tester/test-client.py --time-regex $TIME_REGEX --debug
kill -15 $EXAMPLE_PID
wait $EXAMPLE_PID || true

# test the multiple-things example
"./$MULTIS_BIN" &
EXAMPLE_PID=$!
sleep 15
./webthing-tester/test-client.py --time-regex $TIME_REGEX --path-prefix "/0" --debug
kill -15 $EXAMPLE_PID
wait $EXAMPLE_PID || true
