#include "cnf.h"


using std::string;
using std::cout;
using std::endl;
using abc::Aig_Obj_t;
using abc::Abc_Ntk_t;
using abc::Aig_Man_t;
using abc::Cnf_Dat_t;
using abc::Cnf_Man_t;
using abc::Cnf_Cut_t;
using abc::Vec_Ptr_t;
using abc::Vec_Int_t;
using abc::Aig_MmFixed_t;
using abc::abctime;


static Cnf_Man_t * s_pManCnf = NULL;


Cnf_Man_t * My_Cnf_ManStart()
{
    Cnf_Man_t * p;
    int i;
    // allocate the manager
    p = ABC_ALLOC( Cnf_Man_t, 1 );
    memset( p, 0, sizeof(Cnf_Man_t) );
    // derive internal data structures
    abc::Cnf_ReadMsops( &p->pSopSizes, &p->pSops );
    // allocate memory manager for cuts
    p->pMemCuts = abc::Aig_MmFlexStart();
    p->nMergeLimit = 10;
    // allocate temporary truth tables
    p->pTruths[0] = ABC_ALLOC( unsigned, 4 * abc::Abc_TruthWordNum(p->nMergeLimit) );
    for ( i = 1; i < 4; i++ )
        p->pTruths[i] = p->pTruths[i-1] + abc::Abc_TruthWordNum(p->nMergeLimit);
    p->vMemory = abc::Vec_IntAlloc( 1 << 18 );
    return p;
}


void My_Cnf_ManPrepare()
{
    if ( s_pManCnf == NULL )
    {
//        printf( "\n\nCreating CNF manager!!!!!\n\n" );
        s_pManCnf = My_Cnf_ManStart();
    }
}


