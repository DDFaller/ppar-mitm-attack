/*
 * Sorbonne Université - PPAR (S1-24)
 * Project - Direct Meet-in-the-Middle Attack
 *
 * Implementation of the MitM distributed algorithm with a compression strategy
 * that can use less memory than the required by the sequential version.
 *
 * If you wish to optimize the memory allocation (i.e. equally distribute it
 * between processes), you must ensure that cores are evenly distributed in
 * a multinode topology. For instance, if you run the code for 128 cores on
 * 8 nodes, you should put 16 cores per node:
 *
 *     mpiexec -n 128 --map-by ppr:16:node ./mitm_parallel ...
 *
 * This may slow down the code execution, but it also ensures that no memory
 * problems will ensue.
 *
 * Additionally, the program has an early exit strategy, halting as soon as a
 * golden collision is found. It must be set during compilation time by
 * defining the EARLY_EXIT macro.
 *
 * Adapted by: Matheus FERNANDES MORENO
 *             Daniel MACHADO CARNEIRO FALLER
 *
 */

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <getopt.h>
#include <err.h>
#include <math.h>
#include <mpi.h>

typedef uint64_t u64;       /* portable 64-bit integer */
typedef uint32_t u32;       /* portable 32-bit integer */
struct __attribute__ ((packed)) entry { u32 k; u64 v; };  /* hash table entry */

/***************************** global variables ******************************/

u64 n = 0;            /* block size (in bits) */
u64 mask;             /* this is 2**n - 1 */

u64 dict_size;         /* number of slots in the local hash table */
u64 dict_size_global;  /* number of slots in the hash table */
struct entry *A;       /* the hash table */

/* (P, C) : two plaintext-ciphertext pairs */
u32 P[2][2] = {{0, 0}, {0xffffffff, 0xffffffff}};
u32 C[2][2];

/***************************** MPI settings *********************************/

#define N_PROBES_MAX            256

#define ROOT_RANK               0
#define BUFFER_COUNT_MSG_SIZE   1
#define BUFFER_ELEMENT_SIZE     2
#define EARLY_EXIT_MSG_SIZE     1
#define BUFFER_RELATIVE_SIZE    0.001    /* 0.1% of (local) dict size */

/* useful macros for compression algorithm */
#define MIN(x, y)               (((x) < (y)) ? (x) : (y))
#define GET_BUFFER_SIZE(b)      MIN(ceil(BUFFER_RELATIVE_SIZE * (b)), INT_MAX / BUFFER_ELEMENT_SIZE)
#define GB                      1073741824
#define RELAXATION_FACTOR       1.25

#ifndef EARLY_EXIT
#define EARLY_EXIT              0
#endif

int num_processes, rank;

u64 buffer_size;                /* number of elements in a single buffer */
u64 *buffers;                   /* buffers for a process */
u64 *buffers_counts;            /* counters for flushing/batch processing */

int compress_factor = 0;        /* to deal memory limitations */

/* timers for performance evaluation */
double compute_time = 0, communication_time = 0, fill_time = 0, probe_time = 0;

/* variables to measure the buffer efficiency */
int num_exchanges = 0;
double cum_buffer_occupancy = 0;

/************************ tools and utility functions *************************/

double wtime()
{
    struct timeval ts;
	gettimeofday(&ts, NULL);
	return (double)ts.tv_sec + ts.tv_usec / 1E6;
}

void time_comm(void (*f)())
{
    double tic = wtime();
    (*f)();
    communication_time += wtime() - tic;
}

// murmur64 hash functions, tailorized for 64-bit ints / Cf. Daniel Lemire
u64 murmur64(u64 x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ull;
    x ^= x >> 33;
    return x;
}

/* represent n in 4 bytes */
void human_format(u64 n, char *target)
{
    if (n < 1024) {
        sprintf(target, "%" PRId64, n);
        return;
    }
    if (n < 1048576) {
        sprintf(target, "%.1fK", n / 1e3);
        return;
    }
    if (n < 1073741824) {
        sprintf(target, "%.1fM", n / 1e6);
        return;
    }
    if (n < 1099511627776ll) {
        sprintf(target, "%.1fG", n / 1e9);
        return;
    }
    if (n < 1125899906842624ll) {
        sprintf(target, "%.1fT", n / 1e12);
        return;
    }
}

/******************************** SPECK block cipher **************************/

#define ROTL32(x,r) (((x)<<(r)) | (x>>(32-(r))))
#define ROTR32(x,r) (((x)>>(r)) | ((x)<<(32-(r))))

