//---------------------------------------------------------------------------------
// SceneEditor.h
//---------------------------------------------------------------------------------
//
// Headless core of the generic scene editor. It edits the live ECS world (the
// global ECS instance); SceneEditorScene is only the GUI on top of it, and the
// unit tests / AuthorScenes tool drive the same API from code.
//
// A scene is a field (a large flat rectangle) with objects placed on it:
// basic 2D shapes (rectangle, circle, triangle, regular polygon), 3D models or
// empty transforms (no shape: groups, markers, spawn points). Objects form a
// hierarchy through their Transforms: a child moves with its parent.
// Every object is an ECS entity built by SceneObjects (World/SceneObjects.h);
// C++ scripts are attached by name with editable parameters.
//
// Undo / redo and play mode snapshot the world with the serializer, so every
// edit also exercises a full save + load of the scene.
//
#pragma once

#include "Entity.h"
#include "Reflection/Reflection.h"
#include "Serialization/WorldSerializer.h"
#include "Vec3.h"
#include "World/Prefab.h"
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
        // Only a Transform (and a name): groups other objects, marks points
        Empty,
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

    /**
     * \brief One component of an object, as the inspector lists it
     */
    struct ComponentView
    {
        std::string Name;
        // Transform, SceneObject, Shape2D / Mesh, RigidBody, Script: edited
        // by the object's properties, not field by field
        bool BuiltIn = false;
        bool Removable = false;
        // Short state of a built in component ("Static", "Rotator")
        std::string Summary;
        // Reflected fields of a catalog component (null for built-ins)
        const Reflection::TypeInfo* Type = nullptr;
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
         * \brief Create an object with default settings, as a child of
         *        `parent` when given (it keeps `position` in the world). One
         *        undo step; the new object is selected. What the "Create"
         *        context menus call
         */
        Entity Create(ObjectKind kind,
                      const Vec3& position,
                      Entity parent = NULL_ENTITY,
                      const PlaceSettings& settings = {});

        /**
         * \brief Copy of an object (same shape, body, tag, script, components)
         *        next to it, with copies of all of its children. The copy has
         *        the same parent as the original
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

        // Positions and rotations are in world space: moving a parent carries
        // its children along, moving a child moves it alone
        bool Move(Entity entity, const Vec3& position, bool recordUndo = true);
        bool SetYaw(Entity entity, float degrees);

        /**
         * \brief Delete an object and all of its children
         */
        bool Remove(Entity entity);

        // -- Prefabs ---------------------------------------------------------------
        //
        // Instances are ordinary objects whose root has a PrefabLink

        /**
         * \brief Place a copy of a prefab (under `parent` when given). One undo
         *        step, the instance's root is selected
         */
        Entity PlacePrefab(const Prefab::Data& prefab, const Vec3& position, Entity parent = NULL_ENTITY);

        /**
         * \brief The prefab an instance root comes from ("" for other objects)
         */
        std::string PrefabOf(Entity entity) const;

        /**
         * \brief The object and its children as a prefab named `name`
         */
        Prefab::Data CapturePrefab(Entity root, const std::string& name) const;

        /**
         * \brief Make an object the root of an instance of `name` (after saving
         *        it as that prefab)
         */
        bool LinkPrefab(Entity root, const std::string& name);

        /**
         * \brief Turn an instance back into ordinary objects
         */
        bool UnpackPrefab(Entity root);

        /**
         * \brief Replace an instance by a fresh copy of the prefab, at the same
         *        place, rotation and parent, with the same root name. Returns
         *        the new root (NULL_ENTITY if it is not an instance)
         */
        Entity ResetToPrefab(Entity root, const Prefab::Data& prefab);

        /**
         * \brief Reset every instance of the prefab (after it was edited). One
         *        undo step; returns how many were updated
         */
        int UpdatePrefabInstances(const Prefab::Data& prefab);

        /**
         * \brief Every object of the scene (except the field) as one prefab.
         *        A single top level object is the root; several are grouped
         *        under a new empty root named `name`. What saving in the
         *        prefab editor stores
         */
        Prefab::Data CaptureStage(const std::string& name) const;

        /**
         * \brief Replace the world with a prefab stage: the field and a copy
         *        of `prefab` at the origin (not linked to it), or a new empty
         *        root named `name` when prefab is null. No undo history, not
         *        dirty. Returns the root
         */
        Entity OpenPrefabStage(const Prefab::Data* prefab, const std::string& name);

        /**
         * \brief The document was written to its file (clears the unsaved flag)
         */
        void MarkSaved() { m_Dirty = false; }

        // -- Sessions (prefab editing) ---------------------------------------------

        /**
         * \brief Everything needed to come back to a scene: the world, undo /
         *        redo history, unsaved state and selection
         */
        struct Session
        {
            std::vector<std::uint8_t> World;
            std::vector<std::vector<std::uint8_t>> Undo;
            std::vector<std::vector<std::uint8_t>> Redo;
            bool Dirty = false;
            Entity Selected = NULL_ENTITY;
        };

        /**
         * \brief Put the scene aside (to edit a prefab in an empty stage)
         */
        Session Suspend() const;

        /**
         * \brief Bring a suspended scene back exactly as it was
         */
        void Resume(const Session& session);

        // -- Hierarchy -------------------------------------------------------------

        /**
         * \brief Make `child` a child of `parent` (NULL_ENTITY: back to the top
         *        of the scene). The child keeps its place in the world. Refused
         *        for the field (neither parent nor child) and for loops (an
         *        object under one of its own children)
         */
        bool SetParent(Entity child, Entity parent);

        /**
         * \brief New empty object at a world position, as a child of `parent`
         *        (NULL_ENTITY: top level). One undo step; it is selected
         */
        Entity AddEmpty(const Vec3& position, Entity parent = NULL_ENTITY);

        /**
         * \brief Parent object, NULL_ENTITY for a top level object
         */
        Entity ParentOf(Entity entity) const;

        /**
         * \brief Child objects, in order
         */
        std::vector<Entity> ChildrenOf(Entity entity) const;

        /**
         * \brief Top level objects (no parent), the field first
         */
        std::vector<Entity> RootObjects() const;

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

        // -- Components ------------------------------------------------------------
        //
        // Besides its built in components, an object can have any component
        // registered in the ComponentCatalog (Reflection/ComponentCatalog.h).
        // Their fields are read and written by name through reflection.

        static constexpr const char* COMPONENT_RIGIDBODY = "RigidBody";
        static constexpr const char* COMPONENT_SCRIPT = "Script";

        /**
         * \brief Components of an object, built in ones first
         */
        std::vector<ComponentView> ComponentsOf(Entity entity) const;

        /**
         * \brief Components that can still be added to the object (RigidBody,
         *        Script and the catalog components it does not have)
         */
        std::vector<std::string> AddableComponents(Entity entity) const;

        bool HasComponent(Entity entity, const std::string& component) const;

        /**
         * \brief Add a component with its default values. RigidBody adds a
         *        static body, Script the first registered object script
         */
        bool AddComponent(Entity entity, const std::string& component);

        /**
         * \brief Remove a removable component (RigidBody, Script, catalog
         *        components). Transform, SceneObject, Shape2D and Mesh stay
         */
        bool RemoveComponent(Entity entity, const std::string& component);

        /**
         * \brief Set a reflected field. The value is converted and kept inside
         *        the field's range; false (nothing changed) when it does not
         *        fit, the field is read only, or the object / component /
         *        field does not exist. Entity fields only take objects (or
         *        NULL_ENTITY)
         */
        bool SetField(Entity entity,
                      const std::string& component,
                      const std::string& field,
                      const Reflection::FieldValue& value);

        /**
         * \brief Current value of a reflected field, false when it does not exist
         */
        bool GetField(Entity entity,
                      const std::string& component,
                      const std::string& field,
                      Reflection::FieldValue& value) const;

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
        // One object (no children) at a world position, same world yaw
        Entity CopyObject(Entity source, const Vec3& position);
        // Place without an undo step
        Entity PlaceObject(ObjectKind kind, const Vec3& position, const PlaceSettings& settings);
        Entity DuplicateTree(Entity source, const Vec3& offset, Entity parent);
        void PushUndo(std::vector<std::uint8_t> snapshot);
        // Entity fields pointing at a removed object are cleared
        void ClearReferencesTo(Entity removed);
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
