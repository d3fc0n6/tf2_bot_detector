#include "Clock.h"

#include <mh/chrono/chrono_helpers.hpp>

using namespace tf2_bot_detector;

tm tf2_bot_detector::ToTM(const time_point_t& ts)
{
	return mh::chrono::to_tm(ts, mh::chrono::time_zone::local);
}

tm tf2_bot_detector::GetLocalTM()
{
	return mh::chrono::current_tm(mh::chrono::time_zone::local);
}

time_point_t tf2_bot_detector::GetLocalTimePoint()
{
	return mh::chrono::current_time_point();
}
