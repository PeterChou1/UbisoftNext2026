//---------------------------------------------------------------------------------
// Scene.h
//---------------------------------------------------------------------------------
//
// A Scene represents a discrete chunk of the game such as a level or a menu.
// Every Scene of the game subclasses this class
//
#pragma once

class Scene
{
  public:
    virtual ~Scene() = default;

    /**
     * \brief Called once, when the scene is registered: register any
     *        Resources and set up any Systems the Scene needs
     */
    virtual void Start() {}

    /**
     * \brief Called every time the scene becomes the active one (on a
     *        freshly reset ECS)
     */
    virtual void Setup() {}

    /**
     * \brief Called every iteration of the game loop
     * \param deltaTime time in ms between this frame and the previous
     */
    virtual void Update(float deltaTime) {}

    /**
     * \brief Called after the render pipeline is done: draw any custom
     *        graphics (UI) here
     */
    virtual void Render() {}

    /**
     * \brief Called after a save file replaced the world of this scene
     *        (Setup has already run). Use this to rebuild runtime only state
     *        that is not stored in save files (behaviour trees, caches ...)
     */
    virtual void OnWorldRestored() {}

    /**
     * \brief Whether the gameplay simulation (physics, particles, AI) runs
     *        while this scene is active. Editors return false to keep the
     *        authored world frozen; rendering and UI always run
     */
    virtual bool SimulatesWorld() const { return true; }
};
