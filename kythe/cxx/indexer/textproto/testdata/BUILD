load(":textproto_indexer_test.bzl", "textproto_indexer_test")

# Required by textproto_indexer_test macro.
sh_binary(
    name = "run_case",
    srcs = ["run_case.sh"],
    data = [
        "//kythe/cxx/indexer/textproto:indexer",
        "@io_kythe//kythe/cxx/verifier",
    ],
)

textproto_indexer_test(
    name = "example",
    src = "example.pbtxt",
    message_name = "example.MyMessage",
    protos = ["example.proto"],
    tags = ["basic"],
)

sh_test(
    name = "xref_test",
    srcs = ["xref_test.sh"],
    data = [
        "example.pbtxt",
        "example.proto",
        "//kythe/cxx/indexer/proto:indexer",
        "//kythe/cxx/indexer/textproto:indexer",
        "@io_kythe//kythe/cxx/verifier",
    ],
)

genrule(
    name = "example_pb_facts",
    srcs = ["example.proto"],
    outs = ["example_pb.entries"],
    cmd = "$(location //kythe/cxx/indexer/proto:indexer)" +
          " $(location example.proto) -o $@",
    tools = ["//kythe/cxx/indexer/proto:indexer"],
)

genrule(
    name = "example_pbtxt_facts",
    srcs = [
        "example.pbtxt",
        "example.proto",
    ],
    outs = ["example_pbtxt.entries"],
    cmd = "$(location //kythe/cxx/indexer/textproto:indexer)" +
          " --text_proto_file=$(location example.pbtxt) -o $@ --message_name=example.MyMessage $(location example.proto)",
    tools = ["//kythe/cxx/indexer/textproto:indexer"],
)
