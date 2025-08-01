// src/tools/network_test.cpp - UDP bağlantısını test etmek için basit utility

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

// Renkli çıktı için ANSI kodları
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

class NetworkTester {
public:
    NetworkTester() {
#ifdef _WIN32
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif
    }

    ~NetworkTester() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void test_connection(const std::string& target_ip, int send_port, int listen_port) {
        std::cout << CYAN << "🧪 Network Connection Test" << RESET << std::endl;
        std::cout << "   " << BLUE << "📡 Target: " << target_ip << ":" << send_port << RESET << std::endl;
        std::cout << "   " << BLUE << "📻 Listen: Port " << listen_port << RESET << std::endl;

        // Test mesajları
        std::vector<std::string> test_messages = {
            "NOVAENGINE_PING_001",
            "VOICE_ENGINE_READY",
            "AUDIO_TEST_PACKET",
            "CONNECTION_VERIFIED",
            "UDP_TUNNEL_CHECK"
        };

        // Sender socket oluştur
#ifdef _WIN32
        SOCKET sender_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sender_socket == INVALID_SOCKET) {
#else
        int sender_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sender_socket < 0) {
#endif
            std::cerr << RED << "❌ Failed to create sender socket!" << RESET << std::endl;
            return;
        }

        // Server address ayarla
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(send_port);

