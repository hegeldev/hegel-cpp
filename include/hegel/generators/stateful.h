#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "hegel/core.h"

/**
 * @brief Stateful (model-based) property testing.
 *
 * A stateful test exercises a system through a sequence of randomly chosen
 * actions ("rules") applied to a state machine. Derive a machine from
 * @ref StateMachine and define its @c rules(). Each @ref Rule is a name and a
 * step function that performs one application of the rule, drawing any
 * arguments it needs from the test case and mutating the machine. Invariants
 * are predicates on the machine evaluated before any step is run and after
 * every successful step. An invariant must throw when violated.
 *
 * To run a state machine, pass an instance to @ref run inside a
 * @ref hegel::test. Examples in this documentation assume the alias
 * `namespace gs = hegel::generators;`.
 *
 * Example: an integer stack.
 *
 * @code{.cpp}
    struct IntegerStack
        : hegel::stateful::StateMachine<IntegerStack, std::vector<int>> {
        IntegerStack() : StateMachine({.initial_state = {}}) {}

        std::vector<hegel::stateful::Rule<IntegerStack>> rules() {
            return {
                hegel::stateful::Rule<IntegerStack>(
                    "push", [](hegel::TestCase& tc, IntegerStack& m) {
                        m.state.push_back(tc.draw(gs::integers<int>(
                            {.min_value = 0, .max_value = 100})));
                    }),
                hegel::stateful::Rule<IntegerStack>(
                    "pop", [](hegel::TestCase& tc, IntegerStack& m) {
                        tc.assume(!m.state.empty());
                        m.state.pop_back();
                    }),
            };
        }
    };

    HEGEL_TEST(stack_operations)(hegel::TestCase& tc) {
        IntegerStack machine;
        hegel::stateful::run(machine, tc);
    }
 * @endcode
 */
namespace hegel::stateful {

    using hegel::generators::Generator;
    using hegel::generators::IGenerator;

    template <typename T> class VariablesGenerator;

    /**
     * @brief A pool of previously generated values.  A pool lets data flow from
     * one rule to another, so a rule can act on a handle or identifier that an
     * earlier rule produced rather than on a freshly drawn value.
     *
     * Create one with its constructor and populate it with @ref add. To draw
     * from the pool, use the generators @ref values_reusable (returns a value
     * without removing it) and @ref values_consumed (removes and returns a
     * value).
     *
     * A @ref Pool is neither copyable nor movable. A state that embeds a pool
     * is passed to @ref run by reference and never copied.
     *
     * Example: a resource allocator. The @c alloc rule creates a fresh resource
     * and deposits it in the pool. The @c free rule draws one of those resource
     * back out and releases it.
     *
     * @code{.cpp}
        struct Allocator
            : hegel::stateful::StateMachine<Allocator, std::set<int>> {
            // The pool is tied to a test case and cannot be copied, so it is
            // a member of the machine rather than part of the state.
            hegel::stateful::Pool<int> handles;
            int next_handle = 0;

            explicit Allocator(hegel::TestCase& tc)
                : StateMachine({.initial_state = {}}), handles(tc) {}

            std::vector<hegel::stateful::Rule<Allocator>> rules() {
                return {
                    hegel::stateful::Rule<Allocator>(
                        "alloc", [](hegel::TestCase&, Allocator& m) {
                            int h = m.next_handle++;
                            m.handles.add(h);
                            m.state.insert(h);
                        }),
                    hegel::stateful::Rule<Allocator>(
                        "free", [](hegel::TestCase& tc, Allocator& m) {
                            // draws a handle a prior alloc put in the pool
                            auto h = tc.draw(
                                "h",
                                hegel::stateful::values_consumed(m.handles));
                            m.state.erase(h);
                        }),
                };
            }
        };

        HEGEL_TEST(allocator_state_machine)(hegel::TestCase& tc) {
            Allocator machine(tc);
            hegel::stateful::run(machine, tc);
        }
     * @endcode
     *
     * @tparam T The type of variables in the pool.
     */
    template <typename T> class Pool {
      public:
        /**
         * @brief Creates an empty pool. Pools are tied to a test case. Do not
         * reuse one across test cases.
         *
         * @param tc The test case tied to the pool
         */
        explicit Pool(const TestCase& tc) : tc_(tc), pool_handle_(tc) {}

        /**
         * @brief Adds @p element to the pool. Overload for copying lvalues.
         *
         * @param element The element to add to the pool
         */
        void add(const T& element) {
            int64_t var_id = pool_handle_.add(tc_);
            // GCOVR_EXCL_START
            if (pool_.find(var_id) != pool_.end()) {
                throw std::runtime_error("unexpected variable id in map");
            }
            // GCOVR_EXCL_STOP
            pool_.emplace(var_id, element);
        }

        /**
         * @brief Adds @p element to the pool. Overload for moving rvalues.
         *
         * @param element The element to add to the pool
         */
        void add(T&& element) {
            int64_t var_id = pool_handle_.add(tc_);
            // GCOVR_EXCL_START
            if (pool_.find(var_id) != pool_.end()) {
                throw std::runtime_error("unexpected variable id in map");
            }
            // GCOVR_EXCL_STOP
            pool_.emplace(var_id, std::move(element));
        }

        /**
         * @brief Returns the number of variables in the pool.
         *
         * @return The number of variables in the pool
         */
        std::size_t size() const { return pool_.size(); }

        Pool(const Pool&) = delete;

      private:
        const TestCase& tc_;
        hegel::internal::PoolHandle pool_handle_;
        std::map<int64_t, T> pool_;

        friend class VariablesGenerator<T>;
    };

