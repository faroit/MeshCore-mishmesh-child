#pragma once

namespace mishmesh {
namespace remote {

enum Command { Child, Parent, Pin, Timezone, Preset, Name, Retry, Gps, Sound, Alarm, Time, Help, Count };

struct CommandHelp {
  const char* name;
  const char* usage;
};

constexpr CommandHelp commands[] = {
  {"!child",  "!child on|off"},
  {"!parent", "!parent lock|unlock"},
  {"!pin",    "!pin <old PIN> <new PIN> (6 digits each)"},
  {"!tz",     "!tz <UTC offset in minutes> (e.g. +60)"},
  {"!preset", "!preset <1-10> [reply text] (omit text to remove)"},
  {"!name",   "!name <name to show on the pager>"},
  {"!retry",  "!retry on|off"},
  {"!gps",    "!gps on|off"},
  {"!sound",  "!sound low|mid|high|mute"},
  {"!alarm",  "!alarm HH:MM [days] or !alarm days <days>\n"
              "!alarm off or !alarm status\n"
              "Days: daily, weekdays, weekends, or mon,wed,fri."},
  {"!time",   "!time (read the pager clock)"},
  {"!help",   "!help [command] (e.g. !help alarm)"},
};

static_assert(sizeof(commands) / sizeof(commands[0]) == Count, "Every remote command needs help text");

}  // namespace remote
}  // namespace mishmesh
