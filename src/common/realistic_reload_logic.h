#pragma once

#include "weapon_interaction_logic.h"
#include "weapon_accessory_logic.h"
#include <cstring>

// Titanfall-style step-by-step reload, layered on the accepted Manual Reload
// gesture. Press Reload: the part comes out. Take a fresh one from the hip
// pouch, insert it where the old one came out, then finish the weapon's own
// action (charging handle, slide, pump, cover). Only then is Halo's normal
// reload requested through the same native button transport as the original
// gesture. Like the original, this is a controller-input layer: it never
// writes ammo, inventory, animation clocks or native tags.
//
// Per-weapon recipes come from docs/MANUAL-RELOAD-WEAPON-REFERENCE-2026-10-13.md
// (official kit node names plus Halopedia). They are gesture design data
// keyed by catalogued model name, never a native binding.
namespace realistic_reload
{
using weapon_interaction::Vec;
using weapon_interaction::Quat;
using weapon_interaction::Rotate;
using weapon_interaction::Dot;
using weapon_interaction::Finite;
using weapon_interaction::Near;
using weapon_interaction::Normal;
using weapon_interaction::Setting;

enum class Part : uint8_t { None, Magazine, Shell, Pod, Cartridge };
enum class Finish : uint8_t { None, PullBack, Pump, Push };
// Where the new part goes, in the weapon controller's grip frame
// (OpenXR: +Y up, -Z along the barrel). Magazines prefer the authored
// receiver point published from the native weapon when it is available.
enum class Port : uint8_t { Under, Grip, UnderFront, Top, Rear, Breech };
// Where the finishing action is taken from.
enum class Handle : uint8_t { None, TopRail, Slide, Forend, RearBar, Cover };

struct Recipe
{
    Part part{Part::Magazine};
    Finish finish{Finish::None};
    Port port{Port::Under};
    Handle handle{Handle::None};
};

inline Vec PortLocal(Port port) noexcept
{
    switch(port)
    {
    case Port::Grip:return {0,-.08f,.03f};
    case Port::UnderFront:return {0,-.05f,-.12f};
    case Port::Top:return {0,.09f,-.06f};
    case Port::Rear:return {0,.05f,.22f};
    case Port::Breech:return {0,.07f,-.10f};
    default:return {0,-.06f,-.04f}; // the accepted gesture's fallback receiver
    }
}
inline Vec HandleLocal(Handle handle) noexcept
{
    switch(handle)
    {
    case Handle::TopRail:return {0,.07f,-.06f};
    case Handle::Slide:return {0,.06f,0};
    case Handle::Forend:return {0,-.02f,-.24f};
    case Handle::RearBar:return {0,.06f,.18f};
    case Handle::Cover:return {0,.08f,-.08f};
    default:return {};
    }
}

// Lowercase catalogue names lose their per-title prefixes: "fp_magnum",
// "storm_magnum" and "magnum" are the same recipe; CE uses spaces.
inline void Normalize(const char* name,char* out,size_t size) noexcept
{
    if(!out||!size) return;
    out[0]=0;
    if(!name) return;
    if(!std::strncmp(name,"fp_",3)) name+=3;
    if(!std::strncmp(name,"storm_",6)) name+=6;
    size_t n=0;
    for(;name[n]&&n+1<size;++n) out[n]=name[n]==' '?'_':name[n];
    out[n]=0;
}

struct Entry { const char* name; Recipe recipe; };
inline constexpr Recipe kNoReload{Part::None,Finish::None,Port::Under,Handle::None};
inline constexpr Entry kRecipes[]{
    // UNSC magazine weapons: magazine, then the operating/charging handle.
    {"assault_rifle",{Part::Magazine,Finish::PullBack,Port::Under,Handle::TopRail}},
    {"battle_rifle",{Part::Magazine,Finish::PullBack,Port::Under,Handle::TopRail}},
    {"br",{Part::Magazine,Finish::PullBack,Port::Under,Handle::TopRail}},
    {"dmr",{Part::Magazine,Finish::PullBack,Port::Under,Handle::TopRail}},
    {"smg",{Part::Magazine,Finish::PullBack,Port::Under,Handle::TopRail}},
    {"smg_silenced",{Part::Magazine,Finish::PullBack,Port::Under,Handle::TopRail}},
    {"gpmg",{Part::Magazine,Finish::PullBack,Port::Under,Handle::TopRail}},
    {"sniper_rifle",{Part::Magazine,Finish::PullBack,Port::Under,Handle::TopRail}},
    {"lmg",{Part::Magazine,Finish::None,Port::Under,Handle::None}},
    // Pistols: magazine into the grip, then the slide.
    {"magnum",{Part::Magazine,Finish::PullBack,Port::Grip,Handle::Slide}},
    {"automag",{Part::Magazine,Finish::PullBack,Port::Grip,Handle::Slide}},
    {"pistol",{Part::Magazine,Finish::PullBack,Port::Grip,Handle::Slide}},
    // Shotguns: one shell per insertion, then the pump.
    {"shotgun",{Part::Shell,Finish::Pump,Port::UnderFront,Handle::Forend}},
    // Single loads and pods closed by a bar, hatch, cover or breech.
    {"rocket_launcher",{Part::Pod,Finish::Push,Port::Rear,Handle::RearBar}},
    {"concussion_rifle",{Part::Pod,Finish::Push,Port::Top,Handle::Cover}},
    {"grenade_launcher",{Part::Pod,Finish::Push,Port::Breech,Handle::Cover}},
    {"rail_gun",{Part::Pod,Finish::Push,Port::Breech,Handle::Cover}},
    {"sticky_detonator",{Part::Pod,Finish::Push,Port::Breech,Handle::Cover}},
    {"flamethrower",{Part::Pod,Finish::Push,Port::Under,Handle::Cover}},
    // Covenant/Banished: cells, drums, belts, rods and crystals.
    {"covenant_carbine",{Part::Magazine,Finish::None,Port::Under,Handle::None}},
    {"spike_rifle",{Part::Magazine,Finish::PullBack,Port::Under,Handle::TopRail}},
    {"excavator",{Part::Magazine,Finish::None,Port::Under,Handle::None}},
    {"brute_shot",{Part::Pod,Finish::None,Port::Top,Handle::None}},
    {"flak_cannon",{Part::Pod,Finish::None,Port::Top,Handle::None}},
    {"fuel_rod_cannon",{Part::Pod,Finish::None,Port::Top,Handle::None}},
    {"plasma_cannon",{Part::Pod,Finish::None,Port::Top,Handle::None}},
    {"needle_rifle",{Part::Pod,Finish::None,Port::Top,Handle::None}},
    {"needler",{Part::Pod,Finish::None,Port::Under,Handle::None}},
    // Halo 4 Forerunner: hard-light cartridge. Insertion point unconfirmed.
    {"forerunner_smg",{Part::Cartridge,Finish::None,Port::Under,Handle::None}},
    {"forerunner_rifle",{Part::Cartridge,Finish::None,Port::Under,Handle::None}},
    {"spread_gun",{Part::Cartridge,Finish::None,Port::Under,Handle::None}},
    {"stasis_pistol",{Part::Cartridge,Finish::None,Port::Under,Handle::None}},
    {"forerunner_sniper_rifle",{Part::Cartridge,Finish::None,Port::Under,Handle::None}},
    {"forerunner_incineration_launcher",{Part::Cartridge,Finish::None,Port::Under,Handle::None}},
    // No manual reload: overheating, batteries, melee and objectives. The
    // Reload button passes straight through to Halo for these.
    {"plasma_rifle",kNoReload},{"plasma_rifle_red",kNoReload},{"brute_plasma_rifle",kNoReload},
    {"plasma_pistol",kNoReload},{"plasma_repeater",kNoReload},{"plasma_launcher",kNoReload},
    {"focus_rifle",kNoReload},{"spartan_laser",kNoReload},{"beam_rifle",kNoReload},
    {"sentinel_gun",kNoReload},{"energy_blade",kNoReload},{"energy_sword",kNoReload},
    {"flood_sword",kNoReload},{"gravity_hammer",kNoReload},{"flag",kNoReload},
    {"ball",kNoReload},{"skull",kNoReload},{"assault_bomb",kNoReload},{"target_laser",kNoReload},
    {"jackal_shield",kNoReload},{"bishop_beam",kNoReload},{"attach_beam",kNoReload},
    {"blam_bomb",kNoReload},{"burst_pistol",kNoReload},{"stasis_rifle",kNoReload},
    {"assault_carbine",kNoReload},
};

// Unknown or custom weapons get the plain magazine recipe without a finish.
inline Recipe Lookup(GameTitle title,const weapon_model::Model* model) noexcept
{
    if(!model||!model->name) return {};
    char name[64];
    Normalize(model->name,name,sizeof name);
    for(const auto& entry:kRecipes)
    {
        if(std::strcmp(entry.name,name)) continue;
        Recipe recipe=entry.recipe;
        // Halopedia: Halo 2's sniper needs no charging step after a fresh
        // magazine, and Halo 2's shotgun does not work the action.
        if(title==GameTitle::Halo2&&(!std::strcmp(name,"sniper_rifle")||!std::strcmp(name,"shotgun")))
        { recipe.finish=Finish::None;recipe.handle=Handle::None; }
        return recipe;
    }
    return {};
}

struct Settings
{
    bool enabled{};
    bool finishStep{true};  // require the charging handle/slide/pump/cover step
    bool blockFire{true};   // the trigger does nothing while the part is out
    float finishRadius{0.12f};
    float finishTravel{0.06f};
    unsigned holdPassMs{350}; // hold Reload this long to use it natively instead
};

struct Sample
{
    weapon_interaction::Sample base{};
    bool reloadSourceHeld{};
    bool reloadSourceSupported{};
};

enum class Phase : uint8_t { Idle, Ejected, Holding, Inserted, Finishing };

struct Output
{
    bool withholdReload{}, blockFire{};
    bool consumeSupport{}, releaseTwoHand{};
    bool ejected{}, picked{}, inserted{}, droppedHeld{}, finished{}, reloadRequested{}, cancelled{};
    bool holding{};
    Phase phase{Phase::Idle};
    Part part{Part::None};
    float primaryHaptic{}, supportHaptic{};
    uint32_t buttons{};
    uint64_t pulseUntil{};
    unsigned shells{};
    Vec ejectAt{};       // where the old part leaves the weapon (tracking space)
    Quat ejectRotation{};
};

inline Vec Unrotate(Quat q,Vec v) noexcept { return Rotate({-q.x,-q.y,-q.z,q.w},v); }

class State
{
public:
    void Reset() noexcept { *this=State{}; }
    Phase CurrentPhase() const noexcept { return phase_; }
    bool HasClaimedGrip() const noexcept { return owned_; }

