#include "analyzer.h"

#include <fcntl.h>
#include <iostream>
#include <memory>
#include "absl/container/node_hash_map.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/text_format.h"
#include "kythe/cxx/common/indexing/KytheCachingOutput.h"
#include "kythe/cxx/common/indexing/KytheGraphRecorder.h"
#include "kythe/cxx/common/protobuf_metadata_file.h"
#include "kythe/cxx/common/utf8_line_index.h"
#include "kythe/cxx/indexer/proto/indexer_frontend.h"
#include "kythe/cxx/indexer/proto/relative_paths.h"
#include "kythe/cxx/indexer/proto/source_tree.h"
#include "kythe/cxx/indexer/proto/vname.h"
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

// // Pretty-prints an EdgeKindID.
// std::string StringifyKind(EdgeKindID kind) {
//   return std::string(spelling_of(kind));
// }

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

// Analyzes a single textproto
class TextProtoAnalyzer {
 public:
  TextProtoAnalyzer(const proto::CompilationUnit* unit,
                    const std::vector<proto::FileData>* file_data,
                    std::string message_name,
                    const FileVNameGenerator* file_vnames,
                    KytheGraphRecorder* recorder)
      : compilation_unit_(unit),
        files_(file_data),
        message_name_(message_name),
        file_vnames_(file_vnames),
        recorder_(recorder) {}

  void Analyze();

 private:
  void AnalyzeMessage(
      const proto::VName& file_vname, const google::protobuf::Message* proto,
      const google::protobuf::Descriptor* descriptor,
      const google::protobuf::TextFormat::ParseInfoTree* parse_tree);

  void AddNode(const proto::VName& node_name, NodeKindID node_kind);
  proto::VName CreateAndAddAnchorNode(
      const proto::VName& file, const google::protobuf::FieldDescriptor* field,
      google::protobuf::TextFormat::ParseLocation loc);

  proto::VName VNameFromFullPath(const std::string& path);
  proto::VName VNameFromRelPath(const std::string& simplified_path);

  const proto::CompilationUnit* compilation_unit_;
  const std::vector<proto::FileData>* files_;
  const std::string message_name_;

  // A generator for consistently mapping file paths to VNames.
  const FileVNameGenerator* file_vnames_;

  KytheGraphRecorder* recorder_;
  std::unique_ptr<const UTF8LineIndex> line_index_;
};

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

  size_t len =
      field->is_extension() ? field->full_name().size() : field->name().size();

  int begin = line_index_->ComputeByteOffset(loc.line, loc.column);

  if (field->is_extension()) {
    begin += 1;  // Skip leading "[" for extension
  }

  const int end = begin + len;

  // LOG(ERROR) << "FIELD FULL NAME: " << field

  auto* const signature = anchor.mutable_signature();
  absl::StrAppend(signature, "@", begin, ":", end);

  AddNode(anchor, NodeKindID::kAnchor);
  recorder_->AddProperty(VNameRef(anchor), PropertyID::kLocationStartOffset,
                         begin);
  recorder_->AddProperty(VNameRef(anchor), PropertyID::kLocationEndOffset, end);

  return anchor;
}

VName TextProtoAnalyzer::VNameFromFullPath(const std::string& path) {
  for (const auto& input : compilation_unit_->required_input()) {
    if (input.info().path() == path) {
      return input.v_name();
    }
  }
  return file_vnames_->LookupVName(path);
}

VName TextProtoAnalyzer::VNameFromRelPath(const std::string& simplified_path) {
  // string full_path = FindWithDefault(*path_substitution_cache_,
  //                                         simplified_path, simplified_path);

  // TODO
  std::string full_path = simplified_path;
  return VNameFromFullPath(full_path);
}

