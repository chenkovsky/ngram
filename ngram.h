#ifndef __NGRAM_H__
#define __NGRAM_H__ 1
#include <stddef.h>
#include <stdint.h>
#include "hash_vocab.h"

#define MAX_ORDER 64
typedef struct{
    uint32_t word_id;
    uint32_t prob;
}NextWordResult_T;

typedef struct Ngram_T* Ngram;
Ngram Ngram_init_from_bin(const char* mem);
uint32_t Ngram_prob(const Ngram model, const uint32_t* gram_words, uint32_t gram_len);
uint32_t Ngram_prob2(const Ngram model, const char** gram_words, uint32_t gram_len);
uint32_t Ngram_bow(const Ngram model, const uint32_t* gram_words, uint32_t gram_len);
uint32_t Ngram_bow2(const Ngram model, const char** gram_words, uint32_t gram_len);
uint32_t Ngram_next_words(const Ngram model, const uint32_t* context, uint32_t context_len, uint32_t next_word_limit, NextWordResult_T* rets);
uint32_t Ngram_id(const Ngram model, const char* word);
uint32_t Ngram_order(const Ngram model);
size_t Ngram_gram_num(const Ngram model, uint32_t order);
uint32_t Ngram_word_num(const Ngram model);

const char* Ngram_word(const Ngram model, uint32_t word_id);
void Ngram_free(Ngram *model_);

typedef struct NgramBuilder_T* NgramBuilder;
NgramBuilder NgramBuilder_init(const size_t* gram_nums, uint32_t order);
int NgramBuilder_serialize(const NgramBuilder builder, Serializer serializer, void* param);
void NgramBuilder_free(NgramBuilder* builder_);
uint32_t NgramBuilder_add_word(NgramBuilder builder, const char* words, uint32_t prob, uint32_t bow);
uint32_t NgramBuilder_add_ngram(NgramBuilder builder, const uint32_t* wids, uint32_t order, uint32_t prob, uint32_t bow);
uint32_t NgramBuilder_add_ngram2(NgramBuilder builder, const char** words, uint32_t order, uint32_t prob, uint32_t bow);
//int NgramBuilder_add_ngram2(NgramBuilder builder, char** wids, uint32_t order, uint32_t prob, uint32_t bow);
size_t NgramBuilder_serialize_size(const NgramBuilder builder);
HashVocab NgramBuilder_vocab(const NgramBuilder builder);
int NgramBuilder_save(NgramBuilder builder, char* path);
#endif
