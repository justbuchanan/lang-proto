#include "analyzer.h"

#include <fcntl.h>
#include <iostream>
#include <memory>

#include "absl/container/node_hash_map.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/text_format.h"
#include "kythe/cxx/common/indexing/KytheCachingOutput.h"
#include "kythe/cxx/common/indexing/KytheGraphRecorder.h"
#include "kythe/cxx/common/protobuf_metadata_file.h"
#include "kythe/cxx/indexer/proto/indexer_frontend.h"
#include "kythe/cxx/indexer/proto/source_tree.h"
#include "kythe/proto/analysis.pb.h"

namespace kythe {
namespace lang_textproto {

using google::protobuf::FieldDescriptor;
using kythe::proto::VName;

namespace {
// Pretty-prints a VName.
std::string StringifyNode(const VName& v_name) {
  return absl::StrCat(v_name.path(), ":", v_name.signature());
}

// Pretty-prints a NodeKindID.
std::string StringifyKind(NodeKindID kind) {
  return std::string(spelling_of(kind));
}

// Pretty-prints an EdgeKindID.
std::string StringifyKind(EdgeKindID kind) {
  return std::string(spelling_of(kind));
}

class LoggingMultiFileErrorCollector
    : public google::protobuf::compiler::MultiFileErrorCollector {
 public:
  void AddError(const std::string& filename, int line, int column,
                const std::string& message) override {
    LOG(ERROR) << "ErrorCollector filename: " << filename << "line " << line
               << "; column: " << column << "; msg: " << message;
  }

  void AddWarning(const std::string& filename, int line, int column,
                  const std::string& message) override {
    LOG(ERROR) << "ErrorCollector warning filename: " << filename << "line "
               << line << "; column: " << column << "; msg: " << message;
  }
};

}  // anonymous namespace

VName VNameFromFullPath(const std::string& path) {
  // for (const auto& input : unit_->required_input()) {
  //   if (input.info().path() == path) {
  //     return input.v_name();
  //   }
  // }
  // return file_vnames_->LookupVName(path);

  return VName();
}

VName VNameFromRelPath(const std::string& simplified_path) {
  // string full_path = FindWithDefault(*path_substitution_cache_,
  //                                         simplified_path, simplified_path);
  std::string full_path = simplified_path;
  return VNameFromFullPath(full_path);
}

// copied from proto_graph_builder.h
// Returns a VName for the given protobuf descriptor. Descriptors share
// various member names but do not participate in any sort of inheritance
// hierarchy, so we're stuck with a template.
template <typename SomeDescriptor>
VName VNameForDescriptor(const SomeDescriptor* descriptor) {
  VName vname;
  class PathSink : public ::google::protobuf::io::AnnotationCollector {
   public:
    PathSink(const std::function<VName(const std::string&)>& vname_for_rel_path,
             VName* vname)
        : vname_for_rel_path_(vname_for_rel_path), vname_(vname) {}

    void AddAnnotation(size_t begin_offset, size_t end_offset,
                       const std::string& file_path,
                       const std::vector<int>& path) override {
      *vname_ = VNameForProtoPath(vname_for_rel_path_(file_path), path);
    }

   private:
    const std::function<VName(const std::string&)>& vname_for_rel_path_;
    VName* vname_;
  };
  // VName vname_for_rel_path;
  const auto& vname_for_rel_path = [](const std::string& p) {
    // return VNameForProtoPath(file_vname, )
    // TODO
    VName v;
    return v;
  };
  PathSink path_sink(vname_for_rel_path, &vname);
  // We'd really like to use GetLocationPath here, but it's private, so
  // we have to go through some contortions. On the plus side, this is the
  // *exact* same code that protoc backends use for writing out annotations,
  // so if AddAnnotation ever changes we'll know.
  std::string s;
  ::google::protobuf::io::StringOutputStream stream(&s);
  ::google::protobuf::io::Printer printer(&stream, '$', &path_sink);
  printer.Print("$0$", "0", "0");
  printer.Annotate("0", descriptor);
  return vname;
}

void TextProtoAnalyzer::AddNode(const VName& node_name, NodeKindID node_kind) {
  VLOG(1) << "Writing node: " << StringifyNode(node_name) << "["
          << StringifyKind(node_kind) << "]";
  recorder_->AddProperty(VNameRef(node_name), node_kind);
}

VName TextProtoAnalyzer::CreateAndAddAnchorNode(
    const VName& file, const FieldDescriptor* field,
    google::protobuf::TextFormat::ParseLocation loc) {
  VName anchor = file;
  anchor.set_language(kLanguageName);

  const int begin = line_index_->ComputeByteOffset(loc.line, loc.column);
  const int end = begin + field->name().size();  // TODO: subtract one?

  auto* const signature = anchor.mutable_signature();
  absl::StrAppend(signature, "@", begin, ":", end);

  AddNode(anchor, NodeKindID::kAnchor);
  recorder_->AddProperty(VNameRef(anchor), PropertyID::kLocationStartOffset,
                         begin);
  recorder_->AddProperty(VNameRef(anchor), PropertyID::kLocationEndOffset, end);

  return anchor;
}

void TextProtoAnalyzer::DoIt() {
  LOG(ERROR) << "Processing proto";

  CHECK(compilation_unit_->source_file().size() == 1)
      << "Expected CU to contain 1 source file";

  CHECK(files_->size() >= 2)
      << "Must provide at least 2 files: a textproto and 1+ .proto files";

  std::string pbtxt_name = compilation_unit_->source_file(0);

  std::vector<std::pair<std::string, std::string>> path_substitutions;
  absl::node_hash_map<std::string, std::string> file_substitution_cache;
  // ParsePathSubstitutions(unit, &path_substitutions);
  // ProtoFileReader file_reader(&path_substitutions, &file_substitution_cache);
  // google::protobuf::util::ProtoFileParser proto_parser(&file_reader);

  PreloadedProtoFileTree file_reader(&path_substitutions,
                                     &file_substitution_cache);
  LoggingMultiFileErrorCollector error_collector;
  google::protobuf::compiler::Importer proto_importer(&file_reader,
                                                      &error_collector);

  std::vector<std::string> proto_filenames;

  const proto::FileData* pbtxt_file_data = nullptr;
  for (const auto& file_data : *files_) {
    // the textproto file is in the list, but we skip over it because it doesn't
    // go in the descriptor db - only .proto files.
    if (pbtxt_name == file_data.info().path()) {
      pbtxt_file_data = &file_data;
      continue;
    }

    LOG(ERROR) << "Added file to descriptor db: " << file_data.info().path();
    CHECK(file_reader.AddFile(file_data.info().path(), file_data.content()));

    proto_filenames.push_back(file_data.info().path());
  }

  // Build protodb/pool
  for (const std::string& fname : proto_filenames) {
    LOG(ERROR) << "importing into db/pool: " << fname;
    CHECK(proto_importer.Import(fname)) << "Error importing proto file: " << fname;
    // LOG(ERROR) << "Last SourceTree error: "
               // << file_reader.GetLastErrorMessage();
  }

  CHECK(pbtxt_file_data != nullptr)
      << "Couldn't find textproto source in file data";

  const google::protobuf::DescriptorPool* descriptor_pool =
      proto_importer.pool();

  google::protobuf::TextFormat::Parser parser;
  // relax parser restrictions - even if the proto is partially ill-defined,
  // we'd like to analyze the parts that are good.
  parser.AllowPartialMessage(true);
  parser.AllowUnknownExtension(true);

  // record symbol locations
  google::protobuf::TextFormat::ParseInfoTree infoTree;
  parser.WriteLocationsTo(&infoTree);

  const google::protobuf::Descriptor* msgType =
      descriptor_pool->FindMessageTypeByName(msg_type_name_);
  LOG(ERROR) << "msg type name: " << msg_type_name_;
  CHECK(msgType != nullptr) << "Unable to find proto in descriptor pool";
  LOG(ERROR) << "descriptor: " << msgType->DebugString();

  google::protobuf::DynamicMessageFactory msg_factory;
  std::unique_ptr<google::protobuf::Message> proto(
      msg_factory.GetPrototype(msgType)->New());
  if (!parser.ParseFromString(pbtxt_file_data->content(), proto.get())) {
    LOG(FATAL) << "Failed to parse text proto";
  }
  LOG(ERROR) << "parsed: \n" << proto->DebugString();

  const google::protobuf::Reflection* reflection = proto->GetReflection();

  std::vector<const FieldDescriptor*> fieldsThatAreSet;
  // access to fields/extensions unknown to the parser.
  // TODO: don't use ListFields() - it won't work with proto3 since protos don't
  // have HasBits()
  reflection->ListFields(*proto, &fieldsThatAreSet);

  for (auto& field : fieldsThatAreSet) {
    LOG(ERROR) << "Looking for field: " << field->DebugString();

    VName file_vname;

    // TODO: recursively handle message types
    // TODO: handle extensions / message sets
    // TODO: handle comments?

    if (!field->is_repeated()) {
      google::protobuf::TextFormat::ParseLocation loc =
          infoTree.GetLocation(field, -1 /* non-repeated */);
      if (loc.line == -1) {
        LOG(ERROR) << "  Not found";
      } else {
        LOG(ERROR) << "  line " << loc.line << ", col: " << loc.column;
        CreateAndAddAnchorNode(file_vname, field, loc);
      }
    } else {
      // repeated
      int count = reflection->FieldSize(*proto, field);
      LOG(ERROR) << "  repeated field count " << count;

      for (int i = 0; i < count; i++) {
        LOG(ERROR) << "  index " << i;
        google::protobuf::TextFormat::ParseLocation loc =
            infoTree.GetLocation(field, i);
        CHECK(loc.line != -1) << "  Not found this should never happen";
        LOG(ERROR) << "  line " << loc.line << ", col: " << loc.column;
        CreateAndAddAnchorNode(file_vname, field, loc);
      }
    }
  }
}

}  // namespace lang_textproto
}  // namespace kythe
