# microbit-v2-samples

This repository is provides the tooling needed to compile a C/C++ CODAL program for the micro:bit v2 and produce a HEX file that can be downloaded to the device.

## Raising Issues
Any issues regarding the micro:bit are gathered on the [lancaster-university/codal-microbit-v2](https://github.com/lancaster-university/codal-microbit-v2) repository. Please raise yours there too.

# Installation
You need some open source pre-requisites to build this repo. You can either install these tools yourself, or use the docker image provided below.

- [GNU Arm Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads)
- [Github desktop](https://desktop.github.com/)
- [CMake](https://cmake.org/download/)
- [Python 3](https://www.python.org/downloads/)

We use Ubuntu Linux for most of our tests. You can also install these tools easily through the package manager:

```
    sudo apt install gcc
    sudo apt install git
    sudo apt install cmake
    sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi
```

## Yotta
For backwards compatibility with [microbit-samples](https://github.com/lancaster-university/microbit-samples) users, we also provide a yotta target for this repository.

## Docker
You can use the [Dockerfile](https://github.com/lancaster-university/microbit-v2-samples/blob/master/Dockerfile) provided to build the samples, or your own project sources, without installing additional dependencies.

Run the following command to build the image locally:

```
    docker build -t microbit-tools .
```

# Building
- Clone this repository
- In the root of this repository type `python build.py`
- The hex file will be built `MICROBIT.HEX` and placed in the root folder.

## Docker
You can use the image you built previously to build the project sources in the current working directory (equivalent to executing `build.py`).

```
    docker run -v $(pwd):/app --rm microbit-tools
```

You can also provide additional arguments like `--clean`.

```
    docker run -v $(pwd):/app --rm microbit-tools --clean
```

# Developing
You will find a simple main.cpp in the `source` folder which you can edit. CODAL will also compile any other C/C++ header files our source files with the extension `.h .c .cpp` it finds in the source folder.

The `samples` folder contains a number of simple sample programs that utilise you may find useful.

# Compatibility
This repository is designed to follow the principles and APIs developed for the first version of the micro:bit. We have also included a compatibility layer so that the vast majority of C/C++ programs built using [microbit-dal](https://www.github.com/lancaster-university/microbit-dal) will operate with few changes.

# Documentation
API documentation is embedded in the code using doxygen. We will produce integrated web-based documentation soon.

# This project is a branch for testing out the teak runtime on trashbots.

Key change is adding an option to compile the codal runtime with nordic compliant BLE UART service

for debugging, if the device is usins the serial output

Type screen /dev/cu.usbmodem1422 115200, replacing the 'usbmodem' number with the number you found in the previous step. This will open the micro:bit's serial output and show all messages received from the device. 

That short instruction comes form here:
https://support.microbit.org/support/solutions/articles/19000022103-outputing-serial-data-from-the-micro-bit-to-a-computer
