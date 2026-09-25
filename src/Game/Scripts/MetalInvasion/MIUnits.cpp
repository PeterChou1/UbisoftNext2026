#include "MIUnits.h"

#include "Assets.h"
#include "FragShaderTag.h"
#include "MIGame.h"
#include "MINames.h"
#include "RigidBody.h"
#include "World/SceneComponents.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float RAD_TO_DEG = 180.0f / PI;

    const Vec3 PLAYER_LASER = {0.3f, 0.7f, 1.0f};
    const Vec3 ENEMY_LASER = {1.0f, 0.25f, 0.2f};

    // Weapons (values of the original game)
    constexpr float SOLDIER_RANGE = 5.0f;
    constexpr float SOLDIER_RELOAD = 2.0f;
    constexpr float SOLDIER_DAMAGE = 20.0f;
    constexpr float ENEMY_RANGE = 5.0f;
    constexpr float ENEMY_RELOAD = 3.0f;
    constexpr float ENEMY_DAMAGE = 10.0f;
    constexpr float MINING_RANGE = 2.0f;
    constexpr float MINING_TIME = 2.0f;
    constexpr float TANK_RANGE = 10.0f;
    constexpr float TANK_RELOAD = 2.0f;
    constexpr float TANK_SPEED = 2.0f;
    constexpr float TURRET_TURN_SPEED = 3.0f; // radians / s
    constexpr float BULLET_SPEED = 10.0f;
    constexpr float BULLET_FUSE = 1.5f;
    constexpr float EXPLOSION_RADIUS = 1.5f;
    constexpr float EXPLOSION_DAMAGE = 35.0f;
    constexpr float EXPLOSION_TIME = 0.4f;
    // Units stop when this close to their move order
    constexpr float ARRIVE_DISTANCE = 1.0f;

    float WrapAngle(float radians)
    {
        while (radians > PI)
            radians -= 2.0f * PI;
        while (radians < -PI)
            radians += 2.0f * PI;
        return radians;
    }

    Vec3 Flat(Vec3 v)
    {
        v.Y = 0.0f;
        return v;
    }

    const char* const PLAYER_TAGS[] = {MI::Tags::Unit, MI::Tags::Wall};
    const char* const ENEMY_TAGS[] = {MI::Tags::Enemy};
} // namespace

//-----------------------------------------------------------------------------
// MIUnit
//-----------------------------------------------------------------------------

void MIUnit::OnStart()
{
    m_MaxHealth = Param("Health") > 0.0f ? Param("Health") : 100.0f;
    m_Health = m_MaxHealth;
}

void MIUnit::Damage(float amount)
{
    if (m_Dead)
        return;
    m_Health -= amount;
    if (m_Health <= 0.0f)
    {
        m_Dead = true;
        OnKilled();
    }
}

MetalInvasion* MIUnit::Game() const
{
    return SceneScriptAs<MetalInvasion>();
}

Entity MIUnit::NearestOf(MI::Side side, float range, bool includeBase)
{
    Vec3 here = Position();
    Entity best = NULL_ENTITY;
    float bestDistance = range;
    auto consider = [&](Entity e) {
        if (e == Self())
            return;
        if (auto* unit = ScriptOf<MIUnit>(e); unit != nullptr && unit->IsDead())
            return;
        float d = Flat(PositionOf(e) - here).GetMagnitude();
        if (d <= bestDistance)
        {
            best = e;
            bestDistance = d;
        }
    };
    if (side == MI::Side::Player)
    {
        for (const char* tag : PLAYER_TAGS)
            for (Entity e : FindByTag(tag))
                consider(e);
        if (includeBase)
            for (Entity e : FindByTag(MI::Tags::Base))
                consider(e);
    }
    else if (side == MI::Side::Enemy)
    {
        for (const char* tag : ENEMY_TAGS)
            for (Entity e : FindByTag(tag))
                consider(e);
    }
    return best;
}

void MIUnit::Hit(Entity target, float amount)
{
    if (auto* unit = ScriptOf<MIUnit>(target))
        unit->Damage(amount);
}

