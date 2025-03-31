#include <vector>
#include <rtl-sdr.h>
#include "scanner.h"

extern rtlsdr_dev_t *g_dev;

Scanner::Scanner(rtlsdr_dev_t *dev, std::vector<ScanList> scanlist) {
    //device = dev;
    ch_index = 8;

    copy(scanlist.begin(), scanlist.end(), back_inserter(channels));

    printf("[Scanner] scanlist size %ld %ld\n", channels.size(), scanlist.size());

    for (uint8_t i = 0; i < channels.size(); i++ ) {
        printf("[Scanner] Frq %d  name %s\n", channels[i].frequency, channels[i].ch_name.c_str());
    }
}

void Scanner::NextCh(void)
{
    ch_index++;

    if (ch_index >= channels.size()) {
        ch_index = 0;
    }

    uint32_t freq = channels[ch_index].frequency;

    rtlsdr_set_center_freq(g_dev, freq);
    //printf("ret = %d %d\n", ret, g_dev);
}