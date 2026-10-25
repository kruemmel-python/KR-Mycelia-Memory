#include "memory_import.hpp"
#include "memory_relations.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void u16(std::ofstream& out,std::uint16_t value){out.put(char(value));out.put(char(value>>8));}
void u32(std::ofstream& out,std::uint32_t value){u16(out,std::uint16_t(value));u16(out,std::uint16_t(value>>16));}
void wave(const std::filesystem::path& path){constexpr std::uint32_t rate=8000,count=96000,data=count*2;std::ofstream out(path,std::ios::binary);out.write("RIFF",4);u32(out,36+data);out.write("WAVEfmt ",8);u32(out,16);u16(out,1);u16(out,1);u32(out,rate);u32(out,rate*2);u16(out,2);u16(out,16);out.write("data",4);u32(out,data);for(std::uint32_t i{};i<count;++i)u16(out,std::uint16_t(std::int16_t(std::sin(2*3.141592653589793*220*i/rate)*10000)));}
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}

int main(){try{
    auto root=std::filesystem::temp_directory_path()/"kr_mycelia_media_dad";std::filesystem::create_directories(root);auto source=root/"meeting.wav";wave(source);mycelia::DynamicAssociativeDatabase db;auto imported=krmemory::import_file(db,source);auto segments=db.query_sql_like("memory_media_segments");require(segments.size()>=2,"timed audio segments were not stored");for(const auto& segment:segments){auto parent=segment.data.find("source_signature"),begin=segment.data.find("start_seconds"),end=segment.data.find("end_seconds");require(parent!=segment.data.end()&&mycelia::sql::display(parent->second)==imported.signature,"media source reference missing");require(begin!=segment.data.end()&&end!=segment.data.end(),"media timestamps missing");bool linked{};for(const auto& relation:krmemory::related(db,segment.signature)){auto type=relation.data.find("relation_type"),target=relation.data.find("to_signature");linked|=type!=relation.data.end()&&target!=relation.data.end()&&mycelia::sql::display(type->second)=="derived_from"&&mycelia::sql::display(target->second)==imported.signature;}require(linked,"derived media relation missing");}
    auto restored=mycelia::DynamicAssociativeDatabase::from_snapshot_bytes(db.snapshot_bytes());require(restored.get_sql_record(imported.signature).has_value(),"original media missing after restore");require(restored.query_sql_like("memory_media_segments").size()==segments.size(),"media segments missing after restore");std::filesystem::remove_all(root);std::cout<<"PASS: native media -> timed DAD segments -> source relations -> snapshot restore\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
