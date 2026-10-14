#pragma once

#include "weapon_interaction_logic.h"
#include "weapon_accessory_logic.h"

// Chest grenades (Titanfall-style): squeeze grip with either hand at the
// grenade on the chest (weapon-hand side by default) to take it. While holding it, that controller's face buttons
// (A/B on the right controller, X/Y on the left) press Halo's Switch Grenade
// instead of their normal actions. Let go with a throwing motion to press
// Halo's Throw Grenade; let go slowly to put it back.
//
// Controller-input layer only: Halo still owns grenade counts, the selected
// type, the throw animation and the throw speed. With free throw on, the
// throw direction is published so the aim steering can point Halo's own aim
// along the arm while the grenade leaves the hand; the native press waits a
// short lead for that aim to turn. Nothing native is read or written here.
namespace grenade_gesture
{
using weapon_interaction::Vec;
using weapon_interaction::Quat;
using weapon_interaction::Rotate;
using weapon_interaction::Dot;
using weapon_interaction::Finite;
using weapon_interaction::Near;
using weapon_interaction::Normal;
using weapon_interaction::Setting;

enum class Hand : uint8_t { None, Primary, Support };

struct Settings
{
    bool enabled{}, leftHanded{};
    float zoneRadius{0.14f};
    float chestDown{0.32f};   // chest centre below the head
    float chestSide{0.10f};   // toward the weapon hand: the right chest unless left-handed
    float throwSpeed{1.5f};   // m/s at release to count as a throw
    bool freeThrow{true};     // aim Halo's throw along the arm's throw
    uint32_t throwButton{}, switchButton{};
};
// Free throw timing: aim turns this long before the native press, and stays on
// the throw this long after it, covering Halo's throw animation to release.
inline constexpr uint64_t kThrowAimLeadMs=150, kThrowAimHoldMs=800;

struct Sample
{
    uint64_t now{}, space{};
    uint32_t generation{};
    GameTitle title{GameTitle::None};
    bool ready{}, dualWield{};
    Vec head{}, primary{}, support{};
    Quat headRotation{}, primaryRotation{}, supportRotation{};
    float primaryGrip{}, supportGrip{};
    bool primaryBusy{}, supportBusy{}; // grip already claimed by another gesture
    bool a{}, b{}, x{}, y{};           // physical face buttons: A/B right, X/Y left
};

struct Output
{
    Hand holding{Hand::None};
    bool consumePrimary{}, consumeSupport{}, releaseTwoHand{};
    bool withholdA{}, withholdB{}, withholdX{}, withholdY{};
    bool withholdPrimaryTrigger{}, withholdSupportTrigger{};
    bool grabbed{}, switched{}, thrown{}, putBack{}, cancelled{};
    float primaryHaptic{}, supportHaptic{};
    uint32_t buttons{};
    uint64_t pulseUntil{};
    Vec releaseVelocity{};
    bool aimActive{};   // free throw: steer Halo's aim along aimDirection now
    Vec aimDirection{}; // unit vector, tracking space
};

// Horizontal body forward from the head, robust to looking straight down.
inline bool BodyForward(const Sample& s,Vec& forward) noexcept
{
    if(!Normal(s.headRotation)) return false;
    forward=Rotate(s.headRotation,{0,0,-1});forward.y=0;
    float n=Dot(forward,forward);
    if(!std::isfinite(n)) return false;
    if(n<0.04f)
    {
        Vec side=Rotate(s.headRotation,{1,0,0});side.y=0;
        n=Dot(side,side);
        if(!std::isfinite(n)||n<0.04f) return false;
        forward={side.z,0,-side.x};n=Dot(forward,forward);
    }
    forward=forward*(1/std::sqrt(n));
    return true;
}
// The player's horizontal right for a body forward (OpenXR: +Y up, -Z ahead).
inline Vec BodyRight(Vec forward) noexcept { return {-forward.z,0,forward.x}; }
// Grenade on the chest, offset sideways toward the weapon hand.
inline bool Chest(const Sample& s,const Settings& c,Vec& chest) noexcept
{
    Vec forward{};
    if(!Finite(s.head)||!BodyForward(s,forward)) return false;
    const float side=Setting(c.chestSide,-0.30f,0.30f,0.10f)*(c.leftHanded?-1.0f:1.0f);
    chest=s.head+forward*0.12f+BodyRight(forward)*side+Vec{0,-Setting(c.chestDown,0.15f,0.60f,0.32f),0};
    return Finite(chest);
}
// Grenade centre in the palm of the holding controller.
inline Vec HeldCenter(Vec hand,Quat rotation) noexcept
{ return hand+Rotate(rotation,{0,-.01f,-.04f}); }

class State
{
public:
    void Reset() noexcept { *this=State{}; }
    Hand Holding() const noexcept { return holding_; }
    bool HasClaimedGrip() const noexcept { return ownedP_||ownedS_; }
    // Physical right controller is the weapon hand unless left-handed.
    static bool RightController(Hand hand,bool leftHanded) noexcept
    { return hand!=Hand::None&&((hand==Hand::Primary)!=leftHanded); }

