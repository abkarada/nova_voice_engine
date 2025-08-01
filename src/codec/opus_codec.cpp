#include "codec/opus_codec.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace codec {
    OpusCodec::OpusCodec(int sample_rate, int channels)
        : sample_rate_(sample_rate), channels_(channels), frame_size_(sample_rate / 100) { // 10ms frame

        int error;

        // Encoder oluştur
        encoder_ = opus_encoder_create(sample_rate_, channels_, OPUS_APPLICATION_VOIP, &error);
        if (error != OPUS_OK) {
            throw std::runtime_error("Opus encoder oluşturulamadı: " + std::string(opus_strerror(error)));
        }

        // Decoder oluştur
        decoder_ = opus_decoder_create(sample_rate_, channels_, &error);
        if (error != OPUS_OK) {
            opus_encoder_destroy(encoder_);
            throw std::runtime_error("Opus decoder oluşturulamadı: " + std::string(opus_strerror(error)));
        }

        // Encoder ayarlarını optimize et
        opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(64000));           // 64 kbps
        opus_encoder_ctl(encoder_, OPUS_SET_VBR(1));                  // Variable bitrate
        opus_encoder_ctl(encoder_, OPUS_SET_VBR_CONSTRAINT(0));       // Unconstrained VBR
        opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(5));           // Orta karmaşıklık
        opus_encoder_ctl(encoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE)); // Ses sinyali
        opus_encoder_ctl(encoder_, OPUS_SET_DTX(1));                  // Discontinuous transmission
        opus_encoder_ctl(encoder_, OPUS_SET_INBAND_FEC(1));          // Forward error correction

        std::cout << "✓ Opus codec başarıyla başlatıldı ("
                  << sample_rate_ << " Hz, " << channels_ << " kanal, "
                  << frame_size_ << " sample/frame)" << std::endl;
    }

    OpusCodec::~OpusCodec() {
        if (encoder_) {
            opus_encoder_destroy(encoder_);
            encoder_ = nullptr;
        }
        if (decoder_) {
            opus_decoder_destroy(decoder_);
            decoder_ = nullptr;
        }
        std::cout << "✓ Opus codec temizlendi." << std::endl;
    }

    std::vector<uint8_t> OpusCodec::encode(const std::vector<int16_t>& pcm_data) {
        if (!encoder_) {
            std::cerr << "HATA: Encoder mevcut değil!" << std::endl;
            return {};
        }

        if (pcm_data.empty()) {
            std::cerr << "HATA: Encode edilecek PCM verisi boş!" << std::endl;
            return {};
        }

        // Frame size kontrolü
        const size_t expected_samples = frame_size_ * channels_;
        if (pcm_data.size() != expected_samples) {
            std::cerr << "UYARI: PCM data boyutu beklenen boyutla eşleşmiyor. Beklenen: "
                      << expected_samples << ", Gelen: " << pcm_data.size() << std::endl;

            // Eğer veri çok küçükse, sıfırlarla doldur
            if (pcm_data.size() < expected_samples) {
                std::vector<int16_t> padded_data = pcm_data;
                padded_data.resize(expected_samples, 0);
                return encode(padded_data);  // Recursive call
            } else {
                // Eğer çok büyükse, kırp
                std::vector<int16_t> trimmed_data(pcm_data.begin(), pcm_data.begin() + expected_samples);
                return encode(trimmed_data);  // Recursive call
            }
        }

        // Maksimum compressed data boyutu (Opus için güvenli)
        std::vector<uint8_t> compressed_data(4000);

        opus_int32 result = opus_encode(encoder_, pcm_data.data(), frame_size_,
                                       compressed_data.data(), compressed_data.size());

        if (result < 0) {
            std::cerr << "HATA: Opus encode hatası: " << opus_strerror(result) << std::endl;
            return {};
        }

        if (result == 0) {
            std::cerr << "UYARI: Opus encode sıfır byte döndürdü (DTX aktif olabilir)" << std::endl;
            return {};
        }

        compressed_data.resize(result);

        // Debug bilgisi (nadiren)
        static int encode_debug_counter = 0;
        if (++encode_debug_counter % 1000 == 0) {  // Her 10 saniyede bir
            std::cout << "📊 Encode: " << pcm_data.size() << " → " << result << " bytes (sıkıştırma: "
                      << (100.0f * result / (pcm_data.size() * sizeof(int16_t))) << "%)" << std::endl;
        }

        return compressed_data;
    }

    std::vector<int16_t> OpusCodec::decode(const std::vector<uint8_t>& encoded_data) {
        if (!decoder_) {
            std::cerr << "HATA: Decoder mevcut değil!" << std::endl;
            return {};
        }

        if (encoded_data.empty()) {
            std::cerr << "HATA: Decode edilecek data boş!" << std::endl;
            return {};
        }

        // Decode buffer'ı oluştur (birkaç frame'lik alan bırak)
        const size_t max_samples = frame_size_ * channels_ * 6;  // 60ms için alan
        std::vector<int16_t> decoded_data(max_samples);

        int decoded_samples = opus_decode(decoder_, encoded_data.data(), encoded_data.size(),
                                         decoded_data.data(), frame_size_ * 6, 0);

        if (decoded_samples < 0) {
            std::cerr << "HATA: Opus decode hatası: " << opus_strerror(decoded_samples) << std::endl;
            return {};
        }

        if (decoded_samples == 0) {
            std::cerr << "UYARI: Opus decode sıfır sample döndürdü!" << std::endl;
            return {};
        }

        // Sonucu gerçek boyuta getir
        const size_t total_samples = decoded_samples * channels_;
        decoded_data.resize(total_samples);

        // Debug bilgisi (nadiren)
        static int decode_debug_counter = 0;
        if (++decode_debug_counter % 1000 == 0) {  // Her 10 saniyede bir
            std::cout << "📊 Decode: " << encoded_data.size() << " bytes → " << total_samples
                      << " samples (" << decoded_samples << " samples/channel)" << std::endl;
        }

        return decoded_data;
    }
}