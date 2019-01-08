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

def _verifier_test(ctx):
    # Write script to be executed by 'bazel test'.
    script = "cat {facts} | {verifier} {files} --show_protos --show_goals".format(
        verifier = ctx.executable._verifier_bin.short_path,
        facts = " ".join([f.short_path for f in ctx.files.facts]),
        files = " ".join([f.path for f in ctx.files.files]),
    )
    ctx.actions.write(
        output = ctx.outputs.executable,
        content = script,
    )

    # To ensure the files needed by the script are available, we put them in
    # the runfiles.
    runfiles = ctx.runfiles(files = ctx.files.files + ctx.files.facts + [ctx.executable._verifier_bin])
    return [DefaultInfo(runfiles = runfiles)]

verifier_test = rule(
    implementation = _verifier_test,
    test = True,
    attrs = {
        "facts": attr.label_list(allow_files = True, mandatory = True),
        "_verifier_bin": attr.label(cfg = "host", executable = True, allow_files = True, default = Label("@io_kythe//kythe/cxx/verifier")),
        "files": attr.label_list(allow_files = True, mandatory = True),
    },
)

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
        outs = [name + "_proto.facts"],
        cmd = "$(location //kythe/cxx/indexer/proto:indexer) -o $(OUTS) " +
              " ".join(["$(location %s)" % p for p in protos]),
        tools = ["//kythe/cxx/indexer/proto:indexer"],
    )

    # Run verifier
    verifier_test(
        name = name,
        facts = [pbtxt_facts, proto_facts],
        files = [textproto] + protos,
    )
