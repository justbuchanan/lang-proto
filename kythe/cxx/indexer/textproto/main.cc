#include <fcntl.h>
#include <fstream>
#include <iostream>
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/text_format.h"
#include "kythe/cxx/common/indexing/KytheCachingOutput.h"
#include "kythe/cxx/common/indexing/KytheGraphRecorder.h"
#include "kythe/cxx/common/protobuf_metadata_file.h"
#include "kythe/cxx/indexer/textproto/analyzer.h"
#include "kythe/proto/analysis.pb.h"

DEFINE_string(o, "-", "Output filename.");
DEFINE_bool(flush_after_each_entry, true,
            "Flush output after writing each entry.");
DEFINE_string(message_name, "",
              "Qualified message name of the proto. For example: "
              "\"kythe.proto.CompilationUnit\".");
DEFINE_string(text_proto_file, "", "Input textproto file");
// DEFINE_string(proto_descriptor_list_file, "", "Text file with a
// path/to/file.proto on each line that lists the deps of the input
// textproto.");

namespace {
//
// void IndexTextProtoCompilationUnit(const proto::CompilationUnit& unit,
//                                      const std::vector<proto::FileData>&
//                                      files, KytheOutputStream* output) {
//   // TODO
// }
//

std::string ReadTextFile(const std::string& path) {
  std::ifstream in_stream(path);
  std::string buf;

  // allocate space up-front
  in_stream.seekg(0, std::ios::end);
  buf.reserve(in_stream.tellg());
  in_stream.seekg(0, std::ios::beg);

  buf.assign((std::istreambuf_iterator<char>(in_stream)),
             std::istreambuf_iterator<char>());

  return buf;
}

};  // namespace

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  gflags::SetUsageMessage(
      R"(Command-line frontend for the Kythe TextProto indexer.

Examples:
  indexer -o foo.bin -- --file foo.textproto --message_name "my.namespace.MyMessage"
  indexer foo.textproto bar.textproto | verifier foo.textproto bar.textproto")");

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  std::vector<std::string> final_args(argv + 1, argv + argc);

  CHECK(FLAGS_message_name.size() > 0) << "Please provide a --message_name";
  CHECK(FLAGS_text_proto_file.size()) << "Please provide an input --file";

  const std::string input = ReadTextFile(FLAGS_text_proto_file);

  int write_fd = STDOUT_FILENO;
  if (FLAGS_o != "-") {
    write_fd = ::open(FLAGS_o.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (write_fd == -1) {
      perror("Can't open output file");
      exit(1);
    }
  }

  kythe::proto::CompilationUnit cu;
  std::vector<kythe::proto::FileData> files;

  {
    google::protobuf::io::FileOutputStream raw_output(write_fd);
    kythe::FileOutputStream kythe_output(&raw_output);
    kythe_output.set_flush_after_each_entry(FLAGS_flush_after_each_entry);

    kythe::KytheGraphRecorder recorder(&kythe_output);

    kythe::lang_textproto::TextProtoAnalyzer analyzer(
        &cu, &files, FLAGS_message_name, &recorder);
    analyzer.DoIt();
  }

  CHECK(::close(write_fd) == 0) << "Error closing output file";

  return 0;
}
