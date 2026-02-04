#include <impl.h>
#include <hegel/internal.h>
#include <hegel/core.h>

namespace hegel::impl {

    [[noreturn]] void stop_test() { throw HegelReject(); }

}