#define ER32(x,y,k) (x=ROTR32(x,8), x+=y, x^=k, y=ROTL32(y,3), y^=x)
#define DR32(x,y,k) (y^=x, y=ROTR32(y,3), x^=k, x-=y, x=ROTL32(x,8))

void Speck64128KeySchedule(const u32 K[],u32 rk[])
{
    u32 i,D=K[3],C=K[2],B=K[1],A=K[0];
    for(i=0;i<27;){
        rk[i]=A; ER32(B,A,i++);
        rk[i]=A; ER32(C,A,i++);
        rk[i]=A; ER32(D,A,i++);
    }
}

void Speck64128Encrypt(const u32 Pt[], u32 Ct[], const u32 rk[])
{
    u32 i;
    Ct[0]=Pt[0]; Ct[1]=Pt[1];
    for(i=0;i<27;)
        ER32(Ct[1],Ct[0],rk[i++]);
}

void Speck64128Decrypt(u32 Pt[], const u32 Ct[], u32 const rk[])
{
    int i;
    Pt[0]=Ct[0]; Pt[1]=Ct[1];
    for(i=26;i>=0;)
        DR32(Pt[1],Pt[0],rk[i--]);
}

/******************************** dictionary ********************************/

/*
 * "classic" hash table for 64-bit key-value pairs, with linear probing.
 * It operates under the assumption that the keys are somewhat random 64-bit integers.
 * The keys are only stored modulo 2**32 - 5 (a prime number), and this can lead
 * to some false positives.
 */
static const u32 EMPTY = 0xffffffff;
static const u64 PRIME = 0xfffffffb;

/* allocate a hash table with `size` slots (12*size bytes) */
void dict_setup(u64 size)
{
	dict_size = size;

	A = malloc(sizeof(*A) * dict_size);
	if (A == NULL)
		err(1, "impossible to allocate the dictionary");
	for (u64 i = 0; i < dict_size; i++)
		A[i].k = EMPTY;
}

/* Insert the binding key |----> value in the dictionary */
void dict_insert(u64 key, u64 value)
{
    u64 h = murmur64(key) % dict_size_global - rank * dict_size;
    for (;;) {
        if (A[h].k == EMPTY)
            break;
        h += 1;
        if (h == dict_size)
            h = 0;
    }
    assert(A[h].k == EMPTY);
    A[h].k = key % PRIME;
    A[h].v = value;
}

/* Query the dictionary with this `key`.  Write values (potentially)
 *  matching the key in `values` and return their number. The `values`
 *  array must be preallocated of size (at least) `maxval`.
 *  The function returns -1 if there are more than `maxval` results.
 */
int dict_probe(u64 key, int maxval, u64 values[])
{
    u32 k = key % PRIME;
    u64 h = murmur64(key) % dict_size_global - rank * dict_size;
    int nval = 0;
    for (;;) {
        if (A[h].k == EMPTY)
            return nval;
        if (A[h].k == k) {
        	if (nval == maxval)
        		return -1;
            values[nval] = A[h].v;
            nval += 1;
        }
        h += 1;
        if (h == dict_size)
            h = 0;
   	}
}

/***************************** MITM problem ***********************************/

/* f : {0, 1}^n --> {0, 1}^n.  Speck64-128 encryption of P[0], using k */
u64 f(u64 k)
{
    assert((k & mask) == k);
    u32 K[4] = {k & 0xffffffff, k >> 32, 0, 0};
    u32 rk[27];
    Speck64128KeySchedule(K, rk);
    u32 Ct[2];
    Speck64128Encrypt(P[0], Ct, rk);
    return ((u64) Ct[0] ^ ((u64) Ct[1] << 32)) & mask;
}

/* g : {0, 1}^n --> {0, 1}^n.  speck64-128 decryption of C[0], using k */
u64 g(u64 k)
{
    assert((k & mask) == k);
    u32 K[4] = {k & 0xffffffff, k >> 32, 0, 0};
    u32 rk[27];
    Speck64128KeySchedule(K, rk);
    u32 Pt[2];
    Speck64128Decrypt(Pt, C[0], rk);
    return ((u64) Pt[0] ^ ((u64) Pt[1] << 32)) & mask;
}

bool is_good_pair(u64 k1, u64 k2)
{
    u32 Ka[4] = {k1 & 0xffffffff, k1 >> 32, 0, 0};
    u32 Kb[4] = {k2 & 0xffffffff, k2 >> 32, 0, 0};
    u32 rka[27];
    u32 rkb[27];
    Speck64128KeySchedule(Ka, rka);
    Speck64128KeySchedule(Kb, rkb);
    u32 mid[2];
    u32 Ct[2];
    Speck64128Encrypt(P[1], mid, rka);
    Speck64128Encrypt(mid, Ct, rkb);
    return (Ct[0] == C[1][0]) && (Ct[1] == C[1][1]);
}

