#include <iostream>
#include <vector>
#include <whisper.h>
#include <sndfile.h>

//g++ -o wav_reader wav_reader.cpp -lsndfile
//./wav_reader

void recognizeSpeech(const std::vector<float>& audioData, const std::string& languageModelPath) {
    // Инициализация Whisper
    whisper_context* ctx = whisper_init_from_file(languageModelPath.c_str());

    /*
    // Передача аудио-байтов для распознавания речи
    if (whisper_add_audio(ctx, audioData.data(), audioData.size()) != 0) {
        std::cerr << "Error adding audio data to Whisper context." << std::endl;
        whisper_free(ctx);
        return;
    }
    */

    // Получение результата распознавания
    int n_segments = whisper_full_n_segments(ctx);

    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx, i);
        // Вывод распознанного текста на печать
        std::cout << "Распознанный текст сегмента " << i << ": " << text << std::endl;
    }

    // Освобождение ресурсов
    whisper_free(ctx);
}

int main() {
    // Путь к файлу с аудио в формате WAV
    std::string wavFilePath = "samples/jfkmy.wav";

    // Инициализация библиотеки libsndfile
    SF_INFO sndInfo;
    SNDFILE* sndFile = sf_open(wavFilePath.c_str(), SFM_READ, &sndInfo);
    if (!sndFile) {
        std::cerr << "Error opening the WAV file." << std::endl;
        return 1;
    }

    // Чтение аудио-байтов из файла
    std::vector<float> audioData(sndInfo.frames * sndInfo.channels);
    sf_readf_float(sndFile, audioData.data(), sndInfo.frames);
    sf_close(sndFile);

    // Путь к файлу с моделью языка Whisper
    std::string languageModelPath = "models/ggml-base.en.bin"; // это путь в файле stream.cpp

    // Распознавание речи
    recognizeSpeech(audioData, languageModelPath);

    return 0;
}
