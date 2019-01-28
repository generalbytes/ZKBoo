#!/bin/bash
rm MPC_SHA256
rm MPC_SHA256_VERIFIER
gcc -Wall -g MPC_SHA256.c -fopenmp -lcrypto -o MPC_SHA256
gcc -Wall -g MPC_SHA256_VERIFIER.c -fopenmp -lcrypto -o MPC_SHA256_VERIFIER

