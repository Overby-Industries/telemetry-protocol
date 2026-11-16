#include "otf1/otf1.hpp"

#include <cmath>
#include <cstring>
#include <limits>

#include "json.hpp"

namespace otf1 {

namespace {

template <typename E>
struct Name {
    E value;
    const char* text;
};

const Name<Role> kRoles[] = {{Role::Node, "node"}, {Role::Coordinator, "coordinator"}};

const Name<NodeType> kNodeTypes[] = {{NodeType::Reclaimer, "reclaimer"},
                                     {NodeType::Miner, "miner"},
                                     {NodeType::Refinery, "refinery"},
                                     {NodeType::Shuttle, "shuttle"}};

const Name<Zone> kZones[] = {{Zone::LEO, "LEO"},           {Zone::MEO, "MEO"},
                             {Zone::GEO, "GEO"},           {Zone::Cislunar, "cislunar"},
                             {Zone::NEA, "NEA"},           {Zone::Belt, "belt"},
                             {Zone::Surface, "surface"}};

const Name<ObjectType> kObjectTypes[] = {{ObjectType::Fragment, "fragment"},
                                         {ObjectType::SpentStage, "spent-stage"},
                                         {ObjectType::DerelictSatellite, "derelict-satellite"},
                                         {ObjectType::Regolith, "regolith"},
                                         {ObjectType::Volatile, "volatile"},
                                         {ObjectType::Ore, "ore"}};

const Name<TaskAction> kTaskActions[] = {{TaskAction::Capture, "capture"},
                                         {TaskAction::Process, "process"},
                                         {TaskAction::Refine, "refine"},
                                         {TaskAction::Store, "store"},
                                         {TaskAction::Deorbit, "deorbit"},
                                         {TaskAction::Deploy, "deploy"}};

const Name<EventAction> kEventActions[] = {{EventAction::Captured, "captured"},
                                           {EventAction::Processed, "processed"},
                                           {EventAction::Refined, "refined"},
                                           {EventAction::Stored, "stored"},
                                           {EventAction::Deorbited, "deorbited"},
                                           {EventAction::Deployed, "deployed"}};

const Name<RepurposedAs> kRepurposed[] = {{RepurposedAs::ShieldBlock, "shield_block"},
                                          {RepurposedAs::Aggregate, "aggregate"},
                                          {RepurposedAs::Ingot, "ingot"},
                                          {RepurposedAs::Propellant, "propellant"},
                                          {RepurposedAs::Polymer, "polymer"},
                                          {RepurposedAs::None, "none"}};

const Name<OutcomeStatus> kOutcomeStatuses[] = {{OutcomeStatus::Stored, "stored"},
                                                {OutcomeStatus::InTransit, "in-transit"},
                                                {OutcomeStatus::Delivered, "delivered"},
                                                {OutcomeStatus::Failed, "failed"}};

const Name<Mode> kModes[] = {{Mode::Nominal, "nominal"},
                             {Mode::Degraded, "degraded"},
                             {Mode::Safe, "safe"}};

const Name<LinkView> kLinkViews[] = {{LinkView::Connected, "connected"},
                                     {LinkView::Autonomous, "autonomous"}};

const Name<TaskState> kTaskStates[] = {{TaskState::Active, "active"},
                                       {TaskState::Completed, "completed"},
                                       {TaskState::Failed, "failed"}};

const Name<AckResult> kAckResults[] = {{AckResult::Accepted, "accepted"},
                                       {AckResult::Rejected, "rejected"}};

const Name<RejectReason> kRejectReasons[] = {{RejectReason::Busy, "busy"},
                                             {RejectReason::Degraded, "degraded"},
                                             {RejectReason::SafeMode, "safe-mode"},
                                             {RejectReason::Expired, "expired"},
                                             {RejectReason::Unsupported, "unsupported"},
                                             {RejectReason::Refused, "refused"}};

template <typename E, std::size_t N>
const char* nameOf(const Name<E> (&table)[N], E value) {
    for (const Name<E>& entry : table) {
        if (entry.value == value) return entry.text;
    }
    return "";
}

template <typename E, std::size_t N>
bool valueOf(const Name<E> (&table)[N], const std::string& text, E& out) {
    for (const Name<E>& entry : table) {
        if (text == entry.text) {
            out = entry.value;
            return true;
        }
    }
    return false;
}

// Sequence numbers travel as JSON numbers, which are doubles. Above 2^53 a
// double can no longer tell neighbouring integers apart, so that is the cap.
constexpr double kMaxSeq = 9007199254740992.0;

// For quantities the schema bounds only from below. The JSON parser has
// already refused anything that is not a finite double.
constexpr double kNoMaximum = std::numeric_limits<double>::infinity();

}  // namespace

const char* toString(Role value) { return nameOf(kRoles, value); }
const char* toString(NodeType value) { return nameOf(kNodeTypes, value); }
const char* toString(Zone value) { return nameOf(kZones, value); }
const char* toString(ObjectType value) { return nameOf(kObjectTypes, value); }
const char* toString(TaskAction value) { return nameOf(kTaskActions, value); }
const char* toString(EventAction value) { return nameOf(kEventActions, value); }
const char* toString(RepurposedAs value) { return nameOf(kRepurposed, value); }
const char* toString(OutcomeStatus value) { return nameOf(kOutcomeStatuses, value); }
const char* toString(Mode value) { return nameOf(kModes, value); }
const char* toString(LinkView value) { return nameOf(kLinkViews, value); }
const char* toString(TaskState value) { return nameOf(kTaskStates, value); }
const char* toString(AckResult value) { return nameOf(kAckResults, value); }
const char* toString(RejectReason value) { return nameOf(kRejectReasons, value); }

EventAction eventActionFor(TaskAction action) {
    switch (action) {
        case TaskAction::Capture: return EventAction::Captured;
        case TaskAction::Process: return EventAction::Processed;
        case TaskAction::Refine: return EventAction::Refined;
        case TaskAction::Store: return EventAction::Stored;
        case TaskAction::Deorbit: return EventAction::Deorbited;
        case TaskAction::Deploy: return EventAction::Deployed;
    }
    return EventAction::Stored;
}

// --------------------------------------------------------------------------
// Encoding

namespace {

Json header(const char* kind, TimeMs timestamp) {
    Json packet = Json::object();
    packet.set("version", "OTF-1");
    packet.set("kind", kind);
    packet.set("timestamp", formatTimestamp(timestamp));
    return packet;
}

Json toJson(const Endpoint& endpoint) {
    Json j = Json::object();
    j.set("id", endpoint.id);
    j.set("role", toString(endpoint.role));
    return j;
}

Json toJson(const Location& location) {
    Json j = Json::object();
    if (location.altitudeKm) j.set("altitude_km", *location.altitudeKm);
    if (location.inclinationDeg) j.set("inclination_deg", *location.inclinationDeg);
    j.set("zone", toString(location.zone));
    return j;
}

Json toJson(const Target& target) {
    Json j = Json::object();
    if (target.catalogId) j.set("catalogId", *target.catalogId);
    j.set("objectType", toString(target.objectType));
    if (target.sizeCm) j.set("size_cm", *target.sizeCm);
    if (target.massKg) j.set("mass_kg", *target.massKg);
    return j;
}

Json toJson(const Task& task) {
    Json j = Json::object();
    j.set("id", task.id);
    j.set("action", toString(task.action));
    j.set("location", toJson(task.location));
    if (task.target) j.set("target", toJson(*task.target));
    return j;
}

}  // namespace

std::string encode(const EventPacket& p) {
    Json packet = header("event", p.timestamp);

    Json mission = Json::object();
    mission.set("id", p.missionId);
    mission.set("podId", p.podId);
    mission.set("type", toString(p.missionType));
    packet.set("mission", std::move(mission));

    packet.set("location", toJson(p.location));
    if (p.target) packet.set("target", toJson(*p.target));
    packet.set("action", toString(p.action));

    Json outcome = Json::object();
    if (p.processedKg) outcome.set("processedKg", *p.processedKg);
    if (p.repurposedAs) outcome.set("repurposedAs", toString(*p.repurposedAs));
    outcome.set("status", toString(p.outcomeStatus));
    packet.set("outcome", std::move(outcome));

    Json certifications = Json::object();
    certifications.set("SMESSC", p.smessc);
    if (p.odciContributionKg) certifications.set("ODCI_contribution", *p.odciContributionKg);
    if (p.proofOfCleanupToken) certifications.set("ProofOfCleanupToken", *p.proofOfCleanupToken);
    packet.set("certifications", std::move(certifications));

    if (p.taskId) packet.set("taskId", *p.taskId);
    return packet.dump();
}

std::string encode(const StatusPacket& p) {
    Json packet = header("status", p.timestamp);
    packet.set("source", toJson(p.source));
    packet.set("seq", p.seq);

    Json node = Json::object();
    node.set("type", toString(p.type));
    node.set("mode", toString(p.mode));
    if (p.phase) node.set("phase", *p.phase);
    node.set("link", toString(p.link));
    Json capabilities = Json::array();
    for (TaskAction action : p.capabilities) capabilities.push(toString(action));
    node.set("capabilities", std::move(capabilities));
    packet.set("node", std::move(node));

    packet.set("location", toJson(p.location));
    if (p.task) {
        Json task = Json::object();
        task.set("id", p.task->id);
        task.set("state", toString(p.task->state));
        if (p.task->progress) task.set("progress", *p.task->progress);
        packet.set("task", std::move(task));
    }
    return packet.dump();
}

std::string encode(const HeartbeatPacket& p) {
    Json packet = header("heartbeat", p.timestamp);
    packet.set("source", toJson(p.source));
    packet.set("seq", p.seq);
    return packet.dump();
}

std::string encode(const AssignmentPacket& p) {
    Json packet = header("assignment", p.timestamp);
    packet.set("source", toJson(p.source));
    packet.set("destination", p.destination);
    packet.set("seq", p.seq);
    packet.set("task", toJson(p.task));
    packet.set("acceptBy", formatTimestamp(p.acceptBy));
    return packet.dump();
}

std::string encode(const AckPacket& p) {
    Json packet = header("ack", p.timestamp);
    packet.set("source", toJson(p.source));
    packet.set("destination", p.destination);
    packet.set("seq", p.seq);
    packet.set("taskId", p.taskId);
    packet.set("result", toString(p.result));
    if (p.reason) packet.set("reason", toString(*p.reason));
    return packet.dump();
}

std::string encode(const Packet& packet) {
    return std::visit([](const auto& p) { return encode(p); }, packet);
}

// --------------------------------------------------------------------------
// Decoding

namespace {

// Reads the members of one JSON object, remembering which were asked for.
// finish() then fails if any member was never read, which is how "no
// unknown fields" is enforced without listing the known ones twice.
//
// Every getter returns false and records the first problem; once one has
// failed the rest are no-ops, so a decoder can read a whole object and
// check ok() once at the end.
class Fields {
public:
    Fields(const Json& object, std::string path, std::string& error)
        : object_(object), path_(std::move(path)), error_(error),
          seen_(object.members().size(), false) {
        if (!object.isObject()) {
            fail("", "must be an object");
        }
    }

