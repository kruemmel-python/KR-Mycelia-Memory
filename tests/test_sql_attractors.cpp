#ifdef NDEBUG
#undef NDEBUG
#endif
#include "mycelia.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace mycelia;
int main(){
 DynamicAssociativeDatabase db(64);
 auto dump=sql::DumpImage::parse("CREATE TABLE `people` (`id` int, `name` varchar(60), `active` bool);"
  "INSERT INTO `people` VALUES (1,'Ada',TRUE),(2,'Mira',FALSE);");
 assert(db.ingest_dump(dump)==2);assert(db.external_record_count()==2);
 assert(db.list_external_tables()==std::vector<std::string>{"people"});
 auto rows=db.query_sql_like("PEOPLE",{{"id",std::int64_t{1}}});assert(rows.size()==1);
 auto rec=rows.front();
 assert(rec.signature=="26fccbb4cc9c4ff32b591c5d9c3f2ca1a4628e3e5cf2857333f78dffc34aec70");
 assert(rec.table=="people");assert(rec.visits==1);
 // Golden vector executed against the unchanged Python original.
 auto pattern=db.get_pattern(rec.signature);assert(pattern);
 assert(std::abs(pattern->energy_mean-(-0.21806668192568857))<1e-14);
 assert(std::abs(pattern->pheromone_mean-0.678461890592813)<1e-14);
 assert(std::abs(pattern->nutrient_mean-(-0.43852903028915846))<1e-14);
 assert(std::abs(pattern->mood_vector[0]-0.8578622110322728)<1e-14);
 assert(std::abs(pattern->mood_vector[1]-0.6267795834287022)<1e-14);
 assert(std::abs(pattern->mood_vector[2]-0.46430151827267874)<1e-14);
 assert(db.get_sql_record(rec.signature).has_value());
 assert(db.associative_sql_lookup("Ada",1.0,1).size()==1);
 assert(db.update_sql_record(rec.signature,{{"id",std::int64_t{1}},{"name",std::string("Ada II")},{"active",true}},0.5));
 auto updated=db.get_sql_record(rec.signature);assert(updated&&updated->visits==2);
 assert(std::abs(updated->stability-0.7)<1e-12);
 assert(db.query_sql_like("people",{{"name",std::string("Ada II")}}).size()==1);
 assert(db.query_sql_like("people",{},0).empty());
 assert(db.associative_sql_lookup("Ada",1.0,0).empty());
 assert(db.delete_sql_record(rec.signature));assert(!db.get_sql_record(rec.signature));
 assert(db.external_record_count()==1);db.clear();assert(db.external_record_count()==0);
 assert(db.list_external_tables().empty());
 std::cout<<"SQL attractor integration passed\n";
}
