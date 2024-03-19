import os
import time
import psutil
import re

def exec(comm):
    result = os.popen(comm).read()
    return result

if __name__ == "__main__":
    # folder = 'input/MAE/mult8'
    # nInp = 16
    folder = 'input/MAE/mult10'
    nInp = 20
    # folder = 'input/MAE/mult12'
    # nInp = 28
    # folder = 'input/MAE/mult15'
    # nInp = 30
    # folder = 'input/MAE/mult16-new'
    # nInp = 32
    # folder = 'input/MAE/add32'
    # nInp = 64
    # folder = 'input/MAE/add64'
    # nInp = 128
    # folder = 'input/MAE/add128'
    # nInp = 256
    # folder = 'input/MAE/buttfly'
    # nInp = 32
    # folder = 'input/MAE/bar'
    # nInp = 135
    # folder = 'input/MAE/mac'
    # nInp = 12
    # folder = 'input/MAE/absdiff'
    # nInp = 16
    # folder = 'input/MAE/binsqrd'
    # nInp = 16
    originGanak = 1
    nFiles = 20
    nPatt = 2**nInp

    if originGanak:
        print('name\tmc\tganak time(s)')
    else:
        print('name\tmc\tvacem time(s)')

    assert os.path.exists(folder)
    for _id in range(1, nFiles + 1):
        mae = 0
        totTime = 0
        # print(f'\n_id = {_id}')
        for name in sorted(os.listdir(folder)):
            if name.startswith(f'{_id}_') and name.find('.cnf') != -1:
                recName = name
                if name.find('_const1') != -1:
                    assert 0
                    continue
                elif name.find('_const0') != -1:
                    continue
                else:
                    # find bit
                    pos0 = name.rfind('_')
                    assert pos0 != -1
                    pos1 = name.find('.cnf')
                    assert pos1 != -1 and pos0 < pos1
                    bit = (int)(name[pos0 + 1: pos1])

                    # find nIsolated
                    _file = os.path.join(folder, name)
                    comm = f'cat {_file} | grep nIsolated'
                    res = exec(comm)
                    pos0 = res.find('nIsolated')
                    assert pos0 != -1
                    nIsolatedPIs = (int)(res[pos0 + 13: -1])

                    # model counting
                    if not originGanak:
                        comm = f'./SimSharpSat.out -noIBCP -t 14400 {_file} | grep -e \"mc including free PIs\" -e \"c time:\"'
                    else:
                        comm = f'ganak -t 14400 -noIBCP {_file} | grep -e \"s mc\" -e \"c time:\"'
                    res = exec(comm)

                    if res.find('time: ') != -1:
                        # parse time
                        pos0 = res.find('time: ')
                        assert pos0 != -1
                        pos1 = res.find('s\n')
                        assert pos1 != -1
                        _time = (float)(res[pos0 + 6: pos1])

                        # parse model counts
                        if not originGanak:
                            pos0 = res.find('PIs = ')
                            assert pos0 != -1
                            mc = (int)(res[pos0 + 6:])
                            # print(f'bit = {bit}\tmc = {mc}\ttime = {_time}')
                        else:
                            pos0 = res.find('s mc ')
                            assert pos0 != -1
                            pos1 = res.find('\nc time:')
                            assert pos1 != -1
                            mc = (int)(res[pos0 + 5: pos1]) * 2**nIsolatedPIs
                            # print(f'bit = {bit}\tmc = {mc}\ttime = {_time}')

                        # update mae
                        # mae += mc * 2**bit / nPatt
                        mae += mc * 2**(bit-nInp)
                        totTime += _time
                    else:
                        assert 0
                        # comm = f'ganak -t 14400 -noPP -noIBCP {_file} | grep -e \"s mc\"'
                        # res = exec(comm)
                        # assert res == 's mc 0\n'


        # print(f'id = {_id}, mae = {mae}, totTime = {totTime}, {_file}')
        # print(f'{_id}\t{mae}\t{totTime}\t{_file}')
        print(f'{recName}\t{mae}\t{totTime}')

