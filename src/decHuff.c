#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Node {
	struct Node *left;
	struct Node *right;
	int value;
};

void writeByteAndSaveEccessD(char bit[], FILE *output) {
	char bitToWrite[8 + 1] = { 0 };
	int len = strlen(bit);
	strncpy(bitToWrite, bit, 8);
	fputc(toDecimalD(bitToWrite), output);
	char tmpBit[len];
	memset(tmpBit, 0, len);
	for (int w = 8; w < len; w++)
		tmpBit[w - 8] = bit[w];
	memset(bit, 0, len);
	strcpy(bit, tmpBit);
}

int toBinaryD(int num, char store[]) {
	int i = 0;
	memset(store, 0, strlen(store));
	if (num <= 1) {
		store[0] = num + 48;
		return 1;
	}
	while (num > 1) {
		store[i++] = num % 2 + 48;
		num = (num - num % 2) / 2;
	}
	store[i] = '1';
	//reversing
	char tmp[strlen(store)];
	memset(tmp, 0, strlen(store));
	for (int q = strlen(store) - 1; q >= 0; q--)
		tmp[strlen(store) - 1 - q] = store[q];
	for (int q = 0; q < strlen(store); q++)
		store[q] = tmp[q];
	return strlen(store);
}

int toDecimalD(char bitToWrite[]) {
	int val = 0;
	int mul = 1;
	for (int q = strlen(bitToWrite) - 1; q >= 0; q--) {
		val += mul * (bitToWrite[q] - 48);
		mul *= 2;
	}
	return val;
}

void readMap(unsigned char *buffer, int largestEncodingLen, FILE *pFile,
		int lengths[], short inputSize) {
	//fseek(pFile, 1, SEEK_SET);
	fread(buffer, sizeof(char),
			ceil((pow(2, inputSize) * largestEncodingLen) / 8), pFile); //useless ceil
	int byteI = 0;
	char bit[100];
	memset(bit, 0, 100);
	char tmpBit[8 + 1];
	memset(tmpBit, 0, 9);
	int lenI = 0;
	while (lenI < pow(2, inputSize)) {
		toBinaryD(buffer[byteI++], tmpBit);
		while (strlen(tmpBit) < 8) //fill 0
		{
			for (int q = strlen(tmpBit) + 1; q > 0; q--)
				tmpBit[q] = tmpBit[q - 1];
			tmpBit[0] = '0';
		}

		strcat(bit, tmpBit);
		memset(tmpBit, 0, 9);
		//when bit is long enough to be interpreted as a canonical length
		while (strlen(bit) >= largestEncodingLen) {
			strncpy(tmpBit, bit, largestEncodingLen);
			lengths[lenI++] = toDecimalD(tmpBit);
//			printf("%d %d\n", lenI - 1, toDecimalD(tmpBit));
			for (int q = 0; q < strlen(bit) - largestEncodingLen; q++)
				bit[q] = bit[q + largestEncodingLen];
			bit[strlen(bit) - largestEncodingLen] = 0;
		}
	}
	//at the end of map reading it's possible that I've read some compressed bit too
	strncpy(buffer, bit, strlen(bit));
	if (strlen(bit) == 0)
		memset(buffer, 0, strlen(buffer));
}

