#include <cstdlib>
#include <algorithm>
#include <future>
#include <switch.h>
#include <switch_json.h>
#include <grpc++/grpc++.h>
#include "mod_call_bot.h"
#include "simple_buffer.h"
#define CHUNKSIZE (320)

namespace
{
    int case_insensitive_match(std::string s1, std::string s2)
    {
        std::transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
        std::transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
        if (s1.compare(s2) == 0)
            return 1; // The strings are same
        return 0;     // not matched
    }
}