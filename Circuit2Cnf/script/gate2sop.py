import os
import subprocess
from optparse import OptionParser


def GetOpt():
    parser = OptionParser()
    parser.add_option('-i', '--input',
                      action='store',
                      dest='_input',
                      default='./input.v',
                      help='path to input Verilog file')
    parser.add_option('-o', '--output',
                      action='store',
                      dest='output',
                      default='./output.blif',
                      help='path to output blif file')
    options, _ = parser.parse_args()
    return options


def RunAbc(comm, logFileName=""):
    if logFileName != "":
        subprocess.run(f'abc -c \"{comm}\" >> {logFileName}', shell=True)
    else:
        subprocess.run(f'abc -c \"{comm}\"', shell=True)


if __name__ == "__main__":
    folder = 'input/cla32/gate'
    outFolder = 'input/cla32/raw'
    assert os.path.exists(folder)
    for name in sorted(os.listdir(folder)):
        if name.find('.blif') != -1:
            _file = os.path.join(folder, name)
            outFile = os.path.join(outFolder, name)
            print(_file, outFile)
            RunAbc(f"r input/standard-cell/mcnc.genlib; r {_file}; st; compress2rs; ps; w {outFile};")