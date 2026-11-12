# Lab 7 Edge Machine Learning

Train a keyword model and a hand-gesture model, then run each on the XIAO ESP32S3 Sense. Complete the TODOs in the collection, training, and inference files.

Use the course handout for the full instructions and checkpoint requirements. Each task also has a README with its commands.

## Set up

Use esp32 core 3.3.12, board XIAO_ESP32S3, USB CDC On Boot enabled, and OPI PSRAM. Task 1 needs arduinoFFT 2.0.4. Both inference sketches need Chirale_TensorFLowLite 2.0.0. Install the libraries in Arduino Library Manager.

Activate your Python 3.11 or 3.12 environment and install the packages from this folder:

```text
pip install -r requirements.txt
```

The task READMEs use `python` or `python3`. Use the command for that environment on your computer. Close Arduino Serial Monitor and Serial Plotter while collecting through Python.

## Task folders

- `task1/keyword_collect`: record two seconds and compute audio features.
- `task1/tools`: save examples, inspect them, and train the keyword CNN.
- `task1/keyword_inference`: recognize new recordings on the board.
- `task2/gesture_collect`: capture and preprocess camera images.
- `task2/tools`: save examples, inspect them, and train the gesture CNN.
- `task2/gesture_inference`: recognize gestures on the board.

Run each task's Python commands from its `task1` or `task2` folder. List ports first, then replace `YOUR_PORT` with the XIAO's port.

## Files created by training

`config.h` holds the hand-written board settings and is included with each sketch. `model_data.h` and `model_config.h` are generated together when you train. They are not included in this package. Copy both into the corresponding inference folder before compiling it.

For Task 2, also copy your completed `image_transform.h` from `gesture_collect` to `gesture_inference`. This keeps collection and recognition on the same image transformation.
