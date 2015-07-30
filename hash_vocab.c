#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "murmur_hash.h"
#include "hash_vocab.h"

typedef struct {
    uint64_t key;//hash of string
    uint32_t value;//word_id
}ProbingVocabularyEntry_T;

typedef struct{
    size_t buckets_;
    ProbingVocabularyEntry_T* begin_;
    ProbingVocabularyEntry_T* end_;
}ProbingHashTable_T;//open address hash table

//static inline uint64_t hash2(const char* str, size_t len){
//    return MurmurHash64(str, len, 0);
//}

static inline uint64_t hash(const char* str){
    return MurmurHash64(str, strlen(str), 0);
}

/*
static uint64_t kUnknownHash = -1; 
static uint64_t kUnknownCapHash = -1;
static void init_unknown_hash(){
    if (kUnknownHash == (uint64_t)-1) {
        kUnknownHash = hash("<unk>");
    }
    if (kUnknownCapHash == (uint64_t)-1) {
        kUnknownCapHash = hash("<UNK>"); 
    }
}*/

uint64_t kInvalidHash = (uint64_t)-1L;

//get the ideal slot for key.
static inline ProbingVocabularyEntry_T* HashTable_ideal(ProbingHashTable_T* tb, uint64_t key){
    return tb->begin_ + key % tb->buckets_;
}


struct HashVocab_T{
    //DictItem_T* dict;
    ProbingHashTable_T tb;
    size_t bound_;//max(word id)+1
    uint32_t *words;//array to word_strs offset
    char* word_strs;//word\0word...
    size_t init_from_bin;//-1 means init from bin, otherwise is word_num


    //this is used for init empty vocab
    size_t word_strs_size;
    size_t word_strs_used_size;

};

static uint32_t HashVocab_cp_str(HashVocab vocab, const char* word);

//assume little ending
HashVocab HashVocab_init_from_bin(const char* mem){
    //init_unknown_hash();
    HashVocab voc = malloc(sizeof(struct HashVocab_T));
#define READ(ret, typ, ptr) ret =*(typ *)ptr;\
                            ptr+=sizeof(typ);
    READ(voc->bound_ , size_t , mem)
    READ(voc->tb.buckets_ , size_t, mem)
    assert(voc->bound_ != 0);
    assert(voc->tb.buckets_ != 0);
    //printf("voc->bound_:%zu\n",voc->bound_ );
    //printf("voc->tb.buckets_:%zu\n", voc->tb.buckets_ );
    voc->tb.begin_ = (ProbingVocabularyEntry_T*)mem;
    voc->tb.end_ = voc->tb.begin_ + voc->tb.buckets_;
    mem = (char*)voc->tb.end_;
    voc->words = (uint32_t*)mem;
    mem = (char*)(voc->words + voc->bound_);
    voc->word_strs = (char*)mem;
    voc->init_from_bin = (size_t)-1;//init_from_bin is true
#undef READ
    return voc;
}

uint32_t HashVocab_id(const HashVocab vocab, const char* word){
    uint64_t hashed = hash(word);
    //if (hashed == kUnknownHash || hashed == kUnknownCapHash) {
    //    return 0;
    //}
    ProbingVocabularyEntry_T *slot = HashTable_ideal(&vocab->tb, hashed);
    uint8_t check_dead_loop = 0;
    while (1) {
        if (slot->key == kInvalidHash) {
            //printf("hash:%zu\n", hashed);
            //printf("cannot find\n");
            return -1;//cannot find
        }
        if (slot->key == hashed) {
            return slot->value;
        }
        if ((++slot) == vocab->tb.end_) {
            check_dead_loop++;
            if (check_dead_loop >= 2) {
                //printf("hash:%zu\n", hashed);
                //printf("cannot find dead loop\n");
                return -1;
            }
            slot = vocab->tb.begin_;
        }
    }
    return -1;
}
const char* HashVocab_word(const HashVocab vocab, uint32_t word_id){
    if (word_id >= vocab->bound_ || (vocab->init_from_bin != (size_t)-1 && vocab->init_from_bin <= word_id)) {
        return NULL;
    }
    return vocab->word_strs + vocab->words[word_id];
}

