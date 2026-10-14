#include "../src/common/grenade_gesture_logic.h"
#include "../src/common/weapon_generic_magazine.h"
#include <cstdio>
#include <iterator>
#include <cmath>
#include <limits>
#include <cmath>

using namespace grenade_gesture;
unsigned checks{},failures{};
void Check(bool ok,const char* message)
{ ++checks;if(!ok){++failures;std::printf("FAIL: %s\n",message);} }

constexpr uint32_t kThrow=1u<<16, kSwitch=0x100;
struct Rig
{
    State state;
    Settings c;
    Sample s;
    Vec chest{};
    Output last{};
    explicit Rig(bool left=false)
    {
        c.enabled=true;c.leftHanded=left;c.throwButton=kThrow;c.switchButton=kSwitch;
        s.title=GameTitle::Halo3;s.generation=2;s.space=5;s.ready=true;s.now=1000;
        s.head={0,1.65f,0};s.headRotation=s.primaryRotation=s.supportRotation={0,0,0,1};
        s.primary={0.22f,1.25f,-0.45f};s.support={-0.25f,1.20f,-0.40f};
        Chest(s,c,chest);
        Step();Step();
    }
    Output Step(unsigned dt=11) { s.now+=dt;last=state.Update(s,c);return last; }
    Output GrabSupport() { s.support=chest;s.supportGrip=0;Step();s.supportGrip=1;return Step(); }
    Output GrabPrimary() { s.primary=chest;s.primaryGrip=0;Step();s.primaryGrip=1;return Step(); }
    // Move the holding hand along a direction at a speed, then let go.
    Output Throw(bool support,Vec direction,float speed,unsigned frames=12)
    {
        Output o{};
        for(unsigned i=0;i<frames;++i)
        {
            Vec& hand=support?s.support:s.primary;
            hand=hand+direction*(speed*0.011f);
            o=Step();
        }
        (support?s.supportGrip:s.primaryGrip)=0;
        return Step();
    }
    // Run frames until any pulse appears; return its button.
    uint32_t NextPulse(unsigned frames=40)
    {
        for(unsigned i=0;i<frames;++i) { auto o=Step(); if(o.buttons) return o.buttons; }
        return 0;
    }
};

void ChestZoneAndGrab()
{
    Rig rig;
    Check(Finite(rig.chest)&&rig.chest.y<1.65f-0.2f&&rig.chest.z<0,"chest in front, below the head");
    auto o=rig.GrabSupport();
    Check(o.grabbed&&o.holding==Hand::Support&&o.consumeSupport&&o.releaseTwoHand,"support hand takes grenade");
    Check(o.withholdX&&o.withholdY&&!o.withholdA,"left buttons belong to the grenade");
    Check(o.withholdSupportTrigger&&!o.withholdPrimaryTrigger,"holding hand trigger withheld");
    Rig away;away.s.support={0.6f,0.6f,0.6f};away.s.supportGrip=0;away.Step();away.s.supportGrip=1;o=away.Step();
    Check(!o.grabbed,"no grenade away from chest");
    Rig carried;carried.s.supportGrip=1;carried.Step();carried.s.support=carried.chest;o=carried.Step();
    Check(!o.grabbed,"already-held grip carried to chest is not a grab");
    Rig busy;busy.s.supportBusy=true;o=busy.GrabSupport();
    Check(!o.grabbed,"grip claimed by reload/holster is not stolen");
    Rig primary;o=primary.GrabPrimary();
    Check(o.grabbed&&o.holding==Hand::Primary&&o.consumePrimary&&o.withholdA&&o.withholdB&&!o.withholdX,
        "weapon hand takes grenade with A/B");
    Rig left(true);o=left.GrabPrimary();
    Check(o.holding==Hand::Primary&&o.withholdX&&o.withholdY&&!o.withholdA,"left-handed weapon hand uses X/Y");
}

