#ifndef VOICE_ENGINE_APPLICATION_HPP
#define VOICE_ENGINE_APPLICATION_HPP

#include "core/non_copyable.hpp"
#include "capture/audio_capturer.hpp"
#include "codec/opus_codec.hpp"
#include "streaming/slicer.hpp"
#include "streaming/collector.hpp"
#include "network/udp_sender.hpp"
#include "network/udp_receiver.hpp"
#include "playback/audio_player.hpp"
#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace app {
    class Application : private core::NonCopyable {
    public:
        Application();
        ~Application();
        void run(const std::string& target_ip, int send_port, int listen_port);
    private:
        void on_audio_captured(const std::vector<int16_t>& pcm_data);
        void on_packet_received(core::Packet packet);
        void on_audio_collected(const std::vector<uint8_t>& encoded_data);

        std::unique_ptr<capture::AudioCapturer> capturer_;
        std::unique_ptr<codec::OpusCodec>       codec_;
        std::unique_ptr<streaming::Slicer>      slicer_;
        std::unique_ptr<network::UdpSender>     sender_;
        std::unique_ptr<network::UdpReceiver>   receiver_;
        std::unique_ptr<streaming::Collector>   collector_;
        std::unique_ptr<playback::AudioPlayer>  player_;
    };
}

#endif