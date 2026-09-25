#include "MIGame.h"

#include "Assets.h"
#include "Camera.h"
#include "FragShaderTag.h"
#include "MINames.h"
#include "MIUnits.h"
#include "Map.h"
#include "UIState.h"
#include "Widget.h"
#include "World/SceneComponents.h"

#include <algorithm>
#include <cmath>

namespace
{

    // Crystal deposits appear in a ring around the base
    constexpr float CRYSTAL_RING_MIN = 10.0f;
    constexpr float CRYSTAL_RING_MAX = 20.0f;
    constexpr int CRYSTAL_AMOUNT = 30;
    // Enemies arrive on a circle around the base
    constexpr float ENEMY_SPAWN_DISTANCE = 24.0f;
    constexpr float ENEMY_SPEED_MIN = 1.0f;
    constexpr float ENEMY_SPEED_MAX = 1.5f;
    constexpr float ENEMY_TANK_HEALTH = 200.0f;
    constexpr float ENEMY_TO_TANK_RATIO = 0.7f;
    constexpr int BATTALION_SIZE = 5;
    // Purchased units appear here (in front of the base)
    const Vec3 RALLY_POINT = {0.0f, 0.0f, -3.5f};

    constexpr float CAMERA_PAN_SPEED = 18.0f;
    constexpr float CAMERA_LIMIT = 25.0f;
    constexpr float PICK_MARGIN = 0.25f;

    const Vec3 WALL_OK = {0.6f, 0.6f, 0.65f};
    const Vec3 WALL_BLOCKED = {0.9f, 0.2f, 0.2f};

    std::string Seconds(float s) { return std::to_string(static_cast<int>(std::ceil(std::max(s, 0.0f)))); }
} // namespace

//-----------------------------------------------------------------------------
// Lifetime
//-----------------------------------------------------------------------------

void MetalInvasion::OnStart()
{
    m_Crystals = static_cast<int>(Param("Crystals"));
    m_SpawnVolume = std::max(1, static_cast<int>(Param("SpawnVolume")));
    m_InvasionTime = Param("InvasionTime");
    m_Random.seed(static_cast<unsigned>(Param("Seed")));

    auto settings = Resource<SceneSettings>();
    m_View = SceneCamera::Current();

    // The authored scene holds the base; it blocks the path finding grid
    std::vector<Entity> bases = FindByTag(MI::Tags::Base);
    m_Base = bases.empty() ? NULL_ENTITY : bases.front();
    if (m_Base != NULL_ENTITY && !Has<AIObstacle>(m_Base))
    {
        float half = MI::BASE_SCALE * 0.5f + 0.3f;
        ECS.AddComponent<AIObstacle>(m_Base, {half, half});
    }

    // Battalion ids continue after the units already in the scene
    for (Entity unit : FindByTag(MI::Tags::Unit))
    {
        if (Has<ScriptComponent>(unit))
        {
            auto& params = Get<ScriptComponent>(unit).Params;
            auto it = params.find("Battalion");
            if (it != params.end())
                m_NextBattalion = std::max(m_NextBattalion, static_cast<int>(it->second) + 1);
        }
    }

    // Two path finding fields over the whole field: enemies go to the base,
    // units go to the last move order
    float halfWidth = settings->FieldWidth * 0.5f;
    float halfHeight = settings->FieldHeight * 0.5f;
    m_EnemyField.Build(halfWidth, halfHeight);
    m_UnitField.Build(halfWidth, halfHeight);
    m_EnemyField.SetGoal(m_Base != NULL_ENTITY ? PositionOf(m_Base) : Vec3(0, 0, 0));
}

void MetalInvasion::OnUpdate(float deltaSeconds)
{
    if (m_Phase == Phase::GameOver)
    {
        if (KeyPressed(App::KEY_ENTER))
            RestartScene();
        return;
    }
    UpdateCamera(deltaSeconds);
    UpdateRound(deltaSeconds);
    if (m_Phase == Phase::Preparation && KeyPressed(App::KEY_ENTER))
        SkipPreparation();
    HandleMouse();
}

float MetalInvasion::PhaseTimeLeft() const
{
    if (m_Phase == Phase::Preparation)
        return m_PrepLeft;
    if (m_Phase == Phase::Invasion)
        return m_InvasionLeft;
    return 0.0f;
}

