#ifndef VOICE_ENGINE_ECHO_CANCELLER_HPP
#define VOICE_ENGINE_ECHO_CANCELLER_HPP

#include <vector>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <memory>

namespace processing {
    class EchoCanceller {
    public:
        explicit EchoCanceller(size_t filter_length = 512, float step_size = 0.01f);
        void on_playback(const std::vector<int16_t>& samples);
        void process(std::vector<int16_t>& capture);
        void reset();
        
    private:
        // Adaptive filter parameters
        size_t filter_length_;
        float step_size_;
        
        // Buffers for LMS algorithm
        std::vector<float> filter_weights_;
        std::vector<float> reference_buffer_;
        std::vector<float> error_buffer_;
        
        // Signal buffers
        std::vector<int16_t> playback_buffer_;
        size_t playback_write_pos_;
        
        // Statistics for adaptation control
        float error_power_;
        float reference_power_;
        float alpha_power_;
        
        std::mutex mutex_;
        
        // Helper functions
        float lms_step(float reference, float error);
        float calculate_power(const std::vector<float>& signal, size_t length);
        void update_statistics(float error, float reference);
    };
}

#endif
