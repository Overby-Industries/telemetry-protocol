# Overby Telemetry Format v1 (OTF‑1)

[![Version](https://img.shields.io/badge/Version-OTF--1-blue?style=for-the-badge&logo=github)](schema/otf-1.schema.json)
[![Validate](https://img.shields.io/github/actions/workflow/status/Overby-Industries/telemetry-protocol/validate.yml?branch=main&style=for-the-badge&label=Validate&logo=githubactions&logoColor=white)](https://github.com/Overby-Industries/telemetry-protocol/actions/workflows/validate.yml)
[![C++](https://img.shields.io/github/actions/workflow/status/Overby-Industries/telemetry-protocol/cpp.yml?branch=main&style=for-the-badge&label=C%2B%2B%2017&logo=cplusplus&logoColor=white)](https://github.com/Overby-Industries/telemetry-protocol/actions/workflows/cpp.yml)
[![License](https://img.shields.io/badge/License-Apache_2.0-lightgrey?style=for-the-badge&logo=apache)](LICENSE)
[![JSON Schema](https://img.shields.io/badge/JSON_Schema-Draft_2020--12-purple?style=for-the-badge&logo=json)](https://json-schema.org/draft/2020-12/release-notes)
[![Certification](https://img.shields.io/badge/Certification-SMESSC_%7C_ODCI-green?style=for-the-badge)](#certification-frameworks)
[![Status](https://img.shields.io/badge/Status-Open_Consultation-orange?style=for-the-badge)](#roadmap-for-otf1)

**Overby Industries – Telemetry Protocol Specification**

---

## Overview
OTF‑1 (Overby Telemetry Format v1) the canonical open protocol, but also ties together with the Overby certification frameworks:

- SMESSC → Space Mining Environmental, Safety, & Sustainability Certification
- ODCI → Orbital Debris Cleanup Index (live metric)
- Proof of Cleanup (PoC) credits → blockchain‑verifiable KPI
- Overby Transparency Dashboard → the place where OTF‑1 feeds + certifications display

This becomes our authority layer → regulators, insurers, investors, operators see Overby as both operator AND standards body, much like ISO for space.

OTF‑1 defines the **Overby Telemetry Format** for real‑time reporting of:
- Autonomous space mining operations
- Orbital debris reclamation
- Environmental impact metrics
- Certification compliance (SMESSC, ODCI)

This open standard ensures Overby’s transparency while enabling:
- Public trust via live data feeds
- Auditable logs for regulators/insurers
- Interoperability across fleets and partners

---

## Why OTF‑1?
- **Trust** → Transparency is verifiable via blockchain Proof of Cleanup credit logging.  
- **Authority** → OTF‑1 standardizes reporting across all Overby missions.  
- **Interoperability** → Designed for spacecraft PLC runtimes + ground ops dashboards.  
- **Legitimacy** → Enables international recognition and compliance.  

---

## Certification Frameworks
- SMESSC (Space Mining Environmental, Safety & Sustainability Certification)
  - Ensures operations follow Overby’s zero-waste, risk‑mitigated, and ethical mining/reclamation standards.
  - Incorporated into every OTF packet under certifications.SMESSC.

- ODCI (Orbital Debris Cleanup Index)
  - Aggregated global metric (percentage of orbit cleaned, debris mass removed).
  - Incremental contributions (ODCI_contribution) are logged per event.
  - Investors/regulators can watch “orbital health” in a live metric.

- Proof of Cleanup Credits (PoC)
  - Every debris capture/removal event issues a blockchain token.
  - Immutable record that “X object was reclaimed, Y kg removed”.
  - Tradable or certifiable with insurers/operators as compliance evidence.

# Benefits

- OTF‑1 schema JSON = **machine & human readable** telemetry → simple, global adoption.  
- Certification integration: SMESSC | ODCI | PoC Credit.  
- Creates Overby’s **authority layer** → you’re not just *cleaning space*, you’re **defining the rules of the game**.  
- This signals to **UN, ESA, NASA, insurers**: *“Overby sets the standard globally.”*  
- Public sees transparent metrics → “leaps and bounds” feel LITERAL on your dashboard.

## Core Schema (JSON)

```json
{
  "version": "OTF-1",
  "timestamp": "2025-02-18T14:05:22Z",
  "mission": {
    "id": "orbital-demo-1",
    "podId": "A1",
    "type": "reclaimer"
  },
  "location": {
    "altitude_km": 825,
    "inclination_deg": 51.6,
    "zone": "LEO"
  },
  "target": {
    "catalogId": "NORAD-33953",
    "objectType": "fragment",
    "size_cm": 45,
    "mass_kg": 5.2
  },
  "action": "captured",
  "outcome": {
    "processedKg": 5.0,
    "repurposedAs": "shield_block",
    "status": "stored"
  },
  "certifications": {
    "SMESSC": true,
    "ODCI_contribution": 5.0,
    "ProofOfCleanupToken": "0xabc123debriscredit"
  }
}
```
## Formal Schema & Validation

The JSON above is now a real, enforceable [JSON Schema](schema/otf-1.schema.json)
(Draft 2020-12), with runnable examples in [`examples/`](examples/) and a
validator in [`tools/validate.py`](tools/validate.py):

```bash
pip install jsonschema
python tools/validate.py                 # validates everything under examples/
python tools/validate.py my-packet.json
python tools/validate.py fleet-log.jsonl # a log: one packet per line
python tools/validate.py --expect-invalid tests/invalid   # every one must be rejected
```

[`tests/invalid/`](tests/invalid/) holds one malformed packet per file, each
broken in exactly one way. CI
([`.github/workflows/validate.yml`](.github/workflows/validate.yml)) runs
both checks on every push and PR, so a schema change that breaks a good
example, or that quietly starts accepting a bad packet, gets caught before
it merges.

## Packet Kinds

OTF‑1 began as one packet: the mission event above. A fleet also has to say
things that are not events. "I am here and healthy." "Take this task." "No."
[Starlifter OS](https://github.com/Overby-Industries/starlifter-os) calls
for OTF‑1 to be the only wire format between a vehicle and its coordinator,
so a packet now states its `kind`.

| `kind` | Sent by | Says |
|---|---|---|
| `event` | node | Something happened to material: a capture, a processing run, a deployment. The packet above, and the one the public record is built from. |
| `status` | node | A complete snapshot of the node: health, link, capabilities, location, and the task it holds or last finished. |
| `heartbeat` | node or coordinator | I am alive. |
| `assignment` | coordinator | An offer of one task to one node, valid until `acceptBy`. |
| `ack` | node | The answer to an offer: accepted, or rejected with a reason. |

**Nothing already written breaks.** A packet with no `kind` is an `event`,
so every packet that was valid before the fleet kinds existed is valid now,
unchanged. Events gained two things, both optional: `mission.type` may be
`shuttle`, and `taskId` names the assignment an event completes.

### Fields the fleet kinds share

- `source` → who sent it: `{ "id": "A1", "role": "node" }`. `role` is `node`
  or `coordinator`. A `status` or `ack` must come from a node and an
  `assignment` from a coordinator; the schema enforces it.
- `seq` → a per-source counter that increases with every fleet packet the
  source sends. An integer from 0 to 2^53.
- `destination` → on `assignment` and `ack`, the id of the one recipient.
- `timestamp` → when the sender transmitted the packet. As everywhere in
  OTF‑1, an RFC 3339 instant with an uppercase `T` and either `Z` or a
  numeric offset: `2025-02-18T14:05:30Z`. UTC is preferred.

### `status`

```json
{
  "version": "OTF-1",
  "kind": "status",
  "timestamp": "2025-02-18T14:05:30Z",
  "source": { "id": "A1", "role": "node" },
  "seq": 412,
  "node": {
    "type": "reclaimer",
    "mode": "nominal",
    "phase": "Capturing",
    "link": "connected",
    "capabilities": ["capture", "deorbit"]
  },
  "location": { "altitude_km": 825, "inclination_deg": 51.6, "zone": "LEO" },
  "task": { "id": "T-0042", "state": "active", "progress": 0.4 }
}
```

- `node.type` → `reclaimer`, `miner`, `refinery` or `shuttle`.
- `node.mode` → health as the node itself judges it.
  - `nominal` → available for work.
  - `degraded` → finishing what it holds, taking nothing new.
  - `safe` → local fault response has taken over; any held task is abandoned.
- `node.phase` → the name of the node's local control state, reported
  verbatim (for a shuttle, a `ShuttleFSM` state such as `OrbitalCruise`).
  Informative only. A coordinator must never branch on it.
- `node.link` → the node's own view of coordinator contact. `autonomous`
  means it has stopped hearing the coordinator and is running on local
  defaults, even though this packet may still get through.
- `node.capabilities` → the task actions this node can perform. A node
  advertises them so a coordinator needs no built-in table of node types.
- `task` → the task the node is working on, or the last one it finished,
  with `state` of `active`, `completed` or `failed`. It stays in every
  status until a newer task replaces it.

### `heartbeat`

```json
{
  "version": "OTF-1",
  "kind": "heartbeat",
  "timestamp": "2025-02-18T14:05:25Z",
  "source": { "id": "fleet-coordinator", "role": "coordinator" },
  "seq": 96
}
```

### `assignment`

```json
{
  "version": "OTF-1",
  "kind": "assignment",
  "timestamp": "2025-02-18T14:05:00Z",
  "source": { "id": "fleet-coordinator", "role": "coordinator" },
  "destination": "A1",
  "seq": 95,
  "task": {
    "id": "T-0042",
    "action": "capture",
    "location": { "altitude_km": 825, "inclination_deg": 51.6, "zone": "LEO" },
    "target": { "catalogId": "NORAD-33953", "objectType": "fragment", "size_cm": 45, "mass_kg": 5.2 }
  },
  "acceptBy": "2025-02-18T14:05:10Z"
}
```

- `task.action` → what to do: `capture`, `process`, `refine`, `store`,
  `deorbit` or `deploy`. Each is the imperative of an event action, so a
  completed `capture` task is reported as a `captured` event.
- `task.location` and `task.target` → the same objects an event uses.
- `acceptBy` → the node must refuse the offer if it arrives after this
  instant.

### `ack`

```json
{
  "version": "OTF-1",
  "kind": "ack",
  "timestamp": "2025-02-18T14:07:41Z",
  "source": { "id": "A2", "role": "node" },
  "destination": "fleet-coordinator",
  "seq": 388,
  "taskId": "T-0043",
  "result": "rejected",
  "reason": "safe-mode"
}
```

- `result` → `accepted` or `rejected`.
- `reason` → required when rejected, not allowed otherwise.
  - `busy`, `expired`, `degraded`, `safe-mode` → true for now.
  - `unsupported`, `refused` → do not offer this node this task again.

### What a receiver can rely on

- **An assignment is an offer, not a command.** The node's own control
  software has the final say and answers with an `ack`.
- **A node may take an offer only before `acceptBy`.** This bounds how stale
  an offer can be when it lands after minutes in transit. It also means a
  `status` the node sent *after* `acceptBy` reflects its final decision: if
  the task is not listed, the offer was not taken, whichever packet got
  lost.
- **A status is a snapshot, not a change.** One status brings a receiver
  that missed any number of earlier packets fully up to date, including the
  outcome of the last task. A lossy link needs no retransmission.
- **Order a source's packets by `timestamp`, then `seq`,** and discard any
  status or heartbeat that is not newer than what has already been applied.
  Timestamp comes first so that a source which restarts and counts from
  zero is still understood.
- **An event is the record; a status is the bookkeeping.** A completed task
  produces one `event` carrying its `taskId`. A failed task produces none,
  because an event claims something happened to material.

The reference implementation of both ends is
[`starlifter-os`](https://github.com/Overby-Industries/starlifter-os), which
also records the reasoning behind each rule in its
`docs/fleet-coordination.md`.

## C++ Reference Implementation

[`cpp/`](cpp/) holds **libotf1**: the same format as C++ values, with one
struct per packet kind and two functions that turn them into bytes and back.
C++17, no dependencies beyond the standard library.

```cpp
#include <otf1/otf1.hpp>

otf1::HeartbeatPacket beat;
beat.timestamp = 1739887525000;                 // ms since the Unix epoch, UTC
beat.source = {"fleet-coordinator", otf1::Role::Coordinator};
beat.seq = 96;
std::string text = otf1::encode(beat);          // one line of compact JSON

otf1::Packet packet;
std::string why;
if (!otf1::decode(text, packet, &why)) {
    // Not OTF-1. `why` names the field, for example "node/mode is not a
    // recognised value". Nothing in `packet` should be used.
} else if (const auto* status = std::get_if<otf1::StatusPacket>(&packet)) {
    // status->mode, status->task, ...
}
```

`decode()` applies the schema's rules in C++: required fields, closed enums,
value ranges, no unknown fields, real timestamps, a source whose role fits
the packet's kind. A program built on it never acts on a packet the schema
would reject.

**Build and test:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

**Use it from another CMake project:**

```cmake
add_subdirectory(path/to/telemetry-protocol)
target_link_libraries(your_target PRIVATE otf1::otf1)
```

**Check packets with the C++ decoder** instead of the schema:

```bash
./build/otf1_check examples/*.json
./build/otf1_check fleet-log.jsonl
./build/otf1_check --expect-invalid tests/invalid/*.json
```

### Two implementations, held to one answer

The schema and the C++ decoder are two descriptions of the same format, and
two descriptions drift unless something forces them together. Three things
do:

- **One corpus.** The C++ tests run over this repository's own `examples/`
  and `tests/invalid/`, the same files the schema is validated against.
  Nothing is copied.
- **Differential testing.** [`tools/differential.py`](tools/differential.py)
  mutates the example packets about 17,000 ways (a field dropped, a field
  added, a value swapped for another type, another enum value, a boundary
  number, a malformed timestamp) and requires the schema and the C++
  decoder to give the same accept-or-reject verdict on every one. For each
  packet both accept, the C++ re-encoding must say exactly what the input
  said. It then mutates JSON text at the byte level, about 46,000 inputs,
  and requires the C++ parser to accept exactly what a strict RFC 8259
  reader accepts.
- **CI runs all of it** on every push and pull request, and builds the
  library on Linux and Windows.

```bash
python tools/differential.py        # needs build/otf1_check
```

Writing that tool tightened the format in three places where the two had
room to disagree. Timestamps now have an explicit `pattern` as well as a
`format`, and the validator checks that the date and time are real. `seq`
has a maximum of 2^53, beyond which a JSON number can no longer tell
neighbouring integers apart. And strings must be well-formed UTF-8.

## Example Usage
`Reclaimer Pod Capture Event`
The pod captures a 5.2kg fragment →

- OTF‑1 packet is emitted
- Blockchain PoC credit registered
- Dashboard updated live
- ODCI increment updated by +5kg
- SMESSC flag confirms safe, certified process

`Mining Pod Refinery Report`
Pod converts asteroid regolith into UHPC aggregate →

- OTF‑1 packet logs processing
- SMESSC compliance verified (dust contained, zero orbital debris)
- Environmental/safety scores updated

---

## Roadmap for OTF‑1
Phase 1 (Q4 2025):

- Schema finalized in open consultation (Overby GitHub Issues)
- Simulation feeds (mock JSON events) published

Phase 2 (Q2 2026):

- Ground demo rigs output real telemetry
- Dashboard uses live‑updating WebSocket API

Phase 3 (2030‑2035):

- First Orbital Demonstrators push OTF‑1 packets from CubeSats
- Proof of Cleanup credits issued publicly

Phase 4 (2035+):

- All Overby fleets exclusively speak OTF‑1
- Certification & ODCI adoption by regulators/insurers

## How to Contribute
- Open Issues to propose schema updates
- Submit PRs for JSON schema docs, validation tools, or API middleware
- Share expertise in telemetry, PLC comms, or blockchain audit

## License
OTF‑1 is released under Apache 2.0 for full open-source adoption.
