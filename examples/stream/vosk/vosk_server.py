import json
import os
import asyncio
import websockets
import concurrent.futures
import logging
import argparse
from vosk import Model, SpkModel, KaldiRecognizer

def get_logger(**kwargs):
    # Настройка форматирования для сообщений
    formatter = logging.Formatter(kwargs['log_format'])

    # Создание логгера
    logger = logging.getLogger('tlogger')
    logger.setLevel(logging.INFO)

    # Создание обработчика для вывода на экран
    console_handler = logging.StreamHandler()
    # console_handler.setFormatter(formatter)

    # Создание обработчика для записи в файл
    file_handler = logging.FileHandler(kwargs['log_file'])
    file_handler.setFormatter(formatter)

    # Добавление обработчиков к логгеру
    logger.addHandler(console_handler)
    logger.addHandler(file_handler)

    return logger


def process_chunk(rec, message):
    if message == '{"eof" : 1}':
        return rec.FinalResult(), False
    elif rec.AcceptWaveform(message):
        return rec.Result(), False
    else:
        return rec.PartialResult(), False

async def recognize(websocket, path):
    async def ws_close(ws):
        gkwargs['logger'].info('Connection closed from %s', websocket.remote_address)
        await ws.close()

    global model
    global spk_model
    global args
    global pool
    global show_words
    global first_start_flg

    stop = False

    loop = asyncio.get_running_loop()
    rec = None
    phrase_list = None
    sample_rate = args.samplerate
    max_alternatives = args.max_alternatives

    gkwargs['logger'].info('Connection open from %s', websocket.remote_address);

    while True:
        
        try:
            message = await websocket.recv()
        except websockets.ConnectionClosed:
            await ws_close(websocket)
            break

        if True and message == 'ping':
            try:
                await websocket.send('pong')
            except websockets.ConnectionClosed:
                await ws_close(websocket)
                break
            continue
             
        # Load configuration if provided
        if isinstance(message, str) and 'config' in message:
            jobj = json.loads(message)['config']
            gkwargs['logger'].info("Config %s", jobj)
            if 'phrase_list' in jobj:
                phrase_list = jobj['phrase_list']
            if 'sample_rate' in jobj:
                sample_rate = float(jobj['sample_rate'])
            if 'model' in jobj:
                model = Model(jobj['model'])
                model_changed = True
            if 'words' in jobj:
                show_words = bool(jobj['words'])
            if 'max_alternatives' in jobj:
                max_alternatives = int(jobj['max_alternatives'])
            continue

        # Create the recognizer, word list is temporary disabled since not every model supports it
        if not rec or model_changed:
            model_changed = False
            if phrase_list:
                rec = KaldiRecognizer(model, sample_rate, json.dumps(phrase_list, ensure_ascii=False))
            else:
                rec = KaldiRecognizer(model, sample_rate)
            rec.SetWords(show_words)
            rec.SetMaxAlternatives(max_alternatives)
            if spk_model:
                rec.SetSpkModel(spk_model)

        # if isinstance(message, (bytes, bytearray)):
        response, stop = await loop.run_in_executor(pool, process_chunk, rec, message)
        try:
            await websocket.send(response)
        except websockets.ConnectionClosed:
            await ws_close(websocket)
            break 
        if stop: break

async def start():

    global model
    global spk_model
    global args
    global pool
    global show_words
    global gkwargs

    parser = argparse.ArgumentParser(description='VOSK Service')
    parser.add_argument('--host', type=str, default='0.0.0.0',
                        help='VOSK server host')
    parser.add_argument('--port', type=int, default=2700,
                        help='VOSK server port')
    parser.add_argument('--model', type=str, default='models/vosk-model-small-ru-0.22',
                        help='path to VOSK model')
    parser.add_argument('--samplerate', type=int, default=16000,
                        help='VOSK sample rate')
    parser.add_argument('--max_alternatives', type=int, default=0,
                        help='VOSK alternatives')
    parser.add_argument('--show_words', type=int, default=1,
                        help='VOSK show words')
    parser.add_argument('--log_file', type=str, default='logs/vosk.log',
                        help='Path to log file')
    parser.add_argument('--log_format', type=str, default='%(asctime)s;%(message)s',
                        help='Log format (like "%(asctime)s;%(message)s")')
    args = parser.parse_args()

    if args.show_words < 1:
        show_words = False
    else:
        show_words = True

    gkwargs = vars(args)
    
    gkwargs['logger'] = get_logger(**gkwargs)

    spk_model_path = os.environ.get('VOSK_SPK_MODEL_PATH')

    # Gpu part, uncomment if vosk-api has gpu support
    #
    # from vosk import GpuInit, GpuInstantiate
    # GpuInit()
    # def thread_init():
    #     GpuInstantiate()
    # pool = concurrent.futures.ThreadPoolExecutor(initializer=thread_init)

    model = Model(args.model)
    spk_model = SpkModel(args.spk_model_path) if spk_model_path else None

    pool = concurrent.futures.ThreadPoolExecutor((os.cpu_count() or 1))

    async with websockets.serve(recognize, args.host, args.port,
                                logger=gkwargs['logger']):
        await asyncio.Future()

if __name__ == '__main__':
    # first_start_flg = True
    asyncio.run(start())