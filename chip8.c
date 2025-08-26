#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <time.h>
#include <math.h>

#define MEMORY_SIZE 4096
#define STACK_SIZE 16
#define REGISTERS_COUNT 16
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define PIXEL_SCALE 10
#define WINDOW_WIDTH (SCREEN_WIDTH * PIXEL_SCALE)
#define WINDOW_HEIGHT (SCREEN_HEIGHT * PIXEL_SCALE)
#define CPU_HZ 200
#define TIMER_HZ 60

typedef struct {
    uint8_t memory[MEMORY_SIZE];
    uint8_t V[REGISTERS_COUNT];
    uint16_t I;
    uint16_t PC;
    uint16_t stack[STACK_SIZE];
    uint8_t SP;
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint8_t gfx[SCREEN_WIDTH * SCREEN_HEIGHT];
    uint8_t keys[16];
    int draw_flag;
} Chip8;

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

const uint8_t fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,
    0x20, 0x60, 0x20, 0x20, 0x70,
    0xF0, 0x10, 0xF0, 0x80, 0xF0,
    0xF0, 0x10, 0xF0, 0x10, 0xF0,
    0x90, 0x90, 0xF0, 0x10, 0x10,
    0xF0, 0x80, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0xF0, 0x90, 0xF0,
    0xF0, 0x10, 0x20, 0x40, 0x40,
    0xF0, 0x90, 0xF0, 0x90, 0xF0,
    0xF0, 0x90, 0xF0, 0x10, 0xF0,
    0xF0, 0x90, 0xF0, 0x90, 0x90,
    0xE0, 0x90, 0xE0, 0x90, 0xE0,
    0xF0, 0x80, 0x80, 0x80, 0xF0,
    0xE0, 0x90, 0x90, 0x90, 0xE0,
    0xF0, 0x80, 0xF0, 0x80, 0xF0,
    0xF0, 0x80, 0xF0, 0x80, 0x80
};

uint32_t get_ticks_ms() {
    return SDL_GetTicks();
}

void init_chip8(Chip8 *chip8) {
    memset(chip8, 0, sizeof(Chip8));
    memcpy(chip8->memory, fontset, sizeof(fontset));
    chip8->PC = 0x200;
    chip8->draw_flag = 0;
}

int load_rom(Chip8 *chip8, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Error: Cannot open ROM %s\n", filename);
        return 0;
    }
    fseek(file, 0, SEEK_END);
    long rom_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (rom_size > MEMORY_SIZE - 0x200) {
        printf("Error: ROM too large\n");
        fclose(file);
        return 0;
    }
    fread(chip8->memory + 0x200, 1, rom_size, file);
    fclose(file);
    return 1;
}

void handle_input(Chip8 *chip8) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            exit(0);
        }
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            int key_state = (event.type == SDL_KEYDOWN) ? 1 : 0;
            switch (event.key.keysym.sym) {
                case SDLK_1: chip8->keys[0x1] = key_state; break;
                case SDLK_2: chip8->keys[0x2] = key_state; break;
                case SDLK_3: chip8->keys[0x3] = key_state; break;
                case SDLK_4: chip8->keys[0xC] = key_state; break;
                case SDLK_q: chip8->keys[0x4] = key_state; break;
                case SDLK_w: chip8->keys[0x5] = key_state; break;
                case SDLK_e: chip8->keys[0x6] = key_state; break;
                case SDLK_r: chip8->keys[0xD] = key_state; break;
                case SDLK_a: chip8->keys[0x7] = key_state; break;
                case SDLK_s: chip8->keys[0x8] = key_state; break;
                case SDLK_d: chip8->keys[0x9] = key_state; break;
                case SDLK_f: chip8->keys[0xE] = key_state; break;
                case SDLK_z: chip8->keys[0xA] = key_state; break;
                case SDLK_x: chip8->keys[0x0] = key_state; break;
                case SDLK_c: chip8->keys[0xB] = key_state; break;
                case SDLK_v: chip8->keys[0xF] = key_state; break;
                case SDLK_ESCAPE: exit(0);
            }
        }
    }
}

void draw_graphics(Chip8 *chip8) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            if (chip8->gfx[y * SCREEN_WIDTH + x]) {
                SDL_Rect rect = {x * PIXEL_SCALE, y * PIXEL_SCALE, PIXEL_SCALE, PIXEL_SCALE};
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }
    SDL_RenderPresent(renderer);
    chip8->draw_flag = 0;
}

