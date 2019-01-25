#!/bin/bash
# Copyright 2018 The Kythe Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -eo pipefail

PROTOC_ARGS="--proto_path=kythe/cxx/indexer/textproto/testdata"

HAD_ERRORS=0
VERIFIER="external/io_kythe/kythe/cxx/verifier/verifier"
INDEXER="kythe/cxx/indexer/textproto/indexer"

IGNORE_DUPS="$1"; shift
GOAL_PREFIX="$1"; shift
CONVERT_MARKED_SOURCE="$1"; shift
MESSAGE_NAME="$1"; shift
TEXT_PROTO_FILE="$1"; shift

"${INDEXER}" -v 1 "$MESSAGE_NAME" "$TEXT_PROTO_FILE" "$@" | \
    "${VERIFIER}" --show_protos --show_goals "$IGNORE_DUPS" "$GOAL_PREFIX" "$CONVERT_MARKED_SOURCE" "$@"
RESULTS=( "${PIPESTATUS[0]}" "${PIPESTATUS[1]}" )
if [[ "${RESULTS[0]}" -ne 0 ]]; then
  echo "[ FAILED INDEX: $* (error code ${RESULTS[0]}) ]"
  HAD_ERRORS=1
elif [[ "${RESULTS[1]}" -ne 0 ]]; then
  echo "[ FAILED VERIFY: $* ]"
  HAD_ERRORS=1
else
  echo "[ OK: $* ]"
fi
exit "$HAD_ERRORS"
