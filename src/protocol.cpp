#include <protocol.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <hegel/settings.h>
#include <string>

namespace hegel::impl::protocol {
    static thread_local bool protocol_debug_ = false;

    void set_protocol_debug(bool enabled) { protocol_debug_ = enabled; }
    bool protocol_debug_enabled() { return protocol_debug_; }

    static bool is_protocol_debug_env() {
        const char* val = std::getenv("HEGEL_PROTOCOL_DEBUG");
        if (!val)
            return false;
        std::string v(val);
        std::transform(v.begin(), v.end(), v.begin(), ::tolower);
        return v == "1" || v == "true";
    }

    void init_protocol_debug(Verbosity verbosity) {
        set_protocol_debug(verbosity == Verbosity::Debug ||
                           is_protocol_debug_env());
    }

} // namespace hegel::impl::protocol
