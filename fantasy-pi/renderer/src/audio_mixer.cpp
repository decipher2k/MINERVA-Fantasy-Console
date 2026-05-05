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

#include "audio_mixer.h"
#include <cstring>
#include <algorithm>

namespace fantasy {

AudioMixer::AudioMixer()
    : sample_pool_(nullptr),
      sample_pool_used_(0)
{
    memset(channels_, 0, sizeof(channels_));
}

AudioMixer::~AudioMixer() {
    Shutdown();
}

bool AudioMixer::Init() {
    sample_pool_ = new int16_t[SAMPLE_POOL_SIZE / sizeof(int16_t)];
    sample_pool_used_ = 0;
    return true;
}

void AudioMixer::Shutdown() {
    delete[] sample_pool_;
    sample_pool_ = nullptr;
}

bool AudioMixer::LoadSample(uint32_t id, const int16_t* data, uint32_t frames) {
    // Store sample pointer in channel slot id for direct lookup
    if (id < AUDIO_CHANNELS) {
        channels_[id].data = data;
        channels_[id].size = frames;
        channels_[id].position = 0;
        channels_[id].volume = 255;
        channels_[id].playing = false;
        return true;
    }
    return false;
}

void AudioMixer::Play(uint32_t sample_id, uint32_t channel, uint32_t volume) {
    if (channel >= AUDIO_CHANNELS) return;
    if (sample_id >= AUDIO_CHANNELS) return;
    channels_[channel] = channels_[sample_id]; // copy sample metadata
    channels_[channel].position = 0;
    channels_[channel].volume = volume;
    channels_[channel].playing = true;
}

void AudioMixer::Stop(uint32_t channel) {
    if (channel >= AUDIO_CHANNELS) return;
    channels_[channel].playing = false;
}

void AudioMixer::Mix(int16_t* buffer, uint32_t frames) {
    memset(buffer, 0, frames * 2 * sizeof(int16_t)); // Stereo output

    for (uint32_t ch = 0; ch < AUDIO_CHANNELS; ch++) {
        AudioSample& sample = channels_[ch];
        if (!sample.playing || !sample.data) continue;

        for (uint32_t i = 0; i < frames; i++) {
            if (sample.position >= sample.size) {
                sample.playing = false;
                break;
            }
            int32_t left = buffer[i * 2];
            int32_t right = buffer[i * 2 + 1];
            int32_t s = sample.data[sample.position];
            s = (s * (int32_t)sample.volume) / 256;
            left += s;
            right += s;
            buffer[i * 2] = (int16_t)std::clamp(left, (int32_t)-32768, (int32_t)32767);
            buffer[i * 2 + 1] = (int16_t)std::clamp(right, (int32_t)-32768, (int32_t)32767);
            sample.position++;
        }
    }
}

} // namespace fantasy
