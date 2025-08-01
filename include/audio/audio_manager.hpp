#ifndef VOICE_ENGINE_AUDIO_MANAGER_HPP
#define VOICE_ENGINE_AUDIO_MANAGER_HPP

#include "core/non_copyable.hpp"
#include <portaudio.h>
#include <vector>
#include <functional>
#include <cstdint>
#include <atomic>

namespace audio {
    class AudioManager : private core::NonCopyable {
    public:
        using InputCallback = std::function<void(const std::vector<int16_t>&)>;
        using OutputCallback = std::function<void(std::vector<int16_t>&)>;

        static constexpr int SAMPLE_RATE = 48000;
        static constexpr int NUM_CHANNELS = 1;
        static constexpr PaSampleFormat FORMAT = paInt16;
        static constexpr int FRAMES_PER_BUFFER = 480; // 10ms

        AudioManager();
        ~AudioManager();

        bool start(InputCallback input_cb, OutputCallback output_cb);
        void stop();
        bool is_active() const;

    private:
        static int pa_callback(const void* input, void* output, unsigned long frame_count,
                               const PaStreamCallbackTimeInfo* time_info,
                               PaStreamCallbackFlags status_flags, void* user_data);
        
        int process(const int16_t* input, int16_t* output, unsigned long frame_count);

        PaStream* stream_ = nullptr;
        InputCallback input_callback_;
        OutputCallback output_callback_;
        std::atomic<bool> is_active_{false};
    };
}

#endif