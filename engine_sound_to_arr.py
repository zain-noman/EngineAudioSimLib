import numpy as np
import scipy

rate, data = scipy.io.wavfile.read("car-starting-and-accelerating-clipped_smol.wav")

print(rate)
print(data.shape)

f = open('engineSound.h',mode="w")

f.write("int16_t engineSound[] = {")
for i in range(len(data)):
    # Convert to int16 and handle negative values
    int16_value = np.int16(data[i])
    
    # Convert to hexadecimal representation
    hex_value = hex(int16_value & 0xFFFF).upper()  # Use bitwise AND to get the two's complement
    
    f.write("0x%s," % hex_value[2:].zfill(4))  # Remove the '0x' prefix and zero-fill to 4 digits
f.write("};")