extern "C" int Cnf_SopCountLiterals( char * pSop, int nCubes );
extern "C" int Cnf_IsopCountLiterals( Vec_Int_t * vIsop, int nVars );
extern "C" int Cnf_IsopWriteCube( int Cube, int nVars, int * pVars, int * pLiterals );
Cnf_Dat_t * My_Cnf_ManWriteCnf( Cnf_Man_t * p, Vec_Ptr_t * vMapped, int nOutputs )
{
    int fChangeVariableOrder = 0; // should be set to 0 to improve performance
    Aig_Obj_t * pObj;
    Cnf_Dat_t * pCnf;
    Cnf_Cut_t * pCut;
    Vec_Int_t * vCover, * vSopTemp;
    int OutVar, PoVar, pVars[32], * pLits, ** pClas;
    unsigned uTruth;
    int i, k, nLiterals, nClauses, Cube, Number;

    // count the number of literals and clauses
    nLiterals = 1 + Aig_ManCoNum( p->pManAig ) + 3 * nOutputs;
    nClauses = 1 + Aig_ManCoNum( p->pManAig ) + nOutputs;
    Vec_PtrForEachEntry( Aig_Obj_t *, vMapped, pObj, i )
    {
        assert( Aig_ObjIsNode(pObj) );
        pCut = Cnf_ObjBestCut( pObj );

        // positive polarity of the cut
        if ( pCut->nFanins < 5 )
        {
            uTruth = 0xFFFF & *Cnf_CutTruth(pCut);
            nLiterals += Cnf_SopCountLiterals( p->pSops[uTruth], p->pSopSizes[uTruth] ) + p->pSopSizes[uTruth];
            assert( p->pSopSizes[uTruth] >= 0 );
            nClauses += p->pSopSizes[uTruth];
        }
        else
        {
            nLiterals += Cnf_IsopCountLiterals( pCut->vIsop[1], pCut->nFanins ) + abc::Vec_IntSize(pCut->vIsop[1]);
            nClauses += abc::Vec_IntSize(pCut->vIsop[1]);
        }
        // negative polarity of the cut
        if ( pCut->nFanins < 5 )
        {
            uTruth = 0xFFFF & ~*Cnf_CutTruth(pCut);
            nLiterals += Cnf_SopCountLiterals( p->pSops[uTruth], p->pSopSizes[uTruth] ) + p->pSopSizes[uTruth];
            assert( p->pSopSizes[uTruth] >= 0 );
            nClauses += p->pSopSizes[uTruth];
        }
        else
        {
            nLiterals += Cnf_IsopCountLiterals( pCut->vIsop[0], pCut->nFanins ) + abc::Vec_IntSize(pCut->vIsop[0]);
            nClauses += abc::Vec_IntSize(pCut->vIsop[0]);
        }
//printf( "%d ", nClauses-(1 + Aig_ManCoNum( p->pManAig )) );
    }
//printf( "\n" );

    // allocate CNF
    pCnf = ABC_CALLOC( Cnf_Dat_t, 1 );
    pCnf->pMan = p->pManAig;
    pCnf->nLiterals = nLiterals;
    pCnf->nClauses = nClauses;
    pCnf->pClauses = ABC_ALLOC( int *, nClauses + 1 );
    pCnf->pClauses[0] = ABC_ALLOC( int, nLiterals );
    pCnf->pClauses[nClauses] = pCnf->pClauses[0] + nLiterals;
    // create room for variable numbers
    pCnf->pVarNums = ABC_ALLOC( int, Aig_ManObjNumMax(p->pManAig) );
//    memset( pCnf->pVarNums, 0xff, sizeof(int) * Aig_ManObjNumMax(p->pManAig) );
    for ( i = 0; i < Aig_ManObjNumMax(p->pManAig); i++ )
        pCnf->pVarNums[i] = -1;

    // if ( !fChangeVariableOrder )
    // {
    //     // assign variables to the last (nOutputs) POs
    //     Number = 1;
    //     if ( nOutputs )
    //         assert(0);
    //     // assign variables to the internal nodes
    //     Vec_PtrForEachEntry( Aig_Obj_t *, vMapped, pObj, i )
    //         pCnf->pVarNums[pObj->Id] = Number++;
    //     // assign variables to the PIs and constant node
    //     Aig_ManForEachCi( p->pManAig, pObj, i ) {
    //         pCnf->pVarNums[pObj->Id] = Number++;
    //         cout << "PI " << pObj->Id << ", cnf ID " << pCnf->pVarNums[pObj->Id] << endl;
    //     }
    //     pCnf->pVarNums[Aig_ManConst1(p->pManAig)->Id] = Number++;
    //     pCnf->nVars = Number;
    // }
    // else
    //     assert(0);

    if ( !fChangeVariableOrder )
    {
        // assign variables to the last (nOutputs) POs
        Number = 1;
        if ( nOutputs )
            assert(0);
        // assign variables to the PIs and constant node
        Aig_ManForEachCi( p->pManAig, pObj, i ) {
            pCnf->pVarNums[pObj->Id] = Number++;
            cout << "PI " << pObj->Id << ", cnf ID " << pCnf->pVarNums[pObj->Id] << endl;
        }
        // assign variables to the internal nodes
        Vec_PtrForEachEntry( Aig_Obj_t *, vMapped, pObj, i )
            pCnf->pVarNums[pObj->Id] = Number++;
        pCnf->pVarNums[Aig_ManConst1(p->pManAig)->Id] = Number++;
        pCnf->nVars = Number;
    }
    else
        assert(0);

    // assign the clauses
    vSopTemp = abc::Vec_IntAlloc( 1 << 16 );
    pLits = pCnf->pClauses[0];
    pClas = pCnf->pClauses;
    Vec_PtrForEachEntry( Aig_Obj_t *, vMapped, pObj, i )
    {
        pCut = Cnf_ObjBestCut( pObj );

        // save variables of this cut
        OutVar = pCnf->pVarNums[ pObj->Id ];
        for ( k = 0; k < (int)pCut->nFanins; k++ )
        {
            pVars[k] = pCnf->pVarNums[ pCut->pFanins[k] ];
            assert( pVars[k] <= Aig_ManObjNumMax(p->pManAig) );
        }

        // positive polarity of the cut
        if ( pCut->nFanins < 5 )
        {
            uTruth = 0xFFFF & *Cnf_CutTruth(pCut);
            Cnf_SopConvertToVector( p->pSops[uTruth], p->pSopSizes[uTruth], vSopTemp );
            vCover = vSopTemp;
        }
        else
            vCover = pCut->vIsop[1];
        Vec_IntForEachEntry( vCover, Cube, k )
        {
            *pClas++ = pLits;
            *pLits++ = 2 * OutVar; 
            pLits += Cnf_IsopWriteCube( Cube, pCut->nFanins, pVars, pLits );
        }

        // negative polarity of the cut
        if ( pCut->nFanins < 5 )
        {
            uTruth = 0xFFFF & ~*Cnf_CutTruth(pCut);
            Cnf_SopConvertToVector( p->pSops[uTruth], p->pSopSizes[uTruth], vSopTemp );
            vCover = vSopTemp;
        }
        else
            vCover = pCut->vIsop[0];
        Vec_IntForEachEntry( vCover, Cube, k )
        {
            *pClas++ = pLits;
            *pLits++ = 2 * OutVar + 1; 
            pLits += Cnf_IsopWriteCube( Cube, pCut->nFanins, pVars, pLits );
        }
    }
    Vec_IntFree( vSopTemp );
 
    // write the constant literal
    OutVar = pCnf->pVarNums[ Aig_ManConst1(p->pManAig)->Id ];
    assert( OutVar <= Aig_ManObjNumMax(p->pManAig) );
    *pClas++ = pLits;
    *pLits++ = 2 * OutVar; 

    // write the output literals
    Aig_ManForEachCo( p->pManAig, pObj, i )
    {
        OutVar = pCnf->pVarNums[ Aig_ObjFanin0(pObj)->Id ];
        if ( i < Aig_ManCoNum(p->pManAig) - nOutputs )
        {
            *pClas++ = pLits;
            *pLits++ = 2 * OutVar + Aig_ObjFaninC0(pObj); 
        }
        else
        {
            PoVar = pCnf->pVarNums[ pObj->Id ];
            // first clause
            *pClas++ = pLits;
            *pLits++ = 2 * PoVar; 
            *pLits++ = 2 * OutVar + !Aig_ObjFaninC0(pObj); 
            // second clause
            *pClas++ = pLits;
            *pLits++ = 2 * PoVar + 1; 
            *pLits++ = 2 * OutVar + Aig_ObjFaninC0(pObj); 
        }
    }

    // verify that the correct number of literals and clauses was written
    assert( pLits - pCnf->pClauses[0] == nLiterals );
    assert( pClas - pCnf->pClauses == nClauses );
//Cnf_DataPrint( pCnf, 1 );
    return pCnf;
}


