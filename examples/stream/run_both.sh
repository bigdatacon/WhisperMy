#!/bin/bash

# Запускаем сервер
../stream_server_fin -vth > /dev/null 2>&1 &

# Запускаем клиента
python3 asr_ws_client.py --host ws://localhost:9002 --show_devices true

# Ждем завершения всех фоновых процессов
wait