    Output Update(const Sample& in,const weapon_interaction::Settings& c,const Settings& r) noexcept
    {
        using namespace weapon_interaction;
        Output out{};
        const auto& s=in.base;
        const bool validS=std::isfinite(s.supportGrip)&&s.supportGrip>=0&&s.supportGrip<=1;
        const bool heldS=validS&&s.supportGrip>0.55f;
        const bool releasedS=validS&&s.supportGrip<0.35f;
        const bool identity=title_!=s.title||generation_!=s.generation||space_!=s.space||
            graph_!=s.weaponGraph||left_!=c.leftHanded||enabled_!=r.enabled||
            reloadButton_!=c.reloadButton||finishStep_!=r.finishStep;
        const bool gap=!last_||s.now<last_||s.now-last_>200;
        const bool ready=r.enabled&&c.reload&&in.reloadSourceSupported&&s.ready&&s.now&&s.space&&
            s.generation&&TitleIndex(s.title)>=0&&!s.dualWield&&s.weaponGraph&&validS&&c.reloadButton&&
            Finite(s.primary)&&Finite(s.support)&&Normal(s.primaryRotation);
        recipe_=Lookup(s.title,weapon_model::Find(s.title,s.weaponGraph));
        Vec pouch{},holster{};
        const bool poses=ready&&Zones(s,c,pouch,holster);
        if(identity||gap||!ready||!poses)
        {
            if(phase_!=Phase::Idle) out.cancelled=true;
            phase_=Phase::Idle;shells_=0;pulseUntil_=0;pulse_=0;
            // A press already in progress when the flow becomes unavailable is
            // passed through from now on; it never becomes a delayed eject.
            pressing_=false;passthrough_=in.reloadSourceHeld;armed_=false;
        }
        title_=s.title;generation_=s.generation;space_=s.space;graph_=s.weaponGraph;
        left_=c.leftHanded;enabled_=r.enabled;reloadButton_=c.reloadButton;finishStep_=r.finishStep;last_=s.now;
        // Grip ownership lasts through its release frame, including cancellation.
        out.consumeSupport=owned_;
        if(releasedS) { owned_=false;armed_=true; }
        if(!ready||!poses)
        {
            if(!in.reloadSourceHeld) passthrough_=false;
            return out;
        }
        out.part=recipe_.part;

        // Reload source: a tap ejects, a hold is Halo's own Reload/Use/pick up.
        if(recipe_.part==Part::None)
        {
            pressing_=false;passthrough_=in.reloadSourceHeld;
        }
        else if(in.reloadSourceHeld)
        {
            if(!pressing_&&!passthrough_) { pressing_=true;pressedAt_=s.now; }
            if(pressing_&&s.now-pressedAt_>=r.holdPassMs)
            {
                // Native hold: cancel the step-by-step reload and hand over.
                pressing_=false;passthrough_=true;
                if(phase_!=Phase::Idle) { out.cancelled=true;phase_=Phase::Idle;shells_=0; }
            }
            out.withholdReload=pressing_;
        }
        else
        {
            if(pressing_&&phase_==Phase::Idle)
            {
                phase_=Phase::Ejected;shells_=0;
                out.ejected=true;out.primaryHaptic=0.30f;
                out.ejectAt=s.primary+Rotate(s.primaryRotation,PortLocal(recipe_.port));
                if(recipe_.part==Part::Magazine&&s.receiverValid&&Finite(s.receiver)) out.ejectAt=s.receiver;
                out.ejectRotation=s.primaryRotation;
            }
            pressing_=false;passthrough_=false;
        }

        const Vec port=PortPoint(s);
        const float pouchRadius=Setting(c.zoneRadius,0.08f,0.40f,0.20f);
        switch(phase_)
        {
        case Phase::Ejected:
        case Phase::Inserted:
            if(armed_&&heldS&&!owned_&&Near(s.support,pouch,pouchRadius)&&
               (phase_==Phase::Ejected||recipe_.part==Part::Shell))
            {
                // Another shell may be taken after one is loaded.
                phase_=Phase::Holding;owned_=true;armed_=false;grabbedAt_=s.now;leftPouch_=false;
                out.picked=true;out.supportHaptic=0.20f;out.consumeSupport=true;
            }
            else if(phase_==Phase::Inserted&&NeedsFinish()&&armed_&&heldS&&!owned_&&
                Near(s.support,HandlePoint(s),Setting(r.finishRadius,0.06f,0.25f,0.12f)))
            {
                phase_=Phase::Finishing;owned_=true;armed_=false;grabbedAt_=s.now;
                strokeStart_=Local(s);strokeBack_=false;
                out.consumeSupport=true;out.supportHaptic=0.20f;
            }
            break;
        case Phase::Holding:
        {
            out.consumeSupport=true;
            if(!Near(s.support,pouch,pouchRadius+0.04f)) leftPouch_=true;
            const Vec held=weapon_interaction::HeldMagazineCenter(s,
                Normal(s.supportRotation)?s.supportRotation:s.primaryRotation);
            if(leftPouch_&&s.now-grabbedAt_>=120&&Finite(port)&&
               Near(held,port,Setting(c.insertRadius,0.06f,0.30f,0.18f)))
            {
                // Seated: no release needed. The grip stays owned until release.
                out.inserted=true;out.supportHaptic=0.45f;out.primaryHaptic=0.25f;
                if(recipe_.part==Part::Shell) ++shells_;
                phase_=Phase::Inserted;
                if(!NeedsFinish()) Complete(out,s,c);
            }
            else if(releasedS)
            {
                // Let go away from the weapon: the part falls; take another.
                out.droppedHeld=true;
                phase_=shells_?Phase::Inserted:Phase::Ejected;
            }
            break;
        }
        case Phase::Finishing:
        {
            out.consumeSupport=true;
            const float travel=Setting(r.finishTravel,0.03f,0.15f,0.06f);
            const Vec d=Local(s)-strokeStart_;
            bool done=false;
            if(recipe_.finish==Finish::PullBack) done=d.z>=travel;
            else if(recipe_.finish==Finish::Push) done=-d.z>=travel||-d.y>=travel;
            else if(recipe_.finish==Finish::Pump)
            {
                if(!strokeBack_&&d.z>=travel) { strokeBack_=true;strokeStart_=Local(s);out.supportHaptic=0.25f; }
                else if(strokeBack_&&-d.z>=travel*0.6f) done=true;
            }
            if(done) { out.finished=true;Complete(out,s,c); }
            else if(releasedS||s.now-grabbedAt_>4000) phase_=Phase::Inserted; // try again
            break;
        }
        default:break;
        }
        // A grip already held when it reaches a zone is not a new grab.
        if(heldS) armed_=false;
        if(phase_==Phase::Holding||phase_==Phase::Finishing) out.releaseTwoHand=true;
        out.blockFire=r.blockFire&&phase_!=Phase::Idle;
        if(s.now<pulseUntil_) { out.buttons=pulse_;out.pulseUntil=pulseUntil_; }
        else { pulse_=0;pulseUntil_=0; }
        out.holding=phase_==Phase::Holding;
        out.phase=phase_;out.shells=shells_;
        return out;
    }

