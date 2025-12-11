/**
 * @file StateMachine.h
 * @brief 通用有限状态机模板
 * @author mozihe
 * 
 * 提供 DFA（确定性有限状态自动机）实现，用于词法分析器。
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

template <typename S, typename I> struct PairHash {
    std::size_t operator()(const std::pair<S, I> &p) const noexcept {
        std::size_t h1 = std::hash<S>{}(p.first);
        std::size_t h2 = std::hash<I>{}(p.second);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

template <typename State, typename Input,
          typename TransitionMap = std::unordered_map<
              std::pair<State, Input>, State, PairHash<State, Input>>>
class StateMachine {
  public:
    using BeforeCallback = std::function<void(State from, Input in, State to)>;
    using AfterCallback = std::function<void(State from, Input in, State to)>;

    StateMachine() = default;

    void setInitialState(State s) {
        initial_ = s;
        current_ = s;
        hasInitial_ = true;
    }

    void reset() {
        if (!hasInitial_) {
            throw std::logic_error("Initial state not set");
        }
        current_ = initial_;
    }

    void addFinalState(State s) { finals_.insert(s); }

    template <typename Iterable> void setFinalStates(const Iterable &st) {
        finals_.clear();
        for (auto &&s : st)
            finals_.insert(s);
    }

    void addTransition(State from, Input in, State to) {
        transitions_[std::make_pair(from, in)] = to;
    }

    void setBeforeTransition(BeforeCallback cb) { before_ = std::move(cb); }
    void setAfterTransition(AfterCallback cb) { after_ = std::move(cb); }

    bool step(Input in) {
        auto key = std::make_pair(current_, in);
        auto it = transitions_.find(key);
        if (it == transitions_.end())
            return false;
        State from = current_;
        State to = it->second;
        if (before_)
            before_(from, in, to);
        current_ = to;
        if (after_)
            after_(from, in, to);
        return true;
    }

    State current() const { return current_; }

    bool isAccepting() const { return finals_.find(current_) != finals_.end(); }

  private:
    State current_{};
    State initial_{};
    bool hasInitial_{false};
    std::unordered_set<State> finals_;
    TransitionMap transitions_;
    BeforeCallback before_;
    AfterCallback after_;
};

#endif // STATE_MACHINE_H
