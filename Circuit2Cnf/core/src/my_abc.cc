#include "my_abc.h"


using namespace abc;
using namespace std;


std::ostream & operator << (std::ostream & os, const NET_TYPE netwType) {
    const std::string strs[4] = {"AIG", "GATE", "SOP", "STRASH"};
    os << strs[static_cast <int> (netwType)];
    return os;
}


std::ostream & operator << (std::ostream & os, const ORIENT orient) {
    const std::string strs[2] = {"AREA", "DELAY"};
    os << strs[static_cast <int> (orient)];
    return os;
}


std::ostream & operator << (std::ostream & os, const MAP_TYPE cell) {
    const std::string strs[2] = {"LUT", "SCL"};
    os << strs[static_cast <int> (cell)];
    return os;
}


AbcMan::AbcMan() {
    #ifdef DEBUG
    assert(Abc_FrameGetGlobalFrame() != nullptr);
    #endif
}


void AbcMan::Comm(const string & cmd, bool isVerb) {
    if (isVerb)
        cout << "Execute abc command: " << cmd << endl;
    if (Cmd_CommandExecute(GetAbcFrame(), cmd.c_str())) {
        cout << "Execuation failed." << endl;
        assert(0);
    }
}


void AbcMan::ReadNet(const std::string & fileName) {
    if (!IsPathExist(fileName)) {
        cout << "no such file: " << fileName << endl;
        assert(0);
    }
    Comm("r " + fileName);
}


void AbcMan::WriteNet(const std::string & fileName, bool isVerb) {
    Comm("w " + fileName, isVerb);
}


void AbcMan::ReadStandCell(const std::string & fileName) {
    #ifdef DEBUG
    assert(IsPathExist(fileName));
    #endif
    Comm("r " + fileName);
}


void AbcMan::ConvToAig() {
    Comm("aig");
}


void AbcMan::ConvToGate() {
    Map(MAP_TYPE::SCL, ORIENT::AREA);
}


void AbcMan::ConvToSop() {
    if (GetNetType() == NET_TYPE::STRASH)
        Comm("logic;");
    Comm("sop");
}


void AbcMan::Strash() {
    Comm("st");
}


void AbcMan::PrintStat() {
    auto type = GetNetType();
    if (type == NET_TYPE::GATE && UseModernSCL())
        Comm("topo; stime");
    else
        Comm("ps");
}


void AbcMan::TopoSort() {
    auto type = GetNetType();
    assert(type == NET_TYPE::AIG || type == NET_TYPE::SOP || type == NET_TYPE::GATE);
    Comm("topo");

    // fix twin nodes
    auto pNtk = GetNet();
    if (Abc_NtkHasMapping(pNtk)) {
        Abc_Ntk_t * pNtkNew; 
        Abc_Obj_t * pObj, * pFanin;
        int i, k;
        assert(pNtk != nullptr);
        // start the network
        pNtkNew = Abc_NtkStartFrom( pNtk, pNtk->ntkType, pNtk->ntkFunc );
        // copy the internal nodes
        assert(!Abc_NtkIsStrash(pNtk));
        // duplicate the nets and nodes (CIs/COs/latches already dupped)
        set <int> skip;
        Abc_NtkForEachObj( pNtk, pObj, i ) {
            if ( pObj->pCopy == NULL && skip.count(pObj->Id) == 0 ) {
                Abc_NtkDupObj(pNtkNew, pObj, Abc_NtkHasBlackbox(pNtk) && Abc_ObjIsNet(pObj));
                auto pTwin = GetTwinNode(pObj);
                if (pTwin != nullptr) {
                    Abc_NtkDupObj(pNtkNew, pTwin, Abc_NtkHasBlackbox(pNtk) && Abc_ObjIsNet(pTwin));
                    skip.insert(pTwin->Id);
                }
            }
        }
        // reconnect all objects (no need to transfer attributes on edges)
        Abc_NtkForEachObj( pNtk, pObj, i )
            if ( !Abc_ObjIsBox(pObj) && !Abc_ObjIsBo(pObj) )
                Abc_ObjForEachFanin( pObj, pFanin, k )
                    Abc_ObjAddFanin( pObj->pCopy, pFanin->pCopy );
        // duplicate the EXDC Ntk
        if ( pNtk->pExdc )
            pNtkNew->pExdc = Abc_NtkDup( pNtk->pExdc );
        if ( pNtk->pExcare )
            pNtkNew->pExcare = Abc_NtkDup( (Abc_Ntk_t *)pNtk->pExcare );
        // duplicate timing manager
        if ( pNtk->pManTime )
            Abc_NtkTimeInitialize( pNtkNew, pNtk );
        if ( pNtk->vPhases )
            Abc_NtkTransferPhases( pNtkNew, pNtk );
        if ( pNtk->pWLoadUsed )
            pNtkNew->pWLoadUsed = Abc_UtilStrsav( pNtk->pWLoadUsed );
        // check correctness
        if ( !Abc_NtkCheck( pNtkNew ) )
            fprintf( stdout, "Abc_NtkDup(): Network check has failed.\n" );
        pNtk->pCopy = pNtkNew;
        // return pNtkNew;
        SetMainNetw(pNtkNew);
    }
}


void AbcMan::StatTimeAnal() {
    #ifdef DEBUG
    assert(GetNetType() == NET_TYPE::GATE && UseModernSCL());
    #endif
    TopoSort();
    Comm("stime");
}


void AbcMan::Synth(ORIENT orient, bool isVerb) {
    if (isVerb)
        cout << orient << "-oriented synthesis" << endl;
    const string areaComm = {"st; compress2rs;"};
    // const string delayComm = {"st; resyn2"};
    const string delayComm2 = {"st; resyn2;"};
    double oldArea = GetArea();
    double oldDelay = GetDelay();
    bool isCont = true;
    while (isCont) {
        isCont = false;
        auto oldNtk = Abc_NtkDup(GetNet());
        if (orient == ORIENT::AREA)
            Comm(areaComm, isVerb);
        else if (orient == ORIENT::DELAY)
            Comm(delayComm2, isVerb);
        else
            assert(0);
        auto res = make_pair <double, double> (GetArea(), GetDelay());
        if (isVerb)
            PrintStat();
        double newArea = res.first;
        double newDelay = res.second;
        IMPR impr = UpdNetw(oldArea, oldDelay, oldNtk, newArea, newDelay, orient);
        if (impr == IMPR::GOOD) {
            oldArea = newArea;
            oldDelay = newDelay;
            isCont = true;
        }
        if (isVerb)
            cout << (impr == IMPR::GOOD? "accept": "cancel") << endl;
    }
    if (isVerb)
        PrintStat();
}


void AbcMan::SynthAndMap(double maxDelay, bool isVerb) {
    bool cont = true;
    if (isVerb)
        cout << "maxDelay = " << maxDelay << endl;
    TopoSort();
    while (cont) {
        double oldArea = numeric_limits <double>::max(), oldDelay = numeric_limits <double>::max();
        if (GetNetType(GetNet()) == NET_TYPE::GATE)
            oldArea = GetArea(), oldDelay = GetDelay();
        auto pOldNtk = Abc_NtkDup(GetNet());
        if (isVerb)
            cout << "oldArea = " << oldArea << ", " << "oldDelay = " << oldDelay << endl;
        Comm("st; compress2rs; dch; amap;", isVerb);
        TopoSort();
        double newArea = GetArea(), newDelay = GetDelay();
        if (isVerb)
            cout << "newArea = " << newArea << ", " << "newDelay = " << newDelay << endl;
        if (newDelay <= maxDelay) {
            auto impr = UpdNetw(oldArea, oldDelay, pOldNtk, newArea, newDelay, ORIENT::AREA);
            if (impr != IMPR::GOOD) {
                cont = false;
                if (isVerb)
                    cout << "reject" << endl;
            }
            else {
                if (isVerb)
                    cout << "accept" << endl;
            }
        }
        else {
            SetMainNetw(pOldNtk);
                cont = false;
            if (isVerb)
                cout << "reject" << endl;
        }
    }
    PrintStat();
}


void AbcMan::Sweep() {
    #ifdef DEBUG
    assert(GetNetType() == NET_TYPE::SOP);
    #endif
    Comm("sweep; sop;");
}


pair <double, double> AbcMan::Map(MAP_TYPE cell, ORIENT orient, bool isVerb) {
    double oldArea = numeric_limits <double>::max();
    double oldDelay = numeric_limits <double>::max();
    ostringstream LutInpStr("");
    LutInpStr << LutInp;
    if ((cell == MAP_TYPE::SCL && GetNetType() == NET_TYPE::GATE) ||
        (cell == MAP_TYPE::LUT && IsLutNetw())) {
        oldArea = GetArea();
        oldDelay = GetDelay();
    }
    bool isCont = true;
    while (isCont) {
        auto oldNtk = Abc_NtkDup(GetNet());
        // Comm("st; dch;", isVerb);
        Comm("st;", isVerb);
        if (cell == MAP_TYPE::SCL) {
            if (orient == ORIENT::AREA)
                Comm("amap", isVerb);
            else if (orient == ORIENT::DELAY)
                Comm("map", isVerb);
            else
                assert(0);
        }
        else if (cell == MAP_TYPE::LUT) {
            if (orient == ORIENT::AREA)
                Comm("if -a -K " + LutInpStr.str(), isVerb);
            else if (orient == ORIENT::DELAY)
                Comm("if -K " + LutInpStr.str(), isVerb);
            else
                assert(0);
        }
        else
            assert(0);
        double newArea = GetArea();
        double newDelay = GetDelay();
        IMPR impr = UpdNetw(oldArea, oldDelay, oldNtk, newArea, newDelay, orient);
        if (impr == IMPR::GOOD) {
            oldArea = newArea;
            oldDelay = newDelay;
        }
        else
            isCont = false;
        if (isVerb)
            PrintStat();
    }
    return make_pair(oldArea, oldDelay);
}


