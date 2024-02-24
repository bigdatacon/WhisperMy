import json
import websockets
import asyncio
from queue import Queue
import argparse
import sounddevice as sd
import numpy as np
import struct


buffer_queue = Queue()

def is_bool_str(str):
    return str.strip().lower() == 'true' or str.strip().lower() == '1'

def get_args(**kwargs):
    parser = argparse.ArgumentParser(description='Test WS client')
    parser.add_argument('--host', type=str, default=kwargs['host'],
                        help='WebSocket server host')
    parser.add_argument('--device_in', type=int, default=kwargs['device_in'], 
                        help='Microphone device ID')
    parser.add_argument('--device_out', type=int, default=kwargs['device_out'],
                        help='Speaker device ID')
    parser.add_argument('--channels_in', type=int, default=kwargs['channels_in'],
                        help='Number of channels for input stream')
    parser.add_argument('--samplerate_in', type=int, default=kwargs['samplerate_in'],
                        help='Sample rate for input stream')
    parser.add_argument('--chunksize_in', type=int, default=kwargs['chunksize_in'], 
                        help='Size of chunk for input stream')
    parser.add_argument('--show_devices', type=is_bool_str, default=str(kwargs['show_devices']),
                        help='Set to True or 1 to show and select available devices')

    return {**kwargs, **vars(parser.parse_args())}

def show_devices(**kwargs):
    if kwargs['show_devices']:   
        print(sd.query_devices())                               
        device_in_ = input('Выберите входное устройство (микрофон): ')
        device_out_ = input('Выберите воспроизводящее устройство (наушники): ')
        if device_in_ != '':
            kwargs['device_in'] = int(device_in_)
        if device_out_ != '':
            kwargs['device_out'] = int(device_out_) 
    return kwargs['device_in'], kwargs['device_out']

async def inputstream_generator(**kwargs):
    queue_to_ws = asyncio.Queue()
    loop = asyncio.get_event_loop()

    def callback_in_raw(indata, frame_count, time_info, status):
        loop.call_soon_threadsafe(queue_to_ws.put_nowait, (indata, status))
    
    rec_stream = sd.RawInputStream(device=kwargs['device_in'], 
                                   channels=kwargs['channels_in'], 
                                   samplerate=kwargs['samplerate_in'], 
                                   dtype='int16',
                                   # dtype='float32',
                                   blocksize=kwargs['chunksize_in'],
                                   callback=callback_in_raw,
                                   )
    
    chunk = {'asr_model': 60,
             'reset_flg': False,
             'bytes': b''}

    with rec_stream:
        while True:
            indata, status = await queue_to_ws.get()
            # Преобразование indata в массив NumPy
            indata_np = np.frombuffer(indata, dtype=np.int16)
            # Преобразование данных в float32 и нормализация
            indata_float = indata_np.astype(np.float32) / np.iinfo(np.int16).max
            # Получаем байты из indata_float
            bytes_data = indata_float.tobytes()
            # chunk['bytes'] = bytes(indata)
            chunk['bytes'] = bytes_data

            yield chunk

def make_vosk_config_str(framerate=16000, phrase_list=None):
    if phrase_list is None \
       or type(phrase_list) != list \
       or (type(phrase_list) == list and len(phrase_list) == 0):
        message = "{ 'config' : { 'sample_rate' : %d } }"%(framerate)
    else:
        message = "{ 'config' : { 'sample_rate' : %d, 'phrase_list' : %s } }"%(
            framerate, phrase_list #+ ['[unk]']
            )
    return message.replace("'", '"')

async def process_asr(m_name, desc, **kwargs):
    async for ws in websockets.connect(kwargs['host'], 
                                       ping_interval=60):
        try:
            log_message = f"Соединение с ws сервером ASR {kwargs['host']} установлено."
            print(log_message)
            await ws.send(make_vosk_config_str(kwargs["samplerate_in"]))
            async for chunk in inputstream_generator(**kwargs):
                if chunk['asr_model'] == 60:
                    if chunk['reset_flg']:
                        await ws.send(kwargs['vosk_reset_str'])
                    else:
                        await ws.send(chunk['bytes'])
                        print(f"байты от клиента на сервер отправлены")
                    resp = await ws.recv()
                    resp = json.loads(resp)
                    text = resp.get('text', resp.get('partial', ''))
                    if text != '':
                        print(text)
                # 50 - YANDEX
                elif chunk['asr_model'] == 50:
                    continue
                else:
                    continue
        except websockets.ConnectionClosed:
            log_message = f"Соединение с ws сервером ASR {kwargs['host']} закрыто."
            print(log_message)
            continue

def worker_asr(m_name, desc, **kwargs):
    log_message = f'Воркер {m_name} ({desc}) запущен.'
    print(log_message)
    asyncio.run(process_asr(m_name, desc,  **kwargs))


kwargs = {
    # 'host': 'ws://localhost:2700',
    'host': 'ws://localhost:9002',
    'device_in': 0,
    'device_out': 1,
    'channels_in': 1,
    'samplerate_in': 16000,
    'chunksize_in': 1000,
    'show_devices': True
    }

if __name__ == '__main__':
    m_name = 'ASR'
    desc = 'ASR WS Client'
    kwargs = get_args(**kwargs)
    kwargs['device_in'], kwargs['device_out'] = show_devices(**kwargs)
    worker_asr(m_name, desc, **kwargs)
