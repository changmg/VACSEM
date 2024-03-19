#include "dsat.h"


using abc::Abc_Ntk_t;
using abc::ABC_INT64_T;
using abc::Aig_Man_t;
using abc::sat_solver;
using abc::Cnf_Dat_t;
using abc::abctime;
using abc::Vec_Int_t;
using abc::l_True;
using abc::l_False;
using abc::l_Undef;
using abc::lit;
using abc::lbool;

using std::string;
using std::cout;
using std::endl;


static const int varX  = 3;
static inline int      sat_solver_dl(sat_solver* s)                { return veci_size(&s->trail_lim); }
static inline void    var_set_value (sat_solver* s, int v, int val)   { s->assigns[v] = val;   }
static inline void    var_set_polar (sat_solver* s, int v, int pol)   { s->polarity[v] = pol;  }


static inline void order_update(sat_solver* s, int v) { // updateorder 
    int*    orderpos = s->orderpos;
    int*    heap     = veci_begin(&s->order);
    int     i        = orderpos[v];
    int     x        = heap[i];
    int     parent   = (i - 1) / 2;

    assert(s->orderpos[v] != -1);

    while (i != 0 && s->activity[x] > s->activity[heap[parent]]){
        heap[i]           = heap[parent];
        orderpos[heap[i]] = i;
        i                 = parent;
        parent            = (i - 1) / 2;
    }

    heap[i]     = x;
    orderpos[x] = i;
}


static inline void order_unassigned(sat_solver* s, int v) { // undoorder 
    int* orderpos = s->orderpos;
    if (orderpos[v] == -1){
        orderpos[v] = veci_size(&s->order);
        veci_push(&s->order,v);
        order_update(s,v);
//printf( "+%d ", v );
    }
}


static void sat_solver_canceluntil(sat_solver* s, int level) {
    int      bound;
    int      lastLev;
    int      c;
    
    if (sat_solver_dl(s) <= level)
        return;

    assert( veci_size(&s->trail_lim) > 0 );
    bound   = (veci_begin(&s->trail_lim))[level];
    lastLev = (veci_begin(&s->trail_lim))[veci_size(&s->trail_lim)-1];

    ////////////////////////////////////////
    // added to cancel all assignments
//    if ( level == -1 )
//        bound = 0;
    ////////////////////////////////////////

    for (c = s->qtail-1; c >= bound; c--) {
        int     x  = abc::lit_var(s->trail[c]);
        var_set_value(s, x, varX);
        s->reasons[x] = 0;
        if ( c < lastLev )
            var_set_polar( s, x, !abc::lit_sign(s->trail[c]) );
    }
    //printf( "\n" );

    for (c = s->qhead-1; c >= bound; c--)
        order_unassigned(s,abc::lit_var(s->trail[c]));

    s->qhead = s->qtail = bound;
    veci_resize(&s->trail_lim,level);
}


int my_sat_solver_solve(sat_solver* s, lit* begin, lit* end, ABC_INT64_T nConfLimit, ABC_INT64_T nInsLimit, ABC_INT64_T nConfLimitGlobal, ABC_INT64_T nInsLimitGlobal) {
    lbool status;
    lit * i;
    ////////////////////////////////////////////////
    if ( s->fSolved )
    {
        assert(!s->pStore);
        return l_False;
    }
    ////////////////////////////////////////////////

    if ( s->fVerbose )
        printf( "Running SAT solver with parameters %d and %d and %d.\n", s->nLearntStart, s->nLearntDelta, s->nLearntRatio );

    abc::sat_solver_set_resource_limits( s, nConfLimit, nInsLimit, nConfLimitGlobal, nInsLimitGlobal );

    // Perform assumptions:
    s->root_level = 0;
    for ( i = begin; i < end; i++ ) {
        if ( !abc::sat_solver_push(s, *i) ) {
            sat_solver_canceluntil(s,0);
            s->root_level = 0;
            return l_False;
        }
    }
    assert(s->root_level == sat_solver_dl(s));

    status = abc::sat_solver_solve_internal(s);

    sat_solver_canceluntil(s,0);
    s->root_level = 0;

    ////////////////////////////////////////////////
    if ( status == l_False && s->pStore )
    {
        assert(0);
    }
    ////////////////////////////////////////////////
    return status;
}


