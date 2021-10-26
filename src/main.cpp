#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "imgui_memory_editor.h"

#define VX V[(opcode & 0x0F00) >> 8]
#define VY V[(opcode & 0x00F0) >> 4]


static int keymap[0x10] = {
    SDLK_0,
    SDLK_1,
    SDLK_2,
    SDLK_3,
    SDLK_4,
    SDLK_5,
    SDLK_6,
    SDLK_7,
    SDLK_8,
    SDLK_9,
    SDLK_a,
    SDLK_b,
    SDLK_c,
    SDLK_d,
    SDLK_e,
    SDLK_f
};

unsigned char chip8_fontset[80] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
    0x90, 0x90, 0xF0, 0x10, 0x10, //4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
    0xF0, 0x10, 0x20, 0x40, 0x40, //7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
    0xF0, 0x90, 0xF0, 0x90, 0x90, //A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
    0xF0, 0x80, 0x80, 0x80, 0xF0, //C
    0xE0, 0x90, 0x90, 0x90, 0xE0, //D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
    0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};

struct CHIP_8
{
    unsigned char memory[4 * 1024];
    unsigned char display[64 * 32];
    unsigned char key[16];

    unsigned short pc;
    unsigned short opcode;
    unsigned short I;
    unsigned short sp;

    unsigned char V[16];

    unsigned short stack[16];

    unsigned char delay_timer;
    unsigned char sound_timer;

    void clear_display()
    {
        memset(display, 0, 64 * 32);
    }

    void tick()
    {
        opcode = memory[pc] << 8 | memory[pc + 1];

        switch (opcode & 0xF000)
        {
        case 0x0000:
        {
            switch (opcode & 0x000F)
            {
            case 0x0000: // 00E0 Clear Display
                clear_display();
                pc += 2;
                break;
            case 0x000E: // 000EE return from sub routine
                pc = stack[(--sp) & 0xF] + 2;
                break;
            }
        }
        break;

        case 0x1000: // 1NNN Jump to address NNN
            pc = opcode & 0x0FFF;
            break;

        case 0x2000:                  // 2NNN Call subroutine at NNN
            stack[(sp++) & 0xF] = pc; // push pc onto stack and increment sp
            pc = opcode & 0x0FFF;
            break;

        case 0x3000: // 3XNN skip the next instruction if VX == NN
            if (V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF))
                pc += 4;
            else
                pc += 2;
            break;

        case 0x4000: // 4XNN skip the next instruction if VX != NN
            if (V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF))
                pc += 4;
            else
                pc += 2;
            break;

        case 0x5000: // 5XY0 skip the next instruction if VX == VY
            if (V[(opcode & 0x0F00) >> 8] == V[(opcode & 0x00F0) >> 4])
                pc += 4;
            else
                pc += 2;
            break;

        case 0x6000: // 6XNN set VX to NN
            V[(opcode & 0xF00) >> 8] = (opcode & 0x00FF);
            pc += 2;
            break;

        case 0x7000: // 7XNN add NN to VX
            V[(opcode & 0xF00) >> 8] += (opcode & 0x00FF);
            pc += 2;
            break;

        case 0x8000:
            switch (opcode & 0x000F)
            {
            case 0x0000: // 8XY0 set VX = VY
                VX = VY;
                pc += 2;
                break;
            case 0x0001: // 8XY1 set VX = VX | VY
                VX = VX | VY;
                pc += 2;
                break;
            case 0x0002: // 8XY2 set VX = VX & VY
                VX = VX & VY;
                pc += 2;
                break;
            case 0x0003: // 8XY1 set VX = VX ^ VY
                VX = VX ^ VY;
                pc += 2;
                break;
            case 0x0004: // 8XY4 set VX += VY
                if ((int)VX + (int)VY < 256)
                    V[0xF] &= 0;
                else
                    V[0xF] = 1;
                VX += VY;
                pc += 2;
                break;
            case 0x0005: // 8XY5 set VX -= VY
                if ((int)VX - (int)VY >= 0)
                    V[0xF] = 1;
                else
                    V[0xF] &= 0;
                VX -= VY;
                pc += 2;
                break;
            case 0x0006: // 8XY6 VX >>= 1
                V[0xF] = VX & 7;
                VX = VX >> 1;
                pc += 2;
                break;
            case 0x0007: // 8XY7 VX = VY - VX
                if ((int)VX - (int)VY > 0)
                    V[0xF] = 1;
                else
                    V[0xF] &= 0;
                VX = VY - VX;
                pc += 2;
                break;
            case 0x000E:
                V[0xF] = VX & 7;
                VX = VX << 1;
                pc += 2;
                break;
            }
            break;