void emulate_cycle(Chip8 *chip8) {
    uint16_t opcode = (chip8->memory[chip8->PC] << 8) | chip8->memory[chip8->PC + 1];
    
    switch (opcode & 0xF000) {
        case 0x0000: {
            switch (opcode & 0x00FF) {
                case 0x00E0:
                    memset(chip8->gfx, 0, sizeof(chip8->gfx));
                    chip8->draw_flag = 1;
                    chip8->PC += 2;
                    break;
                case 0x00EE:
                    if (chip8->SP > 0) {
                        chip8->SP--;
                        chip8->PC = chip8->stack[chip8->SP];
                    }
                    chip8->PC += 2;
                    break;
                default:
                    printf("Unknown opcode: 0x%04X\n", opcode);
                    chip8->PC += 2;
                    break;
            }
            break;
        }
        case 0x1000: {
            chip8->PC = opcode & 0x0FFF;
            break;
        }
        case 0x2000: {
            chip8->stack[chip8->SP] = chip8->PC;
            chip8->SP++;
            chip8->PC = opcode & 0x0FFF;
            break;
        }
        case 0x3000: {
            uint8_t X = (opcode & 0x0F00) >> 8;
            uint8_t NN = opcode & 0x00FF;
            if (chip8->V[X] == NN) chip8->PC += 2;
            chip8->PC += 2;
            break;
        }
        case 0x4000: {
            uint8_t X = (opcode & 0x0F00) >> 8;
            uint8_t NN = opcode & 0x00FF;
            if (chip8->V[X] != NN) chip8->PC += 2;
            chip8->PC += 2;
            break;
        }
        case 0x5000: {
            uint8_t X = (opcode & 0x0F00) >> 8;
            uint8_t Y = (opcode & 0x00F0) >> 4;
            if (chip8->V[X] == chip8->V[Y]) chip8->PC += 2;
            chip8->PC += 2;
            break;
        }
        case 0x6000: {
            uint8_t X = (opcode & 0x0F00) >> 8;
            uint8_t NN = opcode & 0x00FF;
            chip8->V[X] = NN;
            chip8->PC += 2;
            break;
        }
        case 0x7000: {
            uint8_t X = (opcode & 0x0F00) >> 8;
            uint8_t NN = opcode & 0x00FF;
            chip8->V[X] += NN;
            chip8->PC += 2;
            break;
        }
        case 0x8000: {
            uint8_t X = (opcode & 0x0F00) >> 8;
            uint8_t Y = (opcode & 0x00F0) >> 4;
            switch (opcode & 0x000F) {
                case 0x0:
                    chip8->V[X] = chip8->V[Y];
                    break;
                case 0x1:
                    chip8->V[X] |= chip8->V[Y];
                    break;
                case 0x2:
                    chip8->V[X] &= chip8->V[Y];
                    break;
                case 0x3:
                    chip8->V[X] ^= chip8->V[Y];
                    break;
                case 0x4: {
                    uint16_t sum = chip8->V[X] + chip8->V[Y];
                    chip8->V[0xF] = (sum > 255) ? 1 : 0;
                    chip8->V[X] = sum & 0xFF;
                    break;
                }
                case 0x5:
                    chip8->V[0xF] = (chip8->V[X] >= chip8->V[Y]) ? 1 : 0;
                    chip8->V[X] -= chip8->V[Y];
                    break;
                case 0x6:
                    chip8->V[0xF] = chip8->V[X] & 0x01;
                    chip8->V[X] >>= 1;
                    break;
                case 0x7:
                    chip8->V[0xF] = (chip8->V[Y] >= chip8->V[X]) ? 1 : 0;
                    chip8->V[X] = chip8->V[Y] - chip8->V[X];
                    break;
                case 0xE:
                    chip8->V[0xF] = (chip8->V[X] >> 7) & 0x01;
                    chip8->V[X] <<= 1;
                    break;
                default:
                    printf("Unknown opcode: 0x%04X\n", opcode);
                    break;
            }
            chip8->PC += 2;
            break;
        }
        case 0x9000: {
            uint8_t X = (opcode & 0x0F00) >> 8;
            uint8_t Y = (opcode & 0x00F0) >> 4;
            if (chip8->V[X] != chip8->V[Y]) chip8->PC += 2;
            chip8->PC += 2;
            break;
        }
        case 0xA000: {
            chip8->I = opcode & 0x0FFF;
            chip8->PC += 2;
            break;
        }
        case 0xB000: {
            chip8->PC = chip8->V[0] + (opcode & 0x0FFF);
            break;
        }
        case 0xC000: {
            uint8_t X = (opcode & 0x0F00) >> 8;
            uint8_t NN = opcode & 0x00FF;
            chip8->V[X] = (rand() % 256) & NN;
            chip8->PC += 2;
            break;
        }
        case 0xD000: {
            uint8_t X = (opcode & 0x0F00) >> 8;
            uint8_t Y = (opcode & 0x00F0) >> 4;
            uint8_t height = opcode & 0x000F;
            uint8_t px;
            chip8->V[0xF] = 0;
            
            for (int yline = 0; yline < height; yline++) {
                px = chip8->memory[chip8->I + yline];
                for (int xline = 0; xline < 8; xline++) {
                    if ((px & (0x80 >> xline)) != 0) {
                        int screen_x = (chip8->V[X] + xline) % SCREEN_WIDTH;
                        int screen_y = (chip8->V[Y] + yline) % SCREEN_HEIGHT;
                        int idx = screen_y * SCREEN_WIDTH + screen_x;
                        if (chip8->gfx[idx] == 1) chip8->V[0xF] = 1;
                        chip8->gfx[idx] ^= 1;
                    }
                }
            }
            chip8->draw_flag = 1;
            chip8->PC += 2;
            break;
        }
        case 0xE000: {
            uint8_t X = (opcode & 0x0F00) >> 8;
            switch (opcode & 0x00FF) {
                case 0x009E:
                    if (chip8->keys[chip8->V[X]]) chip8->PC += 2;
                    chip8->PC += 2;
                    break;
                case 0x00A1:
                    if (!chip8->keys[chip8->V[X]]) chip8->PC += 2;
                    chip8->PC += 2;
                    break;
                default:
                    printf("Unknown opcode: 0x%04X\n", opcode);
                    break;
            }
            break;
        }
        case 0xF000: {
            uint8_t X = (opcode & 0x0F00) >> 8;
            switch (opcode & 0x00FF) {
                case 0x0007:
                    chip8->V[X] = chip8->delay_timer;
                    break;
                case 0x000A: {
                    int key_pressed = 0;
                    for (int i = 0; i < 16; i++) {
                        if (chip8->keys[i]) {
                            chip8->V[X] = i;
                            key_pressed = 1;
                            break;
                        }
                    }
                    if (!key_pressed) return;
                    break;
                }
                case 0x0015:
                    chip8->delay_timer = chip8->V[X];
                    break;
                case 0x0018:
                    chip8->sound_timer = chip8->V[X];
                    break;
                case 0x001E:
                    chip8->I += chip8->V[X];
                    break;
                case 0x0029:
                    chip8->I = chip8->V[X] * 5;
                    break;
                case 0x0033:
                    chip8->memory[chip8->I] = chip8->V[X] / 100;
                    chip8->memory[chip8->I + 1] = (chip8->V[X] / 10) % 10;
                    chip8->memory[chip8->I + 2] = chip8->V[X] % 10;
                    break;
                case 0x0055: 
                    for (int i = 0; i <= X; i++) {
                        chip8->memory[chip8->I + i] = chip8->V[i];
                    }
                    break;
                case 0x0065:  
                    for (int i = 0; i <= X; i++) {
                        chip8->V[i] = chip8->memory[chip8->I + i];
                    }
                    break;
                default:
                    printf("Unknown opcode: 0x%04X\n", opcode);
                    break;
            }
            chip8->PC += 2;
            break;
        }
        default:
            printf("Unknown opcode: 0x%04X\n", opcode);
            chip8->PC += 2;
            break;
    }
}

