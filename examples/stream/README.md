# stream_server and stream_client  
# Внимание, звук пишется только с микрофона, если отключить микрофон и пользоваться встроенным динамиком - записи не будет  
# Запуск клиента python(vosk - asr_ws_client.py) и сервера whisper 
## в докере:  
### в корневом каталоге проекта (порт 9002 должен совпадать с портом, указанным в клиенте питон - если не совпадает то привести в соответствие ) :   
#docker build . -t whisper  
#docker run --name whisper -p 9002:9002 whisper    

### в папке examples/stream :   
python asr_ws_client.py --host ws://localhost:9002 --show_devices true

## без исопльзования докера:  
### в корневом каталоге проекта :   
make clean  
make stream_client_docker
./stream_server_docker > -vth > /dev/null 
### в папке examples/stream :     
python asr_ws_client.py --host ws://localhost:9002 --show_devices true

# Запуск клиента whisper и сервера whisper без докера  
## в корневом каталоге проекта :  
make clean  
make stream_client_fin  
make stream_server_fin  

## запуск сервера  
./stream_server_fin > -vth > /dev/null     
--или так - разницы нет:    
./stream_server_fin > /dev/null    


## запуск клиента   
#### Без use vad  
./stream_client_fin 127.0.0.1 > /dev/null   
--вместо 127.0.0.1 можно указать любой адрес сервера, в ридми указывается адрес текущего компьютера)  
#### С USE VAD:  
./stream_client_fin 127.0.0.1 --step -3000 -vth 0.5 > /dev/null
  






# stream

This is a naive example of performing real-time inference on audio from your microphone.
The `stream` tool samples the audio every half a second and runs the transcription continously.
More info is available in [issue #10](https://github.com/ggerganov/whisper.cpp/issues/10).

```java
./stream -m ./models/ggml-base.en.bin -t 8 --step 500 --length 5000
```

https://user-images.githubusercontent.com/1991296/194935793-76afede7-cfa8-48d8-a80f-28ba83be7d09.mp4

## Sliding window mode with VAD

Setting the `--step` argument to `0` enables the sliding window mode:

```java
 ./stream -m ./models/ggml-small.en.bin -t 6 --step 0 --length 30000 -vth 0.6
```

In this mode, the tool will transcribe only after some speech activity is detected. A very
basic VAD detector is used, but in theory a more sophisticated approach can be added. The
`-vth` argument determines the VAD threshold - higher values will make it detect silence more often.
It's best to tune it to the specific use case, but a value around `0.6` should be OK in general.
When silence is detected, it will transcribe the last `--length` milliseconds of audio and output
a transcription block that is suitable for parsing.

## Building

The `stream` tool depends on SDL2 library to capture audio from the microphone. You can build it like this:

```bash
# Install SDL2 on Linux
sudo apt-get install libsdl2-dev

# Install SDL2 on Mac OS
brew install sdl2

make stream
```

Ensure you are at the root of the repo when running `make stream`.  Not within the `examples/stream` dir
as the libraries needed like `common-sdl.h` are located within `examples`.  Attempting to compile within
`examples/steam` means your compiler cannot find them and it gives an error it cannot find the file.

```bash
whisper.cpp/examples/stream$ make stream
g++     stream.cpp   -o stream
stream.cpp:6:10: fatal error: common/sdl.h: No such file or directory
    6 | #include "common/sdl.h"
      |          ^~~~~~~~~~~~~~
compilation terminated.
make: *** [<builtin>: stream] Error 1
```

## Web version

This tool can also run in the browser: [examples/stream.wasm](/examples/stream.wasm)