int My_Fra_FraigSat( Aig_Man_t * pMan, ABC_INT64_T nConfLimit, ABC_INT64_T nInsLimit, int nLearnedStart, int nLearnedDelta, int nLearnedPerce, int fFlipBits, int fAndOuts, int fNewSolver, int fVerbose ) {
    assert(!fNewSolver);
    sat_solver * pSat;
    Cnf_Dat_t * pCnf;
    int status, RetValue = 0;
    abctime clk = abc::Abc_Clock();
    Vec_Int_t * vCiIds;

    assert( Aig_ManRegNum(pMan) == 0 );
    pMan->pData = NULL;

    // derive CNF
    pCnf = abc::Cnf_Derive( pMan, Aig_ManCoNum(pMan) );

    assert(!fFlipBits); 

    if ( fVerbose ) {
        printf( "CNF stats: Vars = %6d. Clauses = %7d. Literals = %8d. ", pCnf->nVars, pCnf->nClauses, pCnf->nLiterals );
        abc::Abc_PrintTime( 1, "Time", abc::Abc_Clock() - clk );
    }

    // convert into SAT solver
    pSat = (sat_solver *)Cnf_DataWriteIntoSolver( pCnf, 1, 0 );
    if ( pSat == NULL ) {
        Cnf_DataFree( pCnf );
        return 1;
    }

    assert(!nLearnedStart);
    assert(!nLearnedDelta);
    assert(!nLearnedPerce);
    if (fVerbose) 
        pSat->fVerbose = fVerbose;

    if ( fAndOuts )
        assert(0);
    else {
        // add the OR clause for the outputs
        if ( !Cnf_DataWriteOrClause( pSat, pCnf ) )
        {
            sat_solver_delete( pSat );
            Cnf_DataFree( pCnf );
            return 1;
        }
    }
    vCiIds = Cnf_DataCollectPiSatNums( pCnf, pMan );
    Cnf_DataFree( pCnf );

    // simplify the problem
    clk = abc::Abc_Clock();
    status = sat_solver_simplify(pSat);
    if ( status == 0 )
    {
        Vec_IntFree( vCiIds );
        sat_solver_delete( pSat );
        return 1;
    }

    // solve the miter
    clk = abc::Abc_Clock();
    status = my_sat_solver_solve( pSat, NULL, NULL, (ABC_INT64_T)nConfLimit, (ABC_INT64_T)nInsLimit, (ABC_INT64_T)0, (ABC_INT64_T)0 );
    if ( status == l_Undef ) {
        RetValue = -1;
    }
    else if ( status == l_True ) {
        RetValue = 0;
    }
    else if ( status == l_False ) {
        RetValue = 1;
    }
    else
        assert( 0 );

    // if ( status == l_True ) {
    //     pMan->pData = Sat_SolverGetModel( pSat, vCiIds->pArray, vCiIds->nSize );
    // }
    // free the sat_solver
    if ( fVerbose )
        Sat_SolverPrintStats( stdout, pSat );
    sat_solver_delete( pSat );
    Vec_IntFree( vCiIds );
    return RetValue;
}


extern "C" {Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );}
int My_Abc_NtkDSat( Abc_Ntk_t * pNtk, ABC_INT64_T nConfLimit, ABC_INT64_T nInsLimit, int nLearnedStart, int nLearnedDelta, int nLearnedPerce, int fAlignPol, int fAndOuts, int fNewSolver, int fVerbose )
{
    Aig_Man_t * pMan;
    int RetValue;//, clk = Abc_Clock();
    assert( Abc_NtkIsStrash(pNtk) );
    assert( Abc_NtkLatchNum(pNtk) == 0 );
//    assert( Abc_NtkPoNum(pNtk) == 1 );
    pMan = Abc_NtkToDar( pNtk, 0, 0 );
    RetValue = My_Fra_FraigSat( pMan, nConfLimit, nInsLimit, nLearnedStart, nLearnedDelta, nLearnedPerce, fAlignPol, fAndOuts, fNewSolver, fVerbose ); 
    // pNtk->pModel = (int *)pMan->pData, pMan->pData = NULL;
    Aig_ManStop( pMan );
    return RetValue;
}


void DSATTest(cmdline::parser& opt) {
    AbcMan abc;
    abc.Comm("miter " + opt.get<string>("exact") + " " + opt.get<string>("appr"));
    abc.Synth(ORIENT::AREA, true);
    abc.Comm("logic; aig;");

    NetMan mit(abc.GetNet(), true);
    assert(mit.GetPoNum() == 1);
    // insert an inverter
    auto poDriv = mit.GetPoDriv(0);
    mit.DelObj(mit.GetPo(0));
    auto pInv = mit.CreateInv(poDriv);
    mit.CreatePo(pInv, "F");
    // #SAT
    mit.Comm("st;");
    clock_t startTime = clock();
    int RetValue = My_Abc_NtkDSat(mit.GetNet(), 0, 0, 0, 0, 0, 0, 0, 0, 1);
    ll totTime = clock() - startTime;
    if (RetValue == -1)
        cout << "UNDECIDED      ";
    else if (RetValue == 0)
        cout << "SATISFIABLE    ";
    else
        cout << "UNSATISFIABLE  ";
    cout << totTime / pow(10, 6) << "s" << endl;
}