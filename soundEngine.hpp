#pragma once

#include "miniaudio.h"
#include <utility>

class SoundEngine {
public:
    static int init();
    
    static void playSound(ma_sound* sound);

    static ma_sound registerSound(const char* path);

    static void deallocSound(ma_sound* sound);

    static void shutdown();
};