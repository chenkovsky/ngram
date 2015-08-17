
#include <stdio.h>
#include <string.h> 
#include <stdlib.h>
#include <assert.h>
#include "ngram.h"

typedef struct{
    size_t offset;
    char* ptr;
}SerializerParam;

void serializer(char* data,size_t len, void* param_){
    SerializerParam *param = (SerializerParam*) param_;
    memcpy(param->ptr+param->offset, data, len);
    param->offset += len;
}

NgramBuilder builder;
Ngram ngram;

#define MAX_BUF_LEN 200000
char iobuf[MAX_BUF_LEN];
void ReadUTF8_test(FILE* fp)
{
    fgets((char*)iobuf, sizeof(iobuf), fp);
    int order = atoi(iobuf);
    assert(Ngram_order(ngram) == order);
    size_t gram_nums[MAX_ORDER];
    for (uint32_t i = 0;i<order;i++) {
         fgets((char*)iobuf, sizeof(iobuf), fp);
         gram_nums[i] = atol(iobuf);
         assert(Ngram_gram_num(ngram, i+1) == gram_nums[i]);
    }
    assert(Ngram_word_num(ngram) == gram_nums[0]);
    char* words[MAX_ORDER];
    while( fgets((char*)iobuf, sizeof(iobuf), fp))
    {
        size_t len = strlen(iobuf);
        if (iobuf[len-1] == '\n') {
            iobuf[len-1] = 0;
        }
        //order\tword\t...\tprob\tbow
        int order = atoi(strtok(iobuf, "\t"));
        for (int i = 0;i<order;i++) {
            words[i] = strtok(NULL,"\t");
        }
        uint32_t prob = atoi(strtok(NULL, "\t"));
        uint32_t bow = atoi(strtok(NULL, "\t"));
        assert(Ngram_prob2(ngram,(const char**)words,order) == prob);
        assert(Ngram_bow2(ngram, (const char**)words,order) == bow);
    }
}

void ReadUTF8(FILE* fp)
{
    //order
    //gram num of order1
    //gram num of order2
    //......
    fgets((char*)iobuf, sizeof(iobuf), fp);
    int order = atoi(iobuf);
    size_t gram_nums[MAX_ORDER];
    for (uint32_t i = 0;i<order;i++) {
         fgets((char*)iobuf, sizeof(iobuf), fp);
         gram_nums[i] = atol(iobuf);
    }

    builder = NgramBuilder_init(gram_nums, order);
    HashVocab vocab = NgramBuilder_vocab(builder);
    while( fgets((char*)iobuf, sizeof(iobuf), fp))
    {
        size_t len = strlen(iobuf);
        if (iobuf[len-1] == '\n') {
            iobuf[len-1] = 0;
        }
        //order\tword\t...\tprob\tbow
        int order = atoi(strtok(iobuf, "\t"));
        if (order == 1) {
            char* word = strtok(NULL,"\t");
            uint32_t prob = atoi(strtok(NULL, "\t"));
            uint32_t bow = atoi(strtok(NULL, "\t"));
            uint32_t word_id = NgramBuilder_add_word(builder,word,prob,bow);
            printf("word:%s\tid:%d\n", word, word_id);
        }else{
            uint32_t wids[MAX_ORDER];
            for (int i = 0;i<order;i++) {
                char* word = strtok(NULL,"\t");
                wids[i] = HashVocab_id(vocab, word);
                assert(wids[i] != -1);
            }
            uint32_t prob = atoi(strtok(NULL, "\t"));
            uint32_t bow = atoi(strtok(NULL, "\t"));
            NgramBuilder_add_ngram(builder, wids, order, prob, bow); 
        }
    }

    size_t size = NgramBuilder_serialize_size(builder);
    printf("rom size:%zu\n", size);
    char* rom = malloc(size);
    SerializerParam param;
    param.ptr = rom;
    param.offset = 0;
    NgramBuilder_serialize(builder,serializer, &param);
    assert(param.offset == size);
    ngram = Ngram_init_from_bin(rom);
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
