#include "mycelia.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <iterator>
#include <cmath>
#include <iomanip>
#include <locale>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/sha.h>
#endif
namespace mycelia {
namespace {
std::string formatted(double v) { std::ostringstream s; s.imbue(std::locale::classic()); s << std::fixed << std::setprecision(6) << v; return s.str(); }
double clamp01(double d) { return std::clamp(d,0.0,1.0); }

// Python 3.12 random.Random(int_seed): CPython _randommodule.c init_by_array,
// random.py _randbelow_with_getrandbits; independent from std::mt19937 seeding.
class PythonRandom {
public:
    explicit PythonRandom(std::uint64_t seed) {
        std::uint32_t key[2] = {static_cast<std::uint32_t>(seed), static_cast<std::uint32_t>(seed >> 32)};
        const auto len = key[1] == 0 ? 1u : 2u;
        init_genrand(19650218u);
        std::size_t i=1, j=0, k=std::max<std::size_t>(624,len);
        for (;k;--k) {
            state_[i]=(state_[i] ^ ((state_[i-1] ^ (state_[i-1] >> 30)) * 1664525u)) + key[j] + static_cast<std::uint32_t>(j);
            ++i; ++j;
            if (i>=624) {state_[0]=state_[623];i=1;}
            if (j>=len) j=0;
        }
        for (k=623;k;--k) {
            state_[i]=(state_[i] ^ ((state_[i-1] ^ (state_[i-1] >> 30)) * 1566083941u)) - static_cast<std::uint32_t>(i);
            ++i;
            if (i>=624) {state_[0]=state_[623];i=1;}
        }
        state_[0]=0x80000000u;
    }
    std::uint32_t getrandbits(unsigned k) {
        if(k==0 || k>32) throw std::invalid_argument("getrandbits requires 1..32");
        return next() >> (32-k);
    }
    std::uint32_t randint(std::uint32_t low, std::uint32_t high) {
        if(high<low) throw std::invalid_argument("invalid randint bounds");
        const std::uint32_t count=high-low+1;
        const unsigned bits=std::bit_width(count);
        std::uint32_t result;
        do {result=getrandbits(bits);} while(result>=count);
        return low+result;
    }
private:
    std::array<std::uint32_t,624> state_{};
    std::size_t index_{624};
    void init_genrand(std::uint32_t seed) {
        state_[0]=seed;
        for(std::size_t i=1;i<624;++i)
            state_[i]=1812433253u*(state_[i-1]^(state_[i-1]>>30))+static_cast<std::uint32_t>(i);
        index_=624;
    }
    std::uint32_t next() {
        if(index_>=624) {
            for(std::size_t kk=0;kk<624;++kk) {
                const auto y=(state_[kk]&0x80000000u)|(state_[(kk+1)%624]&0x7fffffffu);
                state_[kk]=state_[(kk+397)%624]^(y>>1)^((y&1u)?0x9908b0dfu:0u);
            }
            index_=0;
        }
        std::uint32_t y=state_[index_++];
        y^=y>>11;y^=(y<<7)&0x9d2c5680u;y^=(y<<15)&0xefc60000u;y^=y>>18;
        return y;
    }
};
std::uint64_t seed_prefix(std::string_view hex, std::size_t length) {
    if(hex.size()<length) throw std::invalid_argument("hash prefix too short");
    std::uint64_t result=0;
    for(std::size_t i=0;i<length;++i) {
        const auto c=hex[i];
        const auto digit=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
        if(digit<0) throw std::invalid_argument("invalid hash hex prefix");
        result=(result<<4)|static_cast<std::uint64_t>(digit);
    }
    return result;
}

}
std::string DynamicAssociativeDatabase::sha256(std::string_view text) {
    std::array<unsigned char, 32> hash{};
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg{}; BCRYPT_HASH_HANDLE handle{};
    if (BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0) < 0) throw std::runtime_error("BCryptOpenAlgorithmProvider failed");
    if (BCryptCreateHash(alg,&handle,nullptr,0,nullptr,0,0) < 0) { BCryptCloseAlgorithmProvider(alg,0); throw std::runtime_error("BCryptCreateHash failed"); }
    // BCryptHashData declares a non-const PUCHAR even though it reads the input.
    // Keep the string_view immutable at the API boundary; cast only for Win32 ABI.
    auto* data = reinterpret_cast<PUCHAR>(const_cast<char*>(text.data()));
    const bool ok = text.size() <= UINT32_MAX && BCryptHashData(handle,data,static_cast<ULONG>(text.size()),0) >= 0 && BCryptFinishHash(handle,hash.data(),static_cast<ULONG>(hash.size()),0) >= 0;
    BCryptDestroyHash(handle); BCryptCloseAlgorithmProvider(alg,0);
    if (!ok) throw std::runtime_error("BCrypt SHA256 failed");
#else
    SHA256(reinterpret_cast<const unsigned char*>(text.data()), text.size(),hash.data());
#endif
    std::ostringstream result;
    for (unsigned char b:hash) result << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return result.str();
}
void Pattern::update(double e,double p,double n,std::array<double,3> mood,double s) {
    const double blend=1.0/static_cast<double>(visits+1);
    energy_mean=(1-blend)*energy_mean+blend*e;
    pheromone_mean=(1-blend)*pheromone_mean+blend*p;
    nutrient_mean=(1-blend)*nutrient_mean+blend*n;
    for(std::size_t i=0;i<3;++i) mood_vector[i]=(1-blend)*mood_vector[i]+blend*mood[i];
    stability=(1-blend)*stability+blend*s;
    ++visits;
}
DynamicAssociativeDatabase::DynamicAssociativeDatabase(std::size_t retention,double ng,double mg,double ag)
    :retention_(std::max<std::size_t>(4,retention)),noise_gain_(std::max(0.0,ng)),
     mood_gain_(std::max(0.0,mg)),agent_gain_(std::max(0.05,ag)){}
