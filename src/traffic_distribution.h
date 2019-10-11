#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _traffic_distribution_h
#define _traffic_distribution_h

#include "iperf.h"

#define STATS_FILE_CHUNK_SIZE                                   512
#define IPERF_PACKET_LENGTH_TABLE_MAX_SIZE                      10000

typedef struct {
    uint16_t    length;
    uint32_t    frequency;
    double      probability;
} iperf_traffic_values;

typedef struct {
    char                        *file;
    uint32_t                    valuesCount;
    iperf_traffic_values        *values;
    uint16_t                    length_table_size;
    uint16_t                    length_table[IPERF_PACKET_LENGTH_TABLE_MAX_SIZE];
    uint16_t			longest_packet_length;
} iperf_traffic;

extern int loadTrafficTable(struct iperf_test *test);

extern void testTrafficDistribution(void);

extern iperf_traffic *getTrafficDistribution(void);

extern uint16_t getPacketLength(void);

#endif

#ifdef __cplusplus
} /* end extern "C" */

#endif

