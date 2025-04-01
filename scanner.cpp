#include <vector>
#include <chrono>
#include <rtl-sdr.h>
#include "scanner.h"

extern rtlsdr_dev_t *g_dev;

Scanner::Scanner(rtlsdr_dev_t *dev, std::vector<ScanList> scanlist) {
    //device = dev;
    ch_index = 8;

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
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count() >= 100) {
            last_time = now;
            ch_index++;

            if (ch_index >= channels.size()) {
                ch_index = 0;
            }
        
            retval = channels[ch_index].frequency;
        } 
    }

    return retval;
}