        case 0x9000: // 9XY0 skip the next instruction if VX != VY
            if (V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4])
                pc += 4;
            else
                pc += 2;
            break;

        case 0xA000: // ANNN set I to address NNN
            I = (opcode & 0x0FFF);
            pc += 2;
            break;

        case 0xB000: // BNNN jump to address NNN + V0
            pc = (opcode & 0x0FFF) + V[0];
            pc += 2;
            break;

        case 0xC000: // CXNN sets VX to random number & NN
            V[(opcode & 0x0F00) >> 8] = rand() & (opcode & 0x00FF);
            pc += 2;
            break;

        case 0xD000: // DXYN Draw a sprite at (VX, VY) width 8 and height of N pixels
        {
            int vx = V[(opcode & 0x0F00) >> 8];
            int vy = V[(opcode & 0x00F0) >> 4];
            int height = (opcode & 0x000F);
            V[0xF] &= 0;

            for (int y = 0; y < height; ++y)
            {
                int pixel = memory[I + y];
                for (int x = 0; x < 8; ++x)
                {
                    if (pixel & (0x80 >> x))
                    {
                        if (display[x + vx + (y + vy) * 64])
                        {
                            V[0xF] = 1;
                        }
                        display[x + vx + (y + vy) * 64] ^= 1;
                    }
                }
            }
            pc += 2;
        }
        break;

        case 0xE000:
            switch (opcode & 0x000F)
            {
            case 0x000E: // EX9E: Skips the next instruction if the key stored in VX is pressed
                {

                const unsigned char* keys = SDL_GetKeyboardState(NULL);
                if (keys[keymap[V[(opcode & 0x0F00) >> 8]]])
                    pc += 4;
                else
                    pc += 2;
                }
                break;

            case 0x0001: // EXA1: Skips the next instruction if the key stored in VX isn't pressed
                {
                const unsigned char* keys = SDL_GetKeyboardState(NULL);
                    
                if (!keys[keymap[V[(opcode & 0x0F00) >> 8]]])
                    pc += 4;
                else
                    pc += 2;
                }
                break;
            }
            break;
        case 0xF000:
            switch (opcode & 0x00FF)
            {
            case 0x0007: // FX07: Sets VX to the value of the delay timer
                V[(opcode & 0x0F00) >> 8] = delay_timer;
                pc += 2;
                break;

            case 0x000A: // FX0A: A key press is awaited, and then stored in VX
                {

                const unsigned char* keys = SDL_GetKeyboardState(NULL);
                for (int i = 0; i < 0x10; i++)
                    if (keys[keymap[i]])
                    {
                        V[(opcode & 0x0F00) >> 8] = i;
                        pc += 2;
                    }
                }
                break;

            case 0x0015: // FX15: Sets the delay timer to VX
                delay_timer = V[(opcode & 0x0F00) >> 8];
                pc += 2;
                break;

            case 0x0018: // FX18: Sets the sound timer to VX
                sound_timer = V[(opcode & 0x0F00) >> 8];
                pc += 2;
                break;

            case 0x001E: // FX1E: Adds VX to I
                I += V[(opcode & 0x0F00) >> 8];
                pc += 2;
                break;

            case 0x0029: // FX29: Sets I to the location of the sprite for the character in VX. Characters 0-F (in hexadecimal) are represented by a 4x5 font
                I = V[(opcode & 0x0F00) >> 8] * 5;
                pc += 2;
                break;

            case 0x0033: // FX33: Stores the Binary-coded decimal representation of VX, with the most significant of three digits at the address in I, the middle digit at I plus 1, and the least significant digit at I plus 2
                memory[I] = V[(opcode & 0x0F00) >> 8] / 100;
                memory[I + 1] = (V[(opcode & 0x0F00) >> 8] / 10) % 10;
                memory[I + 2] = V[(opcode & 0x0F00) >> 8] % 10;
                pc += 2;
                break;

            case 0x0055: // FX55: Stores V0 to VX in memory starting at address I
                for (int i = 0; i <= ((opcode & 0x0F00) >> 8); i++)
                    memory[I + i] = V[i];
                pc += 2;
                break;

            case 0x0065: //FX65: Fills V0 to VX with values from memory starting at address I
                for (int i = 0; i <= ((opcode & 0x0F00) >> 8); i++)
                    V[i] = memory[I + i];
                pc += 2;
                break;
            }
            break;
        }

        if(delay_timer > 0)
            delay_timer--;
        if(sound_timer > 0)
            sound_timer--;
    }

    void restart()
    {
        pc = 0x200;
        I = 0;
        opcode = 0;
        sp = 0;
        // unsigned char memory[4 * 1024];
        clear_display();
        memset(key, 0, 16);
        memset(V, 0, 16);
        memset(stack, 0, sizeof(unsigned short) * 16);
        delay_timer = 60;
        sound_timer = 60;

        for (int i = 0; i < 80; ++i)
        {
            memory[i] = chip8_fontset[i];
        }
    }

    void loadfile(const char *file_path)
    {
        FILE *file = fopen(file_path, "rb");
        if (!file)
        {
            printf("failed to load %s into memory!\n", file_path);
            return;
        }

        fread(memory + 0x200, 1, (4 * 1024) - 0x200, file);

        fclose(file);
    }
};