void HashVocab_free(HashVocab *voc_){
    assert(voc_ && *voc_);
    HashVocab voc = *((HashVocab*)voc_);
    if (voc->init_from_bin) {
        free(voc);
    }else{
        free(voc->tb.begin_);
        free(voc->words);
        free(voc->word_strs);
    }
    *voc_ = NULL;
}


static const size_t avg_word_len = 8;
static const float bucket_ratio = 1.5;
HashVocab HashVocab_init(size_t size){
    //printf("size:%zu\n", size);
    //init_unknown_hash();
    assert(size != 0);
    HashVocab voc = malloc(sizeof(struct HashVocab_T));
    voc->init_from_bin = 0;//not init from binary. word_num is 0.

    voc->tb.buckets_ = size*bucket_ratio;
    voc->tb.begin_ = malloc(sizeof(ProbingVocabularyEntry_T)*voc->tb.buckets_);
    memset(voc->tb.begin_, -1, sizeof(ProbingVocabularyEntry_T)*voc->tb.buckets_);
    voc->tb.end_ = voc->tb.begin_ + voc->tb.buckets_;

    voc->bound_ = size;
    voc->words = malloc(sizeof(uint32_t)*size);
    voc->word_strs_size = avg_word_len*size;
    //printf("word_strs_size:%zu\n",voc->word_strs_size);
    voc->word_strs_used_size = 0;
    voc->word_strs = malloc(voc->word_strs_size);

    return voc;
}


//return word id
uint32_t HashVocab_add(HashVocab vocab, const char* word){
    if (vocab->init_from_bin == (size_t)-1) {
        return -1;//cannot add word when inited from binary.
    }
    uint64_t hashed = hash(word);
    //if (hashed == kUnknownHash || hashed == kUnknownCapHash) {
    //    return 0;
    //}
    ProbingVocabularyEntry_T *slot = HashTable_ideal(&vocab->tb, hashed);
    uint8_t check_dead_loop = 0;
    while (1) {
        if (slot->key == kInvalidHash) {
            //insert it into this slot
            uint32_t wid = vocab->init_from_bin++;
            assert(wid < vocab->bound_ );
            slot->key = hashed;
            slot->value = wid;
            vocab->words[wid] = HashVocab_cp_str(vocab,word);
        }
        if (slot->key == hashed) {
            return slot->value;
        }
        if ((++slot) == vocab->tb.end_) {
            check_dead_loop++;
            if (check_dead_loop >= 2) {
                return -1;
            }
            slot = vocab->tb.begin_;
        }
    }
    return -1;
}

#define max(a,b) ((a)>(b)?(a):(b))
static uint32_t HashVocab_cp_str(HashVocab vocab, const char* word){
    size_t wlen = strlen(word);
    if(vocab->word_strs_used_size + wlen+1 >= vocab->word_strs_size){
        vocab->word_strs_size = max(vocab->word_strs_size, wlen+1)*2;
        vocab->word_strs = realloc(vocab->word_strs, vocab->word_strs_size);
    }
    //printf("new_word_strs_size:%zu\n", vocab->word_strs_used_size);
    //printf("word_strs_size:%zu\n", vocab->word_strs_size);

    strncpy(vocab->word_strs + vocab->word_strs_used_size, word, wlen+1);
    uint32_t offset = vocab->word_strs_used_size;
    vocab->word_strs_used_size += wlen + 1;
    return offset;
}

int HashVocab_serialize(const HashVocab voc, Serializer serializer, void* param){
    //printf("serialize, bound:%d, buckets:%d\n", voc->bound_,voc->tb.buckets_);
    serializer((char*)&(voc->bound_), sizeof(voc->bound_), param);
    serializer((char*)&(voc->tb.buckets_), sizeof(voc->tb.buckets_), param);
    serializer((char*)voc->tb.begin_, sizeof(ProbingVocabularyEntry_T)* voc->tb.buckets_, param);
    serializer((char*)voc->words, sizeof(uint32_t)*voc->bound_, param);
    serializer((char*)voc->word_strs, voc->word_strs_used_size, param);
    return 0;
}

size_t HashVocab_serialize_size(const HashVocab voc){
    return sizeof(voc->bound_) + sizeof(voc->tb.buckets_) + sizeof(ProbingVocabularyEntry_T)* voc->tb.buckets_
    +  sizeof(uint32_t)*voc->bound_ + voc->word_strs_used_size;
}

uint32_t HashVocab_word_num(const HashVocab vocab){
    return vocab->bound_;
}