void TextProtoAnalyzer::Analyze() {
  LOG(ERROR) << "Processing proto";

  CHECK(compilation_unit_->source_file().size() == 1)
      << "Expected CU to contain 1 source file";
  CHECK(files_->size() >= 2)
      << "Must provide at least 2 files: a textproto and 1+ .proto files";

  std::string pbtxt_name = compilation_unit_->source_file(0);

  // file node
  VName file_vname = VNameFromFullPath(pbtxt_name);
  recorder_->AddProperty(VNameRef(file_vname), NodeKindID::kFile);

  // TODO
  absl::node_hash_map<std::string, std::string> file_substitution_cache;
  auto path_substitutions = ParsePathSubstitutions(*compilation_unit_);

  // Load all proto files into in-memory SourceTree.
  PreloadedProtoFileTree file_reader(&path_substitutions,
                                     &file_substitution_cache);
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
  CHECK(pbtxt_file_data != nullptr)
      << "Couldn't find textproto source in file data";

  // Build protodb/pool
  LoggingMultiFileErrorCollector error_collector;
  google::protobuf::compiler::Importer proto_importer(&file_reader,
                                                      &error_collector);
  for (const std::string& fname : proto_filenames) {
    LOG(ERROR) << "importing into db/pool: " << fname;
    CHECK(proto_importer.Import(fname))
        << "Error importing proto file: " << fname;
  }
  const google::protobuf::DescriptorPool* descriptor_pool =
      proto_importer.pool();

  // record source text as a fact
  recorder_->AddProperty(VNameRef(file_vname), PropertyID::kText,
                         pbtxt_file_data->content());

  google::protobuf::TextFormat::Parser parser;
  // relax parser restrictions - even if the proto is partially ill-defined,
  // we'd like to analyze the parts that are good.
  parser.AllowPartialMessage(true);
  // parser.AllowUnknownExtension(true); // TODO: uncomment this
  // record symbol locations
  google::protobuf::TextFormat::ParseInfoTree parse_tree;
  parser.WriteLocationsTo(&parse_tree);

  const google::protobuf::Descriptor* descriptor =
      descriptor_pool->FindMessageTypeByName(message_name_);
  LOG(ERROR) << "msg type name: " << message_name_;
  CHECK(descriptor != nullptr) << "Unable to find proto in descriptor pool";

  google::protobuf::DynamicMessageFactory msg_factory;
  std::unique_ptr<google::protobuf::Message> proto(
      msg_factory.GetPrototype(descriptor)->New());
  CHECK(parser.ParseFromString(pbtxt_file_data->content(), proto.get()))
      << "Failed to parse text proto";

  line_index_ = absl::make_unique<UTF8LineIndex>(pbtxt_file_data->content());

  AnalyzeMessage(file_vname, proto.get(), descriptor, &parse_tree);
}

