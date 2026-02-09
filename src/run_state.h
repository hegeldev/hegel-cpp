#pragma once

namespace hegel::impl::run_state {
/**
 * @brief Check if this is the last test run.
 *
 * Useful in embedded mode to only print output once, after shrinking
 * is complete and the final failing case has been found.
 *
 * @return true if this is the last (possibly failing) run
 */
bool is_last_run();

void set_is_last_run(bool v);
} // namespace hegel::impl::run_state
