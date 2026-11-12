"""Build and train the gesture CNN. Dataset loading and export are supplied."""
from training_support import run_training


def build_model(tf):
    # TODO 2.3: YOUR CODE HERE
    # Return a tf.keras.Sequential model for input (64,64,1):
    # Conv2D(8), MaxPooling2D(2), Conv2D(16), MaxPooling2D(2),
    # Conv2D(24), MaxPooling2D(2), Flatten(), Dense(32,relu),
    # Dropout(0.2), Dense(4,softmax).
    # Each convolution uses a 3x3 kernel, padding='same', activation='relu'.
    # Use tf.keras.layers.Input(shape=(64,64,1)) as the first layer.
    raise NotImplementedError("Complete TODO 2.3 before training.")


def train_model(tf, model, x_train, y_train, x_val, y_val, epochs, batch_size):
    # TODO 2.4: YOUR CODE HERE
    # Labels are integer class indices. Compile with Adam,
    # sparse_categorical_crossentropy, and accuracy.
    # Fit x_train/y_train; pass (x_val,y_val) as validation_data.
    # Use the supplied epochs/batch_size and an EarlyStopping callback with
    # monitor='val_loss', patience=8, restore_best_weights=True.
    # Return the History object from model.fit().
    raise NotImplementedError("Complete TODO 2.4 before training.")


if __name__ == '__main__':
    run_training(build_model, train_model)
