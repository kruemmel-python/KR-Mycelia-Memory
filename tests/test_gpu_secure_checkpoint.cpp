#include "gpu_observation.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace mycelia;
void check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
template<class Fn> void expect_throw(Fn&& fn, const char* reason) {
    bool rejected = false;
    try { fn(); } catch (const std::exception&) { rejected = true; }
    check(rejected, reason);
}
int main() {
    try {
        MyceliaWorldCpu world(4,3,1,4,0.1);
        DynamicAssociativeDatabase db;
        auto frame = world.step();
        auto observer = ObserverNetwork::encode(frame.fields.pheromone, frame.fields.nutrient);
        (void)db.observe(frame.fields, observer, observer.harmony_score, observer.tension_score, 0.1);
        GpuObservation observed{23, 12, 3, {0.1f, 0.2f, -0.3f}, {0.5f, 0.6f, 0.7f}};
        const auto original = GpuObservationCodec::encode(world,db,observed);
        auto decoded = GpuObservationCodec::decode(original);
        check(decoded.gpu.nutrient == observed.nutrient, "GPU data changed");
#ifdef _WIN32
        auto directory = std::filesystem::temp_directory_path() / "mycelia_gpu_secure_test";
        std::filesystem::create_directories(directory);
        auto path = directory / "gpu_observation_private.kms";
        auto damaged = directory / "gpu_observation_corrupt.kms";
        std::filesystem::remove(path);
        std::filesystem::remove(damaged);
        GpuObservationCodec::save_secure(path, world, db, observed);
        auto recovered = GpuObservationCodec::load_secure(path);
        check(recovered.gpu.activity == observed.activity && recovered.gpu.nutrient == observed.nutrient,
              "DPAPI GPU observation roundtrip changed");
        check(recovered.checkpoint.world.serialize() == world.serialize(), "DPAPI world mismatch");
        check(recovered.checkpoint.memory.snapshot_bytes() == db.snapshot_bytes(), "DPAPI DAD mismatch");
        expect_throw([&] { GpuObservationCodec::save_secure(path,world,db,observed); },
                     "checkpoint unexpectedly overwritten");
        std::ifstream source(path,std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(source)),{});
        // Windows does not permit removing a file that this process still holds.
        source.close();
        check(bytes.find("KRGPUOBS1") == std::string::npos, "plaintext magic visible on disk");
        check(bytes.size()>90,"DPAPI ciphertext is too short");
        bytes[bytes.size()-20] ^= 0x01;
        { std::ofstream destination(damaged,std::ios::binary); destination.write(bytes.data(),bytes.size()); }
        expect_throw([&] { (void)GpuObservationCodec::load_secure(damaged); },
                     "tampered GPU checkpoint accepted");
        std::filesystem::remove(path);
        std::filesystem::remove(damaged);
        std::cout << "PASS: encrypted GPU observation + world/DAD DPAPI persistence (NOT VRAM restore)\n";
#else
        expect_throw([&] { GpuObservationCodec::save_secure("not_created.kms",world,db,observed); },
                     "unencrypted Linux fallback accepted");
        expect_throw([&] { (void)GpuObservationCodec::load_secure("not_created.kms"); },
                     "Linux secure load unexpectedly succeeded");
        std::cout << "PASS: secure GPU observation fails closed without Windows DPAPI\n";
#endif
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
