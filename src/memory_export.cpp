#include "memory_export.hpp"
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace krmemory {namespace {std::string escape(std::string_view text){std::string out;for(unsigned char c:text){switch(c){case '"':out+="\\\"";break;case '\\':out+="\\\\";break;case '\n':out+="\\n";break;case '\r':out+="\\r";break;case '\t':out+="\\t";break;default:if(c>=32)out+=char(c);}}return out;}}
std::size_t export_memories_json(const mycelia::DynamicAssociativeDatabase& db,const std::filesystem::path& destination,std::optional<std::string_view> table){if(!destination.is_absolute())throw std::invalid_argument("export path must be absolute");auto rows=db.query_sql_like(table);auto temporary=destination;temporary+=L".tmp";std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output)throw std::runtime_error("cannot create export");output<<"{\"format\":\"kr-mycelia-memory-export-v1\",\"records\":[";bool first=true;for(const auto& record:rows){if(!first)output<<',';first=false;output<<"{\"signature\":\""<<escape(record.signature)<<"\",\"table\":\""<<escape(record.table)<<"\",\"fields\":{";bool first_field=true;for(const auto& [key,value]:record.data){if(!first_field)output<<',';first_field=false;output<<'\"'<<escape(key)<<"\":\""<<escape(mycelia::sql::display(value))<<'\"';}output<<"}}";}output<<"]}";output.close();if(!output){std::filesystem::remove(temporary);throw std::runtime_error("cannot finish export");}
#ifdef _WIN32
if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::filesystem::remove(temporary);throw std::runtime_error("cannot replace export");}
#else
std::error_code error;std::filesystem::rename(temporary,destination,error);if(error){std::filesystem::remove(temporary);throw std::runtime_error("cannot replace export");}
#endif
return rows.size();}
}
