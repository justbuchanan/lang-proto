#!/bin/bash
set -eo pipefail

VERIFIER_BIN="external/io_kythe/kythe/cxx/verifier/verifier"
PB_INDEXER_BIN="kythe/cxx/indexer/proto/indexer"
PBTXT_INDEXER_BIN="kythe/cxx/indexer/textproto/indexer"

EXAMPLE_PB=kythe/cxx/indexer/textproto/testdata/example.proto
EXAMPLE_PBTXT=kythe/cxx/indexer/textproto/testdata/example.pbtxt

PB_ENTRIES="$TEST_TMPDIR/pb_entries"
PBTXT_ENTRIES="$TEST_TMPDIR/pbtxt_entries"

# Index proto file
$PB_INDEXER_BIN $EXAMPLE_PB > $PB_ENTRIES

# Index textproto file
$PBTXT_INDEXER_BIN \
    --text_proto_file=$EXAMPLE_PBTXT \
    --message_name=example.MyMessage \
    $EXAMPLE_PB \
    > $PBTXT_ENTRIES

# Verify both files, ensuring that xrefs work.
cat $PB_ENTRIES $PBTXT_ENTRIES | \
    $VERIFIER_BIN $EXAMPLE_PBTXT $EXAMPLE_PB \
    --show_goals \
    --goal_regex="\s*(?:#|(?://))\-(.*)" 
