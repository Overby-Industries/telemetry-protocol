#pragma once

// libotf1 -- the C++ reference implementation of OTF-1.
//
// OTF-1 is defined by the JSON Schema in schema/otf-1.schema.json. This
// header is the same format as C++ values: one struct per packet kind, and
// the two functions that turn them into bytes and back.
//
//     std::string text = otf1::encode(packet);     // compact JSON, one line
//     otf1::Packet packet;
//     std::string why;
//     if (!otf1::decode(text, packet, &why)) { /* not OTF-1; `why` says so */ }
//
// decode() applies the schema's rules in C++ -- required fields, closed
// enums, value ranges, no unknown fields, a source whose role fits the
// packet's kind -- so a program built on this library never acts on a
// packet the schema would reject. The two are held together three ways:
// the test suite runs the decoder over the same examples/ and
// tests/invalid/ the schema is checked against; tools/differential.py
// feeds thousands of mutated packets to both and fails on any
// disagreement; and everything encode() produces is validated against the
// schema.
//
// C++17, no dependencies beyond the standard library.

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "otf1/time.hpp"

namespace otf1 {

enum class Role { Node, Coordinator };
enum class NodeType { Reclaimer, Miner, Refinery, Shuttle };
enum class Zone { LEO, MEO, GEO, Cislunar, NEA, Belt, Surface };
enum class ObjectType { Fragment, SpentStage, DerelictSatellite, Regolith, Volatile, Ore };

// What a node can be asked to do, and what it reports having done. The two
// enums are the same six verbs in two tenses; eventActionFor() is the map.
enum class TaskAction { Capture, Process, Refine, Store, Deorbit, Deploy };
enum class EventAction { Captured, Processed, Refined, Stored, Deorbited, Deployed };
EventAction eventActionFor(TaskAction action);

enum class RepurposedAs { ShieldBlock, Aggregate, Ingot, Propellant, Polymer, None };
enum class OutcomeStatus { Stored, InTransit, Delivered, Failed };

// Health as the node itself judges it.
//   Nominal   available for work
//   Degraded  finishing what it holds, taking nothing new
//   Safe      local fault response has taken over; any held task is abandoned
enum class Mode { Nominal, Degraded, Safe };

// The node's own view of coordinator contact.
enum class LinkView { Connected, Autonomous };

enum class TaskState { Active, Completed, Failed };
enum class AckResult { Accepted, Rejected };
enum class RejectReason { Busy, Degraded, SafeMode, Expired, Unsupported, Refused };

const char* toString(Role value);
const char* toString(NodeType value);
const char* toString(Zone value);
const char* toString(ObjectType value);
const char* toString(TaskAction value);
const char* toString(EventAction value);
const char* toString(RepurposedAs value);
const char* toString(OutcomeStatus value);
const char* toString(Mode value);
const char* toString(LinkView value);
const char* toString(TaskState value);
const char* toString(AckResult value);
const char* toString(RejectReason value);

struct Endpoint {
    std::string id;
    Role role = Role::Node;
};

struct Location {
    Zone zone = Zone::LEO;
    std::optional<double> altitudeKm;
    std::optional<double> inclinationDeg;
};

struct Target {
    ObjectType objectType = ObjectType::Fragment;
    std::optional<std::string> catalogId;
    std::optional<double> sizeCm;
    std::optional<double> massKg;
};

// One unit of work a coordinator can offer to a node.
struct Task {
    std::string id;
    TaskAction action = TaskAction::Capture;
    Location location;
    std::optional<Target> target;
};

// A node's report on the task it holds, or the last one it finished.
struct TaskStatus {
    std::string id;
    TaskState state = TaskState::Active;
    std::optional<double> progress;  // 0..1
};

// kind: "event" -- something that happened to material. The original OTF-1
// packet, and the one the public record is built from.
struct EventPacket {
    TimeMs timestamp = 0;
    std::string missionId;
    std::string podId;
    NodeType missionType = NodeType::Reclaimer;
    Location location;
    std::optional<Target> target;
    EventAction action = EventAction::Captured;
    std::optional<double> processedKg;
    std::optional<RepurposedAs> repurposedAs;
    OutcomeStatus outcomeStatus = OutcomeStatus::Stored;
    bool smessc = false;
    std::optional<double> odciContributionKg;
    std::optional<std::string> proofOfCleanupToken;
    std::optional<std::string> taskId;  // the assignment this event completes
};

// kind: "status" -- a node's complete snapshot of itself.
struct StatusPacket {
    TimeMs timestamp = 0;
    Endpoint source;  // role is always Node
    std::uint64_t seq = 0;
    NodeType type = NodeType::Reclaimer;
    Mode mode = Mode::Nominal;
    std::optional<std::string> phase;  // local control state name, informative only
    LinkView link = LinkView::Autonomous;
    std::vector<TaskAction> capabilities;
    Location location;
    std::optional<TaskStatus> task;
};

// kind: "heartbeat" -- liveness, from either side.
struct HeartbeatPacket {
    TimeMs timestamp = 0;
    Endpoint source;
    std::uint64_t seq = 0;
};

// kind: "assignment" -- a coordinator offering one task to one node.
struct AssignmentPacket {
    TimeMs timestamp = 0;
    Endpoint source;  // role is always Coordinator
    std::string destination;
    std::uint64_t seq = 0;
    Task task;
    TimeMs acceptBy = 0;  // the node must refuse the offer after this instant
};

// kind: "ack" -- a node's answer to an assignment.
struct AckPacket {
    TimeMs timestamp = 0;
    Endpoint source;  // role is always Node
    std::string destination;
    std::uint64_t seq = 0;
    std::string taskId;
    AckResult result = AckResult::Accepted;
    std::optional<RejectReason> reason;  // present exactly when rejected
};

using Packet =
    std::variant<EventPacket, StatusPacket, HeartbeatPacket, AssignmentPacket, AckPacket>;

// Serialise a packet as one line of compact JSON.
std::string encode(const EventPacket& packet);
std::string encode(const StatusPacket& packet);
std::string encode(const HeartbeatPacket& packet);
std::string encode(const AssignmentPacket& packet);
std::string encode(const AckPacket& packet);
std::string encode(const Packet& packet);

// Parse and validate one packet. Returns false -- and says why in `error` --
// for anything that is not a well-formed OTF-1 packet: malformed JSON, a
// missing or mistyped field, a value outside a closed enum, an unknown
// field, or a source whose role does not fit the packet's kind.
bool decode(const std::string& text, Packet& out, std::string* error = nullptr);

}  // namespace otf1
