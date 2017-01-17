#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

#define CODED 1
#define UNCODED 0

/*
 * Structure to contain the main window of the compression algorithm,
 * implemented as a circular buffer.
 */
typedef struct slWindow {
	char *window;
	int dictionary_position, look_ahead_position, end_position;
	short full_size_dictionary_flag;
	int MAX_WINDOW_SIZE, MAX_DICTIONARY_SIZE, MAX_LOOK_AHEAD_SIZE;
	int current_look_ahead_size, current_dictionary_size;
} SlidingWindow;

/*
 * A simple circular buffer used to write bits to output file.
 */
typedef struct wBuf {
	FILE *outfile;
	char *bitValues;
	int MAX_SIZE, current_size, start_position, write_position;
} WritingBuffer;

/*
 * A match as defined in LZ77: a set of three values (offset, length, next character)
 */
typedef struct match {
	int offset;
	int length;
	char next;
} Match;

/*
 * Naïve string search algorithm: sweep entire dictionary one character at a time
 * and find longest match from current position.
 */
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

/*
 * Knuth-Morris-Pratt string search algorithm: sweep entire dictionary, but
 * use information of every mismatch to jump ahead in dictionary, thus improving
 * the naïve implementation
 */
Match kmpFinder(SlidingWindow *sw) {
	// Initialize default match
	Match m;
	m.offset = 0;
	m.length = 0;
	m.next = *((sw->window) + sw->look_ahead_position); // First char in look ahead

	// Compute partial match table for look ahead buffer
	int partialMatchTable[sw->current_look_ahead_size];
	partialMatchTable[0] = -1; // by definition
	if (sw->current_look_ahead_size > 1)
		partialMatchTable[1] = 0; // by definition
	int currentPosition = 2, currentLength = 0;
	while (currentPosition < sw->current_look_ahead_size) {
		// Retrieve current chars in circular buffer
		char currentChar = *(sw->window
				+ (sw->look_ahead_position
				+ currentPosition - 1) % sw->MAX_WINDOW_SIZE);
		char currentMatchChar = *((sw->window + sw->look_ahead_position
				+ currentLength));

		// Check if there is a match, partial match or no match at current position
		// set values accordingly
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
		// Get current characters in both dictionary and look ahead buffers (which are circular)
		char currentDictionaryChar = *(sw->window
				+ (sw->dictionary_position
				+ currentDictionaryPosition + currentLookAheadPosition)
				% sw->MAX_WINDOW_SIZE);
		char currentLookAheadChar = *(sw->window
				+ (sw->look_ahead_position
				+ currentLookAheadPosition) % sw->MAX_WINDOW_SIZE);

		// Check equality, but be careful not to go past look ahead buffer
		if (currentDictionaryChar == currentLookAheadChar
				&& currentLookAheadPosition < sw->current_look_ahead_size - 1) {

			currentLookAheadPosition++;

			// Check if match is better than previous, in which case update it,
			// also favouring smaller offsets when length is the same
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
			// If the match has failed, see if you can skip ahead some characters
			// based on the partial match table computed before
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

/*
 * Function to write a given value using given number of bits on a writing buffer.
 * If the buffer contains enough information to write a char to file, it writes it
 * to the output file linked to the writing buffer.
 */
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

/*
 * Function to fill the Sliding Window with given number of characters from input file.
 * First update Look Ahead and Dictionary positions in the sliding window and update the
 * size of the dictionary until it reaches its full size.
 */
void fillWindow(SlidingWindow *sw, FILE *inputfile, int charCount) {
	// Set new look ahead position
	sw->look_ahead_position += charCount;
	sw->look_ahead_position %= sw->MAX_WINDOW_SIZE;
	sw->current_look_ahead_size -= charCount;

	// Set new dictionary position if dictionary is already full-sized
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

/*
 * Function to empty a writing buffer: writes as many characters as it can, and
 * appends as many '0' as needed to write last characters to the output file.
 */
void flushWritingBuffer(WritingBuffer *wb) {
	// Write characters to output file as long as there are enough bits
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

	// Write last piece of information if present, filling with '0' on the right
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

/*
 * Constructor for a Writing Buffer
 */
WritingBuffer initWritingBuffer(FILE *f, int element_size) {
	WritingBuffer wb;
	wb.outfile = f;
	wb.MAX_SIZE = element_size + 7;
	wb.bitValues = (char *) malloc(sizeof(char) * wb.MAX_SIZE);
	wb.current_size = wb.start_position = wb.write_position = 0;
	return wb;
}

/*
 * Main function, split into 3 parts: Set up, Compress and Clean up
 */
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
	int minMatchLength = 4;
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

	// MAIN LOOP: Loop until look ahead buffer is empty
	while (mainWindow.current_look_ahead_size > 0) {

		// Search dictionary
		//Match m = findMatch(&mainWindow); // <--- NAIVE SEARCH ALGORITHM
		Match m = kmpFinder(&mainWindow);

		// Write different outputs based on the length of the match
		if (m.length < minMatchLength) {
			// Directly write next char
			writeOut(&flagWB, UNCODED, flagBitSize); // 'Uncoded' flag
			writeOut(&nextWB,
					*(mainWindow.window + mainWindow.look_ahead_position),
					nextBitSize);
		} else {
			// Write match data to corresponding write buffers
			writeOut(&flagWB, CODED, flagBitSize); // 'Coded' flag
			writeOut(&offsetWB, m.offset, offsetBitSize);
			writeOut(&lengthWB, m.length, lengthBitSize);
			writeOut(&nextWB, m.next, nextBitSize);
		}

		// Define how many characters were processed
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
