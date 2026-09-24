//---------------------------------------------------------------------------------
// Scene.h
//---------------------------------------------------------------------------------
//
// A Scene represents a discrete chunk in the game such as a level or a menu
// every Scene is the game must subclass this class
//
#pragma once

class Scene
{
  public:
    Scene() = default;

    virtual ~Scene() = default;

    /**
     * \brief Start is called once onm start up of this is used to register any
     *        Resources and setup any Systems a Scene might need
     */
    virtual void Start() {}

    /**
     * \brief Setup is called every time
     */
    virtual void Setup() {}

    /**
     * \brief calls every iteration of the game loop
     * \param deltaTime time in ms between this frame and the previous
     */
    virtual void Update(float deltaTime) {}

    /**
     * \brief called after pipeline render is done use this to render some
     *        custom graphics you need
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
