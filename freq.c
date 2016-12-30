#include <stdio.h>

void printFreq(double counters[], int len)
{
	int i;
	for (i = 0; i < len; i++) {
		if (counters[i] != 0)
			printf("%-5d %6f\n", i, counters[i]);
	}
}

int main()
{
	// Frequency analyzer
	FILE *infile;
	int counters[256];
	double freqs[256];
	int tot = 0;

	// Set all counters to 0
	int i;
	for (i = 0; i < 256; i++) {
		counters[i] = 0;
	}

	infile = fopen("../testFiles/ecl.gz", "r");
	char c;
	while(!feof(infile)) {
		fscanf(infile, "%c", &c);
		counters[c]++;
		tot++;
	}

	// Compute frequencies
	for (i = 0; i < 256; i++) {
		freqs[i] = (double) counters[i]/tot;
	}
	printFreq(freqs, 256);

	fclose(infile);
}
