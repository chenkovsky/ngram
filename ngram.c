#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "ngram.h"
typedef struct {
    uint32_t word;
    uint32_t next_arr_idx;
    uint32_t prob;//int(-prob*1M)
    uint32_t bow;//int(-bow*1M)
}LowGram_T;

typedef struct {
    uint32_t next_arr_idx;
    uint32_t prob;
    uint32_t bow;
}UniGram_T;

typedef struct {
    uint32_t word;
    uint32_t prob;
}HighGram_T;

struct Ngram_T{
    uint32_t ngram_order;
    size_t* ngram_counts;//ngram num for every order
    HashVocab vocab;
    HighGram_T *high_gram;
    UniGram_T *uni_gram;
    LowGram_T **low_grams;
};



uint32_t Ngram_id(const Ngram model, const char* word){
    return HashVocab_id(model->vocab, word);
}

const char* Ngram_word(const Ngram model, uint32_t word_id){
    return HashVocab_word(model->vocab,word_id);
}

Ngram Ngram_init_from_bin(const char* mem){//assume ending is same
#define READ(ret, typ, ptr) ret =*(typ *)ptr;\
                            ptr+=sizeof(typ);
    Ngram model = malloc(sizeof(struct Ngram_T));
    READ(model->ngram_order, uint32_t, mem)

    model->ngram_counts = (size_t*)mem;
    mem += sizeof(size_t)*model->ngram_order;

    size_t vocab_mem_size;
    READ(vocab_mem_size, size_t, mem);
    //printf("vocab_mem_size:%zu\n", vocab_mem_size);
    model->vocab = HashVocab_init_from_bin(mem);
    mem += vocab_mem_size;

    model->uni_gram = (UniGram_T *)mem;
    mem += sizeof(UniGram_T) * (model->ngram_counts[0]+1);

    if (model->ngram_order > 2) {
        model->low_grams = malloc(sizeof(LowGram_T *)*(model->ngram_order-2));
        for (uint32_t order = 2; order < model->ngram_order; order++) {
            model->low_grams[order-2] = (LowGram_T*)mem;
            mem += sizeof(LowGram_T) * (model->ngram_counts[order-1]+1);
        }
    }else{
        model->low_grams = NULL;
    }
    if (model->ngram_order > 1) {
        model->high_gram = (HighGram_T *)mem; 
    }else{
        model->high_gram = NULL;
    }
    return model;
}

#define SEARCH_GRAM(cur_id, grams, gram_size) \
    int64_t start = 0;\
    int64_t end = gram_size - 1;\
    int64_t mid;\
    while (start <= end) {\
        mid = (start + end)>> 1;\
        if (grams[mid].word == cur_id) {\
            return mid;\
        }else if(grams[mid].word > cur_id){\
            end = mid -1;\
        }else{\
            start = mid + 1;\
        }\
    }\
    return -1;

uint32_t search_low_gram(uint32_t cur_id, LowGram_T* grams, size_t gram_size){
    SEARCH_GRAM(cur_id, grams, gram_size)
}

uint32_t search_high_gram(uint32_t cur_id, HighGram_T* grams, size_t gram_size){
    SEARCH_GRAM(cur_id, grams, gram_size)
}

#undef SEARCH_GRAM


int next_word_cmp(const void* o1, const void* o2){
    uint32_t p1 = ((NextWordResult_T*)o1)->prob;
    uint32_t p2 = ((NextWordResult_T*)o2)->prob;
    if (p1 > p2) {
        return 1;
    }else if(p1 < p2){
        return -1;
    }
    return 0;
}

//alloc one more

