# Changelog

This document lists the version history of the software and describes relevant changes introduced in each version.
Versions are ordered from most to less recent.

## Version 3.2 (06/02/2024)

- New ProfileDescriptor field (flowID) representing MPAM Partition ID has been added as optional field to traffic profile configuration files; Added example configuration using new flowID field
- Use gem5 PartitionFieldsExtension extension to store flowIDs in generated packets
- Consolidated internal packet metadata tagging functionality into PacketTagger
- Bump C++ version used to 17 to retain compatibility with current libabseil and protobuf
- Add gem5 v22.0+ compatibility; drop gem5 v20.1 support; note that gem5 v23.1+ is required to map flowIDs to PartitionFieldsExtensions
- Updated documentation file to show new features

## Version 3.1 (03/02/2020)

- `ATPDevice` gem5 model is a programmable device enabling run-time configuration of traffic generation
  - Supports generation of [DeviceTree](https://www.devicetree.org/) entry for discovery by Linux
- Support for integration with gem5 v20.1
- New `baremetal_atp.py` configuration for instantiating ProfileGen and ATPDevice in a gem5 simulation
- New Linux kernel modules under `linux` for driving ATPDevice based simulations
  - Enable allocation of shared memory buffers, configuration and activation of traffic profiles
  - Complementary user API under `linux/uapi` for applications to access driving capabilities
  - Integration tests under `linux/test` for verifying kernel module and user API correctness
- New programmability.md document, which specifies
  - The *Programmers Model* implemented by ATPDevice
  - The Linux kernel modules and user API design and usage
- Version history now in CHANGELOG.md document
- Other fixes

## Version 3.0 (19/05/2020) [initial release]

- Standalone Engine C++ code, Protocol Buffer data layer and build files
  - Standalone Engine test suite in `test_atp.[hh|cc]`
- Examples of traffic profiles in `.atp` files under `configs`
- `ProfileGen` gem5 model is an adapter enabling integration with open-source [gem5 simulator](https://github.com/gem5/gem5)
  - See [building with ATP Engine with gem5](https://github.com/ARM-software/ATP-Engine#hosted-gem5) for details
  - Standard `atp.py` configuration for instantiating ProfileGen in a gem5 simulation
- Python utilities under `utils` to use for exploration of traffic profiles and Engine traces
- Software specification in Arm AMBA ATP Engine Specification (IHI0092) - Issue A
- License statement - Clear BSD License
- Usage instructions in README.md
