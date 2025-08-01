#include "app/application.hpp"
#include <iostream>
#include <vector>
#include <numeric>
#include <cstring>

namespace app {

Application::Application() {
    try {
        audio_manager_    = std::make_unique<audio::AudioManager>();
        codec_            = std::make_unique<codec::OpusCodec>();
        slicer_           = std::make_unique<streaming::Slicer>();
        sender_           = std::make_unique<network::UdpSender>();
        receiver_         = std::make_unique<network::UdpReceiver>();
        collector_        = std::make_unique<streaming::Collector>();
        echo_canceller_   = std::make_unique<processing::EchoCanceller>(1024, 0.2f);
        noise_suppressor_ = std::make_unique<processing::NoiseSuppressor>(512, -20.0f);
    } catch (const std::exception& e) {
        std::cerr << "Uygulama başlatılırken kritik hata: " << e.what() << std::endl;
        throw;
    }
}

Application::~Application() {
    std::cout << "Uygulama sonlandırılıyor." << std::endl;
}

void Application::run(const std::string& target_ip, int send_port, int listen_port) {
    if (!sender_->connect(target_ip, send_port)) {
        std::cerr << "HATA: Sender bağlanamadı." << std::endl;
        return;
    }
    
    auto packet_callback = [this](core::Packet packet) { this->on_packet_received(std::move(packet)); };
    if (!receiver_->start(listen_port, packet_callback)) {
        std::cerr << "HATA: Receiver başlatılamadı." << std::endl;
        return;
    }

    auto input_callback = [this](const std::vector<int16_t>& data) { this->on_audio_input(data); };
    auto output_callback = [this](std::vector<int16_t>& data) { this->on_audio_output(data); };

    if (!audio_manager_->start(input_callback, output_callback)) {
        std::cerr << "HATA: AudioManager başlatılamadı." << std::endl;
        return;
    }

    std::cout << "\n>>> Voice Engine calisiyor... <<<" << std::endl;
    std::cout << ">>> Hedef: " << target_ip << ":" << send_port << std::endl;
    std::cout << ">>> Dinlenen Port: " << listen_port << std::endl;
    std::cout << ">>> Kapatmak icin Enter'a basin. <<<" << std::endl;
    std::cin.get();

    audio_manager_->stop();
    receiver_->stop();
}

// Mikrofondan ses geldiğinde bu fonksiyon tetiklenir
void Application::on_audio_input(const std::vector<int16_t>& input_data) {
    std::vector<int16_t> processed_data = input_data;

    // Eko iptali için önce hoparlöre ne gönderildiğini (playback) işlemeliyiz.
    // Bu, on_audio_output içinde yapılır.
    // Ardından mikrofon verisini (capture) işleriz.
    echo_canceller_->process(processed_data);
    noise_suppressor_->process(processed_data);

    auto encoded_data = codec_->encode(processed_data);
    if (encoded_data.empty()) return;
    
    auto packets = slicer_->slice(encoded_data, 1200);
    sender_->send(packets);
}

// Hoparlöre ses gönderileceği zaman bu fonksiyon tetiklenir
void Application::on_audio_output(std::vector<int16_t>& output_data) {
    std::lock_guard<std::mutex> lock(playback_mutex_);
    
    const size_t samples_needed = output_data.size();
    if (playback_buffer_.size() >= samples_needed) {
        std::memcpy(output_data.data(), playback_buffer_.data(), samples_needed * sizeof(int16_t));
        playback_buffer_.erase(playback_buffer_.begin(), playback_buffer_.begin() + samples_needed);
    } else {
        // Yeterli veri yoksa sessizlik gönder
        std::memset(output_data.data(), 0, samples_needed * sizeof(int16_t));
    }
    
    // Eko iptalicinin referans olarak kullanması için çalınan sesi ona gönder
    echo_canceller_->on_playback(output_data);
}

// Ağdan paket geldiğinde
void Application::on_packet_received(core::Packet packet) {
    auto collection_callback = [this](const std::vector<uint8_t>& data) { this->on_audio_collected(data); };
    collector_->collect(packet, collection_callback);
}

// Paketler birleşip tam bir ses verisi olduğunda
void Application::on_audio_collected(const std::vector<uint8_t>& encoded_data) {
    auto decoded_data = codec_->decode(encoded_data);
    if (decoded_data.empty()) return;

    // Çalınmak üzere veriyi buffer'a ekle
    std::lock_guard<std::mutex> lock(playback_mutex_);
    playback_buffer_.insert(playback_buffer_.end(), decoded_data.begin(), decoded_data.end());
}

}