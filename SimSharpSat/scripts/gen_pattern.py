# VAR_THRES = 2

for VAR_THRES in range(18, 19):
    N_BITS = 1 << VAR_THRES

    # print(f'const int VAR_THRES = {VAR_THRES};')
    # print(f'const int N_BITS = 1 << VAR_THRES;')
    # str_ = ''.join(['1' for _ in range(N_BITS)])
    # print(f'const std::bitset<N_BITS> ALL_BITS_ONE (\"{str_}\");')
    # str_ = ''.join(['0' for _ in range(N_BITS)])
    # print(f'const std::bitset<N_BITS> ALL_BITS_ZERO(\"{str_}\");')
    # print('const std::vector<std::bitset<N_BITS>> ENUM_PATTERN = {')
    # print('const std::vector<std::bitset<N_BITS>> ENUM_PATTERN = {')
    # for i in range(VAR_THRES):
    #     str_ = ''
    #     value = 1
    #     for j in range(1, N_BITS + 1):
    #         str_ += str(value)
    #         if j % (1 << i) == 0:
    #             value = 1 - value
    #     print(f'                         bitset<N_BITS>(\"{str_}\"),')
    # print('};')

    print(f'    // N_BIT = 2^{VAR_THRES}')
    print('    vector<BitVec>({')
    for i in range(VAR_THRES):
        str_ = ''
        value = 1
        for j in range(1, N_BITS + 1):
            str_ += str(value)
            if j % (1 << i) == 0:
                value = 1 - value
        print(f'        BitVec(string(\"{str_}\")),')
    print('    }),')