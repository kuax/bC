#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <math.h>

typedef struct rBuf {
	FILE *inpfile;
	char *bitValues;
	int MAX_SIZE;
	int ELEMENT_SIZE;
	int current_size;
	int start_pos, write_pos;
	int end_reached;
} ReadingBuffer;

int printMem2(char *buf, int buf_size) {
	int i;
	char* run = buf;
	int val;
	for (i = 0; i < buf_size; i++) {
		val = (unsigned char) *run;
		printf("%d", val);
		run++;
	}
	printf("\n");
	return i;
}

int addBitsToBuf(char **cPtr, char c) {
	// Fill buf
	int nrBits = sizeof(char)*8;
	int mask = 1 << (nrBits-1);
	int i, d;
	for (i = 0; i < nrBits; i++) {
		d = (c & mask) != 0 ? 1 : 0;
		**cPtr = (char) d;
		(*cPtr)++;
		mask = mask >> 1;
	}
	return nrBits;
}

int intVal(char *buf, int len)
{
	char *run = buf;
	int val = 0;
	int i, tmp;
	for (i = 0; i < len; i++) {
		tmp = (unsigned char) *run;
		val += (tmp == 1) ? 1 : 0;
		run++;
		if (i < len-1)
			val = val << 1;
	}
	return val;
}

void sf_lz77d(char infile[], char outfile[]) {
	FILE *infptr, *outfptr;
	infptr = fopen(infile, "r");
	outfptr = fopen(outfile, "w");
	int buf_size, dict_size;
	static outputCounter = 0;

	// Get info about buffer and dictionary sizes
	fread(&buf_size, sizeof(int), 1, infptr);
	fread(&dict_size, sizeof(int), 1, infptr);
	printf("Buf size: %d\nDict size: %d\n", buf_size, dict_size);

	// Compute bits to read for each piece of information
	int idx_size = (int) ceil(log2(dict_size + 1));
	int len_size = (int) ceil(log2(buf_size - dict_size + 1));
	int char_size = sizeof(char) * 8;
	int tot_size = idx_size + len_size + char_size + 1;

	// TODO: Check why is 2 needed... Do I accidentally write past the end?
	char *buf = (char *) malloc(sizeof(char) * buf_size * 2);
	char *dict = buf;
	char *writerPtr, *seekerPtr;

	// Temporary buffer for storing bits to interpret as idx, len, size
	int tmp_buf_size = tot_size + 2 * 8 + 1; // 1 bit for the flag
	char *tmp_buf = (char *) malloc(sizeof(char) * tmp_buf_size);
	char *cur_pos1 = tmp_buf;
	char **cur_pos = &cur_pos1;

	int flag, idx, len;
	char nextChar;
	char c;
	while (!feof(infptr)) {
		fread(&c, 1, 1, infptr);
		// Add one character to tmp_buf
		addBitsToBuf(cur_pos, c);
//		printMem2(tmp_buf, (*cur_pos) - tmp_buf);

// Check if there is enough information to be able to decompress
		if ((*cur_pos) - tmp_buf >= char_size + 1) {
			int newCharsCount, remainder, consumedBits = 0;
			// Interpret values
			flag = intVal(tmp_buf, 1);
			//printf("Flag: %d\n", flag);
			// TEST: print flags in group of 8, to evaluate the frequency of sequences
//			printf("%d", flag);
//			if (++outputCounter % 8 == 0) {
//				printf("\n");
//				outputCounter = 0;
//			}
			if (flag == 0) {
				nextChar = (char) intVal(tmp_buf + 1, char_size);
				//printf("%c\n", nextChar);
				writerPtr = dict;
				*writerPtr = nextChar;
				writerPtr++;

				remainder = *cur_pos - tmp_buf - (char_size + 1);
				consumedBits += char_size + 1;
			} else {
				// Check if enough data in tmp buffer!
				if ((*cur_pos) - tmp_buf < tot_size)
					continue;

				idx = intVal(tmp_buf + 1, idx_size);
				len = intVal(tmp_buf + 1 + idx_size, len_size);
				nextChar = (char) intVal(tmp_buf + 1 + idx_size + len_size,
						char_size);
				//printf("%d %d %c\n", idx, len, nextChar);

				// Fetch chars to write
				seekerPtr = dict;
				writerPtr = dict;
				seekerPtr -= idx;
				int i;
				for (i = 0; i < len; i++) {
					*writerPtr = *seekerPtr;
					seekerPtr++;
					writerPtr++;
				}
				// add last char
				*writerPtr = nextChar;
				writerPtr++;

				remainder = *cur_pos - tmp_buf - tot_size;
				consumedBits += tot_size;
			}

			// Write decompressed characters to file
			newCharsCount = writerPtr - dict;
			//fwrite(dict, newCharsCount, sizeof(char), outfptr);
			int i;
			for (i = 0; i < newCharsCount; i++) {
				fwrite((dict + i), 1, 1, outfptr);
			}

			// Remove interpreted bits from tmp_buf
			for (i = 0; i < remainder; i++)
				*(tmp_buf + i) = *(tmp_buf + i + consumedBits);
			*cur_pos = tmp_buf + remainder;

			// Update dictionary size if not already full size
			dict = (dict - buf < dict_size) ?
					dict + newCharsCount : buf + dict_size;

			// Update current dictionary
			int offset = writerPtr - dict;
			if (offset > 0) {
				for (i = 0; i < dict - buf; i++) {
					*(buf + i) = *(buf + i + offset);
				}
			}

		}
	}

	fclose(outfptr);
	fclose(infptr);
	free(tmp_buf);
	free(buf);

}

