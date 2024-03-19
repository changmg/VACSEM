#pragma once


#include "header.h"
#include "my_abc.h"


class Simulator: public NetMan {
private:
    unsigned seed;
    int nFrame;
    std::vector <BitVec> dat;

public:
    explicit Simulator(NetMan & net, unsigned _seed, int n_frame);
    ~Simulator() = default;
    Simulator(const Simulator &) = delete;
    Simulator(Simulator &&) = delete;
    Simulator & operator = (const Simulator &) = delete;
    Simulator & operator = (Simulator &&) = delete;

    void InpUnifFast();
    void InpEnumFast(int round);
    void InpEnumForBigInt(BigInt round);
    void SimNew();
    void UpdAigNode(abc::Abc_Obj_t * pObj);
    void UpdSopNode(abc::Abc_Obj_t * pObj);
    void UpdGateNode(abc::Abc_Obj_t * pObj);
    void UpdSop(abc::Abc_Obj_t * pObj, char * pSop);
    BigInt GetInput(int iPatt, int lsb, int msb) const;
    void PrintInpStream(int iPatt) const;
    BigInt GetOutput(int iPatt) const;
    void PrintOutpStream(int iPatt) const;
    bool IsPIOSame(const Simulator & oth_smlt) const;

    inline int GetFrameNumb() {return nFrame;}
    inline BitVec* GetDat(abc::Abc_Obj_t * pObj) {return &dat[GetId(pObj)];}
    inline BitVec* GetDat(int id) {return &dat[id];}
    inline int CountNumbOfOnes(int id) {return dat[id].count();}
};
bool IsPIOSame(Simulator & smlt0, Simulator & smlt1);
bool IsPIOSame(NetMan & net0, NetMan & net1);


const int MAX_ENUM = 17;
// const int MAX_ENUM = 16;
// const int MAX_ENUM = 12;
const int MAX_FRAME_NUMB = 1 << MAX_ENUM;
// class EnumMan: public Simulator {
// private:
//     ll totFrame;

// public:
//     explicit EnumMan(NetMan & net):
//         Simulator(net, 0, (net.GetPiNum() < MAX_ENUM)? (1 << net.GetPiNum()): MAX_FRAME_NUMB),
//         totFrame(1ll << net.GetPiNum()) {}
//     ~EnumMan() = default;
//     EnumMan(const EnumMan &) = delete;
//     EnumMan(EnumMan &&) = delete;
//     EnumMan & operator = (const EnumMan &) = delete;
//     EnumMan & operator = (EnumMan &&) = delete;

//     ll CountModel();

//     inline ll GetTotFrame() {return totFrame;}
// };

class EnumMan: public Simulator {
private:
    BigInt totFrame;

public:
    explicit EnumMan(NetMan & net):
        Simulator(net, 0, (net.GetPiNum() < MAX_ENUM)? (1 << net.GetPiNum()): MAX_FRAME_NUMB),
        totFrame(BigInt(1) << net.GetPiNum()) {}
    ~EnumMan() = default;
    EnumMan(const EnumMan &) = delete;
    EnumMan(EnumMan &&) = delete;
    EnumMan & operator = (const EnumMan &) = delete;
    EnumMan & operator = (EnumMan &&) = delete;

    BigInt CountModel();

    inline BigInt GetTotFrame() {return totFrame;}
};