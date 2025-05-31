/**
 * @file ui.c
 */

#include "ui.h"

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

/**
 * @brief main method of UI
 * @param frinfo const pointer to frinfo structure
 * @return void
 */
void ui_loop(const struct frinfo *frinfo) {
    InitWindow(frinfo->config->window_width, frinfo->config->window_height, "Forsan Raneaf Infotainment");
    SetTargetFPS(60);
    while (!WindowShouldClose() && !frinfo->shutdown)
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Congrats! You created your first window!", 190, 200, 20, DARKGRAY);
        EndDrawing();
    }
    CloseWindow();
}
