 /*
 ============================================================================
 Name        : shared.h
 Author      : Sobuno
 Version     : 0.1
 Description : Common functions for the SHA-256 prover and verifier
 ============================================================================
 */

#ifndef SHARED_H_
#define SHARED_H_
#include <string.h>
#include <openssl/sha.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#ifdef _WIN32
#include <openssl/applink.c>
#endif
#include <openssl/rand.h>
#include "omp.h"

#define VERBOSE 1

static const uint32_t hA[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

static const uint32_t k[64] = { 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
		0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
		0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
		0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6,
		0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3,
		0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138,
		0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e,
		0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
		0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
		0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
		0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814,
		0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

//736 because that is how many times AND or ADD is called and ys need to be saved
#define ySize 736

#define DEBUG 0
#define debug_print(fmt, ...) \
            do { if (DEBUG) printf(fmt, __VA_ARGS__); } while (0)
#define NUM_BRANCHES 3
#define TWO_BRANCHES 2

typedef struct { // step in computation - internal state of each step
	unsigned char x[64]; //input share
	uint32_t y[ySize]; //736 32bit values
} View;

typedef struct { //commitment
	uint32_t yp[NUM_BRANCHES][8]; //3 parts of the hash must be xored to give result 
	unsigned char h[NUM_BRANCHES][32]; //hash of whole branch including key(used for randomness) and views
} a; //commitment (hashes and yp for each branch)

typedef struct {
	unsigned char ke0[16]; //key for branch 0
	unsigned char ke1[16]; //key for branch 1
	View ve0; //view states of branch 0
	View ve1; //view states of branch 1
	unsigned char re0[4]; //random used for branch 0
	unsigned char re1[4]; //random used for branch 1
} z; //proof = openings

#define RIGHTROTATE(x,n) (((x) >> (n)) | ((x) << (32-(n))))
#define GETBIT(x, bit) (((x) >> (bit)) & 0x01)
#define SETBIT(x, bit, b)   x= (b)&1 ? (x)|(1 << (bit)) : (x)&(~(1 << (bit)))


void handleErrors(void)
{
	ERR_print_errors_fp(stderr);
	abort();
}


EVP_CIPHER_CTX* setupAES(unsigned char key[16]) {
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	EVP_CIPHER_CTX_init(ctx);

	/* A 128 bit IV */
	unsigned char *iv = (unsigned char *)"01234567890123456";

	if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), NULL, key, iv)){
		handleErrors();
	}
	return ctx;
}

void getAllRandomness(unsigned char key[16], unsigned char randomness[2912]) {
	//Generate randomness: We use 728*32 bit of randomness per key.
	//Since AES block size is 128 bit, we need to run 728*32/128 = 182 iterations

	EVP_CIPHER_CTX* ctx;
	ctx = setupAES(key);
	unsigned char *plaintext = (unsigned char *)"0000000000000000";
	int len;
	for(int j=0;j<182;j++) {
		if(1 != EVP_EncryptUpdate(ctx, &randomness[j*16], &len, plaintext, strlen ((char *)plaintext)))
			handleErrors();

	}
	EVP_CIPHER_CTX_cleanup(ctx);
}

uint32_t getRandom32(unsigned char randomness[2912], int randCount) {
	uint32_t ret;
	memcpy(&ret, &randomness[randCount], 4);
	return ret;
}


void init_EVP() {
	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
	#if OPENSSL_VERSION_NUMBER < 0x10100000L
		OPENSSL_config(NULL); // not needed anylonger with current openssl versions
	#endif
}

void cleanup_EVP() {
	EVP_cleanup();
	ERR_free_strings();
}

void calculateHashForBranch(unsigned char k[16], View v, unsigned char r[4], unsigned char * hash) { //calculates sha256 from whole k,v and r
	SHA256_CTX ctx;
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, k, 16);
	SHA256_Update(&ctx, &v, sizeof(v));
	SHA256_Update(&ctx, r, 4);
	SHA256_Final(hash, &ctx); //write result to hash variable
}


