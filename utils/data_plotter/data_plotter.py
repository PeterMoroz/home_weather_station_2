import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates


df = pd.read_csv('ccs811.csv', sep=';', index_col='timestamp', parse_dates=['timestamp'])

ax = df.plot(figsize=(15, 4))
locator = mdates.MinuteLocator(interval=30)
ax.xaxis.set_major_locator(locator)
ax.xaxis.set_major_formatter(mdates.ConciseDateFormatter(locator))
df.plot(subplots=True, figsize=(15, 6))
plt.show()