    /// @cond INTERNAL
    // Concrete IGenerator for variables.
    template <typename T> class VariablesGenerator : public IGenerator<T> {
      public:
        explicit VariablesGenerator(Pool<T>& p, bool consume)
            : p_(p), consume_(consume) {}

        T do_draw(const TestCase& tc) const override {
            int64_t variable = p_.pool_handle_.draw_variable(tc, consume_);

            auto it = p_.pool_.find(variable);
            // GCOVR_EXCL_START
            if (it == p_.pool_.end()) {
                throw std::runtime_error(
                    "Pool state diverged between the engine and the "
                    "client, or a bug in the pool bookkeeping.");
            }
            // GCOVR_EXCL_STOP
            if (consume_) {
                auto val = std::move(it->second);
                p_.pool_.erase(it);
                return val;
            }
            return it->second;
        }

      private:
        Pool<T>& p_;
        bool consume_;
    };
    /// @endcond

    /// @name Variable pools
    /// @{

    /**
     * @brief Returns a value from the pool and removes it.
     *
     * @code{.cpp}
        HEGEL_TEST(pool_round_trip)(hegel::TestCase& tc) {
            hegel::stateful::Pool<int> pool(tc);
            auto original_set = tc.draw(
                "original_set", gs::sets(gs::integers<int>(), {.max_size =
     10}));

            for (int num : original_set) {
                pool.add(num);
            }

            std::set<int> returned_set;
            for (int i = 0; i < original_set.size(); i++) {
                returned_set.insert(
                    tc.draw(hegel::stateful::values_consumed(pool)));
            }

            assert(original_set == returned_set);
        }
     * @endcode
     *
     * @tparam T Element type
     * @param p The pool to draw from
     * @return An element of the pool
     */
    template <typename T> Generator<T> values_consumed(Pool<T>& p) {
        return Generator<T>(new VariablesGenerator<T>(p, true));
    }

    /**
     * @brief Returns a value from the pool without removing it.
     *
     * @code{.cpp}
        HEGEL_TEST(pool_reuse)(hegel::TestCase& tc) {
            hegel::stateful::Pool<int> pool(tc);
            auto sz = tc.draw("sz", gs::integers<uint8_t>());
            auto original_set = tc.draw(
                "original_set", gs::sets(gs::integers<int>(), {.max_size =
     sz}));

            for (int num : original_set) {
                pool.add(num);
            }

            for (int i = 0; i < original_set.size(); i++) {
                tc.draw(hegel::stateful::values_reusable(pool));
            }

            assert(pool.size() == original_set.size());
        }
     * @endcode
     *
     * @tparam T Element type
     * @param p The pool to draw from
     * @return An element of the pool
     */
    template <typename T> Generator<T> values_reusable(Pool<T>& p) {
        return Generator<T>(new VariablesGenerator<T>(p, false));
    }

