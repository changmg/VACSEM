#include "header.h"
#include "my_abc.h"
#include "cmdline.hpp"
#include "simulator.h"
#include "dsat.h"
#include "circ_sat.h"
#include "cnf.h"


using cmdline::parser;
// using std::vector;
using std::string;
using std::cout;
using std::endl;


parser CommPars(int argc, char * argv[]) {
    parser opt;
    opt.add<string>("type", 't', "type of average error metric", false, "ER");
    opt.add<string>("exact", 'e', "path to exact circuit", false, "./Circuit2Cnf/input/mult15/mult15.blif");
    opt.add<string>("appr", 'a', "path to approximate circuit", false, "./Circuit2Cnf/input/mult15/1_mult15_err_1.52588e-05_size_1780_depth_39.blif");
    opt.add<string>("deviation", 'd', "path to deviation-function circuit, meaningless for ER, only used for MED", false, "./Circuit2Cnf/input/deviation-function/width_30_absolute_error.blif");
    opt.add<string>("outp", 'o', "path to output CNF file, MUST end with .cnf", false, "./tmp/test.cnf");
    // opt.add<int>("flipOutp", 'f', "whether to flip output or not", 0);
    opt.parse_check(argc, argv);
    return opt;
}


void Enumerate(parser& opt) {
    AbcMan abc;
    abc.Comm("read ./Circuit2Cnf/input/standard-cell/mcnc_simple.genlib");
    abc.Comm("read " + opt.get<string>("appr") + "; sop;");

    NetMan mit(abc.GetNet(), true);
    EnumMan enumMan(mit);
    auto nModel = enumMan.CountModel();
    cout << "#model = " << nModel << endl;
    cout << "signal probability = " << BigFlt(nModel) / BigFlt(enumMan.GetTotFrame()) << endl;
}

// for error rate
void ConvertForER(parser& opt) {
    // load network & build miter
    AbcMan abc;
    abc.Comm("miter " + opt.get<string>("exact") + " " + opt.get<string>("appr"));

    // logic synthesis
    abc.Synth(ORIENT::AREA, true);
    abc.Comm("logic; aig;");
    NetMan mit(abc.GetNet(), true);
    assert(mit.GetPoNum() == 1);
    // if (opt.get<int>("flipOutp")) {
    //     // insert an inverter
    //     auto poDriv = mit.GetPoDriv(0);
    //     mit.DelObj(mit.GetPo(0));
    //     auto pInv = mit.CreateInv(poDriv);
    //     mit.CreatePo(pInv, "F");
    // }
    // technology mapping
    abc.Comm("r ./Circuit2Cnf/input/standard-cell/mcnc_simple.genlib");
    mit.Map(MAP_TYPE::SCL, ORIENT::AREA, true);
    // conversion
    MiterToCnf(mit, opt.get<string>("outp"));
}

// for mean error distance
extern "C" void Abc_NtkRemovePo( abc::Abc_Ntk_t * pNtk, int iOutput, int fRemoveConst0 );
extern "C" void Abc_NtkDropOneOutput( abc::Abc_Ntk_t * pNtk, int iOutput, int fSkipSweep, int fUseConst1 );
void ConvertForMED(parser& opt) {
    // check existence
    if (!IsPathExist(opt.get<string>("deviation"))) {
        cout << "The deviation-function circuit does not exist! Please provide the correct deviation-function file name!" << endl;
        cout << "You can use the following script to create the deviation-function for MED: ./Circuit2Cnf/script/BuildDeviation.py" << endl;
        assert(0);
    }

    // load network & build miter
    AbcMan abc;
    abc.Comm("r ./Circuit2Cnf/input/standard-cell/mcnc_simple.genlib");
    NetMan accNet(opt.get<string>("exact"));
    NetMan appNet(opt.get<string>("appr"));
    NetMan mitNet(opt.get<string>("deviation"));
    auto G = BuildMit(accNet, appNet, mitNet);

    // logic synthesis
    G.Comm("st; ifraig;");
    G.Synth(ORIENT::AREA, false);
    G.Comm("ps");

    // split POs
    std::vector<abc::Abc_Obj_t*> delPos;
    delPos.reserve(G.GetPoNum());
    for (int i = 0; i < G.GetPoNum(); ++i) { // only keep the i-th PO
        auto GTemp = G;
        delPos.clear();
        for (int j = 0; j < GTemp.GetPoNum(); ++j) {
            if (i != j) {
                Abc_NtkDropOneOutput( GTemp.GetNet(), j, false, false );
                delPos.emplace_back(GTemp.GetPo(j));
            }
        }
        for (auto delPo: delPos)
            Abc_NtkDeleteObj( delPo );
        assert(GTemp.GetPoNum() == 1);
        GTemp.Map(MAP_TYPE::SCL, ORIENT::AREA, false);
        cout << "keep " << i << "-th PO: "; GTemp.Comm("ps");
        MiterToCnf(GTemp, opt.get<string>("outp"), i);
    }
}

int main(int argc, char* argv[]) {
    GlobStartAbc();
    auto opt = CommPars(argc, argv);
    // create output directory
    auto outpFile = opt.get<string>("outp");
    if (outpFile.substr(outpFile.size() - 4) != ".cnf") {
        cout << "The output file should end with .cnf" << endl;
        assert(0);
    }
    else {
        // if there there is a '/' in the output file, then create the directory
        if (outpFile.find('/') != string::npos)  {
            auto outpDir = outpFile.substr(0, outpFile.find_last_of('/'));
            cout << "output directory: " << outpDir << endl;
            FixAndCreatePath(outpDir);
        }
    }

    // convert to CNF
    if (opt.get<string>("type") == "ER")
        ConvertForER(opt);
    else if (opt.get<string>("type") == "MED")
        ConvertForMED(opt);
    else {
        cout << "Please specify the correct type of average error metric, using one of the following options\n--type ER or --type MED" << endl;
        assert(0);
    }
    // Enumerate(opt);
    GlobStopAbc();
    return 0;
}
