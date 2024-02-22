#!/bin/bash
wget -q --show-progress "https://alphacephei.com/vosk/models/vosk-model-small-ru-0.22.zip" &&
unzip -o vosk-model-small-ru-0.22.zip &&
rm -f vosk-model-small-ru-0.22.zip