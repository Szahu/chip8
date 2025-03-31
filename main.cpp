#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stack>
#include <string>
#include <thread>
#include <unordered_map>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include "SDL3/SDL.h"

void print_hex(const uint16_t &val) { printf("%4X\n", val); }

#ifdef _WIN32
#include <windows.h>
#endif
void play_beep(const unsigned int &ms) {

#ifdef _WIN32 
    Beep(523, ms);
#endif

#ifdef linux
    printf("\a");
#endif

}

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)                                                                       \
    ((byte)&0x80 ? '1' : '0'), ((byte)&0x40 ? '1' : '0'), ((byte)&0x20 ? '1' : '0'),               \
        ((byte)&0x10 ? '1' : '0'), ((byte)&0x08 ? '1' : '0'), ((byte)&0x04 ? '1' : '0'),           \
        ((byte)&0x02 ? '1' : '0'), ((byte)&0x01 ? '1' : '0')

void print_byte(const char byte) { printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(byte)); }

void print_display(const unsigned char *display, const unsigned int display_width,
                   const unsigned int display_height) {
    for (unsigned int i = 0; i < display_height; ++i) {
        for (unsigned int j = 0; j < display_width / 8; ++j) {
            printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(display[i * display_width / 8 + j]));
        }
        printf("\n");
    }
}

void print_registers(const uint8_t *regs) {
    for (int i = 0; i < 16; ++i) {
        printf("register %X: %X\n", i, regs[i]);
    }
}

bool load_program_from_file(const std::string &filename, char *mem, const unsigned int &pc) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    return (bool)file.read(reinterpret_cast<char *>(mem + pc), size);
}

void chip_display_to_pixel_array(const unsigned char *chip_display, unsigned char *pixel_array,
                                 const unsigned int display_width,
                                 const unsigned int display_height) {

    for (unsigned int i = 0; i < display_width * display_height / 8; ++i) {
        for (unsigned int j = 0; j < 8; ++j) {
            char mask = 0b10000000 >> j;
            unsigned char pixel = 0x00;

            if ((mask & chip_display[i]) != 0) {
                pixel = 0xFF;
            }

            pixel_array[3 * (i * 8 + j)] = pixel;
            pixel_array[3 * (i * 8 + j) + 1] = pixel;
            pixel_array[3 * (i * 8 + j) + 2] = pixel;
        }
    }
}

void run_program_menu(std::string& program_filepath) {

    unsigned int entry_index = 0;
    unsigned int entry_max = 0;
    std::vector<std::string> entries;
    for (const auto& entry : std::filesystem::directory_iterator("./../../chip8-roms/demos")) {
        std::cout << entry_index << ": " << entry.path().filename() << std::endl;;
        entry_max = entry_index;
        ++entry_index;
        entries.push_back(entry.path().generic_string());
    }

    unsigned int choice = -1;

    while (choice < 0 || choice > entry_max) {
        std::cout << "Choose a program: ";
        std::cin >> choice;
    }

    program_filepath = entries[choice];
}