    bool ok() const { return ok_; }

    // The raw member, or nullptr if absent. Absence is an error if required.
    const Json* get(const char* key, bool required) {
        if (!ok_) return nullptr;
        const Json::Object& members = object_.members();
        for (std::size_t i = 0; i < members.size(); ++i) {
            if (members[i].first == key) {
                seen_[i] = true;
                return &members[i].second;
            }
        }
        if (required) fail(key, "is required");
        return nullptr;
    }

    bool string(const char* key, std::string& out, bool required = true) {
        const Json* value = get(key, required);
        if (value == nullptr) return false;
        if (!value->isString()) return fail(key, "must be a string");
        out = value->asString();
        return true;
    }

    // Identifiers must be present and non-empty: an empty node or task id
    // would make two different things indistinguishable.
    bool id(const char* key, std::string& out) {
        if (!string(key, out)) return false;
        if (out.empty()) return fail(key, "must not be empty");
        return true;
    }

    bool optionalString(const char* key, std::optional<std::string>& out) {
        std::string text;
        if (!string(key, text, false)) return false;
        out = std::move(text);
        return true;
    }

    bool number(const char* key, double& out, bool required, double min, double max) {
        const Json* value = get(key, required);
        if (value == nullptr) return false;
        if (!value->isNumber()) return fail(key, "must be a number");
        if (value->asNumber() < min || value->asNumber() > max) return fail(key, "is out of range");
        out = value->asNumber();
        return true;
    }

