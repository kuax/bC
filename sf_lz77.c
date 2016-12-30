#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

typedef struct slWindow {
	char *window;
	int dictionary_position, look_ahead_position, end_position;
	short full_size_dictionary_flag;
	// TODO: check which info is REALLY necessary
	int MAX_WINDOW_SIZE, MAX_DICTIONARY_SIZE, MAX_LOOK_AHEAD_SIZE;
	int window_size, dictionary_size, current_look_ahead_size;
} SlidingWindow;

typedef struct wBuf {
	FILE *outfile;
	char *bitValues;
	int MAX_SIZE, current_size, start_position, write_position;
} WritingBuffer;

typedef struct match {
	int offset;
	int length;
	char next;
} Match;

#ifdef OLDCODE

void showBits(int n, int bits) {
	int mask = 1 << (bits-1);
	int i, d;
	for (i = 0; i < bits; i++) {
		d = (n & mask) != 0 ? 1 : 0;
		printf("%d", d);
		mask = mask >> 1;
	}
	printf("\n");
}

void flushBuf(FILE *fp, char *buf, char **curPos, int empty) {
	// Write as many chars as you can
	char *run = buf;
	char c;
	int i;

	while (*curPos-run >= 8) {
		c = 0;
		// Build char
		for (i = 0; i < 8; i++) {
			c += (*run);
			//printf("%d", *run);
			run++;
			if (i < 7)
				c = c << 1;
		}
		//printf(" ");
		fwrite(&c, 1, 1, fp);
	}
	//printf("\n");

	if (empty != 0) {
		// Empty the buffer!
		c = 0;
		int elems = *curPos-run;
		// Write bits left in buffer
		for (i = 0; i < elems; i++) {
			c += (*run);
			run++;
			c = c << 1;
		}
		// Shift to the left as needed
		c = c << (8-elems);
		if (elems > 0) // Only if there actually was something to write
			fwrite(&c, 1, 1, fp);
	}

	// Shift remaining buffer elements to the left
	int offset = run-buf;
	int final_pos = *curPos-run;
	while (run < *curPos) {
		*(run-offset) = *run;
		run++;
	}
	*curPos = buf+final_pos;
}

void writeInt(FILE *fp, char *buf, char **curPos, int val, int nrBits,
		int output) {

	// Fill buf
	int mask = 1 << (nrBits-1);
	int i, d;
	for (i = 0; i < nrBits; i++) {
		d = (val & mask) != 0 ? 1 : 0;
		**curPos = (char) d;
		if (output)
			printf("%d", d);
		(*curPos)++;
		mask = mask >> 1;
	}
	if (output)
		printf("\n");

	// Write to file if there are bits enough
	if (*curPos-buf >= 8)
		flushBuf(fp, buf, curPos, 0);
}

void writeChar(FILE *fp, char *buf, char **curPos, char val, int output) {
	writeInt(fp, buf, curPos, val, 8, output);
}

int shiftFill(FILE* fp, char* mem, int buf_size, int offset) {
	// Check parameters
	if (offset > buf_size)
		return -1;

	char *run = mem;
	int i = 0, charcount = 0;

	// Shift
	if (offset != 0) {
		for (i = 0; i < buf_size - offset; i++)
			*(run + i) = *(run + i + offset);
	}

	// startidx depends on wether a shift was made
	int startidx = 0;
	if (offset != 0)
		startidx = buf_size - offset;

	// Fill
	char c;
	for (i = startidx; i < buf_size; i++) {
		fread(&c, 1, 1, fp);

		if (feof(fp)) {
			break;
		}
		*(run + i) = c;
		charcount++;
	}

	return charcount;
}

int printMem(char *buf, int buf_size) {
	int i;
	char* run = buf;
	for (i = 0; i < buf_size; i++) {
		printf("%c", *run);
		run++;
	}
	printf("\n");
	return i;
}

