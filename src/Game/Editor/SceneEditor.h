//---------------------------------------------------------------------------------
// SceneEditor.h
//---------------------------------------------------------------------------------
//
// Headless core of the scene authoring tool. It edits the live ECS world
// (the global ECS instance) and is used by:
//   - SceneEditorScene: the in-game GUI (palette, inspector, toolbar)
//   - the unit tests and the AuthorScenes tool, which author scenes with
//     code through exactly the same API
//
// Scenes are saved with the save system (WorldSerializer) and are playable:
// loading a scene file with GameManager::LoadGame starts the main level with
// the authored world. Undo / redo snapshots also use the serializer, so every
// edit exercises a full save + load of the world.
//
#pragma once

#include "../Entity.h"
#include "../Serialization/WorldSerializer.h"
#include "../Vec3.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Editor
{
    /**
     * \brief Everything that can be placed from the editor palette
     */
    enum class PrefabType
    {
        Soldier,
        Support,
        PlayerTank,
        EnemySoldier,
        EnemyTank,
        Crystal,
        Wall,
        Count
    };

    const char* PrefabName(PrefabType type);

    /**
     * \brief Role of an entity in a scene, derived from its components
     */
    enum class EntityKind
    {
        None,
        Ground,
        PlayerBase,
        Selector,
        Soldier,
        Support,
        PlayerTank,
        EnemySoldier,
        EnemyTank,
        Crystal,
        Wall,
        Other // e.g. the mesh children of a tank
    };

    const char* KindName(EntityKind kind);

    /**
     * \brief Parameters used when placing a prefab
     */
    struct PrefabSettings
    {
        int Health = 100;
        int Battalion = 1;
        int CrystalAmount = 30;
        float EnemySpeed = 0.0012f;
        float YawDegrees = 0.0f;
    };

    class SceneEditor
    {
      public:
        // Metadata written into every scene file
        static constexpr const char* META_SCENE = "Scene";
        static constexpr const char* META_NAME = "Name";
        static constexpr const char* META_TOOL = "Tool";
        // Scene the authored worlds are played in
        static constexpr const char* PLAYABLE_SCENE = "MainLevel";
        static constexpr size_t MAX_UNDO = 64;

        /**
         * \brief Replace the world with an empty playable scene: ground, player
         *        base, unit selector and default round settings
         */
        void NewScene();

        // -- Entities --------------------------------------------------------

        /**
         * \brief Place a prefab (position is clamped to the play area)
         * \return the root entity of the prefab
         */
        Entity Place(PrefabType type, const Vec3& position, const PrefabSettings& settings = {});

        /**
         * \brief Editable entity under a point on the ground (NULL_ENTITY if none)
         */
        Entity Pick(const Vec3& groundPoint) const;

        EntityKind KindOf(Entity entity) const;

        /**
         * \brief Scene objects the user can select (units, base, crystals ...)
         */
        bool IsEditable(Entity entity) const;

        std::vector<Entity> EditableEntities() const;

        Vec3 GetPosition(Entity entity) const;

        /**
         * \param recordUndo false while dragging (record once when the drag starts)
         */
        bool Move(Entity entity, const Vec3& position, bool recordUndo = true);

        /**
         * \brief Set the rotation around the up axis. Walls snap to 0 / 90 degrees
         *        (the only orientations the game supports for them)
         */
        bool SetYaw(Entity entity, float degrees);

        float GetYaw(Entity entity) const;

        /**
         * \brief Remove an entity and its children. The player base and the unit
         *        selector are required by the game and can not be removed
         */
        bool Remove(Entity entity);

        // -- Properties (return false if the entity has no such property) ------

        int GetHealth(Entity entity) const;
        bool SetHealth(Entity entity, int health);

        int GetBattalion(Entity entity) const;
        bool SetBattalion(Entity entity, int battalion);

        int GetCrystalAmount(Entity entity) const;
        bool SetCrystalAmount(Entity entity, int amount);

        float GetEnemySpeed(Entity entity) const;
        bool SetEnemySpeed(Entity entity, float speed);

        // -- Round settings (GameState) ----------------------------------------

        int GetStartingCrystals() const;
        void SetStartingCrystals(int crystals);

        int GetRoundNumber() const;
        void SetRoundNumber(int round);

        int GetSpawnVolume() const;
        void SetSpawnVolume(int volume);

        // -- Validation / files ------------------------------------------------

        /**
         * \brief Problems that would prevent the scene from being played
         *        (empty = playable)
         */
        std::vector<std::string> Validate() const;

        /**
         * \brief Save the scene. Refused when Validate() reports problems
         */
        Serialization::SaveResult SaveScene(const std::string& path, const std::string& name);

        /**
         * \brief Load a scene file (must be a playable MainLevel scene). On
         *        failure the current scene is left untouched
         */
        Serialization::LoadResult LoadScene(const std::string& path);

        /**
         * \brief Serialize the current scene with scene metadata
         */
        std::vector<std::uint8_t> SaveSceneToBytes(const std::string& name) const;

        // -- Undo / redo -------------------------------------------------------

        /**
         * \brief Snapshot the current world so the next edit can be undone
         */
        void RecordUndo();

        bool Undo();
        bool Redo();

        size_t UndoCount() const { return m_UndoStack.size(); }
        size_t RedoCount() const { return m_RedoStack.size(); }

        bool IsDirty() const { return m_Dirty; }

        // -- Selection (kept valid across undo / redo) ---------------------------

        Entity Selected() const { return m_Selected; }
        void Select(Entity entity);

        /**
         * \brief Called whenever the whole world is replaced (new scene, load,
         *        undo, redo). The GUI uses it to drop per entity render caches
         */
        std::function<void()> OnWorldReplaced;

        static float Snap(float value, float step);
        static Vec3 ClampToPlayArea(const Vec3& position);

      private:
        std::vector<std::uint8_t> Snapshot() const;
        void Restore(const std::vector<std::uint8_t>& snapshot);
        void MarkEdited();
        void ClearHistory();

        std::vector<std::vector<std::uint8_t>> m_UndoStack;
        std::vector<std::vector<std::uint8_t>> m_RedoStack;
        Entity m_Selected = NULL_ENTITY;
        bool m_Dirty = false;
    };
} // namespace Editor
