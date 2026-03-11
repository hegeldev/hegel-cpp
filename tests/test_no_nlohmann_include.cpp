// check that including public hegel headers does not transitively pull in
// nlohmann/json.

#include <hegel/hegel.h>

#ifdef INCLUDE_NLOHMANN_JSON_HPP_
#error "nlohmann/json.hpp was included by a public hegel header"
#endif

#ifdef INCLUDE_NLOHMANN_JSON_FWD_HPP_
#error "nlohmann/json_fwd.hpp was included by a public hegel header"
#endif

#include <gtest/gtest.h>

TEST(Pimpl, NlohmannNotLeaked) { SUCCEED(); }
