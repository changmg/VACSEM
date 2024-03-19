#pragma once


#include "header.h"


const double EPSILON = 1e-8;
const double DELAY_TOL = 1e-3;
const double AREA_TOL = 1e-3;


// util of file system
bool IsPathExist(const std::string & _path);
void CreatePath(const std::string & _path);
void FixPath(std::string & _path);


// util of float-point numbers
static inline bool DoubleEqual     (const double a, const double b, double epsilon = EPSILON) {return fabs(a - b) < epsilon;}
static inline bool DoubleGreat     (const double a, const double b, double epsilon = EPSILON) {return a - b >= epsilon;}
static inline bool DoubleGreatEqual(const double a, const double b, double epsilon = EPSILON) {return a - b > -epsilon;}
static inline bool DoubleLess      (const double a, const double b, double epsilon = EPSILON) {return a - b <= -epsilon;}
static inline bool DoubleLessEqual (const double a, const double b, double epsilon = EPSILON) {return a - b < epsilon;}
static inline void FixAndCreatePath(std::string & _path) {FixPath(_path); CreatePath(_path);}


// util of vectors
template <typename T>
void PrintVect(const std::vector <T> & vect, std::string && endWith = "", int len = std::numeric_limits <int>::max(), bool isRev = false) {
    std::cout << "(";
    int size = std::min(len, static_cast <int> (vect.size()));
    if (!isRev) {
        for (int i = 0; i < size; ++i) {
            if (i == vect.size() - 1)
                std::cout << vect[i];
            else
                std::cout << vect[i] << ",";
        }
    }
    else {
        for (int i = size - 1; i >= 0; --i) {
            if (i == 0)
                std::cout << vect[i];
            else
                std::cout << vect[i] << ",";
        }
    }
    std::cout << ")";
    std::cout << endWith;
}


template <typename T>
void PrintVect(const std::vector <T> && vect, std::string && endWith = "") {
    std::cout << "(";
    for (int i = 0; i < vect.size(); ++i) {
        if (i == vect.size() - 1)
            std::cout << vect[i];
        else
            std::cout << vect[i] << ",";
    }
    std::cout << ")";
    std::cout << endWith;
}


template <typename T>
void PrintVect(const std::vector < std::vector <T> > & vect) {
    for (auto & line: vect)
        PrintVect <T> (line, "\n");
}


template <typename T>
void PrintSet(const std::unordered_set <T> & _set, std::string && endWith = "") {
    PrintVect <T> (std::vector <T> (_set.begin(), _set.end()), std::move(endWith));
}


template <typename T>
void PrintSet(const std::set <T> & _set, std::string && endWith = "") {
    PrintVect <T> (std::vector <T> (_set.begin(), _set.end()), std::move(endWith));
}


template <typename T>
void PrintPair(const std::pair<T, T>& _pair, std::string&& endWith = "") {
    std::cout << "(" << _pair.first << "," << _pair.second << ")" << endWith;
}


template <>
inline void
boost::to_block_range(const boost::dynamic_bitset<>& b, std::tuple<int, ull&> param)
{
    int iBlock = std::get<0>(param);
    std::get<1>(param) = b.m_bits[iBlock];
    return;
}


static inline ull GetBlockFromDynBitset(const boost::dynamic_bitset<>& b, int iBlock) {
    ull res = 0;
    boost::to_block_range(b, std::make_tuple(iBlock, std::ref(res)));
    return res;
}


// misc
int ExecSystComm(const std::string && cmd);
static int WaitForAnyKey() { 
    printf("Press any key to continue...\n");
    struct termios tm, tm_old;
    int fd = STDIN_FILENO,c;
    if (tcgetattr(fd, &tm) < 0)
        return -1;
    tm_old = tm;
    cfmakeraw(&tm);
    if (tcsetattr(fd, TCSANOW, &tm) < 0)
        return -1;
    c = fgetc(stdin);
    if (tcsetattr(fd,TCSANOW,&tm_old) < 0)
        return -1;
    return c;
} 