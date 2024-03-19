#include "simulator.h"


using namespace abc;
using namespace std;
using namespace boost;
using namespace random;


Simulator::Simulator(NetMan & net, unsigned _seed, int n_frame): NetMan(net.GetNet(), false), seed(_seed), nFrame(n_frame) {
    #ifdef DEBUG
    auto type = NetMan::GetNetType();
    assert(type == NET_TYPE::AIG || type == NET_TYPE::GATE || type == NET_TYPE::SOP);
    #endif
}


void Simulator::InpUnifFast() {
    dat.resize(NetMan::GetIdMaxPlus1());

    static random::uniform_int_distribution <ull> unifUll;
    static random::mt19937 eng(seed);
    static variate_generator <random::mt19937, random::uniform_int_distribution <ull> > randUll(eng, unifUll);
    const int unitLength = 64;
    assert((nFrame & (unitLength - 1)) == 0);
    int nUnit = nFrame / unitLength;

    for (int i = 0; i < NetMan::GetPiNum(); ++i) {
        auto piId = NetMan::GetPiId(i);
        dat[piId].resize(0); dat[piId].reserve(nUnit);
        for (int j = 0; j < nUnit; ++j) {
            ull numb = randUll();
            dat[piId].append(numb);
        }
    }

    for (int i = 0; i < NetMan::GetIdMaxPlus1(); ++i) {
        if (NetMan::IsConst0(i))
            dat[i].resize(nFrame, false);
        else if (NetMan::IsConst1(i))
            dat[i].resize(nFrame, true);
    }
}


void Simulator::InpEnumFast(int round) {
    // initialize
    const int unitLength = 64;
    const int nUnit = nFrame / unitLength;
    dat.resize(NetMan::GetIdMaxPlus1());
    assert(GetPiNum() <= 32);
    assert(nFrame <= (1ll << GetPiNum()));
    assert((nFrame & (unitLength - 1)) == 0);

    // pre-compute loop bodies
    static ULLVec loopBodys(6);
    if (round == 0) {
        for (int i = 0; i < 6; ++i) {
            std::bitset<64> bs(0);
            bool phase = true;
            const int change = (1 << i) - 1;
            for (int j = 0; j < 64; ++j) {
                if (!(j & change))
                    phase = !phase;
                if (phase)
                    bs.set(j);
            }
            loopBodys[i] = bs.to_ulong();
        }
    }
    const ull loopBody0 = 0;
    const ull loopBody1 = numeric_limits<ull>::max(); 

    // assign PIs
    // boost::timer::progress_display progrDispl(GetPiNum());
    int baseUnitNumb = round * nFrame / 64;
    static IntVec phase(GetPiNum(), 1);
    for (int i = 0; i < NetMan::GetPiNum(); ++i) {
        auto piId = NetMan::GetPiId(i);
        dat[piId].resize(0); dat[piId].reserve(nUnit);
        if (i <= 5) {
            for (int j = 0; j < nUnit; ++j)
                dat[piId].append(loopBodys[i]);
        }
        else {
            const int len = 1 << (i - 6);
            for (int j = baseUnitNumb; j < baseUnitNumb + nUnit; ++j) {
                if ((j & (len - 1)) == 0)
                    phase[i] = !phase[i];
                if (phase[i])
                    dat[piId].append(loopBody1);
                else
                    dat[piId].append(loopBody0);
            }
        }
        // ++progrDispl;
    }

    // assign constants
    if (round == 0) {
        for (int i = 0; i < NetMan::GetIdMaxPlus1(); ++i) {
            if (NetMan::IsConst0(i))
                dat[i].resize(nFrame, false);
            else if (NetMan::IsConst1(i))
                dat[i].resize(nFrame, true);
        }
    }
}


