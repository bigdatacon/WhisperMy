#!/bin/bash

# Запуск сервера
cd /app && ./examples/stream/stream_server_fin > -vth > /dev/null 2>&1 &

# Запуск клиента
cd /app/examples/stream && python3 asr_ws_client.py --host ws://localhost:9002 --show_devices true


# Ждем завершения всех фоновых процессов
wait