void orderMapAndRebuildEncodings(int *lengths, int *keysOfMap,
		struct Node *tree, short inputSize) {
	//since I not only have to order the canonical lengths but also the associated symbols,
	//an optimized bubble sort can be handled better. Also, inputSize as already stated will not be higher than 13
	int stop = 1;
	do {
		stop = 1;
		for (int q = 0; q < pow(2, inputSize) - 1; q++)	//I've already created keysOfMap with all ordered characters, no need to check lexicographically
			if (lengths[q] > lengths[q + 1]) {
				int tmp = lengths[q];
				lengths[q] = lengths[q + 1];
				lengths[q + 1] = tmp;
				tmp = keysOfMap[q];
				keysOfMap[q] = keysOfMap[q + 1];
				keysOfMap[q + 1] = tmp;
				stop = 0;
			}
	} while (!stop);
	//binaryLen is the progressive canonical encoding
	char *binaryLen = calloc(lengths[(int) pow(2, inputSize) - 1] + 1, 1);
	//memset(binaryLen, 0, lengths[(int)pow(2, inputSize)-1]+1);
	for (int q = 0; q < pow(2, inputSize); q++) {
		if (lengths[q] == 0)
			continue;
		//in either case length[q] is the first symbol present
		if (lengths[q - 1] == 0 || q == 0)
			//as many 0 as the least length
			memset(binaryLen, '0', lengths[q]);
		struct Node *newNode;
		newNode = tree;
		//walk through and create the tree according to binaryLen
		for (int w = 0; w < strlen(binaryLen); w++) {
			if (binaryLen[w] == '0')
				if ((*newNode).left == NULL) {
					(*newNode).left = malloc(sizeof(struct Node));
					(*(*newNode).left).value = -1;
					newNode = (*newNode).left;
					(*newNode).left = NULL;
					(*newNode).right = NULL;
				} else
					newNode = (*newNode).left;
			else if ((*newNode).right == NULL) {
				(*newNode).right = malloc(sizeof(struct Node));
				(*(*newNode).right).value = -1;
				newNode = (*newNode).right;
				(*newNode).left = NULL;
				(*newNode).right = NULL;
			} else
				newNode = (*newNode).right;
			if (strlen(binaryLen) == w + 1) {
				(*newNode).value = keysOfMap[q];
//                printf("%s %d %c\n", binaryLen, keysOfMap[q], keysOfMap[q]);
			}
		}
		binaryLen[strlen(binaryLen) - 1]++;	//increment sequence
		for (int w = strlen(binaryLen) - 1; w > 0; w--) {
			if (binaryLen[w] > '1') {
				binaryLen[w] = '0';
				binaryLen[w - 1]++;
			} else
				break;
		}
		if (binaryLen[0] > '1') {
			//memset(binaryLen, '0', strlen(binaryLen)+1);
			binaryLen[strlen(binaryLen)] = '0';
			binaryLen[0] = '1';
		}
		//if(q<255)
		//{
		while (lengths[q + 1] > strlen(binaryLen))
			binaryLen[strlen(binaryLen)] = '0';
		//}
	}
}

void decompress(struct Node *tree, unsigned char buffer[], FILE *pFile,
		int largestEncodingLen, int fileLen, short bitsToSkip, int uncompressed,
		short inputSize, short nameIndex) {
	unsigned char *outputNames[] = { { "_flags.lztmp" }, { "_lengths.lztmp" }, {
			"_next.lztmp" }, { "_offsets.lztmp" } };
	FILE *output = fopen(outputNames[nameIndex], "wb");
	char tmpBit[9];
	memset(tmpBit, 0, 9);
	char bit[257];        //reading buffer
	memset(bit, 0, 257);
	//some bits read from map may belong to the compressed file
	strncpy(bit, buffer, strlen(buffer));
	struct Node *dummy = tree;
	int nElemRead;
	long absByteCounter = 0;
	char *writingBuffer = calloc(100, 1);
	int bytesToSkip = ceil(uncompressed / 8.);
	while (nElemRead = fread(buffer, 1, 100, pFile)) {
		for (int byteI = 0; byteI < nElemRead; byteI++, absByteCounter++) {
			toBinaryD(buffer[byteI], tmpBit);
			while (strlen(tmpBit) < 8)        //fill 0
			{
				for (int q = strlen(tmpBit) + 1; q > 0; q--)
					tmpBit[q] = tmpBit[q - 1];
				tmpBit[0] = '0';
			}

			strcat(bit, tmpBit);        //put just read 8 bit in a buffer
			memset(tmpBit, 0, 8);
			//if the point where uncompressed bits begins
			if (absByteCounter + largestEncodingLen * pow(2, inputSize) / 8
					> fileLen - 1 - 4 - bytesToSkip) {
				for (int q = 0; q < (uncompressed >= 8 ? 8 : uncompressed); q++)
					writingBuffer[strlen(writingBuffer)] = bit[q];
				writeByteAndSaveEccessD(writingBuffer, output);
				uncompressed -= 8;
			}

			for (int bitI = 0;
					bitI < strlen(bit)
							&& absByteCounter
									+ largestEncodingLen * pow(2, inputSize) / 8
									<= fileLen - 1 - 4 - bytesToSkip; bitI++) {
				if (8 - bitI == bitsToSkip
						&& absByteCounter
								+ largestEncodingLen * pow(2, inputSize) / 8
								== fileLen - 1 - 4 - bytesToSkip) //-4 for the header
					break;
				if (bit[bitI] == '0') {
					dummy = (*dummy).left;
				} else {
					dummy = (*dummy).right;
				}
				if ((*dummy).value > -1) {
					char *localTmp = calloc(100, 1);
					toBinaryD((*dummy).value, localTmp);
					while (strlen(localTmp) % inputSize != 0) {
						for (int q = strlen(localTmp) + 1; q > 0; q--)
							localTmp[q] = localTmp[q - 1];
						localTmp[0] = '0';
					}
					strcat(writingBuffer, localTmp);
					free(localTmp);
					while (strlen(writingBuffer) > 7)
						writeByteAndSaveEccessD(writingBuffer, output);
					dummy = tree;
				}

			}
			memset(bit, 0, strlen(bit));
		}
	}
	fclose(output);
}

