#pragma once
#include "AABB.h"
#include "Entity.h"
#include "Map.h"

#include <cassert>
#include <memory>
#include <utility>
#include <vector>

enum Status
{
    Success,
    Failure,
    Running
};

struct BehaviorTree
{
};

struct Node
{

    Status CurrentStatus;
    size_t CurrentChild;
    std::vector<std::shared_ptr<Node>> Children;

    Node() = default;

    Node(const Node&) = delete;

    Node& operator=(const Node&) = delete;

    virtual ~Node() = default;

    void AddChild(std::shared_ptr<Node> child) { Children.push_back(child); }

    // Called every tick of the behaviour tree
    // Used to check abort conditions for priority selector node
    virtual bool CheckCondition() { return true; }

    // Clean up node when aborted
    virtual void Abort() {}

    virtual Status Process() { return Success; }

    // Reset the tree to a clean slate
    virtual void Clear()
    {
        for (auto child : Children)
        {
            child->Clear();
        }
        CurrentChild = 0;
    }

    virtual void Render() {}
    // Used for debugging
    virtual std::string GetRunning() { return ""; }
};

// A concurrentNode executes every child simultaneously
struct ConcurrentNode : Node
{
    Status Process() override
    {
        for (auto child : Children)
            child->Process();
        return Running;
    }
};

// A sequence Node will visit each child in order and return
// success when all its children return success
struct SequenceNode : Node
{
    Status Process() override
    {
        if (CurrentChild == Children.size())
        {
            CurrentChild = 0;
            return Success;
        }

        switch (Children[CurrentChild]->Process())
        {
        case Success: {
            CurrentChild++;
            return Running;
        }
        case Failure: {
            CurrentChild = 0;
            return Failure;
        }
        case Running:
            return Running;
        }
        assert(false && "unreachable");
    }

    void Abort() override { CurrentChild = 0; }

    std::string GetRunning() override
    {
        size_t Child = CurrentChild == Children.size() ? 0 : CurrentChild;
        return "Sequence Node (child: " + std::to_string(CurrentChild) + ") > " +
               Children[Child]->GetRunning();
    }
};

// A selector priority selector node that always
// Prioritize the child that in order of Child index
struct PrioritySelectorNode : Node
{
    Status Process() override
    {

        // 1. Check all children in priority order:
        for (size_t i = 0; i < Children.size(); ++i)
        {
            // Evaluate your "activation condition" for child i.
            if (Children[i]->CheckCondition())
            {
                // 2. If a higher-priority child is now valid and it's not the one currently
                // running, abort the old one and switch to new one.
                if (i != CurrentChild)
                {
                    if (CurrentChild >= 0)
                    {
                        Children[CurrentChild]->Abort();
                    }
                    CurrentChild = i;
                }
                // 3. Tick the chosen child.
                return Children[CurrentChild]->Process();
            }
        }
        // If no child can run, return Failure
        return Failure;
    }

    std::string GetRunning() override
    {
        size_t Child = CurrentChild == Children.size() ? 0 : CurrentChild;
        return "SelectorNode Node (child: " + std::to_string(CurrentChild) + ") > " +
               Children[Child]->GetRunning();
    }
};

// RepeatNode repeatedly call the child always returns running
struct RepeatNode : Node
{

    RepeatNode() = default;

    Status Process() override
    {
        assert(Children.size() == 1 && "Repeat Node must have 1 children");
        Children[0]->Process();
        return Running;
    }

    std::string GetRunning() override { return "RepeatNode Node > " + Children[0]->GetRunning(); }
};

// LeafNode navigates an entity towards a player target
// Relies on VectorField
struct VectorFieldNode : Node
{
    Entity WalkEntity;
    VectorField& Field;
    // How close you get before you stop
    float StopDistance = 1.0f;
    // How far is too far before we find another path
    float TooFarThreshold = 0.1f;
    // The speed is determined by how dense the vector field is
    float Speed = 0.003f;
    // bigger turn speed = faster the turn
    float TurnSpeed = 0.002f;
    MapLoc* PrevLoc = nullptr;

    VectorFieldNode(VectorField& Field, Entity WalkEntity)
        : WalkEntity(WalkEntity)
        , Field(Field)
    {
    }

    Status Process() override;

    std::string GetRunning() override { return "Vector Field Leaf"; }
};

struct PlayVoiceLine : Node
{
    PlayVoiceLine(std::string VoiceLine)
        : VoiceLine(VoiceLine)
    {
    }

    Status Process() override;

    std::string VoiceLine;
};

// LeafNode select a Random point within a designated box and
// Puts it into PatrolPoint Database
struct SelectRandomPatrolPoint : Node
{
    SelectRandomPatrolPoint(VectorField& Field, Entity WalkEntity, AABB Box)
        : WalkEntity(WalkEntity)
        , Field(Field)
        , PatrolBox(std::move(Box))
    {
    }

    Status Process() override;

    std::string GetRunning() override { return "SelectRandomPatrolPoint"; }

    void Clear() override;

    // Select points atleast this far from entity
    std::vector<MapLoc*> PotentialLocations;
    float Distance = 4.0f;
    Entity WalkEntity;
    VectorField& Field;
    AABB PatrolBox;
};

struct NavigateToPoint : Node
{
    NavigateToPoint(Entity WalkEntity, VectorField& Field)
        : WalkEntity(WalkEntity)
        , Field(Field)
    {
    }

    void Clear() override { PatrolPath.clear(); }
    float moveSpeed = 0.001f;
    float TurnSpeed = 0.002f;
    // how close is "close enough" to the next waypoint
    float closeEnoughThreshold = 0.5f;
    // how far is "too far" to recompute another path
    float tooFarThreshold = 2.0f;

    Status Process() override;
    std::vector<MapLoc> PatrolPath;
    bool TargetCanMove = false;
    Vec3 TargetPos{};
    Entity WalkEntity;
    VectorField& Field;
};

// Execute Children in a certain amount of time
struct ExecuteChildInTime : Node
{
    ExecuteChildInTime(float time)
        : Time(time)
    {
    }

    Status Process() override;

    void Clear() override { CurrentTime = -1.0f; }
    float Time;
    float CurrentTime = -1.0f;
};

// LeafNode Navigate to the Patrol Point
struct NavigateToPatrolPoint : NavigateToPoint
{
    NavigateToPatrolPoint(Entity WalkEntity, VectorField& Field)
        : NavigateToPoint(WalkEntity, Field)
    {
    }

    Status Process() override;

    std::string GetRunning() override { return "NavigateToPatrolPoint Leaf"; }
};

struct Wait : Node
{
    Wait(float Time)
        : Cooldown(Time)
    {
    }

    Status Process() override;

    std::string GetRunning() override { return "Wait Leaf"; }
    void Clear() override { CurrentTime = -1.0f; }

    float CurrentTime = -1.0f;
    float Cooldown;
};