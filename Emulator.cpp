#include "Emulator.h"
#include "vk_types.h"
#include <SDL.h>
#include <imgui.h>

#include <iostream>
#include <fstream>
#include <cstdlib>

Emulator::Emulator(Color display[]) {
    this->display = display;
    this->time = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
    this->memory = new uint8_t[MEM_SIZE];
    editor.Cols = 8;
    editor.OptShowAscii = false;

    load_file("./roms/octojam1title.ch8");

}

Emulator::~Emulator() {
    delete[] this->memory;
}

void Emulator::clear_screen() {
    for (uint8_t i = 0; i < 2; i++) {
        memset(display_bitmap[i], 0, sizeof(display_bitmap[i]));
    }
    sync_display();
}

void Emulator::load_file(const char* filename) {
    memset(memory, 0, MEM_SIZE);
    memset(stack, 0, sizeof(stack));
    clear_screen();
    memcpy(memory, font_data, sizeof(font_data));
    color_plane = 1;
    high_resolution = false;

    std::streampos size;
    std::ifstream file;
    file.open(filename, std::ios::binary | std::ios::ate);
    size = file.tellg() < 0xFF38 ? file.tellg() : (std::streampos)0xFF38;
    file.seekg(0, std::ios::beg);
    file.read((char*)this->memory + 0x200, size);
    std::cout << "READ " << size << " bytes" << std::endl;
    file.close();

    program_counter = 0x0200;
    i_register = 0x0000;
    delay_timer = 0;
    sound_timer = 0;
    counter = 0;
    memset(register_file, 0, sizeof(register_file));
    memset(rpl_file, 0, sizeof(rpl_file));
}

void Emulator::set_keys() {
    for (int i = 0; i < 16; i++) {
        keys[i] = ImGui::IsKeyDown(key_map[i]);
    }
}

void Emulator::get_instruction(Instruction& in) {
    in.data = (memory[program_counter + 1] << 8) | memory[program_counter];
}

void Emulator::sync_display() {
    for (int i = 0; i < sizeof(display_bitmap[0]); i++) {
        for (int j = 0; j < 8; j++) {
            uint8_t bitmap = 0x00;
            for (int k = 1; k >= 0; k--) {
                bitmap <<= 1;
                bitmap |= (display_bitmap[k][i] >> (7 - j)) & 0x01;
            }
            display[i * 8 + j] = palate[bitmap];
        }
    }
}

void Emulator::draw_array_to_display(uint8_t* byte_array, uint8_t x, uint8_t y, int width, int height, uint8_t map_index) {
    uint8_t& VF = register_file[0xF];
    uint8_t byte_offset = x / 8;
    uint8_t bit_offset = x % 8;
    for (int i = 0; i < height; i++) {
        uint16_t bitmap_offset = (y + i) % 64 * 16 + (byte_offset) % 16;
        uint8_t data = byte_array[i * width] >> bit_offset;
        display_bitmap[map_index][bitmap_offset] ^= data;
        if (~display_bitmap[map_index][bitmap_offset] & data) VF = 1;
        for (int j = 1; j < width; j++) {
            bitmap_offset = (y + i) % 64 * 16 + (byte_offset + j) % 16;
            data = byte_array[i * width + j - 1] << (8 - bit_offset) | byte_array[i * width + j] >> bit_offset;
            display_bitmap[map_index][bitmap_offset] ^= data;
            if (~display_bitmap[map_index][bitmap_offset] & data) VF = 1;
        }
        bitmap_offset = (y + i) % 64 * 16 + (byte_offset + width) % 16;
        data = byte_array[i * width + width - 1] << (8 - bit_offset);
        display_bitmap[map_index][bitmap_offset] ^= data;
        if (~display_bitmap[map_index][bitmap_offset] & data) VF = 1;
    }
}

void Emulator::skip_next_instruction() {
    program_counter += 2;

    Instruction in;
    get_instruction(in);
    if (in.get_all() == 0xF000)
        program_counter += 2;
}