int findRepetitions(FILE *fp, char *buf, char *dict, char *la, int scansize, char *bitsBuf, char **curBitsPos, int idxBits, int lenBits) {
	int len = 0, idx = 0, tmpidx, tmplen;
	char *la_copy = la;
	int dictsize = la - dict;

	int i, j, nextOccurrence, changed;
	char c, d; // c: current, d: dictionary
	// Scan through dictionary
	for (i = 0; i < dictsize; i++) {
		nextOccurrence = 0;
		changed = 0;
		la_copy = la; // reset LAptr
		tmplen = 0;
		tmpidx = i;
		j = i;
		d = *(dict + i);
		c = *la_copy;
		while (d == c && (j - i) < scansize - dictsize - 1) { // -1 so the last char of LAbuf remains

			tmplen++;
			j++;
			la_copy++;
			d = *(dict + j);

			// "PERFORMANCE BOOST TEST": if first character of LA is found in current occurrence
			// jump to it on next iteration
//			if (changed == 0) {
//				if (d == *la)
//					changed = 1;
//				else
//					nextOccurrence++;
//			}

			c = *la_copy;
		}

		// "PERFORMANCE BOOST TEST"
//		i += nextOccurrence;

		if (tmplen >= len && tmplen > 0) {
			len = tmplen;
			idx = tmpidx;
		}
	}

	char nextchar = *(la + len);
	int matchpos = (len == 0) ? 0 : dictsize - idx;
	// Debug: print result in user friendly mode
	//printf("(%d, %d)%c\n", matchpos, len, nextchar);
	if (len <= 2) {
		writeInt(fp, bitsBuf, curBitsPos, 0, 1, 0);
		char currentChar = *la;
		writeChar(fp, bitsBuf, curBitsPos, currentChar, 0);
		return 1;
	} else {
		//write flag bit '1'
		writeInt(fp, bitsBuf, curBitsPos, 1, 1, 0);

		writeInt(fp, bitsBuf, curBitsPos, matchpos, idxBits, 0);
		writeInt(fp, bitsBuf, curBitsPos, len, lenBits, 0);
		writeChar(fp, bitsBuf, curBitsPos, nextchar, 0);
		return len + 1;
	}

}

void sf_lz77(char infile[], char outfile[], int buf_size, int dict_size) {
	FILE *infptr, *outfptr;
	infptr = fopen(infile, "r");
	outfptr = fopen(outfile, "w");
	char *window = (char *) malloc(sizeof(char) * buf_size);
	char *Dptr = window + dict_size; // Dictionary pointer (initially equal to LAptr
	char *LAptr = window + dict_size; // Look Ahead pointer

	// Check file opening success
	if (infptr == NULL) {
		printf("Errore durante l'apertura del file %s\n", infile);
		return;
	}

	// compute sizes of numbers to store in bits
	int idx_size = (int) ceil(log2(dict_size + 1));
	int len_size = (int) ceil(log2(buf_size - dict_size + 1));
	int char_size = sizeof(char) * 8;
	char *bitsBuf = (char *) malloc(
			sizeof(char) * (idx_size + len_size + char_size + 8));
	char *curBitsPos = bitsBuf;
	char **curBP = &curBitsPos;

	// Write lengths as integers in first position of output file
	fwrite(&buf_size, sizeof(int), 1, outfptr);
	fwrite(&dict_size, sizeof(int), 1, outfptr);

	// Initial fill and compression (until dict_size has been reached)
	int offset = buf_size - dict_size; // DEBUG: +1 ?
	int actualsize = 1, scansize = 0;
	actualsize = shiftFill(infptr, window, buf_size, offset);
	while (scansize < buf_size && actualsize > 0) {
		scansize += actualsize;
		offset = findRepetitions(outfptr, window, Dptr, LAptr, scansize,
				bitsBuf, curBP, idx_size, len_size);

		Dptr -= offset; // Increase dictionary size
		actualsize = shiftFill(infptr, window, buf_size, offset);
		//i += offset;
	}
	if (LAptr - Dptr > dict_size)
		Dptr = LAptr - dict_size;
	if (scansize > buf_size)
		scansize = buf_size;

	// MAIN LOOP
	while (scansize > dict_size) {

		// Pointers reset
		Dptr = window;
		LAptr = window + dict_size;

		// Find repetitions
		offset = findRepetitions(outfptr, window, Dptr, LAptr, scansize,
				bitsBuf, curBP, idx_size, len_size);
		actualsize = shiftFill(infptr, window, buf_size, offset);

		// Compute real window size to scan
		scansize += actualsize;
		scansize -= offset;
	}
	flushBuf(outfptr, bitsBuf, curBP, 1);

	// Close resources and free allocated memory
	free(bitsBuf);
	free(window);
	fclose(outfptr);
	fclose(infptr);
}

#endif

