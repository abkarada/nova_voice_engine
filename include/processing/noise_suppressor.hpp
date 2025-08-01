#ifndef VOICE_ENGINE_NOISE_SUPPRESSOR_HPP
#define VOICE_ENGINE_NOISE_SUPPRESSOR_HPP

#include <vector>
#include <cstdint>
#include <complex>
#include <cmath>

namespace processing {
    class NoiseSuppressor {
    public:
        explicit NoiseSuppressor(size_t frame_size = 256, float noise_threshold = 0.02f, float over_subtraction = 2.0f);
        void process(std::vector<int16_t>& samples);
        void reset();
        
    private:
        // Frame processing parameters
        size_t frame_size_;
        size_t overlap_size_;
        size_t hop_size_;
        
        // Noise suppression parameters
        float noise_threshold_;
        float over_subtraction_factor_;
        float spectral_floor_;
        float alpha_noise_;
        
        // Buffers
        std::vector<float> input_buffer_;
        std::vector<float> output_buffer_;
        std::vector<float> window_;
        std::vector<float> overlap_buffer_;
        
        // Spectral analysis
        std::vector<std::complex<float>> fft_buffer_;
        std::vector<float> magnitude_spectrum_;
        std::vector<float> phase_spectrum_;
        std::vector<float> noise_spectrum_;
        std::vector<float> gain_spectrum_;
        
        // VAD (Voice Activity Detection)
        float energy_threshold_;
        float zero_crossing_threshold_;
        std::vector<float> energy_history_;
        bool voice_activity_;
        
        size_t buffer_pos_;
        size_t samples_processed_;
        
        // Helper functions
        void process_frame();
        void compute_fft(const std::vector<float>& input, std::vector<std::complex<float>>& output);
        void compute_ifft(const std::vector<std::complex<float>>& input, std::vector<float>& output);
        void apply_window(std::vector<float>& frame);
        void update_noise_estimate(const std::vector<float>& magnitude);
        bool detect_voice_activity(const std::vector<float>& frame);
        void compute_wiener_gains(const std::vector<float>& magnitude);
        float calculate_energy(const std::vector<float>& frame);
        float calculate_zero_crossings(const std::vector<float>& frame);
    };
}

#endif
