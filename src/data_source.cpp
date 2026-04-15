#include <data_source.h>

#include <connection.h>
#include <hegel/json.h>

#include "json_impl.h"

using hegel::internal::json::ImplUtil;

namespace hegel::impl {

    ConnectionDataSource::ConnectionDataSource(Connection* conn,
                                               uint32_t data_stream)
        : conn_(conn), data_stream_(data_stream) {}

    hegel::internal::json::json
    ConnectionDataSource::generate(const hegel::internal::json::json& schema) {
        hegel::internal::json::json request = {{"command", "generate"},
                                               {"schema", schema}};
        return conn_->request(data_stream_, request);
    }

    void ConnectionDataSource::start_span(uint64_t label) {
        hegel::internal::json::json request = {{"command", "start_span"},
                                               {"label", label}};
        conn_->request(data_stream_, request);
    }

    void ConnectionDataSource::stop_span(bool discard) {
        hegel::internal::json::json request = {{"command", "stop_span"},
                                               {"discard", discard}};
        conn_->request(data_stream_, request);
    }

} // namespace hegel::impl