int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    SDL_Window *window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool running = true;

    CHIP_8 chip = {};

    chip.restart();
    chip.loadfile("./roms/pong.ch8");

    ImVec4 clear_color = {};

    MemoryEditor memoryEditor;
    MemoryEditor stackEditor;
    MemoryEditor displayEditor;

    glMatrixMode(GL_PROJECTION_MATRIX);
    glLoadIdentity();
    glOrtho(0, 640, 480, 0, -1, 1);

    bool step = true;
    bool debug = false;
    bool display = false;
    bool memory = false;

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);

            switch (event.type)
            {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.scancode)
                {
                case SDL_SCANCODE_ESCAPE:
                    running = false;
                    break;
                }
                break;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        for (int y = 0; y < 32; y++)
        {
            for (int x = 0; x < 64; x++)
            {
                if (chip.display[(y * 64) + x] == 1)
                {
                    glBegin(GL_QUADS);
                    glVertex2f(x * 10, y * 15);
                    glVertex2f(x * 10, y * 15 + 15);
                    glVertex2f(x * 10 + 10, y * 15 + 15);
                    glVertex2f(x * 10 + 10, y * 15);
                    glEnd();
                }
            }
        }

        if (memory)
        {
            memoryEditor.DrawWindow("Memory", chip.memory, 4 * 1024, (size_t)0);
        }
        // stackEditor.Cols = 2;
        // stackEditor.PreviewDataType = ImGuiDataType_U16;
        // stackEditor.DrawWindow("Stack",chip.stack, sizeof(unsigned short) * 16, (size_t)0);
        if (display)
        {
            displayEditor.Cols = 64;
            displayEditor.OptShowAscii = false;
            displayEditor.DrawWindow("Display Memory", chip.display, sizeof(unsigned char) * 32 * 64, 0);
        }

        ImGui::Begin("Debug");
        ImGui::Text("PC: %d", chip.pc);
        ImGui::Text("I: %d", chip.I);
        ImGui::Text("OpCode: %x", chip.opcode);
        ImGui::Text("SP: %d", chip.sp);
        ImGui::Text("V0: %d", chip.V[0x0]);
        ImGui::Text("V1: %d", chip.V[0x1]);
        ImGui::Text("V2: %d", chip.V[0x2]);
        ImGui::Text("V3: %d", chip.V[0x3]);
        ImGui::Text("V4: %d", chip.V[0x4]);
        ImGui::Text("V5: %d", chip.V[0x5]);
        ImGui::Text("V6: %d", chip.V[0x6]);
        ImGui::Text("V7: %d", chip.V[0x7]);
        ImGui::Text("V8: %d", chip.V[0x8]);
        ImGui::Text("V9: %d", chip.V[0x9]);
        ImGui::Text("VA: %d", chip.V[0xA]);
        ImGui::Text("VB: %d", chip.V[0xB]);
        ImGui::Text("VC: %d", chip.V[0xC]);
        ImGui::Text("VD: %d", chip.V[0xD]);
        ImGui::Text("VE: %d", chip.V[0xE]);
        ImGui::Text("VF: %d", chip.V[0xF]);

        if (ImGui::Button("Step"))
        {
            step = true;
        }

        if (ImGui::Button("Restart"))
        {
            chip.restart();
        }

        ImGui::Checkbox("Show Memory Editor", &memory);
        ImGui::Checkbox("Show Display Editor", &display);

        ImGui::End();

        if (debug)
        {
            if (step)
            {
                chip.tick();
                step = false;
            }
        }
        else
        {
            for (int i = 0; i < 10; i++)
            {
                /* code */
                chip.tick();
            }
            
        }

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