    bool optionalNumber(const char* key, std::optional<double>& out, double min, double max) {
        double value = 0.0;
        if (!number(key, value, false, min, max)) return false;
        out = value;
        return true;
    }

    bool boolean(const char* key, bool& out) {
        const Json* value = get(key, true);
        if (value == nullptr) return false;
        if (!value->isBool()) return fail(key, "must be true or false");
        out = value->asBool();
        return true;
    }

    bool seq(const char* key, std::uint64_t& out) {
        double value = 0.0;
        if (!number(key, value, true, 0.0, kMaxSeq)) return false;
        if (value != std::floor(value)) return fail(key, "must be an integer");
        out = static_cast<std::uint64_t>(value);
        return true;
    }

    bool timestamp(const char* key, TimeMs& out) {
        std::string text;
        if (!string(key, text)) return false;
        if (!parseTimestamp(text, out)) return fail(key, "is not an ISO-8601 UTC timestamp");
        return true;
    }

    template <typename E, std::size_t N>
    bool enumeration(const char* key, const Name<E> (&table)[N], E& out, bool required = true) {
        std::string text;
        if (!string(key, text, required)) return false;
        if (!valueOf(table, text, out)) return fail(key, "is not a recognised value");
        return true;
    }

    template <typename E, std::size_t N>
    bool optionalEnumeration(const char* key, const Name<E> (&table)[N], std::optional<E>& out) {
        E value{};
        if (!enumeration(key, table, value, false)) return false;
        out = value;
        return true;
    }

