//---------------------------------------------------------------------------------
// EditorInspector.cpp
//---------------------------------------------------------------------------------
//
// Inspector of the scene editor (right panel), like Unity's: the selected
// object as one list of component sections, each with its values.
//
//   INSPECTOR
//   Name   [Crate        ]
//   Rectangle  #7           2 children
//   Tag < Wall >
//   - Transform       in Tank          Parent, Pos X / Y / Z, Rot (Scale)
//   - Shape2D         Rectangle        Width, Height, Sides, Thick, colour
//   - Shader          Rim + Wave       fragment and vertex shaders
//   - RigidBody       Static  [Remove] Body
//   - Script          Rotator [Remove] script + its parameters
//   - Health                  [Remove] fields generated from REFLECT
//   [        Add Component        ]    menu of what can be added
//   [ Duplicate ] [ Delete ]
//
// Click a section's title to fold it. When the sections are taller than the
// panel, a scrollbar appears on the right. The Scene button of the toolbar
// shows the scene settings here instead.
//
#include "ECSManager.h"
#include "EditorStyle.h"
#include "GameManager.h"
#include "Mesh.h"
#include "Reflection/ComponentCatalog.h"
#include "SceneEditorScene.h"
#include "ShaderLibrary.h"

#include "ShaderLibrary.h"
#include "Scripting/ScriptRegistry.h"
#include "Scripting/ScriptSystem.h"
#include "Transform.h"
#include "UIState.h"
#include "World/SceneComponents.h"

#include <cmath>

extern ECSManager ECS;
extern GameManager GameSceneManager;

using Editor::ObjectKind;
using SceneObjects::BodyType;
using namespace EditorStyle;

namespace
{
    constexpr int SCROLL_INSPECTOR = 1;
    constexpr float ROW_GAP = 4.0f;
    constexpr float REMOVE_W = 76.0f;
    // Indent of the values inside a section
    constexpr float SECTION_INDENT = 8.0f;
} // namespace

//-----------------------------------------------------------------------------
// Layout
//-----------------------------------------------------------------------------

bool SceneEditorScene::Row(float height, float& y)
{
    y = m_RowCursor - height;
    m_RowCursor -= height + ROW_GAP;
    return y >= m_ViewBottom - 0.5f && y + height <= m_ViewTop + 0.5f;
}

bool SceneEditorScene::Section(
        const std::string& title, const std::string& summary, bool removable, float x, float width, bool& removed)
{
    removed = false;
    bool open = m_Folded.count(title) == 0;
    float y = 0.0f;
    if (!Row(WIDGET_H, y))
        return open;
    float headW = removable ? width - REMOVE_W - 4.0f : width;
    DrawPanel(x - 4.0f, y, width + 8.0f, WIDGET_H, SECTION_FILL, SECTION_FILL);
    // Click the title to fold / unfold the section
    if (m_UI->leftClick && Inside(m_UI->mouseX, m_UI->mouseY, x, y, headW, WIDGET_H) && !m_UI->IsTyping())
    {
        open = !open;
        if (open)
            m_Folded.erase(title);
        else
            m_Folded.insert(title);
    }
    std::string head = (open ? "- " : "+ ") + title;
    Text(x + 4.0f, RowY(y), head, ACCENT, headW - 8.0f);
    float summaryX = x + 4.0f + UIText::Width(head) + 12.0f;
    if (!summary.empty() && summaryX < x + headW - 12.0f)
        Text(summaryX, RowY(y), summary, TEXT_DIM, x + headW - summaryX - 4.0f);
    if (removable && Button(NextId(), x + width - REMOVE_W, y, *m_UI, REMOVE_W, WIDGET_H, "Remove"))
        removed = true;
    return open;
}

