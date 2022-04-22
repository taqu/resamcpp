#include <cstdint>
#include <string>
#include <iostream>
#include <fstream>
#include <charconv>
#include <vector>
#include <limits>
#include <iomanip>

void conv(const char* dst, const char* cppdst, const char* cppdiff, const char* src, uint32_t step)
{
    std::ifstream file(src, std::ios::binary);
    if(!file.is_open()){
        return;
    }
    std::vector<double> data;
    data.reserve(4096);
    std::ofstream outfile(dst, std::ios::binary);
    std::ofstream outfile2(cppdst, std::ios::binary);
    std::ofstream outfile3(cppdiff, std::ios::binary);
    uint32_t count = 0;
    uint32_t count2 = 0;
    std::string line;
    while(!file.eof()){
        line.clear();
        std::getline(file, line);
        if(0 == (count%step)){
            outfile << line << '\n';
            outfile2 << "static_cast<f32>(" << line << "), ";
            ++count2;
            double value;
            std::from_chars(line.c_str(), line.c_str() + line.size(), value);
            data.push_back(value);
            if(0 == (count2%5)){
                outfile2 << std::endl;
            }
        }
        ++count;
    }
    outfile << std::ends;
    count2 = 0;
    for(size_t i = 1; i <= data.size(); ++i) {
        if(i < data.size()) {
            double d = data[i] - data[i-1];
            outfile3
                << "static_cast<f32>("
                << std::setprecision(std::numeric_limits<double>::max_digits10)
                << std::scientific
                << d
                << "), ";
        } else {
            outfile3 << "0.0f, ";
        }
        ++count2;
        if(0 == (count2 % 5)) {
            outfile3 << '\n';
        }
    }
    outfile3 << std::endl;
}

int main(void)
{
    conv("kaiser_fast.csv", "kaiser_fast_cpp.csv", "kaiser_fast_diff_cpp.csv", "kaiser_fast_half_window.csv", 64);
    conv("kaiser_best.csv", "kaiser_best_cpp.csv", "kaiser_best_diff_cpp.csv", "kaiser_best_half_window.csv", 128);
    return 0;
}