    // Marks this object invalid because something nested inside it was. The
    // nested reader has already recorded the specific reason; keep that.
    void poison() { ok_ = false; }

    bool finish() {
        if (!ok_) return false;
        const Json::Object& members = object_.members();
        for (std::size_t i = 0; i < members.size(); ++i) {
            if (!seen_[i]) return fail(members[i].first.c_str(), "is not an OTF-1 field");
        }
        return true;
    }

    std::string child(const char* key) const {
        return path_.empty() ? std::string(key) : path_ + "/" + key;
    }

    bool fail(const char* key, const char* message) {
        if (ok_) {
            ok_ = false;
            std::string where = std::strlen(key) == 0 ? path_ : child(key);
            if (where.empty()) where = "packet";
            error_ = where + " " + message;
        }
        return false;
    }

private:
    const Json& object_;
    std::string path_;
    std::string& error_;
    std::vector<bool> seen_;
    bool ok_ = true;
};

void readEndpoint(Fields& parent, const char* key, Endpoint& out, std::string& error) {
    const Json* value = parent.get(key, true);
    if (value == nullptr) return;
    Fields f(*value, parent.child(key), error);
    f.id("id", out.id);
    f.enumeration("role", kRoles, out.role);
    if (!f.finish()) parent.poison();
}

void readLocation(Fields& parent, const char* key, Location& out, std::string& error) {
    const Json* value = parent.get(key, true);
    if (value == nullptr) return;
    Fields f(*value, parent.child(key), error);
    f.optionalNumber("altitude_km", out.altitudeKm, 0.0, kNoMaximum);
    f.optionalNumber("inclination_deg", out.inclinationDeg, 0.0, 180.0);
    f.enumeration("zone", kZones, out.zone);
    if (!f.finish()) parent.poison();
}

void readTarget(Fields& parent, const char* key, std::optional<Target>& out, std::string& error) {
    const Json* value = parent.get(key, false);
    if (value == nullptr) return;
    Target target;
    Fields f(*value, parent.child(key), error);
    f.optionalString("catalogId", target.catalogId);
    f.enumeration("objectType", kObjectTypes, target.objectType);
    f.optionalNumber("size_cm", target.sizeCm, 0.0, kNoMaximum);
    f.optionalNumber("mass_kg", target.massKg, 0.0, kNoMaximum);
    if (!f.finish()) {
        parent.poison();
        return;
    }
    out = std::move(target);
}

bool decodeEvent(Fields& f, std::string& error, Packet& out) {
    EventPacket p;
    f.timestamp("timestamp", p.timestamp);

    if (const Json* mission = f.get("mission", true)) {
        Fields m(*mission, "mission", error);
        m.string("id", p.missionId);
        m.string("podId", p.podId);
        m.enumeration("type", kNodeTypes, p.missionType);
        if (!m.finish()) return false;
    }
    readLocation(f, "location", p.location, error);
    readTarget(f, "target", p.target, error);
    f.enumeration("action", kEventActions, p.action);

    if (const Json* outcome = f.get("outcome", true)) {
        Fields o(*outcome, "outcome", error);
        o.optionalNumber("processedKg", p.processedKg, 0.0, kNoMaximum);
        o.optionalEnumeration("repurposedAs", kRepurposed, p.repurposedAs);
        o.enumeration("status", kOutcomeStatuses, p.outcomeStatus);
        if (!o.finish()) return false;
    }
    if (const Json* certifications = f.get("certifications", true)) {
        Fields c(*certifications, "certifications", error);
        c.boolean("SMESSC", p.smessc);
        c.optionalNumber("ODCI_contribution", p.odciContributionKg, 0.0, kNoMaximum);
        c.optionalString("ProofOfCleanupToken", p.proofOfCleanupToken);
        if (!c.finish()) return false;
    }
    if (f.get("taskId", false) != nullptr) {
        std::string taskId;
        f.id("taskId", taskId);
        p.taskId = std::move(taskId);
    }
    if (!f.finish()) return false;
    out = std::move(p);
    return true;
}

bool decodeStatus(Fields& f, std::string& error, Packet& out) {
    StatusPacket p;
    f.timestamp("timestamp", p.timestamp);
    readEndpoint(f, "source", p.source, error);
    if (f.ok() && p.source.role != Role::Node) return f.fail("source/role", "must be node");
    f.seq("seq", p.seq);

    if (const Json* node = f.get("node", true)) {
        Fields n(*node, "node", error);
        n.enumeration("type", kNodeTypes, p.type);
        n.enumeration("mode", kModes, p.mode);
        n.optionalString("phase", p.phase);
        n.enumeration("link", kLinkViews, p.link);
        if (const Json* capabilities = n.get("capabilities", true)) {
            if (!capabilities->isArray()) return n.fail("capabilities", "must be an array");
            for (const Json& item : capabilities->items()) {
                TaskAction action{};
                if (!item.isString() || !valueOf(kTaskActions, item.asString(), action)) {
                    return n.fail("capabilities", "holds a value that is not a task action");
                }
                for (TaskAction existing : p.capabilities) {
                    if (existing == action) return n.fail("capabilities", "lists an action twice");
                }
                p.capabilities.push_back(action);
            }
        }
        if (!n.finish()) return false;
    }
    readLocation(f, "location", p.location, error);

    if (const Json* task = f.get("task", false)) {
        TaskStatus status;
        Fields t(*task, "task", error);
        t.id("id", status.id);
        t.enumeration("state", kTaskStates, status.state);
        t.optionalNumber("progress", status.progress, 0.0, 1.0);
        if (!t.finish()) return false;
        p.task = std::move(status);
    }
    if (!f.finish()) return false;
    out = std::move(p);
    return true;
}

bool decodeHeartbeat(Fields& f, std::string& error, Packet& out) {
    HeartbeatPacket p;
    f.timestamp("timestamp", p.timestamp);
    readEndpoint(f, "source", p.source, error);
    f.seq("seq", p.seq);
    if (!f.finish()) return false;
    out = std::move(p);
    return true;
}

bool decodeAssignment(Fields& f, std::string& error, Packet& out) {
    AssignmentPacket p;
    f.timestamp("timestamp", p.timestamp);
    readEndpoint(f, "source", p.source, error);
    if (f.ok() && p.source.role != Role::Coordinator) {
        return f.fail("source/role", "must be coordinator");
    }
    f.id("destination", p.destination);
    f.seq("seq", p.seq);

    if (const Json* task = f.get("task", true)) {
        Fields t(*task, "task", error);
        t.id("id", p.task.id);
        t.enumeration("action", kTaskActions, p.task.action);
        readLocation(t, "location", p.task.location, error);
        readTarget(t, "target", p.task.target, error);
        if (!t.finish()) return false;
    }
    f.timestamp("acceptBy", p.acceptBy);
    if (!f.finish()) return false;
    out = std::move(p);
    return true;
}

bool decodeAck(Fields& f, std::string& error, Packet& out) {
    AckPacket p;
    f.timestamp("timestamp", p.timestamp);
    readEndpoint(f, "source", p.source, error);
    if (f.ok() && p.source.role != Role::Node) return f.fail("source/role", "must be node");
    f.id("destination", p.destination);
    f.seq("seq", p.seq);
    f.id("taskId", p.taskId);
    f.enumeration("result", kAckResults, p.result);
    f.optionalEnumeration("reason", kRejectReasons, p.reason);
    if (!f.finish()) return false;
    if (p.result == AckResult::Rejected && !p.reason) {
        return f.fail("reason", "is required when result is rejected");
    }
    if (p.result == AckResult::Accepted && p.reason) {
        return f.fail("reason", "is only allowed when result is rejected");
    }
    out = std::move(p);
    return true;
}

}  // namespace

bool decode(const std::string& text, Packet& out, std::string* error) {
    std::string problem;
    Json document;
    bool ok = false;

    if (!Json::parse(text, document, &problem)) {
        problem = "not valid JSON: " + problem;
    } else {
        Fields f(document, "", problem);
        std::string version;
        if (f.string("version", version) && version != "OTF-1") {
            f.fail("version", "must be \"OTF-1\"");
        }
        // No "kind" means an event: that is every OTF-1 packet written
        // before the fleet kinds existed, and they must keep working.
        std::string kind = "event";
        f.string("kind", kind, false);

        if (f.ok()) {
            if (kind == "event") ok = decodeEvent(f, problem, out);
            else if (kind == "status") ok = decodeStatus(f, problem, out);
            else if (kind == "heartbeat") ok = decodeHeartbeat(f, problem, out);
            else if (kind == "assignment") ok = decodeAssignment(f, problem, out);
            else if (kind == "ack") ok = decodeAck(f, problem, out);
            else f.fail("kind", "is not a recognised value");
        }
    }

    if (!ok && error != nullptr) {
        *error = problem.empty() ? "invalid OTF-1 packet" : problem;
    }
    return ok;
}

}  // namespace otf1