void SwitchTypes()
{
    Rig rig;rig.GrabSupport();
    rig.s.x=true;auto o=rig.Step();
    Check(o.switched&&o.withholdX,"X switches grenade and is hidden from Halo");
    Check(rig.NextPulse()==kSwitch||o.buttons==kSwitch,"switch pulse sent");
    rig.Step(200);rig.s.x=false;rig.Step();
    rig.s.y=true;o=rig.Step();Check(o.switched,"Y also switches");
    rig.s.y=false;rig.Step();
    rig.s.x=true;rig.Step();rig.s.x=false;rig.Step();rig.s.x=true;rig.Step();rig.s.x=false;
    unsigned pulses=0;bool gap=true,previous=false;
    for(int i=0;i<80;++i) { auto p=rig.Step(); if(p.buttons&&!previous) ++pulses; previous=p.buttons!=0; }
    (void)gap;
    Check(pulses>=2,"queued presses become separate native presses");
    rig.s.a=true;o=rig.Step();Check(!o.switched&&!o.withholdA,"A on the other hand stays normal");
    rig.s.a=false;rig.Step();
    // A button held at grab time is not a press.
    Rig held;held.s.x=true;held.Step();o=held.GrabSupport();
    o=held.Step();Check(!o.switched,"held button at grab ignored");
}

void Throwing()
{
    Rig rig;rig.GrabSupport();
    auto o=rig.Throw(true,{0,0.2f,-1},3.0f);
    Check(o.thrown&&o.holding==Hand::None,"fast forward release throws");
    Check(o.releaseVelocity.z<-1.0f,"release velocity measured");
    Check(o.buttons==kThrow||rig.NextPulse()==kThrow,"native throw pressed");
    Rig slow;slow.GrabSupport();
    o=slow.Throw(true,{0,0,-1},0.5f);
    Check(o.putBack&&!o.thrown,"slow release puts it back");
    Check(!slow.NextPulse(),"no throw after putting back");
    Rig backwards;backwards.GrabSupport();
    o=backwards.Throw(true,{0,0,1},3.0f);
    Check(!o.thrown,"backwards flick is not a throw");
    // Switch requested before the throw is sent first.
    Rig order;order.GrabSupport();
    order.s.x=true;order.Step();order.s.x=false;
    o=order.Throw(true,{0,0,-1},3.0f,8);
    uint32_t first=o.buttons?o.buttons:order.NextPulse();
    Check(first==kSwitch||first==0,"switch first");
    // The press that switched may already be done; the throw must still come.
    bool sawThrow=o.buttons==kThrow;
    for(int i=0;i<60&&!sawThrow;++i) sawThrow=order.Step().buttons==kThrow;
    Check(sawThrow,"throw follows the switch");
    // Holding the face button while throwing keeps it hidden until released.
    Rig drain;drain.GrabSupport();drain.s.y=true;drain.Step();
    o=drain.Throw(true,{0,0,-1},3.0f);
    Check(o.withholdY,"held Y stays hidden after throwing");
    drain.s.y=false;o=drain.Step();Check(!o.withholdY,"released Y is normal again");
}

void Cancelling()
{
    Rig rig;rig.GrabSupport();
    rig.s.dualWield=true;auto o=rig.Step();
    Check(o.cancelled&&o.holding==Hand::None,"dual wield cancels");
    Check(o.consumeSupport,"grip stays claimed until release");
    rig.s.supportGrip=0;o=rig.Step();o=rig.Step();Check(!o.consumeSupport,"released");
    Rig paused;paused.GrabSupport();paused.s.ready=false;o=paused.Step();
    Check(o.holding==Hand::None&&!o.withholdX,"pause cancels");
    Rig off;off.c.enabled=false;o=off.GrabSupport();Check(!o.grabbed,"disabled");
    Rig noThrow;noThrow.c.throwButton=0;o=noThrow.GrabSupport();Check(!o.grabbed,"no throw binding, no gesture");
    Rig gap;gap.GrabSupport();o=gap.Step(400);Check(o.holding==Hand::None,"tracking gap cancels");
}

