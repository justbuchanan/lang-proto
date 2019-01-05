cc_binary(
    name = "indexer",
    srcs = ["main.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":analyzer",
        "@com_google_protobuf//:protobuf",
        "@io_kythe//kythe/cxx/common:lib",
        "@io_kythe//kythe/cxx/common/indexing:caching_output",
        "@io_kythe//kythe/cxx/common/indexing:output",
        "@io_kythe//kythe/proto:analysis_cc_proto",
    ],
)

cc_library(
    name = "analyzer",
    srcs = ["analyzer.cc"],
    hdrs = ["analyzer.h"],
    deps = [
        "//kythe/cxx/indexer/proto:proto_analyzer",
        "//kythe/cxx/indexer/proto:vname",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
        "@io_kythe//kythe/cxx/common:lib",
        "@io_kythe//kythe/cxx/common:utf8_line_index",
        "@io_kythe//kythe/cxx/common/indexing:caching_output",
        "@io_kythe//kythe/cxx/common/indexing:output",
        "@io_kythe//kythe/proto:analysis_cc_proto",
    ],
)

# text_proto(
#     name = "example_pbtxt",
#     src = "example.pbtxt",
#     message = "kythe.proto.CompilationUnit",
#     deps = [
#         "@io_kythe//kythe/proto:analysis_proto",
#     ],
# )
