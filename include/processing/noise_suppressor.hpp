#ifndef VOICE_ENGINE_NOISE_SUPPRESSOR_HPP
#define VOICE_ENGINE_NOISE_SUPPRESSOR_HPP

#include <vector>
#include <cstdint>
#include <complex>

namespace processing {
    class NoiseSuppressor {
    public:
        explicit NoiseSuppressor(int frame_size = 512, float suppression_db = -20.0f);
        void process(std::vector<int16_t>& samples);
        void reset();

    private:
        void process_frame();
        void compute_fft(const std::vector<float>& input, std::vector<std::complex<float>>& output);
        void compute_ifft(const std::vector<std::complex<float>>& input, std::vector<float>& output);

        const int frame_size_;
        const int hop_size_;
        const float suppression_gain_; // suppression_db'den çevrilen kazanç

        std::vector<float> window_;
        std::vector<float> input_buffer_;
        std::vector<float> output_buffer_;
        std::vector<float> frame_buffer_;
        std::vector<float> output_frame_buffer_;
        
        std::vector<std::complex<float>> fft_buffer_;
        std::vector<float> magnitude_spectrum_;
        std::vector<float> noise_spectrum_;

        int input_buffer_pos_;
        int output_buffer_pos_;
        
        float alpha_noise_; // Gürültü spektrumu için yumuşatma faktörü
    };
}

#endif