pair <double, double> AbcMan::Map2(double maxDelay, bool isVerb) {
    double oldArea = numeric_limits <double>::max();
    double oldDelay = numeric_limits <double>::max();
    ostringstream LutInpStr("");
    LutInpStr << LutInp;
    assert(GetNetType() == NET_TYPE::STRASH);
    bool isFirst = true;
    bool isCont = true;
    while (isCont) {
        auto oldNtk = Abc_NtkDup(GetNet());
        if (isFirst) {
            Comm("st; dch;", isVerb);
            isFirst = false;
        }
        else
            Comm("st; b;", isVerb);
        ostringstream oss("");
        oss << "map -D " << maxDelay;
        Comm(oss.str(), isVerb);
        double newArea = GetArea();
        double newDelay = GetDelay();
        IMPR impr = UpdNetw(oldArea, oldDelay, oldNtk, newArea, newDelay, ORIENT::AREA);
        if (impr == IMPR::GOOD) {
            oldArea = newArea;
            oldDelay = newDelay;
        }
        else
            isCont = false;
        // PrintStat();
    }
    return make_pair(oldArea, oldDelay);
}


IMPR AbcMan::UpdNetw(double oldArea, double oldDelay, Abc_Ntk_t * oldNtk, double newArea, double newDelay, ORIENT orient) {
    IMPR impr = IMPR::SAME;
    if (orient == ORIENT::AREA) {
        if (DoubleGreat(newArea, oldArea) || (DoubleEqual(newArea, oldArea) && DoubleGreat(newDelay, oldDelay)))
            impr = IMPR::BAD;
        else if (DoubleEqual(newArea, oldArea) && DoubleEqual(newDelay, oldDelay))
            impr = IMPR::SAME;
        else
            impr = IMPR::GOOD;
    }
    else if (orient == ORIENT::DELAY) {
        if (DoubleGreat(newDelay, oldDelay) || (DoubleEqual(newDelay, oldDelay) && DoubleGreat(newArea, oldArea)))
            impr = IMPR::BAD;
        else if (DoubleEqual(newDelay, oldDelay) && DoubleEqual(newArea, oldArea))
            impr = IMPR::SAME;
        else
            impr = IMPR::GOOD;
    }
    else
        assert(0);
    if (impr == IMPR::BAD) {
        #ifdef DEBUG
        assert(oldArea != numeric_limits <double>::max() && oldDelay != numeric_limits <double>::max());
        assert(oldNtk != nullptr);
        // cout << "Cancel the last abc command" << endl;
        #endif
        SetMainNetw(oldNtk);
    }
    else
        Abc_NtkDelete(oldNtk);
    return impr;
}


NET_TYPE AbcMan::GetNetType(Abc_Ntk_t * pNtk) const {
    if (Abc_NtkIsAigLogic(pNtk))
        return NET_TYPE::AIG;
    else if (Abc_NtkIsMappedLogic(pNtk))
        return NET_TYPE::GATE;
    else if (Abc_NtkIsSopLogic(pNtk))
        return NET_TYPE::SOP;
    else if (Abc_NtkIsStrash(pNtk))
        return NET_TYPE::STRASH;
    else {
        cout << pNtk << endl;
        cout << "invalid network type" << endl;
        assert(0);
    }
}


double AbcMan::GetArea(Abc_Ntk_t * pNtk) const {
    auto type = GetNetType(pNtk);
    if (type == NET_TYPE::AIG || type == NET_TYPE::STRASH)
        return Abc_NtkNodeNum(pNtk);
    else if (type == NET_TYPE::SOP) {
        Abc_Obj_t * pObj = nullptr;
        int i = 0;
        int ret = Abc_NtkNodeNum(pNtk);
        Abc_NtkForEachNode(pNtk, pObj, i) {
            if (Abc_NodeIsConst(pObj))
                --ret;
        }
        return ret;
    }
    else if (type == NET_TYPE::GATE) {
        auto pLibScl = static_cast <SC_Lib *> (GetAbcFrame()->pLibScl);
        if (pLibScl == nullptr)
            return Abc_NtkGetMappedArea(pNtk);
        else {
            #ifdef DEBUG
            assert(pNtk->nBarBufs2 == 0);
            assert(CheckSCLNet(pNtk));
            #endif
            SC_Man * p = Abc_SclManStart(pLibScl, pNtk, 0, 1, 0, 0);
            double area = Abc_SclGetTotalArea(p->pNtk);
            Abc_SclManFree(p);
            return area;
        }
    }
    else
        assert(0);
}


double AbcMan::GetDelay(Abc_Ntk_t * pNtk) const {
    auto type = GetNetType(pNtk);
    if (type == NET_TYPE::AIG || type == NET_TYPE::SOP || type == NET_TYPE::STRASH)
        return Abc_NtkLevel(pNtk);
    else if (type == NET_TYPE::GATE) {
        auto pLibScl = static_cast <SC_Lib *> (GetAbcFrame()->pLibScl);
        if (pLibScl == nullptr) {
            return Abc_NtkDelayTrace(pNtk, nullptr, nullptr, 0);
        }
        else {
            #ifdef DEBUG
            assert(pNtk->nBarBufs2 == 0);
            assert(CheckSCLNet(pNtk));
            #endif
            SC_Man * p = Abc_SclManStart(pLibScl, pNtk, 0, 1, 0, 0);
            int fRise = 0;
            Abc_Obj_t * pPivot = Abc_SclFindCriticalCo(p, &fRise); 
            double delay = Abc_SclObjTimeOne(p, pPivot, fRise);
            Abc_Obj_t * pObj = nullptr;
            int i = 0;
            Abc_NtkForEachObj(pNtk, pObj, i)
                pObj->dTemp = Abc_SclObjTimeMax(p, pObj);
            Abc_SclManFree(p);
            return delay;
        }
    }
    else
        assert(0);
}


bool AbcMan::CheckSCLNet(abc::Abc_Ntk_t * pNtk) const {
    Abc_Obj_t * pObj, * pFanin;
    int i, k, fFlag = 1;
    Abc_NtkIncrementTravId( pNtk );        
    Abc_NtkForEachCi( pNtk, pObj, i )
        Abc_NodeSetTravIdCurrent( pObj );
    Abc_NtkForEachNode( pNtk, pObj, i )
    {
        Abc_ObjForEachFanin( pObj, pFanin, k )
            if ( !Abc_NodeIsTravIdCurrent( pFanin ) )
                printf( "obj %d and its fanin %d are not in the topo order\n", Abc_ObjId(pObj), Abc_ObjId(pFanin) ), fFlag = 0;
        Abc_NodeSetTravIdCurrent( pObj );
        if ( Abc_ObjIsBarBuf(pObj) )
            continue;
        // if ( Abc_ObjFanoutNum(pObj) == 0 )
        //     printf( "node %d has no fanout\n", Abc_ObjId(pObj) ), fFlag = 0;
        if ( !fFlag )
            break;
    }
    // if ( fFlag && fVerbose )
    //     printf( "The network is in topo order and no dangling nodes.\n" );
    return fFlag;
}


Abc_Obj_t * AbcMan::GetTwinNode( Abc_Obj_t * pNode ) {
    assert( Abc_NtkHasMapping(pNode->pNtk) );
    Mio_Gate_t * pGate = (Mio_Gate_t *)pNode->pData;
    if ( pGate == nullptr || Mio_GateReadTwin(pGate) == nullptr )
        return nullptr;
    Abc_Obj_t * pNode2 = nullptr;
    int id = 0;
    Abc_Obj_t * pTwin = nullptr;
    int count = 0;
    Abc_NtkForEachNode(pNode->pNtk, pNode2, id) {
        if ( Abc_ObjFaninNum(pNode) != Abc_ObjFaninNum(pNode2) )
            continue;
        bool sameFanin = true;
        for (int faninId = 0; faninId < Abc_ObjFaninNum(pNode); ++faninId) {
            if (Abc_ObjFanin(pNode, faninId) != Abc_ObjFanin(pNode2, faninId)) {
                sameFanin = false;
                break;
            }
        }
        if (!sameFanin)
            continue;
        if ( Mio_GateReadTwin(pGate) != (Mio_Gate_t *)pNode2->pData )
            continue;
        pTwin = pNode2;
        ++count;
        if (count > 1)
            assert(0);
    }
    return pTwin;
}


