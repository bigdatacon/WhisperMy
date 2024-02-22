FROM python:3.11-slim as builder

WORKDIR /vosk

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    gcc \
    unzip

ADD https://alphacephei.com/vosk/models/vosk-model-small-ru-0.22.zip \
    /vosk/vosk-model-small-ru-0.22.zip
RUN unzip -o /vosk/vosk-model-small-ru-0.22.zip -d /vosk/models && \
    rm -f /vosk/vosk-model-small-ru-0.22.zip

COPY requirements.txt /vosk
RUN pip install --upgrade pip && \
    pip install --no-cache-dir --root-user-action=ignore -r requirements.txt

COPY . /vosk

EXPOSE 2700

ENTRYPOINT ["python", "vosk_server.py", \
            "--model=models/vosk-model-small-ru-0.22", \
            "--host=0.0.0.0", "--port=2700", \
            "--log_file=logs/vosk.log"]

# Старт без docker-compose:
# docker run -p 2700:2700 \
#     -v "./logs:/vosk/logs" \ 
#     vosk_conatiner_name_or_id