ReadingBuffer initReadingBuffer(FILE *f, int element_size) {
	ReadingBuffer rb;
	rb.inpfile = f;
	rb.ELEMENT_SIZE = element_size;
	rb.MAX_SIZE = element_size + 7;
	rb.bitValues = (char *) malloc(sizeof(char) * rb.MAX_SIZE);
	rb.current_size = rb.start_pos = rb.write_pos = rb.end_reached = 0;
	return rb;
}

int lzDecompress(char outfile[]) {

	/* ------------------------- SET UP ------------------------- */

	// Open input/output files (check for errors)
	FILE *outfptr;
	outfptr = fopen(outfile, "w");
	if (outfptr == NULL) {
		printf("Errore durante l'apertura del file in output.\n");
		return -1;
	}

	FILE *flagFile, *offsetFile, *lengthFile, *nextFile;
	flagFile = fopen("_flags.lztmp", "r");
	offsetFile = fopen("_offsets.lztmp", "r");
	lengthFile = fopen("_lengths.lztmp", "r");
	nextFile = fopen("_next.lztmp", "r");
	if (flagFile == NULL || offsetFile == NULL || lengthFile == NULL
			|| nextFile == NULL) {
		printf("Errore di apertura di uno dei files temporanei in lettura\n");
		return -1;
	}

	// Retrieve sizes of window and look ahead buffers
	short window_size, look_ahead_size;
	fread(&window_size, sizeof(short), 1, flagFile);
	fread(&look_ahead_size, sizeof(short), 1, flagFile);
	printf("Window size: %d\nLook Ahead size: %d\n", window_size,
			look_ahead_size);

	// Initialize variables
	int flagBitSize = 1;
	int offsetBitSize = (int) ceil(log2(window_size - look_ahead_size + 1));
	int lengthBitSize = (int) ceil(log2(look_ahead_size - 1));
	int nextBitSize = 8;

	// Initzalize reading buffers
	ReadingBuffer flagRB, offsetRB, lengthRB, nextRB;
	flagRB = initReadingBuffer(flagFile, flagBitSize);
	offsetRB = initReadingBuffer(offsetFile, offsetBitSize);
	lengthRB = initReadingBuffer(lengthFile, lengthBitSize);
	nextRB = initReadingBuffer(nextFile, nextBitSize);

	/* ------------------------- DECOMPRESS ------------------------- */

	// Retrieve window and look ahead buffer sizes
	// Loop while there is enough information to be able to decompress

	/* ------------------------- CLEAN UP ------------------------- */
	// Free allocated memory
	free(nextRB.bitValues);
	free(lengthRB.bitValues);
	free(offsetRB.bitValues);
	free(flagRB.bitValues);

	// Close input/output files
	fclose(nextFile);
	fclose(lengthFile);
	fclose(offsetFile);
	fclose(flagFile);

	fclose(outfptr);

	return 0;
}
