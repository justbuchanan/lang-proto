#!/bin/bash
set -eo pipefail

FACTS_FILE="$1"; shift
cat $FACTS_FILE | external/io_kythe/kythe/cxx/verifier/verifier $@
