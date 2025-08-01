#ifndef VOICE_ENGINE_ECHO_CANCELLER_HPP
#define VOICE_ENGINE_ECHO_CANCELLER_HPP

#include <vector>
#include <cstdint>
#include <mutex>

namespace processing {
    class EchoCanceller {
    public:
        explicit EchoCanceller(size_t filter_length = 1024, float step_size = 0.5f);
        void on_playback(const std::vector<int16_t>& samples);
        void process(std::vector<int16_t>& capture);
        void reset();

    private:
        const size_t filter_length_;
        const float step_size_; // NLMS için 'mu'
        const float epsilon_;   // Stabilizasyon için küçük bir değer

        std::vector<float> filter_weights_;
        std::vector<float> reference_buffer_; // Playback sinyali için buffer
        
        std::mutex mutex_;
    };
}

#endif