void Emulator::execute(Instruction& in) {
    uint8_t X = in.get_high_low();
    uint8_t Y = in.get_low_high();
    uint8_t& VX = register_file[X];
    uint8_t& VY = register_file[Y];
    uint8_t N = in.get_low_low();
    uint8_t NN = in.get_low();
    uint16_t NNN = in.get_all() & 0x0FFF;
    uint8_t& V0 = register_file[0x0];
    uint8_t& VF = register_file[0xF];
    switch (in.get_high_high()) {
    case 0x00:
        switch (in.get_low_high()) {
        case 0x0C:
            // Scroll display N lines down
            for (uint8_t map_index = 0; map_index < 2; map_index++) {
                if (color_plane & (1 << map_index)) {
                    for (uint8_t i = 0; i < 16; i++) {
                        for (uint8_t j = 63; j >= N; j--) {
                            display_bitmap[map_index][j * 16 + i] = display_bitmap[map_index][(j - N) * 16 + i];
                        }
                    }
                }
            }
            sync_display();
            break;
        case 0x0D:
            // Scroll display N lines up
            for (uint8_t map_index = 0; map_index < 2; map_index++) {
                if (color_plane & (1 << map_index)) {
                    for (uint8_t i = 0; i < 16; i++) {
                        for (uint8_t j = 0; j < 64 - N; j++) {
                            display_bitmap[map_index][j * 16 + i] = display_bitmap[map_index][(j + N) * 16 + i];
                        }
                    }
                }
            }
            sync_display();
        }
        switch (in.get_all()) {
        case 0x00E0:
            // Clear the screen
            clear_screen();
            break;
        case 0x00EE:
            // 0x00EE Return from a subroutine
            program_counter = stack[--stack_pointer];
            break;
        case 0x00FB:
            // Scroll display 4 pixels right
            for (uint8_t map_index = 0; map_index < 2; map_index++) {
                if (color_plane & (1 << map_index)) {
                    for (uint8_t j = 0; j < 64; j++) {
                        for (uint8_t i = 15; i > 0; i--) {
                            display_bitmap[map_index][j * 16 + i] = (display_bitmap[map_index][j * 16 + i - 1] << 4) | (display_bitmap[map_index][j * 16 + i] >> 4);
                        }
                        display_bitmap[map_index][j * 16] = display_bitmap[map_index][j * 16] >> 4;
                    }
                }
            }
            sync_display();
            break;
        case 0x00FC:
            // Scroll display 4 pixels left
            for (uint8_t map_index = 0; map_index < 2; map_index++) {
                if (color_plane & (1 << map_index)) {
                    for (uint8_t j = 0; j < 64; j++) {
                        for (uint8_t i = 0; i < 15; i++) {
                            display_bitmap[map_index][j * 16 + i] = (display_bitmap[map_index][j * 16 + i] << 4) | (display_bitmap[map_index][j * 16 + i + 1] >> 4);
                        }
                        display_bitmap[map_index][j * 16 + 15] = display_bitmap[map_index][j * 16 + 15] << 4;
                    }
                }
            }
            sync_display();
            break;
        case 0x00FD:
            // Exit CHIP interpreter
            paused = true;
            break;
        case 0x00FE:
            // Disable extended screen mode
            high_resolution = false;
            break;
        case 0x00FF:
            // Enable extended screen mode
            high_resolution = true;
            break;
        default:
            if (!(in.get_low_high() == 0x0C))
                std::cout << "Not implemented: " << std::hex << in.get_all() << std::endl;
        }
        break;
    case 0x01:
        // Jump to NNN
        program_counter = NNN;
        return;
    case 0x02:
        // Call NNN
        if (stack_pointer < sizeof(stack) / sizeof(stack[0])) {
            stack[stack_pointer++] = program_counter;
            program_counter = NNN;
        }
        return;
    case 0x03:
        // Skip the following instruction if the value of register VX equals NN
        if (VX == NN)
            skip_next_instruction();

        break;
    case 0x04:
        // Skip the following instruction if the value of register VX is not equal to NN
        if (VX != NN)
            skip_next_instruction();
        break;
    case 0x05:
        switch (in.get_low_low()) {
        case 0x00:
            // Skip the following instruction if the value of register VX is equal to the value of register VY
            if (VX == VY)
                skip_next_instruction();
            break;
        case 0x02:
            // Save an inclusive range of registers to memory starting at I
            if (VY > VX && VX < 16 && VY < 16) {
                for (uint8_t i = VX; i <= VY; i++) {
                    memory[i_register + i] = register_file[i];
                }
            }
            break;
        case 0x03:
            // Load an inclusive range of registers from memory starting at I
            if (VY > VX && VX < 16 && VY < 16) {
                for (uint8_t i = VX; i <= VY; i++) {
                    register_file[i] = memory[i_register + i];
                }
            }
            break;
        default:
            std::cout << "Not implemented: " << std::hex << in.get_all() << std::endl;
        }
        break;
    case 0x06:
        // Store number NN in register VX
        VX = NN;
        break;
    case 0x07:
        // Add the value NN to register VX
        VX += NN;
        break;
    case 0x08:
        switch (in.get_low_low()) {
        case 0x00:
            // Store the value of register VY in register VX
            VX = VY;
            break;
        case 0x01:
            // Set VX to VX OR VY
            VX |= VY;
            break;
        case 0x02:
            // Set VX to VX AND VY
            VX &= VY;
            break;
        case 0x03:
            // Set VX to VX XOR VY
            VX ^= VY;
            break;
        case 0x04:
            // Add the value of register VY to register VX
            VX += VY;
            if (VX < VY)
                VF = 0x01;
            else
                VF = 0x00;
            break;
        case 0x05:
            // Subtract the value of register VY from register VX
            N = VX;
            VX = VX - VY;
            if (VX <= N)
                VF = 0x01;
            else
                VF = 0x00;
            break;
        case 0x06:
            // Store the value of register VY shifted right one bit in register VX
            N = VY & 0x01;
            VX = VY >> 1;
            VF = N;
            break;
        case 0x07:
            // Set register VX to the value of VY minus VX
            VX = VY - VX;
            if (VX <= VY)
                VF = 0x01;
            else
                VF = 0x00;
            break;
        case 0x0E:
            // Store the value of register VY shifted left one bit in register VX
            N = (VY & 0x80) >> 7;
            VX = VY << 1;
            VF = N;
            break;
        default:
            std::cout << "Not implemented: " << std::hex << in.get_all() << std::endl;
        }
        break;
    case 0x09:
        switch (in.get_low_low()) {
        case 0x00:
            // Skip the following instruction if the value of register VX is not equal to the value of register VY
            if (VX != VY)
                skip_next_instruction();
            break;
        default:
            std::cout << "Not implemented: " << std::hex << in.get_all() << std::endl;
        }
        break;
    case 0x0A:
        // Store memory address NNN in register I
        i_register = NNN;
        break;
    case 0x0B:
        if (NNN + V0 > MEM_SIZE) return;
        program_counter = NNN + V0;
        return;
    case 0x0C:
        VX = (uint8_t)std::rand() & NN;
        break;
    case 0x0D:
        // Draw a sprite at position VX, VY with N bytes of sprite data starting at the address stored in I
        VF = 0;
        if (N == 0)
            N = 16;
        NN = 0;
        for (uint8_t bitmap_index = 0; bitmap_index < 2; bitmap_index++) {
            if (color_plane & (1 << bitmap_index)) {
                if (high_resolution && N == 16) {
                    uint8_t sprite[32];
                    for (uint8_t i = 0; i < 32; i++) {
                        sprite[i] = memory[i_register + i + NN * 32];
                    }
                    draw_array_to_display(sprite, VX, VY, 2, 16, bitmap_index);
                }
                else if (high_resolution) {
                    uint8_t sprite[16];
                    for (uint8_t i = 0; i < N; i++) {
                        sprite[i] = memory[i_register + i + NN * N];
                    }
                    draw_array_to_display(sprite, VX, VY, 1, N, bitmap_index);
                }
                else if (N == 16) {
                    uint8_t sprite[128];  // (2 * 2) * 2 * N
                    for (uint8_t i = 0; i < 16; i++) {
                        sprite[8 * i] = double_upper_nibble(memory[i_register + 2 * i + NN * 32 ]);
                        sprite[8 * i + 1] = double_upper_nibble(memory[i_register + 2 * i + NN * 32] << 4);
                        sprite[8 * i + 2] = double_upper_nibble(memory[i_register + 2 * i + NN * 32 + 1]);
                        sprite[8 * i + 3] = double_upper_nibble(memory[i_register + 2 * i + NN * 32 + 1] << 4);
                        sprite[8 * i + 4] = double_upper_nibble(memory[i_register + 2 * i + NN * 32]);
                        sprite[8 * i + 5] = double_upper_nibble(memory[i_register + 2 * i + NN * 32] << 4);
                        sprite[8 * i + 6] = double_upper_nibble(memory[i_register + 2 * i + NN * 32 + 1]);
                        sprite[8 * i + 7] = double_upper_nibble(memory[i_register + 2 * i + NN * 32 + 1] << 4);
                    }
                    draw_array_to_display(sprite, 2 * VX, 2 * VY, 4, 32, bitmap_index);
                }
                else {
                    uint8_t sprite[64];  // (2 * 2) * N
                    for (uint8_t i = 0; i < N; i++) {
                        sprite[4 * i] = double_upper_nibble(memory[i_register + i + NN * N]);
                        sprite[4 * i + 1] = double_upper_nibble(memory[i_register + i + NN * N] << 4);
                        sprite[4 * i + 2] = double_upper_nibble(memory[i_register + i + NN * N]);
                        sprite[4 * i + 3] = double_upper_nibble(memory[i_register + i + NN * N] << 4);
                    }
                    draw_array_to_display(sprite, 2 * VX, 2 * VY, 2, 2 * N, bitmap_index);
                }
                NN++;
            }
        }
        sync_display();
        break;
    case 0x0E:
        // Key instructions
        switch (in.get_low()) {
        case 0x9E:
            if (keys[VX])
                skip_next_instruction();
            break;
        case 0xA1:
            if (!keys[VX])
                skip_next_instruction();
            break;
        default:
            std::cout << "Not implemented: " << std::hex << in.get_all() << std::endl;
        }
        break;
    case 0x0F:
        switch (in.get_low()) {
        case 0x00:
            // Load I with a 16 bit address
            if (in.get_high_low() != 0x0) break;
            program_counter += 2;
            Instruction address;
            get_instruction(address);
            i_register = address.get_all();
            break;
        case 0x01:
            // Select zero or more drawing planes by bitmask(0 <= n <= 3).
            N = in.get_high_low();
            color_plane = N & 0x3;
            break;
        case 0x07:
            // Store the current value of the delay timer in register VX
            VX = delay_timer;
            break;
        case 0x0A:
            // Wait for a keypress and store the result in register VX
            uint8_t i;
            if (!waiting_on_release) {
                for (i = 0; i < 16; i++) {
                    if (keys[i]) {
                        VX = i;
                        waiting_on_release = true;
                    }
                }
                if (i == 16) {
                    return;
                }
            }
            else {
                for (i = 0; i < 16; i++) {
                    if (keys[i]) return;
                }
                waiting_on_release = false;
            }
            break;
        case 0x15:
            // Set the delay timer to the value of register VX
            delay_timer = VX;
            break;
        case 0x18:
            // Set the sound timer to the value of VX
            sound_timer = VX;
            break;
        case 0x1E:
            // Add the value stored in register VX to register I
            i_register += VX;
            break;
        case 0x29:
            // Set I to the memory address of the sprite data corresponding to the hexadecimal digit stored in register VX
            if (VX < 0x10)
                i_register = 5 * VX;
            break;
        case 0x30:
            // Point I to a 10-byte font sprite for digit VX (0-9)
            if (VX < 0xA)
                i_register = 0x50 + 10 * VX;
            break;

        case 0x33:
            // Store the binary-coded decimal equivalent of the value stored in register VX at addresses I, I+1, and I+2
            if (i_register < 0xFFFE) {
                memory[i_register] = VX / 100;
                memory[i_register + 1] = (VX / 10) % 10;
                memory[i_register + 2] = VX % 10;
            }
            break;
        case 0x55:
            // Store the values of registers V0 to VX inclusive in memory starting at address I
            // I is set to I + X + 1 after operation
            for (int i = 0; i <= X; i++) {
                memory[i_register + i] = register_file[i];
            }
            i_register += X + 1;
            break;
        case 0x65:
            // Fill registers V0 to VX inclusive with the values stored in memory starting at address I
            // I is set to I + X + 1 after operation
            for (int i = 0; i <= X; i++) {
                register_file[i] = memory[i_register + i];
            }
            i_register += X + 1;
            break;
        case 0x75:
            // Store V0..VX in RPL user flags
            if (VX < 16)
                memcpy(rpl_file, register_file, VX);
            break;
        case 0x85:
            // Load V0..Vx from RPL user flags
            if (VX < 16)
                memcpy(register_file, rpl_file, VX);
            break;
        default:
            std::cout << "Not implemented: " << std::hex << in.get_all() << std::endl;
        }
        break;
    default:
        std::cout << "Not implemented: " << std::hex << in.get_all() << std::endl;
    }
    program_counter += 2;
}

