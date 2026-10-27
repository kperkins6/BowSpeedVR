#include "BowSpeed/MatchedRuntime.h"
#include "Check.h"
#include "BowSpeed/PairingNativeGuard.h"
#include <iostream>
#include <limits>
int main() {
 try {
    using namespace BowSpeed;
    Check(RecognizedPairingDraw(DrawGetterRVA,DrawGetterBytes,DrawPowerBytes,DrawScaleBytes),"Inspected native draw getter and both helpers recognized");
    auto changedDraw=DrawScaleBytes;changedDraw[0]^=1;
    Check(!RecognizedPairingDraw(DrawGetterRVA,DrawGetterBytes,DrawPowerBytes,changedDraw),"Late foreign draw-helper patch rejected");
    Check(!RecognizedPairingDraw(DrawGetterRVA+1,DrawGetterBytes,DrawPowerBytes,DrawScaleBytes),"Foreign draw virtual rejected");
    Check(!RuntimeGetterFactor(5000,3e38,1,3e38),"Runtime getter refuses double factor that underflows engine float");
    Check(!RuntimeGetterFactor(5000,1e-38,1,1e-38),"Runtime getter refuses factor that overflows engine float");
    Check(RuntimeGetterFactor(5000,5000,1,.8)==1.25f,"Runtime factor cancels mounted multiplier without legacy stacking");
    const auto overrides=ParseConfig("[PairingBowPower]\nSkyrim.esm|0x0139B5=1.4\n");
    Check(Near(static_cast<float>(ResolvePairingOverride(overrides.pairingBowPower,"skyrim.esm",0x139B5,.5)),1.4f),"Runtime catalogue lookup honors canonical Skyrim override key");
    Check(ResolvePairingOverride(overrides.pairingBowPower,"other.esp",0x139B5,.5)==.5,"Unknown override preserves catalogue default");
    Check(!PairingLaunchTrusted(true,true,0),"Invalid native launch factor cannot record a paired impact snapshot");
    Check(!PairingLaunchTrusted(true,true,std::numeric_limits<double>::quiet_NaN()),"NaN native launch factor cannot claim paired launch");
    Check(PairingLaunchTrusted(true,true,.7),"Trusted native bow contribution permits paired launch");
    PairingEligibility eligible{true,false,true,true,true,true,true,true,true,false,true,true};
    Check(PairingPermitted(eligible),"Reviewed player bow and ordinary known arrow pair when both paths are trusted");
    for (auto member : {&PairingEligibility::enabled,&PairingEligibility::launchTrusted,&PairingEligibility::impactTrusted,&PairingEligibility::identityTrusted,&PairingEligibility::player,&PairingEligibility::bow,&PairingEligibility::reviewedBow,&PairingEligibility::knownArrow,&PairingEligibility::ordinaryForce,&PairingEligibility::physicalArrow}) {
        auto changed=eligible;changed.*member=false;
        Check(!PairingPermitted(changed),"Unknown/helper/NPC/crossbow/special or late foreign hook must bypass pairing");
    }
    eligible.gravityException=true;Check(!PairingPermitted(eligible),"Unverified player gravity arrows excluded");
    eligible.gravityException=false;eligible.observed=true;Check(!PairingPermitted(eligible),"Observation never applies pairing");
    ShotSnapshots snapshots;
    LaunchSnapshot shot{{0x1000,1,0xFF000001,0x123},0x20,0x30,2.0,4.0,5000,1.2,0,true,true};
    auto forceShot=shot;forceShot.mass=1;forceShot.baseForce=1.5;
    Check(SelectMatchedImpactForce(forceShot,1.5,1.65,5000,true,true)==1.0,"Matched impact divides out mounted force without stacking legacy class");
    forceShot.baseForce=4;
    Check(SelectMatchedImpactForce(forceShot,4,4.4,1250,true,true)==.75,"Impact ratio uses actual incoming speed and force floor");
    forceShot.momentumImpact=false;
    Check(SelectMatchedImpactForce(forceShot,4,4.4,5000,true,true)==4,"MomentumImpact false preserves native force for paired shot");
    Check(SelectMatchedImpactForce({},10,11,5000,true,true)==10,"Special force10 stays native even without eligible snapshot");
    Check(SelectMatchedImpactForce({},4,4.4,5000,true,false)==4,"Late foreign hook passes impact through");
    Check(SelectMatchedImpactForce({},4,4.4,5000,true,true)==4.4,"Unknown pairing snapshot keeps ordinary legacy impact");
    forceShot.momentumImpact=true;
    Check(SelectMatchedImpactForce(forceShot,4,4.4,std::numeric_limits<double>::quiet_NaN(),true,true)==4,"Invalid actual impact speed keeps native force");
    Check(snapshots.Record(shot).has_value(),"Eligible launch is captured");
    auto swapped=shot;swapped.ammo=0x40;swapped.mass=.25;swapped.baseForce=1.5;
    snapshots.Record(swapped);
    auto found=snapshots.Find(shot.identity,0.5,true);
    Check(found && found->mass==2.0 && found->ammo==0x30 && found->baseForce==4.0,"Impact retains original ammo mass and force independently of replacement ammo");
    auto reused=shot.identity;reused.handle=2;
    Check(!snapshots.Find(reused,1,true),"Reused pointer/FormID with different generation invalidates snapshot");
    Check(!snapshots.Find(shot.identity,1,true),"Invalidated generation cannot recover stale snapshot");
    snapshots.Record(shot);reused=shot.identity;reused.form++;
    Check(!snapshots.Find(reused,1,true),"Reused pointer with different form rejected");
    snapshots.Record(shot);reused=shot.identity;reused.base++;
    Check(!snapshots.Find(reused,1,true),"Magical projectile base replacement rejected");
    snapshots.Record(shot);Check(!snapshots.Find(shot.identity,1,false),"Late hook conflict drops paired impact");
    Check(!snapshots.Find(shot.identity,1,true),"Trust loss permanently invalidates old shot");
    snapshots.Record(shot);Check(snapshots.Find(shot.identity,2,true).has_value(),"Age advances normally");
    Check(!snapshots.Find(shot.identity,1,true),"Age reset invalidates reused projectile");
    snapshots.Record(shot);Check(!snapshots.Find(shot.identity,121,true),"Old shots expire");
    shot.mass=std::numeric_limits<double>::quiet_NaN();Check(!snapshots.Record(shot),"Invalid snapshot data refused");shot.mass=2;
    shot.eligible=false;Check(!snapshots.Record(shot),"Ineligible launch not retained");shot.eligible=true;
    snapshots.Record(shot);
    for (unsigned i=0;i<ShotSnapshots::Capacity;++i) {auto next=shot;next.identity.pointer+=i+1;next.identity.handle+=i+1;next.identity.form+=i+1;snapshots.Record(next);}
    Check(!snapshots.Find(shot.identity,1,true),"Bounded storage evicts oldest launch");
    snapshots.Clear();
    const auto expiredSlot=snapshots.Record(shot);
    snapshots.Forget(*expiredSlot);
    Check(!snapshots.Find(shot.identity,0,true),"Owner expiry removes metadata before pointer or handle reuse");
    snapshots.Clear();Check(!snapshots.Find({0x1001,2,0xFF000002,0x123},1,true),"Session reset drops all launch metadata");
    std::cout<<"Matched runtime checks passed\n";return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
