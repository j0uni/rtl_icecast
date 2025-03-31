#ifndef _SCANNER_H
#define _SCANNER_H

#include <string>
#include <vector>
#include <rtl-sdr.h>

typedef struct {
    uint32_t frequency;
    std::string modulation_mode;
    std::string ch_name;
} ScanList;

class Scanner {
    private:
        //rtlsdr_dev_t *device;
        std::vector<ScanList> channels;
        uint8_t ch_index;

    public:
        Scanner(rtlsdr_dev_t *dev, std::vector<ScanList> scanlist);
        void NextCh(void);
};

#endif // _SCANNER_H
