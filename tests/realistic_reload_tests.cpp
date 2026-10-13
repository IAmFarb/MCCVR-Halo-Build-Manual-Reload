#include "../src/common/realistic_reload_logic.h"
#include "../src/common/weapon_generic_magazine.h"
#include <iterator>
#include <cstdio>
#include <cstring>
#include <limits>

using namespace realistic_reload;
using weapon_interaction::Zones;
unsigned checks{},failures{};
void Check(bool ok,const char* message)
{ ++checks;if(!ok){++failures;std::printf("FAIL: %s\n",message);} }

const weapon_model::Model* Named(GameTitle title,const char* name)
{
    for(const auto& m:weapon_model::kModels) if(m.title==title&&!std::strcmp(m.name,name)) return &m;
    return nullptr;
}
uint64_t Graph(GameTitle title,const char* name)
{ const auto* m=Named(title,name);return m?m->identity:0; }

struct Rig
{
    State state;
    weapon_interaction::Settings c;
    Settings r;
    Sample in;
    Vec pouch{},holster{};
    Output last{};
    Rig(GameTitle title=GameTitle::Halo3,const char* weapon="fp_assault_rifle")
    {
        c.reload=true;c.reloadButton=0x4000;r.enabled=true;
        auto& s=in.base;
        s.title=title;s.generation=3;s.space=7;s.ready=true;s.now=1000;
        s.weaponGraph=Graph(title,weapon);
        s.head={0,1.65f,0};s.primary={0.22f,1.25f,-0.45f};s.support={-0.25f,1.20f,-0.40f};
        s.headRotation=s.primaryRotation=s.supportRotation={0,0,0,1};
        s.primaryGrip=0;s.supportGrip=0;
        in.reloadSourceSupported=true;
        Zones(s,c,pouch,holster);
        Step();Step();
    }
    Output Step(unsigned delta=20) { in.base.now+=delta;last=state.Update(in,c,r);return last; }
    Output TapReload()
    {
        in.reloadSourceHeld=true;Step();Step();
        in.reloadSourceHeld=false;return Step();
    }
    Output Grab()
    {
        in.base.support=pouch;in.base.supportGrip=0;Step();
        in.base.supportGrip=1;return Step();
    }
    Output MoveSupport(Vec target,unsigned steps=6)
    {
        const Vec start=in.base.support;Output o{};
        for(unsigned i=1;i<=steps;++i)
        {
            in.base.support=start+(target-start)*(float(i)/steps);
            o=Step(30);
            if(o.inserted||o.reloadRequested||o.finished) return o;
        }
        return o;
    }
    Vec Port() const { return state.PortPoint(in.base); }
    // Move so the held part's centre (not the hand) reaches the port.
    Vec HandForPort() const
    { return Port()-weapon_interaction::Rotate(in.base.supportRotation,{0,-.015f,-.035f}); }
    Output Release() { in.base.supportGrip=0;return Step(); }
    Output GrabHandle()
    {
        in.base.support=state.HandlePoint(in.base);in.base.supportGrip=0;Step();
        in.base.supportGrip=1;return Step();
    }
};