Cnf_Dat_t * My_Cnf_DeriveWithMan( Cnf_Man_t * p, Aig_Man_t * pAig, int nOutputs )
{
    Cnf_Dat_t * pCnf;
    Vec_Ptr_t * vMapped;
    Aig_MmFixed_t * pMemCuts;
    abctime clk;
    // connect the managers
    p->pManAig = pAig;

    // generate cuts for all nodes, assign cost, and find best cuts
clk = abc::Abc_Clock();
    pMemCuts = Dar_ManComputeCuts( pAig, 10, 0, 0 );
p->timeCuts = abc::Abc_Clock() - clk;

    // find the mapping
clk = abc::Abc_Clock();
    Cnf_DeriveMapping( p );
p->timeMap = abc::Abc_Clock() - clk;

    // convert it into CNF
clk = abc::Abc_Clock();
    Cnf_ManTransferCuts( p );
    vMapped = Cnf_ManScanMapping( p, 1, 1 );
    pCnf = My_Cnf_ManWriteCnf( p, vMapped, nOutputs );
    Vec_PtrFree( vMapped );
    Aig_MmFixedStop( pMemCuts, 0 );
p->timeSave = abc::Abc_Clock() - clk;

   // reset reference counters
    Aig_ManResetRefs( pAig );
    return pCnf;
}