/***************************** MPI functions ***********************************/

/* Allocate memory space for the buffers and the buffer counts. */
void setup_buffers()
{
    /* NOTE: The buffer_size describes the number of elements for ONE process.
       Therefore, the total number of elements that a process may hold is
       buffer_size * num_processes. */
    buffer_size = GET_BUFFER_SIZE(dict_size);
    buffers = malloc(sizeof(*buffers) * buffer_size * BUFFER_ELEMENT_SIZE * num_processes);
    buffers_counts = malloc(sizeof(*buffers_counts) * num_processes);

    for (int i = 0; i < num_processes; i++) {
        buffers_counts[i] = 0;
    }
}

/* Add an element to the buffer. Returns 1 if the element's buffer is full. */
int add_to_buffer(u64 key, u64 val)
{
    int h_rank = (murmur64(key) % dict_size_global) / dict_size;

    buffers[2 * buffer_size * h_rank + 2 * buffers_counts[h_rank]] = key;
    buffers[2 * buffer_size * h_rank + 2 * buffers_counts[h_rank] + 1] = val;
    buffers_counts[h_rank]++;

    return (buffers_counts[h_rank] == buffer_size)? 1 : 0;
}

/* Update the buffer occupancy counters. */
void update_buffer_occupancy_statistics()
{
    u64 num_elements = 0;
    for (int i = 0; i < num_processes; i++) {
        num_elements += buffers_counts[i];
    }
    num_exchanges += 1;
    cum_buffer_occupancy += (double) num_elements / (buffer_size * num_processes);
}

/* Exchange buffer sizes and buffers between processes. */
void exchange_buffers()
{
    update_buffer_occupancy_statistics();
    MPI_Alltoall(MPI_IN_PLACE, BUFFER_COUNT_MSG_SIZE, MPI_UINT64_T,
                 buffers_counts, BUFFER_COUNT_MSG_SIZE, MPI_UINT64_T,
                 MPI_COMM_WORLD);
    MPI_Alltoall(MPI_IN_PLACE, buffer_size * BUFFER_ELEMENT_SIZE,
                 MPI_UINT64_T, buffers, buffer_size * BUFFER_ELEMENT_SIZE,
                 MPI_UINT64_T, MPI_COMM_WORLD);
}

/* Insert elements from a buffer into the dict and flush the buffer. */
void batch_insert()
{
    u64 x, z;

    for (int i = 0; i < num_processes; i++) {
        for (u64 e = 0; e < buffers_counts[i]; e++) {
            z = buffers[buffer_size * BUFFER_ELEMENT_SIZE * i + BUFFER_ELEMENT_SIZE * e];
            x = buffers[buffer_size * BUFFER_ELEMENT_SIZE * i + BUFFER_ELEMENT_SIZE * e + 1];
            dict_insert(z, x);
        }
    }

    /* reset buffers' counters */
    for (int i = 0; i < num_processes; i++) {
        buffers_counts[i] = 0;
    }
}

