#include "memory_import.hpp"
#include "memory_collections.hpp"
#include "media_metadata.hpp"
#include "native_ocr.hpp"
#include "native_media_analysis.hpp"
#include "memory_relations.hpp"
#include "memory_delete.hpp"
#include "context_policy.hpp"
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <zlib.h>
#endif
namespace fs=std::filesystem;
namespace krmemory { namespace {
std::string lower(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));});return s;}
const std::vector<std::string>& text_extensions(){static const std::vector<std::string> value={
    ".txt",".md",".markdown",".cpp",".cc",".cxx",".c",".h",".hh",".hpp",".hxx",".inl",
    ".cs",".java",".kt",".kts",".go",".rs",".swift",".py",".pyi",".js",".jsx",".ts",".tsx",
    ".css",".scss",".less",".sql",".sh",".bash",".ps1",".cmd",".bat",".cmake",".make",".mk",
    ".toml",".yaml",".yml",".ini",".cfg",".conf",".properties",".json",".csv",".xml",".html",
    ".htm",".log",".rtf",".docx",".xlsx",".pptx",".pdf"};return value;}
const std::vector<std::string>& media_extensions(){static const std::vector<std::string> value={
    ".png",".jpg",".jpeg",".bmp",".tif",".tiff",".webp",".wav",".mp3",".mp4",".m4a",".webm",".wmv",".wma"};return value;}
