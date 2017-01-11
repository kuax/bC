#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <math.h>

#define INVALID_MATCH 0
#define SIMPLE_MATCH 1
#define FULL_MATCH 2

/*
 * General Match, includes partial, full or invalid match.
 */
typedef struct gMatch {
	short type;
	int offset;
	int length;
	char next;
} GeneralMatch;

/*
 * Sliding Window implemented as a circular buffer
 */
typedef struct slWindow {
	char *window;
	int dictionary_position, look_ahead_position, end_position;
	short full_size_dictionary_flag;
	int MAX_WINDOW_SIZE, MAX_DICTIONARY_SIZE, MAX_LOOK_AHEAD_SIZE;
	int current_look_ahead_size;
} SlidingWindow;

/*
 * A simple circular buffer that gets filled with bit values of input chars.
 */
typedef struct rBuf {
	FILE *inpfile;
	char *bitValues;
	int MAX_SIZE;
	int ELEMENT_SIZE;
	int current_size;
	int start_pos, write_pos;
	int end_reached;
} ReadingBuffer;

/*
 * Constructor for a Reading Buffer.
 */
ReadingBuffer initReadingBuffer(FILE *f, int element_size) {
	ReadingBuffer rb;
	rb.inpfile = f;
	rb.ELEMENT_SIZE = element_size;
	rb.MAX_SIZE = element_size + 7;
	rb.bitValues = (char *) malloc(sizeof(char) * rb.MAX_SIZE);
	rb.current_size = rb.start_pos = rb.write_pos = rb.end_reached = 0;
	return rb;
}

/*
 * Function to fill a Reading Buffer with bit values of chars from the input file
 * until there is enough information to parse an element, or the end of the file is reached.
 */
void fillReadingBuffer(ReadingBuffer *rb) {
	if (!rb->end_reached) {
		while (rb->current_size < rb->ELEMENT_SIZE && !feof(rb->inpfile)) {
			// Get next char in file and convert it into individual bits
			char c = fgetc(rb->inpfile);
			if (feof(rb->inpfile))
				break;
			int i;
			unsigned char mask = (1 << 7);
			for (i = 0; i < 8; i++) {
				*(rb->bitValues + rb->write_pos) = (c & mask) == 0 ? '0' : '1';
				mask = mask >> 1;
				rb->write_pos++;
				rb->write_pos %= rb->MAX_SIZE;
				rb->current_size++;
			}
		}

		// Check if end of file reached
		if (feof(rb->inpfile))
			rb->end_reached = 1;
	}
}

/*
 * Function that gets an element from a Reading Buffer by converting the string of
 * ELEMENT_SIZE bits into an integer value
 */
int getNextElement(ReadingBuffer *rb) {
	// Check if there is enough information
	if (rb->current_size < rb->ELEMENT_SIZE)
		return -1;
	else {
		// Convert string into an integer value
		int result = 0;
		int i;
		for (i = 0; i < rb->ELEMENT_SIZE; i++) {
			result += *(rb->bitValues + rb->start_pos) == '0' ? 0 : 1;
			rb->start_pos++;
			rb->start_pos %= rb->MAX_SIZE;
			rb->current_size--;
			if (i < rb->ELEMENT_SIZE - 1)
				result = result << 1;
		}
		return result;
	}
}

/*
 * Function that returns the current match by interpreting elements in Reading Buffers.
 * If there is not enough information, return an invalid match.
 */
GeneralMatch parseData(ReadingBuffer *flagRB, ReadingBuffer *offsetRB,
		ReadingBuffer *lengthRB, ReadingBuffer *nextRB) {
	// Initialize default match
	GeneralMatch gm;
	gm.type = INVALID_MATCH;

	int flag = getNextElement(flagRB);
	if (flag == 0) {
		// Should expect a simple match, check value
		int next = getNextElement(nextRB);
		if (next >= 0) {
			gm.type = SIMPLE_MATCH;
			gm.next = (char) next;
		}
	} else if (flag == 1) {
		// Should expect a full match, check values
		int offset = getNextElement(offsetRB);
		int length = getNextElement(lengthRB);
		int next = getNextElement(nextRB);
		if (offset >= 0 && length >= 0 && next >= 0) {
			gm.type = FULL_MATCH;
			gm.offset = offset;
			gm.length = length;
			gm.next = (char) next;
		}
	}

	return gm;
}