    Output Update(const Sample& s,const Settings& c) noexcept
    {
        Output out{};
        const bool validP=std::isfinite(s.primaryGrip)&&s.primaryGrip>=0&&s.primaryGrip<=1;
        const bool validS=std::isfinite(s.supportGrip)&&s.supportGrip>=0&&s.supportGrip<=1;
        const bool heldP=validP&&s.primaryGrip>0.55f, heldS=validS&&s.supportGrip>0.55f;
        const bool releasedP=validP&&s.primaryGrip<0.35f, releasedS=validS&&s.supportGrip<0.35f;
        const bool identity=title_!=s.title||generation_!=s.generation||space_!=s.space||
            left_!=c.leftHanded||enabled_!=c.enabled||throwButton_!=c.throwButton||switchButton_!=c.switchButton;
        const bool gap=!last_||s.now<last_||s.now-last_>200;
        Vec chest{};
        const bool ready=c.enabled&&c.throwButton&&s.ready&&s.now&&s.space&&s.generation&&
            weapon_interaction::TitleIndex(s.title)>=0&&!s.dualWield&&validP&&validS&&
            Finite(s.primary)&&Finite(s.support)&&Chest(s,c,chest);
        if(identity||gap||!ready)
        {
            if(holding_!=Hand::None) out.cancelled=true;
            holding_=Hand::None;pending_=0;throwPending_=false;pulse_=0;pulseUntil_=0;
            armedP_=armedS_=false;history_=0;aiming_=false;aimUntil_=0;
        }
        title_=s.title;generation_=s.generation;space_=s.space;left_=c.leftHanded;enabled_=c.enabled;
        throwButton_=c.throwButton;switchButton_=c.switchButton;last_=s.now;
        out.consumePrimary=ownedP_;out.consumeSupport=ownedS_;
        if(releasedP) { ownedP_=false;armedP_=true; }
        if(releasedS) { ownedS_=false;armedS_=true; }

        if(ready)
        {
            if(holding_==Hand::None)
            {
                // Off hand first: grenades are normally taken with the free hand.
                if(armedS_&&heldS&&!s.supportBusy&&!ownedS_&&Near(s.support,chest,Setting(c.zoneRadius,0.06f,0.30f,0.14f)))
                    Grab(Hand::Support,out,s,c);
                else if(armedP_&&heldP&&!s.primaryBusy&&!ownedP_&&Near(s.primary,chest,Setting(c.zoneRadius,0.06f,0.30f,0.14f)))
                    Grab(Hand::Primary,out,s,c);
            }
            else
            {
                const bool primary=holding_==Hand::Primary;
                const Vec hand=primary?s.primary:s.support;
                Remember(s.now,hand);
                // Each fresh face-button press on the holding controller is one
                // native Switch Grenade press.
                const bool right=RightController(holding_,c.leftHanded);
                const bool first=right?s.a:s.x, second=right?s.b:s.y;
                if(((first&&!prevFirst_)||(second&&!prevSecond_))&&c.switchButton&&pending_<3)
                { ++pending_;out.switched=true;(primary?out.primaryHaptic:out.supportHaptic)=0.15f; }
                prevFirst_=first;prevSecond_=second;
                if(primary?releasedP:releasedS)
                {
                    Vec forward{};
                    const Vec v=Velocity(s.now,hand);
                    out.releaseVelocity=v;
                    const float speed=std::sqrt(Dot(v,v));
                    if(std::isfinite(speed)&&speed>=Setting(c.throwSpeed,0.6f,4.0f,1.5f)&&
                       BodyForward(s,forward)&&Dot(v,forward)>0)
                    {
                        throwPending_=true;out.thrown=true;
                        (primary?out.primaryHaptic:out.supportHaptic)=0.50f;
                        if(c.freeThrow)
                        {
                            // Aim first, then press: Halo throws along its aim.
                            throwDirection_=v*(1/speed);aiming_=true;aimUntil_=0;
                            nextPulse_=(std::max)(nextPulse_,s.now+kThrowAimLeadMs);
                        }
                    }
                    else out.putBack=true;
                    holding_=Hand::None;history_=0;
                }
            }
        }
        if(holding_!=Hand::None)
        {
            const bool primary=holding_==Hand::Primary;
            (primary?out.consumePrimary:out.consumeSupport)=true;
            if(!primary) out.releaseTwoHand=true;
            (primary?out.withholdPrimaryTrigger:out.withholdSupportTrigger)=true;
            if(RightController(holding_,c.leftHanded)) { drainA_|=s.a;drainB_|=s.b; }
            else { drainX_|=s.x;drainY_|=s.y; }
        }
        else prevFirst_=prevSecond_=false;
        // A face button pressed while holding stays hidden from Halo until it
        // is released, so letting go of the grenade cannot jump or reload.
        drainA_=drainA_&&s.a;drainB_=drainB_&&s.b;drainX_=drainX_&&s.x;drainY_=drainY_&&s.y;
        out.withholdA=drainA_;out.withholdB=drainB_;out.withholdX=drainX_;out.withholdY=drainY_;
        if(holding_!=Hand::None)
        {
            const bool right=RightController(holding_,c.leftHanded);
            if(right) out.withholdA=out.withholdB=true; else out.withholdX=out.withholdY=true;
        }
        // Separate presses with a gap so Halo sees each one; a throw waits for
        // any switch the player asked for first.
        if(s.now>=pulseUntil_) pulse_=0;
        if(!pulse_&&s.now>=nextPulse_)
        {
            if(pending_&&c.switchButton) { --pending_;Pulse(c.switchButton,s.now); }
            else if(throwPending_)
            {
                throwPending_=false;Pulse(c.throwButton,s.now);
                if(aiming_) aimUntil_=s.now+kThrowAimHoldMs;
            }
        }
        if(pulse_) { out.buttons=pulse_;out.pulseUntil=pulseUntil_; }
        // Steer from release through Halo's throw animation; a later throw
        // replaces the direction, and switching free throw off ends it.
        if(aiming_&&(!c.freeThrow||(!throwPending_&&aimUntil_&&s.now>=aimUntil_))) { aiming_=false;aimUntil_=0; }
        if(aiming_&&Finite(throwDirection_)) { out.aimActive=true;out.aimDirection=throwDirection_; }
        // A grip already squeezed before reaching the chest is not a grab.
        if(heldP) armedP_=false;
        if(heldS) armedS_=false;
        out.holding=holding_;
        return out;
    }

private:
    void Grab(Hand hand,Output& out,const Sample& s,const Settings& c) noexcept
    {
        holding_=hand;history_=0;
        // A button already down when the grenade is taken is not a press.
        const bool right=RightController(hand,c.leftHanded);
        prevFirst_=right?s.a:s.x;prevSecond_=right?s.b:s.y;
        if(hand==Hand::Primary) { ownedP_=true;armedP_=false;out.consumePrimary=true;out.primaryHaptic=0.25f; }
        else { ownedS_=true;armedS_=false;out.consumeSupport=true;out.supportHaptic=0.25f;out.releaseTwoHand=true; }
        out.grabbed=true;
        Remember(s.now,hand==Hand::Primary?s.primary:s.support);
    }
    void Pulse(uint32_t button,uint64_t now) noexcept
    { pulse_=button;pulseUntil_=now+100;nextPulse_=now+170; }
    void Remember(uint64_t now,Vec p) noexcept
    {
        if(!Finite(p)) return;
        times_[head_]=now;points_[head_]=p;head_=(head_+1)%kHistory;
        if(history_<kHistory) ++history_;
    }
    // Hand velocity over roughly the last 80 ms before release.
    Vec Velocity(uint64_t now,Vec current) const noexcept
    {
        for(unsigned i=1;i<=history_;++i)
        {
            const unsigned index=(head_+kHistory-i)%kHistory;
            if(now<times_[index]) break;
            const uint64_t dt=now-times_[index];
            if(dt>=60&&dt<=250) return (current-points_[index])*(1000.0f/float(dt));
        }
        return {};
    }
    static constexpr unsigned kHistory=24;
    Vec points_[kHistory]{};
    uint64_t times_[kHistory]{};
    unsigned head_{},history_{},pending_{};
    Hand holding_{Hand::None};
    GameTitle title_{GameTitle::None};
    uint32_t generation_{},throwButton_{},switchButton_{},pulse_{};
    uint64_t space_{},last_{},pulseUntil_{},nextPulse_{},aimUntil_{};
    Vec throwDirection_{};
    bool aiming_{};
    bool left_{},enabled_{},armedP_{},armedS_{},ownedP_{},ownedS_{},throwPending_{};
    bool prevFirst_{},prevSecond_{},drainA_{},drainB_{},drainX_{},drainY_{};
};

// Grenade at the chest, or in the holding hand.
inline weapon_accessory::Presentation BuildPresentation(const Sample& s,const Settings& c,
    const Output& out,uint64_t identity) noexcept
{
    if(!c.enabled||!s.ready||s.dualWield||!s.now||!s.generation||!s.space) return {};
    weapon_accessory::Pose pose{};
    if(out.holding!=Hand::None)
    {
        const bool primary=out.holding==Hand::Primary;
        const Quat rotation=primary?s.primaryRotation:s.supportRotation;
        const Vec hand=primary?s.primary:s.support;
        if(!Normal(rotation)||!Finite(hand)) return {};
        pose.orientation=rotation;pose.position=HeldCenter(hand,rotation);
    }
    else
    {
        Vec chest{},forward{};
        if(!Chest(s,c,chest)||!BodyForward(s,forward)) return {};
        const float yaw=std::atan2(-forward.x,-forward.z);
        pose.orientation={0,std::sin(yaw*0.5f),0,std::cos(yaw*0.5f)};
        pose.position=chest;
    }
    return {&weapon_model::kGrenadeTokenModel,pose,s.title,s.generation,s.space,s.now,identity};
}
} // namespace grenade_gesture
