#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include "omt.h"

#define KEY_SIZE (20)
#define VAL_SIZE (100)
static uint64_t FLAGS_num = 1000000;
static const char* FLAGS_benchmarks = "fillrandom";

long long get_ustime_sec(void)
{
	struct timeval tv;
	long long ust;

	gettimeofday(&tv, NULL);
	ust = ((long long)tv.tv_sec) * 1000000;
	ust += tv.tv_usec;
	return ust / 1000000;
}

void _random_key(char *key, int length)
{
	int i;
	char salt[36] = "abcdefghijklmnopqrstuvwxyz0123456789";

	for (i = 0; i < length; i++)
		key[i] = salt[rand() % 36];
}

void _bench(uint64_t n, int random)
{
	int i;
	int done = 0;;
	int next_report = 100;
	double cost;
	long long start, end;

	struct omt_tree *omt;
	char kbuf[KEY_SIZE];

	omt = omt_new();

	start = get_ustime_sec();
	for (i = 0; i < n; i++) {

		int key = random ? rand() % FLAGS_num : i;
		memset(kbuf, 0, KEY_SIZE);
		snprintf(kbuf, KEY_SIZE, "%016d", key);
		struct slice k = {.data = kbuf, .size = strlen(kbuf)};
		omt_insert(omt, &k);

		done++;
		if (done >= next_report) {
			if (next_report < 1000)   next_report += 100;
			else if (next_report < 5000)   next_report += 500;
			else if (next_report < 10000)  next_report += 1000;
			else if (next_report < 50000)  next_report += 5000;
			else if (next_report < 100000) next_report += 10000;
			else if (next_report < 500000) next_report += 50000;
			else                            next_report += 100000;
			fprintf(stderr,
			        "%s finished %d ops%30s\r",
			        "insert",
			        done,
			        "");
			fflush(stderr);
		}
	}
	end = get_ustime_sec();
	cost = end - start;
	printf("write [%" PRIu64 "]: %.6f sec/op; %.1f ops/sec(estimated); cost:%.3f(sec)\n",
	       n,
	       (double)(cost / n),
	       (double)(n / cost),
	       cost);

	omt_free(omt);
}

void run()
{
	const char* benchmarks = FLAGS_benchmarks;
	if (strncmp("fillseq", benchmarks, 7) == 0) {
		_bench(FLAGS_num, 0);
	} else if (strncmp("fillrandom", benchmarks, 10) == 0) {
		_bench(FLAGS_num, 1);
	} else {
		fprintf(stderr, "unknown benchmark '%s'\n", benchmarks);
	}
}

int main(int argc, char *argv[])
{

	int i;

	for (i = 1; i < argc; i++) {
		long n;
		char junk;

		if (strncmp(argv[i], "--benchmarks=", 13) == 0) {
			FLAGS_benchmarks = argv[i] + strlen("--benchmarks=");
		} else if (sscanf(argv[i], "--num=%ld%c", &n, &junk) == 1) {
			FLAGS_num = n;
		} else {
			fprintf(stderr, "invalid flag %s\n", argv[i]);
			exit(1);
		}
	}

	run();

	return 1;
}