void audio_callback(void* userdata, Uint8* stream, int len)
{
    Chip8* chip8 = (Chip8*)userdata;
    int16_t* buffer = (int16_t*)stream;
    int samples = len / 2;
    static double phase = 0.0;
    double freq = 440.0;
    double phase_inc = 2.0 * 3.14159 * freq / 44100.0;
    
    for (int i = 0; i < samples; i++)
    {
        if (chip8->sound_timer > 0)
        {
            double val = (fmod(phase, 2.0 * 3.14159) < 3.14159) ? 1.0 : -1.0;
            buffer[i] = (int16_t)(val * 10000);
            phase += phase_inc;
            phase = fmod(phase, 2.0 * 3.14159);
        } else {
            buffer[i] = 0;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <ROM_file.ch8>\n", argv[0]);
        return 1;
    }
    
    srand(time(NULL));
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL Error: %s\n", SDL_GetError());
        return 1;
    }
    
    Chip8 chip8;
    init_chip8(&chip8);
    
    if (!load_rom(&chip8, argv[1])) {
        return 1;
    }
    
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_callback;
    want.userdata = &chip8;
    
    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_device == 0)
    {
        printf("Audio error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_PauseAudioDevice(audio_device, 0);
    
    window = SDL_CreateWindow("CHIP-8 Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Window error: %s\n", SDL_GetError());
        return 1;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Renderer error: %s\n", SDL_GetError());
        return 1;
    }
    
    uint32_t last_cycle = get_ticks_ms();
    uint32_t last_timer = get_ticks_ms();
    
    const uint32_t cycle_delay = 1000 / CPU_HZ;    
    const uint32_t timer_delay = 1000 / TIMER_HZ;   
    
    while (1) {
        handle_input(&chip8);
        
        uint32_t now = get_ticks_ms();
        
        if (now - last_cycle >= cycle_delay) {
            emulate_cycle(&chip8);
            last_cycle = now;
        }
        
        if (now - last_timer >= timer_delay) {
            if (chip8.delay_timer > 0) chip8.delay_timer--;
            if (chip8.sound_timer > 0) chip8.sound_timer--;
            last_timer = now;
        }
        
        if (chip8.draw_flag) {
            draw_graphics(&chip8);
        }
        
        SDL_Delay(1);
    }
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_CloseAudioDevice(audio_device);
    SDL_Quit();
    return 0;
}