void Simulator::InpEnumForBigInt(BigInt round) {
    // initialize
    const int unitLength = 64;
    const int nUnit = nFrame / unitLength;
    dat.resize(NetMan::GetIdMaxPlus1());
    assert(GetPiNum() <= 500);
    assert(nFrame <= (BigInt(1) << GetPiNum()));
    assert((nFrame & (unitLength - 1)) == 0);

    // pre-compute loop bodies
    static ULLVec loopBodys(6);
    if (round == 0) {
        for (int i = 0; i < 6; ++i) {
            std::bitset<64> bs(0);
            bool phase = true;
            const int change = (1 << i) - 1;
            for (int j = 0; j < 64; ++j) {
                if (!(j & change))
                    phase = !phase;
                if (phase)
                    bs.set(j);
            }
            loopBodys[i] = bs.to_ulong();
        }
    }
    const ull loopBody0 = 0;
    const ull loopBody1 = numeric_limits<ull>::max(); 

    // assign PIs
    // boost::timer::progress_display progrDispl(GetPiNum());
    BigInt baseUnitNumb = round * nFrame / unitLength;
    static IntVec phase(GetPiNum(), 1);
    for (int i = 0; i < NetMan::GetPiNum(); ++i) {
        auto piId = NetMan::GetPiId(i);
        dat[piId].resize(0); dat[piId].reserve(nUnit);
        if (i <= 5) {
            for (int j = 0; j < nUnit; ++j)
                dat[piId].append(loopBodys[i]);
        }
        else {
            const int len = 1 << (i - 6);
            for (BigInt j = baseUnitNumb; j < baseUnitNumb + nUnit; ++j) {
                if ((j & (len - 1)) == 0)
                    phase[i] = !phase[i];
                if (phase[i])
                    dat[piId].append(loopBody1);
                else
                    dat[piId].append(loopBody0);
            }
        }
        // ++progrDispl;
    }

    // assign constants
    if (round == 0) {
        for (int i = 0; i < NetMan::GetIdMaxPlus1(); ++i) {
            if (NetMan::IsConst0(i))
                dat[i].resize(nFrame, false);
            else if (NetMan::IsConst1(i))
                dat[i].resize(nFrame, true);
        }
    }
}


void Simulator::SimNew() {
    auto type = GetNetType();
    auto nodes = CalcTopoOrd();

    IntVec fanoutCount(GetIdMaxPlus1(), 0);
    for (int i = 0; i < GetPiNum(); ++i) {
        auto pPi = GetPi(i);
        fanoutCount[pPi->Id] = GetFanoutNum(pPi);
    }
    for (const auto& pObj: nodes)
        fanoutCount[pObj->Id] = GetFanoutNum(pObj);

    // boost::timer::progress_display progrDispl(nodes.size());
    for (const auto& pObj: nodes) {
        if (type == NET_TYPE::AIG)
            UpdAigNode(pObj);
        else if (type == NET_TYPE::SOP)
            UpdSopNode(pObj);
        else if (type == NET_TYPE::GATE)
            UpdGateNode(pObj);
        else
            assert(0);
        for (int i = 0; i < GetFaninNum(pObj); ++i) {
            auto pFanin = GetFanin(pObj, i);
            --fanoutCount[pFanin->Id];
            if (fanoutCount[pFanin->Id] == 0)
                BitVec().swap(dat[pFanin->Id]);
        }
        // ++progrDispl;
    }
    for (int i = 0; i < GetPoNum(); ++i) {
        auto pPo = GetPo(i);
        auto drivId = GetFaninId(pPo, 0);
        assert(!Abc_ObjIsComplement(pPo));
        dat[GetId(pPo)] = dat[drivId];
    }
}


