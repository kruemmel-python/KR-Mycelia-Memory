#ifdef NDEBUG
#undef NDEBUG
#endif
#include "mycelia.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>
using namespace mycelia;
int main() {
 auto o=ObserverNetwork::encode(std::vector<double>{.2,.4},std::vector<double>{.5,.7});
 assert(std::abs(o.harmony_score-.3)<1e-12);
 assert(std::abs(o.tension_score-.3)<1e-12);
 auto fallback=ObserverNetwork::encode(std::nullopt,std::nullopt);
 assert(fallback.harmony_score==.5 && fallback.tension_score==0);
 DynamicAssociativeDatabase db;
 WorldFields field{{.0,1.,2.,3.},{.2,.4},{.5,.7}};
 auto signature=db.observe(field,o,o.harmony_score,o.tension_score,.8);
 assert(db.get_pattern(signature)!=nullptr);
 // Golden vectors calculated by executing the ORIGINAL dynamic_database.py.
 assert(signature=="7565f18b38fa28b85e48d1ed5b14c59298b1a6bf392910a96792cf8c1aab26cd");
 assert(db.get_pattern(signature)->energy_hash=="774e475494b4650e42140f666a53fffd0edde0101bcf7af2424f09aa96e9ba85");
 assert(std::abs(db.get_pattern(signature)->stability-0.36787944117144233)<1e-12);
 assert(db.get_pattern(signature)->energy_hash.size()==64);
 assert(db.history_size()==1);
 assert(db.attractor_count()==1);
 assert(db.rank_patterns("hello").size()==1);
 assert(db.memory_snapshot().size()==1);
 db.clear();
 assert(db.attractor_count()==0 && db.history_size()==0);
 std::cout<<"PASS: observer, coupled observation, signatures, noise, energy hash, history, ranking\n";
}