bool contains_extension(const std::vector<std::string>& values,std::string_view extension){return std::find(values.begin(),values.end(),extension)!=values.end();}
std::string collection_for(const fs::path& p){auto parent=p.parent_path().filename().string();return parent.empty()?"Imported files":parent;}
std::pair<std::string,std::string> changed_regions(std::string_view before,std::string_view after){
    std::size_t prefix{};while(prefix<before.size()&&prefix<after.size()&&before[prefix]==after[prefix])++prefix;
    if(prefix==before.size()&&prefix==after.size())return {};
    if(prefix){auto line=before.rfind('\n',prefix-1);prefix=line==std::string_view::npos?0:line+1;}
    std::size_t suffix{};while(suffix<before.size()-prefix&&suffix<after.size()-prefix&&before[before.size()-1-suffix]==after[after.size()-1-suffix])++suffix;
    if(suffix){auto shared=before.substr(before.size()-suffix);auto newline=shared.find('\n');suffix=newline==std::string_view::npos?0:suffix-newline-1;}
    auto removed=std::string(before.substr(prefix,before.size()-prefix-suffix));auto added=std::string(after.substr(prefix,after.size()-prefix-suffix));
    return {truncate_utf8(std::move(added),4096),truncate_utf8(std::move(removed),4096)};
}
std::string read_file(const fs::path& path){std::ifstream input(path,std::ios::binary);std::string data((std::istreambuf_iterator<char>(input)),{});if(!input&&!input.eof())throw std::runtime_error("Cannot read import source");return data;}
#ifdef _WIN32
std::uint16_t u16(const std::string& data,std::size_t at){if(at+2>data.size())throw std::runtime_error("truncated Office archive");return std::uint16_t((unsigned char)data[at])|(std::uint16_t((unsigned char)data[at+1])<<8);}
std::uint32_t u32(const std::string& data,std::size_t at){if(at+4>data.size())throw std::runtime_error("truncated Office archive");return std::uint32_t((unsigned char)data[at])|(std::uint32_t((unsigned char)data[at+1])<<8)|(std::uint32_t((unsigned char)data[at+2])<<16)|(std::uint32_t((unsigned char)data[at+3])<<24);}
std::string inflate_raw(std::string_view input,std::size_t expected){if(expected>32ull*1024*1024)throw std::runtime_error("Office document content exceeds 32 MiB safety limit");wchar_t module[32768]{};auto n=GetModuleFileNameW(nullptr,module,32768);auto library=n&&n<32768?fs::path(std::wstring(module,n)).parent_path()/L"zlib1.dll":fs::path{};auto dll=library.empty()?nullptr:LoadLibraryExW(library.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);if(!dll)throw std::runtime_error("Office import requires bundled zlib1.dll");using Init=int (ZEXPORT*)(z_streamp,int,const char*,int);using Inflate=int (ZEXPORT*)(z_streamp,int);using End=int (ZEXPORT*)(z_streamp);auto init=reinterpret_cast<Init>(GetProcAddress(dll,"inflateInit2_"));auto inflate=reinterpret_cast<Inflate>(GetProcAddress(dll,"inflate"));auto end=reinterpret_cast<End>(GetProcAddress(dll,"inflateEnd"));if(!init||!inflate||!end){FreeLibrary(dll);throw std::runtime_error("zlib1.dll is incomplete");}z_stream stream{};std::string output(expected,'\0');stream.next_in=reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));stream.avail_in=static_cast<uInt>(input.size());stream.next_out=reinterpret_cast<Bytef*>(output.data());stream.avail_out=static_cast<uInt>(output.size());auto result=init(&stream,-MAX_WBITS,ZLIB_VERSION,sizeof(stream));if(result==Z_OK)result=inflate(&stream,Z_FINISH);end(&stream);FreeLibrary(dll);if(result!=Z_STREAM_END)throw std::runtime_error("Cannot inflate Office document entry");output.resize(stream.total_out);return output;}
std::string strip_xml(std::string_view xml){std::string out;bool tag{};for(std::size_t i{};i<xml.size();++i){auto c=xml[i];if(c=='<'){tag=true;if(!out.empty()&&out.back()!=' ')out+=' ';continue;}if(c=='>'){tag=false;continue;}if(!tag)out+=c;}for(auto& pair:std::vector<std::pair<std::string,std::string>>{{"&amp;","&"},{"&lt;","<"},{"&gt;",">"},{"&quot;","\""},{"&apos;","'"}}){std::size_t pos{};while((pos=out.find(pair.first,pos))!=std::string::npos){out.replace(pos,pair.first.size(),pair.second);pos+=pair.second.size();}}return out;}
bool wanted_office_entry(std::string_view extension,std::string_view name){if(extension==".docx")return name=="word/document.xml"||name.rfind("word/header",0)==0||name.rfind("word/footer",0)==0;if(extension==".xlsx")return name=="xl/sharedStrings.xml"||name.rfind("xl/worksheets/",0)==0;if(extension==".pptx")return name.rfind("ppt/slides/slide",0)==0;return false;}
std::string extract_office_xml(const std::string& archive,std::string_view extension){std::size_t end=archive.rfind("PK\x05\x06");if(end==std::string::npos||end+22>archive.size())throw std::runtime_error("invalid Office ZIP archive");auto entries=u16(archive,end+10);auto central=u32(archive,end+16);std::string text;for(std::size_t entry{};entry<entries;++entry){if(u32(archive,central)!=0x02014b50)throw std::runtime_error("invalid Office ZIP index");auto method=u16(archive,central+10);auto compressed=u32(archive,central+20);auto uncompressed=u32(archive,central+24);auto name_size=u16(archive,central+28);auto extra=u16(archive,central+30);auto comment=u16(archive,central+32);auto local=u32(archive,central+42);if(central+46+name_size+extra+comment>archive.size())throw std::runtime_error("truncated Office ZIP index");auto name=std::string_view(archive).substr(central+46,name_size);if(wanted_office_entry(extension,name)){if(u32(archive,local)!=0x04034b50)throw std::runtime_error("invalid Office ZIP entry");auto local_name=u16(archive,local+26);auto local_extra=u16(archive,local+28);auto start=std::size_t(local)+30+local_name+local_extra;if(start+compressed>archive.size())throw std::runtime_error("truncated Office ZIP entry");auto bytes=std::string_view(archive).substr(start,compressed);std::string xml;if(method==0)xml=std::string(bytes);else if(method==8)xml=inflate_raw(bytes,uncompressed);else throw std::runtime_error("unsupported Office ZIP compression");text+=strip_xml(xml)+"\n";}central+=46+name_size+extra+comment;}if(text.empty())throw std::runtime_error("Office document has no supported text entries");return text;}
#else
std::string extract_office_xml(const std::string&,std::string_view){throw std::runtime_error("Office import is available on Windows only");}
#endif
}
bool import_format_supported(const fs::path& input){auto ext=lower(input.extension().string());return contains_extension(text_extensions(),ext)||contains_extension(media_extensions(),ext);}
std::string normalized_source_key(const fs::path& input){std::error_code error;auto path=fs::weakly_canonical(input,error);if(error)path=input.lexically_normal();auto value=path.generic_string();
#ifdef _WIN32
    value=lower(std::move(value));
#endif
    return value;
}
ImportResult import_file(mycelia::DynamicAssociativeDatabase& db,const fs::path& input,const std::atomic_bool* cancel){
    auto cancelled=[&]{return cancel&&cancel->load();};if(cancelled())throw std::runtime_error("import_cancelled");
    auto path=fs::weakly_canonical(input);if(!fs::is_regular_file(path))throw std::invalid_argument("Import requires a regular file");
    auto ext=lower(path.extension().string());
    const bool is_media=contains_extension(media_extensions(),ext);if(!is_media&&!contains_extension(text_extensions(),ext))throw std::runtime_error("Format requires an optional native decoder and is not imported");
    auto size=fs::file_size(path);const auto size_limit=is_media?2ull*1024*1024*1024:64ull*1024*1024;if(size>size_limit)throw std::runtime_error("Import exceeds format safety limit");
    std::string body,analysis_engine="native",analysis_status="completed";std::vector<AudioSegment> audio_segments;std::vector<VideoFrameAnalysis> video_frames;bool video_audio{};
    if(!is_media){
        if(ext==".pdf"){
            auto ocr=recognize_pdf_text(path);if(cancelled())throw std::runtime_error("import_cancelled");
            if(!ocr.available||!ocr.success)throw std::runtime_error("Native Windows PDF/OCR failed: "+ocr.detail);
            if(ocr.text.empty())throw std::runtime_error("Native Windows PDF/OCR found no text");
            body=std::move(ocr.text);analysis_engine="Windows.Data.Pdf + Windows.Media.Ocr ("+ocr.language+")";
        }else{
            auto raw=read_file(path);
            if(ext==".docx"||ext==".xlsx"||ext==".pptx")body=extract_office_xml(raw,ext);else body=std::move(raw);
        }
    }else{
        auto meta=inspect_media(path);body="Native media metadata: format="+meta.format+", bytes="+std::to_string(meta.bytes);if(meta.width)body+=", dimensions="+std::to_string(meta.width)+"x"+std::to_string(meta.height);if(meta.duration_seconds)body+=", duration_seconds="+std::to_string(*meta.duration_seconds);body+=".";
        static const std::vector<std::string> images={".png",".jpg",".jpeg",".bmp",".tif",".tiff",".webp"};
        if(std::find(images.begin(),images.end(),ext)!=images.end()){
            auto ocr=recognize_image_text(path);if(cancelled())throw std::runtime_error("import_cancelled");analysis_engine="Windows Imaging Component + Windows.Media.Ocr";
            if(ocr.success&&!ocr.text.empty())body+="\n\nRecognized text:\n"+ocr.text;
            else body+="\n\nOCR status: "+ocr.detail;
        }else if(ext==".wav"||ext==".mp3"||ext==".m4a"||ext==".wma"){
            auto audio=decode_audio_native(path,cancel);if(audio.cancelled||cancelled())throw std::runtime_error("import_cancelled");analysis_engine="Windows Media Foundation 16 kHz mono PCM + Windows SAPI";analysis_status=audio.transcription_status;audio_segments=std::move(audio.segments);body+="\n\nAudio preprocessing: "+std::to_string(audio.mono_pcm.size())+" samples, "+std::to_string(audio_segments.size())+" timed segment(s).\nTranscription status: "+analysis_status;
        }else{
            auto video=analyze_video_native(path,cancel);if(video.cancelled||cancelled())throw std::runtime_error("import_cancelled");video_audio=video.has_audio;analysis_engine="Windows Media Foundation + Windows.Media.Ocr + Windows SAPI";analysis_status=video.detail;video_frames=std::move(video.frames);body+="\n\nVideo analysis: "+std::to_string(video_frames.size())+" retained key/scene frame(s).";for(const auto& frame:video_frames)if(!frame.recognized_text.empty())body+="\n["+std::to_string(frame.timestamp_seconds)+" s] "+frame.recognized_text;
            if(video_audio){auto audio=decode_audio_native(path,cancel);if(audio.cancelled||cancelled())throw std::runtime_error("import_cancelled");audio_segments=std::move(audio.segments);analysis_status+="; audio track: "+audio.transcription_status;body+="\nAudio track preprocessing: "+std::to_string(audio_segments.size())+" timed segment(s). Transcription status: "+audio.transcription_status;}
        }
    }
    if(cancelled())throw std::runtime_error("import_cancelled");const auto collection_name=collection_for(path);mycelia::sql::Row collection_filter;collection_filter["title"]=collection_name;auto existing_collections=db.query_sql_like("memory_collections",collection_filter,1);auto collection_signature=existing_collections.empty()?create_collection(db,collection_name,"source_folder"):existing_collections.front().signature;
    const auto source_key=normalized_source_key(path);const auto now=(std::int64_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();const auto content=truncate_utf8(std::move(body),16*1024*1024);const auto content_hash=mycelia::DynamicAssociativeDatabase::sha256(content);
    std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> previous;for(const auto& candidate:db.query_sql_like("imported_memory")){auto key=candidate.data.find("source_key");auto source=candidate.data.find("source_path");if((key!=candidate.data.end()&&mycelia::sql::display(key->second)==source_key)||(source!=candidate.data.end()&&normalized_source_key(fs::path(mycelia::sql::display(source->second)))==source_key))previous.push_back(candidate);}
    auto sequence=[](const auto& record){for(const auto* key:{"source_version","timestamp"}){auto value=record.data.find(key);if(value!=record.data.end())try{return std::stoll(mycelia::sql::display(value->second));}catch(...){}}return std::int64_t{};};std::stable_sort(previous.begin(),previous.end(),[&](const auto& left,const auto& right){return sequence(left)>sequence(right);});
    if(!previous.empty()){auto hash=previous.front().data.find("content_sha256"),active=previous.front().data.find("source_active");if(hash!=previous.front().data.end()&&mycelia::sql::display(hash->second)==content_hash&&(active==previous.front().data.end()||mycelia::sql::display(active->second)=="True")){for(std::size_t index=1;index<previous.size();++index)delete_memory(db,previous[index].signature);return {previous.front().signature,collection_name,path,ext,(std::size_t)size};}}
    std::int64_t version=1;if(!previous.empty()){auto value=previous.front().data.find("source_version");if(value!=previous.front().data.end())try{version=std::stoll(mycelia::sql::display(value->second))+1;}catch(...){version=2;}else version=2;}
    mycelia::sql::Row row;row["title"]=path.filename().string();row["application"]="file_import";row["source_path"]=path.string();row["source_key"]=source_key;row["source_active"]=true;row["source_version"]=version;row["source_modified_clock"]=(std::int64_t)fs::last_write_time(path).time_since_epoch().count();row["content_sha256"]=content_hash;row["format"]=ext;row["content"]=content;row["document_section"]="full_document";row["collection"]=collection_name;row["analysis_engine"]=analysis_engine;row["analysis_status"]=analysis_status;row["timestamp"]=now;
    if(previous.empty()){row["change_kind"]="initial_import";row["last_change_added"]=truncate_utf8(content,4096);row["last_change_removed"]="";}else{auto old=previous.front().data.find("content");const auto before=old==previous.front().data.end()?std::string{}:mycelia::sql::display(old->second);auto regions=changed_regions(before,content);row["change_kind"]="source_update";row["last_change_added"]=std::move(regions.first);row["last_change_removed"]=std::move(regions.second);auto old_hash=previous.front().data.find("content_sha256");row["previous_content_sha256"]=old_hash==previous.front().data.end()?mycelia::DynamicAssociativeDatabase::sha256(before):mycelia::sql::display(old_hash->second);}
    std::string signature;if(previous.empty()){signature=db.store_sql_record("imported_memory",row);assign_to_collection(db,signature,collection_signature,"automatic_source_folder");}else{signature=previous.front().signature;for(const auto& child:db.query_sql_like("memory_media_segments")){auto source=child.data.find("source_signature");if(source!=child.data.end()&&mycelia::sql::display(source->second)==signature)delete_memory(db,child.signature);}db.update_sql_record(signature,row);for(std::size_t index=1;index<previous.size();++index)delete_memory(db,previous[index].signature);}
    for(std::size_t index{};index<audio_segments.size();++index){const auto& segment=audio_segments[index];mycelia::sql::Row item;item["title"]=path.filename().string()+" audio segment "+std::to_string(index+1);item["source_path"]=path.string();item["source_signature"]=signature;item["start_seconds"]=segment.start_seconds;item["end_seconds"]=segment.end_seconds;item["content"]=segment.transcript;item["rms"]=segment.rms;item["collection"]=collection_name;auto derived=db.store_sql_record("memory_media_segments",item);assign_to_collection(db,derived,collection_signature,"media_segment");relate(db,{derived,signature,"derived_from","native_audio_pipeline","decoded timed PCM segment",1.0});}
    for(std::size_t index{};index<video_frames.size();++index){const auto& frame=video_frames[index];mycelia::sql::Row item;item["title"]=path.filename().string()+" video frame "+std::to_string(index+1);item["source_path"]=path.string();item["source_signature"]=signature;item["timestamp_seconds"]=frame.timestamp_seconds;item["content"]=frame.recognized_text;item["scene_score"]=frame.scene_score;item["scene_change"]=frame.scene_change;item["perceptual_hash"]=std::to_string(frame.perceptual_hash);item["collection"]=collection_name;auto derived=db.store_sql_record("memory_media_segments",item);assign_to_collection(db,derived,collection_signature,"video_scene");relate(db,{derived,signature,"derived_from","native_video_pipeline","decoded timestamped scene/keyframe",1.0});}
    return {signature,collection_name,path,ext,(std::size_t)size};
}
std::vector<mycelia::DynamicAssociativeDatabase::ExternalRecord> list_collection(const mycelia::DynamicAssociativeDatabase& db,std::string_view collection){mycelia::sql::Row filter;filter["collection"]=std::string(collection);return db.query_sql_like("imported_memory",filter);}
}