void AbcMan::LoadAlias() {
    Comm("alias hi history", false);
    Comm("alias b balance", false);
    Comm("alias cg clockgate", false);
    Comm("alias cl cleanup", false);
    Comm("alias clp collapse", false);
    Comm("alias cs care_set", false);
    Comm("alias el eliminate", false);
    Comm("alias esd ext_seq_dcs", false);
    Comm("alias f fraig", false);
    Comm("alias fs fraig_sweep", false);
    Comm("alias fsto fraig_store", false);
    Comm("alias fres fraig_restore", false);
    Comm("alias fr fretime", false);
    Comm("alias ft fraig_trust", false);
    Comm("alias ic indcut", false);
    Comm("alias lp lutpack", false);
    Comm("alias pcon print_cone", false);
    Comm("alias pd print_dsd", false);
    Comm("alias pex print_exdc -d", false);
    Comm("alias pf print_factor", false);
    Comm("alias pfan print_fanio", false);
    Comm("alias pg print_gates", false);
    Comm("alias pl print_level", false);
    Comm("alias plat print_latch", false);
    Comm("alias pio print_io", false);
    Comm("alias pk print_kmap", false);
    Comm("alias pm print_miter", false);
    Comm("alias ps print_stats ", false);
    Comm("alias psb print_stats -b", false);
    Comm("alias psu print_supp", false);
    Comm("alias psy print_symm", false);
    Comm("alias pun print_unate", false);
    Comm("alias q quit", false);
    Comm("alias r read", false);
    Comm("alias ra read_aiger", false);
    Comm("alias r3 retime -M 3", false);
    Comm("alias r3f retime -M 3 -f", false);
    Comm("alias r3b retime -M 3 -b", false);
    Comm("alias ren renode", false);
    Comm("alias rh read_hie", false);
    Comm("alias ri read_init", false);
    Comm("alias rl read_blif", false);
    Comm("alias rb read_bench", false);
    Comm("alias ret retime", false);
    Comm("alias dret dretime", false);
    Comm("alias rp read_pla", false);
    Comm("alias rt read_truth", false);
    Comm("alias rv read_verilog", false);
    Comm("alias rvl read_verlib", false);
    Comm("alias rsup read_super mcnc5_old.super", false);
    Comm("alias rlib read_library", false);
    Comm("alias rlibc read_library cadence.genlib", false);
    Comm("alias rty read_liberty", false);
    Comm("alias rlut read_lut", false);
    Comm("alias rw rewrite", false);
    Comm("alias rwz rewrite -z", false);
    Comm("alias rf refactor", false);
    Comm("alias rfz refactor -z", false);
    Comm("alias re restructure", false);
    Comm("alias rez restructure -z", false);
    Comm("alias rs resub", false);
    Comm("alias rsz resub -z", false);
    Comm("alias sa set autoexec ps", false);
    Comm("alias scl scleanup", false);
    Comm("alias sif if -s", false);
    Comm("alias so source -x", false);
    Comm("alias st strash", false);
    Comm("alias sw sweep", false);
    Comm("alias ssw ssweep", false);
    Comm("alias tr0 trace_start", false);
    Comm("alias tr1 trace_check", false);
    Comm("alias trt \"r c.blif; st; tr0; b; tr1\"", false);
    Comm("alias u undo", false);
    Comm("alias w write", false);
    Comm("alias wa write_aiger", false);
    Comm("alias wb write_bench", false);
    Comm("alias wc write_cnf", false);
    Comm("alias wh write_hie", false);
    Comm("alias wl write_blif", false);
    Comm("alias wp write_pla", false);
    Comm("alias wv write_verilog", false);
    Comm("alias resyn       \"b; rw; rwz; b; rwz; b\"", false);
    Comm("alias resyn2      \"b; rw; rf; b; rw; rwz; b; rfz; rwz; b\"", false);
    Comm("alias resyn2a     \"b; rw; b; rw; rwz; b; rwz; b\"", false);
    Comm("alias resyn3      \"b; rs; rs -K 6; b; rsz; rsz -K 6; b; rsz -K 5; b\"", false);
    Comm("alias compress    \"b -l; rw -l; rwz -l; b -l; rwz -l; b -l\"", false);
    Comm("alias compress2   \"b -l; rw -l; rf -l; b -l; rw -l; rwz -l; b -l; rfz -l; rwz -l; b -l\"", false);
    Comm("alias choice      \"fraig_store; resyn; fraig_store; resyn2; fraig_store; fraig_restore\"", false);
    Comm("alias choice2     \"fraig_store; balance; fraig_store; resyn; fraig_store; resyn2; fraig_store; resyn2; fraig_store; fraig_restore\"", false);
    Comm("alias rwsat       \"st; rw -l; b -l; rw -l; rf -l\"", false);
    Comm("alias drwsat2     \"st; drw; b -l; drw; drf; ifraig -C 20; drw; b -l; drw; drf\"", false);
    Comm("alias share       \"st; multi -m; sop; fx; resyn2\"", false);
    Comm("alias addinit     \"read_init; undc; strash; zero\"", false);
    Comm("alias blif2aig    \"undc; strash; zero\"", false);
    Comm("alias v2p         \"&vta_gla; &ps; &gla_derive; &put; w 1.aig; pdr -v\"", false);
    Comm("alias g2p         \"&ps; &gla_derive; &put; w 2.aig; pdr -v\"", false);
    Comm("alias &sw_        \"&put; sweep; st; &get\"", false);
    Comm("alias &fx_        \"&put; sweep; sop; fx; st; &get\"", false);
    Comm("alias &dc3        \"&b; &jf -K 6; &b; &jf -K 4; &b\"", false);
    Comm("alias &dc4        \"&b; &jf -K 7; &fx; &b; &jf -K 5; &fx; &b\"", false);
    Comm("alias src_rw      \"st; rw -l; rwz -l; rwz -l\"", false);
    Comm("alias src_rs      \"st; rs -K 6 -N 2 -l; rs -K 9 -N 2 -l; rs -K 12 -N 2 -l\"", false);
    Comm("alias src_rws     \"st; rw -l; rs -K 6 -N 2 -l; rwz -l; rs -K 9 -N 2 -l; rwz -l; rs -K 12 -N 2 -l\"", false);
    Comm("alias resyn2rs    \"b; rs -K 6; rw; rs -K 6 -N 2; rf; rs -K 8; b; rs -K 8 -N 2; rw; rs -K 10; rwz; rs -K 10 -N 2; b; rs -K 12; rfz; rs -K 12 -N 2; rwz; b\"", false);
    Comm("alias compress2rs \"b -l; rs -K 6 -l; rw -l; rs -K 6 -N 2 -l; rf -l; rs -K 8 -l; b -l; rs -K 8 -N 2 -l; rw -l; rs -K 10 -l; rwz -l; rs -K 10 -N 2 -l; b -l; rs -K 12 -l; rfz -l; rs -K 12 -N 2 -l; rwz -l; b -l\"", false);
    Comm("alias fix_aig     \"logic; undc; strash; zero\"", false);
    Comm("alias fix_blif    \"undc; strash; zero\"", false);
    Comm("alias recadd3     \"st; rec_add3; b; rec_add3; dc2; rec_add3; if -K 8; bidec; st; rec_add3; dc2; rec_add3; if -g -K 6; st; rec_add3\"", false);
}


NetMan::NetMan(): AbcMan(), pNtk(nullptr), isDupl(true) {
}


NetMan::NetMan(Abc_Ntk_t * p_ntk, bool is_dupl): AbcMan(), isDupl(is_dupl) {
    if (is_dupl)
        pNtk = Abc_NtkDup(p_ntk);
    else
        pNtk = p_ntk;
}


NetMan::NetMan(const std::string & fileName): AbcMan(), isDupl(true) {
    AbcMan::ReadNet(fileName);
    pNtk = Abc_NtkDup(AbcMan::GetNet());
}


NetMan::~NetMan() {
    if (isDupl && pNtk != AbcMan::GetNet()) {
        if (pNtk != nullptr) {
            Abc_NtkDelete(pNtk);
            pNtk = nullptr;
        }
    }
}


NetMan::NetMan(const NetMan & net_man): AbcMan(), isDupl(true) {
    // cout << "copy netman" << endl;
    pNtk = Abc_NtkDup(net_man.pNtk);
}


NetMan::NetMan(NetMan && net_man): AbcMan(), pNtk(net_man.pNtk), isDupl(net_man.isDupl) {
    // cout << "move copy netman" << endl;
    net_man.isDupl = false;
    net_man.pNtk = nullptr;
}


NetMan & NetMan::operator = (const NetMan & net_man) {
    // cout << "assign netman" << endl;
    if (this == &net_man)
        return *this;
    if (isDupl && pNtk != nullptr && pNtk != AbcMan::GetNet() && pNtk != net_man.GetNet())
        Abc_NtkDelete(pNtk);
    pNtk = Abc_NtkDup(net_man.GetNet());
    isDupl = true;
    return *this;
}


NetMan & NetMan::operator = (NetMan && net_man) {
    // cout << "move assign netman" << endl;
    if (this == &net_man)
        return *this;
    if (isDupl && pNtk != nullptr && pNtk != AbcMan::GetNet() && pNtk != net_man.GetNet())
        Abc_NtkDelete(pNtk);
    pNtk = net_man.pNtk;
    isDupl = net_man.isDupl;
    net_man.isDupl = false;
    net_man.pNtk = nullptr;
    return *this;
}


