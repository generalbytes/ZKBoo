#!/bin/bash
gcc -g MPC_SHA256.c -fopenmp -lcrypto -o MPC_SHA256
gcc -g MPC_SHA256_VERIFIER.c -fopenmp -lcrypto -o MPC_SHA256_VERIFIER