Match findMatch(SlidingWindow *sw) {
	// Initialize default match
	Match m;
	m.offset = 0;
	m.length = 0;
	m.next = *((sw->window) + sw->look_ahead_position); // First char in look ahead

	// Search through dictionary for better match
	int currentPosition = sw->dictionary_position;
	int maxPosition = sw->look_ahead_position;
	int currentLength, currentOffset;
	char currentDictionaryChar, currentLookAheadChar;

	while (currentPosition != maxPosition) {
		// Reset temporary variables
		currentLength = currentOffset = 0;
		currentDictionaryChar = *((sw->window) + currentPosition);
		currentLookAheadChar = *((sw->window) + sw->look_ahead_position);

		// Find current match
		while (currentDictionaryChar == currentLookAheadChar
				&& currentLength < sw->MAX_LOOK_AHEAD_SIZE) {
			currentLength++;
			int tmpDictionaryPosition = (currentPosition + currentLength)
					% sw->MAX_WINDOW_SIZE;
			int tmpLookAheadPosition = (sw->look_ahead_position + currentLength)
					% sw->MAX_WINDOW_SIZE;
			currentDictionaryChar = *((sw->window) + tmpDictionaryPosition);
			currentLookAheadChar = *((sw->window) + tmpLookAheadPosition);
		}

		// Update best match
		if (currentLength > 0 && currentLength >= m.length) {
			int tmpOffset = currentPosition - sw->look_ahead_position;
			m.offset =
					(tmpOffset < 0) ?
							sw->MAX_WINDOW_SIZE + tmpOffset : tmpOffset;
			m.length = currentLength;
			m.next = currentLookAheadChar;
		}

		// Move search position forward for next iteration
		currentPosition++;
		currentPosition %= sw->MAX_WINDOW_SIZE;
	}

	return m;
}

void writeOut(WritingBuffer *wb, int value, int nrBits) {
	// Convert value to bits and store them in writing buffer
	int mask = 1 << (nrBits - 1);
	int i, d;
	for (i = 0; i < nrBits; i++) {
		d = (value & mask) != 0 ? 1 : 0;
		*(wb->bitValues + wb->write_position) = (char) d;
		wb->write_position++;
		wb->write_position %= wb->MAX_SIZE;
		wb->current_size++;
		mask = mask >> 1;
	}

	// Write to output file if there are enough bits
	while (wb->current_size >= 8) {
		char c = 0;
		// Build char
		for (i = 0; i < 8; i++) {
			c += *(wb->bitValues + wb->start_position);
			wb->current_size--;
			wb->start_position++;
			wb->start_position %= wb->MAX_SIZE;
			if (i < 7)
				c = c << 1;
		}
		// Write to output
		fwrite(&c, 1, 1, wb->outfile);
	}
}

void fillWindow(SlidingWindow *sw, FILE *inputfile, int charCount) {
	// Set new look ahead position
	sw->look_ahead_position += charCount;
	sw->look_ahead_position %= sw->MAX_WINDOW_SIZE;
	sw->current_look_ahead_size -= charCount;

	// Set new dictionary position if necessary
	if (sw->full_size_dictionary_flag) {
		sw->dictionary_position += charCount;
		sw->dictionary_position %= sw->MAX_WINDOW_SIZE;
	} else {
		// Check if full size dictionary is now reached
		if (sw->look_ahead_position >= sw->MAX_DICTIONARY_SIZE) {
			sw->full_size_dictionary_flag = 1;
			sw->dictionary_position = sw->look_ahead_position
					- sw->MAX_DICTIONARY_SIZE;
		}
	}

	// Add one character at a time
	int i = 0;
	while (i < charCount && !feof(inputfile)) {
		fread((sw->window + sw->end_position), sizeof(char), 1, inputfile);
		sw->end_position++;
		sw->end_position %= sw->MAX_WINDOW_SIZE;
		sw->current_look_ahead_size++;
		i++;
	}
}

void flushWritingBuffer(WritingBuffer *wb) {
	// Write to output file if there are enough bits
	int i;
	while (wb->current_size >= 8) {
		char c = 0;
		// Build char
		for (i = 0; i < 8; i++) {
			c += *(wb->bitValues + wb->start_position);
			wb->current_size--;
			wb->start_position++;
			wb->start_position %= wb->MAX_SIZE;
			if (i < 7)
				c = c << 1;
		}
		// Write to output
		fwrite(&c, 1, 1, wb->outfile);
	}

	// Write last piece of information if present
	if (wb->current_size > 0) {
		char c = 0;
		// Build char
		for (i = 0; i < wb->current_size; i++) {
			c += *(wb->bitValues + wb->start_position);
			wb->start_position++;
			wb->start_position %= wb->MAX_SIZE;
			c = c << 1;
		}
		// Shift remaining bits
		c = c << (8 - wb->current_size);
		// Write to output
		fwrite(&c, 1, 1, wb->outfile);
		// Reset current size of writing buffer
		wb->current_size = 0;
	}
}

WritingBuffer initWritingBuffer(FILE *f, int element_size) {
	WritingBuffer wb;
	wb.outfile = f;
	wb.MAX_SIZE = element_size + 7;
	wb.bitValues = (char *) malloc(sizeof(char) * wb.MAX_SIZE);
	wb.current_size = wb.start_position = wb.write_position = 0;
	return wb;
}