pair <int, int> NetMan::GetConstId(bool isVerb) {
    pair <int, int> ret(-1, -1);
    auto type = GetNetType();
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Abc_NtkForEachNode(GetNet(), pObj, i) {
        if (type == NET_TYPE::GATE || type == NET_TYPE::SOP) {
            if (Abc_NodeIsConst0(pObj)) {
                if (isVerb)
                    cout << "find const 0: " << pObj << endl;
                if (ret.first == -1)
                    ret.first = GetId(pObj);
            }
            else if (Abc_NodeIsConst1(pObj)) {
                if (isVerb)
                    cout << "find const 1: " << pObj << endl;
                if (ret.second == -1)
                    ret.second = GetId(pObj);
            }
        }
        else if (type == NET_TYPE::AIG) {
            auto pHopObj = static_cast <Hop_Obj_t *> (pObj->pData);
            auto pHopObjR = Hop_Regular(pHopObj);
            if (Hop_ObjIsConst1(pHopObjR)) {
                #ifdef DEBUG
                assert(Hop_ObjFanin0(pHopObjR) == nullptr);
                assert(Hop_ObjFanin1(pHopObjR) == nullptr);
                #endif
                if (!Hop_IsComplement(pHopObj))
                    ret.second = GetId(pObj);
                else 
                    ret.first = GetId(pObj);
            }
        }
        else
            assert(0);
    }
    return ret;
}


pair <int, int> NetMan::CreateConst(bool isVerb) {
    auto consts = GetConstId(isVerb);
    pair <int, int> ret(consts);
    if (ret.first == -1) {
        auto pObj = Abc_NtkCreateNodeConst0(GetNet());
        Rename(pObj, "zero");
        ret.first = GetId(pObj);
        // if (isVerb)
        //     cout << "create const 0: " << pObj << endl;
    }
    if (ret.second == -1) {
        auto pObj = Abc_NtkCreateNodeConst1(GetNet());
        Rename(pObj, "one");
        ret.second = GetId(pObj);
        // if (isVerb)
        //     cout << "create const 1: " << pObj << endl;
    }
    return ret;
}


void NetMan::MergeConst() {
    pair <int, int> ret(-1, -1);
    auto type = GetNetType();
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Abc_NtkForEachNode(GetNet(), pObj, i) {
        if (type == NET_TYPE::GATE || type == NET_TYPE::SOP) {
            if (Abc_NodeIsConst0(pObj)) {
                if (ret.first == -1)
                    ret.first = GetId(pObj);
                else {
                    // cout << "merge const 0: " << pObj << endl;
                    Abc_ObjReplace(pObj, GetObj(ret.first));
                }
            }
            else if (Abc_NodeIsConst1(pObj)) {
                if (ret.second == -1)
                    ret.second = GetId(pObj);
                else {
                    // cout << "merge const 1: " << pObj << endl;
                    Abc_ObjReplace(pObj, GetObj(ret.second));
                }
            }
        }
        else
            assert(0);
    }
}


void NetMan::TopoSort() {
    AbcMan::SetMainNetw(pNtk); // abc manage the memory of the old network
    AbcMan::TopoSort();
    pNtk = Abc_NtkDup(AbcMan::GetNet()); // NetMan manage the memory of the duplicated network
}


vector <Abc_Obj_t * > NetMan::CalcTopoOrd() const {
    auto nodeIds = CalcTopoOrdOfIds();
    vector <Abc_Obj_t *> nodes;
    nodes.reserve(nodeIds.size());
    for (const auto & nodeId: nodeIds)
        nodes.emplace_back(GetObj(nodeId));
    return nodes;
}


vector <int> NetMan::CalcTopoOrdOfIds() const {
    vector <int> nodes;
    nodes.reserve(GetNodeNum());
    SetNetNotTrav();
    for (int i = 0; i < GetPoNum(); ++i) {
        auto pDriver = GetFanin(GetPo(i), 0);
        if (!GetObjTrav(pDriver))
            CalcTopoOrdOfIdsRec(pDriver, nodes);
    }
    return nodes;
}


void NetMan::CalcTopoOrdOfIdsRec(Abc_Obj_t * pObj, vector <int> & nodes) const {
    if (!IsNode(pObj))
        return;
    // if (IsConst(pObj))
    //     return;
    SetObjTrav(pObj);
    for (int i = 0; i < GetFaninNum(pObj); ++i) {
        auto pFanin = GetFanin(pObj, i);
        if (!GetObjTrav(pFanin))
            CalcTopoOrdOfIdsRec(pFanin, nodes);
    }
    nodes.emplace_back(GetId(pObj));
}


vector <Abc_Obj_t *> NetMan::GetTFI(abc::Abc_Obj_t * pObj) const {
    vector <Abc_Obj_t *> nodes;
    nodes.reserve(GetNodeNum());
    SetNetNotTrav();
    for (int i = 0; i < GetFaninNum(pObj); ++i) {
        auto pFanin = GetFanin(pObj, i);
        if (!GetObjTrav(pFanin))
            GetTFIRec(pFanin, nodes);
    }
    return nodes;
}


void NetMan::GetTFIRec(abc::Abc_Obj_t * pObj, std::vector <abc::Abc_Obj_t *> & nodes) const {
    if (!IsNode(pObj))
        return;
    SetObjTrav(pObj);
    for (int i = 0; i < GetFaninNum(pObj); ++i) {
        auto pFanin = GetFanin(pObj, i);
        if (!GetObjTrav(pFanin))
            GetTFIRec(pFanin, nodes);
    }
    nodes.emplace_back(pObj);
}


vector <int> NetMan::GetTFI(abc::Abc_Obj_t * pObj, const set <int> & critGraph) const {
    vector <int> objs;
    objs.reserve(GetNodeNum());
    SetNetNotTrav();
    for (int i = 0; i < GetFaninNum(pObj); ++i) {
        auto pFanin = GetFanin(pObj, i);
        if (!GetObjTrav(pFanin))
            GetTFIRec(pFanin, objs, critGraph);
    }
    return objs;
}


void NetMan::GetTFIRec(abc::Abc_Obj_t * pObj, std::vector <int> & objs, const set <int> & critGraph) const {
    if (critGraph.count(pObj->Id) == 0)
        return;
    // if (!IsNode(pObj))
    //     return;
    SetObjTrav(pObj);
    for (int i = 0; i < GetFaninNum(pObj); ++i) {
        auto pFanin = GetFanin(pObj, i);
        if (!GetObjTrav(pFanin))
            GetTFIRec(pFanin, objs, critGraph);
    }
    objs.emplace_back(pObj->Id);
}


vector <Abc_Obj_t *> NetMan::GetTFO(abc::Abc_Obj_t * pObj) const {
    vector <Abc_Obj_t *> nodes;
    nodes.reserve(GetNodeNum());
    SetNetNotTrav();
    for (int i = 0; i < GetFanoutNum(pObj); ++i) {
        auto pFanout = GetFanout(pObj, i);
        if (!GetObjTrav(pFanout))
            GetTFORec(pFanout, nodes);
    }
    reverse(nodes.begin(), nodes.end());
    return nodes;
}


void NetMan::GetTFORec(abc::Abc_Obj_t * pObj, std::vector <abc::Abc_Obj_t *> & nodes) const {
    if (!IsNode(pObj))
        return;
    SetObjTrav(pObj);
    for (int i = 0; i < GetFanoutNum(pObj); ++i) {
        auto pFanout = GetFanout(pObj, i);
        if (!GetObjTrav(pFanout))
            GetTFORec(pFanout, nodes);
    }
    nodes.emplace_back(pObj);
}


vector <int> NetMan::GetTFO(abc::Abc_Obj_t * pObj, const set <int> & critGraph) const {
    vector <int> objs;
    objs.reserve(GetNodeNum());
    SetNetNotTrav();
    for (int i = 0; i < GetFanoutNum(pObj); ++i) {
        auto pFanout = GetFanout(pObj, i);
        if (!GetObjTrav(pFanout))
            GetTFORec(pFanout, objs, critGraph);
    }
    reverse(objs.begin(), objs.end());
    return objs;
}


void NetMan::GetTFORec(abc::Abc_Obj_t * pObj, std::vector <int> & objs, const set <int> & critGraph) const {
    if (critGraph.count(pObj->Id) == 0)
        return;
    // if (!IsNode(pObj))
    //     return;
    SetObjTrav(pObj);
    for (int i = 0; i < GetFanoutNum(pObj); ++i) {
        auto pFanout = GetFanout(pObj, i);
        if (!GetObjTrav(pFanout))
            GetTFORec(pFanout, objs, critGraph);
    }
    objs.emplace_back(pObj->Id);
}


void NetMan::Comm(const std::string& cmd, bool isVerb) {
    assert(isDupl == true);
    AbcMan::SetMainNetw(pNtk); // abc manage the memory of the old network
    AbcMan::Comm(cmd, isVerb);
    pNtk = Abc_NtkDup(AbcMan::GetNet()); // NetMan manage the memory of the duplicated network
}


void NetMan::Sweep() {
    #ifdef DEBUG
    assert(isDupl == true);
    #endif
    AbcMan::SetMainNetw(pNtk); // abc manage the memory of the old network
    AbcMan::Sweep();
    pNtk = Abc_NtkDup(AbcMan::GetNet()); // NetMan manage the memory of the duplicated network
}