void Presentation()
{
    Rig rig;
    auto p=BuildPresentation(rig.s,rig.c,rig.last,7);
    Check(p.model==&weapon_model::kGrenadeTokenModel&&Near(p.pose.position,rig.chest,1e-4f),"grenade shown at chest");
    rig.GrabSupport();
    p=BuildPresentation(rig.s,rig.c,rig.last,7);
    Check(Near(p.pose.position,rig.s.support,0.06f)&&p.identity==7,"grenade in hand");
    Check(weapon_model::UsesTokenGeometry(&weapon_model::kGrenadeTokenModel)&&
        std::size(weapon_model::kGenericVertices)>=size_t(weapon_model::kGrenadeTokenModel.firstVertex+
        weapon_model::kGrenadeTokenModel.vertexCount),"grenade token vertices");
    Rig dual;dual.s.dualWield=true;dual.Step();
    Check(!BuildPresentation(dual.s,dual.c,dual.last,7).model,"hidden while dual wielding");
}

void RightChest()
{
    // Facing -Z with an identity head, the player's right is +X.
    Rig rig;
    Check(std::fabs(rig.chest.x-0.10f)<1e-4f&&std::fabs(rig.chest.z+0.12f)<1e-4f,
        "grenade sits 10 cm toward the weapon hand: the right chest");
    Rig left(true);
    Check(std::fabs(left.chest.x+0.10f)<1e-4f,"left-handed mode mirrors it to the left chest");
    Rig turned;
    const float h=std::sqrt(0.5f);
    turned.s.headRotation={0,h,0,h}; // yaw 90 degrees left: forward -X, right -Z
    Vec chest{};Check(Chest(turned.s,turned.c,chest)&&std::fabs(chest.z+0.10f)<1e-4f&&
        std::fabs(chest.x+0.12f)<1e-4f,"right chest follows the body direction");
    Rig centre;centre.c.chestSide=0;Chest(centre.s,centre.c,chest);
    Check(std::fabs(chest.x)<1e-4f,"zero side offset is the chest centre");
    centre.c.chestSide=5;Chest(centre.s,centre.c,chest);
    Check(std::fabs(chest.x-0.30f)<1e-4f,"side offset is clamped to 30 cm");
    centre.c.chestSide=std::numeric_limits<float>::quiet_NaN();Chest(centre.s,centre.c,chest);
    Check(std::fabs(chest.x-0.10f)<1e-4f,"invalid side offset falls back to the right chest");
    Rig far;far.s.support={-0.10f,far.chest.y,far.chest.z};far.s.supportGrip=0;far.Step();
    far.s.supportGrip=1;auto o=far.Step();
    Check(!o.grabbed,"the left chest is empty for a right-handed player");
    Rig shown;
    const auto p=BuildPresentation(shown.s,shown.c,shown.last,0);
    Check(p.model==&weapon_model::kGrenadeTokenModel&&std::fabs(p.pose.position.x-0.10f)<1e-4f,
        "the chest grenade model is drawn on the right chest");
}