    // Exposed for presentation and tests.
    const Recipe& CurrentRecipe() const noexcept { return recipe_; }
    Vec PortPoint(const weapon_interaction::Sample& s) const noexcept
    {
        if(recipe_.part==Part::Magazine&&s.receiverValid&&Finite(s.receiver)) return s.receiver;
        return s.primary+Rotate(s.primaryRotation,PortLocal(recipe_.port));
    }
    Vec HandlePoint(const weapon_interaction::Sample& s) const noexcept
    { return s.primary+Rotate(s.primaryRotation,HandleLocal(recipe_.handle)); }

private:
    bool NeedsFinish() const noexcept
    {
        return finishStep_&&recipe_.finish!=Finish::None&&recipe_.handle!=Handle::None;
    }
    // Support hand in the weapon hand's frame: walking or turning the whole
    // body is not a stroke; only the hand moving along the gun is.
    static Vec Local(const weapon_interaction::Sample& s) noexcept
    { return Unrotate(s.primaryRotation,s.support-s.primary); }
    void Complete(Output& out,const weapon_interaction::Sample& s,const weapon_interaction::Settings& c) noexcept
    {
        pulse_=c.reloadButton;pulseUntil_=s.now+120;
        out.reloadRequested=true;out.primaryHaptic=0.45f;
        if(out.supportHaptic<0.35f) out.supportHaptic=0.35f;
        phase_=Phase::Idle;shells_=0;
    }

