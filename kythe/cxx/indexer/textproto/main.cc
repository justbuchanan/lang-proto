#include <fcntl.h>
#include <fstream>
#include <iostream>
#include "kythe/cxx/common/file_vname_generator.h"
#include "absl/strings/match.h"
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

// TODO: allow indexing from kzip files

namespace {

// Opens the given file and returns its contents as a string.
std::string ReadTextFile(const std::string& path) {
  std::ifstream in_stream(path);
  std::string buf;

  // TODO: check if file exists and give helpful error message

  // allocate space up-front
  in_stream.seekg(0, std::ios::end);
  buf.reserve(in_stream.tellg());
  in_stream.seekg(0, std::ios::beg);

  buf.assign((std::istreambuf_iterator<char>(in_stream)),
             std::istreambuf_iterator<char>());

  return buf;
}

}  // namespace

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  gflags::SetUsageMessage(
      R"(Command-line frontend for the Kythe TextProto indexer.

Examples:
  indexer -o foo.bin --text_proto_file foo.textproto --message_name "my.package.MyMessage"
  indexer --text_proto_file bar.textproto --message_name | verifier foo.textproto bar.textproto")");

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  std::vector<std::string> final_args(argv + 1, argv + argc);

  CHECK(FLAGS_message_name.size() > 0) << "Please provide a --message_name";
  // TODO: auto-detect message name if none is provided

  CHECK(FLAGS_text_proto_file.size())
      << "Please provide an input --text_proto_file";

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

  kythe::proto::FileData pbtxtFile;
  pbtxtFile.set_content(input);
  pbtxtFile.mutable_info()->set_path(FLAGS_text_proto_file);
  // TODO: digest?
  files.push_back(std::move(pbtxtFile));

  for (const std::string& arg : final_args) {
    LOG(ERROR) << "Got arg: " << arg;
  }

  // TODO: better cli interface
  // Add .proto file inputs to FileData list
  for (const std::string& arg : final_args) {
    if (absl::EndsWith(arg, ".proto")) {
      LOG(ERROR) << "Adding proto to file data: " << arg;
      kythe::proto::FileData protoFile;
      protoFile.set_content(ReadTextFile(arg));
      protoFile.mutable_info()->set_path(arg);
      // TODO: set digest?
      files.push_back(std::move(protoFile));
    } else {
      LOG(ERROR) << "Ignoring arg: " << arg;
    }
  }

  LOG(ERROR) << "files size " << files.size();

  cu.add_source_file(FLAGS_text_proto_file);

  {
    google::protobuf::io::FileOutputStream raw_output(write_fd);
    kythe::FileOutputStream kythe_output(&raw_output);
    kythe_output.set_flush_after_each_entry(FLAGS_flush_after_each_entry);

    kythe::FileVNameGenerator file_vnames;
    kythe::KytheGraphRecorder recorder(&kythe_output);

    kythe::lang_textproto::TextProtoAnalyzer analyzer(
        &cu, &files, FLAGS_message_name, &file_vnames, &recorder);
    analyzer.DoIt();
  }

  CHECK(::close(write_fd) == 0) << "Error closing output file";

  return 0;
}
