/*
Copyright 2026 Dennis Michael Heine

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once

#include <cstdint>
#include <cstddef>

namespace fantasy {

// Minimal software audio mixer for Fantasy Pi
// Connects to Circle's sound device callback

constexpr uint32_t AUDIO_CHANNELS = 8;
constexpr uint32_t AUDIO_SAMPLE_RATE = 44100;
constexpr uint32_t AUDIO_BUFFER_SIZE = 2048;

struct AudioSample {
    const int16_t* data;
    uint32_t size;       // in frames
    uint32_t position;   // current playback position
    uint32_t volume;     // 0-255
    bool playing;
};

class AudioMixer {
public:
    AudioMixer();
    ~AudioMixer();

    bool Init();
    void Shutdown();

    // Load a sample (data is copied)
    bool LoadSample(uint32_t id, const int16_t* data, uint32_t frames);

    // Play/stop channels
    void Play(uint32_t sample_id, uint32_t channel, uint32_t volume);
    void Stop(uint32_t channel);

    // Generate audio buffer (called by Circle sound callback)
    void Mix(int16_t* buffer, uint32_t frames);

private:
    AudioSample channels_[AUDIO_CHANNELS];
    int16_t* sample_pool_;
    size_t sample_pool_used_;
    static constexpr size_t SAMPLE_POOL_SIZE = 8 * 1024 * 1024; // 8MB
};

} // namespace fantasy
