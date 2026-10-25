#include "mycelia.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mycelia;
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
int main(){
    auto path=std::filesystem::temp_directory_path()/"kr_mycelia_snapshot_v06_test.bin";
    auto backup=path;backup+=".bak";std::filesystem::remove(path);std::filesystem::remove(backup);
    try{
        DynamicAssociativeDatabase db(4);
        sql::Row row{{"id",std::int64_t(9)},{"title",std::string("Hallo Welt")},{"valid",true},{"none",nullptr},{"fraction",1.25}};
        auto sig=db.store_sql_record("documents",row);
        auto p=db.get_pattern(sig);require(p!=nullptr,"missing pattern");
        auto original=p->energy_mean;
        db.store_sql_record("documents",row);require(db.get_pattern(sig)->visits==2,"visits");
        db.associative_sql_lookup("Hallo");
        db.save_snapshot(path);
        auto restored=DynamicAssociativeDatabase::load_snapshot(path);
        require(restored.get_sql_record(sig).has_value(),"record lost");
        require(restored.get_sql_record(sig)->data==row,"row changed");
        require(restored.get_pattern(sig)->energy_mean==original,"energy changed");
        require(restored.get_pattern(sig)->visits==2,"visits changed");
        require(restored.list_external_tables().size()==1,"table index absent");
        require(restored.last_query().has_value(),"last query lost");
        restored.save_snapshot(path);require(std::filesystem::exists(backup),"backup missing");
        {std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);file.seekp(35);file.put('X');}
        bool failed=false;try{auto invalid=DynamicAssociativeDatabase::load_snapshot(path);(void)invalid;}catch(const std::runtime_error&){failed=true;}
        require(failed,"corruption accepted");
        auto backup_db=DynamicAssociativeDatabase::load_snapshot(backup);require(backup_db.external_record_count()==1,"backup unreadable");
        for(int i=0;i<6;++i){try{db.store_sql_record("documents",sql::Row{{"id",std::int64_t(i+100)}});}catch(const std::runtime_error& e){require(std::string(e.what())=="pattern evicted by retention policy","unexpected insertion failure");}}
        require(db.attractor_count()==4,"retention exceeded");
        require(db.external_record_count()<=db.attractor_count(),"orphan records after retention");
        db.save_snapshot(path);auto kept=DynamicAssociativeDatabase::load_snapshot(path);
        require(kept.external_record_count()==db.external_record_count(),"retention snapshot mismatch");
        std::filesystem::remove(path);std::filesystem::remove(backup);
        std::cout<<"snapshot tests passed\n";
    }catch(...){std::filesystem::remove(path);std::filesystem::remove(backup);throw;}
}
