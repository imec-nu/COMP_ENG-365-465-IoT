# Task 2 Hand gesture recognition

Work from this `task2` folder when running the Python commands.

1. Complete collection TODOs 2.1 and 2.2 in `gesture_collect/image_transform.h`
   and `gesture_collect/gesture_collect.ino`. Upload `gesture_collect.ino`.
2. Install `requirements_collect.txt` in your Python environment. Close Arduino
   Serial Monitor, then run `python tools/collect_gestures.py --list-ports`.
3. Replace `YOUR_PORT` with the port printed for the XIAO. For example:

   `python tools/collect_gestures.py --port YOUR_PORT --label fist --session train_01 --split train --count 60`

   Press Enter for each capture, then use the preview: `s` saves, `n` discards,
   and `q` quits. Collect all four classes in separate train, val, and test
   sessions. Use the label `background` for images without a hand.
4. Check the saved files with `python tools/inspect_dataset.py`.
   Add `--sample SAMPLE_ID` to select a row from `gesture_dataset/labels.csv`.
5. Complete TODOs 2.3 and 2.4 in `tools/train_gestures.py`. Install
   `requirements_train.txt`, then run:

   `python tools/train_gestures.py --epochs 50 --batch-size 8`

6. Copy `model_data.h` and `model_config.h` from `trained_gesture_model` into
   `gesture_inference`. Copy your completed `image_transform.h` from
   `gesture_collect` into `gesture_inference` too. The same crop and grayscale
   conversion must be used for training examples and new images.
7. Complete TODOs 2.5 and 2.6 in `gesture_inference/gesture_inference.ino`.
   Install Chirale_TensorFLowLite 2.0.0 in Arduino Library Manager and upload.
8. Open Serial Monitor at 115200 baud. Send `r` for one image, `a` for repeated
   captures, or `s` to stop. Keep the hand in the center square used in collection.

The handout gives the data collection plan, model layers, and checkpoint tests.
On macOS use `python3` if that is the command for your chosen environment.
On Windows use `python` in that environment, or `py` if using Python Launcher.

`config.h` contains camera and timing settings. `model_config.h` contains labels,
quantization scales, and the score threshold exported with your trained model.
The starter does not include generated model headers. Train the model before
compiling the inference sketch.
