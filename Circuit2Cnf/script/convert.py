import os

if __name__ == "__main__":
    # fileName = 'mult16'
    fileName = 'buttfly'
    isInv = 0
    folder = f'input/{fileName}'
    outFolder = 'output/'
    fileAcc = f'{folder}/{fileName}.blif'
    assert os.path.exists(folder)
    for name in os.listdir(folder):
        if name.find('.blif') != -1:
            _file = os.path.join(folder, name)
            if _file != fileAcc:
                outFile = os.path.join(outFolder, name.replace('.blif', '.cnf'))
                comm = f'./ACVerify.out -e {fileAcc} -a {_file} -o {outFile} -f {isInv}'
                print(comm)
                os.system(comm)
