#include "processing/noise_suppressor.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace processing {

NoiseSuppressor::NoiseSuppressor(size_t frame_size, float over_subtraction, float gain_smoothing)
    : frame_size_(frame_size),
      overlap_size_(frame_size / 2),
      hop_size_(frame_size / 2),
      over_subtraction_factor_(over_subtraction),
      spectral_floor_(0.02f),
      alpha_noise_(0.98f),
      alpha_gain_(gain_smoothing),
      input_buffer_(frame_size, 0.0f),
      output_buffer_(frame_size, 0.0f),
      window_(frame_size),
      fft_buffer_(frame_size),
      magnitude_spectrum_(frame_size / 2 + 1),
      phase_spectrum_(frame_size / 2 + 1),
      noise_spectrum_(frame_size / 2 + 1, 0.0f),
      gain_spectrum_(frame_size / 2 + 1, 1.0f),
      prev_gain_spectrum_(frame_size / 2 + 1, 1.0f),
      energy_threshold_(0.005f),
      zero_crossing_threshold_(0.2f),
      energy_history_(10, 0.0f),
      voice_activity_(false),
      buffer_pos_(0),
      samples_processed_(0) {
    
    // Hanning window oluştur
    for (size_t i = 0; i < frame_size_; ++i) {
        window_[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (frame_size_ - 1)));
    }
    // Gürültü spektrumunu küçük bir değerle başlat
    std::fill(noise_spectrum_.begin(), noise_spectrum_.end(), 0.01f);
}

void NoiseSuppressor::process(std::vector<int16_t>& samples) {
    for (size_t i = 0; i < samples.size(); ++i) {
        // Gelen sample'ı float'a çevir ve input buffer'a koy
        input_buffer_[buffer_pos_] = static_cast<float>(samples[i]) / 32768.0f;
        
        // İşlenmiş sample'ı output buffer'dan al ve kullanıcıya ver
        float output_sample = output_buffer_[buffer_pos_];
        output_sample = std::clamp(output_sample, -1.0f, 1.0f);
        samples[i] = static_cast<int16_t>(output_sample * 32767.0f);
        
        // Output buffer'daki eski veriyi temizle
        output_buffer_[buffer_pos_] = 0.0f;

        buffer_pos_++;
        
        // Eğer hop_size kadar sample biriktiyse, bir frame işle
        if (buffer_pos_ == hop_size_) {
            // Overlap olan kısmı kaydır
            std::copy(input_buffer_.begin() + hop_size_, input_buffer_.end(), input_buffer_.begin());
            // Buffer pozisyonunu sıfırla ve yeni sample'lar için yer aç
            buffer_pos_ = 0;
            process_frame();
        }
    }
}

void NoiseSuppressor::process_frame() {
    // Mevcut frame'i al
    std::vector<float> frame(frame_size_);
    size_t start_pos = (buffer_pos_ + input_buffer_.size() - frame_size_) % input_buffer_.size();
    
    for (size_t i = 0; i < frame_size_; ++i) {
        frame[i] = input_buffer_[(start_pos + i) % input_buffer_.size()];
    }
    
    // Voice Activity Detection
    voice_activity_ = detect_voice_activity(frame);
    
    // Window uygula
    apply_window(frame);
    
    // FFT hesapla
    compute_fft(frame, fft_buffer_);
    
    // Magnitude ve phase spektrumları hesapla
    for (size_t i = 0; i < magnitude_spectrum_.size(); ++i) {
        magnitude_spectrum_[i] = std::abs(fft_buffer_[i]);
        phase_spectrum_[i] = std::arg(fft_buffer_[i]);
    }
    
    // Noise estimate güncelle
    if (!voice_activity_) {
        update_noise_estimate(magnitude_spectrum_);
    }
    
    // Wiener gains hesapla
    compute_wiener_gains(magnitude_spectrum_);
    
    // Gain uygula
    for (size_t i = 0; i < fft_buffer_.size() && i < gain_spectrum_.size(); ++i) {
        fft_buffer_[i] *= gain_spectrum_[i];
    }
    
    // IFFT hesapla
    std::vector<float> output_frame(frame_size_);
    compute_ifft(fft_buffer_, output_frame);
    
    // Window uygula ve overlap-add
    for (size_t i = 0; i < frame_size_; ++i) {
        size_t buffer_idx = (start_pos + i) % input_buffer_.size();
        output_buffer_[buffer_idx] += output_frame[i] * window_[i];
    }
}

void NoiseSuppressor::compute_fft(const std::vector<float>& input, std::vector<std::complex<float>>& output) {
    // Basit DFT implementasyonu (performans için gerçek projelerde FFTW kullanılmalı)
    output.resize(input.size());
    const float N = static_cast<float>(input.size());
    
    for (size_t k = 0; k < input.size(); ++k) {
        std::complex<float> sum(0.0f, 0.0f);
        for (size_t n = 0; n < input.size(); ++n) {
            float angle = -2.0f * M_PI * k * n / N;
            sum += input[n] * std::complex<float>(std::cos(angle), std::sin(angle));
        }
        output[k] = sum;
    }
}

