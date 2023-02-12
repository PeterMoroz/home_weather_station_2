import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

import os.path


def draw_ccs811():
    if not os.path.exists('ccs811.csv'):
        return

    df = pd.read_csv('ccs811.csv', sep=';', index_col='timestamp', parse_dates=['timestamp'])

    ax = df.plot(figsize=(15, 4))
    locator = mdates.MinuteLocator(interval=30)
    ax.xaxis.set_major_locator(locator)
    ax.xaxis.set_major_formatter(mdates.ConciseDateFormatter(locator))
    df.plot(subplots=True, figsize=(15, 6))
    plt.show()

def draw_dht22():
    if not os.path.exists('dht22.csv'):
        return
        
    df = pd.read_csv('dht22.csv', sep=';', index_col='timestamp', parse_dates=['timestamp'])

    ax = df.plot(figsize=(15, 4))
    locator = mdates.MinuteLocator(interval=30)
    ax.xaxis.set_major_locator(locator)
    ax.xaxis.set_major_formatter(mdates.ConciseDateFormatter(locator))
    df.plot(subplots=True, figsize=(15, 6))
    plt.show()


if __name__ == '__main__':
    draw_ccs811()
    draw_dht22()