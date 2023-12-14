#include <Storages/MaterializedView/RefreshSchedule.h>

#include <Common/thread_local_rng.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int BAD_ARGUMENTS;
}

RefreshSchedule::RefreshSchedule(const ASTRefreshStrategy & strategy)
{
    kind = strategy.schedule_kind;
    period = strategy.period->interval;
    if (strategy.offset)
        offset = strategy.offset->interval;
    if (strategy.spread)
        spread = strategy.spread->interval;
}

static std::chrono::sys_seconds advanceEvery(std::chrono::system_clock::time_point prev, CalendarTimeInterval period, CalendarTimeInterval offset)
{
    auto period_start = period.floor(prev);
    auto t = offset.advance(period_start);
    if (t > prev)
        return t;
    t = offset.advance(period.advance(period_start));
    chassert(t > prev);
    return t;
}

std::chrono::sys_seconds RefreshSchedule::prescribeNext(
    std::chrono::system_clock::time_point last_prescribed, std::chrono::system_clock::time_point now) const
{
    if (kind == RefreshScheduleKind::AFTER)
        return period.advance(now);

    /// It's important to use prescribed instead of actual time here, otherwise we would do multiple
    /// refreshes instead of one if the generated spread is negative and the the refresh completes
    /// faster than the spread.
    auto res = advanceEvery(last_prescribed, period, offset);
    if (res < now)
        res = advanceEvery(now, period, offset); // fell behind by a whole period, skip to current time

    return res;
}

std::chrono::system_clock::time_point RefreshSchedule::addRandomSpread(std::chrono::sys_seconds prescribed_time) const
{
    Int64 ms = Int64(spread.minSeconds() * 1000 / 2);
    auto add = std::uniform_int_distribution(-ms, ms)(thread_local_rng);
    return prescribed_time + std::chrono::milliseconds(add);
}

}
