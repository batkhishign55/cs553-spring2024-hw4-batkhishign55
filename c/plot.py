# pip3 install matplotlib

import matplotlib.pyplot as plt
import os

res = []

def extract_info():
    file = open('./result.log', 'r')
    lines = file.readlines()

    for line in lines:
        entry = {}
        if line.startswith('NUM_THREADS_HASH'):
            print(line[18:])
            entry['hash'] = int(line[18:])
        if line.startswith('NUM_THREADS_SORT'):
            print(line[18:])
            entry['sort'] = int(line[18:])
        if line.startswith('NUM_THREADS_WRITE'):
            print(line[19:])
            entry['write']= int(line[10:])
        if line.startswith('Elapsed time (s)'):
            print(line[18:])
            entry['time']=int (line[18:])
            res.append(entry)
            entry = {}
            
x = [1, 2, 4, 8, 16, 32, 64]


def plot_cpu_data():
    fig, axs = plt.subplots(2)
    axs[0].plot(x, cpu_data['eps'][0], label="baremetal")
    axs[0].plot(x, cpu_data['eps'][1], label="container")
    axs[0].plot(x, cpu_data['eps'][2], label="vm")
    axs[0].set_title('CPU - Events per Second')
    axs[0].legend()

    axs[1].plot(x, cpu_data['lat'][0], label="baremetal")
    axs[1].plot(x, cpu_data['lat'][1], label="container")
    axs[1].plot(x, cpu_data['lat'][2], label="vm")
    axs[1].set_title('CPU - Average Latency (ms)')
    axs[1].legend()

    fig.tight_layout()
    plt.savefig('cpu.png')


def plot_mem_data():

    fig, axs = plt.subplots(2)
    axs[0].plot(mem_data['to'][0], label="baremetal")
    axs[0].plot(mem_data['to'][1], label="container")
    axs[0].plot(mem_data['to'][2], label="vm")
    axs[0].set_title('Memory - Total Operations')
    axs[0].legend()

    axs[1].plot(mem_data['tp'][0], label="baremetal")
    axs[1].plot(mem_data['tp'][1], label="container")
    axs[1].plot(mem_data['tp'][2], label="vm")
    axs[1].set_title('Memory - Throughput (MiB/sec)')
    axs[1].legend()

    fig.tight_layout()
    plt.savefig('mem.png')


def plot_disk_data():

    fig, axs = plt.subplots(2)
    axs[0].plot(disk_data['to'][0], label="baremetal")
    axs[0].plot(disk_data['to'][1], label="container")
    axs[0].plot(disk_data['to'][2], label="vm")
    axs[0].set_title('Disk - Total Operations')
    axs[0].legend()

    axs[1].plot(disk_data['tp'][0], label="baremetal")
    axs[1].plot(disk_data['tp'][1], label="container")
    axs[1].plot(disk_data['tp'][2], label="vm")
    axs[1].set_title('Disk - Throughput (MiB/sec)')
    axs[1].legend()

    fig.tight_layout()
    plt.savefig('disk.png')


def plot_net_data():

    fig, axs = plt.subplots(2)
    axs[0].plot(net_data['lat'][0], label="baremetal")
    axs[0].plot(net_data['lat'][1], label="container")
    axs[0].plot(net_data['lat'][2], label="vm")
    axs[0].set_title('Network - Latency')
    axs[0].legend()

    axs[1].plot(net_data['tp'][0], label="baremetal")
    axs[1].plot(net_data['tp'][1], label="container")
    axs[1].plot(net_data['tp'][2], label="vm")
    axs[1].set_title('Network - Throughput (Gbits/sec)')
    axs[1].legend()

    fig.tight_layout()
    plt.savefig('net.png')


if __name__ == "__main__":
    # build_file_loc('cpu')
    extract_info()
    # plot_info_data()
    # print(net_data)