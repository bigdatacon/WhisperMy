# Используем базовый образ с поддержкой C++
FROM ubuntu:20.04

# Установка необходимых пакетов
RUN apt-get update && apt-get install -y \
    gcc \
    g++ \
    make \
    libwebsocketpp-dev \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Копирование всего проекта в контейнер
WORKDIR /app
COPY . .

# Сборка сервера
RUN make clean && make stream_server_fin

# Запуск сервера
CMD ["sh", "-c", "./examples/stream/stream_server_fin -vth > /dev/null"]