    Recipe recipe_{};
    Phase phase_{Phase::Idle};
    GameTitle title_{GameTitle::None};
    uint32_t generation_{},reloadButton_{},pulse_{};
    uint64_t space_{},graph_{},last_{},pressedAt_{},grabbedAt_{},pulseUntil_{};
    unsigned shells_{};
    Vec strokeStart_{};
    bool left_{},enabled_{},finishStep_{true},pressing_{},passthrough_{},armed_{},owned_{},leftPouch_{},strokeBack_{};
};

// Parts that leave the gun fall under gravity, rest at an estimated floor
// and disappear a few seconds later (Halo 3's carbine cell does the same).
// Tracking-space only: no native world object is created.
struct Dropped
{
    const weapon_model::Model* model{};
    Vec position{},velocity{};
    Quat orientation{};
    Vec spinAxis{1,0,0};
    float spinRate{};
    float floorY{};
    uint64_t spawnedAt{},landedAt{},last{};
    bool active{},landed{};
};
inline Quat Multiply(Quat a,Quat b) noexcept
{
    return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
inline Quat Normalized(Quat q) noexcept
{
    const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if(!std::isfinite(n)||n<1e-6f) return {0,0,0,1};
    return {q.x/n,q.y/n,q.z/n,q.w/n};
}
class DropPool
{
public:
    static constexpr unsigned kCapacity=3;
    void Reset() noexcept { *this=DropPool{}; }
    // floorDepth: how far below the head the floor is assumed to be.
    void Spawn(const weapon_model::Model* model,Vec at,Quat rotation,Vec headPosition,
        float floorDepth,uint64_t now) noexcept
    {
        if(!model||!model->vertexCount||!Finite(at)||!Normal(rotation)||!Finite(headPosition)) return;
        Dropped& d=items_[next_++%kCapacity];
        d={};
        d.model=model;d.position=at;d.orientation=rotation;
        d.velocity={0,-0.4f,0};
        // Deterministic tumble about the gun's own side axis.
        d.spinAxis=Rotate(rotation,{1,0,0});d.spinRate=(now&1)?5.0f:-4.0f;
        d.floorY=headPosition.y-Setting(floorDepth,0.8f,2.4f,1.7f);
        if(d.floorY>at.y) d.floorY=at.y;
        d.spawnedAt=d.last=now;d.active=true;
    }
    void Step(uint64_t now,float lingerSeconds) noexcept
    {
        const uint64_t linger=uint64_t(Setting(lingerSeconds,0.5f,15.0f,4.0f)*1000.0f);
        for(auto& d:items_)
        {
            if(!d.active) continue;
            if(now<d.last||now-d.spawnedAt>30000) { d.active=false;continue; }
            const float dt=std::fmin(0.05f,float(now-d.last)*0.001f);
            d.last=now;
            if(d.landed) { if(now-d.landedAt>=linger) d.active=false;continue; }
            d.velocity.y-=9.81f*dt;
            d.position=d.position+d.velocity*dt;
            const float angle=d.spinRate*dt;
            d.orientation=Normalized(Multiply(
                {d.spinAxis.x*std::sin(angle*0.5f),d.spinAxis.y*std::sin(angle*0.5f),
                 d.spinAxis.z*std::sin(angle*0.5f),std::cos(angle*0.5f)},d.orientation));
            if(d.position.y<=d.floorY) { d.position.y=d.floorY;d.landed=true;d.landedAt=now; }
        }
    }
    unsigned Active() const noexcept
    { unsigned n=0;for(const auto& d:items_) n+=d.active?1u:0u;return n; }
    const Dropped& Item(unsigned i) const noexcept { return items_[i%kCapacity]; }
private:
    Dropped items_[kCapacity]{};
    unsigned next_{};
};

// The visible reserve/held part for this weapon. Magazines use the kit-derived
// part when the catalogue has one; everything else uses a stated mod token.
inline const weapon_model::Model* PartModel(GameTitle title,uint64_t graph,Part part,bool genericVisual) noexcept
{
    const auto* model=weapon_model::Find(title,graph);
    switch(part)
    {
    case Part::None:return nullptr;
    case Part::Shell:return &weapon_model::kShellReloadModel;
    case Part::Cartridge:return &weapon_model::kPrometheanReloadModel;
    case Part::Magazine:
        if(model&&model->vertexCount) return model;
        return genericVisual?&weapon_model::kGenericReloadModel:nullptr;
    default:return genericVisual?&weapon_model::kGenericReloadModel:nullptr;
    }
}
// Only a real catalogued part leaves a copy on the floor.
inline const weapon_model::Model* EjectModel(GameTitle title,uint64_t graph,Part part) noexcept
{
    if(part!=Part::Magazine) return nullptr;
    const auto* model=weapon_model::Find(title,graph);
    return model&&model->vertexCount?model:nullptr;
}

// Reserve part at the hip, or in the support hand while it is carried. Same
// zone basis, guards and freshness rules as weapon_accessory::Build.
inline weapon_accessory::Presentation BuildPresentation(const weapon_interaction::Sample& sample,
    const weapon_interaction::Settings& settings,const State& state,const Output& output,
    Quat supportRotation) noexcept
{
    using namespace weapon_interaction;
    const auto* model=PartModel(sample.title,sample.weaponGraph,state.CurrentRecipe().part,settings.genericVisual);
    Vec pouch{},holster{};
    if(!model||!model->vertexCount||!settings.reload||!sample.ready||sample.dualWield||!sample.now||
        !sample.generation||!sample.space||!sample.weaponGraph||!Zones(sample,settings,pouch,holster)||
        !Normal(supportRotation)) return {};
    weapon_accessory::Pose pose{};
    if(output.holding)
    {
        pose.orientation=supportRotation;
        pose.position=HeldMagazineCenter(sample,supportRotation);
    }
    else
    {
        Vec forward=Rotate(sample.headRotation,{0,0,-1});forward.y=0;
        if(Dot(forward,forward)<0.04f)
        {
            const Vec right=Rotate(sample.headRotation,{1,0,0});
            forward={right.z,0,-right.x};
        }
        const float yaw=std::atan2(-forward.x,-forward.z);
        pose.orientation={0,std::sin(yaw*0.5f),0,std::cos(yaw*0.5f)};
        pose.position=pouch;
    }
    return {model,pose,sample.title,sample.generation,sample.space,sample.now,sample.weaponGraph};
}
// A part on the floor. It stays with the tracked play space and follows the
// same title/generation/space freshness as the held part.
inline weapon_accessory::Presentation DroppedPresentation(const Dropped& d,
    const weapon_interaction::Sample& sample) noexcept
{
    if(!d.active||!d.model||!Finite(d.position)||!Normal(d.orientation)||!sample.now||
        !sample.generation||!sample.space||!sample.weaponGraph) return {};
    return {d.model,{d.position,d.orientation},sample.title,sample.generation,sample.space,
        sample.now,sample.weaponGraph};
}
} // namespace realistic_reload
