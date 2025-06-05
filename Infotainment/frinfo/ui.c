/**
 * @file ui.c
 */

#include "ui.h"

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "msg_handler.h"
#include "raygui.h"
#include "../utilities/defines.h"
#include "../utilities/logger.h"

/**
 * @brief main method of UI
 * @param frinfo const pointer to frinfo structure
 * @return void
 */
void ui_loop(struct frinfo *frinfo) {
    InitWindow(frinfo->config->window_width, frinfo->config->window_height, "Forsan Raneaf Infotainment");
    SetTargetFPS(60);
    while (!WindowShouldClose() && !frinfo->shutdown)
    {
        msg_handler_handle(frinfo); // handle serial messages
        BeginDrawing();
        ClearBackground(RAYWHITE);
        if (frinfo->serial_connected) {
            DrawText("Connected to sensor network controller!", 150, 80, 20, DARKGREEN);
        } else {
            DrawText("Failed to connect to sensor network controller!", 110, 80, 20, RED);
        }
        EndDrawing();
    }
    CloseWindow();
}
