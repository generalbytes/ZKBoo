/*
 ============================================================================
 Name        : MPC_SHA256.c
 Author      : Sobuno
 Version     : 0.1
 Description : MPC SHA256 for one block only
 ============================================================================
 */


#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "shared.h"
#include "omp.h"


#define CH(e,f,g) ((e & f) ^ ((~e) & g))


int NUM_ROUNDS = 136;

// void printbits(uint32_t n) {
// 	if (n) {
// 		printbits(n >> 1);
// 		printf("%d", n & 1);
// 	}
// }

void mpc_XOR(uint32_t x[NUM_BRANCHES], uint32_t y[NUM_BRANCHES], uint32_t z[NUM_BRANCHES]) { //xors z = x ^ y //all 3
	z[0] = x[0] ^ y[0];
	z[1] = x[1] ^ y[1];
	z[2] = x[2] ^ y[2];
}

void mpc_AND(uint32_t x[NUM_BRANCHES], uint32_t y[NUM_BRANCHES], uint32_t z[NUM_BRANCHES], unsigned char *randomness[NUM_BRANCHES], int* randCount, View views[NUM_BRANCHES], int* countY) { //calling this function increases countY+1 and randCount+4 (countY is index to view's.y)
	uint32_t r[NUM_BRANCHES] = {
		 getRandom32(randomness[0], *randCount),
		 getRandom32(randomness[1], *randCount),
		 getRandom32(randomness[2], *randCount)
	};
	*randCount += 4;
	uint32_t t[NUM_BRANCHES] = { 0 };

	t[0] = (x[0] & y[1]) ^ (x[1] & y[0]) ^ (x[0] & y[0]) ^ r[0] ^ r[1];
	t[1] = (x[1] & y[2]) ^ (x[2] & y[1]) ^ (x[1] & y[1]) ^ r[1] ^ r[2];
	t[2] = (x[2] & y[0]) ^ (x[0] & y[2]) ^ (x[2] & y[2]) ^ r[2] ^ r[0];
	z[0] = t[0];
	z[1] = t[1];
	z[2] = t[2];
	views[0].y[*countY] = z[0];
	views[1].y[*countY] = z[1];
	views[2].y[*countY] = z[2];
	(*countY)++;
}

void mpc_NEGATE(uint32_t x[NUM_BRANCHES], uint32_t z[NUM_BRANCHES]) {
	z[0] = ~x[0];
	z[1] = ~x[1];
	z[2] = ~x[2];
}

void mpc_ADD(uint32_t x[NUM_BRANCHES], uint32_t y[NUM_BRANCHES], uint32_t z[NUM_BRANCHES], unsigned char *randomness[NUM_BRANCHES], int* randCount, View views[NUM_BRANCHES], int* countY) {  //calling this function increases countY+1 and randCount+4 (countY is index to view's.y)
	uint32_t c[NUM_BRANCHES] = { 0 };
	uint32_t r[NUM_BRANCHES] = {
		getRandom32(randomness[0], *randCount),
		getRandom32(randomness[1], *randCount),
		getRandom32(randomness[2], *randCount)
	};
	*randCount += 4;

	uint8_t a[NUM_BRANCHES];
	uint8_t b[NUM_BRANCHES];
	uint8_t t;

	for(int i=0;i<31;i++) //256bits
	{
		a[0]=GETBIT(x[0] ^ c[0], i);
		a[1]=GETBIT(x[1] ^ c[1], i);
		a[2]=GETBIT(x[2] ^ c[2], i);

		b[0]=GETBIT(y[0] ^ c[0], i);
		b[1]=GETBIT(y[1] ^ c[1], i);
		b[2]=GETBIT(y[2] ^ c[2], i);

		t = (a[0]&b[1]) ^ (a[1]&b[0]) ^ GETBIT(r[1],i);
		SETBIT(c[0],i+1, t ^ (a[0]&b[0]) ^ GETBIT(c[0],i) ^ GETBIT(r[0],i));

		t = (a[1]&b[2]) ^ (a[2]&b[1]) ^ GETBIT(r[2],i);
		SETBIT(c[1],i+1, t ^ (a[1]&b[1]) ^ GETBIT(c[1],i) ^ GETBIT(r[1],i));

		t = (a[2]&b[0]) ^ (a[0]&b[2]) ^ GETBIT(r[0],i);
		SETBIT(c[2],i+1, t ^ (a[2]&b[2]) ^ GETBIT(c[2],i) ^ GETBIT(r[2],i));
	}

	z[0]=x[0]^y[0]^c[0];
	z[1]=x[1]^y[1]^c[1];
	z[2]=x[2]^y[2]^c[2];

	views[0].y[*countY] = c[0];
	views[1].y[*countY] = c[1];
	views[2].y[*countY] = c[2];
	*countY += 1;
}