void NetMan::ConvToSopWithTwoInps() {
    #ifdef DEBUG
    assert(isDupl == true);
    #endif
    AbcMan::SetMainNetw(pNtk); // abc manage the memory of the old network
    AbcMan::Comm("st; logic; sop;");
    pNtk = Abc_NtkDup(AbcMan::GetNet()); // NetMan manage the memory of the duplicated network
}


void NetMan::SimpOpt() {
    assert(isDupl == true);
    AbcMan::SetMainNetw(pNtk); // abc manage the memory of the old network
    AbcMan::Comm("rewrite; ps;");
    pNtk = Abc_NtkDup(AbcMan::GetNet()); // NetMan manage the memory of the duplicated network
}


void NetMan::Synth(ORIENT orient, bool isVerb) {
    #ifdef DEBUG
    assert(isDupl == true);
    #endif
    AbcMan::SetMainNetw(pNtk); // abc manage the memory of the old network
    AbcMan::Synth(orient, isVerb);
    pNtk = Abc_NtkDup(AbcMan::GetNet()); // NetMan manage the memory of the duplicated network
}


void NetMan::SynthAndMap(double maxDelay, bool isVerb) {
    #ifdef DEBUG
    assert(isDupl == true);
    #endif
    AbcMan::SetMainNetw(pNtk); // abc manage the memory of the old network
    AbcMan::SynthAndMap(maxDelay, isVerb);
    pNtk = Abc_NtkDup(AbcMan::GetNet()); // NetMan manage the memory of the duplicated network
}


void NetMan::SATSweep() {
    assert(isDupl == true);
    AbcMan::SetMainNetw(pNtk); // abc manage the memory of the old network
    // AbcMan::Comm("st; ifraig -v; logic; sop;");
    AbcMan::Comm("ps; ifraig; ps;");
    pNtk = Abc_NtkDup(AbcMan::GetNet()); // NetMan manage the memory of the duplicated network
}


std::pair <double, double> NetMan::Map(MAP_TYPE targ, ORIENT orient, bool isVerb) {
    #ifdef DEBUG
    assert(isDupl == true);
    #endif
    AbcMan::SetMainNetw(pNtk); // abc manage the memory of the old network
    auto ret = AbcMan::Map(targ, orient, isVerb);
    pNtk = Abc_NtkDup(AbcMan::GetNet()); // NetMan manage the memory of the duplicated network
    return ret;
}


void NetMan::Print(bool showFunct) const {
    if (GetNet()->pName != nullptr)
        cout << GetNet()->pName << endl;
    else
        cout << "network" << endl;
    for (int i = 0; i < GetIdMaxPlus1(); ++i) {
        if (GetObj(i) == nullptr)
            continue;
        PrintObj(i, showFunct);
    }
}


void NetMan::PrintObjBas(Abc_Obj_t * pObj, string && endWith) const {
    cout << GetName(pObj) << "(" << GetId(pObj) << ")" << endWith;
}


void NetMan::PrintObj(Abc_Obj_t * pObj, bool showFunct) const {
    PrintObjBas(pObj, ":");
    for (int i = 0; i < GetFaninNum(pObj); ++i)
        PrintObjBas(GetFanin(pObj, i), ",");
    if (showFunct) {
        if (NetMan::GetNetType() == NET_TYPE::SOP) {
            if (IsNode(pObj)) {
                auto pSop = static_cast <char *> (pObj->pData);
                for (auto pCh = pSop; *pCh != '\0'; ++pCh) {
                    if (*pCh != '\n')
                        cout << *pCh;
                    else
                        cout << "\\n";
                }
                cout << endl;
            }
            else
                cout << endl;
        }
        else if (NetMan::GetNetType() == NET_TYPE::GATE) {
            if (IsNode(pObj))
                cout << Mio_GateReadName(static_cast <Mio_Gate_t *> (pObj->pData)) << endl;
            else
                cout << endl;
        }
        else if (NetMan::GetNetType() == NET_TYPE::STRASH) {
            if (Abc_AigNodeIsAnd(pObj)) {
                assert(!Abc_ObjIsComplement(pObj));
                cout << !Abc_ObjFaninC0(pObj) << !Abc_ObjFaninC1(pObj) << " " << !Abc_ObjIsComplement(pObj) << endl;
            }
            else if (Abc_AigNodeIsConst(pObj)) {
                assert(pObj == Abc_AigConst1(GetNet()));
                cout << " 1" << endl;
            }
            else if (Abc_ObjIsPi(pObj))
                cout << endl;
            else if (Abc_ObjIsPo(pObj)) {
                if (Abc_ObjFaninC0(pObj))
                    cout << "0 1" << endl;
                else
                    cout << "1 1" << endl;
            }
            else
                assert(0);
        }
        else
            assert(0);
    }
    else
        cout << endl;
}


bool NetMan::IsPIOSame(const NetMan & oth_net) const {
    if (this->GetPiNum() != oth_net.GetPiNum())
        return false;
    for (int i = 0; i < this->GetPiNum(); ++i) {
        if (this->GetPiName(i) != oth_net.GetPiName(i))
            return false;
    }
    if (this->GetPoNum() != oth_net.GetPoNum())
        return false;
    for (int i = 0; i < this->GetPoNum(); ++i) {
        if (this->GetPoName(i) != oth_net.GetPoName(i))
            return false;
    }
    return true;
}


void NetMan::Show(const IntVec& nodes, const string & fileName, const IntVec& cut) const {
    #ifdef DEBUG
    assert(fileName.ends_with(".dot"));
    #endif
    FILE * pFile = fopen(fileName.c_str(), "w");
    #ifdef DEBUG
    assert(pFile != nullptr);
    // cout << "totally " << nodes.size() << " nodes" << endl;
    #endif

    // get level
    int maxLev = GetLev() + 1;

    // headers
    fprintf(pFile, "digraph network {\n");
    fprintf(pFile, "center = true;\n");
    // fprintf(pFile, "edge [dir = back]\n");
    fprintf(pFile, "{\n");
    fprintf(pFile, "node [shape = plaintext];\n");
    fprintf(pFile, "edge [style = invis];\n");
    fprintf(pFile, "LevelTitle1 [label=\"\"];\n");
    for (int iLev = maxLev; iLev >= 0; --iLev)
        fprintf(pFile, "Level%d [label=\"\"];\n", iLev);
    fprintf(pFile, "LevelTitle1");
    for (int iLev = maxLev; iLev >= 0; --iLev)
        fprintf(pFile, " ->  Level%d", iLev);
    fprintf(pFile, ";\n");
    fprintf(pFile, "}\n");

    // title
    fprintf(pFile, "{\n");
    fprintf(pFile, "  rank = same;\n");
    fprintf(pFile, "  LevelTitle1 [label = \"%s, totally %ld nodes\"];\n", GetNet()->pName, nodes.size());
    fprintf(pFile, "}\n");

    // collect nodes
    vector <IntVec> nodesInLevs(maxLev + 1, IntVec());
    for (const auto & node: nodes) {
        auto lev = GetObjLev(node);
        assert(lev >= 0 && lev <= maxLev);
        nodesInLevs[lev].emplace_back(node);
    }
    if (nodesInLevs[0].empty()) {
        assert(maxLev >= 2);
        set <int> piSet;
        for (const auto & node: nodesInLevs[1]) {
            for (int iFanin = 0; iFanin < GetFaninNum(node); ++iFanin) {
                auto faninId = GetFaninId(node, iFanin);
                assert(IsObjPi(faninId));
                if (piSet.count(faninId) == 0) {
                    piSet.insert(faninId);
                    nodesInLevs[0].emplace_back(faninId);
                }
            }
        }
    }
    if (nodesInLevs[maxLev].empty()) {
        assert(maxLev >= 2);
        for (const auto & node: nodesInLevs[maxLev - 1]) {
            for (int iFanout = 0; iFanout < GetFanoutNum(node); ++iFanout) {
                auto fanoutId = GetFanoutId(node, iFanout);
                assert(IsObjPo(fanoutId));
                nodesInLevs[maxLev].emplace_back(fanoutId);
            }
        }
    }
    // for (const auto & nodesInLev: nodesInLevs)
    //     PrintVect(nodesInLev, "\n");

    // nodes
    set <int> cutSet(cut.begin(), cut.end());
    for (int iLev = maxLev; iLev >= 0; --iLev) {
        fprintf(pFile, "{\n");
        fprintf(pFile, "  rank = same;\n");
        fprintf(pFile, "  Level%d [label = \"Level %d\"];\n", iLev, iLev);
        for (const auto & nodeId: nodesInLevs[iLev]) {
            if (iLev == maxLev)
                fprintf(pFile, "  Node%d [label = \"%s(%d)\", shape = rectangle, color = coral, fillcolor = coral];\n", nodeId, GetName(nodeId).c_str(), nodeId);
            else if (iLev == 0)
                fprintf(pFile, "  Node%d [label = \"%s(%d)\", shape = rectangle, color = coral, fillcolor = coral];\n", nodeId, GetName(nodeId).c_str(), nodeId);
            else {
                if (cutSet.count(nodeId))
                    fprintf(pFile, "  Node%d [label = \"%s(%d)\", shape = ellipse, color = red, fillcolor = red];\n", nodeId, GetName(nodeId).c_str(), nodeId);
                else
                    fprintf(pFile, "  Node%d [label = \"%s(%d)\", shape = ellipse];\n", nodeId, GetName(nodeId).c_str(), nodeId);
            }
        }
        fprintf(pFile, "}\n");
    }

    // edges
    set <int> nodeSet;
    for (const auto & nodesInLev: nodesInLevs) {
        for (const auto & node: nodesInLev)
            nodeSet.insert(node);
    }
    for (const auto & node: nodeSet) {
        for (int iFanin = 0; iFanin < GetFaninNum(node); ++iFanin) {
            auto faninId = GetFaninId(node, iFanin);
            if (nodeSet.count(faninId) && GetObjLev(node) == GetObjLev(faninId) + 1)
                fprintf(pFile, "Node%d -> Node%d [style = solid];\n", faninId, node);
        }
    }

    // tail
    fprintf(pFile, "}\n");
    fclose(pFile);
}