void Recipes()
{
    auto r=Lookup(GameTitle::Halo3,Named(GameTitle::Halo3,"fp_assault_rifle"));
    Check(r.part==Part::Magazine&&r.finish==Finish::PullBack&&r.handle==Handle::TopRail,"H3 AR magazine + handle");
    r=Lookup(GameTitle::Halo3,Named(GameTitle::Halo3,"fp_shotgun"));
    Check(r.part==Part::Shell&&r.finish==Finish::Pump,"H3 shotgun shells + pump");
    r=Lookup(GameTitle::Halo2,Named(GameTitle::Halo2,"fp_shotgun"));
    Check(r.part==Part::Shell&&r.finish==Finish::None,"H2 shotgun has no pump step");
    r=Lookup(GameTitle::Halo2,Named(GameTitle::Halo2,"fp_sniper_rifle"));
    Check(r.part==Part::Magazine&&r.finish==Finish::None,"H2 sniper has no charging step");
    r=Lookup(GameTitle::Halo3,Named(GameTitle::Halo3,"fp_sniper_rifle"));
    Check(r.finish==Finish::PullBack,"H3 sniper charging step");
    r=Lookup(GameTitle::HaloCE,Named(GameTitle::HaloCE,"pistol"));
    Check(r.part==Part::Magazine&&r.port==Port::Grip&&r.handle==Handle::Slide,"CE pistol grip + slide");
    r=Lookup(GameTitle::Halo4,Named(GameTitle::Halo4,"storm_magnum"));
    Check(r.port==Port::Grip&&r.handle==Handle::Slide,"H4 magnum");
    r=Lookup(GameTitle::Halo3,Named(GameTitle::Halo3,"fp_plasma_rifle"));
    Check(r.part==Part::None,"plasma rifle has no manual reload");
    r=Lookup(GameTitle::Halo3,Named(GameTitle::Halo3,"fp_rocket_launcher"));
    Check(r.part==Part::Pod&&r.finish==Finish::Push&&r.port==Port::Rear,"rocket pod + push bar");
    r=Lookup(GameTitle::Halo4,Named(GameTitle::Halo4,"storm_forerunner_rifle"));
    Check(r.part==Part::Cartridge,"Forerunner cartridge");
    r=Lookup(GameTitle::Halo3,nullptr);
    Check(r.part==Part::Magazine&&r.finish==Finish::None,"unknown weapon plain magazine");
    weapon_model::Model custom{GameTitle::Halo3,1,"my_custom_gun",false,0,0,0,{},{}};
    r=Lookup(GameTitle::Halo3,&custom);
    Check(r.part==Part::Magazine&&r.finish==Finish::None,"custom weapon plain magazine");
    // Every catalogued model must have a deliberate recipe.
    for(const auto& m:weapon_model::kModels)
    {
        char name[64];Normalize(m.name,name,sizeof name);
        bool found=false;for(const auto& e:kRecipes) found=found||!std::strcmp(e.name,name);
        if(!found) std::printf("missing recipe: %s\n",m.name);
        Check(found,"catalogue model has a recipe");
    }
    char name[16];Normalize("fp_battle_rifle_long_name_that_is_cut",name,sizeof name);
    Check(std::strlen(name)==15,"normalize bounded");
}

void MagazineFlow()
{
    Rig rig;
    Check(rig.state.CurrentPhase()==Phase::Idle,"starts loaded");
    rig.in.reloadSourceHeld=true;
    auto o=rig.Step();
    Check(o.withholdReload,"reload press withheld from the game");
    o=rig.Step();Check(o.withholdReload&&!o.ejected,"still pressed, not ejected yet");
    rig.in.reloadSourceHeld=false;o=rig.Step();
    Check(o.ejected&&o.phase==Phase::Ejected&&o.primaryHaptic>0,"tap ejects");
    Check(weapon_interaction::Finite(o.ejectAt),"eject point");
    Check(o.blockFire,"fire blocked while magazine is out");
    Check(!o.buttons,"no native reload yet");
    // A second tap while ejected does nothing.
    o=rig.TapReload();Check(o.phase==Phase::Ejected&&!o.ejected,"second tap ignored");
    // Carrying an already-held grip into the pouch is not a grab.
    rig.in.base.supportGrip=1;rig.Step();
    rig.in.base.support=rig.pouch;o=rig.Step();
    Check(!o.picked,"held grip carried into pouch is not a grab");
    rig.Release();
    o=rig.Grab();
    Check(o.picked&&o.holding&&o.consumeSupport&&o.releaseTwoHand,"grab from hip");
    o=rig.MoveSupport(rig.HandForPort());
    Check(o.inserted&&!o.reloadRequested,"inserted, waiting for the charging handle");
    Check(o.phase==Phase::Inserted&&o.blockFire,"still blocked until racked");
    o=rig.Release();Check(!o.consumeSupport||o.phase==Phase::Inserted,"release after insert");
    o=rig.GrabHandle();Check(o.phase==Phase::Finishing&&o.consumeSupport,"grab the charging handle");
    const Vec start=rig.in.base.support;
    o=rig.MoveSupport(start+Vec{0,0,0.03f});
    Check(!o.reloadRequested,"short pull is not enough");
    o=rig.MoveSupport(start+Vec{0,0,0.08f});
    Check(o.reloadRequested&&o.finished&&o.buttons==0x4000,"full pull requests native reload");
    Check(o.phase==Phase::Idle&&!o.blockFire,"loaded again");
    o=rig.Step(60);Check(o.buttons==0x4000,"pulse held briefly");
    o=rig.Step(100);Check(!o.buttons,"pulse expires");
}

