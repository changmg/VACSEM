import os
import subprocess
import re
from optparse import OptionParser


def GetOpt():
    parser = OptionParser()
    parser.add_option('-i', '--input',
                      action='store',
                      dest='_input',
                      default='',
                      help='path to input file')
    # parser.add_option('-o', '--output',
    #                   action='store',
    #                   dest='output',
    #                   default='',
    #                   help='path to output file')
    options, _ = parser.parse_args()
    return options


def convert(_input, output, signals = ['in', 'w', 'out']):
    with open(_input, 'r') as f:
        lines = f.readlines()
    with open(output, 'w') as f:
        for line in lines:
            for signal in signals:
                if line.find(signal) != -1:
                    for i in range(10):
                        line = line.replace(f'{signal}[{i}]', f'{signal}[00{i}]')
                    for i in range(10, 100):
                        line = line.replace(f'{signal}[{i}]', f'{signal}[0{i}]')
            f.write(line)

if __name__ == "__main__":
    fileName = 'cla32'
    folder = f'input/{fileName}/raw'
    outFolder = f'input/{fileName}'
    assert os.path.exists(folder)
    for name in sorted(os.listdir(folder)):
        if name.find('.blif') != -1:
            _file = os.path.join(folder, name)
            outFile = os.path.join(outFolder, name)
            print(_file, outFile)
            convert(_file, outFile, ['a', 'b', 'sum'])