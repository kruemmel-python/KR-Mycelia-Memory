#ifdef NDEBUG
#undef NDEBUG
#endif
#include "sql_dump.hpp"
#include <cassert>
#include <stdexcept>
#include <iostream>
using namespace mycelia::sql;
int main(){
 auto dump=DumpImage::parse(R"SQL(-- skipped comment
CREATE TABLE `people` (`id` int,`name` varchar(80),`score` double,`enabled` bool,PRIMARY KEY (`id`));
INSERT INTO `people` VALUES (1,'Ada, Lovelace',2.5,TRUE),(2,'O\'Brien',NULL,FALSE);
INSERT INTO `people` (`id`,`name`) VALUES (3,'Mira');
# ignored
CREATE TABLE `empty` (`id` int);
)SQL");
 assert(dump.list_tables().size()==2);assert(dump.list_tables().at(0)=="people");assert(dump.row_count()==3);
 auto rows=dump.fetch_rows("PEOPLE");assert(rows.size()==3);assert(std::get<std::string>(rows[0].at("name"))=="Ada, Lovelace");assert(std::get<double>(rows[0].at("score"))==2.5);
 assert(std::get<std::string>(rows[1].at("name"))=="O'Brien");assert(std::holds_alternative<std::nullptr_t>(rows[1].at("score")));
 assert(dump.fetch_rows("people","id=2").size()==1);assert(dump.fetch_rows("people","enabled=false").size()==1);
 assert(dump.fetch_rows("people",{},1).size()==1);assert(dump.fetch_rows("people",{},0).empty());
 assert(dump.fetch_rows("people","name='Mira'").size()==1);
 bool threw=false;try{(void)dump.fetch_rows("people","id>1");}catch(const std::invalid_argument&){threw=true;}assert(threw);
 std::cout<<"Native SQL dump importer test passed\n";
}
