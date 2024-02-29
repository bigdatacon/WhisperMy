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

# Копирование всего проекта в контейнер, предполагается, что Dockerfile находится в examples/stream
WORKDIR /app
COPY ../../ .

# Сборка сервера
RUN make clean && make stream_server_fin

# Копирование скрипта для одновременного запуска и перемещение в папку examples/stream
COPY run_both.sh examples/stream/
RUN chmod +x examples/stream/run_both.sh

# Задаем рабочую директорию для клиента
WORKDIR /app/examples/stream

# Запускаем сервер и клиента одновременно
CMD ["./run_both.sh"]
