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

    public:
        Scanner(std::vector<ScanList> scanlist);
        double NextCh(bool frq);
};

#endif // _SCANNER_H