void NoiseSuppressor::compute_ifft(const std::vector<std::complex<float>>& input, std::vector<float>& output) {
    // Basit IDFT implementasyonu
    output.resize(input.size());
    const float N = static_cast<float>(input.size());
    
    for (size_t n = 0; n < input.size(); ++n) {
        std::complex<float> sum(0.0f, 0.0f);
        for (size_t k = 0; k < input.size(); ++k) {
            float angle = 2.0f * M_PI * k * n / N;
            sum += input[k] * std::complex<float>(std::cos(angle), std::sin(angle));
        }
        output[n] = sum.real() / N;
    }
}

void NoiseSuppressor::apply_window(std::vector<float>& frame) {
    for (size_t i = 0; i < frame.size() && i < window_.size(); ++i) {
        frame[i] *= window_[i];
    }
}

void NoiseSuppressor::update_noise_estimate(const std::vector<float>& magnitude) {
    for (size_t i = 0; i < noise_spectrum_.size() && i < magnitude.size(); ++i) {
        noise_spectrum_[i] = alpha_noise_ * noise_spectrum_[i] + 
                            (1.0f - alpha_noise_) * magnitude[i];
    }
}

bool NoiseSuppressor::detect_voice_activity(const std::vector<float>& frame) {
    float energy = calculate_energy(frame);
    float zero_crossings = calculate_zero_crossings(frame);
    
    // Energy history güncelle
    energy_history_.erase(energy_history_.begin());
    energy_history_.push_back(energy);
    
    float avg_energy = std::accumulate(energy_history_.begin(), energy_history_.end(), 0.0f) / energy_history_.size();
    
    // VAD decision
    bool high_energy = energy > energy_threshold_ && energy > avg_energy * 1.5f;
    bool low_zero_crossings = zero_crossings < zero_crossing_threshold_;
    
    return high_energy && low_zero_crossings;
}

void NoiseSuppressor::compute_wiener_gains(const std::vector<float>& magnitude) {
    // Önceki gain'leri bu frame için başlangıç noktası olarak sakla
    prev_gain_spectrum_ = gain_spectrum_;

    for (size_t i = 0; i < gain_spectrum_.size(); ++i) {
        // Sinyal-Gürültü Oranı (SNR) tahmini
        float signal_power = magnitude[i] * magnitude[i];
        float noise_power = noise_spectrum_[i] * noise_spectrum_[i];

        // Priori ve Posteriori SNR hesaplaması
        float post_snr = signal_power / (noise_power + 1e-6f);
        float prior_snr = alpha_gain_ * (prev_gain_spectrum_[i] * prev_gain_spectrum_[i]) * post_snr +
                          (1.0f - alpha_gain_) * std::max(0.0f, post_snr - 1.0f);

        // Wiener kazanç formülü
        float gain = prior_snr / (prior_snr + over_subtraction_factor_);
        
        // Kazancı spektral taban ile sınırla
        gain = std::max(gain, spectral_floor_);
        
        // Kazancı yumuşat
        gain_spectrum_[i] = alpha_gain_ * prev_gain_spectrum_[i] + (1.0f - alpha_gain_) * gain;
        gain_spectrum_[i] = std::clamp(gain_spectrum_[i], 0.0f, 1.0f);
    }
}

float NoiseSuppressor::calculate_energy(const std::vector<float>& frame) {
    float energy = 0.0f;
    for (const auto& sample : frame) {
        energy += sample * sample;
    }
    return energy / frame.size();
}

float NoiseSuppressor::calculate_zero_crossings(const std::vector<float>& frame) {
    if (frame.size() < 2) return 0.0f;
    
    int crossings = 0;
    for (size_t i = 1; i < frame.size(); ++i) {
        if ((frame[i] >= 0 && frame[i-1] < 0) || (frame[i] < 0 && frame[i-1] >= 0)) {
            crossings++;
        }
    }
    return static_cast<float>(crossings) / (frame.size() - 1);
}

void NoiseSuppressor::reset() {
    std::fill(input_buffer_.begin(), input_buffer_.end(), 0.0f);
    std::fill(output_buffer_.begin(), output_buffer_.end(), 0.0f);
    std::fill(noise_spectrum_.begin(), noise_spectrum_.end(), 0.01f);
    std::fill(gain_spectrum_.begin(), gain_spectrum_.end(), 1.0f);
    std::fill(prev_gain_spectrum_.begin(), prev_gain_spectrum_.end(), 1.0f);
    std::fill(energy_history_.begin(), energy_history_.end(), 0.0f);
    
    buffer_pos_ = 0;
    samples_processed_ = 0;
    voice_activity_ = false;
}

}
