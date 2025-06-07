/**
 * @file ui.c
 */

#include "ui.h"

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "msg_handler.h"
#include "raygui.h"

/**
 * @brief main method of UI
 * @param frinfo const pointer to frinfo structure
 * @return void
 */
void ui_loop(struct frinfo *frinfo) {
    InitWindow(frinfo->config->window_width, frinfo->config->window_height, "Forsan Raneaf Infotainment");
    SetTargetFPS(60);

    Camera2D camera = { 0 };
    camera.target = (Vector2){ 720.0f/2.0f, 1280.0f/2.0f };
    camera.offset = (Vector2){ 80.0f, 360.0f};
    camera.rotation = 90.0f;
    camera.zoom = 1.0f;

    while (!WindowShouldClose() && !frinfo->shutdown)
    {
        msg_handler_handle(frinfo); // handle serial messages
        BeginDrawing();
        ClearBackground(RAYWHITE);
        //BeginMode2D(camera);
        if (frinfo->serial_connected) {
            DrawText("Connected to sensor network controller!", 150, 80, 20, DARKGREEN);
        } else {
            DrawText("Failed to connect to sensor network controller!", 110, 80, 20, RED);
        }
        // test button, I want bottom left of screen when horizontal
        //GuiButton((Rectangle){ 25, 665, 125, 30 }, GuiIconText(ICON_FILE_SAVE, "Save File"));
        //DrawRectangleLines( 10, 10, 630, 700, BLUE);
        DrawLine(1280/2, 10, 1280/2, 710, DARKGREEN);
        GuiButton((Rectangle){ 25, 720 - 25 - 30, 125, 30 }, GuiIconText(ICON_FILE_SAVE, "Save File"));
        //EndMode2D();
        EndDrawing();
    }
    CloseWindow();
}
