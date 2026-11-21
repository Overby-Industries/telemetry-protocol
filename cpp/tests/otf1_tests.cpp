#include "check.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "json.hpp"
#include "otf1/otf1.hpp"

using namespace otf1;

namespace {

// The corpus is the repository's own: the same files tools/validate.py runs
// the JSON Schema over. Nothing is copied, so the two cannot drift.
std::vector<std::filesystem::path> jsonFilesIn(const char* subdirectory) {
    std::vector<std::filesystem::path> files;
    const std::filesystem::path dir = std::filesystem::path(OTF1_REPO_DIR) / subdirectory;
    if (std::filesystem::is_directory(dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".json") files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Order-insensitive structural equality: two documents that say the same
// thing with their members in a different order are the same packet.
bool sameDocument(const Json& a, const Json& b) {
    if (a.type() != b.type()) return false;
    switch (a.type()) {
        case Json::Type::Null: return true;
        case Json::Type::Bool: return a.asBool() == b.asBool();
        case Json::Type::Number: return a.asNumber() == b.asNumber();
        case Json::Type::String: return a.asString() == b.asString();
        case Json::Type::Array:
            if (a.items().size() != b.items().size()) return false;
            for (std::size_t i = 0; i < a.items().size(); ++i) {
                if (!sameDocument(a.items()[i], b.items()[i])) return false;
            }
            return true;
        case Json::Type::Object:
            if (a.members().size() != b.members().size()) return false;
            for (const Json::Member& member : a.members()) {
                const Json* other = b.find(member.first);
                if (other == nullptr || !sameDocument(member.second, *other)) return false;
            }
            return true;
    }
    return false;
}

bool refuses(const std::string& text, std::string* why = nullptr) {
    Packet packet;
    std::string error;
    const bool refused = !decode(text, packet, &error);
    if (why != nullptr) *why = error;
    return refused && !error.empty();
}

const char* kHeartbeat =
    R"({"version":"OTF-1","kind":"heartbeat","timestamp":"2025-02-18T14:05:25Z",)"
    R"("source":{"id":"fleet-coordinator","role":"coordinator"},"seq":96})";

// kHeartbeat with one substring swapped for another.
std::string heartbeatWith(const std::string& from, const std::string& to) {
    std::string text = kHeartbeat;
    const std::size_t at = text.find(from);
    if (at == std::string::npos) return "<<fixture substring not found: " + from + ">>";
    text.replace(at, from.size(), to);
    return text;
}

void test_canonical_examples_decode_losslessly() {
    TEST("every example in the spec decodes, and re-encodes to the same packet");

    const auto files = jsonFilesIn("examples");
    CHECK(files.size() >= 8);

    bool sawEvent = false, sawStatus = false, sawHeartbeat = false;
    bool sawAssignment = false, sawAck = false;

    for (const auto& path : files) {
        const std::string original = readFile(path);
        Packet packet;
        std::string error;
        const bool ok = decode(original, packet, &error);
        if (!ok) std::printf("        %s: %s\n", path.filename().string().c_str(), error.c_str());
        CHECK(ok);
        if (!ok) continue;

        sawEvent = sawEvent || std::holds_alternative<EventPacket>(packet);
        sawStatus = sawStatus || std::holds_alternative<StatusPacket>(packet);
        sawHeartbeat = sawHeartbeat || std::holds_alternative<HeartbeatPacket>(packet);
        sawAssignment = sawAssignment || std::holds_alternative<AssignmentPacket>(packet);
        sawAck = sawAck || std::holds_alternative<AckPacket>(packet);

        // Nothing may be dropped, renamed or altered on the way through.
        // The one allowed difference: an event written without "kind" comes
        // back with it stated.
        Json before, after;
        CHECK(Json::parse(original, before));
        CHECK(Json::parse(encode(packet), after));
        if (before.find("kind") == nullptr) before.set("kind", "event");
        const bool same = sameDocument(before, after);
        if (!same) std::printf("        %s changed in transit\n", path.filename().string().c_str());
        CHECK(same);
    }

    CHECK(sawEvent);
    CHECK(sawStatus);
    CHECK(sawHeartbeat);
    CHECK(sawAssignment);
    CHECK(sawAck);
}

void test_every_invalid_fixture_is_refused() {
    TEST("every packet the schema rejects, the decoder rejects too");

    const auto files = jsonFilesIn("tests/invalid");
    CHECK(files.size() >= 13);
    for (const auto& path : files) {
        std::string why;
        const bool refused = refuses(readFile(path), &why);
        if (!refused) std::printf("        %s was accepted\n", path.filename().string().c_str());
        CHECK(refused);
    }
}

void test_packets_without_a_kind_are_events() {
    TEST("a packet written before the fleet kinds existed is still an event");

    const std::string legacy =
        R"({"version":"OTF-1","timestamp":"2025-02-18T14:05:22Z",)"
        R"("mission":{"id":"orbital-demo-1","podId":"A1","type":"reclaimer"},)"
        R"("location":{"altitude_km":825,"inclination_deg":51.6,"zone":"LEO"},)"
        R"("target":{"catalogId":"NORAD-33953","objectType":"fragment","size_cm":45,"mass_kg":5.2},)"
        R"("action":"captured",)"
        R"("outcome":{"processedKg":5.0,"repurposedAs":"shield_block","status":"stored"},)"
        R"("certifications":{"SMESSC":true,"ODCI_contribution":5.0,)"
        R"("ProofOfCleanupToken":"0xabc123debriscredit"}})";

    Packet packet;
    CHECK(decode(legacy, packet));
    const EventPacket* event = std::get_if<EventPacket>(&packet);
    CHECK(event != nullptr);
    if (event == nullptr) return;

    CHECK_EQ(event->timestamp, TimeMs(1739887522000));
    CHECK_EQ(event->podId, std::string("A1"));
    CHECK(event->missionType == NodeType::Reclaimer);
    CHECK(event->location.zone == Zone::LEO);
    CHECK(event->target && event->target->massKg && *event->target->massKg == 5.2);
    CHECK(event->action == EventAction::Captured);
    CHECK(event->repurposedAs && *event->repurposedAs == RepurposedAs::ShieldBlock);
    CHECK(event->smessc);
    CHECK(event->odciContributionKg && *event->odciContributionKg == 5.0);
    CHECK(event->proofOfCleanupToken && *event->proofOfCleanupToken == "0xabc123debriscredit");
    CHECK(!event->taskId);
}

void test_each_kind_round_trips() {
    TEST("each fleet kind encodes and decodes field for field");

    StatusPacket status;
    status.timestamp = 1739887530000;
    status.source = {"A1", Role::Node};
    status.seq = 412;
    status.type = NodeType::Reclaimer;
    status.mode = Mode::Degraded;
    status.phase = "Capturing";
    status.link = LinkView::Connected;
    status.capabilities = {TaskAction::Capture, TaskAction::Deorbit};
    status.location.zone = Zone::LEO;
    status.location.altitudeKm = 825.0;
    status.task = TaskStatus{"T-0042", TaskState::Active, 0.4};

    Packet packet;
    CHECK(decode(encode(status), packet));
    if (const StatusPacket* s = std::get_if<StatusPacket>(&packet)) {
        CHECK_EQ(s->timestamp, status.timestamp);
        CHECK_EQ(s->source.id, std::string("A1"));
        CHECK_EQ(s->seq, std::uint64_t(412));
        CHECK(s->mode == Mode::Degraded);
        CHECK(s->phase && *s->phase == "Capturing");
        CHECK(s->link == LinkView::Connected);
        CHECK(s->capabilities.size() == 2 && s->capabilities[1] == TaskAction::Deorbit);
        CHECK(s->location.altitudeKm && *s->location.altitudeKm == 825.0);
        CHECK(!s->location.inclinationDeg);
        CHECK(s->task && s->task->id == "T-0042" && s->task->state == TaskState::Active);
        CHECK(s->task && s->task->progress && *s->task->progress == 0.4);
    } else {
        CHECK(false);
    }

    AssignmentPacket assignment;
    assignment.timestamp = 1739887500000;
    assignment.source = {"fleet-coordinator", Role::Coordinator};
    assignment.destination = "A1";
    assignment.seq = 95;
    assignment.task.id = "T-0042";
    assignment.task.action = TaskAction::Capture;
    assignment.task.location.zone = Zone::LEO;
    assignment.task.target = Target{ObjectType::Fragment, std::string("NORAD-33953"), 45.0, 5.2};
    assignment.acceptBy = 1739887510500;

    CHECK(decode(encode(assignment), packet));
    if (const AssignmentPacket* a = std::get_if<AssignmentPacket>(&packet)) {
        CHECK_EQ(a->destination, std::string("A1"));
        CHECK_EQ(a->task.id, std::string("T-0042"));
        CHECK(a->task.action == TaskAction::Capture);
        CHECK(a->task.target && a->task.target->catalogId && *a->task.target->catalogId == "NORAD-33953");
        CHECK_EQ(a->acceptBy, TimeMs(1739887510500));  // sub-second deadline survives
    } else {
        CHECK(false);
    }

    AckPacket ack;
    ack.timestamp = 1739887501000;
    ack.source = {"A1", Role::Node};
    ack.destination = "fleet-coordinator";
    ack.seq = 411;
    ack.taskId = "T-0042";
    ack.result = AckResult::Rejected;
    ack.reason = RejectReason::SafeMode;

    CHECK(decode(encode(ack), packet));
    if (const AckPacket* a = std::get_if<AckPacket>(&packet)) {
        CHECK(a->result == AckResult::Rejected);
        CHECK(a->reason && *a->reason == RejectReason::SafeMode);
        CHECK_EQ(a->taskId, std::string("T-0042"));
    } else {
        CHECK(false);
    }

    HeartbeatPacket heartbeat;
    heartbeat.timestamp = 1739887525000;
    heartbeat.source = {"fleet-coordinator", Role::Coordinator};
    heartbeat.seq = 96;
    CHECK_EQ(encode(heartbeat), std::string(kHeartbeat));
    CHECK(decode(kHeartbeat, packet));
    CHECK(std::holds_alternative<HeartbeatPacket>(packet));
}

void test_strictness_beyond_the_fixtures() {
    TEST("malformed packets are refused, with a reason that names the field");

    Packet packet;
    CHECK(decode(kHeartbeat, packet));  // the baseline the cases below break

    CHECK(refuses("not json at all"));
    CHECK(refuses("[]"));
    CHECK(refuses("{}"));
    CHECK(refuses(heartbeatWith("\"OTF-1\"", "\"OTF-2\"")));
    CHECK(refuses(heartbeatWith("\"kind\":\"heartbeat\"", "\"kind\":5")));
    CHECK(refuses(heartbeatWith("\"seq\":96", "\"seq\":96.5")));
    CHECK(refuses(heartbeatWith("\"seq\":96", "\"seq\":\"96\"")));
    CHECK(refuses(heartbeatWith("\"seq\":96", "\"seq\":1e300")));
    CHECK(refuses(heartbeatWith("2025-02-18T14:05:25Z", "last tuesday")));
    CHECK(refuses(heartbeatWith("\"id\":\"fleet-coordinator\"", "\"id\":\"\"")));
    CHECK(refuses(heartbeatWith("\"role\":\"coordinator\"", "\"role\":\"admiral\"")));
    CHECK(refuses(heartbeatWith("\"role\":\"coordinator\"", "\"role\":\"coordinator\",\"rank\":1")));
    CHECK(refuses(heartbeatWith(",\"seq\":96", "")));

    std::string why;
    StatusPacket status;
    status.source = {"A1", Role::Node};
    status.capabilities = {TaskAction::Capture};
    std::string text = encode(status);

    const std::size_t mode = text.find("\"nominal\"");
    CHECK(mode != std::string::npos);
    if (mode != std::string::npos) {
        std::string broken = text;
        broken.replace(mode, 9, "\"panicking\"");
        CHECK(refuses(broken, &why));
        CHECK(why.find("node/mode") != std::string::npos);
    }

    const std::size_t caps = text.find("[\"capture\"]");
    CHECK(caps != std::string::npos);
    if (caps != std::string::npos) {
        std::string broken = text;
        broken.replace(caps, 11, "[\"capture\",\"capture\"]");
        CHECK(refuses(broken, &why));
        CHECK(why.find("capabilities") != std::string::npos);
    }

    const std::size_t zone = text.find("\"zone\":\"LEO\"");
    CHECK(zone != std::string::npos);
    if (zone != std::string::npos) {
        std::string broken = text;
        broken.replace(zone, 12, "\"zone\":\"LEO\",\"sector\":7");
        CHECK(refuses(broken, &why));
        CHECK(why.find("location/sector") != std::string::npos);
    }
}

void test_task_and_event_actions_line_up() {
    TEST("each task action maps to the event action that reports it");

    CHECK(eventActionFor(TaskAction::Capture) == EventAction::Captured);
    CHECK(eventActionFor(TaskAction::Process) == EventAction::Processed);
    CHECK(eventActionFor(TaskAction::Refine) == EventAction::Refined);
    CHECK(eventActionFor(TaskAction::Store) == EventAction::Stored);
    CHECK(eventActionFor(TaskAction::Deorbit) == EventAction::Deorbited);
    CHECK(eventActionFor(TaskAction::Deploy) == EventAction::Deployed);
}

}  // namespace

void runOtf1Tests() {
    std::printf("otf1\n");
    test_canonical_examples_decode_losslessly();
    test_every_invalid_fixture_is_refused();
    test_packets_without_a_kind_are_events();
    test_each_kind_round_trips();
    test_strictness_beyond_the_fixtures();
    test_task_and_event_actions_line_up();
}
