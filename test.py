import numpy as np
import matplotlib.pyplot as plt

X = np.linspace(-10, 10, 1000)
y = np.sin(X)
plt.plot(X,y)
plt.savefig('test.jpg')
plt.show()