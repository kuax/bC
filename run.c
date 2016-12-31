#include "sf_lz77.h"
#include "sf_lz77d.h"
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#define BUF_SIZE 511
#define DICT_SIZE 504

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Numero di argomenti insufficiente!\n");
		return 1;
	} else {
		if (strcmp(argv[1], "-c") == 0) {
			char *infile = strcat(argv[2], "\0");
			// Launch compression
			//printf("--- Compression ---\n");
			printf("%s\n", infile);
			long int inpfsize;
			struct stat inputfstat;
			if (stat(infile, &inputfstat) == 0)
				inpfsize = inputfstat.st_size;

//			printf("BUF_SIZE\tDICT_SIZE\tLA_SIZE\tRATIO\tTIME\n");
			printf("BUF_SIZE\tDICT_SIZE\tLA_SIZE\tTIME\n");

			int i, j;
			for (i = 10; i < 11; i++) { // to test all range: 7 - 12
				for (j = 4; j < 5; j++) { // to test all range: 3 - (i-1)
					int d_s = 1 << i;
					int la_s = 1 << j;
					//b_s--;
					//la_s--;
					int b_s = d_s + la_s;

					clock_t begin = clock();
					//sf_lz77(infile, outfile, b_s, d_s);
					lzCompress(infile, d_s, la_s);
					clock_t end = clock();

//					long int outfsize;
//					struct stat outputfstat;
//					if (stat(outfile, &outputfstat) == 0)
//						outfsize = outputfstat.st_size;
//
//					double comp_ratio = (double) outfsize / inpfsize * 100.0;

					double tot_time = (double) (end - begin) / CLOCKS_PER_SEC;
//					printf("%d\t%d\t%d\t%.2f\t%.2f\n", (b_s), (d_s), (la_s), comp_ratio, tot_time);
					printf("%d\t%d\t%d\t%.2f\n", (b_s), (d_s), (la_s),
							tot_time);
				}
			}
			printf("\n");
			return 0;
		} else if (strcmp(argv[1], "-d") == 0) {
			char *outfile = strcat(argv[2], "\0");
			// Decompression
			printf("\n--- Decompression ---\n");
			//sf_lz77d(infile, outfile);
			lzDecompress(outfile);
			return 0;
		} else {
			fprintf(stderr, "Argomento non valido!\n");
			return 2;
		}
	}

	return 0;
}
