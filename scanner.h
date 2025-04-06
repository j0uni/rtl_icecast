#ifndef _SCANNER_H
#define _SCANNER_H

#include <string>
#include <vector>

typedef struct {
    double frequency;
    std::string modulation_mode;
    std::string ch_name;
} ScanList;

class Scanner {
    private:
        std::vector<ScanList> channels;
        std::size_t ch_index;
        uint16_t stepDelayMs;

    public:
        Scanner(std::vector<ScanList> scanlist);
        double NextCh(bool frq);
        void SetStepDelay(uint16_t delay);
};

#endif // _SCANNER_H