//context is context word id array
uint32_t Ngram_next_words(const Ngram model, const uint32_t* context, uint32_t context_len, uint32_t next_word_limit, NextWordResult_T* rets){
    if (context_len == 0) {
        return 0;
    }
    uint32_t start = (context_len >= model->ngram_order) ? (context_len - model->ngram_order + 1): 0;
    uint32_t res_num = 0;
    for (uint32_t i = start; i < context_len; i++) {
        uint32_t cur_word_id = context[i];
        uint32_t next_word_start = model->uni_gram[cur_word_id].next_arr_idx;
        uint32_t next_word_end = model->uni_gram[cur_word_id+1].next_arr_idx;
        uint32_t next = -1;
        uint32_t ngram_idx = 0;
        for (uint32_t j = i+1; j < context_len;j++) {
            next = search_low_gram(context[j], model->low_grams[ngram_idx] + next_word_start, next_word_end - next_word_start);
            ngram_idx+=1;
            if (next == -1) {
                break;
            }else{
                next_word_start = model->low_grams[ngram_idx][next].next_arr_idx;
                next_word_end = model->low_grams[ngram_idx][next+1].next_arr_idx;
            }
        }
        if (next != -1) {
            //have found
            size_t arr_size = next_word_end - next_word_start;
            size_t arr_real_size = 0;
            NextWordResult_T* word_results = malloc(sizeof(NextWordResult_T)*arr_size);
            if (ngram_idx+2 != model->ngram_order) {
                //in low gram array
                for (size_t i = 0; i < arr_size; i++) {
                    LowGram_T* gram = &model->low_grams[ngram_idx][next_word_start+i];
                    if (gram->prob != (uint32_t)-1) {//srilm's bug
                        word_results[arr_real_size].word_id = gram->word; 
                        word_results[arr_real_size].prob = gram->prob;
                        arr_real_size++;
                    }
                }
            }else{
                //in high gram array
                for (size_t i = 0; i < arr_size; i++) {
                    HighGram_T* gram = &model->high_gram[next_word_start+i];
                    if (gram->prob != (uint32_t)-1) {
                        word_results[arr_real_size].word_id = gram->word; 
                        word_results[arr_real_size].prob = gram->prob;
                        arr_real_size++;
                    }
                }
            }
            qsort(word_results,arr_real_size, sizeof(NextWordResult_T), next_word_cmp);
            //copy to dst
            //if enough, then break
            for (size_t i = 0;i< arr_real_size;i++) {
                rets[res_num++] = word_results[i];
                if (res_num >= next_word_limit) {
                    free(word_results);
                    return next_word_limit;
                }
            }
            free(word_results);
        }
    }
    return res_num;
}

void Ngram_free(Ngram *model_){
    assert(model_ && *model_);
    Ngram model = *model_;
    HashVocab_free(&model->vocab);
    free(model);
    *model_ = NULL;
}


uint32_t Ngram_prob(const Ngram model, const uint32_t* gram_words, uint32_t gram_len){
    uint32_t cur_word_id = gram_words[0];
    uint32_t next_word_start = model->uni_gram[cur_word_id].next_arr_idx;
    uint32_t next_word_end = model->uni_gram[cur_word_id+1].next_arr_idx;
    //printf("order:%d, cur_word_id:%d,next_word_start:%d, next_word_end:%d\n",gram_len, cur_word_id, next_word_start, next_word_end);
    if (gram_len == 1) {
        return model->uni_gram[cur_word_id].prob;
    }
    if (gram_len > model->ngram_order) {
        return -1;
    }
    uint32_t next = -1;
    uint32_t ngram_idx = 0;
    for (uint32_t j = 1; j < gram_len;j++) {
        if (j == gram_len -1) {
            if (j + 1 == model->ngram_order) {
                next = search_high_gram(gram_words[j], model->high_gram + next_word_start, next_word_end - next_word_start)+next_word_start;
                //printf("next:%d, prob:%d\n", next, model->high_gram[next].prob);
                return model->high_gram[next].prob;
            }else{
                next = search_low_gram(gram_words[j], model->low_grams[ngram_idx] + next_word_start, next_word_end - next_word_start)+next_word_start; 
                //printf("next:%d, prob:%d\n", next, model->low_grams[ngram_idx][next].prob);
                return model->low_grams[ngram_idx][next].prob;
            }
        }else{
            next = search_low_gram(gram_words[j], model->low_grams[ngram_idx] + next_word_start, next_word_end - next_word_start)+next_word_start; 
        }
        if (next == -1) {
            //printf("cannot find.\n");
            return -1;
        }else{
            next_word_start = model->low_grams[ngram_idx][next].next_arr_idx;
            next_word_end = model->low_grams[ngram_idx][next+1].next_arr_idx;
        }
        ngram_idx+=1;
        //printf("next:%d, next_word_start:%d, next_word_end:%d\n", next,next_word_start,next_word_end);
    }
    //printf("cannot find.\n");
    return -1;
}

