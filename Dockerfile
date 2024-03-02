# Используем базовый образ с поддержкой C++
FROM debian:12.5

# Установка необходимых пакетов
# Добавляем SDL2 и nlohmann json
RUN apt-get update && apt-get install -y \
    gcc \
    g++ \
    make \
    libwebsocketpp-dev \
    libboost-all-dev \
    libsdl2-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Копирование всего проекта в контейнер
WORKDIR /app
COPY . .

# Сборка сервера
RUN make clean && make stream_server_fin

# Устанавливаем права на выполнение для исполняемого файла
# Исправлен путь к исполняемому файлу, теперь он находится в корне /app
RUN chmod +x ./stream_server_fin

# Запуск сервера
# Исправлен путь запуска сервера, используя обновленное расположение исполняемого файла
CMD ["sh", "-c", "./stream_server_fin > /dev/null"]


#docker build . -t whisper
#docker run --name whisper -p 9002:9002 whisper
