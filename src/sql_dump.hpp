#pragma once
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
namespace mycelia::sql {
using Value=std::variant<std::nullptr_t,bool,std::int64_t,double,std::string>;
using Row=std::map<std::string,Value>;
struct Table {std::string name; std::vector<std::string> columns; std::vector<Row> rows;};
class DumpImage {
public:
 static DumpImage parse(std::string_view sql);
 static DumpImage from_file(const std::filesystem::path& path);
 std::vector<std::string> list_tables() const;
 std::vector<Row> fetch_rows(std::string_view table, std::optional<std::string_view> where={}, std::optional<std::size_t> limit={}) const;
 std::size_t row_count() const;
private:
 std::map<std::string,Table> tables_;
 std::vector<std::string> order_;
};
std::string display(const Value& value);
}
