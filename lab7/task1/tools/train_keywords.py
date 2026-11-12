"""Student work for normalization, the CNN, and fitting the model."""
import numpy as np
from training_support import run_training


def normalize_inputs(data):
    # TODO 1.3: data maps train/val/test to (inputs, integer_labels).
    # Compute one mean and standard deviation from train inputs only.
    # Use max(std, 1e-6), then apply (x - mean) / std to ALL three splits.
    # Keep their labels unchanged. Return (normalized_data, mean, std).
    raise NotImplementedError('TODO 1.3: normalize using training data')


def build_model(tf):
    # TODO 1.4: Return a tf.keras.Sequential CNN for inputs (124, 32, 1).
    # Use 3x3 Conv2D layers with 8, 16, then 24 filters, padding='same',
    # activation='relu'. Put MaxPooling2D(2) after the first two convolutions.
    # Finish with GlobalAveragePooling2D and Dense(6, activation='softmax').
    # The six outputs correspond to the label order saved in labels.json.
    raise NotImplementedError('TODO 1.4: build the CNN')


def fit_model(model, x_train, y_train, x_val, y_val, epochs, batch_size, callbacks):
    # TODO 1.5: Compile with Adam, sparse_categorical_crossentropy, and accuracy.
    # Labels are integer class indices, so use the sparse version of the loss.
    # Fit using train inputs/labels and validation_data=(x_val, y_val).
    # Pass epochs, batch_size, and the supplied callbacks; return the History.
    raise NotImplementedError('TODO 1.5: compile and fit the model')


if __name__ == '__main__':
    run_training(normalize_inputs, build_model, fit_model)