/*
 * Function to write match to output file and update the Sliding Window accordingly.
 */
void writeAndUpdateWindow(SlidingWindow *sw, GeneralMatch *gm, FILE *outfile) {
	// Check Match type
	if (gm->type == INVALID_MATCH) {
		return;
	} else if (gm->type == FULL_MATCH) {
		// Write entire sequence while keeping sliding window up to date
		int i;
		for (i = 0; i < gm->length; i++) {
			// Compute current position to read character from
			int currentPosition = sw->look_ahead_position - gm->offset
					+ sw->MAX_WINDOW_SIZE;
			currentPosition %= sw->MAX_WINDOW_SIZE;

			char currentChar = *(sw->window + currentPosition);

			// Write character to output file
			fwrite(&currentChar, sizeof(char), 1, outfile);

			// Update sliding window
			*(sw->window + sw->look_ahead_position) = currentChar;
			sw->look_ahead_position++;
			sw->look_ahead_position %= sw->MAX_WINDOW_SIZE;

			// Check if full dictionary size is reached
			if (!sw->full_size_dictionary_flag
					&& sw->look_ahead_position >= sw->MAX_DICTIONARY_SIZE) {
				sw->full_size_dictionary_flag = 1;
				sw->dictionary_position += sw->look_ahead_position
						- sw->MAX_DICTIONARY_SIZE;
			} else if (sw->full_size_dictionary_flag) {
				sw->dictionary_position++;
				sw->dictionary_position %= sw->MAX_WINDOW_SIZE;
			}
		}
	}

	// As long as match is valid, there is a next char to write!
	// Write next character to output file
	fwrite(&(gm->next), sizeof(char), 1, outfile);

	// Update sliding window
	*(sw->window + sw->look_ahead_position) = gm->next;
	sw->look_ahead_position++;
	sw->look_ahead_position %= sw->MAX_WINDOW_SIZE;

	// Check if full dictionary size is reached
	if (!sw->full_size_dictionary_flag
			&& sw->look_ahead_position >= sw->MAX_DICTIONARY_SIZE) {
		sw->full_size_dictionary_flag = 1;
		sw->dictionary_position += sw->look_ahead_position
				- sw->MAX_DICTIONARY_SIZE;
	} else if (sw->full_size_dictionary_flag) {
		sw->dictionary_position++;
		sw->dictionary_position %= sw->MAX_WINDOW_SIZE;
	}
}

/*
 * Main decompression function, split into 3 parts: Set up, Decompression and Clean up.
 */
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
	short dictionary_size, look_ahead_size;
	fread(&dictionary_size, sizeof(short), 1, flagFile);
	fread(&look_ahead_size, sizeof(short), 1, flagFile);
	printf("Dictionary size: %d\nLook Ahead size: %d\n", dictionary_size,
			look_ahead_size);

	// Initialize variables
	int flagBitSize = 1;
	int windowSize = dictionary_size + look_ahead_size;
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


	// Initialize reading buffers
	ReadingBuffer flagRB, offsetRB, lengthRB, nextRB;
	flagRB = initReadingBuffer(flagFile, flagBitSize);
	offsetRB = initReadingBuffer(offsetFile, offsetBitSize);
	lengthRB = initReadingBuffer(lengthFile, lengthBitSize);
	nextRB = initReadingBuffer(nextFile, nextBitSize);

	/* ------------------------- DECOMPRESS ------------------------- */

	// Loop while there is enough information to be able to decompress
	GeneralMatch gm;
	do {
		// Fill reading buffers
		fillReadingBuffer(&flagRB);
		fillReadingBuffer(&offsetRB);
		fillReadingBuffer(&lengthRB);
		fillReadingBuffer(&nextRB);

		// Interpret data
		gm = parseData(&flagRB, &offsetRB, &lengthRB, &nextRB);

		// Write characters to output file
		writeAndUpdateWindow(&mainWindow, &gm, outfptr);

	} while (gm.type != INVALID_MATCH);

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