void SceneEditorScene::RenderInspector()
{
    float left = SCREEN_W - INSPECTOR_W;
    DrawPanel(left, PANEL_BOTTOM, INSPECTOR_W, PANEL_TOP - PANEL_BOTTOM, PANEL_FILL, PANEL_BORDER);
    float x = left + 10.0f;
    // Room is always kept for the scrollbar: values don't move when it appears
    float width = INSPECTOR_W - 20.0f - SCROLLBAR_W - 4.0f;
    m_ViewTop = PANEL_TOP - 8.0f;
    m_ViewBottom = PANEL_BOTTOM + 6.0f;
    m_RowCursor = m_ViewTop + m_InspectorScroll;
    float start = m_RowCursor;

    if (m_Editor.IsPlaying())
        RenderPlayingInspector(x, width);
    else if (m_ShowScene && !m_PrefabMode)
        RenderSceneInspector(x, width);
    else if (m_Editor.Selected() != NULL_ENTITY)
        RenderObjectInspector(m_Editor.Selected(), x, width);
    else
    {
        float y = 0.0f;
        if (Row(WIDGET_H, y))
            Text(x, RowY(y), "INSPECTOR", ACCENT);
        if (Row(WIDGET_H, y))
            Text(x, RowY(y), "Nothing selected", TEXT_DIM);
        if (Row(WIDGET_H, y))
            Text(x, RowY(y), "Click an object to edit it,", TEXT_DIM);
        if (Row(WIDGET_H, y))
            Text(x, RowY(y), "right click to create one", TEXT_DIM);
    }
    m_InspectorContent = start - m_RowCursor;
    Scrollbar(SCROLL_INSPECTOR, left + INSPECTOR_W - SCROLLBAR_W - 6.0f, m_ViewBottom, m_ViewTop, m_InspectorContent,
              m_InspectorScroll);
}

void SceneEditorScene::RenderPlayingInspector(float x, float width)
{
    float y = 0.0f;
    if (Row(WIDGET_H, y))
        Text(x, RowY(y), "PLAYING", PLAY_TEXT, width);
    if (Row(WIDGET_H, y))
        Text(x, RowY(y), "Scripts: " + std::to_string(GameSceneManager.Scripts().InstanceCount()), TEXT_DIM, width);
    for (const std::string& missing : GameSceneManager.Scripts().MissingScripts())
    {
        if (Row(WIDGET_H, y))
            Text(x, RowY(y), "Missing " + missing, ERROR_TEXT, width);
    }
}

//-----------------------------------------------------------------------------
// Object
//-----------------------------------------------------------------------------

