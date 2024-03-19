import os
import time
import re
from optparse import OptionParser


def exec(comm):
    result = os.popen(comm).read()
    return result


# def GetOpt():
#     parser = OptionParser()
#     # parser.add_option('-f', '--folder',
#     #                   action='store',
#     #                   dest='folder',
#     #                   default='./input/ER/mult15-noinv',
#     #                   help='')
#     parser.add_option('-o', '--originGanak',
#                       action='store',
#                       dest='originGanak',
#                       default='1',
#                       help='')
#     # parser.add_option('-n', '--nFiles',
#     #                   action='store',
#     #                   dest='nFiles',
#     #                   default='10',
#     #                   help='')
#     options, _ = parser.parse_args()
#     return options


if __name__ == "__main__":
    # options = GetOpt()

    folders = [
                # './SimSharpSat/input/ER/mult15-noinv',
                './SimSharpSat/input/ER/mult16-new-noinv',
                # '.SimShartSat/input/ER/add128-noinv',
                # '.SimShartSat/input/ER/add16-noinv', 
                # '.SimShartSat/input/ER/add32-noinv',
                # '.SimShartSat/input/ER/add64-noinv',
                # '.SimShartSat/input/ER/absdiff',
                # '.SimShartSat/input/ER/bar',
                # '.SimShartSat/input/ER/binsqrd',
                # '.SimShartSat/input/ER/buttfly',
                # '.SimShartSat/input/ER/mac',
                # '.SimShartSat/input/ER/sin',
                # '.SimShartSat/input/ER/mult8-noinv',
                # '.SimShartSat/input/ER/mult10-noinv',
                # '.SimShartSat/input/ER/mult12-noinv',
                # '.SimShartSat/input/ER/bar',
                # '.SimShartSat/input/ER/arbiter',
                # '.SimShartSat/input/ER/ctrl',
                # '.SimShartSat/input/ER/cavlc',
                # '.SimShartSat/input/ER/dec',
                # '.SimShartSat/input/ER/i2c',
                # '.SimShartSat/input/ER/int2float',
                # '.SimShartSat/input/ER/memctrl',
                # '.SimShartSat/input/ER/priority',
                # '.SimShartSat/input/ER/router',
                # '.SimShartSat/input/ER/voter',
               ]
    # originGanak = int(options.originGanak)
    # if originGanak:
    #     print('test original ganak')
    # else:
    #     print('test vecam')
    nFiles = 1000
    originGanak = 0

    for folder in folders:
        print('')
        print(folder)
        assert os.path.exists(folder)
        if originGanak:
            print('name\tmc\tganak time(s)')
        else:
            print('name\tmc\tvacem time(s)')
        for _id in range(1, nFiles + 1):
            for name in sorted(os.listdir(folder)):
                if name.startswith(f'{_id}_') and name.find('.cnf') != -1:
                    # find nIsolated
                    _file = os.path.join(folder, name)
                    comm = f'cat {_file} | grep nIsolated'
                    res = exec(comm)
                    pos0 = res.find('nIsolated')
                    assert pos0 != -1
                    nIsolatedPIs = (int)(res[pos0 + 13: -1])

                    # model counting
                    if not originGanak:
                        comm = f'./SimSharpSat.out -noIBCP -t 14400 {_file} | grep -e \"mc including free PIs\" -e \"c time:\" -e \"simulation\" -e \"cache miss rate\" -e \"average pi\" -e \"average size\"'
                    else:
                        comm = f'ganak -noIBCP -t 14400 {_file} | grep -e \"s mc\" -e \"c time:\" -e \"simulation\" -e \"cache miss rate\"'
                    res = exec(comm)
                    # print(name, res)

                    # parse time
                    pos0 = res.find('time: ')
                    # assert pos0 != -1
                    if pos0 == -1:
                        print(f'{name}\ttle')
                        continue
                    pos1 = res.find('s\n')
                    assert pos1 != -1
                    _time = (float)(res[pos0 + 6: pos1])

                    # parse model counts
                    if not originGanak:
                        pos0 = res.find('PIs = ')
                        assert pos0 != -1
                        mc = (int)(res[pos0 + 6:])
                        print(f'{name}\t{mc}\t{_time}')
                    else:
                        pos0 = res.find('s mc ')
                        assert pos0 != -1
                        pos1 = res.find('\nc time:')
                        assert pos1 != -1
                        mc = (int)(res[pos0 + 5: pos1]) * 2**nIsolatedPIs
                        print(f'{name}\t{mc}\t{_time}')
