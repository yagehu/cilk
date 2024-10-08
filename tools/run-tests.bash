#!/usr/bin/env bash

set -euo pipefail

for test in build/tests/test-*; do
    test_name=${test#"build/tests/test-"}

    printf "Running test: $test_name ... "

    if [ -z ${VALGRIND+x} ]; then
        LD_LIBRARY_PATH=build/lib ${test}
    else
        LD_LIBRARY_PATH=build/lib "$VALGRIND" ${test}
    fi

    if [[ $? == "0" ]]; then
        printf "$(tput setaf 2)OK$(tput sgr0)\n"
    else
        printf "$(tput setaf 1)Failed$(tput sgr0)\n"
    fi
done