        if (inet_pton(AF_INET, target_ip.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << RED << "❌ Invalid IP address: " << target_ip << RESET << std::endl;
#ifdef _WIN32
            closesocket(sender_socket);
#else
            close(sender_socket);
#endif
            return;
        }

        // Test mesajlarını gönder
        std::cout << "\n" << YELLOW << "📤 Sending test packets..." << RESET << std::endl;
        int success_count = 0;

        auto start_time = std::chrono::steady_clock::now();

        for (size_t i = 0; i < test_messages.size(); ++i) {
            const auto& msg = test_messages[i];

            // Timestamp ekle
            auto now = std::chrono::steady_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - start_time).count();

            std::string full_msg = msg + "_" + std::to_string(timestamp);

            ssize_t sent = sendto(sender_socket, full_msg.c_str(), full_msg.length(), 0,
                                 (const sockaddr*)&server_addr, sizeof(server_addr));

            if (sent > 0) {
                std::cout << "   " << GREEN << "✅ Packet " << (i+1) << " sent: "
                          << msg << " (" << sent << " bytes)" << RESET << std::endl;
                success_count++;
            } else {
                std::cout << "   " << RED << "❌ Packet " << (i+1) << " FAILED!" << RESET << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

#ifdef _WIN32
        closesocket(sender_socket);
#else
        close(sender_socket);
#endif

        std::cout << "\n" << MAGENTA << "📊 Test Results:" << RESET << std::endl;
        std::cout << "   " << GREEN << "✅ Successful: " << success_count << "/" << test_messages.size() << RESET << std::endl;

        if (success_count == test_messages.size()) {
            std::cout << "   " << GREEN << "🎉 All packets sent successfully!" << RESET << std::endl;
            std::cout << "   " << CYAN << "💡 If the receiver gets these packets, UDP connection is working." << RESET << std::endl;
        } else {
            std::cout << "   " << YELLOW << "⚠️  Some packets failed to send." << RESET << std::endl;
            std::cout << "   " << CYAN << "💡 Check firewall settings and network connectivity." << RESET << std::endl;
        }
    }

    void listen_for_tests(int port) {
        std::cout << CYAN << "🔍 Listening for Test Packets" << RESET << std::endl;
        std::cout << "   " << BLUE << "📻 Port: " << port << RESET << std::endl;
        std::cout << "   " << YELLOW << "Press Ctrl+C to stop" << RESET << std::endl;

#ifdef _WIN32
        SOCKET listen_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (listen_socket == INVALID_SOCKET) {
#else
        int listen_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (listen_socket < 0) {
#endif
            std::cerr << RED << "❌ Failed to create listen socket!" << RESET << std::endl;
            return;
        }

        // Reuse address
        int opt = 1;
        setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (bind(listen_socket, (const sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << RED << "❌ Failed to bind to port " << port << "!" << RESET << std::endl;
#ifdef _WIN32
            closesocket(listen_socket);
#else
            close(listen_socket);
#endif
            return;
        }

        std::cout << GREEN << "✅ Listening on port " << port << "..." << RESET << std::endl;
        std::cout << "\n" << YELLOW << "📨 Waiting for packets..." << RESET << std::endl;

        char buffer[2048];
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int message_count = 0;
        auto start_time = std::chrono::steady_clock::now();

        while (true) {
            int received = recvfrom(listen_socket, buffer, sizeof(buffer) - 1, 0,
                                  (sockaddr*)&client_addr, &client_len);

            if (received > 0) {
                buffer[received] = '\0';
                message_count++;

                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - start_time).count();

                std::cout << GREEN << "📨 #" << message_count << " [+" << elapsed << "ms]: "
                          << RESET << "\"" << buffer << "\" "
                          << CYAN << "from " << client_ip << ":" << ntohs(client_addr.sin_port)
                          << " (" << received << " bytes)" << RESET << std::endl;

                // NovaEngine paketlerini özel olarak işaretle
                if (std::string(buffer).find("NOVAENGINE") != std::string::npos ||
                    std::string(buffer).find("VOICE_ENGINE") != std::string::npos) {
                    std::cout << "   " << MAGENTA << "🎙️  NovaEngine packet detected!" << RESET << std::endl;
                }

            } else if (received < 0) {
                std::cerr << RED << "❌ Receive error!" << RESET << std::endl;
                break;
            }
        }

#ifdef _WIN32
        closesocket(listen_socket);
#else
        close(listen_socket);
#endif

        std::cout << "\n" << MAGENTA << "📊 Session Summary:" << RESET << std::endl;
        std::cout << "   " << GREEN << "Total packets received: " << message_count << RESET << std::endl;
    }
};

void print_test_usage(const char* program_name) {
    std::cout << "\n" << CYAN << "🧪 NovaEngine Network Tester v1.0" << RESET << "\n" << std::endl;

    std::cout << YELLOW << "Mode 1 - Send Test Packets:" << RESET << std::endl;
    std::cout << "  " << program_name << " send <target_ip> <target_port> <local_listen_port>" << std::endl;

    std::cout << "\n" << YELLOW << "Mode 2 - Listen for Test Packets:" << RESET << std::endl;
    std::cout << "  " << program_name << " listen <listen_port>" << std::endl;

    std::cout << "\n" << GREEN << "Example Usage:" << RESET << std::endl;
    std::cout << "  " << BLUE << "Computer A: " << RESET << program_name << " listen 9001" << std::endl;
    std::cout << "  " << BLUE << "Computer B: " << RESET << program_name << " send 192.168.1.100 9001 9002" << std::endl;

    std::cout << "\n" << MAGENTA << "💡 Tips:" << RESET << std::endl;
    std::cout << "  • Test your network connectivity before running voice_engine" << std::endl;
    std::cout << "  • Make sure firewall allows UDP traffic on specified ports" << std::endl;
    std::cout << "  • Use different ports for sending and receiving" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << CYAN << "🚀 NovaEngine Network Tester" << RESET << std::endl;
    std::cout << "================================" << std::endl;

    try {
        if (argc < 2) {
            print_test_usage(argv[0]);
            return 1;
        }

        NetworkTester tester;
        std::string mode = argv[1];

        if (mode == "send" && argc == 5) {
            std::string target_ip = argv[2];
            int send_port = std::stoi(argv[3]);
            int listen_port = std::stoi(argv[4]);

            tester.test_connection(target_ip, send_port, listen_port);

        } else if (mode == "listen" && argc == 3) {
            int listen_port = std::stoi(argv[2]);
            tester.listen_for_tests(listen_port);

        } else {
            std::cerr << RED << "❌ Invalid arguments!" << RESET << std::endl;
            print_test_usage(argv[0]);
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << RED << "❌ ERROR: " << e.what() << RESET << std::endl;
        return 1;
    }

    return 0;
}