#ifndef __HASH_VOCAB_H__
#define __HASH_VOCAB_H__ 1
#include <stddef.h>
#include <stdint.h>
typedef struct HashVocab_T* HashVocab;

HashVocab HashVocab_init_from_bin(const char* mem);

uint32_t HashVocab_id(const HashVocab vocab, const char* word);

const char* HashVocab_word(const HashVocab vocab, uint32_t word_id);

void HashVocab_free(HashVocab *vocab);

HashVocab HashVocab_init(size_t size);

//return word id
uint32_t HashVocab_add(HashVocab vocab, const char* word);

typedef void (*Serializer)(char* data,size_t len, void* param);
int HashVocab_serialize(const HashVocab vocab, Serializer serializer,void* param);
size_t HashVocab_serialize_size(const HashVocab voc);

uint32_t HashVocab_word_num(const HashVocab vocab);

#endif
