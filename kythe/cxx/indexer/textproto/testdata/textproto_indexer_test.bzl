"""Rules for testing the textproto indexer"""
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


def textproto_indexer_test(
        name,
        textproto,
        protos,
        message_name):
    # Index textproto
    pbtxt_facts = name + "_pbtxt_facts"
    native.genrule(
        name = pbtxt_facts,
        srcs = [textproto] + protos,
        outs = [name + "_pbtxt.facts"],
        cmd = "$(location //kythe/cxx/indexer/textproto:indexer)" +
              " --text_proto_file=$(location %s) -o $(OUTS) --message_name=%s " % (textproto, message_name) +
              " ".join(["$(location %s)" % p for p in protos]),
        tools = ["//kythe/cxx/indexer/textproto:indexer"],
    )

    # Index protos
    proto_facts = name + "_proto_facts"
    native.genrule(
        name = proto_facts,
        srcs = protos,
        outs = [name+"_proto.facts"],
        cmd = "$(location //kythe/cxx/indexer/proto:indexer) -o $(OUTS) " +
              " ".join(["$(location %s)" % p for p in protos]),
        tools = ["//kythe/cxx/indexer/proto:indexer"],
    )

    # Aggregate graph facts into single file to pass to verifier
    agg_facts = name + "_agg_facts"
    native.genrule(
        name = agg_facts,
        srcs = [
            pbtxt_facts,
            proto_facts,
        ],
        outs = [name + "_agg.facts"],
        cmd = "cat $(SRCS) > $(OUTS)",
    )

    # Run verifier
    native.sh_test(
        name = name,
        srcs = ["verifier_wrapper.sh"],
        args = [
            "$(location %s)" % agg_facts,
            " ".join(["$(location %s)" % p for p in protos]),
            "--show_protos",
            "--show_goals",
            # goal regex matches both "//-" (for .proto) and "#-" (for textproto)
            "--goal_regex=\"\s*(?:#|(?://))\-(.*)\"",
            "$(location %s)" % textproto,
        ],
        data = [
            textproto,
            agg_facts,
            "@io_kythe//kythe/cxx/verifier",
        ] + protos,
    )
