#include "BowSpeed/DiagnosticGate.h"
#include "Check.h"
void DiagnosticTests() {
    using namespace BowSpeed;
    DiagnosticGate gate;
    DiagnosticState state;state.weapon=10;state.ammo=20;state.player=true;state.bow=true;
    Check(gate.ShouldEmit(state), "Initial calculation must be sampled");
    Check(!gate.ShouldEmit(state), "Unchanged calculation must not spam");
    state.moving=true;
    Check(gate.ShouldEmit(state), "First moving stage sampled");
    state.runtime=0.5f;
    Check(gate.ShouldEmit(state), "Ricochet restitution change must be sampled");
    state.weapon=30;
    Check(gate.ShouldEmit(state), "Source changes must be observable");
    Check(gate.calls==5, "Calculation sequence includes suppressed calls");
    state.selected=1.3f;
    Check(gate.ShouldEmit(state), "Factor changes must be observable");
    for (int i=0;i<1000;++i) {state.runtime=static_cast<float>(i);gate.ShouldEmit(state);}
    Check(gate.emitted==8, "Per-projectile sample count must be bounded");
    DiagnosticGate npc;
    state.player=false;
    Check(npc.ShouldEmit(state), "Representative NPC calculation sampled");
}
