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