std::string DynamicAssociativeDatabase::generate_signature(double e,double p,double n,double h,double t,double q,std::optional<double> chaos) const {
    return sha256(formatted(e)+"|"+formatted(p)+"|"+formatted(n)+"|"+formatted(h)+"|"+formatted(t)+"|"+formatted(q)+"|"+formatted(chaos.value_or(noise_factor_)));
}
Pattern& DynamicAssociativeDatabase::store_pattern(Pattern p) {
    p.stability=clamp01(p.stability);
    p.visits=std::max<std::size_t>(1,p.visits);
    if(p.signature.empty()) throw std::invalid_argument("signature must not be empty");
    const bool is_new=!attractors_.contains(p.signature);
    const std::string new_sig=p.signature;
    auto [it, inserted]=attractors_.insert_or_assign(p.signature,std::move(p));
    if(is_new) insertion_order_.push_back(new_sig);
    const std::string signature=it->first;
    enforce_retention();
    auto found=attractors_.find(signature);
    if(found==attractors_.end()) throw std::runtime_error("pattern evicted by retention policy");
    return found->second;
}
const Pattern* DynamicAssociativeDatabase::get_pattern(std::string_view sig) const {
    auto it=attractors_.find(sig); return it==attractors_.end()?nullptr:&it->second;
}
bool DynamicAssociativeDatabase::delete_pattern(std::string_view sig) {
    auto p=get_pattern(sig);
    if(!p) return false;
    if(external_records_.contains(std::string(sig))) {
        auto key=p->source_table;
        std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        auto it=table_index_.find(key);
        if(it!=table_index_.end()) {it->second.erase(std::string(sig));if(it->second.empty())table_index_.erase(it);}
        external_records_.erase(std::string(sig));
    }
    if(!attractors_.erase(std::string(sig))) return false;
    std::erase(insertion_order_,std::string(sig));
    return true;
}
void DynamicAssociativeDatabase::clear() { external_records_.clear(); table_index_.clear(); attractors_.clear(); insertion_order_.clear(); energy_hash_history_.clear(); last_query_.reset(); }
std::vector<Pattern> DynamicAssociativeDatabase::list_patterns() const {
    std::vector<Pattern> out;for(auto const& [_,p]:attractors_)out.push_back(p);
    std::stable_sort(out.begin(),out.end(),[](auto const&a,auto const&b){return a.visits>b.visits;});return out;
}

