#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include "sql_dump.hpp"
#include <set>
#include <filesystem>

namespace mycelia {
struct Observation {
    std::array<double,3> embeddings{};
    double harmony_score{}, tension_score{};
};
struct WorldFields {
    std::vector<double> energy, pheromone, nutrient;
};
struct QueryRecord {
    std::string cue;
    double intensity{};
    std::size_t returned{};
    std::vector<std::string> signatures;
};
class ObserverNetwork {
public:
    static Observation encode(const std::optional<std::vector<double>>& pheromone,
                              const std::optional<std::vector<double>>& nutrient);
};
struct AgentDescriptor { double x{}, y{}, energy{}, coupling{}; };
struct Pattern {
    std::string signature;
    double energy_mean{}, pheromone_mean{}, nutrient_mean{};
    std::array<double, 3> mood_vector{};
    double stability{1.0};
    std::size_t visits{1};
    std::string energy_hash, source_table;
    void update(double energy, double pheromone, double nutrient,
                std::array<double,3> mood, double stability_new);
};
struct RankedAttractor { std::string signature; double similarity{}; };
class DynamicAssociativeDatabase {
    friend struct SnapshotCodec;
public:
    explicit DynamicAssociativeDatabase(std::size_t retention=64, double noise_gain=0.18,
                                       double mood_gain=0.42, double agent_gain=0.75);
    static std::string sha256(std::string_view text);
    std::string generate_signature(double energy, double pheromone, double nutrient,
                                   double harmony, double tension, double qualia,
                                   std::optional<double> chaos_key=std::nullopt) const;
    Pattern& store_pattern(Pattern pattern);
    // Port of observe(snapshot, observation, harmony, tension, qualia).
    std::string observe(const WorldFields& fields, const Observation& observation,
                        double harmony, double tension, double qualia);
    std::vector<RankedAttractor> rank_patterns(std::string_view cue) const;
    std::vector<Pattern> memory_snapshot(std::size_t top_n=3) const;
    std::optional<QueryRecord> last_query() const { return last_query_; }
    std::size_t history_size() const { return energy_hash_history_.size(); }
    const Pattern* get_pattern(std::string_view signature) const;
    bool delete_pattern(std::string_view signature);
    void clear();
    std::string snapshot_bytes() const;
    static DynamicAssociativeDatabase from_snapshot_bytes(std::string_view bytes);
    void save_snapshot(const std::filesystem::path& path) const;
    // Windows: user-bound DPAPI ciphertext, never writes a plaintext temp snapshot.
    void save_secure_snapshot(const std::filesystem::path& path) const;
    static DynamicAssociativeDatabase load_secure_snapshot(const std::filesystem::path& path);

    static DynamicAssociativeDatabase load_snapshot(const std::filesystem::path& path);
    std::vector<Pattern> list_patterns() const;
    std::vector<AgentDescriptor> associative_query(std::string_view cue, double intensity=1.0);
    double update_noise(const std::vector<double>& energy);
    double noise_factor() const { return noise_factor_; }
    std::size_t attractor_count() const { return attractors_.size(); }
    double average_stability() const;
    struct ExternalRecord {
        std::string signature, table;
        sql::Row data;
        double stability{};
        std::size_t visits{};
        std::array<double,3> mood_vector{};
    };
    std::string store_sql_record(std::string_view table, const sql::Row& row,
                                 double stability=0.9, std::optional<std::array<double,3>> mood={},
                                 std::optional<double> chaos={});
    bool update_sql_record(std::string_view signature, const sql::Row& row,
                           std::optional<double> stability={}, std::optional<std::array<double,3>> mood={});
    bool delete_sql_record(std::string_view signature);
    std::optional<ExternalRecord> get_sql_record(std::string_view signature) const;
    std::vector<ExternalRecord> query_sql_like(std::optional<std::string_view> table={},
                                               const sql::Row& filters={},
                                               std::optional<std::size_t> limit={}) const;
    std::vector<ExternalRecord> associative_sql_lookup(std::string_view cue, double intensity=1.0,
                                                        std::optional<std::size_t> limit={});
    std::size_t ingest_dump(const sql::DumpImage& dump);
    std::vector<std::string> list_external_tables() const;
    std::size_t external_record_count() const {return external_records_.size();}
    // Not yet ported: remaining SQL payload corner cases,

    // native snapshots, GPU state, C/OpenCL world simulation, enterprise and security surfaces.
private:
    void enforce_retention();
    std::array<double,3> cue_vector(std::string_view cue) const;
    double similarity(const Pattern& pattern, std::array<double,3> cue) const;
    std::size_t retention_;
    double noise_gain_, mood_gain_, agent_gain_, noise_factor_{0.437};
    std::map<std::string, Pattern, std::less<>> attractors_;
    std::vector<std::string> insertion_order_;
    std::deque<std::string> energy_hash_history_;
    std::optional<QueryRecord> last_query_;
    std::map<std::string,sql::Row,std::less<>> external_records_;
    std::map<std::string,std::set<std::string>,std::less<>> table_index_;
};
}