    /**
     * @brief A rule is one possible action in a stateful test.
     *
     * @code{.cpp}
        auto push = hegel::stateful::Rule<std::vector<int>>(
            "push", [](hegel::TestCase& tc, std::vector<int>& stack) {
                stack.push_back(tc.draw(gs::integers<int>()));
            });
     * @endcode
     *
     * @tparam T The state-machine type the rule acts on
     */
    template <typename T> class Rule {
      public:
        /**
         * @brief Declares a new Rule
         *
         * @param name name of the rule
         * @param step function representing the step a rule takes. It mutates
         * the state in place, drawing any arguments it needs from the test
         * case. Always check preconditions with @c assume() before mutating. A
         * rule that rejects after mutating leaves the state partially modified.
         */
        explicit Rule(std::string name, std::function<void(TestCase&, T&)> step)
            : name_(std::move(name)), step_(std::move(step)) {}

        /**
         * @brief Returns the name of the rule.
         *
         * @return const std::string&
         */
        const std::string& name() const { return name_; }
        /**
         * @brief Returns the underlying step function of the rule.
         *
         * @return const std::function<void(TestCase&, T&)>&
         */
        const std::function<void(TestCase&, T&)>& step() const { return step_; }

      private:
        std::string name_;
        std::function<void(TestCase&, T&)> step_;
    };

    /**
     * @brief An invariant is a predicate that must hold at any point in the
     * stateful test. They are evaluated on the initial state and after every
     * valid step. The invariant function should throw when the invariant is
     * violated.
     *
     * @tparam T The state-machine type the invariant checks
     */
    template <typename T> class Invariant {
      public:
        /**
         * @brief Declare a new invariant.
         *
         * @param name
         * @param invariant
         */
        explicit Invariant(std::string name,
                           std::function<void(const T&)> invariant)
            : name_(std::move(name)), invariant_(std::move(invariant)) {}

        /**
         * @brief Returns the name of the invariant.
         *
         * @return const std::string&
         */
        const std::string& name() const { return name_; }
        /**
         * @brief Returns the function representing the predicate of the
         * invariant.
         *
         * @return const std::function<void(const T&)>&
         */
        const std::function<void(const T&)>& invariant() const {
            return invariant_;
        }

      private:
        std::string name_;
        std::function<void(const T&)> invariant_;
    };

    /**
     * @brief Arguments for the @ref StateMachine constructor.
     *
     * @tparam State The type of the state the rules act on
     */
    template <typename State> struct StateMachineParams {
        /// The state the first rule acts on.
        State initial_state;
    };

    /**
     * @brief Base class for a state machine. Holds the state and declares the
     * rules and the invariants that act on it.
     *
     * Derive from it with the deriving type as @p Derived and the state's type
     * as @p State, pass the initial state to this constructor, and define
     *
     * @code{.cpp}
        std::vector<Rule<Derived>> rules();
     * @endcode
     *
     * returning the actions the test may apply. Optionally override
     * @ref invariants to add predicates checked before the first step and
     * after every valid step. Rules mutate @ref state in place. Pass an
     * instance to @ref run.
     *
     * @code{.cpp}
        struct Counter : hegel::stateful::StateMachine<Counter, int> {
            Counter() : StateMachine({.initial_state = 0}) {}

            std::vector<hegel::stateful::Rule<Counter>> rules() {
                return {hegel::stateful::Rule<Counter>(
                    "inc", [](hegel::TestCase&, Counter& m) { m.state++; })};
            }
        };
     * @endcode
     *
     * A machine may hold members besides the state, for anything the state's
     * type cannot carry. A @ref Pool, for instance, is tied to a test case and
     * is neither copyable nor movable.
     *
     * @tparam Derived The deriving state-machine type
     * @tparam State The type of the state the rules act on
     */
    template <typename Derived, typename State> class StateMachine {
      public:
        /// The type of the state the rules act on.
        using state_type = State;
        /**
         * @brief Builds a machine holding the given initial state.
         *
         * @code{.cpp}
         * Counter() : StateMachine({.initial_state = 0}) {}
         * @endcode
         *
         * @param params The initial state. See StateMachineParams.
         */
        explicit StateMachine(StateMachineParams<State> params)
            : state(std::move(params.initial_state)) {}

        /// The state the rules act on. Rules mutate it in place, and a
        /// failing run prints it. See RunParams::print_state.
        State state;

        /**
         * @brief Invariants checked before the first step and after every valid
         * step. Override to add them. Defaults to none.
         *
         * @return const std::vector<Invariant<Derived>>&
         */
        std::vector<Invariant<Derived>> invariants() { return {}; }

      protected:
        ~StateMachine() = default;
    };

    /**
     * @brief Options for @ref run.
     */
    struct RunParams {
        /// If true (the default), a failing sequence prints the machine's
        /// state before its first step and after every step that runs to
        /// completion.
        ///
        /// @code{.txt}
        /// state = 0
        /// Step 1: add
        ///   auto amount = 3;
        /// state = 3
        /// Step 2: add
        ///   auto amount = 9;
        /// @endcode
        ///
        /// A step that throws prints no state, so the last state shown is
        /// the one the failing step started from.
        bool print_state = true;
    };

