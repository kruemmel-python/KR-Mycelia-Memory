#include "gpu_observation.hpp"
#include <bit>
#include <iostream>
#include <filesystem>
#include <stdexcept>
using namespace mycelia;
void check(bool b,const char* message) { if (!b) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    try {
        check(argc==2,"expected fake GPU library argument");
        GpuBridge bridge;
        bridge.open(std::filesystem::absolute(argv[1]));
        bridge.initialize(0); bridge.init_mycel(8,2,3); bridge.set_active(3);
        const std::vector<float> supplied{0.25f,-0.5f,1.0f};
        bridge.update(supplied);
        const auto observed=GpuObservationCodec::capture(bridge,supplied,8,7);
        check(observed.nutrient.size()==3 && observed.nutrient[0]==0.25f,"live DLL readback wrong");
        bridge.close();
        MyceliaWorldCpu world(4,3,1,4,0.1);
        DynamicAssociativeDatabase memory;
        const auto frame = world.step();
        const auto obs = ObserverNetwork::encode(frame.fields.pheromone,frame.fields.nutrient);
        (void)memory.observe(frame.fields,obs,obs.harmony_score,obs.tension_score,0.25);
        GpuObservation gpu=observed;
        const auto bytes=GpuObservationCodec::encode(world,memory,gpu);
        auto restored=GpuObservationCodec::decode(bytes);
        check(restored.gpu.simulation_step==7,"step lost");
        check(restored.gpu.capacity==8 && restored.gpu.active_cells==3,"dimensions lost");
        check(restored.gpu.nutrient==gpu.nutrient && restored.gpu.activity==gpu.activity,"readback altered");
        check(restored.checkpoint.world.serialize()==world.serialize(),"world changed");
        check(restored.checkpoint.memory.snapshot_bytes()==memory.snapshot_bytes(),"DAD changed");
        auto corrupt=bytes; corrupt[corrupt.size()-70]^=1;
        bool rejected=false;
        try { (void)GpuObservationCodec::decode(corrupt); } catch (const std::exception&) { rejected=true; }
        check(rejected,"tampering accepted");
        rejected=false;
        try { (void)GpuObservationCodec::decode(bytes.substr(0,bytes.size()-3)); } catch (const std::exception&) { rejected=true; }
        check(rejected,"truncation accepted");
        rejected=false;
        try { (void)GpuObservationCodec::encode(world,memory,{1,2,3,{1,2,3},{1,2,3}}); } catch(const std::exception&) { rejected=true; }
        check(rejected,"invalid shape accepted");
        std::cout << "PASS: GPU observed nutrient/activity + world/DAD checkpoint (NOT full VRAM restore)\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