void sdl_thread_fun(unsigned char *chip_display, bool *terminate, uint8_t *keys_pressed,
                    bool *waiting_for_key, uint8_t *last_key_pressed, std::mutex *key_mutex) {

    const unsigned int SDL_FPS = 60;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Error while initing SDL.\n");
        return;
    }

    SDL_Window *window = SDL_CreateWindow("CHIP8 emulator", 640, 320, 0);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL, 0);

    SDL_Event event;

    unsigned char pixel_array[64 * 32 * 3];

    std::unordered_map<int32_t, uint8_t> key_to_hex = {
        {SDLK_x, 0x0}, {SDLK_1, 0x1}, {SDLK_2, 0x2}, {SDLK_3, 0x3}, {SDLK_q, 0x4}, {SDLK_w, 0x5},
        {SDLK_e, 0x6}, {SDLK_a, 0x7}, {SDLK_s, 0x8}, {SDLK_d, 0x9}, {SDLK_z, 0xA}, {SDLK_c, 0xB},
        {SDLK_4, 0xC}, {SDLK_r, 0xD}, {SDLK_f, 0xE}, {SDLK_v, 0xF},
    };

    while (!(*terminate)) {

        while (SDL_PollEvent(&event)) {

            *terminate = event.type == SDL_EVENT_QUIT;

            if (event.type == SDL_EVENT_KEY_DOWN) {

                std::lock_guard<std::mutex> lock(*key_mutex);
                auto itr = key_to_hex.find(event.key.keysym.sym);

                if (itr != key_to_hex.end()) {
                    keys_pressed[itr->second] = 1;
                }

            } else if (event.type == SDL_EVENT_KEY_UP) {

                std::lock_guard<std::mutex> lock(*key_mutex);
                auto itr = key_to_hex.find(event.key.keysym.sym);

                if (itr != key_to_hex.end()) {
                    keys_pressed[itr->second] = 0;
                    if (*waiting_for_key && *last_key_pressed == 0xFF) {
                        *waiting_for_key = false;
                        *last_key_pressed = itr->second;
                    }
                }
            }
        }

        // Clear the renderer
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        chip_display_to_pixel_array(chip_display, pixel_array, 64, 32);
        SDL_Surface *surface =
            SDL_CreateSurfaceFrom(reinterpret_cast<void *>(pixel_array), 64, 32, 3 * 64,
                                  SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGB24);
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

        SDL_RenderTexture(renderer, texture, NULL, NULL);

        // Update the screen
        SDL_RenderPresent(renderer);

        std::this_thread::sleep_for(
            std::chrono::milliseconds((unsigned int)((1.0f / SDL_FPS) * 1000.0f)));
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char **argv) {

    std::cout << "Running the emulator!" << std::endl;

    constexpr unsigned int MEM_SIZE = 4096;
    constexpr unsigned int DISPLAY_WIDTH = 64;
    constexpr unsigned int DISPLAY_HEIGHT = 32;
    constexpr unsigned int PC_START_POS = 0x200;
    constexpr unsigned int DELEY_TIMER_FPS = 60;
    constexpr unsigned int SOUND_TIMER_FPS = 60;
    constexpr unsigned int CLOCK_FPS = 1000;
    constexpr uint16_t INSTR_TERMIANTE = 0xFFFF;
    constexpr bool USE_OLD_BIT_SHIFT = false;

    constexpr unsigned char FONT[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };
    constexpr unsigned int FONT_LOC = 0x050;

    unsigned char mem[MEM_SIZE];
    unsigned int program_counter = PC_START_POS;
    std::stack<uint16_t> stack;
    std::atomic<uint8_t> delay_timer = 0;
    std::atomic<uint8_t> sound_timer = 0;

    uint8_t regs[16];
    std::memset(regs, 0x00, 16);

    uint16_t index_register = 0;

    uint8_t keys_pressed[16];
    uint8_t last_key_pressed = 0xFF;
    memset(keys_pressed, 0, sizeof(keys_pressed));

    unsigned char display[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8];
    std::memset(display, 0x00, DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);

    std::memcpy(mem + FONT_LOC, FONT, sizeof(FONT));

    bool terminate = false;
    bool waiting_for_key = false;

    std::string program_path;

    run_program_menu(program_path);

    if (!load_program_from_file(program_path, reinterpret_cast<char *>(mem), program_counter)) {
        printf("Failed to load the program, from path %s.\n", argv[1]);
        return 1;
    }

    std::thread deley_timer_thread([&]() {
        while (!terminate) {
            if (delay_timer > 0) {
                delay_timer--;
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds((unsigned int)((1.0f / DELEY_TIMER_FPS) * 1000.0f)));
        }
    });

    std::thread sound_timer_thread([&]() {
        while (!terminate) {
            if (sound_timer > 0) {
                sound_timer--;
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds((unsigned int)((1.0f / SOUND_TIMER_FPS) * 1000.0f)));
        }
    });

    std::thread sound_thread([&]() {
        while (!terminate) {
            if (sound_timer > 0) {
                play_beep((unsigned int)((1.0f / SOUND_TIMER_FPS) * 1000) * sound_timer);
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds((unsigned int)((1.0f / SOUND_TIMER_FPS) * 1000.0f)));
        }
    });

    std::mutex key_mutex;

    std::thread sdl_thread(sdl_thread_fun, display, &terminate, keys_pressed, &waiting_for_key,
                           &last_key_pressed, &key_mutex);

    // main cpu cycle
    while (!terminate) {

        uint16_t instruction = 0;

        // fetch
        instruction = (mem[program_counter] << 8) | mem[program_counter + 1];
        program_counter += 2;

        // decode and execute
        uint8_t X = (instruction & 0xF000) >> 12;
        uint8_t Y = (instruction & 0x0F00) >> 8;
        uint8_t Z = (instruction & 0x00F0) >> 4;
        uint8_t W = (instruction & 0x000F);

        if (instruction == 0xFFFF) {
            terminate = true;
        } else if (instruction == 0x00E0) {
            // 0E00 clear screen
            memset(display, 0x00, DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);

        } else if (X == 0x1) {

            // 1NNN -> jump to address NNN
            program_counter = (instruction & 0x0FFF);

        } else if (X == 0x3) {

            // 3XNN -> skip next instruction if regs[X] == NN
            if (regs[Y] == (instruction & 0x00FF))
                program_counter += 2;

        } else if (X == 0x4) {

            // 4XNN -> skip next instruction if regs[X] != NN
            if (regs[Y] != (instruction & 0x00FF))
                program_counter += 2;

        } else if (X == 0x5 && W == 0x0) {

            // 5XY0 -> skip next instruction if regs[X] == regs[Y]
            if (regs[Y] == regs[Z])
                program_counter += 2;

        } else if (X == 0x9 && W == 0x0) {

            // 9XY0 -> skip next instruction if regs[X] != regs[Y]
            if (regs[Y] != regs[Z])
                program_counter += 2;

        } else if (X == 0x6) {

            // set register
            regs[Y] = (instruction & 0x00FF);

        } else if (X == 0x7) {

            // add to register
            regs[Y] += (instruction & 0x00FF);

        } else if (X == 0xA) {

            // set index register
            index_register = (instruction & 0x0FFF);

        } else if (X == 0xF && Z == 0x2 && W == 0x9) {

            // set index register to font sprite address
            index_register = FONT_LOC + regs[Y] * 5;

        } else if (X == 0x8 && W == 0x0) {

            // copy register
            regs[Y] = regs[Z];

        } else if (X == 0x8 && W == 0x1) {

            // bitwise or
            regs[Y] = regs[Y] | regs[Z];

        } else if (X == 0x8 && W == 0x2) {

            // bitwise and
            regs[Y] = regs[Y] & regs[Z];

        } else if (X == 0x8 && W == 0x3) {

            // bitwise xor
            regs[Y] = regs[Y] ^ regs[Z];

        } else if (X == 0x8 && W == 0x4) {

            // 8NM4 -> Add regs[N] and regs[M]
            regs[0xF] = (regs[Y] > 0xFF - regs[Z] || regs[Z] > 0xFF - regs[Y]);
            regs[Y] = regs[Y] + regs[Z];

        } else if (X == 0x8 && W == 0x5) {

            // subtract registers
            regs[0xF] = regs[Z] < regs[Y];
            regs[Y] = regs[Y] - regs[Z];

        } else if (X == 0x8 && W == 0x7) {

            // subtract registers
            regs[0xF] = regs[Z] > regs[Y];
            regs[Y] = regs[Z] - regs[Y];

        } else if (X == 0x8 && W == 0x6) {

            // bit shift right
            if (USE_OLD_BIT_SHIFT) {
                regs[Y] = regs[Z];
            }
            regs[0xF] = regs[Y] & 0x01;
            regs[Y] = regs[Y] >> 1;

        } else if (X == 0x8 && W == 0xE) {

            // bit shift left
            if (USE_OLD_BIT_SHIFT) {
                regs[Y] = regs[Z];
            }
            regs[0xF] = (regs[Y] & 0x80) >> 7;
            regs[Y] = regs[Y] << 1;

        } else if (X == 0xD) {

            // display
            uint16_t height = W;
            uint16_t x_coord = regs[Y] % DISPLAY_WIDTH;
            uint16_t y_coord = regs[Z] % DISPLAY_HEIGHT;

            bool flipped_off = false;

            for (uint16_t h = 0; h < height && y_coord + h < DISPLAY_HEIGHT; ++h) {
                unsigned char sprite_byte = mem[index_register + h];
                unsigned int left_index = ((y_coord + h) * DISPLAY_WIDTH + x_coord) / 8;

                char old_left_byte = display[left_index];
                char old_right_byte = display[left_index];

                if (x_coord / 8 < DISPLAY_WIDTH / 8) {
                    char left_byte_xor = display[left_index] ^ (sprite_byte >> (x_coord % 8));
                    display[left_index] = left_byte_xor;
                    flipped_off |= (old_left_byte & (old_left_byte ^ left_byte_xor)) != 0x0;
                }

                if (x_coord / 8 + 1 < DISPLAY_WIDTH / 8) {
                    char right_byte_xor =
                        display[left_index + 1] ^ (sprite_byte << (8 - (x_coord % 8)));
                    display[left_index + 1] = right_byte_xor;
                    flipped_off |= (old_right_byte & (old_right_byte ^ right_byte_xor)) != 0x0;
                }
            }

            regs[0xF] = flipped_off;
        } else if (X == 0xF && Z == 0x5 && W == 0x5) {

            // FX55 -> Store registers from 0 to X to memory starting at I
            for (unsigned int i = 0; i <= Y; ++i) {
                mem[index_register + i] = regs[i];
            }

        } else if (X == 0xF && Z == 0x6 && W == 0x5) {

            // FN65 -> Load registers from 0 to N from memory at I
            for (unsigned int i = 0; i <= Y; ++i) {
                regs[i] = mem[index_register + i];
            }

        } else if (X == 0xF && Z == 0x3 && W == 0x3) {

            // FN33 -> convert regs[N] to 3 decimal digits and store under mem[I+1/2]

            uint8_t num = regs[Y];
            mem[index_register] = num / 100;
            mem[index_register + 1] = (num % 100) / 10;
            mem[index_register + 2] = num % 10;

        } else if (X == 0xF && Z == 0x1 && W == 0xE) {

            // FN1E -> Add regs[N] to index register
            index_register += regs[Y];

        } else if (X == 0xF && Z == 0x0 && W == 0xA) {

            // FN0A -> Wait for a key press then store it in regs[N]
            std::lock_guard<std::mutex> lock(key_mutex);
            if (last_key_pressed == 0xFF) {
                waiting_for_key = true;
                program_counter -= 2;
            } else {
                regs[Y] = last_key_pressed;
                last_key_pressed = 0xFF;
                waiting_for_key = false;
            }

        } else if (X == 0xF && Z == 0x0 && W == 0x7) {

            // FN07 -> set regs[N] to value of deley timer
            regs[Y] = delay_timer;

        } else if (X == 0xF && Z == 0x1 && W == 0x5) {

            // FN15 -> set deley timer to regs[N]
            delay_timer = regs[Y];

        } else if (X == 0xF && Z == 0x1 && W == 0x8) {

            // FN18 -> set the sound timer to regs[Y]
            sound_timer = regs[Y];

        } else if (X == 0x2) {

            // 2NNN -> exec subroutine under NNN
            stack.push(program_counter);
            program_counter = (instruction & 0x0FFF);

        } else if (instruction == 0x00EE) {

            // 00EE -> return from subroutine
            program_counter = stack.top();
            stack.pop();

        } else if (X == 0xE && Z == 0x9 && W == 0xE) {

            // EN9E -> if key under regs[N] is pressed skip next instruction
            std::lock_guard<std::mutex> lock(key_mutex);
            if (keys_pressed[regs[Y]])
                program_counter += 2;

        } else if (X == 0xE && Z == 0xA && W == 0x1) {

            // ENA1 -> if key under regs[N] is not pressed skip next instruction
            std::lock_guard<std::mutex> lock(key_mutex);
            if (!keys_pressed[regs[Y]])
                program_counter += 2;

        } else if (X == 0xC) {

            // CQNN -> set regs[Y] to NN & random number
            regs[Y] = (rand() % 0xFF) & (instruction && 0x00FF);

        } else {
            std::cout << "Unrecognised instruction: " << std::hex << instruction << std::dec
                      << std::endl;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds((unsigned int)((1.0f / CLOCK_FPS) * 1000.0f)));
    }

    sdl_thread.join();
    deley_timer_thread.join();
    sound_timer_thread.join();
    sound_thread.join();

    std::cout << "All threads joined, terminating." << std::endl;

    return 0;
}