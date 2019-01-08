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
    script = "cat {facts} | {verifier} {files} --show_protos --show_goals --goal_regex=\"\s*(?:#|//)-(.*)\"".format(
        verifier = ctx.executable._verifier_bin.short_path,
        facts = " ".join([f.short_path for f in ctx.files.facts]),
        files = " ".join([f.path for f in ctx.files.files]),
    )
    ctx.actions.write(
        output = ctx.outputs.executable,
        content = script,
    )
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

def _index(ctx):
    args = ctx.attr.extra_args + [
        "-o",
        ctx.outputs.facts.path,
    ] + [p.path for p in ctx.files.files]
    if ctx.files.textproto:
        args += ["--text_proto_file", ctx.files.textproto[0].path]

    ctx.actions.run(
        outputs = [ctx.outputs.facts],
        inputs = ctx.files.files + ctx.files.textproto,
        executable = ctx.executable.indexer_bin,
        arguments = args,
    )

index = rule(
    implementation = _index,
    attrs = {
        "files": attr.label_list(allow_files = True, mandatory = True),
        "indexer_bin": attr.label(cfg = "host", executable = True, allow_files = True),
        "textproto": attr.label(cfg = "host", allow_files = True, mandatory = False),
        "extra_args": attr.string_list(),
    },
    outputs = {
        "facts": "%{name}.facts",
    },
)

def textproto_indexer_test(
        name,
        textproto,
        protos,
        message_name):
    # Index textproto
    pbtxt_facts = name + "_pbtxt_facts"
    index(
        name = pbtxt_facts,
        textproto = textproto,
        indexer_bin = "//kythe/cxx/indexer/textproto:indexer",
        extra_args = ["--message_name=" + message_name],
        files = protos,
    )

    # Index protos
    proto_facts = name + "_proto_facts"
    index(
        name = proto_facts,
        indexer_bin = "//kythe/cxx/indexer/proto:indexer",
        files = protos,
    )

    # Run verifier
    verifier_test(
        name = name,
        facts = [pbtxt_facts, proto_facts],
        files = [textproto] + protos,
    )