void MIUnit::FireLaser(Entity target, const Vec3& color)
{
    MI::SpawnLaser(Position(), PositionOf(target), color);
}

void MIUnit::Walk(const Vec3& direction, float speed)
{
    SetVelocity(Vec2(direction.X, direction.Z) * speed);
    if (direction.GetMagnitude() > 0.0f)
        SetYaw(std::atan2(direction.X, direction.Z) * RAD_TO_DEG);
}

void MIUnit::Halt()
{
    SetVelocity(Vec2(0.0f, 0.0f));
}

float MIUnit::DistanceTo(Entity other) const
{
    return Flat(PositionOf(other) - Position()).GetMagnitude();
}

//-----------------------------------------------------------------------------
// MITurret
//-----------------------------------------------------------------------------

void MITurret::Attach(Entity hull, const std::string& hullName)
{
    // A saved game may already hold the cannon
    std::string name = hullName + " cannon";
    m_Cannon = SceneObjects::FindByName(name);
    if (m_Cannon == NULL_ENTITY)
    {
        m_Cannon = SceneObjects::CreateModel(name, MI::Models::TankCannon, SceneObjects::GetPosition(hull),
                                             SceneObjects::GetYaw(hull), MI::TANK_SCALE);
        ECS.GetComponent<SceneObject>(m_Cannon).Tag = MI::Tags::Turret;
    }
    Follow(hull);
}

void MITurret::Follow(Entity hull)
{
    if (m_Cannon == NULL_ENTITY || !ECS.IsEntityAlive(m_Cannon))
        return;
    Vec3 p = SceneObjects::GetPosition(hull);
    SceneObjects::SetPosition(m_Cannon, Vec3(p.X, 0.3f * MI::TANK_SCALE, p.Z));
}

bool MITurret::Aim(const Vec3& target, float deltaSeconds)
{
    if (m_Cannon == NULL_ENTITY || !ECS.IsEntityAlive(m_Cannon))
        return false;
    Vec3 to = Flat(target - SceneObjects::GetPosition(m_Cannon));
    float current = SceneObjects::GetYaw(m_Cannon) / RAD_TO_DEG;
    float desired = std::atan2(to.X, to.Z);
    float diff = WrapAngle(desired - current);
    float step = TURRET_TURN_SPEED * deltaSeconds;
    if (std::fabs(diff) <= 0.1f)
        return true;
    SceneObjects::SetYaw(m_Cannon, (current + std::clamp(diff, -step, step)) * RAD_TO_DEG);
    return false;
}

void MITurret::Remove()
{
    if (m_Cannon != NULL_ENTITY && ECS.IsEntityAlive(m_Cannon))
        SceneObjects::Destroy(m_Cannon);
    m_Cannon = NULL_ENTITY;
}

//-----------------------------------------------------------------------------
// Player units
//-----------------------------------------------------------------------------

void MIPlayerUnit::OnStart()
{
    MIUnit::OnStart();
    m_Battalion = static_cast<int>(Param("Battalion"));
}

void MIPlayerUnit::OnUpdate(float deltaSeconds)
{
    UpdateHighlight();
    FollowOrders();
    Act(deltaSeconds);
}

void MIPlayerUnit::FollowOrders()
{
    MetalInvasion* game = Game();
    // Like the original: selected units walk to the last order
    if (game == nullptr || !m_Selected || !game->HasMoveOrder())
    {
        Halt();
        return;
    }
    MI::FlowField& field = game->UnitField();
    if (Flat(field.Goal() - Position()).GetMagnitude() <= ARRIVE_DISTANCE)
    {
        Halt();
        return;
    }
    Walk(field.Direction(Position()), Speed());
}

void MIPlayerUnit::UpdateHighlight()
{
    // Selected units are drawn with the engine's red shader
    if (!Has<FragShaderTag>(Self()))
        return;
    FragShaderTag& shader = Get<FragShaderTag>(Self());
    FragShaderTypeID wanted = m_Selected ? RedShaderID : BlinnPhongID;
    if (shader.FragAssetId != wanted)
        shader.FragAssetId = wanted;
}

