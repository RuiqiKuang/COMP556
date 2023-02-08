import matplotlib.pyplot as plt
size = [i for i in range(100, 65501, 100)]
tmp = open('out.txt', 'r').readlines()
latency = []
for x in tmp:
    x = x.replace('\n', '')
    latency.append(float(x))

plt.plot(size, latency)
plt.show()