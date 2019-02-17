# memory-pools
[![Build Status](https://travis-ci.com/CHr15F0x/memory-pools.svg?branch=master)](https://travis-ci.com/CHr15F0x/memory-pools)
[![codecov](https://codecov.io/gh/CHr15F0x/memory-pools/branch/master/graph/badge.svg)](https://codecov.io/gh/CHr15F0x/memory-pools)

# Introduction
Memory pool experiments, so far one the ideas is implemented.

# LiberalPool
Aims to be almost as good as this [Fast Efficient Fixed-Size Memory Pool](https://www.google.pl/url?sa=t&rct=j&q=&esrc=s&source=web&cd=1&ved=2ahUKEwiHsOK0g8PgAhXqp4sKHTs4DzQQFjAAegQIBRAC&url=https%3A%2F%2Fwww.thinkmind.org%2Fdownload.php%3Farticleid%3Dcomputation_tools_2012_1_10_80006&usg=AOvVaw2kKpQA4k8Y4aBq5KpD5Gtf) but at the same time allowing double free errors without pool metadata corruption, hence the __liberal__ prefix.
This comes at a cost of roughly __1 bit per block__.