void mpc_ADDK(uint32_t x[NUM_BRANCHES], uint32_t y, uint32_t z[NUM_BRANCHES], unsigned char *randomness[NUM_BRANCHES], int* randCount, View views[NUM_BRANCHES], int* countY) {  //calling this function increases countY+1 and randCount+4 (countY is index to view's.y)
	uint32_t c[NUM_BRANCHES] = { 0 };
	uint32_t r[NUM_BRANCHES] = {
		getRandom32(randomness[0], *randCount), 
		getRandom32(randomness[1], *randCount), 
		getRandom32(randomness[2], *randCount)
	};
	*randCount += 4;

	uint8_t a[NUM_BRANCHES], b[NUM_BRANCHES];

	uint8_t t;

	for(int bit=0;bit<31;bit++)
	{
		a[0]=GETBIT(x[0] ^ c[0], bit);
		a[1]=GETBIT(x[1] ^ c[1], bit);
		a[2]=GETBIT(x[2] ^ c[2], bit);

		b[0]=GETBIT(y ^ c[0], bit);
		b[1]=GETBIT(y ^ c[1], bit);
		b[2]=GETBIT(y ^ c[2], bit);

		t = (a[0]&b[1]) ^ (a[1]&b[0]) ^ GETBIT(r[1], bit);
		SETBIT(c[0],bit+1, t ^ (a[0]&b[0]) ^ GETBIT(c[0], bit) ^ GETBIT(r[0], bit));

		t = (a[1]&b[2]) ^ (a[2]&b[1]) ^ GETBIT(r[2], bit);
		SETBIT(c[1],bit+1, t ^ (a[1]&b[1]) ^ GETBIT(c[1], bit) ^ GETBIT(r[1], bit));

		t = (a[2]&b[0]) ^ (a[0]&b[2]) ^ GETBIT(r[0], bit);
		SETBIT(c[2],bit+1, t ^ (a[2]&b[2]) ^ GETBIT(c[2], bit) ^ GETBIT(r[2], bit));
	}

	z[0]=x[0] ^ y ^ c[0];
	z[1]=x[1] ^ y ^ c[1];
	z[2]=x[2] ^ y ^ c[2];


	views[0].y[*countY] = c[0];
	views[1].y[*countY] = c[1];
	views[2].y[*countY] = c[2];
	*countY += 1;
}


void mpc_RIGHTROTATE(uint32_t x[], int bits, uint32_t z[]) {
	z[0] = RIGHTROTATE(x[0], bits);
	z[1] = RIGHTROTATE(x[1], bits);
	z[2] = RIGHTROTATE(x[2], bits);
}

void mpc_RIGHTSHIFT(uint32_t x[NUM_BRANCHES], int bits, uint32_t z[NUM_BRANCHES]) {
	z[0] = x[0] >> bits;
	z[1] = x[1] >> bits;
	z[2] = x[2] >> bits;
}

void mpc_MAJ(uint32_t a[], uint32_t b[NUM_BRANCHES], uint32_t c[NUM_BRANCHES], uint32_t z[NUM_BRANCHES], unsigned char *randomness[NUM_BRANCHES], int* randCount, View views[NUM_BRANCHES], int* countY) {
	uint32_t t0[NUM_BRANCHES];
	uint32_t t1[NUM_BRANCHES];

	mpc_XOR(a, b, t0);
	mpc_XOR(a, c, t1);
	mpc_AND(t0, t1, z, randomness, randCount, views, countY);
	mpc_XOR(z, a, z);
}