float MetalInvasion::BaseHealth()
{
    auto* base = ScriptOf<MIBase>(m_Base);
    return base != nullptr ? base->Health() : 0.0f;
}

float MetalInvasion::Random01()
{
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(m_Random);
}

//-----------------------------------------------------------------------------
// Rounds (GameRoundControllerSystem + EnemyControllerSystem)
//-----------------------------------------------------------------------------

void MetalInvasion::UpdateRound(float deltaSeconds)
{
    switch (m_Phase)
    {
    case Phase::Spawn:
        SpawnCrystals();
        m_PrepLeft = Param("PrepTime");
        m_InvasionLeft = m_InvasionTime;
        // Every round the invasion lasts one spawn interval longer
        m_InvasionTime += Param("SpawnInterval");
        m_SpawnTimer = 0.0f;
        m_Phase = Phase::Preparation;
        break;
    case Phase::Preparation:
        m_PrepLeft -= deltaSeconds;
        if (m_PrepLeft <= 0.0f)
            m_Phase = Phase::Invasion;
        break;
    case Phase::Invasion:
        m_InvasionLeft -= deltaSeconds;
        if (m_InvasionLeft >= 0.0f)
        {
            m_SpawnTimer -= deltaSeconds;
            if (m_SpawnTimer <= 0.0f)
            {
                m_SpawnTimer = Param("SpawnInterval");
                SpawnEnemyWave();
            }
        }
        else if (FindByTag(MI::Tags::Enemy).empty())
        {
            // Round won: harder next time
            m_InvasionLeft = 0.0f;
            ++m_Round;
            m_SpawnVolume += 2;
            m_Phase = Phase::Spawn;
        }
        break;
    case Phase::GameOver:
        break;
    }
}

void MetalInvasion::SkipPreparation()
{
    if (m_Phase == Phase::Preparation)
        m_PrepLeft = 0.0f;
}

void MetalInvasion::SpawnCrystals()
{
    int missing = static_cast<int>(Param("CrystalCount")) - static_cast<int>(FindByTag(MI::Tags::Crystal).size());
    for (int i = 0; i < missing; ++i)
    {
        // Uniform in the ring
        float angle = Random01() * 2.0f * PI;
        float r2 = CRYSTAL_RING_MIN * CRYSTAL_RING_MIN +
                   Random01() * (CRYSTAL_RING_MAX * CRYSTAL_RING_MAX - CRYSTAL_RING_MIN * CRYSTAL_RING_MIN);
        float radius = std::sqrt(r2);
        AddObstacle(MI::SpawnCrystal(Vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius), CRYSTAL_AMOUNT));
    }
}

void MetalInvasion::SpawnEnemyWave()
{
    for (int i = 0; i < m_SpawnVolume; ++i)
    {
        float angle = Random01() * 2.0f * PI;
        Vec3 at(std::cos(angle) * ENEMY_SPAWN_DISTANCE, 0.0f, std::sin(angle) * ENEMY_SPAWN_DISTANCE);
        if (Random01() < ENEMY_TO_TANK_RATIO)
        {
            float speed = ENEMY_SPEED_MIN + Random01() * (ENEMY_SPEED_MAX - ENEMY_SPEED_MIN);
            MI::SpawnEnemyBattalion(at, BATTALION_SIZE, speed, Param("EnemyHealth"));
        }
        else
            MI::SpawnEnemyTank(at, ENEMY_TANK_HEALTH);
    }
}

void MetalInvasion::OnBaseDestroyed()
{
    m_Phase = Phase::GameOver;
    CancelWall();
    m_Mode = Mode::Command;
}

//-----------------------------------------------------------------------------
// Path finding obstacles
//-----------------------------------------------------------------------------

void MetalInvasion::AddObstacle(Entity entity)
{
    m_EnemyField.AddObstacle(entity);
    m_UnitField.AddObstacle(entity);
    m_EnemyField.Refresh();
    m_UnitField.Refresh();
}

void MetalInvasion::OnObstacleRemoved(Entity entity)
{
    m_EnemyField.RemoveObstacle(entity);
    m_UnitField.RemoveObstacle(entity);
    m_EnemyField.Refresh();
    m_UnitField.Refresh();
}

