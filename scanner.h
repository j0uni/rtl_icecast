#ifndef _SCANNER_H
#define _SCANNER_H

#include <string>
#include <vector>
#include <rtl-sdr.h>

typedef struct {
    double frequency;
    std::string modulation_mode;
    std::string ch_name;
} ScanList;

class Scanner {
    private:
        std::vector<ScanList> channels;
        uint8_t ch_index;

    public:
        Scanner(rtlsdr_dev_t *dev, std::vector<ScanList> scanlist);
        double NextCh(bool frq);
};

#endif // _SCANNER_H
