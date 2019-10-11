#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#include "traffic_distribution.h"

#ifdef __cplusplus
extern "C"
{
#endif

static iperf_traffic traffic_distribution = {0};

static unsigned long long getMicroTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return(((unsigned long long)tv.tv_sec * (unsigned long long)1000000) +
           (unsigned long long)tv.tv_usec);
}

#define RS_DEVICE           1
#define RS_TRY_DEVICE       2
#define RS_CLIB             3

static int RandomSource = RS_TRY_DEVICE;
static FILE *RandomDevice = NULL;

static uint32_t random32()
{
    uint32_t number;
    switch(RandomSource) {
        case RS_DEVICE:
            if (fread(&number, sizeof(number), 1, RandomDevice) == 1) {
                return(number);
            }
            RandomSource = RS_CLIB;
        case RS_TRY_DEVICE:
            RandomDevice = fopen("/dev/urandom", "r");
            if (RandomDevice != NULL) {
                if (fread(&number, sizeof(number), 1, RandomDevice) == 1) {
                    srandom(number);
                    RandomSource = RS_DEVICE;
                    return(number);
                }
                fclose(RandomDevice);
            }
            RandomSource = RS_CLIB;
            srandom((unsigned int)(getMicroTime() & (uint64_t)0xffffffff));
        break;
    }
    const uint16_t a = random() & 0xffff;
    const uint16_t b = random() & 0xffff;
    return( (((uint32_t)a) << 16) | (uint32_t)b);
}

void testTrafficDistribution(void)
{
    double totalPercentage = 0;
    for (uint32_t i = 0; i < traffic_distribution.valuesCount; i++) {

        printf("packet frequency: %hu, length: %u, probability: %.6f\n",
                traffic_distribution.values[i].frequency,
                traffic_distribution.values[i].length,
                traffic_distribution.values[i].probability);

        totalPercentage += traffic_distribution.values[i].probability;
    }
    printf("Total Percentage: %.2f\n", totalPercentage);

    for (uint32_t i = 0; i < IPERF_PACKET_LENGTH_TABLE_MAX_SIZE; i++) {
        printf("lengths[%u] = %u\n", i, traffic_distribution.length_table[i]);
    }
    uint32_t r;
    uint32_t *sizes = (uint32_t *) calloc(traffic_distribution.longest_packet_length + 1, sizeof(uint32_t));
    if (!sizes) {
	printf("Could not allocate memory.\n");
        return;
    }

    uint32_t index = 0;
    uint16_t length;
    // Test the probability-weighted distribution.
    for (uint32_t i = 0; i < 1000000; i++) {
        r = random32();
        index = (uint32_t)(traffic_distribution.length_table_size * \
                           (double)((double)r / (double)UINT32_MAX));
        printf("Random Number: %u ==> length table index: %u\n", r, index);
        // Lookup the packet length
        length = traffic_distribution.length_table[index];
	printf("packet length = %u\n", length);
	// Restrict lengths to something reasonable
	if (length <= traffic_distribution.longest_packet_length) {
        	sizes[length]++;
    	}
    }

    for (uint32_t i = 0; i <= traffic_distribution.longest_packet_length; i++) {
        printf("sizes[%u]: %u\n", i, sizes[i]);
    }

    return;
}

int loadTrafficTable(struct iperf_test *test)
{
    // Load .csv file and calculate probabilities of each packet length
    int                     rc = 0;
    FILE                    *f = NULL;
    iperf_traffic           *stats = &traffic_distribution;

    if (!stats->file) {
        return 0;
    }

    f = fopen(stats->file, "r");
    if (!f) {
        fprintf(stderr, "Could not open traffic packet length distribution file: %s\n",
                stats->file);
        rc = -1;
        goto cleanup;
    }
    // TODO: Guess size based on file size...
    stats->values = (iperf_traffic_values *) malloc(STATS_FILE_CHUNK_SIZE *\
                                                    sizeof(iperf_traffic_values));
    if (!stats->values) {
        fprintf(stderr, "Out of memory\n");
        rc = -1;
        goto cleanup;
    }
    stats->longest_packet_length = 0;
    stats->valuesCount = 0;
    char line[100];
    bool headers = false;
    uint32_t totalFrequency = 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        if (headers) {
            headers = false;
        } else {
            char *save_ptr;
            const char *freq = strtok_r(line, ",", &save_ptr);
            const char *length = strtok_r(NULL, ",\n", &save_ptr);
            //printf("line[%d]: %s, %s\n", stats->valuesCount, freq, length);
            // TODO: use better string conversion functions
            stats->values[stats->valuesCount].length = atoi(length);
            stats->values[stats->valuesCount].frequency = atoi(freq);
            totalFrequency += stats->values[stats->valuesCount].frequency;
            stats->valuesCount++;
            if ((stats->valuesCount % STATS_FILE_CHUNK_SIZE) == 0) {
                stats->values = (iperf_traffic_values *) realloc(stats->values,
                                (stats->valuesCount + STATS_FILE_CHUNK_SIZE) *\
                                sizeof(iperf_traffic_values));
            }
            if (atoi(length) > stats->longest_packet_length) {
                stats->longest_packet_length = atoi(length);
            }
        }
    }

    //printf("stats->longest_packet_length: %u\n", stats->longest_packet_length);

    // Calculate probability of packet length
    for (uint32_t i = 0; i < stats->valuesCount; i++) {
        stats->values[i].probability = (double)stats->values[i].frequency /\
                                       (double)totalFrequency;
    }
    //printf("stats count: %u\n", stats->valuesCount);
    stats->length_table_size = 0;
    for (uint32_t i = 0; i < stats->valuesCount; i++) {
        //printf("Packet Length: %u, Frequency: %u, Probability: %.5f\n",
        //       stats->values[i].length, stats->values[i].frequency, stats->values[i].probability);
        uint16_t entryCount = stats->values[i].probability * IPERF_PACKET_LENGTH_TABLE_MAX_SIZE;
        //printf("EntryCount: %u\n", entryCount);
        for (uint32_t j = stats->length_table_size; j < (stats->length_table_size + entryCount); j++) {
            stats->length_table[j] = stats->values[i].length;
            //printf("table[%u]: length: %u\n", j, stats->values[i].length);
        }
        stats->length_table_size += entryCount;
    }
    //printf("ending length_table_size: %u\n", stats->length_table_size);
cleanup:
    fclose(f);

    /*
    // Enable this code to test the traffic distribution
    testTrafficDistribution();
    return -1;
    */

    test->settings->blksize = stats->longest_packet_length;

    return rc;
}

iperf_traffic *getTrafficDistribution(void)
{
    return &traffic_distribution;
}

uint16_t getPacketLength(void)
{
    uint32_t r = random32();
    uint32_t index = (uint32_t)(traffic_distribution.length_table_size *\
                               (double)((double)r / (double)UINT32_MAX));
    //printf("Random Number: %u ==> length table index: %u\n", r, index);
    // Lookup the packet length
    return traffic_distribution.length_table[index];
}

#ifdef __cplusplus
}
#endif
