#include "app/application.hpp"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>

// Global değişken - sinyal yakalama için
std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int signal) {
    std::cout << "\n🛑 Kapatma sinyali alındı (" << signal << "), güvenli kapatma başlatılıyor..." << std::endl;
    g_shutdown_requested = true;
}

void print_usage(const char* program_name) {
    std::cout << "\n🎙️ NovaEngine Voice Engine\n" << std::endl;
    std::cout << "Kullanım: " << program_name << " <hedef_ip> <gönderme_portu> <dinleme_portu>\n" << std::endl;
    std::cout << "Örnekler:" << std::endl;
    std::cout << "  " << program_name << " 127.0.0.1 9001 9002    # Lokal test" << std::endl;
    std::cout << "  " << program_name << " 192.168.1.100 5000 5001 # LAN üzerinden" << std::endl;
    std::cout << "\nNot: Her iki tarafta da farklı portlar kullanın!" << std::endl;
    std::cout << "     Örneğin A bilgisayarı: 9001'e gönder, 9002'yi dinle" << std::endl;
    std::cout << "            B bilgisayarı: 9002'ye gönder, 9001'i dinle" << std::endl;
}

bool validate_port(int port) {
    if (port < 1024 || port > 65535) {
        std::cerr << "❌ HATA: Port numarası 1024-65535 aralığında olmalıdır. Verilen: " << port << std::endl;
        return false;
    }
    return true;
}

bool validate_ip(const std::string& ip) {
    // Basit IP validasyonu - daha detaylı kontrol yapılabilir
    if (ip.empty()) {
        std::cerr << "❌ HATA: IP adresi boş olamaz." << std::endl;
        return false;
    }

    if (ip == "localhost") {
        std::cout << "ℹ️  'localhost' → '127.0.0.1' olarak çevrilecek" << std::endl;
        return true;
    }

    // Basit format kontrolü
    size_t dot_count = 0;
    for (char c : ip) {
        if (c == '.') {
            dot_count++;
        } else if (!std::isdigit(c)) {
            std::cerr << "❌ HATA: Geçersiz IP adresi formatı: " << ip << std::endl;
            return false;
        }
    }

    if (dot_count != 3) {
        std::cerr << "❌ HATA: IP adresi 4 bölümden oluşmalıdır: " << ip << std::endl;
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    // Sinyal yakalayıcıları kur
    std::signal(SIGINT, signal_handler);   // Ctrl+C
    std::signal(SIGTERM, signal_handler);  // Terminate
#ifndef _WIN32
    std::signal(SIGHUP, signal_handler);   // Hangup (Unix/Linux)
#endif

    std::cout << "🎙️ NovaEngine Voice Engine v1.0" << std::endl;
    std::cout << "=================================" << std::endl;

    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    try {
        std::string target_ip = argv[1];

        // localhost'u çevir
        if (target_ip == "localhost") {
            target_ip = "127.0.0.1";
        }

        int send_port, listen_port;

        // Port numaralarını parse et
        try {
            send_port = std::stoi(argv[2]);
            listen_port = std::stoi(argv[3]);
        } catch (const std::exception& e) {
            std::cerr << "❌ HATA: Port numaraları geçerli tamsayılar olmalıdır." << std::endl;
            print_usage(argv[0]);
            return 1;
        }

        // Validasyonlar
        if (!validate_ip(target_ip)) {
            return 1;
        }

        if (!validate_port(send_port) || !validate_port(listen_port)) {
            return 1;
        }

        if (send_port == listen_port) {
            std::cerr << "❌ HATA: Gönderme ve dinleme portları aynı olamaz!" << std::endl;
            std::cerr << "   Gönderme portu: " << send_port << std::endl;
            std::cerr << "   Dinleme portu: " << listen_port << std::endl;
            return 1;
        }

        std::cout << "\n✅ Parametreler doğrulandı:" << std::endl;
        std::cout << "   📡 Hedef: " << target_ip << ":" << send_port << std::endl;
        std::cout << "   📻 Dinleme: Port " << listen_port << std::endl;

        // Uygulamayı başlat
        std::cout << "\n🚀 Uygulama başlatılıyor..." << std::endl;
        app::Application app;

        // Eğer sinyal geldiyse, çalıştırma
        if (g_shutdown_requested) {
            std::cout << "🛑 Başlatma esnasında durdurma sinyali alındı." << std::endl;
            return 0;
        }

        app.run(target_ip, send_port, listen_port);

    } catch (const std::invalid_argument& e) {
        std::cerr << "❌ HATA: Geçersiz argüman - " << e.what() << std::endl;
        print_usage(argv[0]);
        return 1;
    } catch (const std::out_of_range& e) {
        std::cerr << "❌ HATA: Sayı aralık dışında - " << e.what() << std::endl;
        print_usage(argv[0]);
        return 1;
    } catch (const std::runtime_error& e) {
        std::cerr << "❌ ÇALIŞMA ZAMANI HATASI: " << e.what() << std::endl;
        std::cerr << "\n🔧 Olası çözümler:" << std::endl;
        std::cerr << "   • Ses kartı bağlantılarını kontrol edin" << std::endl;
        std::cerr << "   • PortAudio ve Opus kütüphanelerinin yüklü olduğundan emin olun" << std::endl;
        std::cerr << "   • Firewall ayarlarını kontrol edin" << std::endl;
        std::cerr << "   • Başka bir uygulama portları kullanıyor olabilir" << std::endl;
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "❌ BEKLENMEDIK HATA: " << e.what() << std::endl;
        return 3;
    }

    std::cout << "\n✅ Program başarıyla sonlandırıldı." << std::endl;
    return 0;
}