#pragma once

#include <QByteArray>
#include <deque>

// A history of emulator states for rewinding, kept as XOR differences between neighbouring snapshots: the latest
// state is stored whole and each older one is the latest minus a small, run-length coded difference. The oldest
// differences are dropped when the memory budget is full, so the history is a sliding window.
class RewindBuffer {
public:
    // Forgets everything. A budget of 0 turns rewinding off.
    void reset(std::size_t budgetBytes);
    bool enabled() const { return m_budget > 0; }

    // Adds the latest state. A state of a different size than the previous one (another game) restarts the history.
    void push(const QByteArray& state);
    // Steps back one snapshot and returns it; false when there is nothing older.
    bool pop(QByteArray* state);

    int depth() const { return static_cast<int>(m_deltas.size()); }
    std::size_t bytesUsed() const { return m_used + static_cast<std::size_t>(m_current.size()); }

    // Exposed for tests: the difference of two states of the same size, and applying it back.
    static QByteArray encodeDifference(const QByteArray& a, const QByteArray& b);
    static QByteArray applyDifference(const QByteArray& state, const QByteArray& difference);

private:
    std::size_t m_budget = 0;
    std::size_t m_used = 0; // bytes held by the differences
    QByteArray m_current;
    std::deque<QByteArray> m_deltas;
};
