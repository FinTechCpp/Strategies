#include "common.h"
#include <iostream>

// Implementation of Time struct methods
bool Time::operator<(const Time& other) const {
    return std::tie(hour, minute, second) < std::tie(other.hour, other.minute, other.second);
}

bool Time::operator<=(const Time& other) const {
    return std::tie(hour, minute, second) <= std::tie(other.hour, other.minute, other.second);
}

bool Time::operator==(const Time& other) const {
    return hour == other.hour && minute == other.minute && second == other.second;
}

bool Time::operator!=(const Time& other) const {
    return !(*this == other);
}


// Implementation of DateTime struct methods
bool DateTime::is_valid() const {
    return year > 0 && month > 0 && month <= 12 && day > 0 && day <= 31;
}

bool DateTime::operator==(const DateTime& other) const {
    return year == other.year && month == other.month && day == other.day && time == other.time;
}

bool DateTime::operator!=(const DateTime& other) const {
    return !(*this == other);
}

bool DateTime::operator<(const DateTime &other) const
{
    return std::tie(year, month, day, time) < std::tie(other.year, other.month, other.day, other.time);
}

bool DateTime::operator<=(const DateTime &other) const
{
    return std::tie(year, month, day, time) <= std::tie(other.year, other.month, other.day, other.time);
}

bool DateTime::operator>(const DateTime &other) const
{
    return std::tie(year, month, day, time) > std::tie(other.year, other.month, other.day, other.time);
}

bool DateTime::operator>=(const DateTime &other) const
{
    return std::tie(year, month, day, time) >= std::tie(other.year, other.month, other.day, other.time);
}

std::string DateTime::to_string() const {
    static thread_local char buffer[20];
    int len = snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d",
            year, month, day, time.hour, time.minute, time.second);
    return std::string(buffer, len);
}

