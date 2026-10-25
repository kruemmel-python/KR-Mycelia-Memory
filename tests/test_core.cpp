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
#include <stdexcept>
using namespace mycelia;
int main() {
 DynamicAssociativeDatabase db;
 assert(DynamicAssociativeDatabase::sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
 assert(db.generate_signature(.125,.25,.375,.5,.625,.75)=="995161dcb42503d6fd6d161ce5a5d7b29bc0f6b28acd83180033f2add2288c54");
 assert(std::abs(db.update_noise({0.,1.,2.,3.})-0.98166368999999987)<1e-12);
 Pattern p; p.signature="one";p.stability=.8;p.mood_vector={.1,.2,.3};
 db.store_pattern(p);
 assert(db.get_pattern("one") && db.attractor_count()==1);
 auto copy=*db.get_pattern("one");copy.update(.5,.6,.7,{.4,.5,.6},.9);
 assert(copy.visits==2 && std::abs(copy.energy_mean-.25)<1e-12);
 assert(db.delete_pattern("one") && !db.delete_pattern("one"));
 auto empty=db.associative_query("hello");
 assert(empty.empty() && db.last_query()->returned==0);
 std::cout<<"PASS: SHA256, Python signature parity, noise parity, pattern lifecycle, empty-query parity\n";
}
