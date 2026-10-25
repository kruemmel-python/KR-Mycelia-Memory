#include "world_checkpoint.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mycelia;
void require(bool pass, const char* reason) { if (!pass) throw std::runtime_error(reason); }
int main() {
    try {
        MyceliaWorldCpu world(4,3,1,4,0.1);
        DynamicAssociativeDatabase memory;
        for (int i = 0; i != 3; ++i) {
            auto frame = world.step();
            auto observation = ObserverNetwork::encode(frame.fields.pheromone, frame.fields.nutrient);
            auto signature = memory.observe(frame.fields, observation, observation.harmony_score,
                                             observation.tension_score,
                                             (observation.harmony_score-observation.tension_score)*0.5);
            require(memory.get_pattern(signature) != nullptr, "stored world attractor missing");
        }
        const auto checkpoint = WorldCheckpointCodec::encode(world, memory);
        auto restored = WorldCheckpointCodec::decode(checkpoint);
        require(restored.world.serialize() == world.serialize(), "world state differs after restoration");
        require(restored.memory.snapshot_bytes() == memory.snapshot_bytes(), "memory state differs after restoration");
        const auto next = world.step();
        const auto resumed = restored.world.step();
        require(next.step == resumed.step, "step number differs on resume");
        require(next.fields.energy == resumed.fields.energy, "energy differs on resume");
        require(next.fields.nutrient == resumed.fields.nutrient, "nutrient differs on resume");
        auto corrupt = checkpoint; corrupt[35] ^= 0x11;
        bool rejected = false;
        try { (void)WorldCheckpointCodec::decode(corrupt); } catch (const std::exception&) { rejected = true; }
        require(rejected, "corrupt checkpoint accepted");
        rejected = false;
        try { (void)WorldCheckpointCodec::decode(checkpoint.substr(0,checkpoint.size()-1)); }
        catch (const std::exception&) { rejected = true; }
        require(rejected, "truncated checkpoint accepted");
        const auto path = std::filesystem::temp_directory_path() / "kr_mycelia_world_checkpoint_test.kms";
        std::filesystem::remove(path);
#ifdef _WIN32
        WorldCheckpointCodec::save_secure(path, restored.world, restored.memory);
        {
            std::ifstream f(path, std::ios::binary);
            const std::string data((std::istreambuf_iterator<char>(f)), {});
            require(data.starts_with("KRCDPAPI"), "secure envelope missing");
            require(data.find("MYCW011") == std::string::npos, "world plaintext leaked");
            require(data.find("MYCELIA6") == std::string::npos, "memory plaintext leaked");
        }
        auto secure = WorldCheckpointCodec::load_secure(path);
        require(secure.world.serialize() == restored.world.serialize(), "secure world restore mismatch");
        require(secure.memory.snapshot_bytes() == restored.memory.snapshot_bytes(), "secure memory restore mismatch");
        std::filesystem::remove(path);
#else
        rejected = false;
        try { WorldCheckpointCodec::save_secure(path, restored.world, restored.memory); }
        catch (const std::exception&) { rejected = true; }
        require(rejected && !std::filesystem::exists(path), "DPAPI must fail closed on Linux");
#endif
        std::cout << "PASS: world-memory checkpoint integrity, resume, secure storage policy\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