void mpc_CH(uint32_t e[], uint32_t f[NUM_BRANCHES], uint32_t g[NUM_BRANCHES], uint32_t z[NUM_BRANCHES], unsigned char *randomness[NUM_BRANCHES], int* randCount, View views[NUM_BRANCHES], int* countY) {
	uint32_t t0[NUM_BRANCHES];

	//e & (f^g) ^ g
	mpc_XOR(f,  g,  t0);
	mpc_AND(e,  t0, t0, randomness, randCount, views, countY);
	mpc_XOR(t0, g,  z);
}

// int sha256(unsigned char* result, unsigned char* input, int numBits) {
// 	uint32_t hA[8] = { 
// 		0x6a09e667,
// 		0xbb67ae85,
// 		0x3c6ef372,
// 		0xa54ff53a,
// 		0x510e527f,
// 		0x9b05688c,
// 		0x1f83d9ab, 
// 		0x5be0cd19 };


// 	if (numBits > 447) {
// 		printf("Input too long, aborting!");
// 		return -1;
// 	}
// 	int chars = numBits >> 3;
// 	unsigned char* chunk = calloc(64, 1); //512 bits
// 	memcpy(chunk, input, chars);
// 	chunk[chars] = 0x80;
// 	//Last 8 chars used for storing length of input without padding, in big-endian.
// 	//Since we only care for one block, we are safe with just using last 9 bits and 0'ing the rest

// 	//chunk[60] = numBits >> 24;
// 	//chunk[61] = numBits >> 16;
// 	chunk[62] = numBits >> 8;
// 	chunk[63] = numBits;

// 	uint32_t w[64];
// 	int i;
// 	for (i = 0; i < 16; i++) {
// 		w[i] = (chunk[i * 4 + 0] << 24) |
// 		       (chunk[i * 4 + 1] << 16) |
// 			   (chunk[i * 4 + 2] <<  8) |
// 			    chunk[i * 4 + 3];
// 	}

// 	uint32_t s0, s1;
// 	for (i = 16; i < 64; i++) {
// 		s0 =   RIGHTROTATE(w[i - 15], 7)
// 		     ^ RIGHTROTATE(w[i - 15], 18)
// 			            ^ (w[i - 15] >> 3);
// 		s1 =   RIGHTROTATE(w[i - 2], 17)
// 		     ^ RIGHTROTATE(w[i - 2], 19)
// 						^ (w[i - 2] >> 10);
		
// 		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
// 	}

// 	uint32_t a, b, c, d, e, f, g, h, temp1, temp2, maj;
// 	a = hA[0];
// 	b = hA[1];
// 	c = hA[2];
// 	d = hA[3];
// 	e = hA[4];
// 	f = hA[5];
// 	g = hA[6];
// 	h = hA[7];

// 	for (i = 0; i < 64; i++) {
// 		s1 = RIGHTROTATE(e,6) ^ RIGHTROTATE(e, 11) ^ RIGHTROTATE(e, 25);

// 		temp1 = h + s1 + CH(e, f, g) + k[i] + w[i];
// 		s0 = RIGHTROTATE(a,2) ^ RIGHTROTATE(a, 13) ^ RIGHTROTATE(a, 22);

// 		maj = (a & (b ^ c)) ^ (b & c);
// 		temp2 = s0 + maj;


// 		h = g;
// 		g = f;
// 		f = e;
// 		e = d + temp1;
// 		d = c;
// 		c = b;
// 		b = a;
// 		a = temp1 + temp2;
// 	}

// 	hA[0] += a;
// 	hA[1] += b;
// 	hA[2] += c;
// 	hA[3] += d;
// 	hA[4] += e;
// 	hA[5] += f;
// 	hA[6] += g;
// 	hA[7] += h;

// 	for (i = 0; i < 8; i++) {
// 		result[i * 4] = (hA[i] >> 24);
// 		result[i * 4 + 1] = (hA[i] >> 16);
// 		result[i * 4 + 2] = (hA[i] >> 8);
// 		result[i * 4 + 3] = hA[i];
// 	}
// 	return 0;
// }



