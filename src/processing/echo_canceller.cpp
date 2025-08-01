#include "processing/echo_canceller.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace processing {

EchoCanceller::EchoCanceller(size_t filter_length, float step_size)
    : filter_length_(filter_length),
      step_size_(step_size),
      filter_weights_(filter_length, 0.0f),
      reference_buffer_(filter_length * 2, 0.0f),
      error_buffer_(128, 0.0f),
      playback_buffer_(filter_length * 4, 0),
      playback_write_pos_(0),
      error_power_(1.0f),
      reference_power_(1.0f),
      alpha_power_(0.99f) {}

void EchoCanceller::on_playback(const std::vector<int16_t>& samples) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& sample : samples) {
        playback_buffer_[playback_write_pos_] = sample;
        playback_write_pos_ = (playback_write_pos_ + 1) % playback_buffer_.size();
        
        // Reference buffer için float'a çevir ve normalize et
        float normalized_sample = static_cast<float>(sample) / 32768.0f;
        
        // Reference buffer'ı kaydır ve yeni sample'ı ekle
        std::copy(reference_buffer_.begin() + 1, reference_buffer_.end(), reference_buffer_.begin());
        reference_buffer_.back() = normalized_sample;
    }
}

void EchoCanceller::process(std::vector<int16_t>& capture) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& sample : capture) {
        float input = static_cast<float>(sample) / 32768.0f;
        
        // LMS adaptive filtering
        float echo_estimate = 0.0f;
        for (size_t i = 0; i < filter_length_; ++i) {
            echo_estimate += filter_weights_[i] * reference_buffer_[filter_length_ - 1 - i];
        }
        
        // Error signal (eko iptal edilmiş sinyal)
        float error = input - echo_estimate;
        
        // LMS weight update
        update_statistics(error, reference_buffer_[filter_length_ - 1]);
        
        // Adaptif step size hesaplama
        float adaptive_step = step_size_;
        if (reference_power_ > 1e-6f) {
            adaptive_step = step_size_ / (1.0f + reference_power_);
        }
        
        // Weight güncelleme (sadece referans sinyali varsa)
        if (reference_power_ > 1e-6f) {
            for (size_t i = 0; i < filter_length_; ++i) {
                filter_weights_[i] += adaptive_step * error * reference_buffer_[filter_length_ - 1 - i];
            }
        }
        
        // Çıkış sinyalini normalize et ve clamp et
        float output = error;
        
        // Residual echo suppression
        if (std::abs(output) < 0.01f && reference_power_ > 1e-4f) {
            output *= 0.5f; // Hafif bastırma
        }
        
        output = std::clamp(output, -1.0f, 1.0f);
        sample = static_cast<int16_t>(output * 32767.0f);
    }
}

void EchoCanceller::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fill(filter_weights_.begin(), filter_weights_.end(), 0.0f);
    std::fill(reference_buffer_.begin(), reference_buffer_.end(), 0.0f);
    std::fill(playback_buffer_.begin(), playback_buffer_.end(), 0);
    playback_write_pos_ = 0;
    error_power_ = 1.0f;
    reference_power_ = 1.0f;
}

float EchoCanceller::lms_step(float reference, float error) {
    return step_size_ * error * reference;
}

float EchoCanceller::calculate_power(const std::vector<float>& signal, size_t length) {
    float power = 0.0f;
    size_t start = signal.size() >= length ? signal.size() - length : 0;
    for (size_t i = start; i < signal.size(); ++i) {
        power += signal[i] * signal[i];
    }
    return power / static_cast<float>(signal.size() - start);
}

void EchoCanceller::update_statistics(float error, float reference) {
    error_power_ = alpha_power_ * error_power_ + (1.0f - alpha_power_) * error * error;
    reference_power_ = alpha_power_ * reference_power_ + (1.0f - alpha_power_) * reference * reference;
}

}