Cnf_Dat_t * My_Cnf_Derive( Aig_Man_t * pAig, int nOutputs )
{
    My_Cnf_ManPrepare();
    return My_Cnf_DeriveWithMan( s_pManCnf, pAig, nOutputs );
}


extern "C" Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
void My_NtkDarToCnf(abc::Abc_Ntk_t * pNtk, char * pFileName, int fFastAlgo, int fChangePol, int fVerbose)
{
    Aig_Man_t * pMan;
    Cnf_Dat_t * pCnf;
    abctime clk = abc::Abc_Clock();
    assert( Abc_NtkIsStrash(pNtk) );

    // convert to the AIG manager
    pMan = Abc_NtkToDar( pNtk, 0, 0 );
    if ( pMan == NULL )
        return;
    if ( !Aig_ManCheck( pMan ) )
    {
        abc::Abc_Print( 1, "Abc_NtkDarToCnf: AIG check has failed.\n" );
        Aig_ManStop( pMan );
        return;
    }
    // perform balance
    if ( fVerbose )
    Aig_ManPrintStats( pMan );

    // derive CNF
    if ( fFastAlgo )
        assert(0);
    else
        pCnf = My_Cnf_Derive( pMan, 0 );

    // adjust polarity
    if ( fChangePol )
        Cnf_DataTranformPolarity( pCnf, 0 );

    // print stats
    {
        abc::Abc_Print( 1, "CNF stats: Vars = %6d. Clauses = %7d. Literals = %8d.   ", pCnf->nVars, pCnf->nClauses, pCnf->nLiterals );
        abc::Abc_PrintTime( 1, "Time", abc::Abc_Clock() - clk );
    }

    // write CNF into a file
    Cnf_DataWriteIntoFile( pCnf, pFileName, 0, NULL, NULL );
    Cnf_DataFree( pCnf );
    abc::Cnf_ManFree();
    Aig_ManStop( pMan );
}


NetMan BuildMit(NetMan& accNet, NetMan& appNet, NetMan& mitNet) {
    // init
    accNet.Strash(); appNet.Strash(); mitNet.Strash();
    assert(accNet.IsPIOSame(appNet));
    NetMan net;
    net.StartStrashNet();
    // copy accNet
    auto pNet = net.GetNet();
    auto pAccNet = accNet.GetNet();
    AbcObj* pObj = nullptr;
    int i = 0;
    Abc_NtkCleanCopy(pAccNet);
    Abc_AigConst1(pAccNet)->pCopy = Abc_AigConst1(pNet);
    std::unordered_map <string, AbcObj*> name2PI;
    Abc_NtkForEachPi(pAccNet, pObj, i) {
        Abc_NtkDupObj(pNet, pObj, 0);
        RenameAbcObj(pObj->pCopy, string(Abc_ObjName(pObj)));
        name2PI[string(Abc_ObjName(pObj))] = pObj->pCopy;
    }
    Abc_AigForEachAnd(pAccNet, pObj, i) {
        pObj->pCopy = Abc_AigAnd((abc::Abc_Aig_t *)pNet->pManFunc, Abc_ObjChild0Copy(pObj), Abc_ObjChild1Copy(pObj));
        RenameAbcObj(pObj->pCopy, string(Abc_ObjName(pObj)) + "_acc");
    }
    // copy appNet
    auto pAppNet = appNet.GetNet();
    Abc_NtkCleanCopy(pAppNet);
    Abc_AigConst1(pAppNet)->pCopy = Abc_AigConst1(pNet);
    Abc_NtkForEachPi(pAppNet, pObj, i) {
        assert(name2PI.count(string(Abc_ObjName(pObj))));
        pObj->pCopy = name2PI[string(Abc_ObjName(pObj))];
    }
    Abc_AigForEachAnd(pAppNet, pObj, i) {
        pObj->pCopy = Abc_AigAnd((abc::Abc_Aig_t *)pNet->pManFunc, Abc_ObjChild0Copy(pObj), Abc_ObjChild1Copy(pObj));
        RenameAbcObj(pObj->pCopy, string(Abc_ObjName(pObj)) + "_app");
    }
    // copy miter
    auto pMitNet = mitNet.GetNet();
    int nPo = accNet.GetPoNum();
    assert(appNet.GetPoNum() == nPo && mitNet.GetPiNum() == nPo * 2);
    Abc_NtkCleanCopy(pMitNet);
    Abc_AigConst1(pMitNet)->pCopy = Abc_AigConst1(pNet);
    Abc_NtkForEachPo(pAccNet, pObj, i)
        Abc_NtkPi(pMitNet, i)->pCopy = Abc_ObjChild0Copy(pObj);
    Abc_NtkForEachPo(pAppNet, pObj, i)
        Abc_NtkPi(pMitNet, i + nPo)->pCopy = Abc_ObjChild0Copy(pObj);
    Abc_AigForEachAnd(pMitNet, pObj, i) {
        pObj->pCopy = abc::Abc_AigAnd((abc::Abc_Aig_t *)pNet->pManFunc, Abc_ObjChild0Copy(pObj), Abc_ObjChild1Copy(pObj));
        // if (!Abc_ObjIsComplement(pObj->pCopy))
        //     RenameAbcObj(pObj->pCopy, string(Abc_ObjName(pObj)) + "_mit");
    }
    Abc_NtkForEachPo(pMitNet, pObj, i)
        Abc_NtkDupObj(pNet, pObj, 1);
    Abc_NtkForEachPo(pMitNet, pObj, i)
        Abc_ObjAddFanin(pObj->pCopy, Abc_ObjChild0Copy(pObj));
    // cout << "miter net" << endl;
    net.PrintStat();
    // net.Print(true); cout << endl;
    return net;
}


