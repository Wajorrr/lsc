# Unnamed

Welcome! This repository holds all of the simulation and graphing code for Log-structured Cache, which is based on the source code of [Kangaroo](https://github.com/saramcallister/Kangaroo).

## Simulation Code 

#### Build Instructions

Install SCons (https://scons.org/). Recommended install using python:

```
pip install SCons
cd simulator
scons
```

The executable is `simulator/bin/cache` which can be run with individual configurations.

#### Generate Simulator Configurations

The easiest way to generate workloads is using the `genConfigs.py` script in `run-scripts`.

```
cd run-scripts
./genConfigs.py -h # to see all the options
```

#### Run Simulator

To run the generated configurations, use `run-scripts/runLocal.py`.

```
./runLocal.py configs --jobs 3
```

Each simulator instance is single threaded. Multiple configs can be run at once using the jobs parameter.

#### Adding Parsing Code

One of the most common code additions is adding parsers for new trace formats. To add a new parser,
1) Write a parser that inherits from Parser (in `simulator/parsers/parser.hpp`). Examples can be found in the `parsers` directory.
2) Add the parser as an option in `simulator/parsers/parser.cpp`.
3) Add parser to the configuration generator if desired (`run-scripts/genConfigs.py`).

Supported trace format:
* zipf
* processed block traces：bin、bin.zst

## Cache Replacement Algorithms & Log-structured Cache Simulation

All the cache replacement algorithms' codes are in the `cacheAlgo` directory, and the codes of log-structured cache are in the `segment` directory.

Supported cache replacement algorithm:
* FIFO
* LRU
* S3FIFO
* SIEVE

Supported log-structured cache algorithm:
* FIFOLog
* RIPQ

## Graphing

All the graphing scripts are in the `graph-scripts` directory.