int mpc_sha256(unsigned char* results[NUM_BRANCHES], unsigned char* inputs[NUM_BRANCHES], int numBits, unsigned char *randomness[NUM_BRANCHES], View views[NUM_BRANCHES], int* countY) {
	if (numBits > 447) {
		printf("Input too long, aborting!");
		return -1;
	}

	int* randCount = calloc(1, sizeof(int));

	int chars = numBits >> 3;
	unsigned char* chunks[NUM_BRANCHES];
	uint32_t w[64][NUM_BRANCHES];

	for (int i = 0; i < NUM_BRANCHES; i++) {
		chunks[i] = calloc(64, 1); //512 bits
		memcpy(chunks[i], inputs[i], chars);
		chunks[i][chars] = 0x80;
		//Last 8 chars used for storing length of input without padding, in big-endian.
		//Since we only care for one block, we are safe with just using last 9 bits and 0'ing the rest

		//chunk[60] = numBits >> 24;
		//chunk[61] = numBits >> 16;
		chunks[i][62] = numBits >> 8;
		chunks[i][63] = numBits;
		memcpy(views[i].x, chunks[i], 64);

		for (int j = 0; j < 16; j++) {
			w[j][i] = (chunks[i][j * 4] << 24) | (chunks[i][j * 4 + 1] << 16)
							| (chunks[i][j * 4 + 2] << 8) | chunks[i][j * 4 + 3];
		}
		free(chunks[i]);
	}

	uint32_t s0[NUM_BRANCHES], s1[NUM_BRANCHES];
	uint32_t t0[NUM_BRANCHES], t1[NUM_BRANCHES];
	for (int j = 16; j < 64; j++) {
		//s0[i] = RIGHTROTATE(w[i][j-15],7) ^ RIGHTROTATE(w[i][j-15],18) ^ (w[i][j-15] >> 3);
		mpc_RIGHTROTATE(w[j-15], 7, t0);

		mpc_RIGHTROTATE(w[j-15], 18, t1);
		mpc_XOR(t0, t1, t0);
		mpc_RIGHTSHIFT(w[j-15], 3, t1);
		mpc_XOR(t0, t1, s0);

		//s1[i] = RIGHTROTATE(w[i][j-2],17) ^ RIGHTROTATE(w[i][j-2],19) ^ (w[i][j-2] >> 10);
		mpc_RIGHTROTATE(w[j-2], 17, t0);
		mpc_RIGHTROTATE(w[j-2], 19, t1);

		mpc_XOR(t0, t1, t0);
		mpc_RIGHTSHIFT(w[j-2], 10, t1);
		mpc_XOR(t0, t1, s1);

		//w[i][j] = w[i][j-16]+s0[i]+w[i][j-7]+s1[i];

		mpc_ADD(w[j-16], s0, t1   , randomness, randCount, views, countY);
		mpc_ADD(w[j-7],  t1, t1   , randomness, randCount, views, countY);
		mpc_ADD(t1,      s1, w[j] , randomness, randCount, views, countY);
	}

	uint32_t a[NUM_BRANCHES] = { hA[0], hA[0], hA[0] };
	uint32_t b[NUM_BRANCHES] = { hA[1], hA[1], hA[1] };
	uint32_t c[NUM_BRANCHES] = { hA[2], hA[2], hA[2] };
	uint32_t d[NUM_BRANCHES] = { hA[3], hA[3], hA[3] };
	uint32_t e[NUM_BRANCHES] = { hA[4], hA[4], hA[4] };
	uint32_t f[NUM_BRANCHES] = { hA[5], hA[5], hA[5] };
	uint32_t g[NUM_BRANCHES] = { hA[6], hA[6], hA[6] };
	uint32_t h[NUM_BRANCHES] = { hA[7], hA[7], hA[7] };
	uint32_t temp1[NUM_BRANCHES], temp2[NUM_BRANCHES], maj[NUM_BRANCHES];

	for (int i = 0; i < 64; i++) {
		//s1 = RIGHTROTATE(e,6) ^ RIGHTROTATE(e,11) ^ RIGHTROTATE(e,25);
		mpc_RIGHTROTATE(e, 6,  t0);
		mpc_RIGHTROTATE(e, 11, t1);
		mpc_XOR(t0, t1, t0);

		mpc_RIGHTROTATE(e, 25, t1);
		mpc_XOR(t0, t1, s1);


		//ch = (e & f) ^ ((~e) & g);
		//temp1 = h + s1 + CH(e,f,g) + k[i]+w[i];

		//t0 = h + s1

		mpc_ADD(h, s1, t0, randomness, randCount, views, countY);


		mpc_CH(e, f, g, t1, randomness, randCount, views, countY);

		//t1 = t0 + t1 (h+s1+ch)
		mpc_ADD(t0, t1, t1, randomness, randCount, views, countY);

		mpc_ADDK(t1, k[i], t1, randomness, randCount, views, countY);

		mpc_ADD(t1, w[i], temp1, randomness, randCount, views, countY);

		//s0 = RIGHTROTATE(a,2) ^ RIGHTROTATE(a,13) ^ RIGHTROTATE(a,22);
		mpc_RIGHTROTATE(a, 2, t0);
		mpc_RIGHTROTATE(a, 13, t1);
		mpc_XOR(t0, t1, t0);
		mpc_RIGHTROTATE(a, 22, t1);
		mpc_XOR(t0, t1, s0);


		mpc_MAJ(a, b, c, maj, randomness, randCount, views, countY);

		//temp2 = s0+maj;
		mpc_ADD(s0, maj, temp2, randomness, randCount, views, countY);

		memcpy(h, g, sizeof(uint32_t) * NUM_BRANCHES);
		memcpy(g, f, sizeof(uint32_t) * NUM_BRANCHES);
		memcpy(f, e, sizeof(uint32_t) * NUM_BRANCHES);
		//e = d+temp1;
		mpc_ADD(d, temp1, e, randomness, randCount, views, countY);
		memcpy(d, c, sizeof(uint32_t) * NUM_BRANCHES);
		memcpy(c, b, sizeof(uint32_t) * NUM_BRANCHES);
		memcpy(b, a, sizeof(uint32_t) * NUM_BRANCHES);
		//a = temp1+temp2;
		mpc_ADD(temp1, temp2, a, randomness, randCount, views, countY);
	}

	uint32_t hHa[8][NUM_BRANCHES] = { 
		{ hA[0], hA[0], hA[0] },
		{ hA[1], hA[1], hA[1] }, 
		{ hA[2], hA[2], hA[2] }, 
		{ hA[3], hA[3], hA[3] },
		{ hA[4], hA[4], hA[4] },
		{ hA[5], hA[5], hA[5] }, 
		{ hA[6], hA[6], hA[6] }, 
		{ hA[7], hA[7], hA[7] } 
	};

	mpc_ADD(hHa[0], a, hHa[0], randomness, randCount, views, countY);
	mpc_ADD(hHa[1], b, hHa[1], randomness, randCount, views, countY);
	mpc_ADD(hHa[2], c, hHa[2], randomness, randCount, views, countY);
	mpc_ADD(hHa[3], d, hHa[3], randomness, randCount, views, countY);
	mpc_ADD(hHa[4], e, hHa[4], randomness, randCount, views, countY);
	mpc_ADD(hHa[5], f, hHa[5], randomness, randCount, views, countY);
	mpc_ADD(hHa[6], g, hHa[6], randomness, randCount, views, countY);
	mpc_ADD(hHa[7], h, hHa[7], randomness, randCount, views, countY);

	for (int i = 0; i < 8; i++) {
		mpc_RIGHTSHIFT(hHa[i], 24, t0);
		results[0][i * 4] = t0[0];
		results[1][i * 4] = t0[1];
		results[2][i * 4] = t0[2];
		mpc_RIGHTSHIFT(hHa[i], 16, t0);
		results[0][i * 4 + 1] = t0[0];
		results[1][i * 4 + 1] = t0[1];
		results[2][i * 4 + 1] = t0[2];
		mpc_RIGHTSHIFT(hHa[i], 8, t0);
		results[0][i * 4 + 2] = t0[0];
		results[1][i * 4 + 2] = t0[1];
		results[2][i * 4 + 2] = t0[2];

		results[0][i * 4 + 3] = hHa[i][0];
		results[1][i * 4 + 3] = hHa[i][1];
		results[2][i * 4 + 3] = hHa[i][2];
	}
	free(randCount);
	return 0;
}



