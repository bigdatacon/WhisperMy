#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include "common.h"
#include "whisper.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <iostream>
#include <queue>


#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <streambuf>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool got_config = false;

using namespace std;

typedef websocketpp::server<websocketpp::config::asio> server;

std::queue<std::vector<float>> audio_queue;

/* Считать вектор вещественных чисел из сообщения от клиента */
//std::vector<float> read_float_vector(server::message_ptr msg) {
//    const char* data = msg->get_payload().c_str();
//    size_t length = msg->get_payload().length();
//
//    std::vector<float> pcmf32(reinterpret_cast<const float *>(data),
//                              reinterpret_cast<const float *>(data + length));
//    return pcmf32;
//}

std::vector<float> read_float_vector(websocketpp::server<websocketpp::config::asio>::message_ptr msg) {
    try {
//        std::cerr << "Пытаюсь прочитать байты от клиента" << std::endl;
        const char* data = msg->get_payload().c_str();
        size_t length = msg->get_payload().length();

        // Проверяем, что длина данных кратна размеру int16_t
        assert(length % sizeof(int16_t) == 0);

        size_t num_samples = length / sizeof(int16_t);
        std::vector<float> pcmf32(num_samples);

        // Преобразование каждого int16_t значения в float
        const int16_t* int16_data = reinterpret_cast<const int16_t*>(data);
        for (size_t i = 0; i < num_samples; ++i) {
            // Нормализация к диапазону от -1.0 до 1.0
            pcmf32[i] = int16_data[i] / static_cast<float>(INT16_MAX);
        }

        return pcmf32;
    } catch (const std::exception& e) {
        std::cerr << "Произошла ошибка при чтении байтов от клиента: " << e.what() << std::endl;
        return std::vector<float>(); // Возвращаем пустой вектор в случае ошибки
    } catch (...) {
        std::cerr << "Произошла неизвестная ошибка." << std::endl;
        return std::vector<float>(); // Возвращаем пустой вектор в случае неизвестной ошибки
    }
}

static websocketpp::connection_hdl last_handle;
/* Отправить клиенту распознанный текст, и сразу же вывести его на экран */
void send_text(server *s, websocketpp::connection_hdl hdl, const std::string &text)
{
    s->send(hdl, std::string(text), websocketpp::frame::opcode::text);
    std::cerr << "Отправляем клиенту распознанный текст: " << text << std::endl;
}


std::string to_timestamp(int64_t t) {
    int64_t sec = t/100;
    int64_t msec = t - sec*100;
    int64_t min = sec/60;
    sec = sec - min*60;

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d.%03d", (int) min, (int) sec, (int) msec);

    return std::string(buf);
}

// command-line parameters
struct whisper_params {
    int32_t n_threads  = std::min(4, (int32_t) std::thread::hardware_concurrency());
    int32_t step_ms    = 3000;
    int32_t length_ms  = 10000;
    int32_t keep_ms    = 200;
    int32_t capture_id = -1;
    int32_t max_tokens = 32;
    int32_t audio_ctx  = 0;

    float vad_thold    = 0.6f;
    float freq_thold   = 100.0f;

    bool speed_up      = false;
    bool translate     = false;
    bool no_fallback   = false;
    bool print_special = false;
    bool no_context    = true;
    bool no_timestamps = false;
    bool tinydiarize   = false;
    bool save_audio    = false; // save audio to wav file
    bool use_gpu       = true;

    std::string language  = "en";
    std::string model     = "models/ggml-base.en.bin";
//    std::string model     = "models/ggml-base.bin";
    std::string fname_out;
};

void whisper_print_usage(int argc, char ** argv, const whisper_params & params);