//-----------------------------------------------------------------------------
// Camera (GameCameraController)
//-----------------------------------------------------------------------------

void MetalInvasion::UpdateCamera(float deltaSeconds)
{
    // Pan along the ground, relative to where the camera looks
    Vec3 forward = SceneCamera::Forward(m_View.Yaw);
    Vec3 right = SceneCamera::Right(m_View.Yaw);
    Vec3 pan(0, 0, 0);
    if (KeyDown(App::KEY_W))
        pan = pan + forward;
    if (KeyDown(App::KEY_S))
        pan = pan - forward;
    if (KeyDown(App::KEY_A))
        pan = pan - right;
    if (KeyDown(App::KEY_D))
        pan = pan + right;
    Vec3& target = m_View.Target;
    target = target + pan * (CAMERA_PAN_SPEED * deltaSeconds);
    target.X = std::clamp(target.X, -CAMERA_LIMIT, CAMERA_LIMIT);
    target.Z = std::clamp(target.Z, -CAMERA_LIMIT, CAMERA_LIMIT);
    SceneCamera::Apply(*Resource<Camera>(), m_View);
}

//-----------------------------------------------------------------------------
// Mouse (UnitControllerSystem, BuildObstaclesSystem, MainLevelUI::Update)
//-----------------------------------------------------------------------------

Entity MetalInvasion::UnitUnderMouse(const Vec3& ground)
{
    for (Entity unit : FindByTag(MI::Tags::Unit))
    {
        if (SceneObjects::Contains(unit, ground, PICK_MARGIN))
            return unit;
    }
    return NULL_ENTITY;
}

void MetalInvasion::HandleMouse()
{
    Vec3 ground;
    bool onGround = MouseGround(ground);

    // Hovering the base highlights it (it opens the shop)
    bool hovered = m_Mode == Mode::Command && onGround && m_Base != NULL_ENTITY &&
                   SceneObjects::Contains(m_Base, ground);
    if (hovered != m_BaseHovered && m_Base != NULL_ENTITY && Has<FragShaderTag>(m_Base))
        Get<FragShaderTag>(m_Base).FragAssetId = hovered ? RedShaderID : BlinnPhongID;
    m_BaseHovered = hovered;

    switch (m_Mode)
    {
    case Mode::BaseMenu:
        // The menu's buttons are handled in OnRender, right click closes it
        if (MouseRightClicked())
            CloseBaseMenu();
        return;
    case Mode::PlaceWall:
        if (onGround)
            MoveWallPreview(ground);
        if (KeyPressed(App::KEY_R))
            RotateWallPreview();
        if (MouseRightClicked())
            CancelWall();
        else if (MouseClicked())
            PlaceWall();
        return;
    case Mode::Command:
        break;
    }

    if (MouseRightClicked())
    {
        ClearSelection();
        return;
    }
    if (!MouseClicked() || !onGround)
        return;
    Entity unit = UnitUnderMouse(ground);
    if (unit != NULL_ENTITY)
        Select(unit, KeyDown(App::KEY_SPACE));
    else if (hovered)
        OpenBaseMenu();
    else if (!SelectedUnits().empty())
        OrderMove(ground);
}

void MetalInvasion::Select(Entity unit, bool merge)
{
    auto* picked = ScriptOf<MIPlayerUnit>(unit);
    if (picked == nullptr)
        return;
    if (!merge)
        ClearSelection();
    int battalion = picked->Battalion();
    for (Entity e : FindByTag(MI::Tags::Unit))
    {
        auto* u = ScriptOf<MIPlayerUnit>(e);
        if (u == nullptr)
            continue;
        if (u->Battalion() == battalion)
            u->SetSelected(true);
        // Space + click: everything selected becomes one battalion
        if (merge && u->IsSelected())
            u->SetBattalion(battalion);
    }
}

void MetalInvasion::ClearSelection()
{
    for (Entity e : FindByTag(MI::Tags::Unit))
    {
        if (auto* u = ScriptOf<MIPlayerUnit>(e))
            u->SetSelected(false);
    }
    m_HasMoveOrder = false;
}