void Simulator::UpdAigNode(Abc_Obj_t * pObj) {
    #ifdef DEBUG
    assert(Abc_ObjIsNode(pObj));
    #endif
    auto pNtk = NetMan::GetNet();
    auto pMan = static_cast <Hop_Man_t *> (pNtk->pManFunc);
    auto pRoot = static_cast <Hop_Obj_t *> (pObj->pData);
    auto pRootR = Hop_Regular(pRoot);

    // skip constant node
    if (Hop_ObjIsConst1(pRootR))
        return;

    // get topological order of subnetwork in aig
    Vec_Ptr_t * vHopNodes = Hop_ManDfsNode(pMan, pRootR);

    // init internal hop nodes
    int maxHopId = -1;
    int i = 0;
    Hop_Obj_t * pHopObj = nullptr;
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    Vec_PtrForEachEntry( Hop_Obj_t *, pMan->vPis, pHopObj, i )
        maxHopId = max(maxHopId, pHopObj->Id);
    vector < BitVec > interData(maxHopId + 1, BitVec (nFrame, 0));
    unordered_map <int, BitVec *> hop2Data;
    Abc_Obj_t * pFanin = nullptr;
    Abc_ObjForEachFanin(pObj, pFanin, i)
        hop2Data[Hop_ManPi(pMan, i)->Id] = &dat[pFanin->Id];

    // special case for inverter or buffer
    if (pRootR->Type == AIG_PI) {
        pFanin = Abc_ObjFanin0(pObj);
        dat[pObj->Id] = dat[pFanin->Id];
    }

    // simulate
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i) {
        assert(Hop_ObjIsAnd(pHopObj));
        auto pHopFanin0 = Hop_ObjFanin0(pHopObj);
        auto pHopFanin1 = Hop_ObjFanin1(pHopObj);
        #ifdef DEBUG
        assert(!Hop_ObjIsConst1(pHopFanin0));
        assert(!Hop_ObjIsConst1(pHopFanin1));
        #endif
        BitVec & data0 = Hop_ObjIsPi(pHopFanin0) ? *hop2Data[pHopFanin0->Id] : interData[pHopFanin0->Id];
        BitVec & data1 = Hop_ObjIsPi(pHopFanin1) ? *hop2Data[pHopFanin1->Id] : interData[pHopFanin1->Id];
        BitVec & out = (pHopObj == pRootR) ? dat[pObj->Id] : interData[pHopObj->Id];
        bool isFanin0C = Hop_ObjFaninC0(pHopObj);
        bool isFanin1C = Hop_ObjFaninC1(pHopObj);
        if (!isFanin0C && !isFanin1C)
            out = data0 & data1;
        else if (!isFanin0C && isFanin1C)
            out = data0 & ~data1;
        else if (isFanin0C && !isFanin1C)
            out = ~data0 & data1;
        else if (isFanin0C && isFanin1C)
            out = ~(data0 | data1);
    }

    // complement
    if (Hop_IsComplement(pRoot))
        dat[pObj->Id].flip();

    // recycle memory
    Vec_PtrFree(vHopNodes); 
}


void Simulator::UpdSopNode(Abc_Obj_t * pObj) {
    #ifdef DEBUG
    assert(Abc_ObjIsNode(pObj));
    #endif
    // skip constant node
    if (Abc_NodeIsConst(pObj))
        return;
    // update sop
    char * pSop = static_cast <char *> (pObj->pData);
    UpdSop(pObj, pSop);
}


void Simulator::UpdGateNode(Abc_Obj_t * pObj) {
    #ifdef DEBUG
    assert(Abc_ObjIsNode(pObj));
    #endif
    // skip constant node
    if (Abc_NodeIsConst(pObj))
        return;
    // update sop
    char * pSop = static_cast <char *> ((static_cast <Mio_Gate_t *> (pObj->pData))->pSop);
    UpdSop(pObj, pSop);
}


