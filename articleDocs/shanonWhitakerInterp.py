from math import sin,pi
import numpy as np
import matplotlib.pyplot as plt

def ShanonWhitakerFilt(offset:float, rate:float, len: int):
    retval = []
    cutOff = 1/rate if rate>=1 else 1
    for i in range(-len,len):
        if (abs(float(i)-offset) > 0.01):
            pi_x  = 3.1415 * (float(i)-offset);
            retval.append(sin(pi_x*cutOff) / (pi_x))
        else:
            retval.append(1)
    return retval


rates = [0.5, 1.2, 2]
length = 50

fig, axes = plt.subplots(3, 2, figsize=(12, 18))

for i, rate in enumerate(rates):
    filter_vals = ShanonWhitakerFilt(0, rate, length)
    fft_vals = np.fft.fftshift(np.abs(np.fft.fft(filter_vals)))
    
    # Time domain plot
    axes[i, 0].plot(range(-length, length), filter_vals)
    axes[i, 0].set_xlim(-10, 10)
    # axes[i, 0].set_title(f'Time Domain Filter (Rate = {rate})')
    axes[i, 0].set_xlabel('Sample')
    axes[i, 0].set_ylabel('Amplitude')
    
    # Frequency domain plot
    axes[i, 1].plot(np.linspace(-pi, pi, len(fft_vals)), fft_vals)
    # axes[i, 1].set_title(f'FFT of Filter (Rate = {rate})')
    axes[i, 1].set_xlabel('Frequency')
    axes[i, 1].set_ylabel('Magnitude')

plt.tight_layout()
plt.show()