void SceneEditorScene::RenderObjectInspector(Entity e, float x, float width)
{
    bool isField = m_Editor.IsField(e);
    bool isShape = ECS.HasComponent<Shape2D>(e);
    bool isModel = ECS.HasComponent<Mesh>(e);
    float y = 0.0f;
    int d = 0;
    bool removed = false;
    float ix = x + SECTION_INDENT;
    float iw = width - SECTION_INDENT;

    // -- Header: name, kind, tag -----------------------------------------------------
    if (Row(WIDGET_H, y))
        Text(x, RowY(y), "INSPECTOR", ACCENT, width);
    if (isField)
    {
        if (Row(WIDGET_H, y))
            Text(x, RowY(y), "Field  #" + std::to_string(e), TEXT_DIM, width);
        if (Row(WIDGET_H, y))
            Text(x, RowY(y), "The field holds the scene", TEXT_DIM, width);
    }
    else
    {
        if (Row(WIDGET_H, y))
        {
            // Name: type a new one (must be unique)
            Text(x, RowY(y), "Name", TEXT, LABEL_W - 4.0f);
            std::string name = m_Editor.NameOf(e);
            FieldDrawn(ID_FIELD_NAME);
            if (TextField(ID_FIELD_NAME, x + LABEL_W, y, width - LABEL_W, WIDGET_H, *m_UI, name) ==
                        TextFieldEvent::Committed &&
                name != m_Editor.NameOf(e) && !m_Editor.Rename(e, name))
                SetStatus("Can not rename to '" + name + "' (empty or already used)", true);
        }
        if (Row(WIDGET_H, y))
        {
            std::string kind = m_Editor.IsCamera(e)  ? "Game camera"
                               : m_Editor.IsLight(e) ? "Light"
                                                     : Editor::ObjectKindName(m_Editor.KindOf(e));
            Text(x, RowY(y), kind + "  #" + std::to_string(e), TEXT_DIM, width * 0.55f);
            std::size_t children = m_Editor.ChildrenOf(e).size();
            if (children > 0)
                Text(x + width * 0.6f, RowY(y), std::to_string(children) + (children == 1 ? " child" : " children"),
                     TEXT_DIM, width * 0.4f);
        }
        if (Row(WIDGET_H, y))
        {
            std::string tag = ECS.GetComponent<SceneObject>(e).Tag;
            if ((d = Stepper(x, y, width, "Tag " + TagLabel(tag), "<", ">")) != 0)
                m_Editor.SetTag(e, TAGS[Cycle(IndexOf(TAGS, tag), d, TAG_COUNT)]);
        }
    }

    // -- Prefab instance -----------------------------------------------------------------
    std::string prefab = m_Editor.PrefabOf(e);
    if (!prefab.empty() && Section("Prefab", prefab, false, x, width, removed) && Row(WIDGET_H, y))
    {
        float third = (iw - 8.0f) / 3.0f;
        if (Button(NextId(), ix, y, *m_UI, third, WIDGET_H, "Edit"))
            EditPrefab(prefab);
        else if (Button(NextId(), ix + third + 4.0f, y, *m_UI, third, WIDGET_H, "Reset"))
        {
            Prefab::Data data;
            std::string error;
            if (!Prefab::LoadFile(Prefab::PathOf(prefab, m_PrefabDirectory), data, error))
                SetStatus("Can not load prefab " + prefab + ": " + error, true);
            else if (m_Editor.ResetToPrefab(e, data) != NULL_ENTITY)
                SetStatus("Reset to prefab " + prefab);
            return;
        }
        else if (Button(NextId(), ix + 2.0f * (third + 4.0f), y, *m_UI, third, WIDGET_H, "Unpack") &&
                 m_Editor.UnpackPrefab(e))
            SetStatus(m_Editor.NameOf(e) + " is no longer linked to " + prefab);
    }

    // -- Transform -------------------------------------------------------------------------
    if (!isField)
    {
        Entity parent = m_Editor.ParentOf(e);
        std::string where = parent == NULL_ENTITY ? std::string() : "in " + m_Editor.NameOf(parent);
        if (Section("Transform", where, false, x, width, removed))
        {
            if (Row(WIDGET_H, y))
            {
                // Parent: type an object's name ("-" = top level), or drag it
                // in the Hierarchy
                std::string parentName = parent == NULL_ENTITY ? std::string("-") : m_Editor.NameOf(parent);
                std::string typed = parentName;
                Text(ix, RowY(y), "Parent", TEXT, LABEL_W - 4.0f);
                FieldDrawn(ID_FIELD_PARENT);
                if (TextField(ID_FIELD_PARENT, ix + LABEL_W, y, iw - LABEL_W, WIDGET_H, *m_UI, typed) ==
                            TextFieldEvent::Committed &&
                    typed != parentName)
                {
                    bool none = typed.empty() || typed == "-";
                    Entity target = none ? NULL_ENTITY : SceneObjects::FindByName(typed);
                    if (target == NULL_ENTITY && !none)
                        SetStatus("No object named " + typed, true);
                    else if (!m_Editor.SetParent(e, target))
                        SetStatus("Can not put " + m_Editor.NameOf(e) + " under " + typed, true);
                    else
                        SetStatus(none ? m_Editor.NameOf(e) + " is now a top level object"
                                       : m_Editor.NameOf(e) + " is now a child of " + typed);
                }
            }
            Vec3 p = SceneObjects::GetPosition(e);
            float px = p.X, py = p.Y, pz = p.Z, yaw = SceneObjects::GetYaw(e);
            if (Row(WIDGET_H, y) && NumberRow(ix, y, iw, "Pos X", ID_FIELD_POS_X, px, SNAP_STEP))
                m_Editor.Move(e, Vec3(px, p.Y, p.Z));
            // Height (I / K in the scene view)
            if (Row(WIDGET_H, y) && NumberRow(ix, y, iw, "Pos Y", ID_FIELD_POS_Y, py, SNAP_STEP))
                m_Editor.SetHeight(e, py);
            if (Row(WIDGET_H, y) && NumberRow(ix, y, iw, "Pos Z", ID_FIELD_POS_Z, pz, SNAP_STEP))
                m_Editor.Move(e, Vec3(p.X, p.Y, pz));
            if (Row(WIDGET_H, y) && NumberRow(ix, y, iw, "Rot", ID_FIELD_ROT, yaw, ROTATE_STEP, "%.0f"))
                m_Editor.SetYaw(e, yaw);
            if (!isShape)
            {
                // Models and empties: the transform's scale (an empty's scale
                // also scales its children)
                float scale = ECS.GetComponent<Transform>(e).LocalScale.X;
                if (Row(WIDGET_H, y) && NumberRow(ix, y, iw, "Scale", ID_FIELD_SCALE, scale, SIZE_STEP))
                    m_Editor.SetSize(e, scale, scale);
            }
        }
    }

    // -- Shape2D ------------------------------------------------------------------------------
    if (isShape && Section("Shape2D", Editor::ObjectKindName(m_Editor.KindOf(e)), false, x, width, removed))
    {
        Shape2D shape = ECS.GetComponent<Shape2D>(e);
        bool round = shape.Type == Shape2DType::Circle || shape.Type == Shape2DType::Polygon;
        float w = shape.Width, h = shape.Height, thick = shape.Thickness, sides = static_cast<float>(shape.Sides);
        if (Row(WIDGET_H, y) && NumberRow(ix, y, iw, round ? "Size" : "Width", ID_FIELD_WIDTH, w, SIZE_STEP))
            m_Editor.SetSize(e, w, shape.Height);
        if (!round && Row(WIDGET_H, y) && NumberRow(ix, y, iw, "Height", ID_FIELD_HEIGHT, h, SIZE_STEP))
            m_Editor.SetSize(e, shape.Width, h);
        if (shape.Type == Shape2DType::Polygon && Row(WIDGET_H, y) &&
            NumberRow(ix, y, iw, "Sides", ID_FIELD_SIDES, sides, 1.0f, "%.0f"))
            m_Editor.SetSides(e, static_cast<int>(std::lround(sides)));
        if (!isField && Row(WIDGET_H, y) && NumberRow(ix, y, iw, "Thick", ID_FIELD_THICK, thick, 0.05f))
            m_Editor.SetThickness(e, thick);
        if (Row(WIDGET_H, y))
        {
            Text(ix, RowY(y), "Color", TEXT, LABEL_W - 4.0f);
            float step = std::min(SWATCH + 3.0f, (iw - LABEL_W) / static_cast<float>(COLOR_COUNT));
            for (int i = 0; i < COLOR_COUNT; ++i)
            {
                float sx = ix + LABEL_W + i * step;
                if (ColorSwatch(NextId(), sx, y + 2.0f, step - 3.0f, ToColor(COLORS[i]), SameColor(shape.Color, COLORS[i]),
                                *m_UI))
                    m_Editor.SetColor(e, COLORS[i]);
            }
        }
    }

    // -- Mesh ------------------------------------------------------------------------------------
    if (isModel)
    {
        const std::string model = ECS.GetComponent<Mesh>(e).Model;
        if (Section("Mesh", model, false, x, width, removed) && !m_Models.empty() && Row(WIDGET_H, y) &&
            (d = Stepper(ix, y, iw, "Model " + model, "<", ">")) != 0)
        {
            int count = static_cast<int>(m_Models.size());
            m_Editor.SetModel(e, m_Models[Cycle(IndexOf(m_Models, model), d, count)]);
        }
    }
    if (isField)
        return;

    // -- Shader (FragShaderTag / VertShaderTag) ---------------------------------------------
    if (isShape || isModel)
    {
        FragShaderTypeID fragment = SceneObjects::FragmentShaderOf(e);
        VertShaderTypeID vertex = SceneObjects::VertexShaderOf(e);
        std::string summary = ShaderLibrary::Name(fragment);
        if (vertex != DefaultVertShaderID)
            summary += " + " + ShaderLibrary::Name(vertex);
        if (Section("Shader", summary, false, x, width, removed))
        {
            const auto& fragments = ShaderLibrary::FragmentShaders();
            const auto& vertices = ShaderLibrary::VertexShaders();
            if (Row(WIDGET_H, y) && (d = Stepper(ix, y, iw, "Frag " + ShaderLibrary::Name(fragment), "<", ">")) != 0)
            {
                int count = static_cast<int>(fragments.size());
                int index = 0;
                for (int i = 0; i < count; ++i)
                    if (fragments[static_cast<std::size_t>(i)].Value == fragment)
                        index = i;
                const auto& next = fragments[static_cast<std::size_t>(Cycle(index, d, count))];
                m_Editor.SetFragmentShader(e, next.Value);
                SetStatus(std::string("Fragment shader ") + next.Name + ": " + next.Description);
            }
            if (Row(WIDGET_H, y) && (d = Stepper(ix, y, iw, "Vert " + ShaderLibrary::Name(vertex), "<", ">")) != 0)
            {
                int count = static_cast<int>(vertices.size());
                const auto& next =
                        vertices[static_cast<std::size_t>(Cycle(static_cast<int>(vertex), d, count))];
                m_Editor.SetVertexShader(e, next.Value);
                SetStatus(std::string("Vertex shader ") + next.Name + ": " + next.Description);
            }
        }
    }

    // -- RigidBody -------------------------------------------------------------------------------
    BodyType body = SceneObjects::GetBodyType(e);
    if (body != BodyType::None)
    {
        bool open = Section(Editor::SceneEditor::COMPONENT_RIGIDBODY, SceneObjects::BodyTypeName(body), true, x, width,
                            removed);
        if (removed)
        {
            m_Editor.RemoveComponent(e, Editor::SceneEditor::COMPONENT_RIGIDBODY);
            SetStatus("Removed RigidBody");
            return;
        }
        // Static / Dynamic / Trigger (None is Remove)
        if (open && Row(WIDGET_H, y) &&
            (d = Stepper(ix, y, iw, std::string("Body ") + SceneObjects::BodyTypeName(body), "<", ">")) != 0)
        {
            int count = static_cast<int>(BodyType::Count) - 1;
            int index = Cycle(static_cast<int>(body) - 1, d, count);
            m_Editor.SetBody(e, static_cast<BodyType>(index + 1));
        }
    }

    // -- Script ------------------------------------------------------------------------------------
    std::string script = m_Editor.GetScript(e);
    if (!script.empty())
    {
        bool open = Section(Editor::SceneEditor::COMPONENT_SCRIPT, script, true, x, width, removed);
        if (removed)
        {
            m_Editor.RemoveComponent(e, Editor::SceneEditor::COMPONENT_SCRIPT);
            SetStatus("Removed Script");
            return;
        }
        if (open)
        {
            std::vector<std::string> scripts = ScriptRegistry::Get().Names(false);
            if (Row(WIDGET_H, y) && (d = Stepper(ix, y, iw, "Script " + script, "<", ">")) != 0 && !scripts.empty())
            {
                int count = static_cast<int>(scripts.size());
                m_Editor.SetScript(e, scripts[Cycle(IndexOf(scripts, script), d, count)]);
            }
            if (const ScriptInfo* info = ScriptRegistry::Get().Find(script))
            {
                int index = 0;
                for (const ScriptParam& param : info->Params)
                {
                    if (index >= MAX_PARAM_FIELDS)
                        break;
                    float value = m_Editor.GetScriptParam(e, param.Name);
                    if (Row(WIDGET_H, y) &&
                        NumberRow(ix, y, iw, param.Name, ID_FIELD_PARAM + index, value, param.Step))
                        m_Editor.SetScriptParam(e, param.Name, value);
                    ++index;
                }
            }
        }
    }

    // -- The project's components (reflected) ------------------------------------------------------
    for (const ComponentEntry& entry : ComponentCatalog::Get().Entries())
    {
        if (!entry.Has(ECS, e))
            continue;
        bool open = Section(entry.Name, "", true, x, width, removed);
        if (removed)
        {
            if (m_Editor.RemoveComponent(e, entry.Name))
                SetStatus("Removed " + entry.Name);
            return;
        }
        for (const Reflection::FieldInfo& field : entry.Type->Fields)
        {
            if (open && !field.Hidden)
                RenderField(e, entry.Name, field, ix, iw);
        }
    }

    // -- Add Component, Duplicate / Delete --------------------------------------------------------------
    m_RowCursor -= 6.0f;
    if (Row(BUTTON_H, y) && Button(NextId(), x, y, *m_UI, width, BUTTON_H, "Add Component"))
    {
        std::vector<MenuItem> items;
        for (const std::string& name : m_Editor.AddableComponents(e))
        {
            items.push_back({name, [this, e, name] {
                                 if (m_Editor.AddComponent(e, name))
                                 {
                                     m_Folded.erase(name);
                                     SetStatus("Added " + name);
                                 }
                                 else
                                     SetStatus("Can not add " + name, true);
                             }});
        }
        if (items.empty())
            items.push_back({"Nothing left to add", nullptr, {}, false});
        m_Menu.Open(x, y, items);
    }
    if (Row(BUTTON_H, y))
    {
        float half = (width - 6.0f) * 0.5f;
        if (Button(NextId(), x, y, *m_UI, half, BUTTON_H, "Duplicate"))
            DuplicateSelected();
        if (Button(NextId(), x + half + 6.0f, y, *m_UI, half, BUTTON_H, "Delete"))
            DeleteSelected();
    }
}

