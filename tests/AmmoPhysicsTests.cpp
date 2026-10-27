#include "BowSpeed/AmmoPhysics.h"
#include "BowSpeed/ImpactGuard.h"
#include "BowSpeed/Config.h"
#include "BowSpeed/Policy.h"
#include "Check.h"
#include <limits>
void ImpactThunkTests();
void AmmoPhysicsTests() {
    using namespace BowSpeed;
    const auto light=CalculateAmmoModifiers(AmmoClass::Light,0.2f);
    const auto medium=CalculateAmmoModifiers(AmmoClass::Medium,0.2f);
    const auto heavy=CalculateAmmoModifiers(AmmoClass::Heavy,0.2f);
    const auto massive=CalculateAmmoModifiers(AmmoClass::Massive,0.2f);
    const auto bound=CalculateAmmoModifiers(AmmoClass::ExtraLight,0.2f);
    Check(Near(light.speed,1.0717735f) && Near(light.impact,0.9330330f), "Light arrow factors match approved compressed mass model");
    Check(medium.speed==1 && medium.impact==1, "Medium is the unchanged reference");
    Check(Near(heavy.speed,light.impact) && Near(heavy.impact,light.speed), "Heavy factors reverse light factors");
    Check(Near(massive.speed,0.8959585f) && Near(massive.impact,1.1161232f), "Massive factors match approved model");
    Check(Near(bound.speed,massive.impact) && Near(bound.impact,massive.speed), "Bound is extra light and exactly opposite massive");
    for (const auto category : {AmmoClass::ExtraLight,AmmoClass::Light,AmmoClass::Medium,AmmoClass::Heavy,AmmoClass::Massive}) {
        const auto a=CalculateAmmoModifiers(category,0.2f);
        Check(Near(a.speed*a.impact,1), "Effective constant-energy speed and momentum factors are inverse");
        Check(CalculateAmmoModifiers(category,0).speed==1, "Zero influence disables ammo effect");
    }
    Check(CalculateAmmoModifiers(std::nullopt,0.2f).speed==1, "Unknown arrow is neutral");
    for (float invalid : {-0.01f,0.41f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        Check(CalculateAmmoModifiers(AmmoClass::Massive,invalid).speed==1, "Invalid influence is neutral");
    int ammo=0,unknown=0;
    const AmmoClasses classes{{&ammo,AmmoClass::Massive}};
    Check(Near(ResolveAmmoModifiers(true,true,true,&ammo,classes,0.2f).speed,massive.speed), "Stored player bow ammo selects mass class");
    for (auto args : {std::array<bool,3>{false,true,true},{true,false,true},{true,true,false}})
        Check(ResolveAmmoModifiers(args[0],args[1],args[2],&ammo,classes,0.2f).speed==1, "Disabled, NPC, crossbow or unusable source is neutral");
    Check(ResolveAmmoModifiers(true,true,true,nullptr,classes,0.2f).impact==1, "Missing ammo passes through");
    Check(ResolveAmmoModifiers(true,true,true,&unknown,classes,0.2f).impact==1, "Unmapped mod ammo passes through");
    const float longSpeed=SelectWeaponFactor(true,true,std::nullopt,0.7f,1.25f)*massive.speed;
    const float daedricSpeed=SelectWeaponFactor(true,true,std::nullopt,1.0f,1.25f)*massive.speed;
    Check(Near(daedricSpeed/longSpeed,1/0.7f), "Ammo preserves native differences across bows");
    Check(Near(ScalePhysicalImpact(1.5f,bound.impact),1.3439378f), "Bound scales its existing force rather than erasing the record");
    Check(ScalePhysicalImpact(0,massive.impact)==0, "No new impulse invented for zero native force");
    Check(ScalePhysicalImpact(1,std::numeric_limits<float>::infinity())==1, "Invalid impact factor preserves native force");
    const auto defaults=DefaultAmmoClasses();
    Check(defaults.at({"Skyrim.esm",0x139BB})==AmmoClass::Heavy, "Orcish is heavy per Requiem's own data");
    Check(defaults.at({"Skyrim.esm",0x10B0A7})==AmmoClass::ExtraLight, "Bound Arrow default is extra light");
    Check(defaults.at({"dawnguard.esm",0x176F4})==AmmoClass::Massive, "Dragonbone is massive");
    Check(!defaults.contains({"dawnguard.esm",0xBB3}), "Bolts have no mass mapping");
    Check(RecognizedImpactPath(true,true,ImpactContext,NativeForceEntry), "Inspected native VR impact code is accepted");
    for (std::size_t i=0;i<ImpactContext.size();++i) {
        auto changed=ImpactContext;changed[i]^=1;
        Check(!RecognizedImpactPath(true,true,changed,NativeForceEntry), "A conflicting call, changed force offset or register must refuse impact hook");
    }
    Check(!RecognizedImpactPath(false,true,ImpactContext,NativeForceEntry), "Other runtime refuses impact hook");
    Check(!RecognizedImpactPath(true,false,ImpactContext,NativeForceEntry), "Nonexecutable impact path refuses hook");
    auto changed=NativeForceEntry;changed[0]=0xE9;
    Check(!RecognizedImpactPath(true,true,ImpactContext,changed), "A foreign native force hook is refused");
    ImpactThunkTests();
}
