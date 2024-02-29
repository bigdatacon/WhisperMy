# Используем базовый образ с поддержкой C++ и Python
FROM ubuntu:20.04

# Установка необходимых пакетов
RUN apt-get update && apt-get install -y \
    gcc \
    g++ \
    make \
    python3 \
    python3-pip \
    libwebsocketpp-dev \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Установка зависимостей Python
RUN pip3 install websockets sounddevice numpy

# Копирование всего проекта в контейнер
WORKDIR /app
COPY . .

# Сборка сервера
RUN make clean && make stream_server_fin

# Перемещаемся в директорию, где находится скрипт для запуска
WORKDIR /app/examples/stream

# Копирование и предоставление прав выполнения скрипту запуска
COPY run_both.sh /app/examples/stream/
RUN chmod +x /app/examples/stream/run_both.sh

# Запускаем сервер и клиента одновременно
CMD ["/app/examples/stream/run_both.sh"]