std::vector<Entity> MetalInvasion::SelectedUnits()
{
    std::vector<Entity> selected;
    for (Entity e : FindByTag(MI::Tags::Unit))
    {
        auto* u = ScriptOf<MIPlayerUnit>(e);
        if (u != nullptr && u->IsSelected())
            selected.push_back(e);
    }
    return selected;
}

void MetalInvasion::OrderMove(const Vec3& target)
{
    m_UnitField.SetGoal(target);
    m_HasMoveOrder = true;
}

//-----------------------------------------------------------------------------
// Shop and walls
//-----------------------------------------------------------------------------

void MetalInvasion::OpenBaseMenu()
{
    m_Mode = Mode::BaseMenu;
    m_MenuJustOpened = true;
}

void MetalInvasion::CloseBaseMenu()
{
    if (m_Mode == Mode::BaseMenu)
        m_Mode = Mode::Command;
}

bool MetalInvasion::Purchase(Item item)
{
    static const int costs[] = {SOLDIERS_COST, SUPPORT_COST, TANK_COST, WALL_COST};
    int cost = costs[static_cast<int>(item)];
    if (m_Phase == Phase::GameOver || m_Crystals < cost || m_WallPreview != NULL_ENTITY)
        return false;
    m_Crystals -= cost;
    m_Mode = Mode::Command;
    switch (item)
    {
    case Item::Soldiers:
        MI::SpawnBattalion(RALLY_POINT, BATTALION_SIZE, m_NextBattalion++, false);
        break;
    case Item::Support:
        MI::SpawnBattalion(RALLY_POINT, BATTALION_SIZE, m_NextBattalion++, true);
        break;
    case Item::Tank:
        MI::SpawnTank(RALLY_POINT, m_NextBattalion++);
        break;
    case Item::Wall: {
        m_Mode = Mode::PlaceWall;
        m_WallRotated = false;
        Vec3 ground;
        m_WallPreview = MI::SpawnWall(MouseGround(ground) ? ground : RALLY_POINT, false, true);
        m_WallPreviewShownValid = true;
        break;
    }
    }
    return true;
}

void MetalInvasion::MoveWallPreview(const Vec3& position)
{
    if (m_WallPreview == NULL_ENTITY)
        return;
    SceneObjects::SetPosition(m_WallPreview, Vec3(position.X, 0.0f, position.Z));
    bool valid = WallPreviewValid();
    if (valid != m_WallPreviewShownValid)
    {
        // Red while it can not be built (rebuilds the shape's mesh)
        Get<Shape2D>(m_WallPreview).Color = valid ? WALL_OK : WALL_BLOCKED;
        SceneObjects::ShapeChanged(m_WallPreview);
        m_WallPreviewShownValid = valid;
    }
}

void MetalInvasion::RotateWallPreview()
{
    if (m_WallPreview == NULL_ENTITY)
        return;
    m_WallRotated = !m_WallRotated;
    SceneObjects::SetYaw(m_WallPreview, m_WallRotated ? 90.0f : 0.0f);
}

bool MetalInvasion::WallPreviewValid()
{
    if (m_WallPreview == NULL_ENTITY)
        return false;
    // Nothing may stand where the wall goes
    const char* const blocking[] = {MI::Tags::Unit, MI::Tags::Enemy, MI::Tags::Crystal, MI::Tags::Wall, MI::Tags::Base};
    for (const char* tag : blocking)
    {
        float margin = std::string(tag) == MI::Tags::Base ? MI::BASE_SCALE * 0.5f : 0.6f;
        for (Entity e : FindByTag(tag))
        {
            if (e != m_WallPreview && SceneObjects::Contains(m_WallPreview, PositionOf(e), margin))
                return false;
        }
    }
    return true;
}

bool MetalInvasion::PlaceWall()
{
    if (m_WallPreview == NULL_ENTITY || !WallPreviewValid())
        return false;
    MI::BuildWall(m_WallPreview, m_WallRotated);
    AddObstacle(m_WallPreview);
    m_WallPreview = NULL_ENTITY;
    m_Mode = Mode::Command;
    return true;
}

void MetalInvasion::CancelWall()
{
    if (m_WallPreview == NULL_ENTITY)
        return;
    Destroy(m_WallPreview);
    m_WallPreview = NULL_ENTITY;
    m_Crystals += WALL_COST;
    m_Mode = Mode::Command;
}