void Simulator::UpdSop(Abc_Obj_t * pObj, char * pSop) {
    int nVars = Abc_SopGetVarNum(pSop);
    BitVec product(nFrame, 0);
    for (char * pCube = pSop; *pCube; pCube += nVars + 3) {
        bool isFirst = true;
        for (int i = 0; pCube[i] != ' '; i++) {
            Abc_Obj_t * pFanin = Abc_ObjFanin(pObj, i);
            switch (pCube[i]) {
                case '-':
                    continue;
                    break;
                case '0':
                    if (isFirst) {
                        isFirst = false;
                        product = ~dat[pFanin->Id];
                    }
                    else
                        product &= ~dat[pFanin->Id];
                    break;
                case '1':
                    if (isFirst) {
                        isFirst = false;
                        product = dat[pFanin->Id];
                    }
                    else
                        product &= dat[pFanin->Id];
                    break;
                default:
                    assert(0);
            }
        }
        if (isFirst) {
            isFirst = false;
            product.set();
        }
        #ifdef DEBUG
        assert(!isFirst);
        #endif
        if (pCube == pSop)
            dat[pObj->Id] = product;
        else
            dat[pObj->Id] |= product;
    }

    // complement
    if (Abc_SopIsComplement(pSop))
        dat[pObj->Id].flip();
}


BigInt Simulator::GetInput(int iPatt, int lsb, int msb) const {
    #ifdef DEBUG
    assert(lsb >= 0 && msb < NetMan::GetPiNum());
    assert(iPatt < nFrame);
    assert(lsb <= msb && msb - lsb < 512);
    #endif
    BigInt ret(0);
    for (int k = msb; k >= lsb; --k) {
        ret <<= 1;
        if (dat[NetMan::GetPiId(k)][iPatt])
            ++ret;
    }
    return ret;
}


void Simulator::PrintInpStream(int iPatt) const {
    #ifdef DEBUG
    assert(iPatt < nFrame);
    #endif
    for (int k = GetPiNum() - 1; k >= 0; --k)
        cout << dat[GetPiId(k)][iPatt];
    cout << endl;
}


BigInt Simulator::GetOutput(int iPatt) const {
    int lsb = 0;
    int msb = NetMan::GetPoNum() - 1;
    #ifdef DEBUG
    assert(iPatt < nFrame);
    assert(msb < 512);
    #endif
    BigInt ret(0);
    for (int k = msb; k >= lsb; --k) {
        ret <<= 1;
        if (dat[NetMan::GetPoId(k)][iPatt])
            ++ret;
    }
    return ret;
}


void Simulator::PrintOutpStream(int iPatt) const {
    #ifdef DEBUG
    assert(iPatt < nFrame);
    #endif
    for (int k = GetPoNum() - 1; k >= 0; --k)
        cout << dat[GetPoId(k)][iPatt];
    cout << endl;
}


bool Simulator::IsPIOSame(const Simulator & oth_smlt) const {
    if (this->GetPiNum() != oth_smlt.GetPiNum())
        return false;
    for (int i = 0; i < this->GetPiNum(); ++i) {
        // if (strcmp(Abc_ObjName(Abc_NtkPo(pNtk1, i)), Abc_ObjName(Abc_NtkPo(pNtk2, i))) != 0)
        if (this->GetPiName(i) != oth_smlt.GetPiName(i))
            return false;
    }
    if (this->GetPoNum() != oth_smlt.GetPoNum())
        return false;
    for (int i = 0; i < this->GetPoNum(); ++i) {
        // if (strcmp(Abc_ObjName(Abc_NtkPo(pNtk1, i)), Abc_ObjName(Abc_NtkPo(pNtk2, i))) != 0)
        if (this->GetPoName(i) != oth_smlt.GetPoName(i))
            return false;
    }
    return true;
}


