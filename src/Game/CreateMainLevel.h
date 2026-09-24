#pragma once

void CreateMainLevel();

/**
 * \brief Rebuild the runtime state of the main level that is derived from the
 *        saved world and therefore not stored in save files: AI vector field
 *        grids + obstacles, the unit selector target and every behaviour tree.
 *        Called after a save file was loaded
 */
void RestoreMainLevelRuntimeState();
