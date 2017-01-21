/*
 * This file contains all the necessary functions to compress the 4 files representing the LZ77 output.
 * This Huffman adapts itself to whatever input size (defined as the number of bits that constitute each symbol)
 * LZ77 has used to compress the original file
 *
 * Developer: Christian Paoliello
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

struct encoding {
	int letter;
	char *code;
};
void convertTo8Bit(int val, char tmp[]);
void group(int differentChars, int freq[], int **label) {
	int indexFreq = 0;
	//group chars
	//write the used characters(better say symbols) one after the other in label
	for (int q = 0; q < differentChars; q++) {
		while (freq[indexFreq] == 0) //run the frequencies until it finds a used character
			indexFreq++;
		label[q][0] = indexFreq++;
		label[q][1] = -1;    //termination number
	}
	//group frequencies
	//typical situtation before : 1 0 0 1 0 1 (3 different characters)
	//after:					  1 1 1 0 0 0
	for (int q = 0; q < differentChars; q++) {
		int offset = q;
		while (freq[offset] == 0)
			offset++;
		if (offset != q) //if the 1 is already in the right position don't enter, or it'll become 0
				{
			freq[q] = freq[offset];
			freq[offset] = 0;
		}
	}

}

void initEncoding(int differentChars, struct encoding *myEncoding, int **label,
		int upperBound) {
	for (int q = 0; q < differentChars; q++) {
		myEncoding[q].letter = label[q][0];
		myEncoding[q].code = (char*) calloc(upperBound + 1, sizeof(char));
	}
}

void orderFreqAndLabel(int differentChars, int **label, int freq[],
		int differentLabels, unsigned char singleRound) { //called after a join of 2 label occurs
	int touched = 1;
	//singleRound is used all the times except the first, when the array is not sorted at all
	if (singleRound == 'n') {
		while (touched) //optimized bubble sort, sort the labels and frequencies
		{
			touched = 0;
			for (int q = 0; q < differentLabels - 1; q++) {
				if (freq[q] < freq[q + 1]) {
					int tmp = freq[q];
					freq[q] = freq[q + 1];
					freq[q + 1] = tmp;
					int *tmpW;

					tmpW = label[q];
					label[q] = label[q + 1];
					label[q + 1] = tmpW;

					touched = 1;

				}
			}
		}
	} else {
		for (int q = differentLabels - 2; q >= 0; q--) {
			if (freq[q] < freq[q + 1]) {
				int tmp = freq[q];
				freq[q] = freq[q + 1];
				freq[q + 1] = tmp;
				int *tmpW;

				tmpW = label[q];
				label[q] = label[q + 1];
				label[q + 1] = tmpW;

				touched = 1;

			}
		}
	}
}

void buildEncoding(int differentChars, struct encoding myEncoding[],
		int **label, int freq[]) {
	orderFreqAndLabel(differentChars, label, freq, differentChars, 'n');
	//with "left" I mean the symbols on the row before
	int leftMergingElemInd = differentChars - 2;
	for (; leftMergingElemInd >= 0; leftMergingElemInd--) {
		for (int labelInd = leftMergingElemInd;
				labelInd - leftMergingElemInd < 2; labelInd++)
			//typical situation before in label: a bc de
			//it means I'll put in the encoding code of 'b' and 'c' a 0(because they are left node)
			//and in the encoding of 'd' and 'e' a 1
			for (int labelCharInd = 0; label[labelInd][labelCharInd] >= 0;
					labelCharInd++)    //cycle letters in the same label
				for (int myEncInd = 0; myEncInd < differentChars; myEncInd++) { //optimizable code exploiting binary search
					if (myEncoding[myEncInd].letter
							== label[labelInd][labelCharInd]) {
						//assign 0 if left node otherwise 1(because labelInd will increase)
						myEncoding[myEncInd].code[strlen(
								myEncoding[myEncInd].code)] = labelInd
								- leftMergingElemInd + 48;
						break;
					}
				}
		//join the labels
		int endOfArray = 0;
		//search end of left joining side array
		for (; label[leftMergingElemInd][endOfArray] >= 0; endOfArray++)
			;
		int lenOfRightArray = 0;
		for (; label[leftMergingElemInd + 1][lenOfRightArray] >= 0;
				lenOfRightArray++)
			;
		label[leftMergingElemInd] = realloc(label[leftMergingElemInd],
				sizeof(int) * (endOfArray + 1 + lenOfRightArray));
		int futureEndofJointArray;
		for (futureEndofJointArray = endOfArray;
				label[leftMergingElemInd + 1][futureEndofJointArray - endOfArray]
						>= 0; futureEndofJointArray++)
			label[leftMergingElemInd][futureEndofJointArray] =
					label[leftMergingElemInd + 1][futureEndofJointArray
							- endOfArray];
		label[leftMergingElemInd][futureEndofJointArray] = -1;
		//I can address freq with the below index because label and freq are synchronized
		freq[leftMergingElemInd] += freq[leftMergingElemInd + 1];
		freq[leftMergingElemInd + 1] = 0;
		label[leftMergingElemInd + 1][0] = -1;
		free(label[leftMergingElemInd + 1]);
		orderFreqAndLabel(differentChars, label, freq, leftMergingElemInd + 1,
				'y');    //leftMergingElemInd+1 = number of present label
	}
}

long calcFreqFromFile(FILE *pFile, int freq[], short inputSize) {
	fseek(pFile, 0, SEEK_END);
	long lSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	char word[1000];
	//characters just read are put in buffer, here they may are filled with 0s
	char buffer[200] = { 0 };
	char tmp[200];
	for (long q = 0; q < (long) (lSize / 1000); q++)
			{
		fread(word, 1, 1000, pFile);

		for (int w = 0; w < 1000; w++) {
			convertTo8Bit((unsigned char) word[w], tmp);
			strcat(buffer, tmp);
			while (strlen(buffer) >= inputSize) {
				tmp[inputSize] = 0;

				freq[toDecimal(strncpy(memset(tmp, 0, 200), buffer, inputSize))]++;
				for (int e = inputSize; e < strlen(buffer); e++) {
					buffer[e - inputSize] = buffer[e];
				}

				buffer[strlen(buffer) - inputSize] = 0;
			}
		}
	}
	fread(word, 1, lSize % 1000, pFile);    

	for (int w = 0; w < lSize % 1000; w++) {
		convertTo8Bit((unsigned char) word[w], tmp);
		strcat(buffer, tmp);
		while (strlen(buffer) >= inputSize) {
			tmp[inputSize] = 0;
			freq[toDecimal(strncpy(tmp, buffer, inputSize))]++;
			for (int e = inputSize; e < strlen(buffer); e++)
				buffer[e - inputSize] = buffer[e];
			buffer[strlen(buffer) - inputSize] = 0;
		}
	}
	return lSize;    
}

void convertTo8Bit(int val, char tmp[]) {
	toBinary(val, tmp);
	int fill0InFront = 8 - strlen(tmp);
	for (int e = 8; e >= fill0InFront; e--) {
		tmp[e] = tmp[e - fill0InFront];
	}
	for (int e = 0; e < fill0InFront; e++)
		tmp[e] = '0';
}

int toDecimal(char bitToWrite[]) {
	int val = 0;
	int mul = 1;    
	for (int q = strlen(bitToWrite) - 1; q >= 0; q--) {
		val += mul * (bitToWrite[q] - 48);
		mul *= 2;
	}
	return val;
}

int toBinary(int num, char store[]) {
	int i = 0;
	memset(store, 0, 200);
	if (num <= 1) {
		store[0] = num + 48;
		return 1;
	}
	while (num > 1) {
		store[i++] = num % 2 + 48;
		num = (num - num % 2) / 2;
	}
	store[i] = '1';
	//reversing bit order
	char tmp[200];
	memset(tmp, 0, 200);
	for (int q = strlen(store) - 1; q >= 0; q--)
		tmp[strlen(store) - 1 - q] = store[q];
	for (int q = 0; q < strlen(store); q++)
		store[q] = tmp[q];
	return strlen(store);
}

void writeByteAndSaveEccess(char bit[], int len, FILE *output) {
	char bitToWrite[8 + 1] = { 0 };
	strncpy(bitToWrite, bit, 8);
	fputc(toDecimal(bitToWrite), output);
	char tmpBit[len];
	memset(tmpBit, 0, len);
	for (int w = 8; w < len; w++)
		tmpBit[w - 8] = bit[w];
	memset(bit, 0, len);
	strcpy(bit, tmpBit);
}

void writeCompressed(short inputSize, int hs, FILE *pFile,
		struct encoding myEncoding[], int letterMappingInStruct[], long lSize,
		int differentChars, short nameIndex) {
	//rewind(pFile);
	unsigned char *outputName = calloc(5, sizeof(unsigned char));
	strcpy(outputName, "out");
	outputName[3] = nameIndex + 48;
	FILE *output = fopen(outputName, "wb");
	//with 200 there'll never be an overflow since inputSize is at max 13(because of how LZ77 was implemented)
	char bit[200]; //is the buffer I use for the map then write to file when reaches 8 bit
	memset(bit, 0, 200);
	fputc(0, output); //I'll change this byte when I know how many bits I filled
	fputc(0, output);    //will write how many bits uncompressed
	fputc(inputSize, output);
	//WRITE MAP
	//usually largestEncLen will be around 5(which means 5 bit to express max length)
	int largestEncodingLen = strlen(myEncoding[differentChars - 1].code);
	largestEncodingLen = ceil(log2(largestEncodingLen + 1));
	fputc(largestEncodingLen, output);
	for (int q = 0; q < hs; q++) {
		int currentLen;
		if (letterMappingInStruct[q] > -1)
			currentLen = strlen(myEncoding[letterMappingInStruct[q]].code);
		else
			currentLen = 0;
		char tmpBit[200];
		//in tmpBit goes currentLen converted in binary
		toBinary(currentLen, tmpBit);    
		while (strlen(tmpBit) < largestEncodingLen) {
			for (int w = strlen(tmpBit); w > 0; w--)
				tmpBit[w] = tmpBit[w - 1];
			tmpBit[0] = '0';
		}
		strcat(bit, tmpBit);
		while (strlen(bit) > 7) {
			writeByteAndSaveEccess(bit, strlen(bit), output);
		}
	}
	//END OF MAP WRITING
	char buffer[200] = { 0 };
	//begin of file compressed writing
	for (int byteIndex = 0; byteIndex < lSize || strlen(buffer) >= inputSize;) {
		char tmp[200] = { 0 };

		while (strlen(buffer) < inputSize && byteIndex < lSize) {
			convertTo8Bit(fgetc(pFile), tmp);
			strcat(buffer, tmp);
			byteIndex++;
		}
		if (strlen(buffer) < inputSize)
			break; //here goes only when file ends before buffer reaches inputSize
		strncpy(memset(tmp, 0, 200), buffer, inputSize);
		short oldLenBuffer = strlen(buffer);
		for (int q = 0; q <= oldLenBuffer - inputSize; q++)
			buffer[q] = buffer[q + inputSize];
		strcat(bit, myEncoding[letterMappingInStruct[toDecimal(tmp)]].code);
		while (strlen(bit) > 7) {
			writeByteAndSaveEccess(bit, strlen(bit), output);
		}
	}
	fseek(output, 0, SEEK_SET);
	char asdf[] = { (8 - strlen(bit)) % 8 };
	fwrite(asdf, 1, 1, output); //write how many bits I filled at the end to form a byte
	asdf[0] = strlen(buffer);
	fwrite(asdf, 1, 1, output);    //write how many bits uncompressed
	fseek(output, 0, SEEK_END);
	if (strlen(bit) > 0)    //leftovers
			{
		for (int q = strlen(bit); q < 8; q++)
			bit[q] = '0';
		bit[8] = 0;
		fputc(toDecimal(bit), output);
	}
	if (strlen(buffer) > 0) {
		for (int q = strlen(buffer); strlen(buffer) % 8 != 0; q++) {
			buffer[q] = '0';
			buffer[q + 1] = 0;
		}
		while (strlen(buffer) > 0) {
			char localTmp[200] = { 0 };
			strncpy(localTmp, buffer, 8);
			fputc(toDecimal(localTmp), output);
			int tmpLenBuffer = strlen(buffer);
			for (int q = 0; q < tmpLenBuffer; q++)
				buffer[q] = buffer[q + 8];

		}
	}
	fclose(output);
}

void reverseBit(struct encoding myEncoding[], int differentChars) {
	//situation before: 11100001
	//after the function: 10000111
	for (int encI = 0; encI < differentChars; encI++) {
		int lenCode = strlen(myEncoding[encI].code);
		char *tmp = malloc(sizeof(char) * (lenCode + 1));
		strcpy(tmp, myEncoding[encI].code);
		for (int charI = lenCode - 1; charI >= 0; charI--)
			myEncoding[encI].code[lenCode - charI - 1] = tmp[charI];
		free(tmp);
	}

}

void canonicalize(int upperBound, struct encoding myEncoding[],
		int differentChars, int letterMappingInStruct[]) {
	int touched = 1;
	while (touched)    //optimized bubble sort
	{
		touched = 0;
		for (int encI = 0; encI < differentChars - 1; encI++) { //these 2 if can be joined
			if (strlen(myEncoding[encI].code)
					> strlen(myEncoding[encI + 1].code)) {
				struct encoding tmp = myEncoding[encI];
				myEncoding[encI] = myEncoding[encI + 1];
				myEncoding[encI + 1] = tmp;
				touched = 1;
				letterMappingInStruct[myEncoding[encI].letter]--;
				letterMappingInStruct[myEncoding[encI + 1].letter]++;
			}
		}
	}

	char *newCode = malloc(sizeof(char) * (upperBound + 1));
	memset(newCode, 0, upperBound + 1);
	for (int q = 0; q < strlen(myEncoding[0].code); q++)
		newCode[q] = '0';
	//printf("THIS %c %s \n", myEncoding[0].letter, myEncoding[0].code);
	strcpy(myEncoding[0].code, newCode);
	for (int encI = 1; encI < differentChars; encI++) {
		for (int q = strlen(newCode) - 1;
				q > 0 || (q == 0 && strlen(newCode) == 1); q--) { //next binary code
			if (newCode[q] == '0')
				newCode[q] = '1';
			else if (q > 0) {
				newCode[q] = '0';
				newCode[q - 1]++;
			}
			if (q > 0 && newCode[q - 1] <= '1')
				break;
		}
		if (newCode[0] == '2') {
			newCode[0] = '1';
			newCode[strlen(newCode)] = '0';
		}
		while (strlen(newCode) < strlen(myEncoding[encI].code)) {
			newCode[strlen(newCode)] = '0';
		}
		strcpy(myEncoding[encI].code, newCode);
	}
	free(newCode);
}

int compressHuffman(char outputName[]) {
	//files' names of the lz77 output
	unsigned char *names[] = { { "_flags.lztmp" }, { "_lengths.lztmp" }, {
			"_next.lztmp" }, { "_offsets.lztmp" } };
	//respectively, max pattern matching length and how far back in the buffer lz77 searches
	int lengthsSize = 0, offsetSize = 0;
	for (int nameIndex = 0; nameIndex < 4; nameIndex++) {
		FILE *pFile = fopen(names[nameIndex], "rb");
		if (pFile == NULL) {
			printf("File %s not found", names[nameIndex]);
			return -1;
		}
		//the _flag.lztmp file is the one containing offsetSize and lengthsSize
		if (nameIndex == 0) {
			fread(&offsetSize, sizeof(short), 1, pFile);
			fread(&lengthsSize, sizeof(short), 1, pFile);
			offsetSize = (int) ceil(log2(offsetSize));
			lengthsSize = (int) ceil(log2(lengthsSize - 1));
		}
		short inputSize = 0;
		switch (nameIndex) {
		case 0:
			inputSize = 8;
			break;
		case 1:
			inputSize = lengthsSize;
			break;
		case 2:
			inputSize = 8;
			break;
		case 3:
			inputSize = offsetSize;
		}
		//number of all possible symbols with inputSize bits
		int hs = pow(2, inputSize);
		int *freq = calloc(hs, sizeof(int));

		//an array used to map symbols in the myEncoding struct
		int *letterMappingInStruct = malloc(sizeof(int) * hs);
		memset(letterMappingInStruct, -1, sizeof(int) * hs);

		long lSize = calcFreqFromFile(pFile, freq, inputSize);
		//if empty file
		if (lSize == 0) {
			free(freq);
			free(letterMappingInStruct);
			fclose(pFile);
			remove(names[nameIndex]);
			continue;
		}
		int differentChars = 0;
		int numberOf0 = 0;
		//each cell contains the index of the value represented by the index without counting absent symbols
		for (int q = 0; q < hs; q++) {
			if (freq[q] > 0) {
				letterMappingInStruct[q] = q - numberOf0;
				differentChars++;
			} else
				numberOf0++;
		}
		//max binary length of a Huffman encoding
		int upperBound = differentChars - 1;
		//don't confuse label with encoding! label are decimal values of symbols, not bit
		int **label = malloc(sizeof(int*) * differentChars);
		for (int q = 0; q < differentChars; q++)
			label[q] = malloc(sizeof(int) * 2);
		group(differentChars, freq, label);
		struct encoding myEncoding[differentChars];
		initEncoding(differentChars, myEncoding, label, upperBound);
		if (differentChars > 1) {
			buildEncoding(differentChars, myEncoding, label, freq);
			//encoding are built from leaf to root, here I reverse them
			reverseBit(myEncoding, differentChars);

			canonicalize(upperBound, myEncoding, differentChars,
					letterMappingInStruct);
		} else {
			myEncoding[0].code[0] = '0';
		}
		/*for(int q=0;q<differentChars;q++)
		 {
		 printf("%d %s\n", myEncoding[q].letter, myEncoding[q].code);
		 }*/
		fseek(pFile, 0, SEEK_SET);

		writeCompressed(inputSize, hs, pFile, myEncoding, letterMappingInStruct,
				lSize, differentChars, nameIndex);

		fclose(pFile);
		free(freq);
		free(letterMappingInStruct);
		free(label[0]);
		free(label);
		remove(names[nameIndex]);
	}

	FILE *out = fopen(outputName, "wb");
	unsigned char *outNames[] =
			{ { "out0" }, { "out1" }, { "out2" }, { "out3" } };
	//merge everything in a single file
	for (int nameIndex = 0; nameIndex < 4; nameIndex++) {
		FILE *pFile = fopen(outNames[nameIndex], "rb");
		if (pFile == NULL) {
			long size = 0;
			fwrite(&size, sizeof(long), 1, out);
			continue;
		}
		fseek(pFile, 0, SEEK_END);
		long size = ftell(pFile);
		rewind(pFile);
		fwrite(&size, sizeof(long), 1, out);
		char *whole = malloc(size * sizeof(char));
		fread(whole, 1, size, pFile);
		fwrite(whole, 1, size, out);
		free(whole);
		fclose(pFile);
		remove(outNames[nameIndex]);
	}
	fclose(out);
	return 0;
}

