# AMBA ATP Engine

AMBA *Adaptive Traffic Profiles* (ATP) Engine is a C++11 implementation of the AMBA ATP specification ([ARM IHI 0082A](http://infocenter.arm.com/help/topic/com.arm.doc.ihi0082a/IHI0082A_amba_adaptive_traffic_profiles_specification.pdf)). The latter describes *Traffic Profiles* as definitions of the transaction characteristics of an interface. With AMBA ATP Engine, Traffic Profiles may be defined and composed in order to model master / slave component behaviours within a system.

## Description

The following are central concepts to AMBA ATP Engine:

* **FIFO Model** - Tracks data either read or written by a master or slave device. Imposes data rate constraints.
* **Packet Descriptor** - Defines patterns for both addresses and sizes of packets generated.
* **Traffic Profile Descriptor** - Abstract entity defining a Traffic Profile as per the specification.
	* **Master Traffic Profile** - Sends requests and receives responses based on its FIFO model and Packet Descriptor.
	* **Checker Traffic Profile** - Tracks another Traffic Profile Descriptor and records its transactions.
	* **Delay Traffic Profile** - Implements a simple delay block. Useful for defining pauses on combining Traffic Profiles.
	* **Slave Traffic Profile** - Fixed latency and bandwidth memory slave. Receives requests and sends responses based on its FIFO model.
* **Events** - Drive interactions among Traffic Profiles. They represent occurrences within the Engine.
* **Kronos** - Engine internal Future Event List (FEL) for event handling.
* **Traffic Profile Manager (TPM)** - Central entity that manages all configured Traffic Profiles and arbitrates their activation and deactivation.
* **Stream** - Buffered sequence of packets defined by one or more Traffic Profiles chained together on *termination* events.
	* TPM provides Stream Management APIs for initialization, configuration and activation of Streams.
* **ATP Files (.atp)** - Protocol Buffer based files for Traffic Profile definitions.
* **Adapter** - When integrating the Engine with a host platform, this entity defines the interface between both.

A copy of the official Guide is included in this repository. Please refer to it for further information.

## Installation

The Engine may be built as a standalone executable or as part of a host platform, such as [gem5](https://www.gem5.org). GNU/Linux and macOS environments are supported.

### Requirements

* Build process: [g++](https://gcc.gnu.org/install/) v4.8.1+ or [clang++](https://clang.llvm.org/get_started.html) v3.3+ and [make](https://www.gnu.org/software/make/) v3.80+
* Traffic Profile definitions: [protobuf](https://github.com/protocolbuffers/protobuf/blob/master/src/README.md) v3.6.1+
* Unit testing: [cppunit](https://www.freedesktop.org/wiki/Software/cppunit/) v1.13.0+
* Documentation: [doxygen](http://www.doxygen.nl/manual/install.html) v1.8.10+

If building as part of a host platform, verify host platform requirements are satisfied. For gem5, see [requirements](https://www.gem5.org/documentation/general_docs/building).

```bash
git clone https://github.com/ARM-software/ATP-Engine
```

### Standalone

```bash
cd ATP-Engine/
make -j $(nproc)
```

An executable ``atpeng`` and a static library ``libatp.a`` are produced as a result.

### Hosted (gem5)

```bash
git clone https://github.com/gem5/gem5 -b v20.0.0.3
cd gem5/
scons EXTRAS=../ATP-Engine -j $(nproc) build/ARM/gem5.opt
```

ATP Engine is integrated in the resulting ``gem5/build/ARM/gem5.opt`` binary.

## Usage

### Traffic Profiles Definition

Traffic Profiles are defined in ATP files (``.atp``), which are later loaded into the Engine through TPM. Several example ATP files may be found under ``configs/``. See *Appendix A - AMBA ATP Engine Configuration* in the official Guide for documentation on ``.atp`` syntax.

### Standalone

#### Basic use

```bash
./atpeng [.atp file, ...] <rate> <latency>
```

This will instantiate and activate the Traffic Profiles defined in those files along with a default Slave Traffic Profile defined by ``rate`` and ``latency``.

#### Interactive mode (experimental)

```bash
./atpeng -i
```

This will spawn an interactive shell. Type ``help`` from within for more information.

#### Output

At the end of a usage, the Engine produces a set of statistics, both global and per maste / slave component. See *3.4 - Statistics* in the official Guide.

### Hosted (gem5)

The ``gem5/`` directory contains models and configurations for integration with the gem5 simulator, in particular:

* ``ProfileGen`` - gem5 Adapter layer for the Engine. Handles interactions between both entities and exposes Engine APIs to the rest of gem5.
* ``atp.py`` - gem5 configuration script with integrated ProfileGen support.

#### Basic use

gem5 may parse, configure, instantiate and run the simulation from ``atp.py`` as follows:

```bash
gem5/build/ARM/gem5.opt ATP-Engine/gem5/atp.py
```

The following are some examples of ``atp.py`` usage:

```bash
# Load and activate standard.atp
[...]/atp.py --config_files ATP-Engine/configs/example.atp
# Configure a STREAM_A read Stream and STREAM_B write Stream each on its own master; provide base, range and simulation ID
# Note: these Streams are required to exist within the Engine, i.e. loaded via "config_files" or "config-paths"
[...]/atp.py --streams STREAM_A,MASTER_A,R,0x1000,0x2000,123456 STREAM_B,MASTER_B,R,0x5000,0x2000,123457
# Load all ATP files and activate Traffic Profiles under gui_examples/
[...]/atp.py --config-paths ATP-Engine/configs/gui_examples
```

#### Output

Engine statistics are combined into gem5 statistics when running along it. See [Understanding gem5 statistics and output](https://www.gem5.org/documentation/learning_gem5/part1/gem5_stats/) for more information.

## Testing

Engine unit tests are based on the [cppunit](https://www.freedesktop.org/wiki/Software/cppunit/) library. To run all tests:

```bash
./atpeng
```

## Authors and acknowledgment

* AMBA ATP Specification (ARM IHI 0082A)
	* Author - Bruce Mathewson (bruce.mathewson@arm.com)
* AMBA ATP Engine (ARM IHI 0092A)
	* Author - Matteo Andreozzi (matteo.andreozzi@arm.com)
	* Maintainers - Matteo Andreozzi, Adrian Herrera (adrian.herrera@arm.com)

## License

AMBA ATP Engine code is licensed under the *Clear BSD License*, see ``LICENSE`` for details.
