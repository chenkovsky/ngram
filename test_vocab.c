#include <stdio.h>
#include <string.h> 
#include <stdlib.h>
#include <assert.h>
#include "hash_vocab.h"
#define MAX_BUF_LEN 200000
char iobuf[MAX_BUF_LEN];


typedef struct{
    size_t offset;
    char* ptr;
}SerializerParam;

void serializer(char* data,size_t len, void* param_){
    SerializerParam *param = (SerializerParam*) param_;
    memcpy(param->ptr+param->offset, data, len);
    param->offset += len;
}

char* rom;

HashVocab vocab;

void ReadUTF8_test(FILE* fp)
{
    fgets((char*)iobuf, sizeof(iobuf), fp);
    int word_num = atoi(iobuf);
    HashVocab vocab2 = HashVocab_init_from_bin(rom);
    while( fgets((char*)iobuf, sizeof(iobuf), fp))
    {
        size_t len = strlen(iobuf);
        if (iobuf[len-1] == '\n') {
            iobuf[len-1] = 0;
        }
        uint32_t id1 = HashVocab_id(vocab, iobuf);
        uint32_t id2 = HashVocab_id(vocab2, iobuf);
        assert(id1 == id2);
    }
}

void ReadUTF8(FILE* fp)
{
    fgets((char*)iobuf, sizeof(iobuf), fp);
    int word_num = atoi(iobuf);
    vocab = HashVocab_init(word_num);
    while( fgets((char*)iobuf, sizeof(iobuf), fp))
    {
        size_t len = strlen(iobuf);
        if (iobuf[len-1] == '\n') {
            iobuf[len-1] = 0;
        }
        uint32_t id = HashVocab_add(vocab, iobuf);
        //printf("%s:%d\n", iobuf, id);
        assert(HashVocab_id(vocab, iobuf) == id);
    }

    //for (int i = 0; i< word_num;i++) {
    //    fprintf(stderr, "%d:%s\n", i, HashVocab_word(vocab,i));
    //}

    size_t size = HashVocab_serialize_size(vocab);
    rom = malloc(size);
    SerializerParam param;
    param.ptr = rom;
    param.offset = 0;
    HashVocab_serialize(vocab,serializer, &param);
    assert(param.offset == size);
}

int main(int argc, char *argv[]) {
    FILE *testcase_fp = NULL;
    do {
        testcase_fp = fopen(argv[1], "rb");
        assert(testcase_fp != NULL);
        unsigned char b[3] = {0};
        fread(b,1,2,testcase_fp);
        if (b[0] == 0xEF && b[1] == 0xBB) {
            fread(b,1,1,testcase_fp);
            ReadUTF8(testcase_fp);
        }else{
            rewind(testcase_fp);
            ReadUTF8(testcase_fp);
        }
    } while (0);

    do {
        testcase_fp = fopen(argv[1], "rb");
        assert(testcase_fp != NULL);
        unsigned char b[3] = {0};
        fread(b,1,2,testcase_fp);
        if (b[0] == 0xEF && b[1] == 0xBB) {
            fread(b,1,1,testcase_fp);
            ReadUTF8_test(testcase_fp);
        }else{
            rewind(testcase_fp);
            ReadUTF8_test(testcase_fp);
        }
    } while (0);
}