uint32_t Ngram_bow(const Ngram model, const uint32_t* gram_words, uint32_t gram_len){
    uint32_t cur_word_id = gram_words[0];
    uint32_t next_word_start = model->uni_gram[cur_word_id].next_arr_idx;
    uint32_t next_word_end = model->uni_gram[cur_word_id+1].next_arr_idx;
    if (gram_len == 1) {
        return model->uni_gram[cur_word_id].bow;
    }
    if (gram_len >= model->ngram_order) {
        return 0;
    }
    uint32_t next = -1;
    uint32_t ngram_idx = 0;
    for (uint32_t j = 1; j < gram_len;j++) {
        next = search_low_gram(gram_words[j], model->low_grams[ngram_idx] + next_word_start, next_word_end - next_word_start)+next_word_start; 
        if(j == gram_len -1){
           return model->low_grams[ngram_idx][next].bow;
        }
        if (next == -1) {
            return -1;
        }else{
            next_word_start = model->low_grams[ngram_idx][next].next_arr_idx;
            next_word_end = model->low_grams[ngram_idx][next+1].next_arr_idx;
        }
        ngram_idx+=1;
    }
    return model->low_grams[ngram_idx][next].bow;
}

uint32_t Ngram_order(const Ngram model){
    return model->ngram_order;
}

size_t Ngram_gram_num(const Ngram model, uint32_t order){
    if (order > model->ngram_order) {
        return 0;
    }
    return model->ngram_counts[order-1];
}

uint32_t Ngram_word_num(const Ngram model){
    return HashVocab_word_num(model->vocab);
}


uint32_t Ngram_prob2(const Ngram model, const char** gram_words, uint32_t gram_len){
    assert(gram_len < MAX_ORDER);
    uint32_t wids[MAX_ORDER];
    for (uint32_t i = 0;i<gram_len;i++) {
        wids[i] = Ngram_id(model,gram_words[i]);
    }
    return Ngram_prob(model, wids, gram_len);
}
uint32_t Ngram_bow2(const Ngram model, const char** gram_words, uint32_t gram_len){
    assert(gram_len < MAX_ORDER);
    uint32_t wids[MAX_ORDER];
    for (uint32_t i = 0;i<gram_len;i++) {
        wids[i] = Ngram_id(model,gram_words[i]);
    }
    return Ngram_bow(model, wids, gram_len);
}

struct NgramBuilder_T{
    HashVocab vocab;
    size_t* gram_nums;
    size_t* added_gram_nums;
    char* ngram_cache;
    size_t added_total_gram_num;
    char** ngram_offsets;
    size_t cache_size;
    size_t used_cache_size;
    uint32_t order;
};

static const size_t avg_ngram_len = 4 * 6;

NgramBuilder NgramBuilder_init(const size_t* gram_nums, uint32_t order){
    assert(order > 0);
    NgramBuilder builder = malloc(sizeof(struct NgramBuilder_T));
    builder->vocab = HashVocab_init(gram_nums[0]);
    builder->gram_nums = malloc(order*sizeof(size_t));
    builder->added_gram_nums = malloc(order*sizeof(size_t));
    uint32_t total_num = 0;
    for (uint32_t i = 0;i< order;i++) {
        builder->gram_nums[i] = gram_nums[i];
        builder->added_gram_nums[i] = 0;
        total_num += gram_nums[i];
    }

    builder->cache_size = avg_ngram_len*total_num;
    builder->used_cache_size = 0;
    builder->ngram_cache = malloc(builder->cache_size);

    builder->ngram_offsets = malloc(total_num * sizeof(char*));
    builder->added_total_gram_num = 0;
    builder->order = order;
    return builder;
}

int debug = 0;

void print_binary(char* arr, uint32_t len){
    for (uint32_t i = 0;i<len;i++) {
        printf("%x ", (unsigned char)arr[i]);
    }
    printf("\n");
}