void BodyMotionIsNotAStroke()
{
    Rig rig;rig.TapReload();rig.Grab();rig.MoveSupport(rig.HandForPort());rig.Release();
    rig.GrabHandle();
    // Both hands move back together: the hand did not move along the gun.
    for(int i=0;i<6;++i)
    {
        rig.in.base.primary=rig.in.base.primary+Vec{0,0,0.02f};
        rig.in.base.support=rig.in.base.support+Vec{0,0,0.02f};
        auto o=rig.Step(30);
        Check(!o.reloadRequested,"walking backwards is not a pull");
    }
    // Rotated weapon: pulling along the gun still counts.
    Rig turned;turned.TapReload();turned.Grab();turned.MoveSupport(turned.HandForPort());turned.Release();
    const float half=0.70710678f;
    turned.in.base.primaryRotation={0,half,0,half}; // gun points to world -X
    turned.Step();
    auto o=turned.GrabHandle();
    Check(o.phase==Phase::Finishing,"grab handle on rotated gun");
    const Vec back=weapon_interaction::Rotate(turned.in.base.primaryRotation,{0,0,0.08f});
    o=turned.MoveSupport(turned.in.base.support+back);
    Check(o.reloadRequested,"pull along the rotated gun");
}

void HoldPassesThrough()
{
    Rig rig;
    rig.in.reloadSourceHeld=true;
    Output o{};
    for(int i=0;i<15;++i) o=rig.Step(20);
    Check(o.withholdReload,"held under threshold withheld");
    for(int i=0;i<10;++i) o=rig.Step(20);
    Check(!o.withholdReload,"long hold passes through for pick up / native reload");
    rig.in.reloadSourceHeld=false;o=rig.Step();
    Check(!o.ejected&&o.phase==Phase::Idle,"long hold does not eject");
    // A long hold while the magazine is out cancels the manual reload.
    rig.TapReload();Check(rig.last.phase==Phase::Ejected,"ejected again");
    rig.in.reloadSourceHeld=true;
    for(int i=0;i<25;++i) o=rig.Step(20);
    Check(o.cancelled||rig.state.CurrentPhase()==Phase::Idle,"long hold cancels");
    Check(!o.blockFire,"fire released after cancel");
}

void NoReloadWeapon()
{
    Rig rig(GameTitle::Halo3,"fp_plasma_rifle");
    rig.in.reloadSourceHeld=true;auto o=rig.Step();
    Check(!o.withholdReload,"plasma rifle reload passes through");
    rig.in.reloadSourceHeld=false;o=rig.Step();
    Check(!o.ejected&&!o.blockFire,"no eject for plasma");
}

void ShotgunShells()
{
    Rig rig(GameTitle::Halo3,"fp_shotgun");
    auto o=rig.TapReload();Check(o.phase==Phase::Ejected,"shotgun ready to load");
    for(unsigned shell=1;shell<=3;++shell)
    {
        rig.Release();
        o=rig.Grab();Check(o.picked,"take a shell");
        o=rig.MoveSupport(rig.HandForPort());
        Check(o.inserted&&o.shells==shell&&!o.reloadRequested,"shell loaded, no pump yet");
    }
    rig.Release();
    o=rig.GrabHandle();Check(o.phase==Phase::Finishing,"grab pump");
    const Vec start=rig.in.base.support;
    o=rig.MoveSupport(start+Vec{0,0,0.08f});
    Check(!o.reloadRequested&&o.phase==Phase::Finishing,"pump back alone is not enough");
    o=rig.MoveSupport(start+Vec{0,0,0.0f});
    Check(o.reloadRequested,"pump back and forward requests reload");
    Rig h2(GameTitle::Halo2,"fp_shotgun");
    h2.TapReload();h2.Grab();o=h2.MoveSupport(h2.HandForPort());
    Check(o.inserted&&o.reloadRequested,"Halo 2 shotgun reloads on first shell");
}

void DroppingAndCancelling()
{
    Rig rig;rig.TapReload();rig.Grab();
    rig.MoveSupport(rig.pouch+Vec{0.3f,0.1f,0});
    auto o=rig.Release();
    Check(o.droppedHeld&&o.phase==Phase::Ejected,"letting go away from the gun drops it");
    o=rig.Grab();Check(o.picked,"take another");
    // Switching weapon cancels and frees the trigger.
    rig.in.base.weaponGraph=Graph(GameTitle::Halo3,"fp_battle_rifle");
    o=rig.Step();
    Check(o.cancelled&&o.phase==Phase::Idle&&!o.blockFire,"weapon change cancels");
    Check(o.consumeSupport,"held grip stays consumed through release");
    // Gameplay not ready: nothing is withheld.
    Rig paused;paused.in.base.ready=false;paused.in.reloadSourceHeld=true;
    o=paused.Step();Check(!o.withholdReload,"paused reload passes through");
    paused.in.base.ready=true;o=paused.Step();
    Check(!o.withholdReload,"press carried out of pause stays native");
    paused.in.reloadSourceHeld=false;o=paused.Step();
    Check(!o.ejected,"no delayed eject after pause");
    // Unsupported reload source (e.g. bound to a trigger) disables the flow.
    Rig unsupported;unsupported.in.reloadSourceSupported=false;unsupported.in.reloadSourceHeld=true;
    o=unsupported.Step();Check(!o.withholdReload,"unsupported source passes through");
    // Dual wield keeps native controls.
    Rig dual;dual.in.base.dualWield=true;dual.in.reloadSourceHeld=true;
    o=dual.Step();Check(!o.withholdReload,"dual wield passes through");
    // Without the finish step the insert reloads.
    Rig quick;quick.r.finishStep=false;quick.Step();quick.TapReload();quick.Grab();
    o=quick.MoveSupport(quick.HandForPort());
    Check(o.inserted&&o.reloadRequested,"finish step disabled reloads on insert");
    // Fire block can be turned off.
    Rig openFire;openFire.r.blockFire=false;o=openFire.TapReload();
    Check(o.phase==Phase::Ejected&&!o.blockFire,"fire block optional");
    // Tracking gap cancels.
    Rig gap;gap.TapReload();o=gap.Step(500);
    Check(o.phase==Phase::Idle,"tracking gap cancels");
}

