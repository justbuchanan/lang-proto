#!/usr/bin/env bash -e

bazel build //kythe/cxx/indexer/proto:indexer \
            //kythe/cxx/indexer/textproto:indexer \
            @io_kythe//kythe/cxx/verifier

./bazel-bin/kythe/cxx/indexer/proto/indexer kythe/cxx/indexer/textproto/testdata/example.proto > pb.entries                                                                 
./bazel-bin/kythe/cxx/indexer/textproto/indexer --text_proto_file kythe/cxx/indexer/textproto/testdata/example.pbtxt --message_name=example.MyMessage kythe/cxx/indexer/textproto/testdata/example.proto > pbtxt.entries                                                                                                                                                                                                                             

 cat pb.entries pbtxt.entries| /opt/kythe/tools/verifier kythe/cxx/indexer/textproto/testdata/example.pbtxt kythe/cxx/indexer/textproto/testdata/example.proto --show_goals \
--goal_regex="(?:#|(?://))\-(.*)" 
