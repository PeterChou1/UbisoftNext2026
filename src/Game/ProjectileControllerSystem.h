#include "BlackBoard.h"
#include "Bullet.h"
#include "Camera.h"
#include "GameState.h"
#include "UIState.h"

class ProjectileControllerSystem
{
  public:
    ProjectileControllerSystem();

    void Update(float deltaTime);

  private:
    std::shared_ptr<HandleTankProjectiles> m_TankProjectileHandle;
};