bool IsPIOSame(Simulator & smlt0, Simulator & smlt1) {
    if (smlt0.GetPiNum() != smlt1.GetPiNum())
        return false;
    for (int i = 0; i < smlt0.GetPiNum(); ++i) {
        // if (strcmp(Abc_ObjName(Abc_NtkPo(pNtk1, i)), Abc_ObjName(Abc_NtkPo(pNtk2, i))) != 0)
        if (smlt0.GetPiName(i) != smlt1.GetPiName(i))
            return false;
    }
    if (smlt0.GetPoNum() != smlt1.GetPoNum())
        return false;
    for (int i = 0; i < smlt0.GetPoNum(); ++i) {
        // if (strcmp(Abc_ObjName(Abc_NtkPo(pNtk1, i)), Abc_ObjName(Abc_NtkPo(pNtk2, i))) != 0)
        if (smlt0.GetPoName(i) != smlt1.GetPoName(i))
            return false;
    }
    return true;
}


bool IsPIOSame(NetMan & net0, NetMan & net1) {
    if (net0.GetPiNum() != net1.GetPiNum())
        return false;
    for (int i = 0; i < net0.GetPiNum(); ++i) {
        // if (strcmp(Abc_ObjName(Abc_NtkPo(pNtk1, i)), Abc_ObjName(Abc_NtkPo(pNtk2, i))) != 0)
        if (net0.GetPiName(i) != net1.GetPiName(i))
            return false;
    }
    if (net0.GetPoNum() != net1.GetPoNum())
        return false;
    for (int i = 0; i < net0.GetPoNum(); ++i) {
        // if (strcmp(Abc_ObjName(Abc_NtkPo(pNtk1, i)), Abc_ObjName(Abc_NtkPo(pNtk2, i))) != 0)
        if (net0.GetPoName(i) != net1.GetPoName(i))
            return false;
    }
    return true;
}


// ll EnumMan::CountModel() {
//     int nPI = GetPiNum();
//     assert(nPI <= 63 && GetPoNum() == 1);
//     ll sum = 0;

//     if (nPI >= MAX_ENUM) {
//         assert((totFrame & (MAX_FRAME_NUMB - 1)) == 0);
//         int nRound = totFrame >> MAX_ENUM;
//         boost::timer::progress_display progrDispl(nRound);
//         clock_t st = clock();
//         for (int round = 0; round < nRound; ++round) {
//             InpEnumFast(round);
//             SimNew();
//             sum += CountNumbOfOnes(GetPoId(0));
//             ++progrDispl;
//         }
//         cout << "simulation time = " << (clock() - st) / pow(10, 6) << "s" << endl;
//     }
//     else {
//         clock_t st = clock();
//         InpEnumFast(0);
//         SimNew();
//         sum = CountNumbOfOnes(GetPoId(0));
//         cout << "simulation time = " << (clock() - st) / pow(10, 6) << "s" << endl;
//     }
//     return sum;
// }


BigInt EnumMan::CountModel() {
    int nPI = GetPiNum();
    assert(nPI <= 500 && GetPoNum() == 1);
    BigInt sum(0);

    if (nPI >= MAX_ENUM) {
        assert((totFrame & (MAX_FRAME_NUMB - 1)) == 0);
        BigInt nRound = totFrame >> MAX_ENUM;
        // boost::timer::progress_display progrDispl(nRound);
        clock_t st = clock();
        for (BigInt round = 0; round < nRound; ++round) {
            InpEnumForBigInt(round);
            SimNew();
            sum += CountNumbOfOnes(GetPoId(0));
            if ((round & ((1ll << 10) - 1)) == 0) {
                cout << "round: " << round + 1 << " of " << nRound << "(" << BigFlt(round * 100) / BigFlt(nRound) << "%)" << ", time = " << (clock() - st) / pow(10, 6) << "s" << endl;
            }
            // ++progrDispl;
        }
        cout << "simulation time = " << (clock() - st) / pow(10, 6) << "s" << endl;
    }
    else
        assert(0);
    // else {
    //     clock_t st = clock();
    //     InpEnumFast(0);
    //     SimNew();
    //     sum = CountNumbOfOnes(GetPoId(0));
    //     cout << "simulation time = " << (clock() - st) / pow(10, 6) << "s" << endl;
    // }
    return sum;
}