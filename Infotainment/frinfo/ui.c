/**
 * @file ui.c
 */

#include "ui.h"

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "msg_handler.h"
#include "raygui.h"

#define ROTATION_90 90

static bool use_camera = false;

/**
 * Configure camera based on settings
 * @param config pointer to forsan raneaf config object
 * @param camera pointer to Camera2D object
 */
static void configure_camera(const struct frinfo_config *config, Camera2D *camera) {
    if (config->window_rotation == ROTATION_90) {
        camera->target = (Vector2){ ((float)config->window_width)/2.0f, ((float)config->window_height)/2.0f };
        camera->offset = (Vector2){ 80.0f, ((float)config->window_width)/2.0f }; // i'm not sure about the 80, but it works
        camera->zoom = 1.0f;
        camera->rotation = 90.0f;
        use_camera = true;
    }
}

/**
 * @brief main method of UI
 * @param frinfo const pointer to frinfo structure
 * @return void
 */
void ui_loop(struct frinfo *frinfo) {
    InitWindow(frinfo->config->window_width, frinfo->config->window_height, "Forsan Raneaf Infotainment");
    SetTargetFPS(60);

    Camera2D camera;
    configure_camera(frinfo->config, &camera);

    while (!WindowShouldClose() && !frinfo->shutdown)
    {
        msg_handler_handle(frinfo); // handle serial messages
        BeginDrawing();
        ClearBackground(RAYWHITE);
        if (use_camera)
            BeginMode2D(camera);
        if (frinfo->serial_connected) {
            DrawText("Connected to sensor network controller!", 150, 80, 20, DARKGREEN);
        } else {
            DrawText("Failed to connect to sensor network controller!", 110, 80, 20, RED);
        }
        // test button, I want bottom left of screen when horizontal
        //GuiButton((Rectangle){ 25, 665, 125, 30 }, GuiIconText(ICON_FILE_SAVE, "Save File"));
        //DrawRectangleLines( 10, 10, 630, 700, BLUE);
        DrawLine(1280/2, 10, 1280/2, 710, DARKGREEN);
        if (use_camera) {
            GuiButtonWorld(&camera, (Rectangle){ 25, 25, 125, 30 }, "Top Left");
            GuiButtonWorld(&camera, (Rectangle){ 25, 720 - 30 - 25, 125, 30 }, "Bottom Left");
            GuiButtonWorld(&camera, (Rectangle){ 1280 - 125 - 25, 25, 125, 30 }, "Top Right");
            GuiButtonWorld(&camera, (Rectangle){ 1280 - 125 - 25, 720 - 30 - 25, 125, 30 }, "Bottom Right");
        } else {
            GuiButton((Rectangle){ 25, 25, 125, 30 }, "Top Left");
            GuiButton((Rectangle){ 25, 720 - 30 - 25, 125, 30 }, "Bottom Left");
            GuiButton((Rectangle){ 1280 - 125 - 25, 25, 125, 30 }, "Top Right");
            GuiButton((Rectangle){ 1280 - 125 - 25, 720 - 30 - 25, 125, 30 }, "Bottom Right");
        }
        if (use_camera)
            EndMode2D();
        EndDrawing();
    }
    CloseWindow();
}
