//---------------------------------------------------------------------------------
// SceneEditor.h
//---------------------------------------------------------------------------------
//
// Headless core of the generic scene editor. It edits the live ECS world (the
// global ECS instance); SceneEditorScene is only the GUI on top of it, and the
// unit tests / AuthorScenes tool drive the same API from code.
//
// A scene is a field (a large flat rectangle) with objects placed on it:
// basic 2D shapes (rectangle, circle, triangle, regular polygon) or 3D models.
// Every object is an ECS entity built by SceneObjects (World/SceneObjects.h);
// C++ scripts are attached by name with editable parameters.
//
// Undo / redo and play mode snapshot the world with the serializer, so every
// edit also exercises a full save + load of the scene.
//
#pragma once

#include "Entity.h"
#include "Serialization/WorldSerializer.h"
#include "Vec3.h"
#include "World/SceneObjects.h"
#include "World/ScenePlayer.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Editor
{
    /**
     * \brief What the palette places
     */
    enum class ObjectKind
    {
        Rectangle,
        Circle,
        Triangle,
        Polygon,
        Model,
        Count
    };

    const char* ObjectKindName(ObjectKind kind);

    /**
     * \brief Settings used when placing an object (the editor "brush")
     */
    struct PlaceSettings
    {
        float Width = 1.0f;
        float Height = 1.0f;
        int Sides = 6;
        float Thickness = 0.25f;
        Vec3 Color = {0.85f, 0.55f, 0.25f};
        float YawDegrees = 0.0f;
        SceneObjects::BodyType Body = SceneObjects::BodyType::None;
        std::string Tag;
        // Model name (data/models) when placing ObjectKind::Model
        std::string Model;
    };

    class SceneEditor
    {
      public:
        // Metadata written into every scene file
        static constexpr const char* META_NAME = "Name";
        static constexpr const char* META_TOOL = "Tool";
        // Scene (registered by the Game program) that plays authored scenes
        static constexpr const char* PLAYER_SCENE = ScenePlayer::NAME;
        // Name / tag of the field object every scene has
        static constexpr const char* FIELD_NAME = "Field";
        static constexpr size_t MAX_UNDO = 64;

        /**
         * \brief Replace the world with an empty scene (just the field)
         */
        void NewScene();

        // -- Objects -----------------------------------------------------------

        /**
         * \brief Place an object (position clamped to the field)
         */
        Entity Place(ObjectKind kind, const Vec3& position, const PlaceSettings& settings = {});

        /**
         * \brief Copy of an object (same shape, body, tag, script) next to it
         */
        Entity Duplicate(Entity entity);

        /**
         * \brief Object under a point on the ground. Smaller objects win over
         *        larger ones, the field is only picked when nothing else is hit
         */
        Entity Pick(const Vec3& groundPoint) const;

        /**
         * \brief Pick with the mouse ray: `pointAtHeight(h)` is where the ray
         *        crosses the horizontal plane y = h. Each object is tested from
         *        its top down to its base, so the visible top or side of a thick
         *        shape or a tall model is what gets clicked (not the ground
         *        behind it). `hitHeight` receives the height of the hit
         */
        Entity PickRay(const std::function<Vec3(float)>& pointAtHeight, float* hitHeight = nullptr) const;

        /**
         * \brief Every scene object (entities with a SceneObject)
         */
        std::vector<Entity> Objects() const;

        bool IsObject(Entity entity) const;

        /**
         * \brief The field can be recoloured / resized but not moved or removed
         */
        bool IsField(Entity entity) const;

        ObjectKind KindOf(Entity entity) const;

        std::string NameOf(Entity entity) const;

        bool Move(Entity entity, const Vec3& position, bool recordUndo = true);
        bool SetYaw(Entity entity, float degrees);
        bool Remove(Entity entity);

        /**
         * \brief Shape footprint (Width x Height, circles / polygons use Width).
         *        For models Width is the scale
         */
        bool SetSize(Entity entity, float width, float height);
        bool SetSides(Entity entity, int sides);
        bool SetThickness(Entity entity, float thickness);
        bool SetColor(Entity entity, const Vec3& color);
        bool SetBody(Entity entity, SceneObjects::BodyType body);
        bool SetTag(Entity entity, const std::string& tag);

        /**
         * \brief Rename an object (not the field). Names must be unique and
         *        non empty: false (nothing changed) otherwise
         */
        bool Rename(Entity entity, const std::string& name);
        bool SetModel(Entity entity, const std::string& model);

        /**
         * \brief Attach a registered object script (empty name removes it).
         *        Parameters start at the script's declared defaults
         */
        bool SetScript(Entity entity, const std::string& script);
        bool SetScriptParam(Entity entity, const std::string& param, float value);

        std::string GetScript(Entity entity) const;
        float GetScriptParam(Entity entity, const std::string& param) const;

        // -- Scene settings ------------------------------------------------------

        bool SetSceneScript(const std::string& script);
        bool SetSceneParam(const std::string& param, float value);
        float GetSceneParam(const std::string& param) const;

        /**
         * \brief Resize the playing field (and the field object)
         */
        void SetFieldSize(float width, float height);

        /**
         * \brief Camera used when the scene is played
         */
        void SetGameCamera(const Vec3& target, float distance);

        // -- Validation / files --------------------------------------------------

        /**
         * \brief Problems that make the scene unplayable (empty = playable):
         *        duplicate names, unknown scripts, objects off the field
         */
        std::vector<std::string> Validate() const;

        /**
         * \brief Save the scene (refused when Validate() reports problems)
         */
        Serialization::SaveResult SaveScene(const std::string& path, const std::string& name);

        /**
         * \brief Load a scene file. Validation problems are returned as
         *        warnings (so they can be fixed); a file that is not a scene
         *        leaves the current scene untouched
         */
        Serialization::LoadResult LoadScene(const std::string& path);

        /**
         * \brief The scene file's bytes, in WorldSerializer::FileFormat() (binary
         *        or plain text)
         */
        std::vector<std::uint8_t> SaveSceneToBytes(const std::string& name) const;

        // -- Undo / redo -----------------------------------------------------------

        void RecordUndo();
        bool Undo();
        bool Redo();
        size_t UndoCount() const { return m_UndoStack.size(); }
        size_t RedoCount() const { return m_RedoStack.size(); }
        bool IsDirty() const { return m_Dirty; }

        // -- Play mode ---------------------------------------------------------------

        /**
         * \brief Remember the authored world before scripts start changing it
         */
        void BeginPlay();

        /**
         * \brief Restore the world exactly as it was when BeginPlay was called
         */
        void EndPlay();

        bool IsPlaying() const { return m_Playing; }

        // -- Selection -----------------------------------------------------------------

        Entity Selected() const { return m_Selected; }
        void Select(Entity entity);

        /**
         * \brief Called whenever the whole world is replaced (new, load, undo,
         *        redo, end of play). The GUI drops per entity render caches
         */
        std::function<void()> OnWorldReplaced;

        static float Snap(float value, float step);
        Vec3 ClampToField(const Vec3& position) const;

      private:
        std::vector<std::uint8_t> Snapshot() const;
        void Restore(const std::vector<std::uint8_t>& snapshot);
        void WorldReplaced();
        bool CanEdit(Entity entity) const;

        std::vector<std::vector<std::uint8_t>> m_UndoStack;
        std::vector<std::vector<std::uint8_t>> m_RedoStack;
        std::vector<std::uint8_t> m_PlaySnapshot;
        Entity m_Selected = NULL_ENTITY;
        bool m_Dirty = false;
        bool m_Playing = false;
    };
} // namespace Editor
