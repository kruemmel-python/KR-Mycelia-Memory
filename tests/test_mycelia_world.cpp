#include "mycelia_world.hpp"
#include "mycelia.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace mycelia;
static void check(bool yes,const char* reason) { if(!yes) throw std::runtime_error(reason); }
static void near(double a,double b) { check(std::abs(a-b)<1e-12,"Python numerical parity mismatch"); }
int main() {
    try {
        MyceliaWorldCpu world(3,2,1,4,0.25);
        // Python MyceliaWorld._build_neighbor_lattice() row-major exact offsets.
        const std::vector<int> neighbors{1,3,4,-1, 0,2,4,5, 1,5,4,-1,
                                         4,0,1,-1, 3,5,1,0, 4,2,1,-1};
        check(world.neighbors()==neighbors,"neighbor lattice parity");
        const std::array<double,3> energy{2.,4.,6.};
        const auto activity=world.derive_activity(energy);
        near(activity[0],0.);near(activity[1],0.5);near(activity[2],1.);
        const auto before=world.frame();
        near(before.fields.energy[0],0.5);
        const std::array<std::array<double,4>,1> injected{{{0.,0.,0.2,0.4}}};
        world.inject_agents(injected);
        near(world.frame().fields.energy[0],0.5); // original does not mutate already emitted fields
        const auto first=world.step();
        check(first.step==0,"first step must be zero");
        near(first.fields.energy[0],0.5+0.5*std::sin(0.25));
        near(first.fields.pheromone[0],0.5+0.5*std::sin(0.125+3.141592653589793238462643383279502884/4));
        near(first.fields.nutrient[0],0.5+0.5*std::cos(0.25*0.33));
        check(std::abs(static_cast<double>(first.activity[0])-first.fields.energy[0])<1e-7,"activity float mismatch");
        const auto bytes=world.serialize();
        auto restored=MyceliaWorldCpu::deserialize(bytes);
        const auto second=world.step();
        const auto resumed=restored.step();
        check(second.step==resumed.step,"resume step mismatch");
        near(second.fields.energy[0],resumed.fields.energy[0]);
        near(second.fields.pheromone[0],resumed.fields.pheromone[0]);
        near(second.fields.nutrient[0],resumed.fields.nutrient[0]);
        bool rejected=false;
        try { (void)MyceliaWorldCpu::deserialize(bytes.substr(0,bytes.size()-2)); }
        catch(const std::invalid_argument&) { rejected=true; }
        check(rejected,"truncated world state accepted");
        rejected=false;
        try { MyceliaWorldCpu invalid(0,2,1,4,0.25); }
        catch(const std::invalid_argument&) { rejected=true; }
        check(rejected,"zero-sized grid accepted");
        DynamicAssociativeDatabase db;
        const auto obs=ObserverNetwork::encode(first.fields.pheromone,first.fields.nutrient);
        const auto signature=db.observe(first.fields,obs,obs.harmony_score,obs.tension_score,
                                        (obs.harmony_score-obs.tension_score)*0.5);
        check(db.get_pattern(signature)!=nullptr,"CPU world -> Observer -> DAD failed");
        std::cout<<"PASS: original MyceliaWorld CPU fallback, lattice, activity, resume, Observer -> DAD\n";
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n';return 1; }
}