a commit(int numBytes,unsigned char shares[NUM_BRANCHES][numBytes], unsigned char *randomness[NUM_BRANCHES], unsigned char rs[NUM_BRANCHES][4], View views[NUM_BRANCHES]) {
	unsigned char* inputs[NUM_BRANCHES];
	inputs[0] = shares[0];
	inputs[1] = shares[1];
	inputs[2] = shares[2];
	unsigned char* hashes[NUM_BRANCHES];
	hashes[0] = malloc(32);
	hashes[1] = malloc(32);
	hashes[2] = malloc(32);

	int* countY = calloc(1, sizeof(int));
	mpc_sha256(hashes, inputs, numBytes * 8, randomness, views, countY);

	//Explicitly add y to view
	for(int i = 0; i<8; i++) {
		views[0].y[*countY] = (hashes[0][i * 4] << 24) | (hashes[0][i * 4 + 1] << 16) | (hashes[0][i * 4 + 2] << 8) | hashes[0][i * 4 + 3];
		views[1].y[*countY] = (hashes[1][i * 4] << 24) | (hashes[1][i * 4 + 1] << 16) | (hashes[1][i * 4 + 2] << 8) | hashes[1][i * 4 + 3];
		views[2].y[*countY] = (hashes[2][i * 4] << 24) | (hashes[2][i * 4 + 1] << 16) | (hashes[2][i * 4 + 2] << 8) | hashes[2][i * 4 + 3];
		*countY += 1;
	}
	free(countY);
	free(hashes[0]);
	free(hashes[1]);
	free(hashes[2]);

	uint32_t* result1 = malloc(32);
	uint32_t* result2 = malloc(32);
	uint32_t* result3 = malloc(32);

	output(views[0], result1);
	output(views[1], result2);
	output(views[2], result3);

	a a;
	memcpy(a.yp[0], result1, 32);
	memcpy(a.yp[1], result2, 32);
	memcpy(a.yp[2], result3, 32);

	free(result1);
	free(result2);
	free(result3);

	return a;
}