void calculateEs(uint32_t y[8], a* as, int rounds, int* es) { //calculates in deterministic way Es for each round based on hash of (y and As)
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256_CTX ctx;
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, y, 32);
	SHA256_Update(&ctx, as, sizeof(a)*rounds);
	SHA256_Final(hash, &ctx);

	//Pick bits from hash
	int round = 0;
	int bitPosition = 0;
	while(round < rounds) {
		if(bitPosition >= SHA256_DIGEST_LENGTH * 8) { //Generate new hash as we have run out of bits in the previous hash
			SHA256_Init(&ctx);
			SHA256_Update(&ctx, hash, sizeof(hash));
			SHA256_Final(hash, &ctx);
			bitPosition = 0;
		}

		int b1 = GETBIT(hash[(bitPosition+0)/8], (bitPosition+0) % 8);
		int b2 = GETBIT(hash[(bitPosition+1)/8], (bitPosition+1) % 8);
		if(b1 == 0) {
			if(b2 == 0) {
				es[round] = 0;
			} else {
				es[round] = 1;
			}
			bitPosition += 2;
			round++;
		} else {
			if(b2 == 0) {
				es[round] = 2;
				round++;
			}
			bitPosition += 2;
		}
	}

}



void reconstruct(uint32_t* y0, uint32_t* y1, uint32_t* y2, uint32_t* result) {
	for (int i = 0; i < 8; i++) {
		result[i] = y0[i] ^ y1[i] ^ y2[i];
	}
}

void mpc_XOR2(uint32_t x[TWO_BRANCHES], uint32_t y[TWO_BRANCHES], uint32_t z[TWO_BRANCHES]) {
	z[0] = x[0] ^ y[0];
	z[1] = x[1] ^ y[1];
}

void mpc_NEGATE2(uint32_t x[TWO_BRANCHES], uint32_t z[TWO_BRANCHES]) {
	z[0] = ~x[0];
	z[1] = ~x[1];
}

omp_lock_t *locks;

void openmp_locking_callback(int mode, int type, char *file, int line)
{
  if (mode & CRYPTO_LOCK) {
    omp_set_lock(&locks[type]);
  } else {
    omp_unset_lock(&locks[type]);
  }
}


unsigned long openmp_thread_id(void)
{
  return (unsigned long)omp_get_thread_num();
}

void openmp_thread_setup(void)
{
  int i;
  locks = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(omp_lock_t));
  for (i=0; i<CRYPTO_num_locks(); i++)
  {
    omp_init_lock(&locks[i]);
  }
  CRYPTO_set_id_callback((unsigned long (*)())openmp_thread_id);
  CRYPTO_set_locking_callback((void (*)())openmp_locking_callback);
}

void openmp_thread_cleanup(void)
{
  int i;
  CRYPTO_set_id_callback(NULL);
  CRYPTO_set_locking_callback(NULL);
  for (i=0; i<CRYPTO_num_locks(); i++){
    omp_destroy_lock(&locks[i]);
  }
  OPENSSL_free(locks);
}


int mpc_AND_verify(uint32_t x[TWO_BRANCHES], uint32_t y[TWO_BRANCHES], uint32_t z[TWO_BRANCHES], View ve, View ve1, unsigned char randomness[TWO_BRANCHES][2912], int* randCount, int* countY) {
	uint32_t r[TWO_BRANCHES] = {
		 getRandom32(randomness[0], *randCount),
		 getRandom32(randomness[1], *randCount)
	};
	*randCount += 4;

	uint32_t t = 0;

	t = (x[0] & y[1]) ^ (x[1] & y[0]) ^ (x[0] & y[0]) ^ r[0] ^ r[1];
	if(ve.y[*countY] != t) {
		return 1;
	}
	z[0] = t;
	z[1] = ve1.y[*countY];

	(*countY)++;
	debug_print("countY increased by mpc_AND_verify to %d.\n",(*countY));
	return 0;
}