void SceneEditorScene::RenderField(
        Entity e, const std::string& component, const Reflection::FieldInfo& field, float x, float width)
{
    using Reflection::FieldType;
    using Reflection::FieldValue;
    FieldValue value;
    if (!m_Editor.GetField(e, component, field.Name, value))
        return;
    int rows = field.Type == FieldType::Vec2 ? 2 : (field.Type == FieldType::Vec3 ? 3 : 1);
    // Text box ids are taken whether the rows are visible or not: they stay
    // the same while the inspector scrolls
    int ids[3] = {0, 0, 0};
    bool typed = field.Type == FieldType::Int || field.Type == FieldType::Float || field.Type == FieldType::String ||
                 field.Type == FieldType::Entity || field.Type == FieldType::Vec2 || field.Type == FieldType::Vec3;
    if (typed && !field.ReadOnly)
    {
        for (int i = 0; i < rows; ++i)
            ids[i] = NextFieldId();
    }

    const std::string& label = field.Label;
    auto set = [&](const FieldValue& newValue) {
        if (!m_Editor.SetField(e, component, field.Name, newValue))
            SetStatus("Can not set " + field.Label + " to " + Reflection::ToString(newValue), true);
    };
    auto entityName = [&](const FieldValue& v) {
        auto target = static_cast<Entity>(std::get<std::int64_t>(v));
        return m_Editor.IsObject(target) ? (m_Editor.IsField(target) ? std::string("Field") : m_Editor.NameOf(target))
                                         : std::string("-");
    };
    auto tooltip = [&](float rowY) {
        // Hovering a field shows its tooltip in the status bar
        if (!field.Tooltip.empty() && Inside(m_UI->mouseX, m_UI->mouseY, x, rowY, width, WIDGET_H))
            m_Hint = field.Label + ": " + field.Tooltip;
    };

    float y = 0.0f;
    if (field.ReadOnly)
    {
        if (!Row(WIDGET_H, y))
            return;
        tooltip(y);
        std::string shown = field.Type == FieldType::Enum     ? field.OptionName(std::get<std::int64_t>(value))
                            : field.Type == FieldType::Entity ? entityName(value)
                                                              : Reflection::ToString(value);
        Text(x, RowY(y), label, TEXT_DIM, LABEL_W - 4.0f);
        Text(x + LABEL_W, RowY(y), shown, TEXT_DIM, width - LABEL_W);
        return;
    }

    if (field.Type == FieldType::Vec2 || field.Type == FieldType::Vec3)
    {
        bool three = field.Type == FieldType::Vec3;
        Vec3 v = three ? std::get<Vec3>(value) : Vec3(std::get<Vec2>(value).X, std::get<Vec2>(value).Y, 0.0f);
        float* axes[3] = {&v.X, &v.Y, &v.Z};
        const char* names[3] = {"X", "Y", "Z"};
        auto step = static_cast<float>(field.StepOrDefault());
        const char* format = step < 0.01f ? "%.3f" : "%.2f";
        bool changed = false;
        for (int i = 0; i < rows; ++i)
        {
            if (!Row(WIDGET_H, y))
                continue;
            tooltip(y);
            if (NumberRow(x, y, width, field.Label + " " + names[i], ids[i], *axes[i], step, format))
                changed = true;
        }
        if (changed)
            set(three ? FieldValue(v) : FieldValue(Vec2(v.X, v.Y)));
        return;
    }

    if (!Row(WIDGET_H, y))
        return;
    tooltip(y);
    switch (field.Type)
    {
    case FieldType::Bool: {
        bool b = std::get<bool>(value);
        Text(x, RowY(y), label, TEXT, LABEL_W - 4.0f);
        if (CheckBox(NextId(), x + LABEL_W, y + 3.0f, b, 16.0f, *m_UI))
            set(!b);
        break;
    }
    case FieldType::Int:
    case FieldType::Float: {
        bool isInt = field.Type == FieldType::Int;
        double number = isInt ? static_cast<double>(std::get<std::int64_t>(value)) : std::get<double>(value);
        auto shown = static_cast<float>(number);
        double step = field.StepOrDefault();
        const char* format = isInt ? "%.0f" : (step < 0.01 ? "%.3f" : "%.2f");
        if (NumberRow(x, y, width, label, ids[0], shown, static_cast<float>(step), format))
            set(static_cast<double>(shown));
        break;
    }
    case FieldType::String: {
        std::string text = std::get<std::string>(value);
        Text(x, RowY(y), label, TEXT, LABEL_W - 4.0f);
        FieldDrawn(ids[0]);
        if (TextField(ids[0], x + LABEL_W, y, width - LABEL_W, WIDGET_H, *m_UI, text, TextFilter::Any, 64) ==
                    TextFieldEvent::Committed &&
            text != std::get<std::string>(value))
            set(text);
        break;
    }
    case FieldType::Entity: {
        // Type the name of the object to point at ("-" or empty = none)
        std::string text = entityName(value);
        std::string before = text;
        Text(x, RowY(y), label, TEXT, LABEL_W - 4.0f);
        FieldDrawn(ids[0]);
        if (TextField(ids[0], x + LABEL_W, y, width - LABEL_W, WIDGET_H, *m_UI, text, TextFilter::Any, 64) ==
                    TextFieldEvent::Committed &&
            text != before)
        {
            if (text.empty() || text == "-")
                set(static_cast<std::int64_t>(NULL_ENTITY));
            else if (Entity target = SceneObjects::FindByName(text); target != NULL_ENTITY)
                set(static_cast<std::int64_t>(target));
            else
                SetStatus("No object named " + text, true);
        }
        break;
    }
    case FieldType::Enum: {
        std::int64_t current = std::get<std::int64_t>(value);
        int d = Stepper(x, y, width, label + " " + field.OptionName(current), "<", ">");
        if (d != 0)
        {
            auto count = static_cast<int>(field.EnumMax - field.EnumMin + 1);
            int index = Cycle(static_cast<int>(current - field.EnumMin), d, count);
            set(static_cast<std::int64_t>(field.EnumMin + index));
        }
        break;
    }
    case FieldType::Color: {
        Vec3 color = std::get<Vec3>(value);
        Text(x, RowY(y), label, TEXT, LABEL_W - 4.0f);
        float step = std::min(SWATCH + 3.0f, (width - LABEL_W) / static_cast<float>(COLOR_COUNT));
        for (int i = 0; i < COLOR_COUNT; ++i)
        {
            float sx = x + LABEL_W + i * step;
            if (ColorSwatch(NextId(), sx, y + 2.0f, step - 3.0f, ToColor(COLORS[i]), SameColor(color, COLORS[i]), *m_UI))
                set(COLORS[i]);
        }
        break;
    }
    default:
        break;
    }
}

