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

def _simple_flag(flagname, condition):
    if condition:
        return flagname + "=true"
    return flagname + "=false"

def textproto_script_test(
        script,
        name,
        src, # textproto file
        protos, # list of 1+ .proto files
        message_name,
        tags,
        size,
        ignore_dups,
        goal_prefix,
        convert_marked_source):
    dups = _simple_flag("--ignore_dups", ignore_dups)
    convert = _simple_flag("--convert_marked_source", convert_marked_source)
    goal_prefix_flag = "--goal_prefix=\"" + goal_prefix + "\""
    native.sh_test(
        name = name,
        srcs = ["//kythe/cxx/indexer/textproto/testdata:" + script],
        data = [src] + protos + [
            "//kythe/cxx/indexer/textproto:indexer",
            "@io_kythe//kythe/cxx/verifier",
        ],
        args = [dups, goal_prefix_flag, convert, "--message_name=%s" % message_name, "--text_proto_file=$(location %s)" % src] + ["$(location %s)" % s for s in protos],
        tags = tags,
        size = size,
    )

# A verifier test that should pass and trigger no indexer errors.
def textproto_indexer_test(
        name,
        src, # textproto file
        protos, # list of 1+ .proto files
        message_name,
        deps = [],
        tags = [],
        size = "small",
        ignore_dups = True,
        goal_prefix = "#-",
        convert_marked_source = False):
    textproto_script_test(
        script = "run_case.sh",
        name = name,
        src = src,
        protos = protos,
        message_name = message_name,
        tags = tags,
        size = size,
        ignore_dups = ignore_dups,
        goal_prefix = goal_prefix,
        convert_marked_source = convert_marked_source,
    )

# # A test whose sources will trigger an indexer error but that should not
# # cause the indexer to crash.
# def textproto_error_test(
#         name,
#         srcs,
#         deps = [],
#         tags = [],
#         size = "small",
#         ignore_dups = True,
#         goal_prefix = "#-",
#         convert_marked_source = False):
#     proto_script_test(
#         script = "error_case.sh",
#         name = name,
#         srcs = [
#             "basic/nested-message-field.proto",
#         ] + srcs + [
#             "basic/nested-message.proto",
#         ],
#         deps = deps,
#         tags = tags,
#         size = size,
#         ignore_dups = ignore_dups,
#         goal_prefix = goal_prefix,
#         convert_marked_source = convert_marked_source,
#     )
