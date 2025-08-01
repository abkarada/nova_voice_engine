#include "audio/audio_manager.hpp"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>

namespace audio {

// PortAudio'nun RAII tarzında başlatılması ve sonlandırılması için yardımcı sınıf
class PortAudioInitializer {
public:
    PortAudioInitializer() {
        err_ = Pa_Initialize();
        if (err_ != paNoError) {
            std::cerr << "PortAudio HATA: Pa_Initialize() - " << Pa_GetErrorText(err_) << std::endl;
        }
    }
    ~PortAudioInitializer() {
        if (err_ == paNoError) {
            Pa_Terminate();
            std::cout << "PortAudio başarıyla sonlandırıldı." << std::endl;
        }
    }
    PaError get_error() const { return err_; }
private:
    PaError err_;
};

// Bu, program başladığında PortAudio'yu başlatır ve bittiğinde sonlandırır.
static PortAudioInitializer pa_initializer;

AudioManager::AudioManager() {
    if (pa_initializer.get_error() != paNoError) {
        throw std::runtime_error("PortAudio başlatılamadı.");
    }
}

AudioManager::~AudioManager() {
    stop();
}

bool AudioManager::start(InputCallback input_cb, OutputCallback output_cb) {
    if (is_active_) {
        return true;
    }

    input_callback_ = std::move(input_cb);
    output_callback_ = std::move(output_cb);

    PaStreamParameters input_parameters;
    input_parameters.device = Pa_GetDefaultInputDevice();
    if (input_parameters.device == paNoDevice) {
        std::cerr << "HATA: Varsayılan giriş aygıtı bulunamadı." << std::endl;
        return false;
    }
    input_parameters.channelCount = NUM_CHANNELS;
    input_parameters.sampleFormat = FORMAT;
    input_parameters.suggestedLatency = Pa_GetDeviceInfo(input_parameters.device)->defaultLowInputLatency;
    input_parameters.hostApiSpecificStreamInfo = nullptr;

    PaStreamParameters output_parameters;
    output_parameters.device = Pa_GetDefaultOutputDevice();
    if (output_parameters.device == paNoDevice) {
        std::cerr << "HATA: Varsayılan çıkış aygıtı bulunamadı." << std::endl;
        return false;
    }
    output_parameters.channelCount = NUM_CHANNELS;
    output_parameters.sampleFormat = FORMAT;
    output_parameters.suggestedLatency = Pa_GetDeviceInfo(output_parameters.device)->defaultLowOutputLatency;
    output_parameters.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(
        &stream_,
        &input_parameters,
        &output_parameters,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff, // Kırpmayı önle, sinyali olduğu gibi al
        &AudioManager::pa_callback,
        this
    );

    if (err != paNoError) {
        std::cerr << "PortAudio HATA: Pa_OpenStream() - " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::cerr << "PortAudio HATA: Pa_StartStream() - " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        return false;
    }

    is_active_ = true;
    std::cout << "Full-duplex ses akışı başlatıldı." << std::endl;
    return true;
}

void AudioManager::stop() {
    if (!is_active_ || !stream_) {
        return;
    }
    Pa_StopStream(stream_);
    Pa_CloseStream(stream_);
    stream_ = nullptr;
    is_active_ = false;
    std::cout << "Ses akışı durduruldu." << std::endl;
}

bool AudioManager::is_active() const {
    return is_active_;
}

int AudioManager::pa_callback(const void* input, void* output, unsigned long frame_count,
                           const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* user_data) {
    return static_cast<AudioManager*>(user_data)->process(
        static_cast<const int16_t*>(input),
        static_cast<int16_t*>(output),
        frame_count
    );
}

int AudioManager::process(const int16_t* input_buffer, int16_t* output_buffer, unsigned long frame_count) {
    // 1. Gelen sesi (mikrofon) işlemesi için ana uygulamaya gönder
    if (input_callback_) {
        std::vector<int16_t> input_data(input_buffer, input_buffer + frame_count * NUM_CHANNELS);
        input_callback_(input_data);
    }

    // 2. Hoparlöre gönderilecek sesi ana uygulamadan al
    std::vector<int16_t> output_data(frame_count * NUM_CHANNELS, 0); // Varsayılan olarak sessizlik
    if (output_callback_) {
        output_callback_(output_data);
    }
    
    // 3. Alınan sesi PortAudio'nun çıkış buffer'ına kopyala
    std::memcpy(output_buffer, output_data.data(), output_data.size() * sizeof(int16_t));

    return paContinue;
}

}