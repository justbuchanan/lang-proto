#ifndef KYTHE_CXX_INDEXER_TEXTPROTO_ANALYZER_H_
#define KYTHE_CXX_INDEXER_TEXTPROTO_ANALYZER_H_

#include <cstdio>
#include <functional>
#include <string>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "kythe/cxx/common/file_vname_generator.h"
#include "kythe/cxx/common/indexing/KytheGraphRecorder.h"
#include "kythe/cxx/common/protobuf_metadata_file.h"
#include "kythe/cxx/common/utf8_line_index.h"
#include "kythe/proto/analysis.pb.h"

namespace kythe {
namespace lang_textproto {

// Analyzes a single textproto
class TextProtoAnalyzer {
 public:
  TextProtoAnalyzer(const proto::CompilationUnit* cu,
                    const std::vector<proto::FileData>* file_data,
                    std::string msgTypeName, FileVNameGenerator* file_vnames,
                    KytheGraphRecorder* recorder)
      : compilation_unit_(cu),
        files_(file_data),
        msg_type_name_(msgTypeName),
        file_vnames_(file_vnames),
        recorder_(recorder) {}

  void Analyze();

 private:
  void AnalyzeMessage(
      const proto::VName& file_vname, const google::protobuf::Message* proto,
      const google::protobuf::Descriptor* descriptor,
      const google::protobuf::TextFormat::ParseInfoTree* infoTree);

  // void ProcessField(const proto::VName& file_vname,
  //                   const proto::VName& field_vname,
  //                   const google::protobuf::FieldDescriptor* field,
  //                   const google::protobuf::TextFormat::ParseInfoTree*
  //                   infoTree, int i);

  void AddNode(const proto::VName& node_name, NodeKindID node_kind);
  proto::VName CreateAndAddAnchorNode(
      const proto::VName& file, const google::protobuf::FieldDescriptor* field,
      google::protobuf::TextFormat::ParseLocation loc);

  // copied from proto_graph_builder.h
  // Returns a VName for the given protobuf descriptor. Descriptors share
  // various member names but do not participate in any sort of inheritance
  // hierarchy, so we're stuck with a template.
  template <typename SomeDescriptor>
  proto::VName VNameForDescriptor(const SomeDescriptor* descriptor) {
    proto::VName vname;
    class PathSink : public ::google::protobuf::io::AnnotationCollector {
     public:
      PathSink(const std::function<proto::VName(const std::string&)>&
                   vname_for_rel_path,
               proto::VName* vname)
          : vname_for_rel_path_(vname_for_rel_path), vname_(vname) {}

      void AddAnnotation(size_t begin_offset, size_t end_offset,
                         const std::string& file_path,
                         const std::vector<int>& path) override {
        *vname_ = VNameForProtoPath(vname_for_rel_path_(file_path), path);
      }

     private:
      const std::function<proto::VName(const std::string&)>&
          vname_for_rel_path_;
      proto::VName* vname_;
    };
    const auto& vname_for_rel_path = [this](const std::string& path) {
      return VNameFromRelPath(path);
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

  proto::VName VNameFromFullPath(const std::string& path);
  proto::VName VNameFromRelPath(const std::string& simplified_path);

  const proto::CompilationUnit* compilation_unit_;
  const std::vector<proto::FileData>* files_;
  const std::string msg_type_name_;

  // A generator for consistently mapping file paths to VNames.
  FileVNameGenerator* file_vnames_;

  KytheGraphRecorder* recorder_;
  std::unique_ptr<const UTF8LineIndex> line_index_;
};

// The canonical name for the textproto language in Kythe
static const char kLanguageName[] = "protobuf_textformat";

};  // namespace lang_textproto
};  // namespace kythe

#endif  // KYTHE_CXX_INDEXER_TEXTPROTO_ANALYZER_H_