void FreeThrow()
{
    // Lob up and to the right: Halo's aim follows the arm before the press.
    Rig rig;rig.GrabSupport();
    const float n=std::sqrt(0.6f*0.6f+0.5f*0.5f+0.62f*0.62f);
    const Vec lob{0.6f/n,0.5f/n,-0.62f/n};
    auto o=rig.Throw(true,lob,3.0f);
    Check(o.thrown&&o.aimActive,"a throw steers the aim at once");
    Check(Near(o.aimDirection,lob,0.02f)&&std::fabs(Dot(o.aimDirection,o.aimDirection)-1)<1e-3f,
        "aim direction is the unit throw direction");
    Check(!o.buttons,"the native press waits for the aim to turn");
    const uint64_t released=rig.s.now;
    uint64_t pressed=0;
    for(int i=0;i<60&&!pressed;++i) { o=rig.Step(); if(o.buttons==kThrow) pressed=rig.s.now; Check(o.aimActive,"aim held until the press"); }
    Check(pressed&&pressed-released>=kThrowAimLeadMs&&pressed-released<=kThrowAimLeadMs+30,
        "press follows the lead time");
    bool held=true;
    while(rig.s.now<pressed+kThrowAimHoldMs-20) held=rig.Step().aimActive&&held;
    Check(held,"aim stays on the throw through Halo's throw animation");
    for(int i=0;i<10;++i) o=rig.Step();
    Check(!o.aimActive,"aim returns to the hand afterwards");
    // Off: Halo throws along the gun's aim, with no lead.
    Rig off;off.c.freeThrow=false;off.GrabSupport();
    o=off.Throw(true,{0,0,-1},3.0f);
    Check(o.thrown&&!o.aimActive&&(o.buttons==kThrow||off.Step().buttons==kThrow),"free throw off: immediate press, gun aim");
    // A slow put-back never steers.
    Rig back;back.GrabSupport();o=back.Throw(true,{0,0,-1},0.5f);
    Check(o.putBack&&!o.aimActive,"putting it back leaves the aim alone");
    // Leaving gameplay before the press drops both the throw and the aim.
    Rig cut;cut.GrabSupport();o=cut.Throw(true,{0,0,-1},3.0f);
    cut.s.ready=false;o=cut.Step();
    Check(!o.aimActive&&!o.buttons,"pause cancels the pending throw and aim");
    cut.s.ready=true;Check(!cut.NextPulse(),"no late throw after a pause");
    // Switching free throw off mid-throw returns the aim at once.
    Rig toggle;toggle.GrabSupport();toggle.Throw(true,{0,0,-1},3.0f);
    toggle.c.freeThrow=false;o=toggle.Step();Check(!o.aimActive,"turning free throw off releases the aim");
}

void RealGrenadeModels()
{
    using namespace weapon_model;
    Check(kGrenadeModelCount==3,"Halo 4 frag, plasma and pulse meshes");
    for(unsigned i=0;i<kGrenadeModelCount;++i)
    {
        const auto& m=kGrenadeModels[i];
        Check(m.title==GameTitle::Halo4&&m.vertexCount&&m.vertexCount%3==0&&UsesGrenadeGeometry(&m)&&
            !UsesTokenGeometry(&m),"real grenade record is a drawable Halo 4 mesh");
        float size=0;for(int a=0;a<3;++a) size=std::fmax(size,m.maximum[a]-m.minimum[a]);
        Check(size>0.05f&&size<0.4f,"grenade mesh is hand-sized in metres");
    }
    Check(GrenadeModel(GameTitle::Halo4,0)==&kGrenadeModels[0]&&GrenadeModel(GameTitle::Halo4,1)==&kGrenadeModels[1]&&
        GrenadeModel(GameTitle::Halo4,2)==&kGrenadeModels[2],"Halo 4 type selects its own grenade");
    Check(GrenadeModel(GameTitle::Halo4,-1)==&kGrenadeModels[0]&&GrenadeModel(GameTitle::Halo4,9)==&kGrenadeModels[0],
        "unknown Halo 4 type shows the frag");
    Check(GrenadeModel(GameTitle::Halo3,1)==&kGrenadeTokenModel,"other titles keep the token until exported");
    Rig h4;h4.s.title=GameTitle::Halo4;h4.s.grenadeType=1;h4.Step();
    const auto p=BuildPresentation(h4.s,h4.c,h4.last,0);
    Check(p.model==&kGrenadeModels[1],"Halo 4 chest shows the carried type's model");
}

int main()
{
    RealGrenadeModels();
    ChestZoneAndGrab();SwitchTypes();Throwing();Cancelling();Presentation();RightChest();FreeThrow();
    std::printf("Grenade gesture: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
