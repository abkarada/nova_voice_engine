#include "processing/echo_canceller.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace processing {

EchoCanceller::EchoCanceller(size_t filter_length, float step_size)
    : filter_length_(filter_length),
      step_size_(step_size),
      epsilon_(1e-6f),
      filter_weights_(filter_length, 0.0f),
      reference_buffer_(filter_length, 0.0f) {}

void EchoCanceller::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fill(filter_weights_.begin(), filter_weights_.end(), 0.0f);
    std::fill(reference_buffer_.begin(), reference_buffer_.end(), 0.0f);
}

void EchoCanceller::on_playback(const std::vector<int16_t>& samples) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (int16_t sample : samples) {
        // Buffer'ı kaydır. Overlapping aralıklar nedeniyle move yerine
        // copy_backward kullanılır.
        std::copy_backward(reference_buffer_.begin(),
                           reference_buffer_.end() - 1,
                           reference_buffer_.end());

        // Yeni örneği ekle (normalize edilmiş)
        reference_buffer_[0] = static_cast<float>(sample) / 32768.0f;
    }
}

void EchoCanceller::process(std::vector<int16_t>& capture) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sample_ref : capture) {
        float mic_signal = static_cast<float>(sample_ref) / 32768.0f;

        // 1. Eko tahminini hesapla (filtering)
        float echo_estimate = 0.0f;
        for (size_t i = 0; i < filter_length_; ++i) {
            echo_estimate += filter_weights_[i] * reference_buffer_[i];
        }

        // 2. Hata sinyalini bul (temizlenmiş sinyal)
        float error_signal = mic_signal - echo_estimate;

        // 3. NLMS için referans sinyal gücünü hesapla
        float ref_power = 0.0f;
        for (float val : reference_buffer_) {
            ref_power += val * val;
        }

        // 4. Filtre katsayılarını güncelle (adaptation)
        if (ref_power > 0.0f) {
            float adaptive_step = step_size_ / (epsilon_ + ref_power);
            for (size_t i = 0; i < filter_length_; ++i) {
                filter_weights_[i] += adaptive_step * error_signal * reference_buffer_[i];
            }
        }
        
        // Çıktıyı int16'ya çevir
        sample_ref = static_cast<int16_t>(std::clamp(error_signal * 32768.0f, -32768.0f, 32767.0f));
    }
}

}
