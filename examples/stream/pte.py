# Этот скрипт просто выводит "Hello, World!" на экран
print("Hello, World!")

import sounddevice as sd
print("PRINT DEVICES!")
try:
    print(sd.query_devices())
except Exception as e:
    print(e)


duration = 5  # запись в течение 5 секунд
fs = 44100  # частота дискретизации
print("Start Recording!")

try:
    myrecording = sd.rec(int(duration * fs), samplerate=fs, channels=2, device=0)
    sd.wait()  # ждём окончания записи
    sd.play(myrecording, fs)  # воспроизведение записанного
    sd.wait()
except Exception as e:
    print(e)