Observation ObserverNetwork::encode(const std::optional<std::vector<double>>& pheromone,
                                     const std::optional<std::vector<double>>& nutrient) {
    const auto mean = [](const std::optional<std::vector<double>>& field) {
        if (!field || field->empty()) return 0.0;
        return std::accumulate(field->begin(), field->end(), 0.0) / static_cast<double>(field->size());
    };
    double harmony=mean(pheromone);
    double level=mean(nutrient);
    if (!pheromone) harmony=0.5;
    if (!nutrient) level=0.5;
    const double tension=std::abs(harmony-level);
    return {{harmony,level,tension}, harmony,tension};
}

std::string DynamicAssociativeDatabase::observe(const WorldFields& fields, const Observation& observation,
                                                 double harmony, double tension, double qualia) {
    const auto mean=[](const std::vector<double>& values) {
        if(values.empty()) return 0.0;
        return std::accumulate(values.begin(),values.end(),0.0)/static_cast<double>(values.size());
    };
    const double energy_mean=mean(fields.energy);
    const double pheromone_mean=mean(fields.pheromone);
    const double nutrient_mean=mean(fields.nutrient);
    const double chaos=update_noise(fields.energy);
    std::string hash_material;
    for(std::size_t i=0;i<std::min<std::size_t>(1024,fields.energy.size());++i)
        hash_material+=formatted(fields.energy[i]);
    hash_material+=formatted(chaos);
    const std::string energy_hash=sha256(hash_material);
    const std::string signature=generate_signature(energy_mean,pheromone_mean,nutrient_mean,
                                                    harmony,tension,qualia,chaos);
    double stability=0.0;
    if(!fields.energy.empty()) {
        double deviation=0.0;
        for(double v:fields.energy)deviation+=std::abs(v-energy_mean);
        deviation/=static_cast<double>(fields.energy.size());
        stability=clamp01(std::exp(-deviation)*(1.0+mood_gain_*(observation.harmony_score-observation.tension_score)));
    }
    auto it=attractors_.find(signature);
    if(it==attractors_.end()) {
        insertion_order_.push_back(signature);
        Pattern p;
        p.signature=signature;p.energy_mean=energy_mean;p.pheromone_mean=pheromone_mean;
        p.nutrient_mean=nutrient_mean;p.mood_vector={harmony,tension,qualia};
        p.stability=stability;p.energy_hash=energy_hash;
        attractors_.emplace(signature,std::move(p));
    } else {
        it->second.update(energy_mean,pheromone_mean,nutrient_mean,{harmony,tension,qualia},stability);
        it->second.energy_hash=energy_hash;
    }
    energy_hash_history_.push_back(energy_hash);
    while(energy_hash_history_.size()>retention_)energy_hash_history_.pop_front();
    enforce_retention();
    return signature;
}

std::vector<RankedAttractor> DynamicAssociativeDatabase::rank_patterns(std::string_view cue) const {
    const auto cv=cue_vector(cue);
    std::vector<RankedAttractor> result;
    for(const auto& key:insertion_order_) {
        const auto it=attractors_.find(key);
        if(it!=attractors_.end()) result.push_back({key,similarity(it->second,cv)});
    }
    std::stable_sort(result.begin(),result.end(),[](const auto& lhs,const auto& rhs){return lhs.similarity>rhs.similarity;});
    return result;
}
std::vector<Pattern> DynamicAssociativeDatabase::memory_snapshot(std::size_t top_n) const {
    auto items=list_patterns();
    items.resize(std::min(items.size(),std::max<std::size_t>(1,top_n)));
    return items;
}