z prove(int e, unsigned char keys[NUM_BRANCHES][16], unsigned char rs[NUM_BRANCHES][4], View views[NUM_BRANCHES]) {
	z z;
	memcpy(z.ke, keys[e], 16);
	memcpy(z.ke1, keys[(e + 1) % NUM_BRANCHES], 16);
	z.ve = views[e];
	z.ve1 = views[(e + 1) % NUM_BRANCHES];
	memcpy(z.re, rs[e], 4);
	memcpy(z.re1, rs[(e + 1) % NUM_BRANCHES], 4);
	return z;
}



int main(void) {
	setbuf(stdout, NULL);
	srand((unsigned) time(NULL)); //set seed from timestamp
	init_EVP();
	openmp_thread_setup();

	//testing if random number can be read
	unsigned char garbage[4];
	if(RAND_bytes(garbage, 4) != 1) {
		printf("RAND_bytes failed crypto, aborting\n");
		return 0;
	}
	
	printf("Enter the string to be hashed (Max 55 characters): ");
	char userInput[55]; //55 is max length as we only support 447 bits = 55.875 bytes
	fgets(userInput, sizeof(userInput), stdin);
	
	int inputLen = strlen(userInput)-1;  //user input len
	printf("String length: %d\n", inputLen);
	printf("Iterations of SHA: %d\n", NUM_ROUNDS);

	unsigned char input[inputLen];
	for(int j = 0; j<inputLen; j++) {
		input[j] = userInput[j];
	}

	unsigned char rs  [NUM_ROUNDS][NUM_BRANCHES][4]; //filled with random bits
	unsigned char keys[NUM_ROUNDS][NUM_BRANCHES][16]; //filled with 128 random bits.
	a as[NUM_ROUNDS]; //commitments from all branches and all rounds
	View localViews[NUM_ROUNDS][NUM_BRANCHES]; //view per branch and round
	
	//Generating keys and rs
	if(RAND_bytes((unsigned char *)keys, NUM_ROUNDS * NUM_BRANCHES * 16) != 1) {
		printf("RAND_bytes failed crypto, aborting\n");
		return 0;
	}
	if(RAND_bytes((unsigned char *)rs,   NUM_ROUNDS * NUM_BRANCHES * 4) != 1) {
		printf("RAND_bytes failed crypto, aborting\n");
		return 0;
	}

	//Sharing secrets
	unsigned char shares[NUM_ROUNDS][NUM_BRANCHES][inputLen]; //filled with random bits
	if(RAND_bytes((unsigned char *)shares, NUM_ROUNDS * NUM_BRANCHES * inputLen) != 1) {
		printf("RAND_bytes failed crypto, aborting\n");
		return 0;
	}

	//fill shares for 3rd branch with input xored by other 2 branches.
	#pragma omp parallel for
	for(int round=0; round<NUM_ROUNDS; round++) {
		for (int j = 0; j < inputLen; j++) { //iterate for the len of the input
			shares[round][2][j] = input[j] ^ shares[round][0][j] ^ shares[round][1][j];
		}
	}

	//Generating randomness for each branch 2912 bytes.
	unsigned char *randomness[NUM_ROUNDS][NUM_BRANCHES];
	#pragma omp parallel for
	for(int round=0; round < NUM_ROUNDS; round++) {
		for(int j = 0; j < NUM_BRANCHES; j++) {
			randomness[round][j] = malloc(2912*sizeof(unsigned char)); //why 2912? Answer = ? 
			getAllRandomness(keys[round][j], randomness[round][j]); //randomness is generated via AES with random keys
		}
	}

	//Running MPC-SHA2
	#pragma omp parallel for
	for(int round=0; round<NUM_ROUNDS; round++) {
		//calculate COMMITMENTS for each round and branch
		as[round] = commit(inputLen, shares[round], randomness[round], rs[round], localViews[round]);
		for(int branch=0; branch < NUM_BRANCHES; branch++) {
			free(randomness[round][branch]);
		}
	}

	#pragma omp parallel for
	for(int round=0; round<NUM_ROUNDS; round++) { //calculate hashes for each branch
		unsigned char hash1[SHA256_DIGEST_LENGTH];
		H(keys[round][0], localViews[round][0], rs[round][0], (unsigned char *) hash1);
		memcpy(as[round].h[0], &hash1, 32);
		H(keys[round][1], localViews[round][1], rs[round][1], (unsigned char *) hash1);
		memcpy(as[round].h[1], &hash1, 32);
		H(keys[round][2], localViews[round][2], rs[round][2], (unsigned char *) hash1);
		memcpy(as[round].h[2], &hash1, 32);
	}

	//Generating E
	int es[NUM_ROUNDS];
	uint32_t finalHash[8];
	for (int j = 0; j < 8; j++) {
		finalHash[j] = as[0].yp[0][j] ^ as[0].yp[1][j] ^ as[0].yp[2][j];
	}
	H3(finalHash, as, NUM_ROUNDS, es); //Es is calculated by hashing final hash and contains of as


	//Packing Z
	z* zs = malloc(sizeof(z)*NUM_ROUNDS);
	#pragma omp parallel for
	for(int round = 0; round < NUM_ROUNDS; round++) {
		zs[round] = prove(es[round], keys[round], rs[round], localViews[round]);
	}
	
	
	//Writing to file
	FILE *file;
	char outputFile[3 * sizeof(int) + 8]; //maximum 3 decimals in number of rounds
	sprintf(outputFile, "out%i.bin", NUM_ROUNDS);
	file = fopen(outputFile, "wb");
	if (!file) {
		printf("Unable to open file!");
		return 1;
	}
	fwrite(as, sizeof(a), NUM_ROUNDS, file);
	fwrite(zs, sizeof(z), NUM_ROUNDS, file);
	fclose(file);
	free(zs);

	printf("Proof output to file %s\n", outputFile);
	openmp_thread_cleanup();
	cleanup_EVP();
	return EXIT_SUCCESS;
}