void setTheTreeFree(struct Node *tree) {
	if (tree == NULL)
		return;
	if ((*tree).left != NULL)
		setTheTreeFree((*tree).left);
	if ((*tree).right != NULL)
		setTheTreeFree((*tree).right);
	free(tree);
}

int create4Files(char inputName[]) {
	FILE *in = fopen(inputName, "rb");
	if (in == NULL) {
		printf("File final not found");
		return -1;
	}
	long size;
	unsigned char *outNames[] =
			{ { "out0" }, { "out1" }, { "out2" }, { "out3" } };
	for (short nameIndex = 0; nameIndex < 4; nameIndex++) {
		fread(&size, sizeof(long), 1, in);
		if (size == 0)
			continue;
		FILE *out = fopen(outNames[nameIndex], "wb");
		char *whole = malloc(size * sizeof(char));
		fread(whole, 1, size, in);
		fwrite(whole, 1, size, out);
		free(whole);
		fclose(out);
	}
	fclose(in);
}

int decompressHuffman(char inputName[]) {
	if (create4Files(inputName) == -1)
		return -1;
	unsigned char *names[] = { { "out0" }, { "out1" }, { "out2" }, { "out3" } };
	//decompress each one of the 4 files
	for (short nameIndex = 0; nameIndex < 4; nameIndex++) {
		FILE *pFile = fopen(names[nameIndex], "rb");
		if (pFile == NULL) {
			unsigned char *outputNames[] =
					{ { "_flags.lztmp" }, { "_lengths.lztmp" },
							{ "_next.lztmp" }, { "_offsets.lztmp" } };
			FILE *empty = fopen(outputNames[nameIndex], "wb");
			fclose(empty);
			continue;
		}
		fseek(pFile, 0, SEEK_END);
		int fileLen = ftell(pFile);
		rewind(pFile);
		short bitsToSkip = fgetc(pFile);
		int uncompressed = fgetc(pFile);
		short inputSize = fgetc(pFile);
		int largestEncodingLen = fgetc(pFile);
		unsigned char *buffer = calloc(
				largestEncodingLen * pow(2, inputSize) + 8,
				sizeof(unsigned char));
		struct Node *tree = malloc(sizeof(struct Node));
		(*tree).value = -1;
		(*tree).left = NULL;
		(*tree).right = NULL;
		//canonical lengths
		int *lengths = calloc(pow(2, inputSize), sizeof(int));
		//get decimal lengths
		readMap(buffer, largestEncodingLen, pFile, lengths, inputSize);
		int *keysOfMap = malloc(sizeof(int) * pow(2, inputSize));
		//each cell is filled with its own index
		for (int q = 0; q < pow(2, inputSize); q++)
			keysOfMap[q] = q;
		orderMapAndRebuildEncodings(lengths, keysOfMap, tree, inputSize);
		decompress(tree, buffer, pFile, largestEncodingLen, fileLen, bitsToSkip,
				uncompressed, inputSize, nameIndex);
		fclose(pFile);
		free(buffer);
		free(lengths);
		free(keysOfMap);
		setTheTreeFree(tree);
		remove(names[nameIndex]);
	}
}
