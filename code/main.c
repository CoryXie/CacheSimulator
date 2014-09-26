
/*
 * main.c
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "cache.h"
#include "debug.h"
#include "main.h"

static FILE *traceFile;


int main(int argc, char ** argv)
    {
    dbg_init(MODULE_CACHE, 0, NULL);
    parse_args(argc, argv);
    init_cache();
    play_trace(traceFile);
    print_stats();
    return 0;
    }

/************************************************************/
void parse_args(int argc, char ** argv)
    {
    int index;
    int value;
    int c;
    
    opterr = 0;
    
    while ((c = getopt (argc, argv, "hb:u:i:d:w:p:a:t:")) != -1)
      switch (c)
        {
        case 'h':
            printf("\t-h : show this message\n");
            printf("\t-b <bs>: set cache block size to <bs>\n");
            printf("\t-u <us>: set unified cache size to <us>\n");
            printf("\t-i <is>: set instruction cache size to <is>\n");
            printf("\t-d <ds>: set data cache size to <ds>\n");
            printf("\t-w <wy>: set cache associativity to <wy>\n");
            printf("\t-p <wr>: set write policy to <wr> (wb or wt)\n");
            printf("\t-a <al>: set allocation policy to <al> (wa or wn)\n");
            printf("\t-t <tr>: set trace file name to <tr>\n");
            exit(0);
            break;
        case 'b':
            value = atoi(optarg);
            set_cache_param(CACHE_PARAM_BLOCK_SIZE, value);
            break;
        case 'u':
            value = atoi(optarg);
            set_cache_param(CACHE_PARAM_USIZE, value);
            break;
        case 'i':
            value = atoi(optarg);
            set_cache_param(CACHE_PARAM_ISIZE, value);
            break;
        case 'd':
            value = atoi(optarg);
            set_cache_param(CACHE_PARAM_DSIZE, value);
            break;
        case 'w':
            value = atoi(optarg);
            set_cache_param(CACHE_PARAM_ASSOC, value);
            break;
        case 'p':
            
          if (!strcmp(optarg, "-wt"))
              {
              set_cache_param(CACHE_PARAM_WRITETHROUGH, TRUE);
              }
          else if (!strcmp(optarg, "-wb"))
              {
              set_cache_param(CACHE_PARAM_WRITEBACK, TRUE);
              }
          
            break;
        case 'a':
            
          if (!strcmp(optarg, "-wa"))
              {
              set_cache_param(CACHE_PARAM_WRITEALLOC, TRUE);
              }
          else if (!strcmp(optarg, "-wn"))
              {
              set_cache_param(CACHE_PARAM_NOWRITEALLOC, TRUE);
              }
          
            break;
        case 't':
            /* open the trace file */
            traceFile = fopen(optarg, "r");
            if (traceFile == NULL)
                {
                fprintf (stderr, "trace file %s not opened\n", optarg);
                exit (-1);
                }
            break;
        case '?':
          if ((optopt == 'b') ||
              (optopt == 'u') ||
              (optopt == 'i') ||
              (optopt == 'd') ||
              (optopt == 'w') ||
              (optopt == 'p') ||
              (optopt == 'a') ||
              (optopt == 't'))
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
          else if (isprint (optopt))
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
          else
            fprintf (stderr,
                     "Unknown option character `\\x%x'.\n",
                     optopt);
        default:
          exit (-1);
        }

    for (index = optind; index < argc; index++)
      printf ("Non-option argument %s\n", argv[index]);

    dump_settings();

    return;
    }

/************************************************************/

/************************************************************/
void play_trace(FILE * inFile)
    {
    cpu_addr_t addr;
    unsigned access_type;
    int num_inst;

    num_inst = 0;
    while(read_trace_element(inFile, &access_type, &addr))
        {

        switch (access_type)
            {
            case TRACE_DATA_LOAD:
            case TRACE_DATA_STORE:
            case TRACE_INST_LOAD:
                perform_access(addr, access_type);
                break;

            default:
                printf("skipping access, unknown type(%d)\n", access_type);
            }

        num_inst++;
        if (!(num_inst % PRINT_INTERVAL))
            printf("processed %d references\n", num_inst);
        }

    flush();
    }
/************************************************************/

/************************************************************/
int read_trace_element(FILE * inFile, unsigned * access_type, cpu_addr_t * addr)
    {
    int result;
    char c;

    result = fscanf(inFile, "%u %x%c", access_type, addr, &c);
    while (c != '\n')
        {
        result = fscanf(inFile, "%c", &c);
        if (result == EOF)
            break;
        }
    if (result != EOF)
        return(1);
    else
        return(0);
    }
/************************************************************/
