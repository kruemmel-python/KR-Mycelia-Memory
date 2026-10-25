#include "mycelia.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace mycelia {
namespace {
constexpr std::size_t max_file=128*1024*1024, max_string=16*1024*1024, max_items=1'000'000;
struct Binary {
    std::string b;
    std::size_t pos=0;
    void u8(std::uint8_t x){b.push_back(static_cast<char>(x));}
    void u64(std::uint64_t x){for(int i=0;i<8;++i)u8(static_cast<std::uint8_t>(x>>(i*8)));}
    void d(double x){if(!std::isfinite(x))throw std::runtime_error("nonfinite state");u64(std::bit_cast<std::uint64_t>(x));}
    void str(const std::string& s){if(s.size()>max_string)throw std::runtime_error("string limit");u64(s.size());b+=s;}
    std::uint8_t read8(){if(pos>=b.size())throw std::runtime_error("truncated snapshot");return static_cast<std::uint8_t>(b[pos++]);}
    std::uint64_t read64(){std::uint64_t x=0;for(int i=0;i<8;++i)x|=std::uint64_t(read8())<<(i*8);return x;}
    std::size_t count(std::size_t cap=max_items){auto x=read64();if(x>cap)throw std::runtime_error("snapshot size limit");return static_cast<std::size_t>(x);}
    double rd(){double x=std::bit_cast<double>(read64());if(!std::isfinite(x))throw std::runtime_error("nonfinite snapshot field");return x;}
    std::string rs(){auto n=count(max_string);if(n>b.size()-pos)throw std::runtime_error("truncated string");auto s=b.substr(pos,n);pos+=n;return s;}
};
void row_write(Binary& w,const sql::Row& row){w.u64(row.size());for(const auto& [k,v]:row){w.str(k);std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;
    if constexpr(std::is_same_v<T,std::nullptr_t>)w.u8(0);
    else if constexpr(std::is_same_v<T,bool>){w.u8(1);w.u8(x?1:0);}
    else if constexpr(std::is_same_v<T,std::int64_t>){w.u8(2);w.u64(std::bit_cast<std::uint64_t>(x));}
    else if constexpr(std::is_same_v<T,double>){w.u8(3);w.d(x);}
    else {w.u8(4);w.str(x);}},v);}}
