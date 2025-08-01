#ifndef VOICE_ENGINE_APPLICATION_HPP
#define VOICE_ENGINE_APPLICATION_HPP

#include "audio/audio_manager.hpp"
#include "codec/opus_codec.hpp"
#include "streaming/slicer.hpp"
#include "streaming/collector.hpp"
#include "network/udp_sender.hpp"
#include "network/udp_receiver.hpp"
#include "processing/echo_canceller.hpp"
#include "processing/noise_suppressor.hpp"
#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <mutex>

namespace app {
    class Application : private core::NonCopyable {
    public:
        Application();
        ~Application();
        void run(const std::string& target_ip, int send_port, int listen_port);

    private:
        // Ses akışını yöneten callback'ler
        void on_audio_input(const std::vector<int16_t>& input_data);
        void on_audio_output(std::vector<int16_t>& output_data);

        // Ağdan gelen veriyi işleyen callback'ler
        void on_packet_received(core::Packet packet);
        void on_audio_collected(const std::vector<uint8_t>& encoded_data);

        // Ses altyapısı
        std::unique_ptr<audio::AudioManager> audio_manager_;

        // Ses işleme ve iletim bileşenleri
        std::unique_ptr<codec::OpusCodec>       codec_;
        std::unique_ptr<streaming::Slicer>      slicer_;
        std::unique_ptr<network::UdpSender>     sender_;
        std::unique_ptr<network::UdpReceiver>   receiver_;
        std::unique_ptr<streaming::Collector>   collector_;
        
        // Ses işleme modülleri
        std::unique_ptr<processing::EchoCanceller> echo_canceller_;
        std::unique_ptr<processing::NoiseSuppressor> noise_suppressor_;
        
        // Ağdan gelen ve çalınacak olan ses verisi için güvenli buffer
        std::vector<int16_t> playback_buffer_;
        std::mutex playback_mutex_;
    };
}

#endif