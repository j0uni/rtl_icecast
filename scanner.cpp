#include <vector>
#include <chrono>
#include "scanner.h"

#define SCANNER_DEFAULT_STEP_MS 100

Scanner::Scanner(std::vector<ScanList> scanlist) {
    ch_index = channels.size();

    copy(scanlist.begin(), scanlist.end(), back_inserter(channels));

    printf("[Scanner] scanlist size %ld %ld\n", channels.size(), scanlist.size());

    for (uint8_t i = 0; i < channels.size(); i++ ) {
        printf("[Scanner] Frq %f  name %s\n", channels[i].frequency, channels[i].ch_name.c_str());
    }
}

double Scanner::NextCh(bool sql)
{
    static auto last_time = std::chrono::steady_clock::now();
    double retval = 0.0f;

    if (sql == true) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count() >= SCANNER_DEFAULT_STEP_MS) {
            last_time = now;
            ch_index++;

            if (ch_index >= channels.size()) {
                ch_index = 0;
            }
        
            if (!channels.empty()) {
                retval = channels[ch_index].frequency;
            }
        } 
    }

    return retval;
}