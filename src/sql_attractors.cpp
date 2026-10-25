#include "mycelia.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>
namespace mycelia {
namespace {
std::string lower(std::string_view s) {
    std::string v(s);std::transform(v.begin(),v.end(),v.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return v;
}
std::string trim(std::string_view s) {
    auto first=s.find_first_not_of(" \t\n\r"); if(first==s.npos)return {};
    auto last=s.find_last_not_of(" \t\n\r");return std::string(s.substr(first,last-first+1));
}
std::string repr_string(std::string_view v) {
    // CPython repr chooses double quotes when apostrophes occur but double quotes do not.
    const char quote=(v.find('\'')!=v.npos && v.find('"')==v.npos)?'"':'\'';
    std::string out(1,quote);
    for(unsigned char c:v) {
        if(c==static_cast<unsigned char>(quote) || c=='\\') {out+='\\';out+=char(c);}
        else if(c=='\n')out+="\\n";
        else if(c=='\r')out+="\\r";
        else if(c=='\t')out+="\\t";
        else if(c<32 || c==127) {constexpr char hex[]="0123456789abcdef";out+="\\x";out+=hex[c>>4];out+=hex[c&15];}
        else out+=char(c);
    }
    out+=quote;return out;
}
std::string py_repr(const sql::Value& val) {
    return std::visit([](const auto& v)->std::string {
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,std::nullptr_t>)return "None";
        else if constexpr(std::is_same_v<T,bool>)return v?"True":"False";
        else if constexpr(std::is_same_v<T,std::string>)return repr_string(v);
        else if constexpr(std::is_same_v<T,std::int64_t>)return std::to_string(v);
        else {
            if(!std::isfinite(v)) throw std::invalid_argument("nonfinite SQL number is not supported");
            std::ostringstream ss;ss.imbue(std::locale::classic());ss<<std::setprecision(17)<<v;
            auto result=ss.str();if(result.find_first_of(".eE")==std::string::npos)result+=".0";
            return result;
        }
    },val);
}
std::string encode_row(std::string_view table,const sql::Row& row) {
    std::string encoded(table);encoded+='|';bool first=true;
    for(const auto& [key,val]:row) {
        if(!first)encoded+=';';first=false;encoded+=key;encoded+='=';encoded+=py_repr(val);
    }
    return encoded;
}
std::array<unsigned char,32> hash_bytes(std::string_view text) {
    auto hex=DynamicAssociativeDatabase::sha256(text);
    auto digit=[](char c)->unsigned {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::logic_error("invalid internal digest");};
    std::array<unsigned char,32> b{};for(size_t i=0;i<32;++i)b[i]=static_cast<unsigned char>((digit(hex[2*i])<<4)|digit(hex[2*i+1]));return b;
}
double component(const std::array<unsigned char,32>& b,size_t offset,bool signed_value=true) {
    double x=static_cast<double>((unsigned(b[offset])<<8)|unsigned(b[offset+1]))/65535.0;
    return signed_value?2.0*x-1.0:x;
}
bool auxiliary_search_table(std::string_view table) {
    return table=="search_history"||table=="memory_chat_history"||table=="agent_audit"||
           table=="mcp_agent_permissions"||table=="agent_registry";
}
std::vector<std::string> lexical_terms(std::string_view cue) {
    std::vector<std::string> terms;
    for(std::size_t at{};at<cue.size();){
        while(at<cue.size()&&!std::isalnum(static_cast<unsigned char>(cue[at]))&&cue[at]!='-'&&cue[at]!='_')++at;
        auto end=at;while(end<cue.size()&&(std::isalnum(static_cast<unsigned char>(cue[end]))||cue[end]=='-'||cue[end]=='_'))++end;
        if(end-at>2)terms.push_back(lower(cue.substr(at,end-at)));at=end;
    }
    return terms;
}
std::size_t record_lexical_score(const DynamicAssociativeDatabase::ExternalRecord& record,
                                 std::string_view cue,const std::vector<std::string>& terms) {
    std::string haystack=lower(record.table);for(const auto&[key,value]:record.data)haystack+=' '+lower(key+' '+sql::display(value));
    std::size_t score{};auto normalized=lower(trim(cue));if(!normalized.empty()&&haystack.find(normalized)!=std::string::npos)score+=100000+normalized.size()*100;
    for(const auto& term:terms)if(haystack.find(term)!=std::string::npos)score+=100+term.size();return score;
}
}
std::string DynamicAssociativeDatabase::store_sql_record(std::string_view table,const sql::Row& row,
    double stability,std::optional<std::array<double,3>> mood,std::optional<double> chaos) {
    if(table.empty())throw std::invalid_argument("empty table");
    const std::string name=trim(table);if(name.empty())throw std::invalid_argument("empty table");
    const auto hash=sha256(encode_row(name,row));auto digest=hash_bytes(encode_row(name,row));
    std::ostringstream ss;ss.imbue(std::locale::classic());ss<<std::fixed<<std::setprecision(6)<<chaos.value_or(noise_factor_);
    const auto sig=sha256("sql|"+name+"|"+hash+"|"+ss.str());
    Pattern p; p.signature=sig;p.source_table=name;p.energy_hash=hash;
    p.energy_mean=component(digest,0);p.pheromone_mean=component(digest,4);p.nutrient_mean=component(digest,8);
    p.mood_vector=mood.value_or(std::array<double,3>{component(digest,12,false),component(digest,16,false),component(digest,20,false)});
    p.stability=stability;
    // Preserve the original store_pattern update semantics if the signature already exists.
    if(auto existing=get_pattern(sig)) {
        Pattern updated=*existing;
        updated.update(p.energy_mean,p.pheromone_mean,p.nutrient_mean,p.mood_vector,p.stability);
        p=std::move(updated);
    }
    store_pattern(std::move(p));
    if(get_pattern(sig)) {
        external_records_.insert_or_assign(sig,row);
        table_index_[lower(name)].insert(sig);
    }
    return sig;
}
bool DynamicAssociativeDatabase::update_sql_record(std::string_view signature,const sql::Row& row,
    std::optional<double> stability,std::optional<std::array<double,3>> mood) {
    auto it=attractors_.find(std::string(signature));
    if(it==attractors_.end()||!external_records_.contains(std::string(signature)))return false;
    const auto encoded=encode_row(it->second.source_table,row);const auto hash=sha256(encoded);const auto digest=hash_bytes(encoded);
    auto& p=it->second;p.energy_mean=component(digest,0);p.pheromone_mean=component(digest,4);p.nutrient_mean=component(digest,8);
    p.mood_vector=mood.value_or(std::array<double,3>{component(digest,12,false),component(digest,16,false),component(digest,20,false)});
    p.energy_hash=hash;
    if(stability)p.stability=std::clamp((p.stability+*stability)*0.5,0.0,1.0);
    ++p.visits;external_records_.insert_or_assign(std::string(signature),row);return true;
}
bool DynamicAssociativeDatabase::delete_sql_record(std::string_view signature) {
    return external_records_.contains(std::string(signature))&&delete_pattern(signature);
}
std::optional<DynamicAssociativeDatabase::ExternalRecord> DynamicAssociativeDatabase::get_sql_record(std::string_view signature) const {
    auto it=external_records_.find(signature);const auto* p=get_pattern(signature);
    if(it==external_records_.end()||!p)return {};
    return ExternalRecord{p->signature,p->source_table,it->second,p->stability,p->visits,p->mood_vector};
}
std::vector<DynamicAssociativeDatabase::ExternalRecord> DynamicAssociativeDatabase::query_sql_like(
    std::optional<std::string_view> table,const sql::Row& filters,std::optional<std::size_t> limit) const {
    std::vector<ExternalRecord> results;if(limit&&*limit==0)return results;
    for(const auto& [sig,row]:external_records_) {
        const auto* p=get_pattern(sig);if(!p)continue;
        if(table&&lower(p->source_table)!=lower(*table))continue;
        bool matches=true;for(const auto& [k,v]:filters){auto it=row.find(k);if(it==row.end()||it->second!=v){matches=false;break;}}
        if(matches)results.push_back(ExternalRecord{sig,p->source_table,row,p->stability,p->visits,p->mood_vector});
    }
    std::stable_sort(results.begin(),results.end(),[](const auto& a,const auto& b){return a.visits>b.visits;});
    if(limit&&results.size()>*limit)results.resize(*limit);return results;
}
std::vector<DynamicAssociativeDatabase::ExternalRecord> DynamicAssociativeDatabase::associative_sql_lookup(
    std::string_view cue,double intensity,std::optional<std::size_t> limit) {
    std::vector<ExternalRecord> results;auto ranked=rank_patterns(cue);std::unordered_set<std::string> seen;
    struct Lexical{std::size_t score{};bool auxiliary{};ExternalRecord record;};std::vector<Lexical> lexical;auto terms=lexical_terms(cue);
    for(auto& record:query_sql_like()){auto score=record_lexical_score(record,cue,terms);if(score)lexical.push_back({score,auxiliary_search_table(record.table),std::move(record)});}
    std::stable_sort(lexical.begin(),lexical.end(),[](const auto&a,const auto&b){if(a.auxiliary!=b.auxiliary)return !a.auxiliary;return a.score>b.score;});
    auto add=[&](ExternalRecord record){if((!limit||results.size()<*limit)&&seen.insert(record.signature).second)results.push_back(std::move(record));};
    if(!limit||*limit>0){for(auto& item:lexical)add(std::move(item.record));for(const auto& entry:ranked){if(limit&&results.size()>=*limit)break;if(auto result=get_sql_record(entry.signature))add(std::move(*result));}}
    QueryRecord q;q.cue=std::string(cue);q.intensity=intensity;q.returned=results.size();
    for(auto& entry:ranked)q.signatures.push_back(entry.signature);
    last_query_=std::move(q);return results;
}
std::size_t DynamicAssociativeDatabase::ingest_dump(const sql::DumpImage& dump) {
    std::size_t inserted=0;
    for(const auto& name:dump.list_tables())for(const auto& row:dump.fetch_rows(name)) {
        store_sql_record(name,row);++inserted;
    }
    return inserted;
}
std::vector<std::string> DynamicAssociativeDatabase::list_external_tables() const {
    std::vector<std::string> out;for(const auto& [key,_]:table_index_)out.push_back(key);return out;
}
}