#define max(a,b) ((a)>(b)?(a):(b))
char* _NgramBuilder_cp_str(NgramBuilder builder, const char* encoded_ngram, size_t len){
    if(builder->used_cache_size + len >= builder->cache_size){
        builder->cache_size = max(builder->cache_size, len)*2;
        builder->ngram_cache = realloc(builder->ngram_cache, builder->cache_size);
    }
    //printf("new_word_strs_size:%zu\n", vocab->word_strs_used_size);
    //printf("word_strs_size:%zu\n", vocab->word_strs_size);
    uint32_t offset = builder->used_cache_size;
    memcpy(builder->ngram_cache + offset, encoded_ngram, len);

    builder->used_cache_size = offset + len;
    return offset + builder->ngram_cache; 
}

static inline size_t _encode_uni(uint32_t word_id, uint32_t prob, uint32_t bow, char* buff){
    char header = (1<<7)|1;//has prob; order = 1
    if (bow != 0) {
        header |= (1<<6);
    }
    buff[0] = header;
    size_t offset = sizeof(char);

    memcpy(buff+offset,(char*)&word_id,sizeof(uint32_t));
    offset += sizeof(uint32_t);

    memcpy(buff+offset,(char*)&prob, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (bow != 0) {
        memcpy(buff+offset, (char*)&bow, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
    return offset;
}

static inline uint32_t _decode_order(const char* buff){
    return (*buff) & ((1 << 6)-1);
}

static inline size_t _encode_gram(const uint32_t *wids, uint32_t order, uint32_t prob, uint32_t bow, char* buff){
    assert(order < (1<<6) -1);
    char header = (1<<7)|order;//has prob; order = order
    if (bow != 0) {
        header |= (1<<6);
    }
    buff[0] = header;
    size_t offset = sizeof(char);

    for (uint32_t i = 0; i < order;i++) {
        memcpy(buff + offset, (char*)&wids[i], sizeof(uint32_t)); 
        offset += sizeof(uint32_t);
    }

    memcpy(buff+offset,(char*)&prob, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (bow != 0) {
        memcpy(buff+offset, (char*)&bow, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }
    return offset;
}

static inline void _decode_gram(uint32_t *wids, uint32_t* order, uint32_t* prob, uint32_t* bow,const char* buff){
    char header = *buff;
    *order = (header & ((1<<6)-1));
    uint32_t has_bow = header & (1<<6);
    
    size_t offset = sizeof(char);

    for (uint32_t i = 0; i < (*order);i++) {
        memcpy((char*)&wids[i], buff+offset, sizeof(uint32_t)); 
        offset += sizeof(uint32_t);
    }

    memcpy((char*)prob, buff+offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (has_bow != 0) {
        memcpy((char*)bow, buff+offset, sizeof(uint32_t));
    }else{
        *bow = 0;
    }
}



typedef struct {
    uint32_t next_arr_idx;
    uint32_t wids[MAX_ORDER];
    uint32_t prob;
    uint32_t bow;
    uint32_t order;
}BuilderGram_T;

static inline void _decode_builder_gram(BuilderGram_T* gram,const char* buff){
    _decode_gram(gram->wids, &gram->order, &gram->prob, &gram->bow, buff);
}

void print_ngram(BuilderGram_T *gram);
void print_ngrams(char** ngram_offsets , size_t num);


uint32_t NgramBuilder_add_word(NgramBuilder builder, const char* words, uint32_t prob, uint32_t bow){
    //printf("%s ,prob:%d, bow:%d\n", words, prob, bow);
    uint32_t wid = HashVocab_add(builder->vocab, words);
    char encode_buff[20];//actually is 13
    uint32_t len = _encode_uni(wid, prob, bow, encode_buff);
    assert(len <= 20);
    builder->added_gram_nums[0]++;
    builder->ngram_offsets[builder->added_total_gram_num++] = _NgramBuilder_cp_str(builder, encode_buff, len);
    return wid;
}
//encode ngram to (has_prob:1bit,has_bow:1bit, order:6bit), word_ids(order*4 byte), prob(4byte) bow(4byte)
uint32_t NgramBuilder_add_ngram(NgramBuilder builder, const uint32_t* wids, uint32_t order, uint32_t prob, uint32_t bow){
    char encode_buff[1024];//actually is 3 + order*4
    uint32_t len = _encode_gram(wids, order, prob, bow, encode_buff);
    
    assert(len <= 1024);
    builder->added_gram_nums[order-1]++;
    builder->ngram_offsets[builder->added_total_gram_num++] = _NgramBuilder_cp_str(builder, encode_buff, len);
    return 0; 
}

int ngram_sort_cmp(const void* o1,const void* o2){
    const char* e1 = *((const char**) o1);
    const char* e2 = *((const char**) o2);
    uint32_t order1 = ((*e1) & ((1<<6)-1));
    uint32_t order2 = ((*e2) & ((1<<6)-1));

    //BuilderGram_T cur_gram;
    //_decode_builder_gram(&cur_gram, e1);
    //print_ngram(&cur_gram);
    if (order1 < order2) {
        return -1;
    }
    if (order1 > order2) {
        return 1;
    }
    const uint32_t* wids1 = (const uint32_t*)(e1+1);
    const uint32_t* wids2 = (const uint32_t*)(e2+1);
    for (uint32_t i = 0; i< order1;i++) {
        if (wids1[i] < wids2[i]) {
            return -1;
        }
        if (wids1[i] > wids2[i]) {
            return 1;
        }
    }
    /*printf("o1_addr:%zu, o2_addr:%zu\n", (size_t)o1, (size_t)o2);
    printf("e1_addr:%zu, e2_addr:%zu\n", (size_t)e1, (size_t)e2);
    printf("e1:");
    for (int i = 0;i<order1;i++) {
        printf("%d\t", wids1[i]);
    }
    printf("\n");
    printf("e2:");
    for (int i = 0;i<order2;i++) {
        printf("%d\t", wids2[i]);
    }
    printf("\n");*/

    assert(0 && "cannot go to here"); 
    return 0;
}

size_t _bsearch_order_start(char** ngram_offsets, size_t len, uint32_t order){
    int64_t start = 0;
    int64_t end = len-1;
    int64_t mid;
    while (start <= end) {
        mid = (start + end)>>1;
        uint32_t mid_order = _decode_order(ngram_offsets[mid]);
        if (mid_order >= order) {
            end = mid -1;
        }else{
            start = mid + 1;
        }
    }
    return start;
}


/*
static inline int is_gram_prefix(BuilderGram_T* prefix_gram, BuilderGram_T* gram){
    if (gram->order != prefix_gram->order+1){
        return 0;
    }
    for (uint32_t i = 0;i<prefix_gram->order;i++) {
        if (prefix_gram->wids[i] != gram->wids[i]) {
            return 0;
        }
    }
    return 1;
}*/


static inline int cmp_gram_wids(BuilderGram_T* gram1, BuilderGram_T* gram2, uint32_t order){
    for (uint32_t i = 0;i<order;i++) {
        if (gram1->wids[i] > gram2->wids[i]) {
            return 1;
        }else if(gram1->wids[i] < gram2->wids[i]){
            return -1;
        }
    }
    return 0;
}

size_t _bsearch_prefix_start(char** ngram_offsets, size_t len, BuilderGram_T* prefix){
    int64_t start = 0;
    int64_t end = len-1;
    int64_t mid;
    uint32_t order = prefix->order +1;
    BuilderGram_T mid_gram;
    int res = -1;
    while (start <= end) {
        mid = (start + end)>>1;
        _decode_builder_gram(&mid_gram,ngram_offsets[mid]);
        uint32_t mid_order =mid_gram.order;
        if (mid_order > order) {
            end = mid -1;
        }else if(mid_order < order){
            start = mid + 1;
        }else{
            res = cmp_gram_wids(prefix, &mid_gram, prefix->order);
            if (res > 0) {
                start = mid + 1;
            }else{
                end = mid - 1;
            }
        }
    }
    return start;
}

void serialize_builder_gram(BuilderGram_T* gram, uint32_t model_order,Serializer serializer, void* param){
    if (gram->order == model_order && model_order != 1) {
        HighGram_T hgram;
        hgram.word = gram->wids[gram->order-1];
        hgram.prob = gram->prob;
        //printf("highgram, wid:%d, prob:%d\n", hgram.word, hgram.prob);
        serializer((char*)&hgram, sizeof(HighGram_T), param);
        return;
    }
    if (gram->order > 1 && gram->order < model_order) {
        LowGram_T lgram;
        lgram.word = gram->wids[gram->order-1];
        lgram.next_arr_idx = gram->next_arr_idx;
        lgram.prob = gram->prob;
        lgram.bow = gram->bow;
        //printf("lowgram, order:%d,wid:%d, prob:%d, bow:%d, next_arr_idx:%d\n", gram->order, lgram.word, lgram.prob, lgram.bow, lgram.next_arr_idx);
        serializer((char*)&lgram, sizeof(LowGram_T), param);
        return;
    }
    if (gram->order == 1) {
        UniGram_T ugram;
        ugram.bow = gram->bow;
        ugram.next_arr_idx = gram->next_arr_idx;
        ugram.prob = gram->prob;
        //printf("unigram, prob:%d, bow:%d, next_arr_idx:%d\n", ugram.prob, ugram.bow, ugram.next_arr_idx);
        serializer((char*)&ugram, sizeof(UniGram_T), param);
        return;
    }
}

int NgramBuilder_serialize(const NgramBuilder builder, Serializer serializer, void* param){
    for (uint32_t i = 0;i<builder->order;i++) {
        assert(builder->added_gram_nums[i] == builder->gram_nums[i]);
    }
    serializer((char*)&builder->order,sizeof(uint32_t), param);
    for (uint32_t i = 0; i < builder->order;i++) {
        serializer((char*)&builder->gram_nums[i],sizeof(size_t), param);
    }
    size_t vocab_mem_size = HashVocab_serialize_size(builder->vocab);
    //printf("serialize vocab_mem_size:%zu\n", vocab_mem_size);
    serializer((char*)&vocab_mem_size,sizeof(vocab_mem_size), param);
    HashVocab_serialize(builder->vocab, serializer, param);

    qsort(builder->ngram_offsets, builder->added_total_gram_num, sizeof(char*), ngram_sort_cmp);
    //now sorted by order, wordids
    //print_ngrams(builder->ngram_offsets, builder->added_total_gram_num);

    if (builder->order == 1) {
        BuilderGram_T gram;
        for (uint32_t i = 0;i< builder->gram_nums[0];i++) {
            _decode_builder_gram(&gram, builder->ngram_offsets[i]);
            gram.next_arr_idx = -1;
            serialize_builder_gram(&gram, builder->order, serializer ,param);
        }
        gram.bow = -1;
        gram.prob = -1;
        serialize_builder_gram(&gram, builder->order, serializer, param);//pivot.
        return 0;
    }
    size_t cur_offset = 0;
    size_t next_order_offsets[MAX_ORDER];
    next_order_offsets[0] = 0;
    for (uint32_t cur_order = 1; cur_order< builder->order;cur_order++) {
        next_order_offsets[cur_order] = _bsearch_order_start(builder->ngram_offsets, builder->added_total_gram_num, cur_order+1);
        //printf("serialize: next_order_offsets[%d]= %zu\n", cur_order, next_order_offsets[cur_order]);
    }

    next_order_offsets[builder->order] = builder->added_total_gram_num;

    for (uint32_t cur_order = 1; cur_order < builder->order; cur_order++) {
        BuilderGram_T cur_gram;
        while (cur_offset < next_order_offsets[cur_order]) {
            _decode_builder_gram(&cur_gram, builder->ngram_offsets[cur_offset]);
            //printf("cur_order:%d\n", cur_order);
            //printf("cur_gram.order:%d\n", cur_gram.order);
            assert(cur_gram.order == cur_order);
            cur_gram.next_arr_idx = _bsearch_prefix_start(builder->ngram_offsets, builder->added_total_gram_num, &cur_gram) - next_order_offsets[cur_order];
            //if (cur_order == 1) {
            //    printf("id:%d\t", cur_offset);
            //}
            serialize_builder_gram(&cur_gram, builder->order, serializer, param); 
            cur_offset += 1;
        }
        cur_gram.wids[cur_order-1] = -1;
        cur_gram.order = cur_order;
        cur_gram.prob = -1;
        cur_gram.bow = -1;
        cur_gram.next_arr_idx = next_order_offsets[cur_order+1] - next_order_offsets[cur_order];
        serialize_builder_gram(&cur_gram, builder->order, serializer, param);
    }
    if (builder->order > 1) {
        BuilderGram_T gram;
        gram.order = builder->order;
        for (uint32_t i =  next_order_offsets[builder->order-1];i< builder->added_total_gram_num;i++) {
            _decode_builder_gram(&gram, builder->ngram_offsets[i]);
            gram.next_arr_idx = -1;
            serialize_builder_gram(&gram, builder->order, serializer, param);
        }
        gram.bow = -1;
        gram.prob = -1;
        gram.wids[builder->order-1] = -1;
        serialize_builder_gram(&gram, builder->order, serializer, param);//pivot.
    }
    return 0; 
}

size_t NgramBuilder_serialize_size(const NgramBuilder builder){
    size_t total_size = sizeof(uint32_t)//order
    + sizeof(size_t)* builder->order//ngram num for every order
    + sizeof(size_t) //vocab mem size
    + HashVocab_serialize_size(builder->vocab) //vocab
    + sizeof(UniGram_T) * (builder->gram_nums[0]+1); //unigram
    //at the end of gram array, last element is pivot.
    for (uint32_t i = 1; i < builder->order-1; i++) {
        total_size += sizeof(LowGram_T) * (builder->gram_nums[i]+1);
    }
    if (builder->order > 1) {
        total_size += sizeof(HighGram_T) * (builder->gram_nums[builder->order-1]+1);
    }
    return total_size;
}

void NgramBuilder_free(NgramBuilder* builder_){
    assert(builder_ && *builder_);
    NgramBuilder builder = *builder_;
    HashVocab_free(&builder->vocab);
    free(builder->ngram_cache);
    free(builder->ngram_offsets);
    free(builder->gram_nums);
    free(builder->added_gram_nums);
    free(builder);
    *builder_ = NULL;
}

HashVocab NgramBuilder_vocab(const NgramBuilder builder){
    return builder->vocab;
}

void print_ngram(BuilderGram_T *gram){
    printf("order:%d\t", gram->order);
    printf("words:");
    for (int i = 0;i< gram->order;i++) {
        printf("%d\t", gram->wids[i]);
    }
    printf("\n");
}

void print_ngrams(char** ngram_offsets , size_t num){
    BuilderGram_T cur_gram;
    
    for (size_t i = 0;i<num;i++) {
        _decode_builder_gram(&cur_gram, ngram_offsets[i]);
        print_ngram(&cur_gram);
    }
}


uint32_t NgramBuilder_add_ngram2(NgramBuilder builder, const char** words, uint32_t order, uint32_t prob, uint32_t bow){
    uint32_t wids[MAX_ORDER];
    //for(int i = 0; i<order;i++){
    //    printf("%s ,prob:%d, bow:%d\n", words[i], prob, bow);
    //}
    if (order > 1) {
        for (int i = 0; i < order; i++) {
            wids[i] = HashVocab_id(builder->vocab, words[i]);
        }
        return NgramBuilder_add_ngram(builder, wids, order, prob, bow);
    }else{
        assert(order == 1);
        return NgramBuilder_add_word(builder,words[0],prob,bow);
    }
}

typedef struct{
    size_t offset;
    char* ptr;
}_NgramBuilder_SerializerParam;

void _ngrambuilder_serializer(char* data,size_t len, void* param_){
    _NgramBuilder_SerializerParam *param = (_NgramBuilder_SerializerParam*) param_;
    memcpy(param->ptr+param->offset, data, len);
    param->offset += len;
}

int NgramBuilder_save(NgramBuilder builder, char* path){
    size_t size = NgramBuilder_serialize_size(builder);
    char* rom = malloc(size);
    _NgramBuilder_SerializerParam param;
    param.ptr = rom;
    param.offset = 0;
    NgramBuilder_serialize(builder, _ngrambuilder_serializer, &param);
    assert(param.offset == size);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return 1;
    }
    fwrite(rom, sizeof(char), size, f); 
    fclose(f);

    free(rom);
    return 0;
}