void MISoldier::Act(float deltaSeconds)
{
    m_Reload = std::max(0.0f, m_Reload - deltaSeconds);
    if (m_Reload > 0.0f)
        return;
    Entity target = NearestOf(MI::Side::Enemy, SOLDIER_RANGE);
    if (target == NULL_ENTITY)
        return;
    m_Reload = SOLDIER_RELOAD;
    FireLaser(target, PLAYER_LASER);
    Hit(target, SOLDIER_DAMAGE);
}

void MISupport::Act(float deltaSeconds)
{
    Entity nearest = NULL_ENTITY;
    float best = MINING_RANGE;
    for (Entity crystal : FindByTag(MI::Tags::Crystal))
    {
        float d = DistanceTo(crystal);
        if (d <= best)
        {
            best = d;
            nearest = crystal;
        }
    }
    if (nearest == NULL_ENTITY)
    {
        m_Mining = MINING_TIME;
        return;
    }
    m_Mining -= deltaSeconds;
    if (m_Mining > 0.0f)
        return;
    m_Mining = MINING_TIME;
    auto* crystal = ScriptOf<MICrystal>(nearest);
    MetalInvasion* game = Game();
    if (crystal != nullptr && game != nullptr)
        game->AddCrystals(crystal->Mine(1));
}

void MITank::OnStart()
{
    MIPlayerUnit::OnStart();
    m_Turret.Attach(Self(), NameOf(Self()));
}

void MITank::Act(float deltaSeconds)
{
    m_Turret.Follow(Self());
    m_Turret.Cooldown = std::max(0.0f, m_Turret.Cooldown - deltaSeconds);
    Entity target = NearestOf(MI::Side::Enemy, TANK_RANGE);
    if (target == NULL_ENTITY)
        return;
    if (m_Turret.Aim(PositionOf(target), deltaSeconds) && m_Turret.Cooldown <= 0.0f)
    {
        m_Turret.Cooldown = TANK_RELOAD;
        Entity cannon = m_Turret.Cannon();
        MI::SpawnBullet(PositionOf(cannon), SceneObjects::GetYaw(cannon), MI::Side::Player);
    }
}

void MITank::OnKilled()
{
    MI::SpawnExplosion(Position(), MI::Side::Neutral);
    m_Turret.Remove();
    DestroySelf();
}

void MIBase::OnKilled()
{
    // The base stays (ruined) and the game ends
    if (MetalInvasion* game = Game())
        game->OnBaseDestroyed();
}

void MIWall::OnKilled()
{
    if (MetalInvasion* game = Game())
        game->OnObstacleRemoved(Self());
    DestroySelf();
}

//-----------------------------------------------------------------------------
// Enemies
//-----------------------------------------------------------------------------

void MIEnemySoldier::OnUpdate(float deltaSeconds)
{
    m_Reload = std::max(0.0f, m_Reload - deltaSeconds);
    // The base first, then any player unit or wall in range (original rule)
    Entity target = NULL_ENTITY;
    for (Entity base : FindByTag(MI::Tags::Base))
    {
        if (DistanceTo(base) <= ENEMY_RANGE)
            target = base;
    }
    if (target == NULL_ENTITY)
        target = NearestOf(MI::Side::Player, ENEMY_RANGE, false);

    if (target != NULL_ENTITY)
    {
        Halt();
        if (m_Reload <= 0.0f)
        {
            m_Reload = ENEMY_RELOAD;
            FireLaser(target, ENEMY_LASER);
            Hit(target, ENEMY_DAMAGE);
        }
        return;
    }
    MetalInvasion* game = Game();
    if (game == nullptr)
        return;
    Walk(game->EnemyField().Direction(Position()), Param("Speed"));
}

void MIEnemyTank::OnStart()
{
    MIUnit::OnStart();
    m_Turret.Attach(Self(), NameOf(Self()));
}