double DynamicAssociativeDatabase::update_noise(const std::vector<double>& energy) {
    if(energy.empty())return noise_factor_;
    const double mean=std::accumulate(energy.begin(),energy.end(),0.0)/energy.size();
    double variance=0;for(double v:energy)variance+=(v-mean)*(v-mean);
    variance/=energy.size();
    const double r=3.6+std::min(0.39,variance*noise_gain_*10.0);
    noise_factor_=std::fmod(r*noise_factor_*(1.0-noise_factor_),1.0);
    return noise_factor_;
}
std::array<double,3> DynamicAssociativeDatabase::cue_vector(std::string_view cue) const {
    const std::string hex=sha256(std::string(cue)+"|"+formatted(noise_factor_));
    std::array<double,3> out{};
    for(int i=0;i<3;++i)out[i]=std::stoi(hex.substr(2*i,2),nullptr,16)/255.0;
    return out;
}
double DynamicAssociativeDatabase::similarity(const Pattern& p,std::array<double,3> cue) const {
    double distance=0;for(int i=0;i<3;++i)distance+=(p.mood_vector[i]-cue[i])*(p.mood_vector[i]-cue[i]);
    const double mood=1.0-std::min(1.0,std::sqrt(distance));
    const double entropy=1.0-std::abs(p.stability-noise_factor_);
    return 0.7*mood+0.3*entropy;
}
std::vector<AgentDescriptor> DynamicAssociativeDatabase::associative_query(std::string_view cue,double intensity) {
    if(!std::isfinite(intensity)) throw std::invalid_argument("intensity must be finite");
    const auto ranked=rank_patterns(cue);
    QueryRecord record;
    record.cue=std::string(cue);
    record.intensity=intensity;
    if(ranked.empty()) {
        last_query_=std::move(record);
        return {};
    }
    const auto top_k=static_cast<std::size_t>(std::clamp(std::ceil(intensity*2.0),1.0,3.0));
    std::uint32_t width=1, height=1;
    if(!energy_hash_history_.empty()) {
        // Original: next(iter(self._attractors.values())) -- insertion order.
        const auto& first=attractors_.at(insertion_order_.front());
        PythonRandom grid_rng(seed_prefix(first.energy_hash,8));
        width=grid_rng.randint(8,64);
        height=grid_rng.randint(8,64);
    }
    std::vector<AgentDescriptor> agents;
    for(std::size_t i=0;i<std::min(top_k,ranked.size());++i) {
        const auto& item=ranked[i];
        const auto& pattern=attractors_.at(item.signature);
        PythonRandom projection(seed_prefix(pattern.signature,12));
        const auto x=projection.randint(0,std::max(1u,width-1));
        const auto y=projection.randint(0,std::max(1u,height-1));
        const auto normalize=[](double v){return std::max(0.0,std::min(1.0,(v+1.0)*0.5));};
        agents.push_back({static_cast<double>(x)/std::max(1u,width-1),
                          static_cast<double>(y)/std::max(1u,height-1),
                          agent_gain_*intensity*normalize(pattern.energy_mean),
                          agent_gain_*(pattern.stability+noise_factor_)});
        record.signatures.push_back(item.signature);
    }
    record.returned=top_k; // Python records top_k, even if fewer patterns available.
    last_query_=std::move(record);
    return agents;
}
void DynamicAssociativeDatabase::enforce_retention() {
    while(attractors_.size()>retention_){
        auto victim=std::min_element(attractors_.begin(),attractors_.end(),[](auto const&a,auto const&b){
            return a.second.stability*a.second.visits < b.second.stability*b.second.visits;});
        const auto victim_signature=victim->first;
        if (external_records_.contains(victim_signature)) {
            auto key=victim->second.source_table;
            std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            auto idx=table_index_.find(key);
            if (idx!=table_index_.end()) {idx->second.erase(victim_signature); if (idx->second.empty()) table_index_.erase(idx);}
            external_records_.erase(victim_signature);
        }
        std::erase(insertion_order_,victim_signature);
        attractors_.erase(victim);
    }
}
double DynamicAssociativeDatabase::average_stability() const {
    if(attractors_.empty())return 0.0;
    double sum=0;for(auto const& [_,p]:attractors_)sum+=p.stability;
    return sum/attractors_.size();
}
}