int lzCompress(char inpfile[], int window_size,
		int look_ahead_size) {

	/* ------------------------- SET UP ------------------------- */

	// Open input/output files (check for errors)
	FILE *inpfptr;
	inpfptr = fopen(inpfile, "r");

	if (inpfptr == NULL) {
		printf("Errore durante l'apertura del file in input.\n");
		return -1;
	}

	FILE *flagFile, *offsetFile, *lengthFile, *nextFile;
	flagFile = fopen("_flags.lztmp", "w");
	offsetFile = fopen("_offsets.lztmp", "w");
	lengthFile = fopen("_lengths.lztmp", "w");
	nextFile = fopen("_next.lztmp", "w");
	if (flagFile == NULL || offsetFile == NULL || lengthFile == NULL
			|| nextFile == NULL) {
		printf("Errore di apertura di uno dei files temporanei in scrittura\n");
		return -1;
	}

	// Check if input file is empty!
	struct stat inputfstat;
	stat(inpfile, &inputfstat);
	if (inputfstat.st_size == 0) {
		printf("File in input vuoto.\n");
		fclose(inpfptr);
		return 0;
	}

	// Initialize variables
	int minMatchLength = 3;
	int flagBitSize = 1;
	int offsetBitSize = (int) ceil(log2(window_size - look_ahead_size + 1));
	int lengthBitSize = (int) ceil(log2(look_ahead_size - 1));
	int nextBitSize = 8;

	// Initialize main window
	SlidingWindow mainWindow;
	mainWindow.window = (char *) malloc(sizeof(char) * window_size);
	mainWindow.MAX_WINDOW_SIZE = window_size;
	mainWindow.MAX_LOOK_AHEAD_SIZE = look_ahead_size;
	mainWindow.MAX_DICTIONARY_SIZE = window_size - look_ahead_size;
	mainWindow.dictionary_position = 0;
	mainWindow.look_ahead_position = 0;
	mainWindow.end_position = 0;
	mainWindow.full_size_dictionary_flag = 0;

	// Initialize writing buffers
	WritingBuffer flagWB, offsetWB, lengthWB, nextWB;
	flagWB = initWritingBuffer(flagFile, flagBitSize);
	offsetWB = initWritingBuffer(offsetFile, offsetBitSize);
	lengthWB = initWritingBuffer(lengthFile, lengthBitSize);
	nextWB = initWritingBuffer(nextFile, nextBitSize);

	/* ------------------------ COMPRESS ------------------------ */

	// Write lengths as shorts in first position of output file
	fwrite(&window_size, sizeof(short), 1, flagFile);
	fwrite(&look_ahead_size, sizeof(short), 1, flagFile);

	// Fill look ahead buffer as much as you can
	mainWindow.end_position = fread(mainWindow.window, sizeof(char),
			mainWindow.MAX_LOOK_AHEAD_SIZE, inpfptr);
	mainWindow.current_look_ahead_size = mainWindow.end_position;

	// Loop until look ahead buffer is empty
	while (mainWindow.current_look_ahead_size > 0) {

		// Search dictionary
		Match m = findMatch(&mainWindow);

		// Write output
		if (m.length < minMatchLength) {
			// Directly write next char
			writeOut(&flagWB, 0, flagBitSize); // 'Uncoded' flag
			writeOut(&nextWB, m.next, nextBitSize);
		} else {
			// Write match data to corresponding write buffers
			writeOut(&flagWB, 1, flagBitSize); // 'Coded' flag
			writeOut(&offsetWB, m.offset, offsetBitSize);
			writeOut(&lengthWB, m.length, lengthBitSize);
			writeOut(&nextWB, m.next, nextBitSize);
		}
		int processedCharsCount =
				(m.length < minMatchLength) ? 1 : m.length + 1;

		// Load new input data
		fillWindow(&mainWindow, inpfptr, processedCharsCount); // TODO: implement function!
	}

	/* ------------------------ CLEAN UP ------------------------ */

	// Flush writing buffers
	flushWritingBuffer(&nextWB);
	flushWritingBuffer(&lengthWB);
	flushWritingBuffer(&offsetWB);
	flushWritingBuffer(&flagWB);

	// Free allocated memory
	free(nextWB.bitValues);
	free(lengthWB.bitValues);
	free(offsetWB.bitValues);
	free(flagWB.bitValues);
	free(mainWindow.window);

	// Close input/output files
	fclose(nextFile);
	fclose(lengthFile);
	fclose(offsetFile);
	fclose(flagFile);

	fclose(inpfptr);
	return 0;
}