int mpc_ADD_verify(uint32_t x[TWO_BRANCHES], uint32_t y[TWO_BRANCHES], uint32_t z[TWO_BRANCHES], View ve, View ve1, unsigned char randomness[TWO_BRANCHES][2912], int* randCount, int* countY) {
	uint32_t r[TWO_BRANCHES] = {
		 getRandom32(randomness[0], *randCount),
		 getRandom32(randomness[1], *randCount)
	};
	*randCount += 4;

	uint8_t a[TWO_BRANCHES];
	uint8_t b[TWO_BRANCHES];
	uint8_t t;

	for(int i=0;i<31;i++)
	{
		a[0]=GETBIT(x[0] ^  ve.y[*countY], i);
		a[1]=GETBIT(x[1] ^ ve1.y[*countY], i);

		b[0]=GETBIT(y[0] ^  ve.y[*countY], i);
		b[1]=GETBIT(y[1] ^ ve1.y[*countY], i);

		t = (a[0]&b[1]) ^ (a[1]&b[0]) ^ GETBIT(r[1],i);
		if(GETBIT(ve.y[*countY],i+1) != (t ^ (a[0]&b[0]) ^ GETBIT(ve.y[*countY],i) ^ GETBIT(r[0],i))) {
			return 1;
		}
	}

	z[0]=x[0]^y[0] ^  ve.y[*countY];
	z[1]=x[1]^y[1] ^ ve1.y[*countY];
	(*countY)++;
	debug_print("countY increased by mpc_ADD_verify to %d.\n",(*countY));
	return 0;
}

void mpc_RIGHTROTATE2(uint32_t x[TWO_BRANCHES], int bits, uint32_t z[TWO_BRANCHES]) {
	z[0] = RIGHTROTATE(x[0], bits);
	z[1] = RIGHTROTATE(x[1], bits);
}

void mpc_RIGHTSHIFT2(uint32_t x[TWO_BRANCHES], int bits, uint32_t z[TWO_BRANCHES]) {
	z[0] = x[0] >> bits;
	z[1] = x[1] >> bits;
}

int mpc_MAJ_verify(uint32_t a[TWO_BRANCHES], uint32_t b[TWO_BRANCHES], uint32_t c[TWO_BRANCHES], uint32_t z[NUM_BRANCHES], View ve, View ve1, unsigned char randomness[TWO_BRANCHES][2912], int* randCount, int* countY) {
	uint32_t t0[NUM_BRANCHES];
	uint32_t t1[NUM_BRANCHES];

	mpc_XOR2(a, b, t0);
	mpc_XOR2(a, c, t1);
	if(mpc_AND_verify(t0, t1, z, ve, ve1, randomness, randCount, countY) == 1) {
		return 1;
	}
	mpc_XOR2(z, a, z);
	return 0;
}

int mpc_CH_verify(uint32_t e[TWO_BRANCHES], uint32_t f[TWO_BRANCHES], uint32_t g[TWO_BRANCHES], uint32_t z[TWO_BRANCHES], View ve, View ve1, unsigned char randomness[TWO_BRANCHES][2912], int* randCount, int* countY) {
	uint32_t t0[NUM_BRANCHES];
	mpc_XOR2(f,g,t0);
	if(mpc_AND_verify(e, t0, t0, ve, ve1, randomness, randCount, countY) == 1) {
		return 1;
	}
	mpc_XOR2(t0,g,z);
	return 0;
}


