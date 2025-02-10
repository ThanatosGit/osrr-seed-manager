#include <3ds.h>
#include <arpa/inet.h>
#include <citro2d.h>
#include <citro3d.h>
#include <errno.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "helpers.h"
#include "imgui/imgui.h"
#include "imgui/imgui_sw.h"
#include "server_thread.h"
#include "zip.h"

#define STACKSIZE (4 * 1024)
#define MAX_ENTIRES_PER_DIR 50

std::vector<char *> pixel_buffer();

const char *get_title_id(int selected_region) {
    if (selected_region == 0)
        return "00040000001BFB00";
    // else if (selected_region == 1)
    return "00040000001BB200";
}

void read_zip_files(std::vector<std::string> &zip_files) {
    while (zip_files.size() > 0) zip_files.pop_back();

    FS_Archive sd_archive;
    Handle handle_homebrew_path;
    u32 entries_read;
    FS_DirectoryEntry *entries = (FS_DirectoryEntry *)calloc(MAX_ENTIRES_PER_DIR, sizeof(FS_DirectoryEntry));

    FSUSER_OpenArchive(&sd_archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    FSUSER_OpenDirectory(&handle_homebrew_path, sd_archive, fsMakePath(PATH_ASCII, homebrew_path));
    FSDIR_Read(handle_homebrew_path, &entries_read, MAX_ENTIRES_PER_DIR, entries);
    FSDIR_Close(handle_homebrew_path);
    FSUSER_CloseArchive(sd_archive);

    for (u32 i = 0; i < entries_read; i++) {
        char temp_buffer[MAX_FILENAME];
        int output_length = utf16_to_utf8((uint8_t *)temp_buffer, entries[i].name, MAX_FILENAME);
        std::string temp_string = std::string(temp_buffer, output_length);
        temp_string.find(".zip", temp_string.size() - 3);
        bool found = (temp_string.find(".zip", temp_string.size() - 4)) != std::string::npos;
        if (found) zip_files.push_back(std::string(temp_buffer, output_length));
    }

    free(entries);
}

char *get_config_path() {
    int length = strlen(homebrew_path) + strlen(config_file) + 1;
    char *file_path = (char *)calloc(length, sizeof(char));
    snprintf(file_path, length, "%s%s", homebrew_path, config_file);

    return file_path;
}

int read_config() {
    char *file_path = get_config_path();

    FILE *file_handle = fopen(file_path, "r");
    free(file_path);
    if (file_handle == NULL) return 0;

    char ch = fgetc(file_handle);
    fclose(file_handle);
    return atoi((const char *)&ch);
}

void write_config(int selected_region) {
    char *file_path = get_config_path();

    FILE *file_handle = fopen(file_path, "w");
    free(file_path);
    if (file_handle == NULL) return;

    char buffer[2];
    snprintf(buffer, 2, "%d", selected_region);
    fwrite(buffer, 1, 1, file_handle);
    fclose(file_handle);
}

int main(int argc, char **argv) {
    u16 width = 320, height = 240;
    bool is_window_open = true;
    int selected_region = 0;
    std::vector<uint32_t> pixel_buffer(width * height, 0);
    std::vector<std::string> zip_files;
    bool open_popup = false;
    int open_popup_for = -1;

    // some init
    srvInit();
    fsInit();
    gfxInitDefault();
    atexit(gfxExit);
    // ensure homebrew path exists
    create_file_path_dirs(homebrew_path);
    read_zip_files(zip_files);
    selected_region = read_config();

    // top screen is just for logs
    consoleInit(GFX_TOP, NULL);
    printf(CONSOLE_GREEN);
    printf("Please note that the input is disabled while unpackinig a seed!\n");
    printf(CONSOLE_RESET);

    // lower screen for menu
    C3D_Tex *tex = (C3D_Tex *)malloc(sizeof(C3D_Tex));
    static const Tex3DS_SubTexture subt3x = {512, 256, 0.0f, 1.0f, 1.0f, 0.0f};
    C2D_Image image = (C2D_Image){tex, &subt3x};
    C3D_TexInit(image.tex, 512, 256, GPU_RGBA8);
    C3D_TexSetFilter(image.tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetFilter(image.tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(image.tex, GPU_REPEAT, GPU_REPEAT);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)width, (float)height);
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.MouseDrawCursor = true;
    imgui_sw::bind_imgui_painting();
    imgui_sw::SwOptions sw_options;
    imgui_sw::make_style_fast();
    touchPosition touch;

    // create thread
    s32 prio = 0;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    // The priority of these child threads must be higher (aka the value is
    // lower) than that of the main thread, otherwise there is thread
    // starvation due to stdio being locked.
    server_thread = threadCreate(server_thread_main, NULL, STACKSIZE, prio - 1, -2, false);

    // Main loop
    while (aptMainLoop()) {
        gspWaitForVBlank();
        io.DeltaTime = 1.0f / 60.0f;

        extern uint8_t new_data;
        if (new_data) {
            read_zip_files(zip_files);
            new_data = 0;
        }

        hidScanInput();
        u32 kDown = keysDown();
        if (kDown & KEY_B) {
            get_title_id(selected_region);
            // aptSetChainloader(0x00040000001BFB00, 0);  // *EUR* camera app title ID
            break;
        };  // break in order to return to hbmenu

        ImGui::NewFrame();

        // region selection ui
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 10), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("#combo", &is_window_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
        if (ImGui::RadioButton("PAL", &selected_region, 0))
            write_config(selected_region);
        ImGui::SameLine();
        if (ImGui::RadioButton("NTSC-U", &selected_region, 1))
            write_config(selected_region);
        ImGui::End();

        // seed selection ui
        ImGui::SetNextWindowPos(ImVec2(10, 50.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(io.DisplaySize.x - 20, 180), ImVec2(io.DisplaySize.x - 20, 180));
        ImGui::Begin("#seeds", &is_window_open, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
        ImVec2 current_pos = ImGui::GetWindowPos();
        current_pos.x = 0.0;
        ImGui::SetNextWindowPos(current_pos);
        for (u32 i = 0; i < zip_files.size(); i++) {
            if (ImGui::Selectable(zip_files.at(i).c_str())) {
                open_popup = true;
                open_popup_for = i;
            }
        }

        // popup ui
        if (open_popup) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::OpenPopup("Apply seed?");
            if (ImGui::BeginPopupModal("Apply seed?")) {
                ImGui::Text("Apply seed: %s", zip_files.at(open_popup_for).c_str());
                if (ImGui::Button("Yes")) {
                    unzip(zip_files.at(open_popup_for).c_str(), get_title_id(selected_region));
                    open_popup = false;
                }
                ImGui::SameLine(0.0f, 50.0f);
                if (ImGui::Button("No")) open_popup = false;
                ImGui::EndPopup();
            }
        }

        ImGui::End();

        ImGui::Render();

        memset(io.NavInputs, 0, sizeof(io.NavInputs));
        hidTouchRead(&touch);
        if (touch.px && touch.py) {
            io.MouseDown[0] = true;
            io.MousePos = ImVec2(touch.px, touch.py);
        } else
            io.MouseDown[0] = false;

        // fill gray (this could be any previous rendering)
        std::fill_n(pixel_buffer.data(), width * height, 0x19191919u);

        // overlay the GUI
        paint_imgui(pixel_buffer.data(), width, height, sw_options);
        for (u32 x = 0; x < width; x++) {
            for (u32 y = 0; y < height; y++) {
                u32 dstPos = ((((y >> 3) * (512 >> 3) + (x >> 3)) << 6) + ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3))) * 4;
                u32 srcPos = (y * width + x) * 4;
                memcpy(&((u8 *)image.tex->data)[dstPos], &((u8 *)pixel_buffer.data())[srcPos], 4);
            }
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(bottom, C2D_Color32(32, 38, 100, 0xaa));
        C2D_SceneBegin(bottom);
        C2D_DrawImageAt(image, 0.0f, 0.0f, 0.0f, NULL, 1.0f, 1.0f);
        C3D_FrameEnd(0);
    }

    // tell threads to exit & wait for them to exit
    if (server_run) {
        server_run = false;
        threadJoin(server_thread, 2 * 1000 * 1000 * 1000);
        threadFree(server_thread);
    }

    imgui_sw::unbind_imgui_painting();
    ImGui::DestroyContext();
    gfxExit();
    fsExit();
    srvExit();
    free(tex);

    return 0;
}