bool whisper_params_parse(int argc, char ** argv, whisper_params & params) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
        else if (arg == "-t"    || arg == "--threads")       { params.n_threads     = std::stoi(argv[++i]); }
        else if (                  arg == "--step")          { params.step_ms       = std::stoi(argv[++i]); }
        else if (                  arg == "--length")        { params.length_ms     = std::stoi(argv[++i]); }
        else if (                  arg == "--keep")          { params.keep_ms       = std::stoi(argv[++i]); }
        else if (arg == "-c"    || arg == "--capture")       { params.capture_id    = std::stoi(argv[++i]); }
        else if (arg == "-mt"   || arg == "--max-tokens")    { params.max_tokens    = std::stoi(argv[++i]); }
        else if (arg == "-ac"   || arg == "--audio-ctx")     { params.audio_ctx     = std::stoi(argv[++i]); }
        else if (arg == "-vth"  || arg == "--vad-thold")     { params.vad_thold     = std::stof(argv[++i]); }
        else if (arg == "-fth"  || arg == "--freq-thold")    { params.freq_thold    = std::stof(argv[++i]); }
        else if (arg == "-su"   || arg == "--speed-up")      { params.speed_up      = true; }
        else if (arg == "-tr"   || arg == "--translate")     { params.translate     = true; }
        else if (arg == "-nf"   || arg == "--no-fallback")   { params.no_fallback   = true; }
        else if (arg == "-ps"   || arg == "--print-special") { params.print_special = true; }
        else if (arg == "-kc"   || arg == "--keep-context")  { params.no_context    = false; }
        else if (arg == "-l"    || arg == "--language")      { params.language      = argv[++i]; }
        else if (arg == "-m"    || arg == "--model")         { params.model         = argv[++i]; }
        else if (arg == "-f"    || arg == "--file")          { params.fname_out     = argv[++i]; }
        else if (arg == "-tdrz" || arg == "--tinydiarize")   { params.tinydiarize   = true; }
        else if (arg == "-sa"   || arg == "--save-audio")    { params.save_audio    = true; }
        else if (arg == "-ng"   || arg == "--no-gpu")        { params.use_gpu       = false; }

        else {
            fprintf(stderr, "error: unknown argument: %s\n", arg.c_str());
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
    }

    return true;
}

void whisper_print_usage(int /*argc*/, char ** argv, const whisper_params & params) {
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: %s [options]\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -h,       --help          [default] show this help message and exit\n");
    fprintf(stderr, "  -t N,     --threads N     [%-7d] number of threads to use during computation\n",    params.n_threads);
    fprintf(stderr, "            --step N        [%-7d] audio step size in milliseconds\n",                params.step_ms);
    fprintf(stderr, "            --length N      [%-7d] audio length in milliseconds\n",                   params.length_ms);
    fprintf(stderr, "            --keep N        [%-7d] audio to keep from previous step in ms\n",         params.keep_ms);
    fprintf(stderr, "  -c ID,    --capture ID    [%-7d] capture device ID\n",                              params.capture_id);
    fprintf(stderr, "  -mt N,    --max-tokens N  [%-7d] maximum number of tokens per audio chunk\n",       params.max_tokens);
    fprintf(stderr, "  -ac N,    --audio-ctx N   [%-7d] audio context size (0 - all)\n",                   params.audio_ctx);
    fprintf(stderr, "  -vth N,   --vad-thold N   [%-7.2f] voice activity detection threshold\n",           params.vad_thold);
    fprintf(stderr, "  -fth N,   --freq-thold N  [%-7.2f] high-pass frequency cutoff\n",                   params.freq_thold);
    fprintf(stderr, "  -su,      --speed-up      [%-7s] speed up audio by x2 (reduced accuracy)\n",        params.speed_up ? "true" : "false");
    fprintf(stderr, "  -tr,      --translate     [%-7s] translate from source language to english\n",      params.translate ? "true" : "false");
    fprintf(stderr, "  -nf,      --no-fallback   [%-7s] do not use temperature fallback while decoding\n", params.no_fallback ? "true" : "false");
    fprintf(stderr, "  -ps,      --print-special [%-7s] print special tokens\n",                           params.print_special ? "true" : "false");
    fprintf(stderr, "  -kc,      --keep-context  [%-7s] keep context between audio chunks\n",              params.no_context ? "false" : "true");
    fprintf(stderr, "  -l LANG,  --language LANG [%-7s] spoken language\n",                                params.language.c_str());
    fprintf(stderr, "  -m FNAME, --model FNAME   [%-7s] model path\n",                                     params.model.c_str());
    fprintf(stderr, "  -f FNAME, --file FNAME    [%-7s] text output file name\n",                          params.fname_out.c_str());
    fprintf(stderr, "  -tdrz,    --tinydiarize   [%-7s] enable tinydiarize (requires a tdrz model)\n",     params.tinydiarize ? "true" : "false");
    fprintf(stderr, "  -sa,      --save-audio    [%-7s] save the recorded audio to a file\n",              params.save_audio ? "true" : "false");
    fprintf(stderr, "  -ng,      --no-gpu        [%-7s] disable GPU inference\n",                          params.use_gpu ? "false" : "true");
    fprintf(stderr, "\n");
}

