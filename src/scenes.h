#ifndef RAYRENDER_SCENES_H
#define RAYRENDER_SCENES_H

#include "common.h"

void Maze_Init(int quality, bool bilinear);
void Maze_Rebuild(int quality, bool bilinear);
void Maze_ApplyFilter(bool bilinear);
void Maze_Update(void);
void Maze_Draw(AppState *app);
void Maze_DrawOverlay(void);
void Maze_Shutdown(void);

void Viewer_Init(int quality, bool bilinear);
void Viewer_Rebuild(int quality, bool bilinear);
void Viewer_ApplyFilter(bool bilinear);
void Viewer_Update(void);
void Viewer_Draw(AppState *app);
void Viewer_DrawOverlay(void);
void Viewer_Shutdown(void);

void Stress_Init(int quality, bool bilinear);
void Stress_Rebuild(int quality, bool bilinear);
void Stress_ApplyFilter(bool bilinear);
void Stress_Update(void);
void Stress_Draw(AppState *app);
void Stress_DrawOverlay(void);
void Stress_Shutdown(void);

#endif
