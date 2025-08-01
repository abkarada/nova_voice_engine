#include "network/udp_sender.hpp"
#include <iostream>
#include <stdexcept>
#include <cstdio>

namespace network {
    UdpSender::UdpSender() {
#ifdef _WIN32
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data_) != 0) { throw std::runtime_error("WSAStartup basarisiz oldu."); }
#endif
    }

    UdpSender::~UdpSender() {
        if (socket_ != -1) {
#ifdef _WIN32
            closesocket(socket_);
            WSACleanup();
#else
            close(socket_);
#endif
        }
    }

    bool UdpSender::connect(const std::string& ip_address, int port) {
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
        if (socket_ == INVALID_SOCKET) {
#else
        if (socket_ < 0) {
#endif
            std::cerr << "HATA: Socket olusturulamadi." << std::endl;
            return false;
        }
        server_address_.sin_family = AF_INET;
        server_address_.sin_port = htons(port);
        int ip_result = inet_pton(AF_INET, ip_address.c_str(), &server_address_.sin_addr);
        if (ip_result <= 0) {
            std::cerr << "HATA: Gecersiz IP adresi: " << ip_address << std::endl;
            
#ifdef _WIN32
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
#else
            close(socket_);
            socket_ = -1;
#endif
            return false;
        }
        std::cout << "Sender " << ip_address << ":" << port << " adresine baglanmaya hazir." << std::endl;
        return true;
    }

    void UdpSender::send(const core::Packet& packet) {
        auto bytes = packet.to_bytes();
        ssize_t sent = sendto(socket_, reinterpret_cast<const char*>(bytes.data()),
                              bytes.size(), 0,
                              (const sockaddr*)&server_address_,
                              sizeof(server_address_));
        if (sent < 0) {
            std::perror("sendto");
        }
    }

    void UdpSender::send(const std::vector<core::Packet>& packets) {
        for (const auto& packet : packets) { send(packet); }
    }
}