void on_message(int argc, char ** argv,  server* s, websocketpp::connection_hdl hdl, server::message_ptr msg) {
    if (got_config) {
        std::vector<float> pcmf32 = read_float_vector(msg);
        //    auto non_null_bytes = std::count_if(pcmf32.begin(), pcmf32.end(), [](float x) { return x != 0; });
        audio_queue.push(pcmf32);
//        std::cerr << "Got a vector of " << pcmf32.size() << " floats." << std::endl;
        last_handle = hdl;
    } else {
        std::string conf(msg->get_payload().c_str());
        std::cerr << "Got config string " << conf << std::endl;
        got_config = true;
    }
}


std::vector<float> get_audio() {
    while (audio_queue.empty()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    auto vec = audio_queue.front();
    audio_queue.pop();
    return vec;
}


int audio_processing_function(int argc, char ** argv, server * serv) {
    whisper_params params;
    if (whisper_params_parse(argc, argv, params) == false) {
        return 1;
    }

    params.keep_ms = std::min(params.keep_ms, params.step_ms);
    params.length_ms = std::max(params.length_ms, params.step_ms);

    const int n_samples_step = (1e-3 * params.step_ms) * WHISPER_SAMPLE_RATE*2;
    const int n_samples_len = (1e-3 * params.length_ms) * WHISPER_SAMPLE_RATE;
    const int n_samples_keep = (1e-3 * params.keep_ms) * WHISPER_SAMPLE_RATE;
    const int n_samples_30s = (1e-3 * 30000.0) * WHISPER_SAMPLE_RATE;

    const bool use_vad = n_samples_step <= 0; // sliding window mode uses VAD

    const int n_new_line = !use_vad ? std::max(1, params.length_ms / params.step_ms - 1)
                                    : 1; // number of steps to print new line

    params.no_timestamps = !use_vad;
    params.no_context |= use_vad;
    params.max_tokens = 0;

    // whisper init
    if (params.language != "auto" && whisper_lang_id(params.language.c_str()) == -1) {
        fprintf(stderr, "error: unknown language '%s'\n", params.language.c_str());
        whisper_print_usage(argc, argv, params);
        exit(0);
    }

    struct whisper_context_params cparams;
    cparams.use_gpu = params.use_gpu;

    struct whisper_context *ctx = whisper_init_from_file_with_params(params.model.c_str(), cparams);

    std::vector<float> pcmf32(n_samples_30s, 0.0f);
    std::vector<float> pcmf32_old;
    std::vector<float> pcmf32_new(n_samples_30s, 0.0f);

    std::vector<whisper_token> prompt_tokens;
    int n_iter = 0;
    bool is_running = true;
    fflush(stdout);

    auto t_last = std::chrono::high_resolution_clock::now();
    const auto t_start = t_last;

    wav_writer wavWriter;
//    std::string filename = std::string("test_feb") + ".wav";
    std::string filename = "./examples/stream/" + std::string("test_feb") + ".wav"; // Сохраняет в указанный каталог
    wavWriter.open(filename, WHISPER_SAMPLE_RATE, 16, 1);
    // main audio loop
    while (is_running) {
        // Process new audio

        if (!use_vad) {
            std::cerr <<"    NOT USE VAD   HERE " << std::endl;
            while (true) {
                // собираю байты в буфер(вектор пока его размер меньше n_samples_state)
                vector<float> pcmf32_new;
                while (pcmf32_new.size() < n_samples_step) {
                    auto pcmf32_new_it = get_audio();
                    // Добавляем элементы в mainVector
                    pcmf32_new.insert(pcmf32_new.end(), pcmf32_new_it.begin(), pcmf32_new_it.end());
                    wavWriter.write(pcmf32_new_it.data(), pcmf32_new_it.size());
                }
                // Подсчет ненулевых элементов в векторе pcmf32
                size_t non_zero_count_f = std::count_if(pcmf32_new.begin(), pcmf32_new.end(), [](float val) { return val != 0.0f; });
                std::cerr << "HERE FIRST WRITE IN TEST FEB Non-zero float samples: " << non_zero_count_f << std::endl;



//                pcmf32_new = get_audio();
//                std::cerr << "get_audio() returned "<< pcmf32_new.size() << " values" << std::endl;
//
//// Создаем JSON объект
//                json j;
//                j["text"] = "распознанный текст";
//                j["result"] = json::array({ {{"conf", 0.987654321}, {"end", 1.23}, {"start", 0.56}, {"word", "распознанный"}},
//                                            {{"conf", 0.987654321}, {"end", 2.34}, {"start", 1.23}, {"word", "текст"}} });
//                j["text"] = "распознанный текст";
//
//// Конвертируем JSON объект в строку
//                std::string greet_text = j.dump();
//
//                std::cerr << "Отправляем клиенту greeting текст: " << std::endl;
//
//                send_text(serv, last_handle, greet_text);
                std::cerr <<"A " << pcmf32.size() << " " << count_if(pcmf32.begin(), pcmf32.end(), [](double x) { return x == 0; }) << std::endl;

                if ((int) pcmf32_new.size() > 2 * n_samples_step) {
                    fprintf(stderr, "\n\n%s: WARNING: cannot process audio fast enough, dropping audio ...\n\n",
                            __func__);
                    continue;
                }
                std::cerr <<"B " << use_vad << std::endl;

                if ((int) pcmf32_new.size() >= n_samples_step) {
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            const int n_samples_new = pcmf32_new.size();

            // take up to params.length_ms audio from previous iteration
            const int n_samples_take = std::min((int) pcmf32_old.size(),
                                                std::max(0, n_samples_keep + n_samples_len - n_samples_new));

            pcmf32.resize(n_samples_new + n_samples_take);

            for (int i = 0; i < n_samples_take; i++) {
                pcmf32[i] = pcmf32_old[pcmf32_old.size() - n_samples_take + i];
            }

            memcpy(pcmf32.data() + n_samples_take, pcmf32_new.data(), n_samples_new * sizeof(float));

            pcmf32_old = pcmf32;
        } else {
            std::cerr<< "  HERE USE VAD ! " << std::endl;
            const auto t_now = std::chrono::high_resolution_clock::now();
            const auto t_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_last).count();

            if (t_diff < 2000) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                continue;
            }

            pcmf32_new = get_audio();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (::vad_simple(pcmf32_new, WHISPER_SAMPLE_RATE, 1000, params.vad_thold, params.freq_thold, false)) {
                pcmf32 = get_audio();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                continue;
            }

            t_last = t_now;
        }

        // run the inference
        {
            std::cerr <<"   326 строка обработка текста  " << std::endl;
            whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

            wparams.print_progress = false;
            wparams.print_special = params.print_special;
            wparams.print_realtime = false;
            wparams.print_timestamps = !params.no_timestamps;
            wparams.translate = params.translate;
            wparams.single_segment = !use_vad;
            wparams.max_tokens = params.max_tokens;
            wparams.language = params.language.c_str();
            wparams.n_threads = params.n_threads;

            wparams.audio_ctx = params.audio_ctx;
            wparams.speed_up = params.speed_up;

            wparams.tdrz_enable = params.tinydiarize; // [TDRZ]

            wparams.temperature_inc = params.no_fallback ? 0.0f : wparams.temperature_inc;

            wparams.prompt_tokens = params.no_context ? nullptr : prompt_tokens.data();
            wparams.prompt_n_tokens = params.no_context ? 0 : prompt_tokens.size();
            {
                static int call_count = 1;

                std::string filename = "./examples/stream/chunk-"  + std::to_string(call_count) + std::string(".wav");
                {
                    // Проверка и логирование размера данных и состояния wavWriter перед записью
                    std::cerr << "Preparing to write audio chunk #" << call_count << std::endl;
                    std::cerr << "Filename: " << filename << std::endl;
                    std::cerr << "Data size: " << pcmf32.size() << " float samples" << std::endl;
                    std::cerr << "Sample rate: " << WHISPER_SAMPLE_RATE << ", Bits per sample: 16, Channels: 1" << std::endl;

                    // Подсчет ненулевых элементов в векторе pcmf32
                    size_t non_zero_count = std::count_if(pcmf32.begin(), pcmf32.end(), [](float val) { return val != 0.0f; });
                    std::cerr << "Non-zero float samples: " << non_zero_count << std::endl;
                    // Проверка наличия данных
                    if(pcmf32.empty()) {
                        std::cerr << "Warning: No data to write for chunk #" << call_count << std::endl;
                    }

                    // Проверка, что размер данных соответствует ожидаемому для записи
                    if(pcmf32.size() < n_samples_step) {
                        std::cerr << "Warning: Data size for chunk #" << call_count << " is less than expected n_samples_step: " << n_samples_step << std::endl;
                    }

                    // Дополнительные проверки можно добавить по необходимости
                }

                call_count++;
                wav_writer writer;
                writer.open(filename, WHISPER_SAMPLE_RATE, 16, 1);
                writer.write(pcmf32.data(), pcmf32.size());
                writer.close();
            }

            if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
                fprintf(stderr, "%s: failed to process audio\n", argv[0]);
                return 6;
            }

            // print result;
            {
                if (!use_vad) {
                    printf("\33[2K\r");

                    // print long empty line to clear the previous line
                    printf("%s", std::string(100, ' ').c_str());

                    printf("\33[2K\r");
                } else {
                    const int64_t t1 = (t_last - t_start).count() / 1000000;
                    const int64_t t0 = std::max(0.0, t1 - pcmf32.size() * 1000.0 / WHISPER_SAMPLE_RATE);

                    printf("\n");
                    printf("### Transcription %d START | t0 = %d ms | t1 = %d ms\n", n_iter, (int) t0, (int) t1);
                    printf("\n");
                }

                const int n_segments = whisper_full_n_segments(ctx);
                for (int i = 0; i < n_segments; ++i) {
                    const char *text = whisper_full_get_segment_text(ctx, i);

                    std::cerr << "Распознанный текст от клиента: " << text << std::endl;

                    json j;
                    j["text"] = text; // Присваиваем значение переменной ключу "text"
                    j["result"] = json::array({
                                                      {{"conf", 0.987654321}, {"text", "распознанный"}},
                                                      {{"conf", 0.987654321}, {"text", "текст"}}
                                              });

                    std::string greet_text = j.dump();

                    send_text(serv, last_handle, greet_text);
//                    send_text(serv, last_handle, std::string(text));
//                    std::this_thread::sleep_for(std::chrono::milliseconds(10));

                    if (params.no_timestamps) {
                        printf("%s", text);
                        fflush(stdout);

                    } else {
                        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
                        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);

                        std::string output = "[" + to_timestamp(t0) + " --> " + to_timestamp(t1) + "]  " + text;

                        if (whisper_full_get_segment_speaker_turn_next(ctx, i)) {
                            output += " [SPEAKER_TURN]";
                        }
                        output += "\n";
                        printf("%s", output.c_str());
                        fflush(stdout);
                    }
                }

                if (use_vad) {
                    printf("\n");
                    printf("### Transcription %d END\n", n_iter);
                }
            }

            ++n_iter;

            if (!use_vad && (n_iter % n_new_line) == 0) {
                printf("\n");

                // keep part of the audio for next iteration to try to mitigate word boundary issues
                pcmf32_old = std::vector<float>(pcmf32.end() - n_samples_keep, pcmf32.end());

                // Add tokens of the last full length segment as the prompt
                if (!params.no_context) {
                    prompt_tokens.clear();

                    const int n_segments = whisper_full_n_segments(ctx);
                    for (int i = 0; i < n_segments; ++i) {
                        const int token_count = whisper_full_n_tokens(ctx, i);
                        for (int j = 0; j < token_count; ++j) {
                            prompt_tokens.push_back(whisper_full_get_token_id(ctx, i, j));
                        }
                    }
                }
            }
            fflush(stdout);
        }
    }


    whisper_print_timings(ctx);
    whisper_free(ctx);
    return 0;
}



int main(int argc, char ** argv) {
    server echo_server;
    std::cerr << "START SERV " << std::endl;

    // Установка обработчика сообщений
    echo_server.set_message_handler(bind(on_message, argc,  argv, &echo_server, placeholders::_1, placeholders::_2));

    // Запуск сервера в отдельном потоке
    thread server_thread([&]() {
        // Запуск сервера
        echo_server.init_asio();
        echo_server.listen(9002);
        echo_server.start_accept();
        std::cerr << "RUN SERV " << std::endl;
        echo_server.run();
    });

    audio_processing_function(argc, argv, &echo_server);

    // Ожидание завершения потока сервера
    server_thread.join();

    return 0;
}
