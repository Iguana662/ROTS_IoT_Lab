import tensorflow as tf
import os

# Settings
INPUT_MODEL = 'model.h5'
PREFIX = "TinyML"   # The variable name in Arduino (TinyML_model, TinyML_len)

# 1. Check if file exists
if not os.path.exists(INPUT_MODEL):
    print(f"Error: '{INPUT_MODEL}' not found in this folder.")
    exit()

print("Loading Keras model...")
try:
    # compile=False skips loading the optimizer (Adam/SGD), avoiding many version errors
    model = tf.keras.models.load_model(INPUT_MODEL, compile=False)
except Exception as e:
    print(f"Standard load failed: {e}")
    print("⚠️ Attempting legacy loading via tf_keras...")
    
    # See Fix 2 below if this block is triggered
    import tf_keras
    model = tf_keras.models.load_model(INPUT_MODEL, compile=False)

# 2. Convert to TFLite (Quantized)
print("Converting to TFLite...")
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT] # Critical for reducing size
tflite_model = converter.convert()

# Save the .tflite file
tflite_path = PREFIX + ".tflite"
with open(tflite_path, "wb") as f:
    f.write(tflite_model)

print(f"Generated {tflite_path} ({len(tflite_model)} bytes)")

# 3. Convert to C Header (Hex)
output_header_path = "model_data.h" # Standard name for Arduino

print("Generating Header file...")
hex_lines = [', '.join([f'0x{byte:02x}' for byte in tflite_model[i:i + 12]]) 
             for i in range(0, len(tflite_model), 12)]

hex_array = ',\n  '.join(hex_lines)

with open(output_header_path, 'w') as header_file:
    # Essential Includes
    header_file.write('#include <pgmspace.h>\n\n')
    
    # 1. Length Variable (Needed by Interpreter)
    header_file.write(f'const int {PREFIX}_len = {len(tflite_model)};\n')
    
    # 2. The Array (MUST BE ALIGNED to 16 bytes)
    # "alignas(16)" prevents ESP32 crashes
    # "PROGMEM" keeps it in Flash, saving RAM
    header_file.write(f'alignas(16) const unsigned char {PREFIX}_model[] PROGMEM = {{\n  ')
    header_file.write(f'{hex_array}\n')
    header_file.write('};\n\n')

print(f"✅ Success! Created '{output_header_path}'")
print(f"   Use variables: {PREFIX}_model and {PREFIX}_len")