/* Check if a solution has been found, resulting in an early exit. */
int solution_found(int nres)
{
    int nres_global;
    MPI_Allreduce(&nres, &nres_global, EARLY_EXIT_MSG_SIZE,
                  MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    return nres_global > 0;
}

/* Check elements of the buffer against the local dictionary. */
u64 batch_probe(int *nres, int maxres, u64 k1[], u64 k2[])
{
    u64 y, z;
    u64 x[N_PROBES_MAX];
    u64 ncandidates_partial = 0;

    for (int i = 0; i < num_processes; i++) {
        for (u64 e = 0; e < buffers_counts[i]; e++) {
            y = buffers[buffer_size * BUFFER_ELEMENT_SIZE * i + BUFFER_ELEMENT_SIZE * e];
            z = buffers[buffer_size * BUFFER_ELEMENT_SIZE * i + BUFFER_ELEMENT_SIZE * e + 1];

            int nx = dict_probe(y, N_PROBES_MAX, x);
            assert(nx >= 0);
            ncandidates_partial += nx;
            for (int i = 0; i < nx; i++)
                if (is_good_pair(x[i], z)) {
                    if (*nres == maxres)
                        return -1;
                    k1[*nres] = x[i];
                    k2[*nres] = z;
                    *nres += 1;
                }
        }
    }

    for (int i = 0; i < num_processes; i++) {
        buffers_counts[i] = 0;
    }

    return ncandidates_partial;
}

/* Set compression factor based on maximum memory available. */
void set_compression_factor(double memory_max)
{
    u64 dict_slots = 1.125 * (1ull << n) / num_processes;
    u64 buffers_slots = GET_BUFFER_SIZE(dict_slots) *
                        BUFFER_ELEMENT_SIZE * num_processes;
    u64 memory_required = (dict_slots * sizeof(*A) +
                           buffers_slots * sizeof(*buffers)) * num_processes;
    // NOTE: We put RELAXATION_FACTOR times the memory requirement as to not
    // overload the system
    int minimum_slices = RELAXATION_FACTOR * ceil(memory_required / (memory_max * GB));

    while ((1 << compress_factor) < minimum_slices) {
        compress_factor++;
    }
}

/* Print execution info for easier debugging. */
void print_execution_info()
{
    if (rank == ROOT_RANK) {
        printf("Running with n=%d, C0=(%08x, %08x) and C1=(%08x, %08x)\n",
               (int) n, C[0][0], C[0][1], C[1][0], C[1][1]);
        printf("Number of processes: %d\n", num_processes);
        printf("Compression level: %d (%d rounds)\n", compress_factor,
               1 << compress_factor);

        char hdsize_global[8], hdsize[8];

        human_format(dict_size_global * sizeof(*A), hdsize_global);
        human_format(dict_size * sizeof(*A), hdsize);
        printf("Global dictionary size: %sB (%sB per process)\n",
               hdsize_global, hdsize);

        human_format(GET_BUFFER_SIZE(dict_size) * BUFFER_ELEMENT_SIZE *
                     num_processes * sizeof(*buffers) * num_processes,
                     hdsize_global);
        human_format(GET_BUFFER_SIZE(dict_size) * BUFFER_ELEMENT_SIZE *
                     num_processes * sizeof(*buffers), hdsize);
        printf("Total buffer size: %sB (%sB per process)\n",
               hdsize_global, hdsize);
    }
}

/* Print average buffer occupancy. */
void print_average_buffer_occupancy()
{
    if (rank == ROOT_RANK) {
        MPI_Reduce(MPI_IN_PLACE, &cum_buffer_occupancy, 1, MPI_DOUBLE,
                   MPI_SUM, ROOT_RANK, MPI_COMM_WORLD);
        printf("Average buffer occupancy: %.2f%%\n",
               cum_buffer_occupancy / (num_exchanges * num_processes) * 100);
    } else {
        MPI_Reduce(&cum_buffer_occupancy, &cum_buffer_occupancy, 1,
                   MPI_DOUBLE, MPI_SUM, ROOT_RANK, MPI_COMM_WORLD);
    }
}

/* Print processing and communication times. */
void print_execution_times()
{
    if (rank == ROOT_RANK) {
        printf("Processing time: %.2fs\n", compute_time);
        printf("Communication time: %.2fs\n", communication_time);
        printf("Fill time: %.2fs\n", fill_time);
        printf("Probe time: %.2fs\n", probe_time);
    }
}

/* Print important statistics as a structured row of data. */
void print_statistics_as_structured_data()
{
    if (rank == ROOT_RANK) {
        printf(">>>%d,%d,%d,%.12f,%.12f,%.12f,%.12f,%.12f\n", (int) n,
               num_processes, compress_factor, compute_time, communication_time,
               fill_time, probe_time, cum_buffer_occupancy / (num_exchanges
               * num_processes) * 100);
    }
}

/******************************************************************************/

/* search the "golden collision" */
int golden_claw_search(int maxres, u64 k1[], u64 k2[])
{
    int nres = 0;

    /* step 0: initialize buffers */
    setup_buffers();

    int num_rounds = 1 << compress_factor;
    u64 N = 1ull << n;
    u64 xs_per_round = N >> compress_factor;

    double start_program = wtime();
    for (int round = 0; round < num_rounds; round++) {
        /* step 1: fill up the dictionaries (using cyclic load balancing) */
        double start_fill = wtime();
        u64 xs_per_process = xs_per_round / num_processes;
        u64 x_start = num_rounds * rank + round;
        u64 x_end = x_start + xs_per_process * num_processes * num_rounds;

        for (u64 x = x_start; x < x_end; x += num_processes * num_rounds) {
            u64 z = f(x);
            if (add_to_buffer(z, x)) {
                time_comm(exchange_buffers);
                batch_insert();
            }
        }

        /* set barrier informing that all the elements have been computed */
        MPI_Request fill_barrier;
        int all_fills_complete = 0;
        MPI_Ibarrier(MPI_COMM_WORLD, &fill_barrier);

        /* receive elements from other processes */
        do {
            time_comm(exchange_buffers);
            batch_insert();
            MPI_Test(&fill_barrier, &all_fills_complete, MPI_STATUS_IGNORE);
        } while (!all_fills_complete);
        fill_time += wtime() - start_fill;

        /* step 2: probe the dictionaries (also with cyclic load balancing) */
        double start_probe = wtime();
        u64 zs_per_process = N / num_processes;
        u64 z_start = rank;
        u64 z_end = z_start + zs_per_process * num_processes;

        for (u64 z = z_start; z < z_end; z += num_processes) {
            u64 y = g(z);
            if (add_to_buffer(y, z)) {
                time_comm(exchange_buffers);
                batch_probe(&nres, maxres, k1, k2);
                if (solution_found(nres) && EARLY_EXIT) {
                    probe_time += wtime() - start_probe;
                    compute_time = (wtime() - start_program) - communication_time;
                    return nres;
                }
            }
        }

        /* same non-blocking barrier strategy from the fill phase */
        all_fills_complete = 0;
        MPI_Ibarrier(MPI_COMM_WORLD, &fill_barrier);

        do {
            time_comm(exchange_buffers);
            batch_probe(&nres, maxres, k1, k2);
            if (solution_found(nres) && EARLY_EXIT) {
                probe_time += wtime() - start_probe;
                compute_time = (wtime() - start_program) - communication_time;
                return nres;
            }
            MPI_Test(&fill_barrier, &all_fills_complete, MPI_STATUS_IGNORE);
        } while (!all_fills_complete);
        probe_time += wtime() - start_probe;

        /* reset dictionaries */
        for (u64 i = 0; i < dict_size; i++)
            A[i].k = EMPTY;
    }

    compute_time = (wtime() - start_program) - communication_time;
    return nres;
}

/************************** command-line options ****************************/

void usage(char **argv)
{
        printf("%s [OPTIONS]\n\n", argv[0]);
        printf("Options:\n");
        printf("--n N                       block size [default 24]\n");
        printf("--C0 N                      1st ciphertext (in hex)\n");
        printf("--C1 N                      2nd ciphertext (in hex)\n");
        printf("--mem N                     memory available (in GB)\n");
        printf("\n");
        printf("All arguments are required\n");
        exit(0);
}

void process_command_line_options(int argc, char ** argv)
{
        struct option longopts[5] = {
                {"n", required_argument, NULL, 'n'},
                {"C0", required_argument, NULL, '0'},
                {"C1", required_argument, NULL, '1'},
                {"mem", required_argument, NULL, 'm'},
                {NULL, 0, NULL, 0}
        };
        char ch;
        int set = 0;
        double memory_max;
        while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
                switch (ch) {
                case 'n':
                        n = atoi(optarg);
                        mask = (1ull << n) - 1;
                        break;
                case '0':
                        set |= 1;
                        u64 c0 = strtoull(optarg, NULL, 16);
                        C[0][0] = c0 & 0xffffffff;
                        C[0][1] = c0 >> 32;
                        break;
                case '1':
                        set |= 2;
                        u64 c1 = strtoull(optarg, NULL, 16);
                        C[1][0] = c1 & 0xffffffff;
                        C[1][1] = c1 >> 32;
                        break;
                case 'm':
                        memory_max = atof(optarg);
                        set_compression_factor(memory_max);
                        break;
                default:
                        errx(1, "Unknown option\n");
                }
        }
        if (n == 0 || set != 3) {
        	usage(argv);
        	exit(1);
        }
}

/******************************************************************************/

int main(int argc, char **argv)
{
    /* MPI initialization */
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_processes);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    assert(
        num_processes != 0
        && (num_processes & (num_processes - 1)) == 0
        && "The number of processes must be a power of two."
    );

    process_command_line_options(argc, argv);

    /* setup the distributed dictionary strategy */
    dict_size = ceil(1.125 * (1ull << (n - compress_factor)) / num_processes);
    dict_size_global = dict_size * num_processes;
	dict_setup(dict_size);

    /* print some useful information */
    print_execution_info();

    /* search */
    u64 k1[16], k2[16];
    int nkey = golden_claw_search(16, k1, k2);

	/* validation; barriers to print all solutions together */
    MPI_Barrier(MPI_COMM_WORLD);
    for (int i = 0; i < nkey; i++) {
        assert(f(k1[i]) == g(k2[i]));
        assert(is_good_pair(k1[i], k2[i]));
        printf("Solution found: (%" PRIx64 ", %" PRIx64 ") [checked OK]\n", k1[i], k2[i]);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    /* print some post-processing statistics */
    print_average_buffer_occupancy();
    print_execution_times();
    print_statistics_as_structured_data();

    MPI_Finalize();
}
