#include "soundEngine.hpp"

static ma_engine m_engine;

int SoundEngine::init() {
    ma_result result;
    result = ma_engine_init(NULL, &m_engine);
    if (result != MA_SUCCESS) {
        return result; // Failed to initialize the engine.
    }
    return result;
}

void SoundEngine::playSound(ma_sound *sound) {
    ma_sound_start(sound);
}

ma_sound SoundEngine::registerSound(const char *path) {
    ma_result result;
    ma_sound sound;

    result = ma_sound_init_from_file(&m_engine, path, 0, NULL, NULL, &sound);
    return std::move(sound);
}

void SoundEngine::deallocSound(ma_sound *sound) {
    ma_sound_uninit(sound);
}

void SoundEngine::shutdown() {
    ma_engine_uninit(&m_engine);
}