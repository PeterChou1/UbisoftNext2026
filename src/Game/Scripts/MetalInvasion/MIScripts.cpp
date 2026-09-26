#include "MIScripts.h"

#include "MIGame.h"
#include "MINames.h"
#include "MIUnits.h"
#include "Scripting/ScriptRegistry.h"

void RegisterMetalInvasionScripts(ScriptRegistry& r)
{
    using namespace MI::Scripts;
    r.Register<MetalInvasion>(Game,
                              "Metal Invasion: rounds, crystals, shop, orders, HUD",
                              {{"Crystals", 25.0f, 5.0f},
                               {"PrepTime", 60.0f, 5.0f},
                               {"InvasionTime", 20.0f, 5.0f},
                               {"SpawnInterval", 15.0f, 1.0f},
                               {"SpawnVolume", 1.0f, 1.0f},
                               {"CrystalCount", 10.0f, 1.0f},
                               {"EnemyHealth", 100.0f, 10.0f},
                               {"Seed", 7.0f, 1.0f}});
    r.Register<MIBase>(Base, "Metal Invasion base: game over when destroyed", {{"Health", 1000.0f, 50.0f}});
    r.Register<MICrystal>(Crystal, "Crystal deposit mined by support units", {{"Amount", 30.0f, 5.0f}});
    r.Register<MIWall>(Wall, "Player wall, blocks paths", {{"Health", 250.0f, 25.0f}});
    r.Register<MISoldier>(Soldier, "Player soldier: lasers enemies in range",
                          {{"Health", 100.0f, 10.0f}, {"Battalion", 0.0f, 1.0f}});
    r.Register<MISupport>(Support, "Player support: mines crystals in range",
                          {{"Health", 100.0f, 10.0f}, {"Battalion", 0.0f, 1.0f}});
    r.Register<MITank>(Tank, "Player tank: turret fires explosive bullets",
                       {{"Health", 200.0f, 10.0f}, {"Battalion", 0.0f, 1.0f}});
    r.Register<MIEnemySoldier>(EnemySoldier, "Enemy soldier: walks to the base, shoots what is in range",
                               {{"Health", 100.0f, 10.0f}, {"Speed", 1.2f, 0.1f}});
    r.Register<MIEnemyTank>(EnemyTank, "Enemy tank: walks to the base, turret fires bullets",
                            {{"Health", 200.0f, 10.0f}});
    r.Register<MIBullet>(Bullet, "Tank bullet (Enemy = 1 for enemy bullets)", {{"Enemy", 0.0f, 1.0f}});
    r.Register<MIExplosion>(Explosion, "Bullet explosion (Enemy: 1 hurts the player, 0 hurts enemies, -1 visual)", {{"Enemy", 0.0f, 1.0f}});
}
