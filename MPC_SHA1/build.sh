#!/bin/bash
gcc -g MPC_SHA1.c -fopenmp -lcrypto -o MPC_SHA1
gcc -g MPC_SHA1_VERIFIER.c -fopenmp -lcrypto -o MPC_SHA1_VERIFIER

