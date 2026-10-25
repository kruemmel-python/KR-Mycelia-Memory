#include "folder_collector.hpp"
#include "folder_policy.hpp"
#include <stdexcept>
namespace krmemory {ImportResult collect_changed_file(mycelia::DynamicAssociativeDatabase& db,const std::filesystem::path& file,const std::vector<std::filesystem::path>& roots){if(!permitted_folder(file.parent_path(),roots))throw std::runtime_error("file is outside approved collection roots");return import_file(db,file);}}
