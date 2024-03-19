import os

if __name__ == "__main__":
    # fileName = 'mult15'
    # mitWidth = 30
    # fileName = 'buttfly'
    # mitWidth = 34
    fileName = 'absdiff'
    mitWidth = 8
    isInv = 0

    mitFile = f'input/miter/width_{mitWidth}_absolute_error.blif'
    folder = f'input/{fileName}'
    outFolder = f'output/{fileName}'
    if not os.path.exists(outFolder):
        os.mkdir(outFolder)
    fileAcc = f'{folder}/{fileName}.blif'
    assert os.path.exists(folder)
    for name in sorted(os.listdir(folder)):
        if name.find('.blif') != -1:
            _file = os.path.join(folder, name)
            if _file != fileAcc:
                outFile = os.path.join(outFolder, name.replace('.blif', '.cnf'))
                comm = f'./ACVerify.out -e {fileAcc} -a {_file} -m {mitFile}  -o {outFile} -f {isInv}'
                print(comm)
                os.system(comm)
