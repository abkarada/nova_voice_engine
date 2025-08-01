#include "app/application.hpp"
#include <iostream>
#include <vector>
#include <numeric>
#include <cstring>
#include <thread>
#include <chrono>

namespace app {

Application::Application() {
    try {
        audio_manager_    = std::make_unique<audio::AudioManager>();
        codec_            = std::make_unique<codec::OpusCodec>();
        slicer_           = std::make_unique<streaming::Slicer>();
        sender_           = std::make_unique<network::UdpSender>();
        receiver_         = std::make_unique<network::UdpReceiver>();
        collector_        = std::make_unique<streaming::Collector>();
        echo_canceller_   = std::make_unique<processing::EchoCanceller>(512, 0.1f); // Daha küçük filtre
        noise_suppressor_ = std::make_unique<processing::NoiseSuppressor>(256, -15.0f); // Daha az agresif

        // Playback buffer'ı başlangıçta sessizlik ile doldur
        playback_buffer_.resize(audio::AudioManager::FRAMES_PER_BUFFER * 10, 0);

        std::cout << "Tüm bileşenler başarıyla oluşturuldu." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Uygulama başlatılırken kritik hata: " << e.what() << std::endl;
        throw;
    }
}

Application::~Application() {
    std::cout << "Uygulama sonlandırılıyor." << std::endl;
    if (audio_manager_) {
        audio_manager_->stop();
    }
    if (receiver_) {
        receiver_->stop();
    }
}

void Application::run(const std::string& target_ip, int send_port, int listen_port) {
    std::cout << "Bağlantı kuruluyor..." << std::endl;

    // Önce receiver'ı başlat
    auto packet_callback = [this](core::Packet packet) {
        this->on_packet_received(std::move(packet));
    };

    if (!receiver_->start(listen_port, packet_callback)) {
        std::cerr << "HATA: Receiver başlatılamadı (Port: " << listen_port << ")" << std::endl;
        return;
    }
    std::cout << "✓ Receiver başlatıldı (Port: " << listen_port << ")" << std::endl;

    // Sonra sender'ı bağla
    if (!sender_->connect(target_ip, send_port)) {
        std::cerr << "HATA: Sender bağlanamadı (" << target_ip << ":" << send_port << ")" << std::endl;
        receiver_->stop();
        return;
    }
    std::cout << "✓ Sender bağlandı (" << target_ip << ":" << send_port << ")" << std::endl;

    // Kısa bir bekleme ile network'ün hazır olmasını sağla
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Audio callback'lerini ayarla
    auto input_callback = [this](const std::vector<int16_t>& data) {
        this->on_audio_input(data);
    };
    auto output_callback = [this](std::vector<int16_t>& data) {
        this->on_audio_output(data);
    };

    // Audio manager'ı başlat
    if (!audio_manager_->start(input_callback, output_callback)) {
        std::cerr << "HATA: AudioManager başlatılamadı." << std::endl;
        receiver_->stop();
        return;
    }
    std::cout << "✓ Audio Manager başlatıldı" << std::endl;

    std::cout << "\n🎙️ === Voice Engine Aktif ===" << std::endl;
    std::cout << "📡 Hedef: " << target_ip << ":" << send_port << std::endl;
    std::cout << "📻 Dinleme: Port " << listen_port << std::endl;
    std::cout << "🔊 Ses formatı: " << audio::AudioManager::SAMPLE_RATE << "Hz, "
              << audio::AudioManager::NUM_CHANNELS << " kanal" << std::endl;
    std::cout << "⏱️  Frame boyutu: " << audio::AudioManager::FRAMES_PER_BUFFER << " sample (10ms)" << std::endl;
    std::cout << "\n>>> Konuşmaya başlayabilirsiniz! <<<" << std::endl;
    std::cout << ">>> Durdurmak için Enter'a basın <<<\n" << std::endl;

    std::cin.get();

    std::cout << "\nSistem kapatılıyor..." << std::endl;
    audio_manager_->stop();
    receiver_->stop();
    std::cout << "✓ Tüm bileşenler güvenli şekilde kapatıldı." << std::endl;
}

// Mikrofondan ses geldiğinde bu fonksiyon tetiklenir
void Application::on_audio_input(const std::vector<int16_t>& input_data) {
    if (input_data.empty()) return;

    // Ses seviyesi kontrolü - çok sessiz sinyalleri görmezden gel
    float rms = 0.0f;
    for (const auto& sample : input_data) {
        rms += static_cast<float>(sample * sample);
    }
    rms = std::sqrt(rms / input_data.size()) / 32768.0f;

    // Debug: Ses seviyesini göster (çok sessiz değilse)
    static int debug_counter = 0;
    if (++debug_counter % 100 == 0 && rms > 0.01f) { // Her 1 saniyede bir
        std::cout << "🎤 Mikrofon RMS: " << (rms * 100.0f) << "%" << std::endl;
    }

    // Çok sessiz sesleri filtrelemek için threshold
    if (rms < 0.005f) {
        return; // Çok sessiz, gönderme
    }

    std::vector<int16_t> processed_data = input_data;

    // Echo cancellation - daha konservatif ayarlarla
    try {
        echo_canceller_->process(processed_data);
    } catch (const std::exception& e) {
        std::cerr << "Echo canceller hatası: " << e.what() << std::endl;
    }

    // Noise suppression - daha hafif işlem
    try {
        noise_suppressor_->process(processed_data);
    } catch (const std::exception& e) {
        std::cerr << "Noise suppressor hatası: " << e.what() << std::endl;
    }

    // Opus ile kodla
    std::vector<uint8_t> encoded_data;
    try {
        encoded_data = codec_->encode(processed_data);
    } catch (const std::exception& e) {
        std::cerr << "Encoding hatası: " << e.what() << std::endl;
        return;
    }

    if (encoded_data.empty()) {
        std::cerr << "Encoding boş sonuç döndürdü!" << std::endl;
        return;
    }

    // Debug: Kodlama başarısını göster
    static int encode_counter = 0;
    if (++encode_counter % 200 == 0) { // Her 2 saniyede bir
        std::cout << "📦 Encoded: " << encoded_data.size() << " bytes" << std::endl;
    }

    // Paketlere böl ve gönder
    try {
        auto packets = slicer_->slice(encoded_data, 1200);
        if (!packets.empty()) {
            sender_->send(packets);

            // Debug: Gönderme başarısını göster
            static int send_counter = 0;
            if (++send_counter % 200 == 0) { // Her 2 saniyede bir
                std::cout << "🚀 Gönderildi: " << packets.size() << " paket" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Packet gönderme hatası: " << e.what() << std::endl;
    }
}

// Hoparlöre ses gönderileceği zaman bu fonksiyon tetiklenir
void Application::on_audio_output(std::vector<int16_t>& output_data) {
    std::lock_guard<std::mutex> lock(playback_mutex_);

    const size_t samples_needed = output_data.size();

    if (playback_buffer_.size() >= samples_needed) {
        // Yeterli veri var, kopyala
        std::memcpy(output_data.data(), playback_buffer_.data(), samples_needed * sizeof(int16_t));
        playback_buffer_.erase(playback_buffer_.begin(), playback_buffer_.begin() + samples_needed);

        // Debug: Çalma başarısını göster
        static int play_counter = 0;
        if (++play_counter % 200 == 0) { // Her 2 saniyede bir
            std::cout << "🔊 Çalınıyor, buffer: " << playback_buffer_.size() << " sample" << std::endl;
        }
    } else {
        // Yeterli veri yok - sessizlik gönder ama buffer'ı koru
        std::fill(output_data.begin(), output_data.end(), 0);

        static int silence_counter = 0;
        if (++silence_counter % 500 == 0) { // Her 5 saniyede bir
            std::cout << "🔇 Buffer yetersiz, sessizlik çalınıyor (buffer: "
                      << playback_buffer_.size() << " sample)" << std::endl;
        }
    }

    // Echo canceller için referans sinyali gönder
    try {
        echo_canceller_->on_playback(output_data);
    } catch (const std::exception& e) {
        std::cerr << "Echo canceller playback hatası: " << e.what() << std::endl;
    }
}

// Ağdan paket geldiğinde
void Application::on_packet_received(core::Packet packet) {
    // Debug: Paket alma başarısını göster
    static int receive_counter = 0;
    if (++receive_counter % 200 == 0) { // Her 2 saniyede bir
        std::cout << "📨 Alındı: Seq=" << packet.sequence_number
                  << ", Size=" << packet.data.size() << " bytes" << std::endl;
    }

    try {
        auto collection_callback = [this](const std::vector<uint8_t>& data) {
            this->on_audio_collected(data);
        };
        collector_->collect(packet, collection_callback);
    } catch (const std::exception& e) {
        std::cerr << "Packet collection hatası: " << e.what() << std::endl;
    }
}

// Paketler birleşip tam bir ses verisi olduğunda
void Application::on_audio_collected(const std::vector<uint8_t>& encoded_data) {
    if (encoded_data.empty()) {
        return;
    }

    // Debug: Toplama başarısını göster
    static int collect_counter = 0;
    if (++collect_counter % 200 == 0) { // Her 2 saniyede bir
        std::cout << "🧩 Collected: " << encoded_data.size() << " bytes" << std::endl;
    }

    // Opus ile decode et
    std::vector<int16_t> decoded_data;
    try {
        decoded_data = codec_->decode(encoded_data);
    } catch (const std::exception& e) {
        std::cerr << "Decoding hatası: " << e.what() << std::endl;
        return;
    }

    if (decoded_data.empty()) {
        std::cerr << "Decoding boş sonuç döndürdü!" << std::endl;
        return;
    }

    // Çalınmak üzere veriyi buffer'a ekle
    {
        std::lock_guard<std::mutex> lock(playback_mutex_);
        playback_buffer_.insert(playback_buffer_.end(), decoded_data.begin(), decoded_data.end());

        // Buffer'ın çok büyümesini engelle (maksimum 1 saniye)
        const size_t max_buffer_size = audio::AudioManager::SAMPLE_RATE * audio::AudioManager::NUM_CHANNELS;
        if (playback_buffer_.size() > max_buffer_size) {
            playback_buffer_.erase(playback_buffer_.begin(),
                                 playback_buffer_.begin() + (playback_buffer_.size() - max_buffer_size));
        }

        // Debug: Buffer durumunu göster
        static int buffer_counter = 0;
        if (++buffer_counter % 200 == 0) { // Her 2 saniyede bir
            std::cout << "💾 Decoded: " << decoded_data.size()
                      << ", Buffer total: " << playback_buffer_.size() << " samples" << std::endl;
        }
    }
}

}