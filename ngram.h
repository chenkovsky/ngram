#ifndef __NGRAM_H__
#define __NGRAM_H__ 1
#include <stddef.h>
#include <stdint.h>
#include "hash_vocab.h"

typedef struct{
    uint32_t word_id;
    uint32_t prob;
}NextWordResult_T;

typedef struct Ngram_T* Ngram;
Ngram Ngram_init_from_bin(const char* mem);
Ngram Ngram_init(const size_t* gram_nums, uint32_t order);
uint32_t Ngram_prob(const Ngram model, const uint32_t* gram_words, uint32_t gram_len);
uint32_t Ngram_bow(const Ngram model, const uint32_t* gram_words, uint32_t gram_len);
uint32_t Ngram_next_words(const Ngram model, const uint32_t* context, uint32_t context_len, uint32_t next_word_limit, NextWordResult_T* rets);
uint32_t Ngram_id(const Ngram model, char* word);
const char* Ngram_word(const Ngram model, uint32_t word_id);
void Ngram_free(Ngram *model_);

typedef struct NgramBuilder_T* NgramBuilder;
int NgramBuilder_serialize(const NgramBuilder builder, Serializer serializer, void* param);
void NgramBuilder_free(NgramBuilder* builder_);
uint32_t NgramBuilder_add_word(NgramBuilder builder, char* words, uint32_t prob, uint32_t bow);
int NgramBuilder_add_ngram(NgramBuilder builder, uint32_t* wids, uint32_t order, uint32_t prob, uint32_t bow);
size_t NgramBuilder_serialize_size(const NgramBuilder builder);
#endif