//-----------------------------------------------------------------------------
// Scene settings
//-----------------------------------------------------------------------------

void SceneEditorScene::RenderSceneInspector(float x, float width)
{
    auto settings = ECS.GetResource<SceneSettings>();
    float y = 0.0f;
    int d = 0;
    if (Row(WIDGET_H, y))
        Text(x, RowY(y), "SCENE", ACCENT, width);

    std::vector<std::string> scripts = WithNone(ScriptRegistry::Get().Names(true));
    std::string script = settings->SceneScript;
    std::string scriptLabel = script.empty() ? "-" : script;
    if (Row(WIDGET_H, y) && (d = Stepper(x, y, width, "Script " + scriptLabel, "<", ">")) != 0)
    {
        int count = static_cast<int>(scripts.size());
        m_Editor.SetSceneScript(scripts[Cycle(IndexOf(scripts, script), d, count)]);
    }
    if (const ScriptInfo* info = ScriptRegistry::Get().Find(settings->SceneScript))
    {
        int index = 0;
        for (const ScriptParam& param : info->Params)
        {
            if (index >= MAX_PARAM_FIELDS)
                break;
            float value = m_Editor.GetSceneParam(param.Name);
            if (Row(WIDGET_H, y) &&
                NumberRow(x + SECTION_INDENT, y, width - SECTION_INDENT, param.Name, ID_FIELD_SCENE_PARAM + index,
                          value, param.Step))
                m_Editor.SetSceneParam(param.Name, value);
            ++index;
        }
    }

    m_RowCursor -= 6.0f;
    float fieldW = settings->FieldWidth;
    float fieldH = settings->FieldHeight;
    if (Row(WIDGET_H, y) && NumberRow(x, y, width, "Field W", ID_FIELD_FIELD_W, fieldW, 2.0f, "%.0f"))
        m_Editor.SetFieldSize(fieldW, settings->FieldHeight);
    if (Row(WIDGET_H, y) && NumberRow(x, y, width, "Field H", ID_FIELD_FIELD_H, fieldH, 2.0f, "%.0f"))
        m_Editor.SetFieldSize(settings->FieldWidth, fieldH);

    m_RowCursor -= 6.0f;
    if (Row(BUTTON_H, y) && Button(NextId(), x, y, *m_UI, width, BUTTON_H, "Game camera = view"))
    {
        // The camera object keeps its own field of view
        SceneCamera::View view = m_View;
        Entity existing = m_Editor.GameCameraObject();
        if (existing != NULL_ENTITY)
            view.FieldOfView = ECS.GetComponent<GameCamera>(existing).FieldOfView;
        Entity camera = m_Editor.SetGameCamera(view);
        SetStatus(m_Editor.NameOf(camera) + " now shows the current view");
    }

    // Scene files in plain text (readable / diffable) or binary
    m_RowCursor -= 6.0f;
    bool text = Serialization::WorldSerializer::FileFormat() == Serialization::SaveFormat::Text;
    if (Row(WIDGET_H, y) && CheckBox(NextId(), x, y + 3.0f, text, 16.0f, *m_UI, "Plain text files", width - 30.0f))
    {
        text = !text;
        Serialization::WorldSerializer::SetFileFormat(text ? Serialization::SaveFormat::Text
                                                           : Serialization::SaveFormat::Binary);
        SetStatus(text ? "Scenes are saved as plain text (Save to write it)" : "Scenes are saved as binary");
    }
    if (Row(WIDGET_H, y))
        Text(x, RowY(y), std::string("Snap to grid (G): ") + (m_Snap ? "on" : "off"), TEXT_DIM, width);

    m_RowCursor -= 6.0f;
    if (Row(WIDGET_H, y))
        Text(x, RowY(y), "Objects " + std::to_string(m_Editor.Objects().size()), TEXT_DIM, width);
    if (Row(WIDGET_H, y))
        Text(x, RowY(y), "Undo " + std::to_string(m_Editor.UndoCount()) + "  Redo " + std::to_string(m_Editor.RedoCount()),
             TEXT_DIM, width);
    std::vector<std::string> issues = m_Editor.Validate();
    if (issues.empty())
    {
        if (Row(WIDGET_H, y))
            Text(x, RowY(y), "Playable", TEXT_DIM, width);
        return;
    }
    if (Row(WIDGET_H, y))
        Text(x, RowY(y), std::to_string(issues.size()) + " problem(s):", ERROR_TEXT, width);
    if (Row(WIDGET_H, y))
        Text(x, RowY(y), issues.front(), ERROR_TEXT, width);
}