void NetMan::Show(const IntVec& nodes, const string & fileName, const IntVec& cut, const DblVec& dat, double bound) const {
    #ifdef DEBUG
    assert(fileName.ends_with(".dot"));
    #endif
    FILE * pFile = fopen(fileName.c_str(), "w");
    #ifdef DEBUG
    assert(pFile != nullptr);
    // cout << "totally " << nodes.size() << " nodes" << endl;
    #endif

    // get level
    int maxLev = GetLev() + 1;

    // headers
    fprintf(pFile, "digraph network {\n");
    fprintf(pFile, "center = true;\n");
    // fprintf(pFile, "edge [dir = back]\n");
    fprintf(pFile, "{\n");
    fprintf(pFile, "node [shape = plaintext];\n");
    fprintf(pFile, "edge [style = invis];\n");
    fprintf(pFile, "LevelTitle1 [label=\"\"];\n");
    for (int iLev = maxLev; iLev >= 0; --iLev)
        fprintf(pFile, "Level%d [label=\"\"];\n", iLev);
    fprintf(pFile, "LevelTitle1");
    for (int iLev = maxLev; iLev >= 0; --iLev)
        fprintf(pFile, " ->  Level%d", iLev);
    fprintf(pFile, ";\n");
    fprintf(pFile, "}\n");

    // title
    fprintf(pFile, "{\n");
    fprintf(pFile, "  rank = same;\n");
    fprintf(pFile, "  LevelTitle1 [label = \"%s, totally %ld nodes\"];\n", GetNet()->pName, nodes.size());
    fprintf(pFile, "}\n");

    // collect nodes
    vector <IntVec> nodesInLevs(maxLev + 1, IntVec());
    for (const auto & node: nodes) {
        auto lev = GetObjLev(node);
        assert(lev >= 0 && lev <= maxLev);
        nodesInLevs[lev].emplace_back(node);
    }
    if (nodesInLevs[0].empty()) {
        assert(maxLev >= 2);
        set <int> piSet;
        for (const auto & node: nodesInLevs[1]) {
            for (int iFanin = 0; iFanin < GetFaninNum(node); ++iFanin) {
                auto faninId = GetFaninId(node, iFanin);
                assert(IsObjPi(faninId));
                if (piSet.count(faninId) == 0) {
                    piSet.insert(faninId);
                    nodesInLevs[0].emplace_back(faninId);
                }
            }
        }
    }
    if (nodesInLevs[maxLev].empty()) {
        assert(maxLev >= 2);
        for (const auto & node: nodesInLevs[maxLev - 1]) {
            for (int iFanout = 0; iFanout < GetFanoutNum(node); ++iFanout) {
                auto fanoutId = GetFanoutId(node, iFanout);
                assert(IsObjPo(fanoutId));
                nodesInLevs[maxLev].emplace_back(fanoutId);
            }
        }
    }
    // for (const auto & nodesInLev: nodesInLevs)
    //     PrintVect(nodesInLev, "\n");

    // nodes
    set <int> cutSet(cut.begin(), cut.end());
    for (int iLev = maxLev; iLev >= 0; --iLev) {
        fprintf(pFile, "{\n");
        fprintf(pFile, "  rank = same;\n");
        fprintf(pFile, "  Level%d [label = \"Level %d\"];\n", iLev, iLev);
        for (const auto & nodeId: nodesInLevs[iLev]) {
            if (iLev == maxLev || iLev == 0)
                fprintf(pFile, "  Node%d [label = \"%s(%d)\\n%.4f\", shape = rectangle];\n", nodeId, GetName(nodeId).c_str(), nodeId, dat[nodeId]);
            else {
                if (dat[nodeId] > bound) {
                    if (cutSet.count(nodeId))
                        fprintf(pFile, "  Node%d [label = \"%s(%d)\\n%.4f\", style = filled, shape = ellipse, fillcolor = red];\n", nodeId, GetName(nodeId).c_str(), nodeId, dat[nodeId]);
                    else
                        fprintf(pFile, "  Node%d [label = \"%s(%d)\\n%.4f\", shape = ellipse];\n", nodeId, GetName(nodeId).c_str(), nodeId, dat[nodeId]);
                }
                else {
                    if (cutSet.count(nodeId))
                        fprintf(pFile, "  Node%d [label = \"%s(%d)\\n%.4f\", style = filled, shape = ellipse, fillcolor = red];\n", nodeId, GetName(nodeId).c_str(), nodeId, dat[nodeId]);
                    else
                        fprintf(pFile, "  Node%d [label = \"%s(%d)\\n%.4f\", style = filled, shape = ellipse, fillcolor = lightblue];\n", nodeId, GetName(nodeId).c_str(), nodeId, dat[nodeId]);
                }
            }
        }
        fprintf(pFile, "}\n");
    }

    // edges
    set <int> nodeSet;
    for (const auto & nodesInLev: nodesInLevs) {
        for (const auto & node: nodesInLev)
            nodeSet.insert(node);
    }
    for (const auto & node: nodeSet) {
        for (int iFanin = 0; iFanin < GetFaninNum(node); ++iFanin) {
            auto faninId = GetFaninId(node, iFanin);
            if (nodeSet.count(faninId) && GetObjLev(node) == GetObjLev(faninId) + 1)
                fprintf(pFile, "Node%d -> Node%d [style = solid];\n", faninId, node);
        }
    }

    // tail
    fprintf(pFile, "}\n");
    fclose(pFile);
}


int NetMan::GetNodeMffcSize(Abc_Obj_t * pNode) const {
    #ifdef DEBUG
    assert(IsNode(pNode));
    #endif
    Vec_Ptr_t * vCone = Vec_PtrAlloc( 100 );
    Abc_NodeDeref_rec(pNode);
    Abc_NodeMffcConeSupp(pNode, vCone, nullptr);
    Abc_NodeRef_rec( pNode );
    int ret = Vec_PtrSize(vCone);
    Vec_PtrFree(vCone);
    return ret;
}


vector <Abc_Obj_t *> NetMan::GetNodeMffc(Abc_Obj_t * pNode) const {
    #ifdef DEBUG
    assert(IsNode(pNode));
    #endif
    Vec_Ptr_t * vCone = Vec_PtrAlloc( 100 );
    Abc_NodeDeref_rec(pNode);
    Abc_NodeMffcConeSupp(pNode, vCone, nullptr);
    Abc_NodeRef_rec( pNode );
    vector <Abc_Obj_t *> mffc;
    mffc.reserve(Vec_PtrSize(vCone));
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Vec_PtrForEachEntry(Abc_Obj_t *, vCone, pObj, i)
        mffc.emplace_back(pObj);
    Vec_PtrFree(vCone);
    return mffc;
}


static inline int Vec_IntFindFrom(Vec_Int_t * p, int Entry, int start) {
    int i = 0;
    for ( i = start; i < p->nSize; i++ )
        if ( p->pArray[i] == Entry )
            return i;
    assert(0);
    return -1;
}


IntVec NetMan::TempRepl(abc::Abc_Obj_t * pTS, abc::Abc_Obj_t * pSS) {
    #ifdef DEBUG
    assert(pTS != pSS);
    assert(pTS->pNtk == pSS->pNtk);
    assert(abc::Abc_ObjFanoutNum(pTS));
    #endif
    // record fanouts
    IntVec ret = {pTS->Id, pSS->Id};
    Abc_Obj_t * pFanout = nullptr;
    int i = 0;
    set<pair<int, int>> foIFaninPair;
    Abc_ObjForEachFanout(pTS, pFanout, i) {
        ret.emplace_back(pFanout->Id);
        int start = 0;
        int iFanin = Vec_IntFindFrom(&pFanout->vFanins, pTS->Id, start);
        while (foIFaninPair.count(pair(pFanout->Id, iFanin)))
            iFanin = Vec_IntFindFrom(&pFanout->vFanins, pTS->Id, iFanin + 1);
        ret.emplace_back(iFanin);
        foIFaninPair.emplace(pair(pFanout->Id, iFanin));
    }
    // PrintVect(ret, "\n");
    // transfer fanouts
    abc::Abc_ObjTransferFanout(pTS, pSS);
    return ret;
}


void NetMan::Recov(std::vector <int> & replTrace, bool isVerb) {
    #ifdef DEBUG
    assert(replTrace.size() > 2);
    assert(replTrace.size() % 2 == 0);
    #endif
    auto pTS = GetObj(replTrace[0]), pSS = GetObj(replTrace[1]);
    if (isVerb) cout << "recover " << pTS << " and " << pSS << endl;
    for (int i = 1; i < replTrace.size() / 2; ++i) {
        auto pFanout = GetObj(replTrace[i * 2]);
        auto iFanin = replTrace[i * 2 + 1];
        PatchFanin(pFanout, iFanin, pSS, pTS);
    }
}


