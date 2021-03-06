/*
 * Copyright 2019 The Kythe Authors. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "proto_extractor.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "glog/logging.h"
#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "kythe/cxx/common/file_vname_generator.h"
#include "kythe/cxx/common/path_utils.h"
#include "kythe/cxx/indexer/proto/search_path.h"
#include "kythe/proto/analysis.pb.h"

namespace kythe {
namespace lang_proto {
namespace {

// Error "collector" that just writes messages to log output.
class LoggingMultiFileErrorCollector
    : public google::protobuf::compiler::MultiFileErrorCollector {
 public:
  void AddError(const std::string& filename, int line, int column,
                const std::string& message) override {
    LOG(ERROR) << filename << "@" << line << ":" << column << ": " << message;
  }

  void AddWarning(const std::string& filename, int line, int column,
                  const std::string& message) override {
    LOG(ERROR) << filename << "@" << line << ":" << column << ": " << message;
  }
};

// DiskSourceTree that records which proto files are opened while parsing the
// toplevel proto(s), allowing us to get a list of transitive dependencies.
class RecordingDiskSourceTree
    : public google::protobuf::compiler::DiskSourceTree {
 public:
  google::protobuf::io::ZeroCopyInputStream* Open(
      const std::string& filename) override {
    // Record resolved/canonical path because the same proto may be Open()'d via
    // multiple relative paths and we only want to record it once.
    std::string canonical_path;
    if (!DiskSourceTree::VirtualFileToDiskFile(filename, &canonical_path)) {
      return nullptr;
    }
    if (opened_files_.find(canonical_path) == opened_files_.end()) {
      opened_files_.insert(canonical_path);
    }

    return DiskSourceTree::Open(filename);
  }

  // A set of unique file paths that have been passed to Open().
  const std::set<std::string>& opened_files() const { return opened_files_; }

 private:
  std::set<std::string> opened_files_;
};

// Loads all data from a file or terminates the process.
static std::string LoadFileOrDie(const std::string& file) {
  FILE* handle = fopen(file.c_str(), "rb");
  CHECK(handle != nullptr) << "Couldn't open input file " << file;
  CHECK_EQ(fseek(handle, 0, SEEK_END), 0) << "Couldn't seek " << file;
  long size = ftell(handle);
  CHECK_GE(size, 0) << "Bad size for " << file;
  CHECK_EQ(fseek(handle, 0, SEEK_SET), 0) << "Couldn't seek " << file;
  std::string content;
  content.resize(size);
  CHECK_EQ(fread(&content[0], size, 1, handle), 1) << "Couldn't read " << file;
  CHECK_NE(fclose(handle), EOF) << "Couldn't close " << file;
  return content;
}

}  // namespace

proto::CompilationUnit ProtoExtractor::ExtractProtos(
    const std::vector<std::string>& proto_filenames,
    IndexWriter* index_writer) const {
  proto::CompilationUnit unit;

  CHECK(GetCurrentDirectory(unit.mutable_working_directory()));

  for (const std::string& proto : proto_filenames) {
    unit.add_argument(proto);
  }

  // Add path substitutions to src_tree.
  RecordingDiskSourceTree src_tree;
  src_tree.MapPath("", "");  // Add current directory to VFS.
  for (const auto& sub : path_substitutions) {
    src_tree.MapPath(sub.first, sub.second);
  }

  // Add protoc args to output.
  if (path_substitutions.size()) {
    unit.add_argument("--");
    for (auto& arg : PathSubstitutionsToArgs(path_substitutions)) {
      unit.add_argument(arg);
    }
  }

  // Import the toplevel proto(s), which will record paths of any transitive
  // dependencies to src_tree.
  {
    LoggingMultiFileErrorCollector err_collector;
    for (const std::string& fname : proto_filenames) {
      // Note that a separate importer instance is used for each top-level
      // import to avoid double-importing any subprotos, which would happen if
      // two top-level protos share any transitive dependencies.
      google::protobuf::compiler::Importer importer(&src_tree, &err_collector);
      CHECK(importer.Import(fname) != nullptr)
          << "Failed to import file: " << fname;

      unit.add_source_file(RelativizePath(fname, root_directory));
    }
  }

  // Write each toplevel proto and its transitive dependencies into the kzip.
  for (const std::string& filename : src_tree.opened_files()) {
    // Read file contents
    std::string file_contents;
    {
      std::unique_ptr<google::protobuf::io::ZeroCopyInputStream> in_stream(
          src_tree.Open(filename));
      CHECK(in_stream != nullptr) << "Can't open file: " << filename;

      const void* data = nullptr;
      int size = 0;
      while (in_stream->Next(&data, &size)) {
        file_contents.append(static_cast<const char*>(data), size);
      }
    }

    // Resolve path relative to the proto compiler's search paths.
    std::string full_path;
    CHECK(src_tree.VirtualFileToDiskFile(filename, &full_path))
        << "Error canonicalizing path: " << filename;
    // Make path relative to KYTHE_ROOT_DIRECTORY.
    full_path = RelativizePath(full_path, root_directory);

    // Write file to index.
    auto digest = index_writer->WriteFile(file_contents);
    CHECK(digest.ok()) << digest.status();

    // Record file info to compilation unit.
    proto::CompilationUnit::FileInput* file_input = unit.add_required_input();
    proto::VName vname = vname_gen.LookupVName(full_path);
    if (vname.corpus().empty()) {
      vname.set_corpus(std::string(corpus));
    }
    *file_input->mutable_v_name() = std::move(vname);
    file_input->mutable_info()->set_path(full_path);
    file_input->mutable_info()->set_digest(*digest);
  }

  return unit;
}

void ProtoExtractor::ConfigureFromEnv() {
  if (const char* env_corpus = getenv("KYTHE_CORPUS")) {
    corpus = env_corpus;
  }

  // File paths in the output kzip should be relative to this directory.
  if (const char* env_root_directory = getenv("KYTHE_ROOT_DIRECTORY")) {
    root_directory = env_root_directory;
  }

  // Configure VName generator.
  const char* vname_path = getenv("KYTHE_VNAMES");
  if (vname_path && strlen(vname_path) > 0) {
    std::string json = LoadFileOrDie(vname_path);
    std::string error_text;
    CHECK(vname_gen.LoadJsonString(json, &error_text))
        << "Could not parse vname generator configuration: " << error_text;
  }
}

}  // namespace lang_proto
}  // namespace kythe