int verifyRound(a a, int e, z z) {

	//1. First check if hashes of branches are ok.
	unsigned char* hash = malloc(SHA256_DIGEST_LENGTH);
	calculateHashForBranch(z.ke0, z.ve0, z.re0, hash); //calculate hash from z.ke0, z.ve0 a z.re0

	if (memcmp(a.h[(e + 0) % NUM_BRANCHES], hash, 32) != 0) {
#if VERBOSE
		printf("Failing at %d", __LINE__);
#endif
		return 1;
	}
	calculateHashForBranch(z.ke1, z.ve1, z.re1, hash); //calculate hash from z.ke1, z.ve1 a z.re1
	if (memcmp(a.h[(e + 1) % NUM_BRANCHES], hash, 32) != 0) { 
#if VERBOSE
		printf("Failing at %d", __LINE__);
#endif
		return 1;
	}
	free(hash);

	//2. Check if last step in view is equal to yp for both branches
	uint32_t* result = malloc(32);
	memcpy(result, &(z.ve0).y[ySize - 8], 32);
	if (memcmp(a.yp[(e + 0) % NUM_BRANCHES], result, 32) != 0) { //a.yp[e] must contain same thing as z.ve.y[ySize - 8]
#if VERBOSE
		printf("Failing at %d", __LINE__);
#endif
		return 1;
	}

	memcpy(result, &z.ve1.y[ySize - 8], 32);
	if (memcmp(a.yp[(e + 1) % NUM_BRANCHES], result, 32) != 0) {
#if VERBOSE
		printf("Failing at %d", __LINE__);
#endif
		return 1;
	}

	free(result);

	//3. Generate deterministicaly randomness for both branches based on the supplied AES keys
	unsigned char randomness[TWO_BRANCHES][2912];
	getAllRandomness(z.ke0, randomness[0]);
	getAllRandomness(z.ke1, randomness[1]);

	int* randCount = calloc(1, sizeof(int));
	int* countY    = calloc(1, sizeof(int));


	//4. calculate initial state for SHA256 based on shares.
	uint32_t w[64][TWO_BRANCHES];
	for (int j = 0; j < 16; j++) {
		w[j][0] = (z.ve0.x[j * 4] << 24) | (z.ve0.x[j * 4 + 1] << 16) | (z.ve0.x[j * 4 + 2] << 8) | z.ve0.x[j * 4 + 3];
		w[j][1] = (z.ve1.x[j * 4] << 24) | (z.ve1.x[j * 4 + 1] << 16) | (z.ve1.x[j * 4 + 2] << 8) | z.ve1.x[j * 4 + 3];
	}

	uint32_t s0[TWO_BRANCHES], s1[TWO_BRANCHES];
	uint32_t t0[TWO_BRANCHES], t1[TWO_BRANCHES];
	for (int j = 16; j < 64; j++) {
		//s0[i] = RIGHTROTATE(w[i][j-15],7) ^ RIGHTROTATE(w[i][j-15],18) ^ (w[i][j-15] >> 3);
		mpc_RIGHTROTATE2(w[j-15],  7, t0);
		mpc_RIGHTROTATE2(w[j-15], 18, t1);
		mpc_XOR2(t0, t1, t0);
		mpc_RIGHTSHIFT2(w[j-15], 3, t1);
		mpc_XOR2(t0, t1, s0);

		//s1[i] = RIGHTROTATE(w[i][j-2],17) ^ RIGHTROTATE(w[i][j-2],19) ^ (w[i][j-2] >> 10);
		mpc_RIGHTROTATE2(w[j-2], 17, t0);
		mpc_RIGHTROTATE2(w[j-2], 19, t1);
		mpc_XOR2(t0, t1, t0);
		mpc_RIGHTSHIFT2(w[j-2],10, t1);
		mpc_XOR2(t0, t1, s1);

		//w[i][j] = w[i][j-16]+s0[i]+w[i][j-7]+s1[i];

		if(mpc_ADD_verify(w[j-16], s0, t1, z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d, iteration %d", __LINE__, j);
#endif
			return 1;
		}


		if(mpc_ADD_verify(w[j-7], t1, t1, z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d, iteration %d", __LINE__, j);
#endif
			return 1;
		}
		if(mpc_ADD_verify(t1, s1, w[j], z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d, iteration %d", __LINE__, j);
#endif
			return 1;
		}
	}

	uint32_t va[TWO_BRANCHES] = { hA[0],hA[0] };
	uint32_t vb[TWO_BRANCHES] = { hA[1],hA[1] };
	uint32_t vc[TWO_BRANCHES] = { hA[2],hA[2] };
	uint32_t vd[TWO_BRANCHES] = { hA[3],hA[3] };
	uint32_t ve[TWO_BRANCHES] = { hA[4],hA[4] };
	uint32_t vf[TWO_BRANCHES] = { hA[5],hA[5] };
	uint32_t vg[TWO_BRANCHES] = { hA[6],hA[6] };
	uint32_t vh[TWO_BRANCHES] = { hA[7],hA[7] };
	uint32_t temp1[NUM_BRANCHES], temp2[NUM_BRANCHES], maj[NUM_BRANCHES];
	for (int i = 0; i < 64; i++) {
		//s1 = RIGHTROTATE(e,6) ^ RIGHTROTATE(e,11) ^ RIGHTROTATE(e,25);
		mpc_RIGHTROTATE2(ve, 6, t0);
		mpc_RIGHTROTATE2(ve, 11, t1);
		mpc_XOR2(t0, t1, t0);
		mpc_RIGHTROTATE2(ve, 25, t1);
		mpc_XOR2(t0, t1, s1);




		//ch = (e & f) ^ ((~e) & g);
		//temp1 = h + s1 + CH(e,f,g) + k[i]+w[i];

		//t0 = h + s1

		if(mpc_ADD_verify(vh, s1, t0, z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d, iteration %d", __LINE__, i);
#endif
			return 1;
		}



		if(mpc_CH_verify(ve, vf, vg, t1, z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d, iteration %d", __LINE__, i);
#endif
			return 1;
		}

		//t1 = t0 + t1 (h+s1+ch)
		if(mpc_ADD_verify(t0, t1, t1, z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d, iteration %d", __LINE__, i);
#endif
			return 1;
		}

		t0[0] = k[i];
		t0[1] = k[i];
		if(mpc_ADD_verify(t1, t0, t1, z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d, iteration %d", __LINE__, i);
#endif
			return 1;
		}



		if(mpc_ADD_verify(t1, w[i], temp1, z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d, iteration %d", __LINE__, i);
#endif
			return 1;
		}

		//s0 = RIGHTROTATE(a,2) ^ RIGHTROTATE(a,13) ^ RIGHTROTATE(a,22);
		mpc_RIGHTROTATE2(va, 2, t0);
		mpc_RIGHTROTATE2(va, 13, t1);
		mpc_XOR2(t0, t1, t0);
		mpc_RIGHTROTATE2(va, 22, t1);
		mpc_XOR2(t0, t1, s0);

		//maj = (a & (b ^ c)) ^ (b & c);
		//(a & b) ^ (a & c) ^ (b & c)

		if(mpc_MAJ_verify(va, vb, vc, maj, z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d, iteration %d", __LINE__, i);
#endif
			return 1;
		}

		//temp2 = s0+maj;
		if(mpc_ADD_verify(s0, maj, temp2, z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d, iteration %d", __LINE__, i);
#endif
			return 1;
		}

		memcpy(vh, vg, sizeof(uint32_t) * TWO_BRANCHES);
		memcpy(vg, vf, sizeof(uint32_t) * TWO_BRANCHES);
		memcpy(vf, ve, sizeof(uint32_t) * TWO_BRANCHES);
		//e = d+temp1;
		if(mpc_ADD_verify(vd, temp1, ve, z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d, iteration %d", __LINE__, i);
#endif
			return 1;
		}

		memcpy(vd, vc, sizeof(uint32_t) * TWO_BRANCHES);
		memcpy(vc, vb, sizeof(uint32_t) * TWO_BRANCHES);
		memcpy(vb, va, sizeof(uint32_t) * TWO_BRANCHES);
		//a = temp1+temp2;

		if(mpc_ADD_verify(temp1, temp2, va, z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d, iteration %d", __LINE__, i);
#endif
			return 1;
		}
	}

	uint32_t hHa[8][NUM_BRANCHES] = {
		 { hA[0],hA[0],hA[0] },
		 { hA[1],hA[1],hA[1] },
		 { hA[2],hA[2],hA[2] },
		 { hA[3],hA[3],hA[3] },
		 { hA[4],hA[4],hA[4] },
		 { hA[5],hA[5],hA[5] },
		 { hA[6],hA[6],hA[6] },
		 { hA[7],hA[7],hA[7] }
	};
	if(mpc_ADD_verify(hHa[0], va, hHa[0], z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d", __LINE__);
#endif
		return 1;
	}
	if(mpc_ADD_verify(hHa[1], vb, hHa[1], z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d", __LINE__);
#endif
		return 1;
	}
	if(mpc_ADD_verify(hHa[2], vc, hHa[2], z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d", __LINE__);
#endif
		return 1;
	}
	if(mpc_ADD_verify(hHa[3], vd, hHa[3], z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d", __LINE__);
#endif
		return 1;
	}
	if(mpc_ADD_verify(hHa[4], ve, hHa[4], z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d", __LINE__);
#endif
		return 1;
	}
	if(mpc_ADD_verify(hHa[5], vf, hHa[5], z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d", __LINE__);
#endif
		return 1;
	}
	if(mpc_ADD_verify(hHa[6], vg, hHa[6], z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d", __LINE__);
#endif
		return 1;
	}
	if(mpc_ADD_verify(hHa[7], vh, hHa[7], z.ve0, z.ve1, randomness, randCount, countY) == 1) {
#if VERBOSE
		printf("Failing at %d", __LINE__);
#endif
		return 1;
	}

	free(randCount);
	free(countY);
	return 0;
}


#endif /* SHARED_H_ */