static inline int Vec_IntFindRev( Vec_Int_t * p, int Entry ) {
    int i;
    // for ( i = 0; i < p->nSize; i++ )
    for (i = p->nSize - 1; i >= 0; --i)
        if ( p->pArray[i] == Entry )
            return i;
    return -1;
}


static inline int Vec_IntRemoveRev( Vec_Int_t * p, int Entry ) {
    int i;
    // for ( i = 0; i < p->nSize; i++ )
    for (i = p->nSize - 1; i >= 0; --i)
        if ( p->pArray[i] == Entry )
            break;
    if ( i == p->nSize )
        return 0;
    assert( i < p->nSize );
    for ( i++; i < p->nSize; i++ )
        p->pArray[i-1] = p->pArray[i];
    p->nSize--;
    return 1;
}


static inline void Vec_IntPushMem( Mem_Step_t * pMemMan, Vec_Int_t * p, int Entry ) {
    if ( p->nSize == p->nCap )
    {
        int * pArray;
        int i;

        if ( p->nSize == 0 )
            p->nCap = 1;
        if ( pMemMan )
            pArray = (int *)Mem_StepEntryFetch( pMemMan, p->nCap * 8 );
        else
            pArray = ABC_ALLOC( int, p->nCap * 2 );
        if ( p->pArray )
        {
            for ( i = 0; i < p->nSize; i++ )
                pArray[i] = p->pArray[i];
            if ( pMemMan )
                Mem_StepEntryRecycle( pMemMan, (char *)p->pArray, p->nCap * 4 );
            else
                ABC_FREE( p->pArray );
        }
        p->nCap *= 2;
        p->pArray = pArray;
    }
    p->pArray[p->nSize++] = Entry;
}


void NetMan::PatchFanin( Abc_Obj_t * pObj, int iFanin, Abc_Obj_t * pFaninOld, Abc_Obj_t * pFaninNew ) {
    Abc_Obj_t * pFaninNewR = Abc_ObjRegular(pFaninNew);
    assert( !Abc_ObjIsComplement(pObj) );
    assert( !Abc_ObjIsComplement(pFaninOld) );
    assert( pFaninOld != pFaninNewR );
    assert( pObj->pNtk == pFaninOld->pNtk );
    assert( pObj->pNtk == pFaninNewR->pNtk );
    assert( abc::Abc_ObjFanin(pObj, iFanin) == pFaninOld );

    // remember the attributes of the old fanin
    Vec_IntWriteEntry( &pObj->vFanins, iFanin, pFaninNewR->Id );
    if ( Abc_ObjIsComplement(pFaninNew) )
        Abc_ObjXorFaninC( pObj, iFanin );

    // update the fanout of the fanin
    if ( !Vec_IntRemoveRev( &pFaninOld->vFanouts, pObj->Id ) ) {
        printf( "Node %s is not among", Abc_ObjName(pObj) );
        printf( " the fanouts of its old fanin %s...\n", Abc_ObjName(pFaninOld) );
    }
    Vec_IntPushMem( pObj->pNtk->pMmStep, &pFaninNewR->vFanouts, pObj->Id );
}


void NetMan::Trunc(int truncBit) {
    cout << "***** truncate " << truncBit << " bits" << endl;
    // truncation
    auto consts = CreateConst();
    #ifdef DEBUG
    assert(truncBit <= GetPoNum());
    #endif
    for (int poId = 0; poId < truncBit; ++poId) {
        auto pPo = GetPo(poId);
        #ifdef DEBUG
        assert(GetFaninNum(pPo) == 1);
        #endif
        auto pDriv = GetFanin(pPo, 0);
        Abc_ObjPatchFanin(pPo, pDriv, GetObj(consts.first));
    }
    // clean up
    CleanUp();
}


bool NetMan::CleanUp() {
    bool isUpd = false;
    bool isCont = true;
    while (isCont) {
        isCont = false;
        // delete redundant node
        for (int nodeId = 0; nodeId < GetIdMaxPlus1(); ++nodeId) {
            if (IsNode(nodeId) && GetFanoutNum(nodeId) == 0) {
                isCont = true;
                auto mffc = GetNodeMffc(GetObj(nodeId));
                for (const auto & pObj: mffc) {
                    // cout << "delete " << pObj << " ";
                    // if (GetNetType() == NET_TYPE::GATE) {
                    //     auto gateName = GetGateName(pObj);
                    //     cout << gateName;
                    //     if (gateName.find("HA1") != -1 || gateName.find("FA1") != -1) {
                    //         if (GetTwinNode(pObj) != nullptr)
                    //             cout << " twin " << GetTwinNode(pObj);
                    //     }
                    // }
                    // cout << endl;
                    DelObj(pObj);
                    isUpd = true;
                }
                break;
            }
        }
    }
    return isUpd;
}


bool NetMan::ProcHalfAndFullAdd() {
    // special processing for half/full adder
    if (GetNetType() != NET_TYPE::GATE)
        return false;
    bool isUpd = false;
    int idMaxPlus1 = GetIdMaxPlus1();
    for (int nodeId = 0; nodeId < idMaxPlus1; ++nodeId) {
        if (!IsNode(nodeId)) continue;
        auto pNode = GetObj(nodeId);
        if (GetGateName(pNode).find("HA1") != -1) {
            if (GetTwinNode(pNode) == nullptr) {
                cout << "cannot find twin for "; PrintObj(pNode, true);
                auto sop = string(Mio_GateReadSop((Mio_Gate_t *)pNode->pData));
                auto pLib = (Mio_Library_t *)Abc_FrameReadLibGen();
                auto pNewNode = Abc_NtkCreateNode(GetNet());
                for (int faninId = 0; faninId < GetFaninNum(pNode); ++faninId)
                    Abc_ObjAddFanin(pNewNode, GetFanin(pNode, faninId));
                Abc_Obj_t * pSub = nullptr;
                if (sop == "11 1\n") { // CO=A B
                    auto pGate = Mio_LibraryReadGateByName(pLib, "CKAN2D1BWP7T30P140HVT", nullptr);
                    assert(pGate != nullptr);
                    pNewNode->pData = pGate;
                    pSub = pNewNode;
                }
                else if (sop == "10 1\n01 1\n" || sop == "01 1\n10 1\n") { // S=A^B
                    auto pGate = Mio_LibraryReadGateByName(pLib, "XOR2D0BWP7T30P140HVT", nullptr);
                    assert(pGate != nullptr);
                    pNewNode->pData = pGate;
                    pSub = pNewNode;
                }
                else {
                    cout << sop;
                    assert(0);
                }
                assert(pSub != nullptr);
                cout << "replace " << pNode << " with new node " << pSub << endl;
                TransfFanout(pNode, pSub);
                isUpd = true;
            }
        }
        else if (GetGateName(pNode).find("FA1") != -1) {
            if (GetTwinNode(pNode) == nullptr) {
                cout << "cannot find twin for "; PrintObj(pNode, true);
                auto sop = string(Mio_GateReadSop((Mio_Gate_t *)pNode->pData));
                auto pLib = (Mio_Library_t *)Abc_FrameReadLibGen();
                auto pNewNode = Abc_NtkCreateNode(GetNet());
                for (int faninId = 0; faninId < GetFaninNum(pNode); ++faninId)
                    Abc_ObjAddFanin(pNewNode, GetFanin(pNode, faninId));
                Abc_Obj_t * pSub = nullptr;
                if (sop == "1-1 1\n-11 1\n11- 1\n") { // CO=A B+B CI+A CI
                    auto pGate = Mio_LibraryReadGateByName(pLib, "MAOI222D0BWP7T30P140HVT", nullptr);
                    assert(pGate != nullptr);
                    pNewNode->pData = pGate;
                    pSub = Abc_NtkCreateNodeInv(GetNet(), pNewNode);
                }
                else if (sop == "100 1\n010 1\n111 1\n001 1\n") { // S=A^B^CI
                    auto pGate = Mio_LibraryReadGateByName(pLib, "XOR3D1BWP7T30P140HVT", nullptr);
                    assert(pGate != nullptr);
                    pNewNode->pData = pGate;
                    pSub = pNewNode;
                }
                else {
                    cout << sop;
                    assert(0);
                }
                assert(pSub != nullptr);
                cout << "replace " << pNode << " with new node " << pSub << endl;
                TransfFanout(pNode, pSub);
                isUpd = true;
            }
        }
    }
    return isUpd;
}