//-----------------------------------------------------------------------------
// HUD (MainLevelUI::Render, WinScreen)
//-----------------------------------------------------------------------------

void MetalInvasion::OnRender()
{
    RenderHud();
    if (m_Mode == Mode::BaseMenu)
        RenderBaseMenu();
    else if (m_Mode == Mode::PlaceWall)
    {
        Vec2 mouse = MouseScreen();
        DrawText(mouse.X + 12.0f, mouse.Y + 12.0f, "Press R to Rotate | Click to Place | Right click: cancel");
    }
    if (m_Phase == Phase::GameOver)
        RenderGameOver();
}

void MetalInvasion::RenderHud()
{
    FillBar(100.0f, APP_VIRTUAL_HEIGHT - 100.0f, 300.0f, 50.0f, BaseHealth());
    DrawText(450.0f, APP_VIRTUAL_HEIGHT - 75.0f, "Crystal Count: " + std::to_string(m_Crystals));

    std::string phase;
    if (m_Phase == Phase::Preparation)
        phase = "Preparation Phase: " + Seconds(m_PrepLeft) + " (Enter: start now)";
    else if (m_Phase == Phase::Invasion)
        phase = m_InvasionLeft > 0.0f ? "Invasion Phase Spawn Period: " + Seconds(m_InvasionLeft)
                                      : "Invasion Phase (Kill All Enemy to Advance)";
    DrawText(600.0f, APP_VIRTUAL_HEIGHT - 75.0f, phase);
    DrawText(100.0f, 75.0f, "Round: " + std::to_string(m_Round));
    DrawText(100.0f, 45.0f,
             "WASD to pan camera - LEFT Click to select/guide units - RIGHT Click to unselect - "
             "SPACE to group units - Click the base to buy");
}

void MetalInvasion::RenderBaseMenu()
{
    auto ui = Resource<UIState>();
    // The click that opened the menu must not also press a button
    bool click = ui->leftClick;
    if (m_MenuJustOpened)
        ui->leftClick = false;
    m_MenuJustOpened = false;

    const float width = 400.0f;
    const float height = 400.0f;
    float x = APP_VIRTUAL_WIDTH * 0.5f - width * 0.5f;
    float y = APP_VIRTUAL_HEIGHT * 0.5f - height * 0.5f;
    DrawContainer(static_cast<int>(x), static_cast<int>(y), width, height);

    if (Button(1, x + 40.0f, y + 40.0f, *ui, 140.0f, 25.0f, "Back"))
        CloseBaseMenu();

    struct Entry
    {
        Item What;
        const char* Label;
        int Cost;
    };
    const Entry entries[] = {{Item::Soldiers, "Purchase Soldiers", SOLDIERS_COST},
                             {Item::Support, "Purchase Support", SUPPORT_COST},
                             {Item::Tank, "Purchase Tank", TANK_COST},
                             {Item::Wall, "Purchase Obstacle", WALL_COST}};
    float by = y + height - 100.0f;
    int id = 2;
    for (const Entry& entry : entries)
    {
        if (Button(id++, x + 100.0f, by, *ui, 200.0f, 30.0f, entry.Label) && !Purchase(entry.What))
            App::PlayAudio("data/Sounds/wrongSound.wav");
        by -= 20.0f;
        DrawText(x + 100.0f, by, "Cost " + std::to_string(entry.Cost) + " crystals");
        by -= 40.0f;
    }
    ui->leftClick = click;
}

void MetalInvasion::RenderGameOver()
{
    const float width = 300.0f;
    const float height = 220.0f;
    float x = (APP_VIRTUAL_WIDTH - width) * 0.5f;
    float y = (APP_VIRTUAL_HEIGHT - height) * 0.5f;
    DrawContainer(static_cast<int>(x), static_cast<int>(y), width, height);
    DrawText(x + 105.0f, y + 150.0f, "You Lose", {1.0f, 0.4f, 0.3f});
    DrawText(x + 70.0f, y + 110.0f, "Survived " + std::to_string(m_Round - 1) + " round(s)");
    DrawText(x + 40.0f, y + 60.0f, "Enter: play again   Esc: menu");
}
