#pragma once
#include "Entity.h"

#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

enum Status
{
    Success,
    Failure,
    Running
};

// Time step used by time based nodes (Wait). The owner of a tree (usually a
// script) sets it before ticking the tree each frame
struct BehaviorTreeClock
{
    static inline float DeltaSeconds = 0.0f;
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

// Succeeds once Cooldown seconds have elapsed, Running until then
struct Wait : Node
{
    Wait(float Time)
        : Cooldown(Time)
    {
    }

    Status Process() override
    {
        if (CurrentTime < 0.0f)
            CurrentTime = Cooldown;
        CurrentTime -= BehaviorTreeClock::DeltaSeconds;
        if (CurrentTime > 0.0f)
            return Running;
        CurrentTime = -1.0f;
        return Success;
    }

    std::string GetRunning() override { return "Wait Leaf"; }
    void Clear() override { CurrentTime = -1.0f; }

    float CurrentTime = -1.0f;
    float Cooldown;
};
