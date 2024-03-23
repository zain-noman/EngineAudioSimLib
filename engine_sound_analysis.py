import numpy as np
import scipy
import matplotlib.pyplot as plt

rate, data = scipy.io.wavfile.read("car-starting-and-accelerating-clipped.wav")
_, data2 = scipy.io.wavfile.read("car-starting-and-accelerating-clipped_my_try.wav")
data2 = data2 + np.random.normal(size=len(data2))*1

print(rate)
print(data.shape)

# autocorrelation = np.correlate(data, data, mode='full')  # Use 'full' mode for autocorrelation
# print(autocorrelation.shape)
freq = np.fft.fftshift(np.fft.fft(data))
freq2 = np.fft.fftshift(np.fft.fft(data2))
print(freq.shape)

# ax.plot(autocorrelation)
plt.figure()

plt.subplot(211)
plt.plot(np.linspace(-12000,12000,len(freq)),np.abs(freq))
plt.xlim(-100,100)
plt.ylim(0,0.3e13)

plt.subplot(212)
plt.plot(np.linspace(-12000,12000,len(freq2)),np.abs(freq2))
plt.xlim(-100,100)
plt.ylim(0,0.3e13)

plt.show()