    /// @cond INTERNAL
    // prints the machine's state.
    template <typename M>
    void print_state(TestCase& tc, const M& machine, const RunParams& params) {
        if (params.print_state) {
            tc.note("state = " + internal::repr(machine.state));
        }
    }

    // check if invariants hold on a given state
    template <typename T>
    void check_invariants(TestCase& tc, const std::string& origin,
                          const T& state,
                          const std::vector<Invariant<T>>& invariants) {
        for (const auto& inv : invariants) {
            try {
                (inv.invariant())(state);
            } catch (...) {
                tc.note("Invariant " + inv.name() + " violated " + origin);
                throw;
            }
        }
    }
    /// @endcond

    /**
     * @brief Executes a stateful test by repeatedly applying randomly chosen
     * rules from @p machine to it and checking @p machine's invariants before
     * the first step and after every valid step. Rules mutate @p machine in
     * place. Raises @p std::invalid_argument if the machine declares no rules.
     *
     * On a failing replay, each applied rule prints as @c "Step N: <name>". A
     * violated invariant prints @c "Invariant <name> violated after step M" or
     * @c "Invariant <name> violated in the initial state".
     *
     * @code{.txt}
        Step 1: add
        Step 2: add
        Invariant nonneg violated after step 2
     * @endcode
     *
     * @tparam M The state-machine type, deriving from @ref StateMachine
     * @param machine The state machine, initialized by the caller and mutated
     * by its rules
     * @param tc The test case object
     * @param params Options for the run. See RunParams.
     */
    template <typename M>
    void run(M& machine, TestCase& tc, const RunParams& params = {}) {
        static_assert(
            std::is_base_of<StateMachine<M, typename M::state_type>, M>::value,
            "run() requires a machine deriving from "
            "StateMachine<M, State>.");
        std::vector<Rule<M>> rules = machine.rules();
        std::vector<Invariant<M>> invariants = machine.invariants();
        if (rules.empty()) {
            throw std::invalid_argument(
                "Cannot run a state machine with no rules.");
        }
        std::vector<std::string> rule_names;
        rule_names.reserve(rules.size());
        for (const Rule<M>& rule : rules)
            rule_names.push_back(rule.name());

        std::vector<std::string> invariant_names;
        invariant_names.reserve(invariants.size());
        for (const Invariant<M>& invariant : invariants)
            invariant_names.push_back(invariant.name());

        print_state(tc, machine, params);
        check_invariants(tc, "in the initial state", machine, invariants);

        internal::StateMachineHandle machine_handle(tc, rule_names,
                                                    invariant_names);
        int64_t steps_run = 0;

        // The engine drives execution in rounds. At every join point — before
        // the first rule and after each round's rule stream is exhausted — the
        // root handle asks for the next group; the round then pulls rules until
        // the engine signals the join point. A sequential machine has a single
        // group and one worker.
        while (true) {
            int64_t group_id = machine_handle.next_group(tc);
            if (group_id == internal::state_machine_done) {
                break;
            }
            while (true) {
                int64_t next_rule_idx = machine_handle.next_rule(tc);
                if (next_rule_idx == internal::state_machine_done) {
                    break;
                }
                // GCOVR_EXCL_START
                if (next_rule_idx < 0 ||
                    static_cast<size_t>(next_rule_idx) >= rules.size()) {
                    throw std::runtime_error(
                        "state_machine_next_rule returned out-of-range "
                        "rule index. Please report this as a bug.");
                }
                // GCOVR_EXCL_STOP
                steps_run++;
                const Rule<M>& rule = rules[static_cast<size_t>(next_rule_idx)];
                tc.note("Step " + std::to_string(steps_run) + ": " +
                        rule.name());

                internal::start_span(tc, internal::SpanLabel::StatefulRule);
                try {
                    // nest the draws the step makes under its "Step N" header.
                    {
                        internal::NoteIndentScope indent(tc);
                        rule.step()(tc, machine);
                    }
                    print_state(tc, machine, params);
                    check_invariants(tc,
                                     "after step " + std::to_string(steps_run),
                                     machine, invariants);
                    internal::stop_span(tc);
                } catch (const internal::HegelReject&) {
                    tc.note("Rule stopped early due to violated assumption.");
                    machine_handle.rule_rejected(tc);
                    internal::stop_span(tc, true);
                } catch (...) {
                    internal::stop_span(tc);
                    throw;
                }
            }
        }
    }
    /// @}

} // namespace hegel::stateful