void MIEnemyTank::OnUpdate(float deltaSeconds)
{
    m_Turret.Follow(Self());
    m_Turret.Cooldown = std::max(0.0f, m_Turret.Cooldown - deltaSeconds);
    Entity target = NearestOf(MI::Side::Player, TANK_RANGE);
    if (target == NULL_ENTITY)
    {
        // Nothing in range: drive towards the base
        if (MetalInvasion* game = Game())
            Walk(game->EnemyField().Direction(Position()), TANK_SPEED);
        return;
    }
    Halt();
    if (m_Turret.Aim(PositionOf(target), deltaSeconds) && m_Turret.Cooldown <= 0.0f)
    {
        m_Turret.Cooldown = TANK_RELOAD;
        Entity cannon = m_Turret.Cannon();
        MI::SpawnBullet(PositionOf(cannon), SceneObjects::GetYaw(cannon), MI::Side::Enemy);
    }
}

void MIEnemyTank::OnKilled()
{
    MI::SpawnExplosion(Position(), MI::Side::Neutral);
    m_Turret.Remove();
    DestroySelf();
}

//-----------------------------------------------------------------------------
// World / effects
//-----------------------------------------------------------------------------

void MICrystal::OnStart()
{
    m_Amount = static_cast<int>(Param("Amount"));
}

int MICrystal::Mine(int amount)
{
    int mined = std::min(amount, m_Amount);
    m_Amount -= mined;
    if (m_Amount <= 0)
    {
        if (auto* game = SceneScriptAs<MetalInvasion>())
            game->OnObstacleRemoved(Self());
        DestroySelf();
    }
    return mined;
}

MI::Side MIBullet::Side() const
{
    return Param("Enemy") > 0.5f ? MI::Side::Enemy : MI::Side::Player;
}

void MIBullet::OnUpdate(float deltaSeconds)
{
    m_Age += deltaSeconds;
    if (m_Age >= BULLET_FUSE)
    {
        Explode();
        return;
    }
    float yaw = Yaw() / RAD_TO_DEG;
    SetPosition(Position() + Vec3(std::sin(yaw), 0.0f, std::cos(yaw)) * (BULLET_SPEED * deltaSeconds));
}

void MIBullet::OnCollisionEnter(Entity other)
{
    MI::Side side = MI::SideOf(other);
    // Explodes on the other side's objects only
    if (side != MI::Side::Neutral && side != Side())
        Explode();
}

void MIBullet::Explode()
{
    if (m_Exploded)
        return;
    m_Exploded = true;
    MI::SpawnExplosion(Position(), Side());
    DestroySelf();
}

void MIExplosion::OnStart()
{
    // Damage everything of the other side in the blast, once. A destroyed
    // tank's explosion (Enemy < 0) is only visual
    if (Param("Enemy") < -0.5f)
        return;
    MI::Side attacker = Param("Enemy") > 0.5f ? MI::Side::Enemy : MI::Side::Player;
    const char* const playerTags[] = {MI::Tags::Unit, MI::Tags::Wall, MI::Tags::Base};
    const char* const enemyTags[] = {MI::Tags::Enemy};
    auto blast = [&](const char* tag) {
        for (Entity e : FindByTag(tag))
        {
            if (Flat(PositionOf(e) - Position()).GetMagnitude() <= EXPLOSION_RADIUS)
            {
                if (auto* unit = ScriptOf<MIUnit>(e))
                    unit->Damage(EXPLOSION_DAMAGE);
            }
        }
    };
    if (attacker == MI::Side::Enemy)
        for (const char* tag : playerTags)
            blast(tag);
    else
        for (const char* tag : enemyTags)
            blast(tag);
}

void MIExplosion::OnUpdate(float deltaSeconds)
{
    m_Age += deltaSeconds;
    if (m_Age >= EXPLOSION_TIME)
    {
        DestroySelf();
        return;
    }
    // Grow and spin like the original explosion ball
    GetTransform().Scale(1.0f + 2.0f * deltaSeconds);
    SetYaw(Yaw() + 180.0f * deltaSeconds);
}
