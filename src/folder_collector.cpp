#include "folder_collector.hpp"
#include "folder_policy.hpp"
#include "memory_delete.hpp"
#include <iterator>
#include <stdexcept>
#include <unordered_set>
namespace krmemory {
namespace {
bool beneath(std::string_view path,std::string_view root){return path.size()>=root.size()&&path.compare(0,root.size(),root)==0&&(path.size()==root.size()||path[root.size()]=='/');}
std::filesystem::path owning_root(const std::filesystem::path& file,const std::vector<std::filesystem::path>& roots){
    const auto key=normalized_source_key(file);std::filesystem::path selected;std::size_t selected_size{};
    for(const auto& root:roots){auto root_key=normalized_source_key(root);if(root_key.size()>selected_size&&beneath(key,root_key)){selected=root;selected_size=root_key.size();}}
    return selected;
}
bool project_root(const std::filesystem::path& root){
    std::error_code error;
    for(const auto* marker:{".git","CMakeLists.txt","package.json","Cargo.toml","pyproject.toml","README.md"})if(std::filesystem::exists(root/marker,error)&&!error)return true;else error.clear();
    return false;
}
std::string project_for(const std::filesystem::path& file,const std::filesystem::path& root){
    if(root.empty())return file.parent_path().filename().string();
    if(project_root(root))return root.filename().string();
    std::error_code error;auto relative=std::filesystem::relative(file,root,error);if(!error&&!relative.empty()){auto first=relative.begin(),last=relative.end();if(first!=last&&std::next(first)!=last)return first->string();}
    return root.filename().string();
}
}
ImportResult collect_changed_file(mycelia::DynamicAssociativeDatabase& db,const std::filesystem::path& file,const std::vector<std::filesystem::path>& roots,const std::atomic_bool* cancel){
    if(!permitted_folder(file.parent_path(),roots))throw std::runtime_error("file is outside approved collection roots");
    auto result=import_file(db,file,cancel);auto record=db.get_sql_record(result.signature);auto root=owning_root(file,roots);
    if(record){auto row=record->data;row["watch_root"]=root.string();row["project"]=project_for(file,root);db.update_sql_record(result.signature,row);}
    return result;
}
std::size_t remove_collected_source(mycelia::DynamicAssociativeDatabase& db,const std::filesystem::path& file){const auto key=normalized_source_key(file);std::vector<std::string> matches;for(const auto& record:db.query_sql_like("imported_memory")){auto stored_key=record.data.find("source_key"),source=record.data.find("source_path");if((stored_key!=record.data.end()&&mycelia::sql::display(stored_key->second)==key)||(source!=record.data.end()&&normalized_source_key(std::filesystem::path(mycelia::sql::display(source->second)))==key))matches.push_back(record.signature);}std::size_t removed{};for(const auto& signature:matches)if(delete_memory(db,signature).deleted)++removed;return removed;}
FolderReconcileResult reconcile_collected_root(mycelia::DynamicAssociativeDatabase& db,const std::filesystem::path& root,const std::vector<std::filesystem::path>& roots,std::size_t maximum_files,const std::atomic_bool* cancel){if(!permitted_folder(root,roots))throw std::runtime_error("root is outside approved collection roots");FolderReconcileResult result;std::unordered_set<std::string> present;std::error_code error;std::filesystem::recursive_directory_iterator iterator(root,std::filesystem::directory_options::skip_permission_denied,error),end;while(iterator!=end){if(cancel&&cancel->load())return result;if(error){error.clear();iterator.increment(error);continue;}if(iterator->is_regular_file(error)&&!error){const auto path=iterator->path();if(collector_format_supported(path)){if(present.size()>=maximum_files){result.truncated=true;break;}present.insert(normalized_source_key(path));try{collect_changed_file(db,path,roots,cancel);++result.imported;}catch(...){++result.failed;}}}else error.clear();iterator.increment(error);}
if(result.truncated)return result;const auto root_key=normalized_source_key(root);std::vector<std::filesystem::path> stale;for(const auto& record:db.query_sql_like("imported_memory")){if(cancel&&cancel->load())return result;auto source=record.data.find("source_path");if(source==record.data.end())continue;auto path=std::filesystem::path(mycelia::sql::display(source->second));auto key=normalized_source_key(path);if(collector_format_supported(path)&&key.size()>root_key.size()&&key.compare(0,root_key.size(),root_key)==0&&key[root_key.size()]=='/'&&!present.contains(key))stale.push_back(std::move(path));}for(const auto& path:stale){if(cancel&&cancel->load())return result;result.removed+=remove_collected_source(db,path);}return result;}
}
