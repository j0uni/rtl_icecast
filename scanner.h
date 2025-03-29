#ifndef _SCANNER_H
#define _SCANNER_H

#include <string>
#include <rtl-sdr.h>

typedef struct {
    float frequency;
    std::string modulation_mode;
    std::string ch_name;
} ScanList;

class Scanner {
    private:

    public:
        Scanner(rtlsdr_dev_t *dev, void *scanlist);
};

#endif // _SCANNER_H