// TODO: handle comments?
void TextProtoAnalyzer::AnalyzeMessage(
    const proto::VName& file_vname, const google::protobuf::Message* proto,
    const google::protobuf::Descriptor* descriptor,
    const google::protobuf::TextFormat::ParseInfoTree* parse_tree) {
  const google::protobuf::Reflection* reflection = proto->GetReflection();

  // Iterate across all fields in the message. For proto1 and 2, each field has
  // a bit that tracks whether or not each field was set. This could be used to
  // only look at fields we know are set (with reflection.ListFields()). Proto3
  // however does not have "has" bits, so this approach would not work, thus we
  // look at every field.
  for (int i = 0; i < descriptor->field_count(); i++) {
    const FieldDescriptor* field = descriptor->field(i);
    LOG(ERROR) << "Looking for field: " << field->DebugString();

    if (!field->is_repeated()) {
      google::protobuf::TextFormat::ParseLocation loc =
          parse_tree->GetLocation(field, -1 /* non-repeated */);

      if (loc.line == -1) {
        LOG(ERROR) << "  Not found";
        continue;
      }

      // GetLocation() returns 0-indexed values, but UTF8LineIndex expects
      // 1-indexed line numbers.
      loc.line++;

      LOG(ERROR) << "  line " << loc.line << ", col: " << loc.column;
      VName anchor_vname = CreateAndAddAnchorNode(file_vname, field, loc);

      // add ref to proto field
      VName field_vname = ::kythe::lang_proto::VNameForDescriptor(
          field,
          [this](const std::string& path) { return VNameFromRelPath(path); });
      recorder_->AddEdge(VNameRef(anchor_vname), EdgeKindID::kRef,
                         VNameRef(field_vname));

      // Handle submessage
      if (field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
        google::protobuf::TextFormat::ParseInfoTree* subtree =
            parse_tree->GetTreeForNested(field, -1 /* non-repeated */);
        const google::protobuf::Message& submessage =
            reflection->GetMessage(*proto, field);
        const google::protobuf::Descriptor* subdescriptor =
            field->message_type();
        AnalyzeMessage(file_vname, &submessage, subdescriptor, subtree);
      }
    } else {
      // repeated
      int count = reflection->FieldSize(*proto, field);
      LOG(ERROR) << "  repeated field count " << count;

      if (count == 0) {
        continue;
      }

      VName field_vname = ::kythe::lang_proto::VNameForDescriptor(
          field,
          [this](const std::string& path) { return VNameFromRelPath(path); });

      // Add a ref for each instance of the repeated field.
      for (int i = 0; i < count; i++) {
        google::protobuf::TextFormat::ParseLocation loc =
            parse_tree->GetLocation(field, i);

        // GetLocation() returns 0-indexed values, but UTF8LineIndex expects
        // 1-indexed line numbers.
        loc.line++;

        CHECK(loc.line != -1) << "  Not found this should never happen";
        LOG(ERROR) << "  line " << loc.line << ", col: " << loc.column;
        VName anchor_vname = CreateAndAddAnchorNode(file_vname, field, loc);

        // add ref to proto field
        recorder_->AddEdge(VNameRef(anchor_vname), EdgeKindID::kRef,
                           VNameRef(field_vname));

        // Handle submessage
        if (field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
          google::protobuf::TextFormat::ParseInfoTree* subtree =
              parse_tree->GetTreeForNested(field, i);
          const google::protobuf::Message& submessage =
              reflection->GetRepeatedMessage(*proto, field, i);
          const google::protobuf::Descriptor* subdescriptor =
              field->message_type();
          AnalyzeMessage(file_vname, &submessage, subdescriptor, subtree);
        }
      }
    }
  }

  // Extensions

  std::vector<const FieldDescriptor*> set_fields;
  reflection->ListFields(*proto, &set_fields);
  for (const FieldDescriptor* field : set_fields) {
    // non-extensions are already handled above
    if (!field->is_extension()) {
      continue;
    }

    LOG(ERROR) << "Found set extension: " << field->DebugString();
    LOG(ERROR) << "  extension name: " << field->name();
    LOG(ERROR) << "  extension fullname: " << field->full_name();

    google::protobuf::TextFormat::ParseLocation loc =
        parse_tree->GetLocation(field, -1 /* non-repeated */);

    CHECK(loc.line != -1)
        << "Field is set, but can't find location. This should never happen";

    // GetLocation() returns 0-indexed values, but UTF8LineIndex expects
    // 1-indexed line numbers.
    loc.line++;

    absl::string_view substr = line_index_->GetSubstrFromLine(
        loc.line, loc.column, field->full_name().size());
    LOG(ERROR) << "TEXT OF FIELD: " << substr;

    LOG(ERROR) << "  line " << loc.line << ", col: " << loc.column;
    VName anchor_vname = CreateAndAddAnchorNode(file_vname, field, loc);

    // add ref to proto field
    VName field_vname = ::kythe::lang_proto::VNameForDescriptor(
        field,
        [this](const std::string& path) { return VNameFromRelPath(path); });
    recorder_->AddEdge(VNameRef(anchor_vname), EdgeKindID::kRef,
                       VNameRef(field_vname));

    // Handle submessage
    if (field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
      google::protobuf::TextFormat::ParseInfoTree* subtree =
          parse_tree->GetTreeForNested(field, -1 /*non-repeated */);
      const google::protobuf::Message& submessage =
          reflection->GetMessage(*proto, field);
      const google::protobuf::Descriptor* subdescriptor = field->message_type();
      AnalyzeMessage(file_vname, &submessage, subdescriptor, subtree);
    }
  }
  // CHECK(false);
}

void AnalyzeCompilationUnit(const proto::CompilationUnit& unit,
                            const std::vector<proto::FileData>& file_data,
                            std::string message_name,
                            const FileVNameGenerator& file_vnames,
                            KytheGraphRecorder* recorder) {
  TextProtoAnalyzer analyzer(&unit, &file_data, message_name, &file_vnames,
                             recorder);
  analyzer.Analyze();
}

}  // namespace lang_textproto
}  // namespace kythe