sql::Row row_read(Binary& r){sql::Row row;auto n=r.count();for(std::size_t i=0;i<n;++i){auto key=r.rs();sql::Value v;switch(r.read8()){
case 0:v=nullptr;break;case 1:{auto b=r.read8();if(b>1)throw std::runtime_error("invalid boolean");v=(b==1);break;}
case 2:v=std::bit_cast<std::int64_t>(r.read64());break;case 3:v=r.rd();break;case 4:v=r.rs();break;
default:throw std::runtime_error("unknown value tag");}
if(!row.emplace(std::move(key),std::move(v)).second)throw std::runtime_error("duplicate column");}return row;}
}
struct SnapshotCodec {
static std::string encode(const DynamicAssociativeDatabase& db){
    Binary w;w.b="MYCELIA6";w.u64(1);
    w.u64(db.retention_);w.d(db.noise_gain_);w.d(db.mood_gain_);w.d(db.agent_gain_);w.d(db.noise_factor_);
    w.u64(db.insertion_order_.size());
    for(const auto& sig:db.insertion_order_){auto it=db.attractors_.find(sig);if(it==db.attractors_.end())throw std::runtime_error("invalid insertion index");const auto& p=it->second;
        w.str(p.signature);w.d(p.energy_mean);w.d(p.pheromone_mean);w.d(p.nutrient_mean);
        for(double x:p.mood_vector)w.d(x);w.d(p.stability);w.u64(p.visits);w.str(p.energy_hash);w.str(p.source_table);
    }
    w.u64(db.external_records_.size());for(const auto& [sig,row]:db.external_records_){w.str(sig);row_write(w,row);}
    w.u64(db.energy_hash_history_.size());for(const auto& h:db.energy_hash_history_)w.str(h);
    w.u8(db.last_query_?1:0);if(db.last_query_){const auto& q=*db.last_query_;w.str(q.cue);w.d(q.intensity);w.u64(q.returned);w.u64(q.signatures.size());for(const auto& s:q.signatures)w.str(s);}
    if(w.b.size()>max_file-64)throw std::runtime_error("snapshot exceeds size limit");
    const auto hash=DynamicAssociativeDatabase::sha256(w.b);w.b+=hash;
    return w.b;
}
static DynamicAssociativeDatabase decode(std::string_view bytes){
    if(bytes.size()<8+8+64||bytes.size()>max_file)throw std::runtime_error("snapshot size invalid");
    Binary r;r.b.assign(bytes);
    const auto payload_len=r.b.size()-64;const auto expected=r.b.substr(payload_len);
    if(DynamicAssociativeDatabase::sha256(std::string_view(r.b.data(),payload_len))!=expected)throw std::runtime_error("snapshot SHA-256 mismatch");
    r.b.resize(payload_len);
    if(r.b.substr(0,8)!="MYCELIA6")throw std::runtime_error("snapshot format mismatch");r.pos=8;
    if(r.read64()!=1)throw std::runtime_error("unsupported snapshot version");
    auto retention=r.count();auto ng=r.rd(),mg=r.rd(),ag=r.rd(),noise=r.rd();
    if(retention<4||ng<0||mg<0||ag<0.05||!std::isfinite(noise))throw std::runtime_error("invalid snapshot parameters");
    DynamicAssociativeDatabase db(retention,ng,mg,ag);db.noise_factor_=noise;
    auto count=r.count();if(count>retention)throw std::runtime_error("retention exceeded");
    for(std::size_t i=0;i<count;++i){Pattern p;p.signature=r.rs();p.energy_mean=r.rd();p.pheromone_mean=r.rd();p.nutrient_mean=r.rd();for(auto& x:p.mood_vector)x=r.rd();p.stability=r.rd();p.visits=r.count();p.energy_hash=r.rs();p.source_table=r.rs();
        if(p.signature.empty()||p.visits==0||p.stability<0||p.stability>1||db.attractors_.contains(p.signature))throw std::runtime_error("invalid or duplicate pattern");
        db.insertion_order_.push_back(p.signature);db.attractors_.emplace(p.signature,std::move(p));}
    auto ext=r.count();for(std::size_t i=0;i<ext;++i){auto sig=r.rs();auto* p=db.get_pattern(sig);if(!p||p->source_table.empty())throw std::runtime_error("orphan external record");
        auto row=row_read(r);if(!db.external_records_.emplace(sig,std::move(row)).second)throw std::runtime_error("duplicate external signature");
        std::string table=p->source_table;std::transform(table.begin(),table.end(),table.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});db.table_index_[table].insert(sig);
    }
    auto history=r.count();if(history>retention)throw std::runtime_error("history exceeds retention");for(std::size_t i=0;i<history;++i)db.energy_hash_history_.push_back(r.rs());
    auto has_query=r.read8();if(has_query>1)throw std::runtime_error("invalid query tag");if(has_query){QueryRecord q;q.cue=r.rs();q.intensity=r.rd();q.returned=r.count();auto ns=r.count();for(std::size_t i=0;i<ns;++i)q.signatures.push_back(r.rs());db.last_query_=std::move(q);}
    if(r.pos!=r.b.size())throw std::runtime_error("unexpected trailing payload");
    return db;
}
};
std::string DynamicAssociativeDatabase::snapshot_bytes() const { return SnapshotCodec::encode(*this); }
DynamicAssociativeDatabase DynamicAssociativeDatabase::from_snapshot_bytes(std::string_view bytes) { return SnapshotCodec::decode(bytes); }
void DynamicAssociativeDatabase::save_snapshot(const std::filesystem::path& path) const{
    auto bytes=SnapshotCodec::encode(*this);
    auto tmp=path;tmp+=".tmp";
    if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
    {std::ofstream f(tmp,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("cannot open temp snapshot");f.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));f.flush();if(!f)throw std::runtime_error("snapshot write failed");f.close();if(!f)throw std::runtime_error("snapshot close failed");}
    auto backup=path;backup+=".bak";
    try{
        if(std::filesystem::exists(path)){
            if(std::filesystem::exists(backup))std::filesystem::remove(backup);
            std::filesystem::rename(path,backup);
        }
        std::filesystem::rename(tmp,path);
    }catch(...){if(!std::filesystem::exists(path)&&std::filesystem::exists(backup))std::filesystem::rename(backup,path);throw;}
}
DynamicAssociativeDatabase DynamicAssociativeDatabase::load_snapshot(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("snapshot not readable");
    auto n=f.tellg();if(n<0||n>static_cast<std::streamoff>(max_file))throw std::runtime_error("snapshot size invalid");
    std::string bytes(static_cast<std::size_t>(n),'\0');f.seekg(0);f.read(bytes.data(),n);
    if(!f)throw std::runtime_error("snapshot read failed");
    return SnapshotCodec::decode(bytes);
}
}
