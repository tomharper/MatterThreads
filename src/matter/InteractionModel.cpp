#include "matter/InteractionModel.h"
#include "core/Log.h"
#include <string>

namespace mt {

Result<ReportData> InteractionModel::sendRead(SessionId session_id,
                                                const std::vector<AttributePath>& paths) {
    auto* session = sessions_.findSession(session_id);
    if (!session) return Error("Session not found");

    sessions_.updateActivity(session_id, SteadyClock::now());

    ReportData report;
    for (const auto& path : paths) {
        AttributeReport ar;
        ar.path = path;
        auto result = data_model_.readAttribute(path);
        if (result) {
            ar.value = *result;
        } else {
            ar.has_error = true;
            ar.status_code = -1;
        }
        report.attribute_reports.push_back(std::move(ar));
    }

    MT_DEBUG("im", "Read " + std::to_string(paths.size()) + " attributes on session " +
             std::to_string(session_id));

    return report;
}

Result<SubscriptionId> InteractionModel::sendSubscribe(SessionId session_id,
                                                         const std::vector<AttributePath>& paths,
                                                         Duration min_interval,
                                                         Duration max_interval) {
    auto* session = sessions_.findSession(session_id);
    if (!session) return Error("Session not found");

    sessions_.updateActivity(session_id, SteadyClock::now());

    auto sub_id = subscriptions_.createSubscription(
        session_id, session->peer_node_id, paths, min_interval, max_interval);

    MT_DEBUG("im", "Subscribe " + std::to_string(paths.size()) + " paths on session " +
             std::to_string(session_id) + " -> sub " + std::to_string(sub_id));

    return sub_id;
}

Result<void> InteractionModel::sendWrite(SessionId session_id, const WriteRequestData& req) {
    auto* session = sessions_.findSession(session_id);
    if (!session) return Error("Session not found");

    sessions_.updateActivity(session_id, SteadyClock::now());

    for (const auto& [path, value] : req.writes) {
        auto result = data_model_.writeAttribute(path, value);
        if (!result) {
            MT_WARN("im", "Write failed for attribute");
            return result;
        }
    }

    MT_DEBUG("im", "Wrote " + std::to_string(req.writes.size()) + " attributes on session " +
             std::to_string(session_id));

    return Result<void>::success();
}

Result<InvokeResponseData> InteractionModel::sendInvoke(SessionId session_id,
                                                          const InvokeRequestData& req) {
    auto* session = sessions_.findSession(session_id);
    if (!session) return Error("Session not found");

    sessions_.updateActivity(session_id, SteadyClock::now());

    InvokeResponseData resp;
    resp.command = req.command;
    resp.status_code = 0; // Success

    MT_DEBUG("im", "Invoked command " + std::to_string(req.command.command_id) +
             " on endpoint " + std::to_string(req.command.endpoint_id) +
             " cluster " + std::to_string(req.command.cluster_id));

    return resp;
}

ReportData InteractionModel::handleReadRequest(SessionId session_id,
                                                 const std::vector<AttributePath>& paths) {
    ReportData report;
    for (const auto& path : paths) {
        AttributeReport ar;
        ar.path = path;
        auto result = data_model_.readAttribute(path);
        if (result) {
            ar.value = *result;
        } else {
            ar.has_error = true;
            ar.status_code = -1;
        }
        report.attribute_reports.push_back(std::move(ar));
    }
    return report;
}

void InteractionModel::handleWriteRequest(SessionId session_id, const WriteRequestData& req) {
    for (const auto& [path, value] : req.writes) {
        data_model_.writeAttribute(path, value);
    }
}

void InteractionModel::tick(TimePoint now) {
    exchanges_.tick(now);
    subscriptions_.tick(now);
    sessions_.expireIdleSessions(now);
}

} // namespace mt
