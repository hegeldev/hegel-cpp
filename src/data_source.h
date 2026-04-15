#pragma once

#include <hegel/json.h>

#include <cstdint>

namespace hegel::impl {

    class Connection;

    /**
     * @brief Abstract interface for data-stream communication.
     *
     * DataSource decouples generators from the concrete wire protocol so that
     * tests can inject mock responses without a live hegel-core subprocess.
     */
    class DataSource {
      public:
        virtual ~DataSource() = default;

        /// Send a generate request and return the raw response.
        virtual hegel::internal::json::json
        generate(const hegel::internal::json::json& schema) = 0;

        /// Send a start_span command.
        virtual void start_span(uint64_t label) = 0;

        /// Send a stop_span command.
        virtual void stop_span(bool discard) = 0;
    };

    /**
     * @brief DataSource backed by a Connection and data stream.
     *
     * This is the normal production implementation used during hegel() runs.
     */
    class ConnectionDataSource : public DataSource {
      public:
        ConnectionDataSource(Connection* conn, uint32_t data_stream);

        hegel::internal::json::json
        generate(const hegel::internal::json::json& schema) override;

        void start_span(uint64_t label) override;
        void stop_span(bool discard) override;

      private:
        Connection* conn_;
        uint32_t data_stream_;
    };

} // namespace hegel::impl
