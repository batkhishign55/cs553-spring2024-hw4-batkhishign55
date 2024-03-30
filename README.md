[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-24ddc0f5d75046c5622901739e7c5dd533143b0c8e959d652212380cedb1ea36.svg)](https://classroom.github.com/a/C5s9grq-)

### CS553 Cloud Computing Assignment 4 Repo

**Illinois Institute of Technology**

**Students**:

- Batkhishig Dulamsurankhor (bdulamsurankhor@hawk.iit.edu) A20543498

## Building and running

Go to c folder where blake3 libraries are stored:

```bash
cd c
```

To build the `hashgen.c` source:

```bash
gcc -O3 -o hashgen hashgen.c blake3.c blake3_dispatch.c blake3_portable.c     blake3_sse2_x86-64_unix.S blake3_sse41_x86-64_unix.S blake3_avx2_x86-64_unix.S     blake3_avx512_x86-64_unix.S -lm
```

To run the executable `hashgen`:

```bash
./hashgen -o 16 -t 8 -i 8 -s 1024
```

## Running the test

Go to c folder where blake3 libraries are stored:

```bash
cd c
```

Add execute privilege to `run.sh`:

```bash
chmod +x run.sh
```

Run `run.sh` file:

```bash
./run.sh
```

## Plotting the graph from the raw log files

The result will be saved in `result.log` file.

```bash
python3 plot.py
```
