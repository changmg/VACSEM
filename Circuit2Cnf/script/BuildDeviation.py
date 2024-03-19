import os
import time
import psutil
import math


def convert(_input, output, signals = ['a', 'b', 'abs_err']):
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


def GenAbsoluteErrorUnit(width, baseName):
    verName = baseName + '.v'
    blifName = baseName + '_temp.blif'
    reducedBlifName = baseName + '.blif'
    print(verName)
    with open(verName, 'w') as f:
        f.write(f'module width_{width}_absolute_error(a, b, abs_err);\n')
        f.write('parameter _bit = ' + str(width) + ';\n')
        f.write('input [_bit - 1: 0] a;\n')
        f.write('input [_bit - 1: 0] b;\n')
        f.write('output reg [_bit - 1: 0] abs_err;\n')
        f.write('assign abs_err = (a > b)? (a - b): (b - a);\n')
        f.write('endmodule\n')
    comm = 'yosys -p \"read_verilog ' + verName + '; synth; write_blif ' + blifName + '\"'
    print(comm)
    os.system(comm)
    convert(blifName, blifName)
    synth = 'st; compress2rs;'
    comm = 'abc -c \"source ./abc.rc; r ' + blifName + '; ' + synth + synth + synth + 'logic; sop;' + ' w ' + reducedBlifName + '\"'
    print(comm)
    os.system(comm)


nPo = 32
baseName = f'./input/deviation-function/width_{nPo}_absolute_error'
GenAbsoluteErrorUnit(nPo, baseName)
