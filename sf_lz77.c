#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

typedef struct slWindow {
	char *window;
	int dictionary_position, look_ahead_position, end_position;
	short full_size_dictionary_flag;
	int MAX_WINDOW_SIZE, MAX_DICTIONARY_SIZE, MAX_LOOK_AHEAD_SIZE;
	int current_look_ahead_size, current_dictionary_size;
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
				&& currentLength < sw->current_look_ahead_size - 1) {
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
			int tmpOffset = sw->look_ahead_position - currentPosition;
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

Match kmpFinder(SlidingWindow *sw) {
	// Initialize default match
	Match m;
	m.offset = 0;
	m.length = 0;
	m.next = *((sw->window) + sw->look_ahead_position); // First char in look ahead

	// Compute partial match table for look ahead buffer
	int partialMatchTable[sw->current_look_ahead_size];
	partialMatchTable[0] = -1;
	if (sw->current_look_ahead_size > 1)
		partialMatchTable[1] = 0;
	int currentPosition = 2, currentLength = 0;
	while (currentPosition < sw->current_look_ahead_size) {
		char currentChar = *(sw->window
				+ (sw->look_ahead_position
				+ currentPosition - 1) % sw->MAX_WINDOW_SIZE);
		char currentMatchChar = *((sw->window + sw->look_ahead_position
				+ currentLength));
		if (currentChar == currentMatchChar) {
			partialMatchTable[currentPosition] = ++currentLength;
			currentPosition++;
		} else if (currentLength > 0) {
			currentLength = partialMatchTable[currentLength];
		} else {
			partialMatchTable[currentPosition] = 0;
			currentPosition++;
		}
	}

	// Search the dictionary using KMP algorithm
	int currentDictionaryPosition = 0;
	int currentLookAheadPosition = 0;

	while (currentDictionaryPosition < sw->current_dictionary_size) {
		// Get current characters in both dictionary and look ahead buffers
		char currentDictionaryChar = *(sw->window
				+ (sw->dictionary_position
				+ currentDictionaryPosition + currentLookAheadPosition)
				% sw->MAX_WINDOW_SIZE);
		char currentLookAheadChar = *(sw->window
				+ (sw->look_ahead_position
				+ currentLookAheadPosition) % sw->MAX_WINDOW_SIZE);

		// Check equality
		if (currentDictionaryChar == currentLookAheadChar
				&& currentLookAheadPosition < sw->current_look_ahead_size - 1) {
			currentLookAheadPosition++;

			// Check if match is better than previous, in which case update it
			if (currentLookAheadPosition >= m.length) {
				int tmpOffset = sw->look_ahead_position
						- (sw->dictionary_position + currentDictionaryPosition)
								% sw->MAX_WINDOW_SIZE;
				m.offset =
						tmpOffset >= 0 ?
								tmpOffset : sw->MAX_WINDOW_SIZE + tmpOffset;
				m.length = currentLookAheadPosition;
				m.next = *(sw->window
						+ (sw->look_ahead_position + currentLookAheadPosition)
								% sw->MAX_WINDOW_SIZE);
			}

			// Check if all Look ahead buffer has been matched, in which case break
			if (currentLookAheadPosition == sw->current_look_ahead_size - 1) {
				break;
			}
		} else {
			if (partialMatchTable[currentLookAheadPosition] > -1) {
				currentDictionaryPosition += currentLookAheadPosition
						- partialMatchTable[currentLookAheadPosition];
				currentLookAheadPosition =
						partialMatchTable[currentLookAheadPosition];
			} else {
				currentDictionaryPosition++;
				currentLookAheadPosition = 0;
			}
		}

	}


	return m;
}

void writeOut(WritingBuffer *wb, int value, int nrBits) {
	// Convert value to bits and store them in writing buffer
	int mask = 1 << (nrBits - 1);
	int i;
	char d;
	for (i = 0; i < nrBits; i++) {
		d = (value & mask) != 0 ? '1' : '0';
		*(wb->bitValues + wb->write_position) = d;
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
			c += *(wb->bitValues + wb->start_position) == '0' ? 0 : 1;
			wb->current_size--;
			wb->start_position++;
			wb->start_position %= wb->MAX_SIZE;
			if (i < 7)
				c = c << 1;
		}
		// Write to output
		fwrite(&c, sizeof(char), 1, wb->outfile);
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
			sw->current_dictionary_size = sw->MAX_DICTIONARY_SIZE;
		} else {
			sw->current_dictionary_size += charCount;
		}
	}

	// Add one character at a time
	int i = 0;
	while (i < charCount && !feof(inputfile)) {
		fread((sw->window + sw->end_position), sizeof(char), 1, inputfile);
		if (feof(inputfile))
			break;
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
		unsigned char c = 0;
		// Build char
		for (i = 0; i < wb->current_size; i++) {
			c += *(wb->bitValues + wb->start_position) == '0' ? 0 : 1;
			wb->start_position++;
			wb->start_position %= wb->MAX_SIZE;
			if (i < wb->current_size - 1)
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

int lzCompress(char inpfile[], int dictionary_size,
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
	int windowSize = dictionary_size + look_ahead_size;
	int flagBitSize = 1;
	int offsetBitSize = (int) ceil(log2(dictionary_size));
	int lengthBitSize = (int) ceil(log2(look_ahead_size - 1));
	int nextBitSize = 8;

	// Initialize main window
	SlidingWindow mainWindow;
	mainWindow.window = (char *) malloc(sizeof(char) * windowSize);
	mainWindow.MAX_WINDOW_SIZE = windowSize;
	mainWindow.MAX_LOOK_AHEAD_SIZE = look_ahead_size;
	mainWindow.MAX_DICTIONARY_SIZE = dictionary_size - 1;
	mainWindow.dictionary_position = 0;
	mainWindow.look_ahead_position = 0;
	mainWindow.end_position = 0;
	mainWindow.full_size_dictionary_flag = 0;
	mainWindow.current_dictionary_size = 0;

	// Initialize writing buffers
	WritingBuffer flagWB, offsetWB, lengthWB, nextWB;
	flagWB = initWritingBuffer(flagFile, flagBitSize);
	offsetWB = initWritingBuffer(offsetFile, offsetBitSize);
	lengthWB = initWritingBuffer(lengthFile, lengthBitSize);
	nextWB = initWritingBuffer(nextFile, nextBitSize);

	/* ------------------------ COMPRESS ------------------------ */

	// Write lengths as shorts in first position of output file
	fwrite(&dictionary_size, sizeof(short), 1, flagFile);
	fwrite(&look_ahead_size, sizeof(short), 1, flagFile);

	// Fill look ahead buffer as much as you can
	mainWindow.end_position = fread(mainWindow.window, sizeof(char),
			mainWindow.MAX_LOOK_AHEAD_SIZE, inpfptr);
	mainWindow.current_look_ahead_size = mainWindow.end_position;

	// Loop until look ahead buffer is empty
	while (mainWindow.current_look_ahead_size > 0) {

		// Search dictionary
		//Match m = findMatch(&mainWindow);
		Match m = kmpFinder(&mainWindow);

		// DEBUG
//		if (m.length != testMatch.length) {
//			printf("Due lunghezze differiscono!\n");
//			printf("(%d, %d, %c) -> (%d, %d, %c)\n", m.offset, m.length, m.next,
//					testMatch.offset, testMatch.length, testMatch.next);
//		}
		// END DEBUG

		// Write output
		if (m.length < minMatchLength) {
			// Directly write next char
			writeOut(&flagWB, 0, flagBitSize); // 'Uncoded' flag
			writeOut(&nextWB,
					*(mainWindow.window + mainWindow.look_ahead_position),
					nextBitSize);
//			printf("(%c)\n",
//					*(mainWindow.window + mainWindow.look_ahead_position));
		} else {
			// Write match data to corresponding write buffers
			writeOut(&flagWB, 1, flagBitSize); // 'Coded' flag
			writeOut(&offsetWB, m.offset, offsetBitSize);
			writeOut(&lengthWB, m.length, lengthBitSize);
			writeOut(&nextWB, m.next, nextBitSize);
//			printf("(%d, %d, %c)\n", m.offset, m.length, m.next);
		}
		int processedCharsCount =
				(m.length < minMatchLength) ? 1 : m.length + 1;

		// Load new input data
		fillWindow(&mainWindow, inpfptr, processedCharsCount);
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
