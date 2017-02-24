//
// Created by 朱超 on 2017/2/23.
//

#ifndef MIS_WORK_H
#define MIS_WORK_H
#include <string>
namespace mis{
    class Work{
    public:
        std::string _str_buf;
    public:
        const char *getBufPtr() {
            return _str_buf.c_str();
        }
        std::string& getBuf() {
            return _str_buf;
        }
    };
}
#endif //MIS_WORK_H
