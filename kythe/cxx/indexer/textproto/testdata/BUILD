load(":textproto_indexer_test.bzl", "textproto_indexer_test")

textproto_indexer_test(
    name = "basics_test",
    message_name = "example.MyMessage",
    protos = ["example.proto"],
    textproto = "basics.pbtxt",
)

textproto_indexer_test(
    name = "nested_message_test",
    message_name = "example.MyMessage",
    protos = ["example.proto"],
    textproto = "nested_message.pbtxt",
)

textproto_indexer_test(
    name = "repeated_field_test",
    message_name = "example.MyMessage",
    protos = ["example.proto"],
    textproto = "repeated_field.pbtxt",
)

textproto_indexer_test(
    name = "default_value_test",
    message_name = "example.MyMessage",
    protos = ["example.proto"],
    textproto = "default_value.pbtxt",
)

textproto_indexer_test(
    name = "with_imports_test",
    message_name = "example2.OuterMessage",
    protos = [
        "example.proto",
        "proto_with_imports.proto",
    ],
    textproto = "with_imports.pbtxt",
)
