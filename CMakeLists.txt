cmake_minimum_required(VERSION 3.26)

project(Nes_Snd_Emu VERSION 0.2.0)

# Targets
set(NES_SND_EMU_SOURCES
    emu2413/emu2413.c
    src/Blip_Buffer.cpp
    src/Multi_Buffer.cpp
    src/Nes_Apu.cpp
    src/Nes_Fds_Apu.cpp
    src/Nes_Fme7_Apu.cpp
    src/Nes_Mmc5_Apu.cpp
    src/Nes_Namco_Apu.cpp
    src/Nes_Oscs.cpp
    src/Nes_Vrc6_Apu.cpp
    src/Nes_Vrc7_Apu.cpp
)

add_library(Nes_Snd_Emu STATIC ${NES_SND_EMU_SOURCES})
target_include_directories(Nes_Snd_Emu PUBLIC include emu2413)
target_compile_features(Nes_Snd_Emu PUBLIC cxx_std_23)

if(MSVC)
    target_compile_definitions(Nes_Snd_Emu PRIVATE NOMINMAX _CRT_DECLARE_NONSTDC_NAMES=0)
endif()
