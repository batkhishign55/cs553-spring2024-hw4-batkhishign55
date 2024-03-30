# pip3 install matplotlib

import matplotlib.pyplot as plt
import os

res = []
hash_plot=[[0 for _ in range(3)] for _ in range(3)]
sort_plot=[[0 for _ in range(3)] for _ in range(3)]
write_plot=[[0 for _ in range(3)] for _ in range(3)]
            
x = [1, 4, 16]

def extract_info():
    file = open('./result.log', 'r')
    lines = file.readlines()
    entry = {}

    for line in lines:
        if line.startswith('NUM_THREADS_HASH'):
            # print("hash "+line[18:])
            entry['hash'] = int(line[18:])
        if line.startswith('NUM_THREADS_SORT'):
            # print("sort "+line[18:])
            entry['sort'] = int(line[18:])
        if line.startswith('NUM_THREADS_WRITE'):
            # print("write "+line[19:])
            entry['write']= int(line[19:])
        if line.startswith('Elapsed time (s)'):
            print("{:.2f}%".format(100-float(line[18:])/111.598420*100))
            entry['time']= float(line[18:])
            # print(entry)
            res.append(entry['time'])
            if entry['hash']==entry['sort']:
                write_plot[x.index(entry['hash'])][x.index(entry['write'])]=entry['time']
            if entry['write']==entry['sort']:
                hash_plot[x.index(entry['write'])][x.index(entry['hash'])]=entry['time']
            if entry['hash']==entry['write']:
                sort_plot[x.index(entry['hash'])][x.index(entry['sort'])]=entry['time']
            entry = {}

xs = ["1", "4", "16"]

def plot_data():

    fig, axs = plt.subplots(1)
    axs.plot(xs, hash_plot[0], label="sort, write 1 thread")
    axs.plot(xs, hash_plot[1], label="sort, write 4 thread")
    axs.plot(xs, hash_plot[2], label="sort, write 16 thread")
    axs.set_title('Hash - thread performance')
    axs.legend()
    axs.set_ylabel('Elapsed time(s)')

    fig.tight_layout()
    plt.savefig('hash.png')

    fig, axs = plt.subplots(1)
    axs.plot(xs, sort_plot[0], label="hash, write 1 thread")
    axs.plot(xs, sort_plot[1], label="hash, write 4 thread")
    axs.plot(xs, sort_plot[2], label="hash, write 16 thread")
    axs.set_title('Sort - thread performance')
    axs.legend()
    axs.set_ylabel('Elapsed time(s)')

    fig.tight_layout()
    plt.savefig('sort.png')

    fig, axs = plt.subplots(1)
    axs.plot(xs, write_plot[0], label="sort, hash 1 thread")
    axs.plot(xs, write_plot[1], label="sort, hash 4 thread")
    axs.plot(xs, write_plot[2], label="sort, hash 16 thread")
    axs.set_title('Write - thread performance')
    axs.legend()
    axs.set_ylabel('Elapsed time(s)')

    fig.tight_layout()
    plt.savefig('write.png')

    xss=[]
    for i in (1,4,16):
        for j in (1,4,16):
            for k in (1,4,16):
                xss.append("h"+str(i)+" s"+str(j)+" w"+str(k))

    fig, axs = plt.subplots(1)
    axs.plot(xss, res, label="h-hash, s-sort, w-write")
    axs.set_title('Overall performance')
    axs.legend()
    axs.set_ylabel('Elapsed time(s)')
    plt.xticks(rotation=90)  # Direct rotation

    fig.tight_layout()
    plt.savefig('overall.png')


if __name__ == "__main__":
    # build_file_loc('cpu')
    extract_info()
    plot_data()
    # print(net_data)