void Emulator::tick_timers() {
    if (counter++ > 8) {
        counter = 0;
        if (delay_timer)
            delay_timer--;
        if (sound_timer)
            sound_timer--;
    }
}

void Emulator::step() {
    Instruction in;
    set_keys();
    get_instruction(in);
    execute(in);
    tick_timers();
}

void Emulator::tick() {
    while (true) {
        double current_time = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
        if (current_time - time < 0) {  // Return once we've caught up
            return;
        }
        time = time + frequency / 1000;

        if (!paused && frequency > 0.009) {
            step();
        }
        else if (step_once) {
            step_once = false;
            step();
        }
        else {
            time = current_time;
            return;
        }
    }
}

void Emulator::render() {
    editor.DrawWindow("Memory", memory, MEM_SIZE);
    editor.DrawWindow("Registers", register_file, 16);
    editor.DrawWindow("Stack", stack, sizeof(stack));
    {
        if (ImGui::Begin("Special Registers")) {
            if (ImGui::BeginTable("reg", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_Reorderable)) {
                ImGui::TableSetupColumn("i_register");
                ImGui::TableSetupColumn("program_counter");
                ImGui::TableSetupColumn("next_instruction");
                ImGui::TableSetupColumn("stack_pointer");
                ImGui::TableSetupColumn("delay_timer");
                ImGui::TableSetupColumn("sound_timer");
                ImGui::TableHeadersRow();
                ImGui::TableNextColumn();
                ImGui::Text("%04x", i_register);
                ImGui::TableNextColumn();
                ImGui::Text("%04x", program_counter);
                Instruction in;
                get_instruction(in);
                ImGui::TableNextColumn();
                ImGui::Text("%04x", in.get_all());
                ImGui::TableNextColumn();
                ImGui::Text("%02x", stack_pointer);
                ImGui::TableNextColumn();
                ImGui::Text("%04x", delay_timer);
                ImGui::TableNextColumn();
                ImGui::Text("%04x", sound_timer);
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
    {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open File")) {
                    file_dialog.Open();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        file_dialog.Display();
        if (file_dialog.HasSelected()) {
            load_file(file_dialog.GetSelected().string().c_str());
            file_dialog.ClearSelected();
        }
    }
    {
        ImGui::Begin("Interpreter Controls");
        if (ImGui::Button("Pause")) {
            paused = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Resume")) {
            paused = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Step")) {
            step_once = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Palate")) {
            ImGui::OpenPopup("palate_picker");
        }
        if (ImGui::BeginPopup("palate_picker")) {
            if (ImGui::Button("Apply")) {
                for (int i = 0; i < 4; i++) {
                    palate[i].r = (uint8_t)(color_select[i][0] * 255);
                    palate[i].g = (uint8_t)(color_select[i][1] * 255);
                    palate[i].b = (uint8_t)(color_select[i][2] * 255);
                    palate[i].a = 255;
                }
                sync_display();
            }
            for (int i = 0; i < 4; i++) {
                ImGui::ColorPicker3((std::string("Palate: ") + std::to_string(i)).c_str(), color_select[i], ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
                ImGui::SameLine();
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Frequency")) {
            ImGui::OpenPopup("Frequency Selector");
        }
        if (ImGui::BeginPopup("Frequency Selector")) {
            ImGui::DragFloat("Frequency (ms)", &frequency, 0.1f, 0.1f, 10000);
            ImGui::EndPopup();
        }
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }
}