void Drops()
{
    DropPool pool;
    const auto* mag=Named(GameTitle::Halo3,"fp_assault_rifle");
    pool.Spawn(mag,{0,1.2f,-0.4f},{0,0,0,1},{0,1.65f,0},1.7f,1000);
    Check(pool.Active()==1,"spawned");
    pool.Spawn(nullptr,{0,1,0},{0,0,0,1},{0,1.65f,0},1.7f,1000);
    Check(pool.Active()==1,"null model ignored");
    uint64_t t=1000;bool landed=false;float lastY=1.2f;bool falling=true;
    for(int i=0;i<200&&!landed;++i)
    {
        t+=11;pool.Step(t,4.0f);
        const auto& d=pool.Item(0);
        falling=falling&&d.position.y<=lastY+1e-6f;lastY=d.position.y;
        landed=d.landed;
        Check(weapon_interaction::Normal(d.orientation),"orientation stays normalized");
    }
    Check(falling,"falls monotonically");
    Check(landed&&std::fabs(pool.Item(0).position.y-(1.65f-1.7f))<1e-4f,"lands at floor estimate");
    pool.Step(t+3900,4.0f);Check(pool.Active()==1,"lingers");
    pool.Step(t+4100,4.0f);Check(pool.Active()==0,"then disappears");
    for(int i=0;i<5;++i) pool.Spawn(mag,{0,1.2f,0},{0,0,0,1},{0,1.65f,0},1.7f,2000+i);
    Check(pool.Active()==DropPool::kCapacity,"bounded pool");
    const auto* shell=PartModel(GameTitle::Halo3,Graph(GameTitle::Halo3,"fp_shotgun"),Part::Shell,true);
    Check(shell==&weapon_model::kShellReloadModel&&weapon_model::UsesTokenGeometry(shell),"shell token");
    Check(!EjectModel(GameTitle::Halo3,Graph(GameTitle::Halo3,"fp_shotgun"),Part::Shell),"shells leave no floor copy");
    Check(EjectModel(GameTitle::Halo3,mag->identity,Part::Magazine)==mag,"magazine falls");
    Check(std::size(weapon_model::kGenericVertices)>=size_t(weapon_model::kShellReloadModel.firstVertex+
        weapon_model::kShellReloadModel.vertexCount),"shell vertices present");
}

void Presentation()
{
    Rig rig;
    auto p=BuildPresentation(rig.in.base,rig.c,rig.state,rig.last,{0,0,0,1});
    Check(p.model==Named(GameTitle::Halo3,"fp_assault_rifle")&&weapon_interaction::Near(p.pose.position,rig.pouch,1e-4f),
        "reserve magazine at the hip");
    rig.TapReload();rig.Grab();
    p=BuildPresentation(rig.in.base,rig.c,rig.state,rig.last,{0,0,0,1});
    Check(weapon_interaction::Near(p.pose.position,rig.in.base.support,0.05f),"held magazine in hand");
    Rig plasma(GameTitle::Halo3,"fp_plasma_rifle");
    p=BuildPresentation(plasma.in.base,plasma.c,plasma.state,plasma.last,{0,0,0,1});
    Check(!p.model,"no reserve for plasma");
}

int main()
{
    Recipes();MagazineFlow();BodyMotionIsNotAStroke();HoldPassesThrough();NoReloadWeapon();
    ShotgunShells();DroppingAndCancelling();Drops();Presentation();
    std::printf("Realistic reload: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
