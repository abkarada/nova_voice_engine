#include "app/application.hpp"
#include <iostream>

namespace app {
Application::Application() {
    try {
        capturer_        = std::make_unique<capture::AudioCapturer>();
        codec_           = std::make_unique<codec::OpusCodec>();
        slicer_          = std::make_unique<streaming::Slicer>();
        sender_          = std::make_unique<network::UdpSender>();
        receiver_        = std::make_unique<network::UdpReceiver>();
        collector_       = std::make_unique<streaming::Collector>();
        player_          = std::make_unique<playback::AudioPlayer>();
        // Kararlı ve yüksek kaliteli "altın standart" parametreler
        echo_canceller_  = std::make_unique<processing::EchoCanceller>(1024, 0.2f);
        noise_suppressor_= std::make_unique<processing::NoiseSuppressor>(512, -20.0f);

        player_->set_playback_callback([this](const std::vector<int16_t>& data){
            if (echo_canceller_) {
                echo_canceller_->on_playback(data);
            }
        });
    } catch (const std::exception& e) {
        std::cerr << "Uygulama başlatılırken kritik hata: " << e.what() << std::endl;
        throw;
    }
}

Application::~Application() {
    std::cout << "Uygulama sonlandırılıyor." << std::endl;
}

void Application::run(const std::string& target_ip, int send_port, int listen_port) {
    if (!sender_->connect(target_ip, send_port)) { std::cerr << "HATA: Sender bağlanamadı." << std::endl; return; }
    auto packet_callback = [this](core::Packet packet) { this->on_packet_received(std::move(packet)); };
    if (!receiver_->start(listen_port, packet_callback)) { std::cerr << "HATA: Receiver başlatılamadı." << std::endl; return; }
    if (!player_->start()) { std::cerr << "HATA: Player başlatılamadı." << std::endl; return; }
    auto capture_callback = [this](const std::vector<int16_t>& pcm_data) { this->on_audio_captured(pcm_data); };
    if (!capturer_->start(capture_callback)) { std::cerr << "HATA: Capturer başlatılamadı." << std::endl; return; }

    std::cout << "\n>>> Voice Engine calisiyor... <<<" << std::endl;
    std::cout << ">>> Hedef: " << target_ip << ":" << send_port << std::endl;
    std::cout << ">>> Dinlenen Port: " << listen_port << std::endl;
    std::cout << ">>> Kapatmak icin Enter'a basin. <<<" << std::endl;
    std::cin.get();

    capturer_->stop();
    player_->stop();
    receiver_->stop();
}

void Application::on_audio_captured(const std::vector<int16_t>& pcm_data) {
    std::vector<int16_t> processed = pcm_data;
    echo_canceller_->process(processed);
    noise_suppressor_->process(processed);
    auto encoded_data = codec_->encode(processed);
    if (encoded_data.empty()) return;
    auto packets = slicer_->slice(encoded_data, 1200);
    sender_->send(packets);
}

void Application::on_packet_received(core::Packet packet) {
    auto collection_callback = [this](const std::vector<uint8_t>& data) { this->on_audio_collected(data); };
    collector_->collect(packet, collection_callback);
}

void Application::on_audio_collected(const std::vector<uint8_t>& encoded_data) {
    auto decoded_data = codec_->decode(encoded_data);
    if (decoded_data.empty()) return;
    player_->submit_audio_data(decoded_data);
}

void Application::tune_echo_canceller(float step_size) {
    if (echo_canceller_) {
        echo_canceller_->reset();
        echo_canceller_ = std::make_unique<processing::EchoCanceller>(256, step_size);
        player_->set_playback_callback([this](const std::vector<int16_t>& data){
            echo_canceller_->on_playback(data);
        });
        std::cout << "Eko iptal edici step_size: " << step_size << " olarak ayarlandı." << std::endl;
    }
}

void Application::tune_noise_suppressor(float suppression_db) {
    if (noise_suppressor_) {
        noise_suppressor_.reset(new processing::NoiseSuppressor(512, suppression_db));
        std::cout << "Gürültü engelleyici suppression_db: " << suppression_db << " olarak ayarlandı." << std::endl;
    }
}

void Application::reset_audio_processing() {
    if (echo_canceller_) {
        echo_canceller_->reset();
        std::cout << "Eko iptal edici sıfırlandı." << std::endl;
    }
    if (noise_suppressor_) {
        noise_suppressor_->reset();
        std::cout << "Gürültü engelleyici sıfırlandı." << std::endl;
    }
}

void Application::print_audio_stats() {
    std::cout << "\n=== Ses İşleme Durumu ===" << std::endl;
    std::cout << "Eko İptal Edici: " << (echo_canceller_ ? "Aktif (LMS Adaptive Filter)" : "Deaktif") << std::endl;
    std::cout << "Gürültü Engelleyici: " << (noise_suppressor_ ? "Aktif (Spektral Analiz)" : "Deaktif") << std::endl;
    std::cout << "=========================" << std::endl;
}
}