void NetMan::ProcHalfAndFullAddNew() {
    // special processing for half/full adder
    if (GetNetType() != NET_TYPE::GATE)
        return;
    unordered_set <int> vis;
    vector <int> targNodes;
    for (int iNode = 0; iNode < GetIdMaxPlus1(); ++iNode) {
        if (!IsNode(iNode))
            continue;
        if (vis.count(iNode))
            continue;
        vis.emplace(iNode);
        auto pNode = GetObj(iNode);
        auto pTwin = GetTwinNode(pNode);
        if (pTwin == nullptr)
            continue;
        vis.emplace(pTwin->Id);
        if (GetGateName(pNode).find("HA1") != -1)
            targNodes.emplace_back(iNode);
        else if (GetGateName(pNode).find("FA1") != -1)
            targNodes.emplace_back(iNode);
        else
            assert(0);
    }

    for (int targId: targNodes) {
        auto pNode = GetObj(targId);
        auto pTwin = GetTwinNode(pNode);
        assert(pTwin != nullptr);
        if (GetGateName(pNode).find("HA1") != -1) {
            // print
            // PrintObj(pNode, true); 
            auto sop0 = string(Mio_GateReadSop((Mio_Gate_t *)pNode->pData));
            // cout << sop0;
            // PrintObj(pTwin, true);
            auto sop1 = string(Mio_GateReadSop((Mio_Gate_t *)pTwin->pData));
            // cout << sop1;
            // cout << endl;

            // pNode sop0 S, pTwin sop1 CO
            assert(sop0 == "10 1\n01 1\n" && sop1 == "11 1\n");
            vector <Abc_Obj_t *> fanins;
            int nFanin = GetFaninNum(pNode);
            assert(nFanin == GetFaninNum(pTwin) && nFanin == 2);
            for (int iFanin = 0; iFanin < nFanin; ++iFanin) {
                assert(GetFanin(pNode, iFanin) == GetFanin(pTwin, iFanin));
                fanins.emplace_back(GetFanin(pNode, iFanin));
            }

            // create gates
            auto pNodeCo = CreateGate(std::vector <Abc_Obj_t *> ({fanins[0], fanins[1]}), "CKAN2D1BWP7T30P140HVT");
            auto pNodeN6 = CreateGate(std::vector <Abc_Obj_t *> ({fanins[0], fanins[1]}), "NR2D0BWP7T30P140HVT");
            auto pNodeS = CreateGate(std::vector <Abc_Obj_t *> ({pNodeCo, pNodeN6}), "NR2D0BWP7T30P140HVT");

            // cout << "replace " << pTwin << " with new node " << pNodeCo << endl;
            TransfFanout(pTwin, pNodeCo);
            // cout << "replace " << pNode << " with new node " << pNodeS << endl;
            TransfFanout(pNode, pNodeS);
        }
        else if (GetGateName(pNode).find("FA1") != -1) {
            // print
            // PrintObj(pNode, true); 
            auto sop0 = string(Mio_GateReadSop((Mio_Gate_t *)pNode->pData));
            // cout << sop0;
            // PrintObj(pTwin, true);
            auto sop1 = string(Mio_GateReadSop((Mio_Gate_t *)pTwin->pData));
            // cout << sop1;
            // cout << endl;

            // pNode sop0 S, pTwin sop1 CO
            assert(sop0 == "100 1\n010 1\n111 1\n001 1\n" && sop1 == "1-1 1\n-11 1\n11- 1\n");
            vector <Abc_Obj_t *> fanins;
            int nFanin = GetFaninNum(pNode);
            assert(nFanin == GetFaninNum(pTwin) && nFanin == 3);
            for (int iFanin = 0; iFanin < nFanin; ++iFanin) {
                assert(GetFanin(pNode, iFanin) == GetFanin(pTwin, iFanin));
                fanins.emplace_back(GetFanin(pNode, iFanin));
            }

            // create gates
            auto pNodeN6 = CreateGate(std::vector <Abc_Obj_t *> ({fanins[1], fanins[2], fanins[1], fanins[2]}), "MOAI22D0BWP7T30P140HVT");
            auto pNodeS = CreateGate(std::vector <Abc_Obj_t *> ({fanins[0], pNodeN6, fanins[0], pNodeN6}), "MOAI22D0BWP7T30P140HVT");
            auto pNodeCo = CreateGate(std::vector <Abc_Obj_t *> ({fanins[1], fanins[2], fanins[0], pNodeN6}), "OA22D0BWP7T30P140HVT");

            // cout << "replace " << pTwin << " with new node " << pNodeCo << endl;
            TransfFanout(pTwin, pNodeCo);
            // cout << "replace " << pNode << " with new node " << pNodeS << endl;
            TransfFanout(pNode, pNodeS);
        }
    }
    CleanUp();
}


abc::Abc_Obj_t* NetMan::CreateNode(const std::vector<abc::Abc_Obj_t*>& pFanins, const std::string& sop) {
    auto pNewNode = abc::Abc_NtkCreateNode(GetNet());
    for (const auto& pFanin: pFanins)
        Abc_ObjAddFanin(pNewNode, pFanin);
    #ifdef DEBUG
    assert(GetNetType() == NET_TYPE::SOP);
    #endif
    pNewNode->pData = Abc_SopRegister((Mem_Flex_t *)GetNet()->pManFunc, sop.c_str());
    return pNewNode;
}


int NetMan::CreateNode(const std::vector <int>& faninIds, const std::string& sop) {
    auto pNewNode = abc::Abc_NtkCreateNode(GetNet());
    for (const auto& faninId: faninIds)
        Abc_ObjAddFanin(pNewNode, GetObj(faninId));
    #ifdef DEBUG
    assert(GetNetType() == NET_TYPE::SOP);
    #endif
    pNewNode->pData = Abc_SopRegister((Mem_Flex_t *)GetNet()->pManFunc, sop.c_str());
    return pNewNode->Id;
}


abc::Abc_Obj_t * NetMan::CreateGate(vector <Abc_Obj_t *> && fanins, const std::string & gateName) {
    auto pLib = (Mio_Library_t *)Abc_FrameReadLibGen();
    auto pGate = Mio_LibraryReadGateByName(pLib, const_cast <char *> (gateName.c_str()), nullptr);
    assert(pGate != nullptr);
    auto pNewNode = Abc_NtkCreateNode(GetNet());
    for (const auto & fanin: fanins)
        Abc_ObjAddFanin(pNewNode, fanin);
    pNewNode->pData = pGate;
    return pNewNode;
}


Abc_Obj_t * NetMan::DupObj(Abc_Obj_t * pObj, const char* pSuff) {
    Abc_Obj_t * pObjNew;
    // create the new object
    pObjNew = Abc_NtkCreateObj( pNtk, (Abc_ObjType_t)pObj->Type );
    // transfer names of the terminal objects
    Abc_ObjAssignName(pObjNew, Abc_ObjName(pObj), const_cast<char*>(pSuff));
    // copy functionality/names
    if ( Abc_ObjIsNode(pObj) ) // copy the function if functionality is compatible
    {
        if ( pNtk->ntkFunc == pObj->pNtk->ntkFunc ) 
        {
            if ( Abc_NtkIsStrash(pNtk) ) 
            {}
            else if ( Abc_NtkHasSop(pNtk) || Abc_NtkHasBlifMv(pNtk) )
                pObjNew->pData = Abc_SopRegister( (Mem_Flex_t *)pNtk->pManFunc, (char *)pObj->pData );
#ifdef ABC_USE_CUDD
            else if ( Abc_NtkHasBdd(pNtk) )
                pObjNew->pData = Cudd_bddTransfer((DdManager *)pObj->pNtk->pManFunc, (DdManager *)pNtk->pManFunc, (DdNode *)pObj->pData), Cudd_Ref((DdNode *)pObjNew->pData);
#endif
            else if ( Abc_NtkHasAig(pNtk) )
                pObjNew->pData = Hop_Transfer((Hop_Man_t *)pObj->pNtk->pManFunc, (Hop_Man_t *)pNtk->pManFunc, (Hop_Obj_t *)pObj->pData, Abc_ObjFaninNum(pObj));
            else if ( Abc_NtkHasMapping(pNtk) )
                pObjNew->pData = pObj->pData, pNtk->nBarBufs2 += !pObj->pData;
            else assert( 0 );
        }
    }
    else if ( Abc_ObjIsNet(pObj) ) // copy the name
    {
    }
    else if ( Abc_ObjIsLatch(pObj) ) // copy the reset value
        pObjNew->pData = pObj->pData;
    pObjNew->fPersist = pObj->fPersist;
    // transfer HAIG
//    pObjNew->pEquiv = pObj->pEquiv;
    // remember the new node in the old node
    pObj->pCopy = pObjNew;
    return pObjNew;
}


void NetMan::LimFanout() {
    assert(GetNetType() == NET_TYPE::SOP);
    auto nodes = CalcTopoOrdOfIds();
    for (int id: nodes) {
        int nFo = GetFanoutNum(id);
        if (nFo <= 2)
            continue;
        // PrintObj(id, true);
        list<Abc_Obj_t*> dealtPFos;
        for (int iFo = 1; iFo < nFo; ++iFo)
            dealtPFos.emplace_back(GetFanout(id, iFo));
        auto pDealt = GetObj(id);
        while (!dealtPFos.empty()) {
            auto pBuf = CreateBuf(pDealt);
            Rename(pBuf, (GetName(pDealt) + "_b").c_str());
            for (auto pFo: dealtPFos)
                Abc_ObjPatchFanin(pFo, pDealt, pBuf);
            dealtPFos.pop_front();
            pDealt = pBuf;
        }
    }
}


void GlobStartAbc() {
    Abc_Start();
    AbcMan abcMan;
    abcMan.LoadAlias();
}


void GlobStopAbc() {
    Abc_Stop();
}