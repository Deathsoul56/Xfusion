#include "RewindBuffer.h"

#include <cstdint>

namespace {

constexpr int kMinZeroRun = 4; // shorter gaps are cheaper to keep inside the literal

void appendVarint(QByteArray& out, std::size_t value)
{
    while (value >= 0x80) {
        out.append(static_cast<char>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out.append(static_cast<char>(value));
}

bool readVarint(const QByteArray& in, int* pos, std::size_t* value)
{
    *value = 0;
    for (int shift = 0; *pos < in.size() && shift < 63; shift += 7) {
        const auto byte = static_cast<std::uint8_t>(in[(*pos)++]);
        *value |= static_cast<std::size_t>(byte & 0x7F) << shift;
        if (!(byte & 0x80))
            return true;
    }
    return false;
}

}

// Layout: repeated [zero run length][literal length][literal bytes], where the literal holds the XOR of the two
// states and the zero run is the part where they are equal.
QByteArray RewindBuffer::encodeDifference(const QByteArray& a, const QByteArray& b)
{
    QByteArray out;
    const auto* pa = reinterpret_cast<const std::uint8_t*>(a.constData());
    const auto* pb = reinterpret_cast<const std::uint8_t*>(b.constData());
    const int size = static_cast<int>(a.size());
    int i = 0;
    while (i < size) {
        const int zeroStart = i;
        while (i < size && pa[i] == pb[i])
            ++i;
        if (i == size)
            break; // trailing equal bytes need no entry: the decoder stops with the data
        const int literalStart = i;
        // The literal runs until a stretch of kMinZeroRun equal bytes begins; shorter gaps stay inside it.
        int lastDifferent = i;
        int equal = 0;
        for (int j = i + 1; j < size; ++j) {
            if (pa[j] != pb[j]) {
                lastDifferent = j;
                equal = 0;
            } else if (++equal >= kMinZeroRun) {
                break;
            }
        }
        const int literalEnd = lastDifferent + 1;
        appendVarint(out, static_cast<std::size_t>(literalStart - zeroStart));
        appendVarint(out, static_cast<std::size_t>(literalEnd - literalStart));
        for (int k = literalStart; k < literalEnd; ++k)
            out.append(static_cast<char>(pa[k] ^ pb[k]));
        i = literalEnd;
    }
    return out;
}

QByteArray RewindBuffer::applyDifference(const QByteArray& state, const QByteArray& difference)
{
    QByteArray out = state;
    auto* data = reinterpret_cast<std::uint8_t*>(out.data());
    int pos = 0;
    int at = 0;
    while (pos < difference.size()) {
        std::size_t zeros = 0;
        std::size_t literal = 0;
        if (!readVarint(difference, &pos, &zeros) || !readVarint(difference, &pos, &literal))
            return {};
        at += static_cast<int>(zeros);
        if (static_cast<std::size_t>(difference.size() - pos) < literal || at + static_cast<int>(literal) > out.size())
            return {};
        for (std::size_t k = 0; k < literal; ++k)
            data[at++] ^= static_cast<std::uint8_t>(difference[pos++]);
    }
    return out;
}

void RewindBuffer::reset(std::size_t budgetBytes)
{
    m_budget = budgetBytes;
    m_used = 0;
    m_current.clear();
    m_deltas.clear();
}

void RewindBuffer::push(const QByteArray& state)
{
    if (!enabled())
        return;
    if (m_current.isEmpty() || m_current.size() != state.size()) {
        m_deltas.clear();
        m_used = 0;
        m_current = state;
        return;
    }
    QByteArray delta = encodeDifference(state, m_current);
    m_used += static_cast<std::size_t>(delta.size());
    m_deltas.push_back(std::move(delta));
    m_current = state;
    while (!m_deltas.empty() && bytesUsed() > m_budget) {
        m_used -= static_cast<std::size_t>(m_deltas.front().size());
        m_deltas.pop_front();
    }
}

bool RewindBuffer::pop(QByteArray* state)
{
    if (m_deltas.empty())
        return false;
    const QByteArray older = applyDifference(m_current, m_deltas.back());
    if (older.size() != m_current.size()) {
        reset(m_budget); // damaged difference: nothing safe to go back to
        return false;
    }
    m_used -= static_cast<std::size_t>(m_deltas.back().size());
    m_deltas.pop_back();
    m_current = older;
    *state = older;
    return true;
}