void MiterToCnf(NetMan& mit, string outp, int iOutp) {
    if (iOutp != -1) {
        // deal with name
        int pos = outp.find(".cnf");
        assert(pos != -1);
        std::ostringstream oss("");
        oss << "_" << iOutp << ".cnf";
        outp.replace(pos, 5, oss.str());
    }

    // dump BLIF
    string blifName = outp;
    int pos = blifName.find(".cnf");
    assert(pos != -1);
    blifName.replace(pos, 5, ".blif");
    cout << "output blif file = " << blifName << endl;
    cout << "output CNF file = " << outp << endl;
    mit.Comm("write_blif " + blifName);
    assert(mit.GetPoNum() == 1);

    if (mit.IsConst0(mit.GetPoDrivId(0))) {
        FILE* fp = fopen((outp + "_const0").c_str(), "w");
        if (fp == nullptr) {
            cout << "open " << outp << " failed" << endl;
            assert(0);
        }
        fprintf(fp, "c nIsolatedPIs %d\n", mit.GetPiNum());
        fprintf(fp, "c maxPI 0\n");
        fprintf(fp, "p cnf 1 2\n");
        fprintf(fp, "1 0\n");
        fprintf(fp, "-1 0\n");
        fclose(fp);
        return;
    }
    if (mit.IsConst1(mit.GetPoDrivId(0))) {
        FILE* fp = fopen((outp + "_const1").c_str(), "w");
        if (fp == nullptr) {
            cout << "open " << outp << " failed" << endl;
            assert(0);
        }
        fprintf(fp, "c nIsolatedPIs %d\n", mit.GetPiNum());
        fprintf(fp, "c maxPI 0\n");
        fprintf(fp, "p cnf 1 1\n");
        fprintf(fp, "1 -1 0\n");
        fclose(fp);
        return;
    }

    // id mapping, clause counting, inverter mapping
    int cnfId = 0;
    int nIsolatedPIs = 0;
    int maxPI = 0;
    std::unordered_map<int, int> abcIdToCnfId;
    std::unordered_map<int, int> cnfIdToAbcId;
    for (int i = 0; i < mit.GetPiNum(); ++i) {
        auto pi = mit.GetPi(i);
        auto piId = mit.GetId(pi);
        if (!mit.GetFanoutNum(piId)) {
            ++nIsolatedPIs;
            continue;
        }
        ++cnfId;
        assert(!abcIdToCnfId.count(piId) && !cnfIdToAbcId.count(cnfId));
        abcIdToCnfId[piId] = cnfId;
        cnfIdToAbcId[cnfId] = piId;
        assert(!abcIdToCnfId.count(-piId) && !cnfIdToAbcId.count(-cnfId));
        abcIdToCnfId[-piId] = -cnfId;
        cnfIdToAbcId[-cnfId] = -piId;
    }
    maxPI = cnfId;
    assert(maxPI);

    // cout << "#isolated PIs = " << nIsolatedPI << endl;
    int nClause = 0;
    std::unordered_map<int, int> node_s_inp_inv;
    for (int i = 0; i <= mit.GetIdMax(); ++i) {
        if (mit.IsNode(i)) {
            auto gate = string(abc::Mio_GateReadName(static_cast <abc::Mio_Gate_t *> (mit.GetObj(i)->pData)));
            if (gate == "inv1") {
                auto fi0 = mit.GetFanin(i, 0);
                node_s_inp_inv[i] = mit.GetId(fi0);
                if (mit.IsNode(fi0)) {
                    auto fiGate = string(abc::Mio_GateReadName(static_cast <abc::Mio_Gate_t *> (mit.GetFanin(i, 0)->pData)));
                    assert(fiGate != "inv1" && fiGate != "buf");
                }
            }
            else {
                ++cnfId;
                assert(!abcIdToCnfId.count(i) && !cnfIdToAbcId.count(cnfId));
                abcIdToCnfId[i] = cnfId;
                cnfIdToAbcId[cnfId] = i;
                assert(!abcIdToCnfId.count(-i) && !cnfIdToAbcId.count(-cnfId));
                abcIdToCnfId[-i] = -cnfId;
                cnfIdToAbcId[-cnfId] = -i;
                if (gate == "and2")
                    nClause += 3;
                else
                    assert(0);
            }
        }
    }
    nClause += 1;

    // output cnf
    FILE* fp = fopen(outp.c_str(), "w");
    if (fp == nullptr) {
        cout << "open " << outp << " failed" << endl;
        assert(0);
    }
    fprintf(fp, "c nIsolatedPIs %d\n", nIsolatedPIs);
    fprintf(fp, "c maxPI %d\n", maxPI);
    fprintf(fp, "p cnf %d %d\n", cnfId, nClause);
    for (int i = 0; i <= mit.GetIdMax(); ++i) {
        if (mit.IsNode(i)) {
            auto gate = string(abc::Mio_GateReadName(static_cast <abc::Mio_Gate_t *> (mit.GetObj(i)->pData)));
            if (gate == "and2") {
                int x0 = mit.GetFaninId(i, 0);
                if (node_s_inp_inv.count(x0))
                    x0 = -node_s_inp_inv[x0];
                int x1 = mit.GetFaninId(i, 1);
                if (node_s_inp_inv.count(x1))
                    x1 = -node_s_inp_inv[x1];
                assert(abcIdToCnfId.count(x0) && abcIdToCnfId.count(x1) && abcIdToCnfId.count(i));
                int x0Lit = abcIdToCnfId[x0], x1Lit = abcIdToCnfId[x1], zLit = abcIdToCnfId[i];
                fprintf( fp, "c gate %d %d %d 0\n", zLit, x0Lit, x1Lit );
                fprintf(fp, "%d %d 0\n", x0Lit, -zLit); // x0 + \bar z
                fprintf(fp, "%d %d 0\n", x1Lit, -zLit); // x1 + \bar z
                fprintf(fp, "%d %d %d 0\n", -x0Lit, -x1Lit, zLit); // \bar x0 + \bar x1 + z
            }
        }
    }

    assert(!mit.IsCompl(mit.GetPoId(0)));
    int x0 = mit.GetPoDrivId(0);
    if (node_s_inp_inv.count(x0))
        x0 = -node_s_inp_inv[x0];
    assert(abcIdToCnfId.count(x0));
    int x0Lit = abcIdToCnfId[x0];
    fprintf(fp, "c PO\n");
    fprintf(fp, "%d 0\n", x0Lit);
    
    fclose(fp);
}