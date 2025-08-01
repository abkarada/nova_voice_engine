#include "processing/noise_suppressor.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace processing {

NoiseSuppressor::NoiseSuppressor(int frame_size, float suppression_db)
    : frame_size_(frame_size),
      hop_size_(frame_size / 4), // %75 overlap
      suppression_gain_(std::pow(10.0f, suppression_db / 20.0f)),
      window_(frame_size),
      input_buffer_(frame_size, 0.0f),
      output_buffer_(frame_size, 0.0f),
      frame_buffer_(frame_size),
      output_frame_buffer_(frame_size),
      fft_buffer_(frame_size),
      magnitude_spectrum_(frame_size / 2 + 1),
      noise_spectrum_(frame_size / 2 + 1, 0.0f),
      input_buffer_pos_(0),
      output_buffer_pos_(0),
      alpha_noise_(0.95f) {

    // Hanning window
    for (int i = 0; i < frame_size_; ++i) {
        window_[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (frame_size_ - 1)));
    }
    reset();
}

void NoiseSuppressor::reset() {
    std::fill(input_buffer_.begin(), input_buffer_.end(), 0.0f);
    std::fill(output_buffer_.begin(), output_buffer_.end(), 0.0f);
    // Gürültüyü küçük bir başlangıç gücüyle başlat
    std::fill(noise_spectrum_.begin(), noise_spectrum_.end(), 1e-6f);
    input_buffer_pos_ = 0;
    output_buffer_pos_ = hop_size_; // Gecikmeyi dengelemek için
}

void NoiseSuppressor::process(std::vector<int16_t>& samples) {
    for (size_t i = 0; i < samples.size(); ++i) {
        // Çıktı örneğini al ve int16'ya dönüştür
        float out_sample = output_buffer_[output_buffer_pos_];
        samples[i] = static_cast<int16_t>(std::clamp(out_sample * 32768.0f, -32768.0f, 32767.0f));
        
        // Girdi örneğini al ve input buffer'a yerleştir
        input_buffer_[input_buffer_pos_] = static_cast<float>(samples[i]) / 32768.0f;

        input_buffer_pos_++;
        output_buffer_pos_++;
        
        if (input_buffer_pos_ >= hop_size_) {
            process_frame();
            input_buffer_pos_ = 0;
        }

        if (output_buffer_pos_ >= hop_size_) {
            // Overlap-add için buffer'ları kaydır
            std::move(output_buffer_.begin() + hop_size_, output_buffer_.end(), output_buffer_.begin());
            std::fill(output_buffer_.begin() + (frame_size_ - hop_size_), output_buffer_.end(), 0.0f);
            output_buffer_pos_ = 0;
        }
    }
}

void NoiseSuppressor::process_frame() {
    // Input buffer'dan frame'e kopyala ve window uygula
    for(int i = 0; i < frame_size_; ++i) {
        frame_buffer_[i] = input_buffer_[i] * window_[i];
    }

    compute_fft(frame_buffer_, fft_buffer_);

    // Magnitude ve gürültü spektrumlarını güncelle
    for (size_t i = 0; i < magnitude_spectrum_.size(); ++i) {
        float mag = std::abs(fft_buffer_[i]);
        magnitude_spectrum_[i] = mag;

        // Basit VAD (Voice Activity Detection): Düşük enerjili framelerde gürültüyü güncelle
        // TODO: Daha gelişmiş bir VAD eklenebilir.
        if (mag < noise_spectrum_[i]) {
             noise_spectrum_[i] = alpha_noise_ * noise_spectrum_[i] + (1.0f - alpha_noise_) * mag;
        }
    }
    
    // Spektral çıkarma (spectral subtraction)
    for (size_t i = 0; i < magnitude_spectrum_.size(); ++i) {
        float mag = magnitude_spectrum_[i];
        float noise = noise_spectrum_[i];
        float gain = 1.0f;

        if (mag > noise) {
            gain = std::max(0.0f, 1.0f - (noise / mag));
        } else {
            gain = 0.0f;
        }
        
        gain = std::max(gain, suppression_gain_); // Minimum kazancı uygula
        
        // Frekans bin'ini kazançla çarp
        fft_buffer_[i] *= gain;
        // Nyquist frekansı için simetriği
        if (i > 0 && i < magnitude_spectrum_.size() - 1) {
             fft_buffer_[frame_size_ - i] = std::conj(fft_buffer_[i]);
        }
    }

    compute_ifft(fft_buffer_, output_frame_buffer_);
    
    // Overlap-add: İşlenmiş frame'i window ile çarpıp output buffer'a ekle
    for(int i = 0; i < frame_size_; ++i) {
        output_buffer_[i] += output_frame_buffer_[i] * window_[i];
    }
    
    // Input buffer'ı kaydır
    std::move(input_buffer_.begin() + hop_size_, input_buffer_.end(), input_buffer_.begin());
    std::fill(input_buffer_.begin() + (frame_size_ - hop_size_), input_buffer_.end(), 0.0f);
}


void NoiseSuppressor::compute_fft(const std::vector<float>& input, std::vector<std::complex<float>>& output) {
    for (size_t k = 0; k < frame_size_; ++k) {
        std::complex<float> sum(0.0f, 0.0f);
        for (size_t n = 0; n < frame_size_; ++n) {
            float angle = -2.0f * M_PI * k * n / frame_size_;
            sum += input[n] * std::complex<float>(std::cos(angle), std::sin(angle));
        }
        output[k] = sum;
    }
}

void NoiseSuppressor::compute_ifft(const std::vector<std::complex<float>>& input, std::vector<float>& output) {
    for (size_t n = 0; n < frame_size_; ++n) {
        std::complex<float> sum(0.0f, 0.0f);
        for (size_t k = 0; k < frame_size_; ++k) {
            float angle = 2.0f * M_PI * k * n / frame_size_;
            sum += input[k] * std::complex<float>(std::cos(angle), std::sin(angle));
        }
        output[n] = sum.real() / frame_size_;
    }
}

}
