/* Inference for Llama-2 Transformer model in pure C - Adapted for xv6 */

// ----------------------------------------------------------------------------
// XV6 Headers & Macros
// ----------------------------------------------------------------------------
#include "kernel/types.h"
#include "user/user.h"
#include "user/xv6_maths.h"
#include "user/udp_client.h"

typedef unsigned long size_t;

#define EXIT_FAILURE 1
#define stderr 2

// Math mappings
#define sqrtf xv6_sqrtf
#define expf xv6_expf
#define powf xv6_powf
#define cosf xv6_cosf
#define sinf xv6_sinf
#define fabsf xv6_fabsf

// ----------------------------------------------------------------------------
// Helpers (Missing in xv6)
// ----------------------------------------------------------------------------

int abs(int x) {
    return (x < 0) ? -x : x;
}

int isprint(char c) {
    return c >= 32 && c < 127;
}

int isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

// ----------------------------------------------------------------------------
// Transformer model
// ----------------------------------------------------------------------------

typedef struct {
    int dim; // transformer dimension
    int hidden_dim; // for ffn layers
    int n_layers; // number of layers
    int n_heads; // number of query heads
    int n_kv_heads; // number of key/value heads (can be < query heads because of multiquery)
    int vocab_size; // vocabulary size, usually 256 (byte-level)
    int seq_len; // max sequence length
} Config;

typedef struct {
    // token embedding table
    float* token_embedding_table;    // (vocab_size, dim)
    // weights for rmsnorms
    float* rms_att_weight; // (layer, dim) rmsnorm weights
    float* rms_ffn_weight; // (layer, dim)
    // weights for matmuls. note dim == n_heads * head_size
    float* wq; // (layer, dim, n_heads * head_size)
    float* wk; // (layer, dim, n_kv_heads * head_size)
    float* wv; // (layer, dim, n_kv_heads * head_size)
    float* wo; // (layer, n_heads * head_size, dim)
    // weights for ffn
    float* w1; // (layer, hidden_dim, dim)
    float* w2; // (layer, dim, hidden_dim)
    float* w3; // (layer, hidden_dim, dim)
    // final rmsnorm
    float* rms_final_weight; // (dim,)
    // (optional) classifier weights for the logits, on the last layer
    float* wcls;
} TransformerWeights;

typedef struct {
    // current wave of activations
    float *x; // activation at current time stamp (dim,)
    float *xb; // same, but inside a residual branch (dim,)
    float *xb2; // an additional buffer just for convenience (dim,)
    float *hb; // buffer for hidden dimension in the ffn (hidden_dim,)
    float *hb2; // buffer for hidden dimension in the ffn (hidden_dim,)
    float *q; // query (dim,)
    float *k; // key (dim,)
    float *v; // value (dim,)
    float *att; // buffer for scores/attention values (n_heads, seq_len)
    float *logits; // output logits
    // kv cache
    float* key_cache;   // (layer, seq_len, dim)
    float* value_cache; // (layer, seq_len, dim)
} RunState;

typedef struct {
    Config config; // the hyperparameters of the architecture (the blueprint)
    TransformerWeights weights; // the weights of the model
    RunState state; // buffers for the "wave" of activations in the forward pass
    // Removed fd and file_size, we use raw buffers now
    float* data; // pointer to the raw weight buffer
} Transformer;

void malloc_run_state(RunState* s, Config* p) {
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    s->x = calloc(p->dim, sizeof(float));
    s->xb = calloc(p->dim, sizeof(float));
    s->xb2 = calloc(p->dim, sizeof(float));
    s->hb = calloc(p->hidden_dim, sizeof(float));
    s->hb2 = calloc(p->hidden_dim, sizeof(float));
    s->q = calloc(p->dim, sizeof(float));
    s->key_cache = calloc(p->n_layers * p->seq_len * kv_dim, sizeof(float));
    s->value_cache = calloc(p->n_layers * p->seq_len * kv_dim, sizeof(float));
    s->att = calloc(p->n_heads * p->seq_len, sizeof(float));
    s->logits = calloc(p->vocab_size, sizeof(float));
    // ensure all mallocs went fine
    if (!s->x || !s->xb || !s->xb2 || !s->hb || !s->hb2 || !s->q
     || !s->key_cache || !s->value_cache || !s->att || !s->logits) {
        fprintf(stderr, "malloc failed!\n");
        exit(EXIT_FAILURE);
    }
}

void free_run_state(RunState* s) {
    free(s->x);
    free(s->xb);
    free(s->xb2);
    free(s->hb);
    free(s->hb2);
    free(s->q);
    free(s->att);
    free(s->logits);
    free(s->key_cache);
    free(s->value_cache);
}

void memory_map_weights(TransformerWeights *w, Config* p, float* ptr, int shared_weights) {
    int head_size = p->dim / p->n_heads;
    unsigned long long n_layers = p->n_layers;
    w->token_embedding_table = ptr;
    ptr += p->vocab_size * p->dim;
    w->rms_att_weight = ptr;
    ptr += n_layers * p->dim;
    w->wq = ptr;
    ptr += n_layers * p->dim * (p->n_heads * head_size);
    w->wk = ptr;
    ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
    w->wv = ptr;
    ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
    w->wo = ptr;
    ptr += n_layers * (p->n_heads * head_size) * p->dim;
    w->rms_ffn_weight = ptr;
    ptr += n_layers * p->dim;
    w->w1 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->w2 = ptr;
    ptr += n_layers * p->hidden_dim * p->dim;
    w->w3 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->rms_final_weight = ptr;
    ptr += p->dim;
    ptr += p->seq_len * head_size / 2; 
    ptr += p->seq_len * head_size / 2; 
    w->wcls = shared_weights ? w->token_embedding_table : ptr;
}

// MODIFIED: Fetches weights over network instead of mmap/fopen
void read_checkpoint(Config* config, TransformerWeights* weights, float** data_ptr) {
    int total_size = 0;
    
    printf("Fetching model weights via UDP...\n");
    char* raw_buffer = fetch_model_weights(&total_size);
    if (raw_buffer == 0) {
        fprintf(stderr, "Failed to fetch model weights\n");
        exit(EXIT_FAILURE);
    }
    printf("Model downloaded: %d bytes\n", total_size);

    // 1. Read config from the start of the buffer
    // raw_buffer is char*, we cast to Config* to read header
    Config* fetched_config = (Config*)raw_buffer;
    *config = *fetched_config; // Copy struct

    int shared_weights = config->vocab_size > 0 ? 1 : 0;
    config->vocab_size = abs(config->vocab_size);

    // 2. Weights start immediately after the Config struct
    // We cast the pointer logic to float*
    *data_ptr = (float*)(raw_buffer + sizeof(Config));

    // 3. Map pointers
    memory_map_weights(weights, config, *data_ptr, shared_weights);
}

void build_transformer(Transformer *t) {
    // read in the Config and the Weights from the checkpoint
    read_checkpoint(&t->config, &t->weights, &t->data);
    // allocate the RunState buffers
    malloc_run_state(&t->state, &t->config);
}

void free_transformer(Transformer* t) {
    // We cannot easily 'unmap' or free the big buffer if it was alloc'd inside fetch_model_weights
    // If fetch_model_weights used malloc, we should free the base pointer (t->data - sizeof(Config))
    // For now, in xv6 user programs, OS cleans up on exit.
    free_run_state(&t->state);
}

// ----------------------------------------------------------------------------
// neural net blocks; the dynamics of the Transformer

void rmsnorm(float* o, float* x, float* weight, int size) {
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / sqrtf(ss);
    for (int j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

void softmax(float* x, int size) {
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) {
            max_val = x[i];
        }
    }
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

void matmul(float* xout, float* x, float* w, int n, int d) {
    // W (d,n) @ x (n,) -> xout (d,)
    // REMOVED OMP PARALLEL
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        xout[i] = val;
    }
}

float* forward(Transformer* transformer, int token, int pos) {
    Config* p = &transformer->config;
    TransformerWeights* w = &transformer->weights;
    RunState* s = &transformer->state;
    float *x = s->x;
    int dim = p->dim;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int kv_mul = p->n_heads / p->n_kv_heads; 
    int hidden_dim =  p->hidden_dim;
    int head_size = dim / p->n_heads;

    float* content_row = w->token_embedding_table + token * dim;
    memcpy(x, content_row, dim*sizeof(*x));

    for(unsigned long long l = 0; l < p->n_layers; l++) {
        rmsnorm(s->xb, x, w->rms_att_weight + l*dim, dim);

        int loff = l * p->seq_len * kv_dim; 
        s->k = s->key_cache + loff + pos * kv_dim;
        s->v = s->value_cache + loff + pos * kv_dim;

        matmul(s->q, s->xb, w->wq + l*dim*dim, dim, dim);
        matmul(s->k, s->xb, w->wk + l*dim*kv_dim, dim, kv_dim);
        matmul(s->v, s->xb, w->wv + l*dim*kv_dim, dim, kv_dim);

        for (int i = 0; i < dim; i+=2) {
            int head_dim = i % head_size;
            float freq = 1.0f / powf(10000.0f, head_dim / (float)head_size);
            float val = pos * freq;
            float fcr = cosf(val);
            float fci = sinf(val);
            int rotn = i < kv_dim ? 2 : 1;
            for (int v = 0; v < rotn; v++) {
                float* vec = v == 0 ? s->q : s->k;
                float v0 = vec[i];
                float v1 = vec[i+1];
                vec[i]   = v0 * fcr - v1 * fci;
                vec[i+1] = v0 * fci + v1 * fcr;
            }
        }

        // REMOVED OMP PARALLEL
        for (int h = 0; h < p->n_heads; h++) {
            float* q = s->q + h * head_size;
            float* att = s->att + h * p->seq_len;
            for (int t = 0; t <= pos; t++) {
                float* k = s->key_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                float score = 0.0f;
                for (int i = 0; i < head_size; i++) {
                    score += q[i] * k[i];
                }
                score /= sqrtf(head_size);
                att[t] = score;
            }

            softmax(att, pos + 1);

            float* xb = s->xb + h * head_size;
            memset(xb, 0, head_size * sizeof(float));
            for (int t = 0; t <= pos; t++) {
                float* v = s->value_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                float a = att[t];
                for (int i = 0; i < head_size; i++) {
                    xb[i] += a * v[i];
                }
            }
        }

        matmul(s->xb2, s->xb, w->wo + l*dim*dim, dim, dim);

        for (int i = 0; i < dim; i++) {
            x[i] += s->xb2[i];
        }

        rmsnorm(s->xb, x, w->rms_ffn_weight + l*dim, dim);

        matmul(s->hb, s->xb, w->w1 + l*dim*hidden_dim, dim, hidden_dim);
        matmul(s->hb2, s->xb, w->w3 + l*dim*hidden_dim, dim, hidden_dim);

        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];
            val *= (1.0f / (1.0f + expf(-val)));
            val *= s->hb2[i];
            s->hb[i] = val;
        }

        matmul(s->xb, s->hb, w->w2 + l*dim*hidden_dim, hidden_dim, dim);

        for (int i = 0; i < dim; i++) {
            x[i] += s->xb[i];
        }
    }

    rmsnorm(x, x, w->rms_final_weight, dim);
    matmul(s->logits, x, w->wcls, p->dim, p->vocab_size);
    return s->logits;
}

// ----------------------------------------------------------------------------
// Tokenizer
// ----------------------------------------------------------------------------

typedef struct {
    char *str;
    int id;
} TokenIndex;

typedef struct {
    char** vocab;
    float* vocab_scores;
    TokenIndex *sorted_vocab;
    int vocab_size;
    unsigned int max_token_length;
    unsigned char byte_pieces[512]; 
} Tokenizer;

int compare_tokens(const void *a, const void *b) {
    return strcmp(((TokenIndex*)a)->str, ((TokenIndex*)b)->str);
}

// MODIFIED: Fetches tokenizer over network
void build_tokenizer(Tokenizer* t, int vocab_size) {
    t->vocab_size = vocab_size;
    t->vocab = (char**)malloc(vocab_size * sizeof(char*));
    t->vocab_scores = (float*)malloc(vocab_size * sizeof(float));
    t->sorted_vocab = 0; 

    for (int i = 0; i < 256; i++) {
        t->byte_pieces[i * 2] = (unsigned char)i;
        t->byte_pieces[i * 2 + 1] = '\0';
    }

    printf("Fetching tokenizer via UDP...\n");
    int size = 0;
    char* buf = fetch_tokenizer(&size);
    if (!buf) { fprintf(stderr, "couldn't fetch tokenizer\n"); exit(EXIT_FAILURE); }
    
    // Manual deserialization from buffer
    char* ptr = buf;

    // Read max_token_length (int)
    memcpy(&t->max_token_length, ptr, sizeof(int));
    ptr += sizeof(int);

    int len;
    for (int i = 0; i < vocab_size; i++) {
        // Read score (float)
        memcpy(t->vocab_scores + i, ptr, sizeof(float));
        ptr += sizeof(float);

        // Read len (int)
        memcpy(&len, ptr, sizeof(int));
        ptr += sizeof(int);

        // Read string
        t->vocab[i] = (char *)malloc(len + 1);
        memcpy(t->vocab[i], ptr, len);
        t->vocab[i][len] = '\0';
        ptr += len;
    }
    
    // We should free the network buffer if possible, assuming malloc was used in udp_client
    free(buf);
}

void free_tokenizer(Tokenizer* t) {
    for (int i = 0; i < t->vocab_size; i++) { free(t->vocab[i]); }
    free(t->vocab);
    free(t->vocab_scores);
    free(t->sorted_vocab);
}

char* decode(Tokenizer* t, int prev_token, int token) {
    char *piece = t->vocab[token];
    if (prev_token == 1 && piece[0] == ' ') { piece++; }
    
    unsigned char byte_val;
    // xv6 does not have sscanf. Manual parsing for <0xXX>
    if (piece[0] == '<' && piece[1] == '0' && piece[2] == 'x' && 
        piece[5] == '>' && piece[6] == '\0') {
        
        // simple hex conversion for 2 chars
        char c1 = piece[3];
        char c2 = piece[4];
        int v1 = (c1 >= '0' && c1 <= '9') ? c1 - '0' : (c1 >= 'A' && c1 <= 'F') ? c1 - 'A' + 10 : c1 - 'a' + 10;
        int v2 = (c2 >= '0' && c2 <= '9') ? c2 - '0' : (c2 >= 'A' && c2 <= 'F') ? c2 - 'A' + 10 : c2 - 'a' + 10;
        byte_val = (v1 << 4) | v2;
        piece = (char*)t->byte_pieces + byte_val * 2;
    }
    
    return piece;
}

void safe_printf(char *piece) {
    if (piece == 0) { return; }
    if (piece[0] == '\0') { return; }
    if (piece[1] == '\0') {
        unsigned char byte_val = piece[0];
        if (!(isprint(byte_val) || isspace(byte_val))) {
            return; 
        }
    }
    printf("%s", piece);
}

int str_lookup(char *str, TokenIndex *sorted_vocab, int vocab_size) {
    TokenIndex tok;
    tok.str = str;
    tok.id = 0; 
    TokenIndex *res = bsearch(&tok, sorted_vocab, vocab_size, sizeof(TokenIndex), compare_tokens);
    return res != 0 ? res->id : -1;
}

void encode(Tokenizer* t, char *text, int bos, int eos, int *tokens, int *n_tokens) {
    if (text == 0) { fprintf(stderr, "cannot encode NULL text\n"); exit(EXIT_FAILURE); }

    if (t->sorted_vocab == 0) {
        t->sorted_vocab = malloc(t->vocab_size * sizeof(TokenIndex));
        for (int i = 0; i < t->vocab_size; i++) {
            t->sorted_vocab[i].str = t->vocab[i];
            t->sorted_vocab[i].id = i;
        }
        qsort(t->sorted_vocab, t->vocab_size, sizeof(TokenIndex), compare_tokens);
    }

    char* str_buffer = malloc((t->max_token_length*2 +1 +2) * sizeof(char));
    size_t str_len = 0;

    *n_tokens = 0;
    if (bos) tokens[(*n_tokens)++] = 1;

    if (text[0] != '\0') {
        int dummy_prefix = str_lookup(" ", t->sorted_vocab, t->vocab_size);
        tokens[(*n_tokens)++] = dummy_prefix;
    }

    for (char *c = text; *c != '\0'; c++) {
        if ((*c & 0xC0) != 0x80) {
            str_len = 0;
        }

        str_buffer[str_len++] = *c;
        str_buffer[str_len] = '\0';

        if ((*(c+1) & 0xC0) == 0x80 && str_len < 4) {
            continue;
        }

        int id = str_lookup(str_buffer, t->sorted_vocab, t->vocab_size);

        if (id != -1) {
            tokens[(*n_tokens)++] = id;
        } else {
            for (int i=0; i < str_len; i++) {
                tokens[(*n_tokens)++] = (unsigned char)str_buffer[i] + 3;
            }
        }
        str_len = 0; 
    }

    while (1) {
        float best_score = -1e10;
        int best_id = -1;
        int best_idx = -1;

        for (int i=0; i < (*n_tokens-1); i++) {
            // Replaced sprintf with manual construction or assumption
            // xv6 doesn't have snprintf/sprintf in user.h usually, only printf/fprintf
            // For this milestone, we might skip the BPE merge loop if we lack tools, 
            // BUT since this is critical, we assume user added string functions or we implement minimal copy
            // Let's rely on basic strcpy/strcat if available or simple pointer manipulation
            
            // NOTE: Since xv6 user lib is minimal, we assume standard sprintf isn't available to string buffer
            // To be safe for this specific "run.c" port, we will skip the BPE merge loop 
            // if we cannot concat easily. However, `user.h` has no sprintf.
            // Let's implement a minimal concat manually for the buffer.
            
            char *s1 = t->vocab[tokens[i]];
            char *s2 = t->vocab[tokens[i+1]];
            strcpy(str_buffer, s1);
            int l1 = strlen(s1);
            strcpy(str_buffer + l1, s2);
            
            int id = str_lookup(str_buffer, t->sorted_vocab, t->vocab_size);
            if (id != -1 && t->vocab_scores[id] > best_score) {
                best_score = t->vocab_scores[id];
                best_id = id;
                best_idx = i;
            }
        }

        if (best_idx == -1) {
            break; 
        }

        tokens[best_idx] = best_id;
        for (int i = best_idx+1; i < (*n_tokens-1); i++) {
            tokens[i] = tokens[i+1];
        }
        (*n_tokens)--; 
    }

    if (eos) tokens[(*n_tokens)++] = 2;

    free(str_buffer);
}

// ----------------------------------------------------------------------------
// Sampler
// ----------------------------------------------------------------------------

typedef struct {
    float prob;
    int index;
} ProbIndex; 

typedef struct {
    int vocab_size;
    ProbIndex* probindex; 
    float temperature;
    float topp;
    unsigned long long rng_state;
} Sampler;

int sample_argmax(float* probabilities, int n) {
    int max_i = 0;
    float max_p = probabilities[0];
    for (int i = 1; i < n; i++) {
        if (probabilities[i] > max_p) {
            max_i = i;
            max_p = probabilities[i];
        }
    }
    return max_i;
}

int sample_mult(float* probabilities, int n, float coin) {
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (coin < cdf) {
            return i;
        }
    }
    return n - 1; 
}

int compare(const void* a, const void* b) {
    ProbIndex* a_ = (ProbIndex*) a;
    ProbIndex* b_ = (ProbIndex*) b;
    if (a_->prob > b_->prob) return -1;
    if (a_->prob < b_->prob) return 1;
    return 0;
}

int sample_topp(float* probabilities, int n, float topp, ProbIndex* probindex, float coin) {
    int n0 = 0;
    const float cutoff = (1.0f - topp) / (n - 1);
    for (int i = 0; i < n; i++) {
        if (probabilities[i] >= cutoff) {
            probindex[n0].index = i;
            probindex[n0].prob = probabilities[i];
            n0++;
        }
    }
    qsort(probindex, n0, sizeof(ProbIndex), compare);

    float cumulative_prob = 0.0f;
    int last_idx = n0 - 1; 
    for (int i = 0; i < n0; i++) {
        cumulative_prob += probindex[i].prob;
        if (cumulative_prob > topp) {
            last_idx = i;
            break; 
        }
    }

    float r = coin * cumulative_prob;
    float cdf = 0.0f;
    for (int i = 0; i <= last_idx; i++) {
        cdf += probindex[i].prob;
        if (r < cdf) {
            return probindex[i].index;
        }
    }
    return probindex[last_idx].index; 
}

void build_sampler(Sampler* sampler, int vocab_size, float temperature, float topp, unsigned long long rng_seed) {
    sampler->vocab_size = vocab_size;
    sampler->temperature = temperature;
    sampler->topp = topp;
    sampler->rng_state = rng_seed;
    sampler->probindex = malloc(sampler->vocab_size * sizeof(ProbIndex));
}

void free_sampler(Sampler* sampler) {
    free(sampler->probindex);
}

unsigned int random_u32(unsigned long long *state) {
    *state ^= *state >> 12;
    *state ^= *state << 25;
    *state ^= *state >> 27;
    return (*state * 0x2545F4914F6CDD1Dull) >> 32;
}
float random_f32(unsigned long long *state) { 
    return (random_u32(state) >> 8) / 16777216.0f;
}

int sample(Sampler* sampler, float* logits) {
    int next;
    if (sampler->temperature == 0.0f) {
        next = sample_argmax(logits, sampler->vocab_size);
    } else {
        for (int q=0; q<sampler->vocab_size; q++) { logits[q] /= sampler->temperature; }
        softmax(logits, sampler->vocab_size);
        float coin = random_f32(&sampler->rng_state);
        if (sampler->topp <= 0 || sampler->topp >= 1) {
            next = sample_mult(logits, sampler->vocab_size, coin);
        } else {
            next = sample_topp(logits, sampler->vocab_size, sampler->topp, sampler->probindex, coin);
        }
    }
    return next;
}

// ----------------------------------------------------------------------------
// Utilities
// ----------------------------------------------------------------------------

// MODIFIED: Returns CPU cycles for timing
unsigned long long get_time() {
    return rdcycle();
}

// ----------------------------------------------------------------------------
// Generation Loop
// ----------------------------------------------------------------------------

void generate(Transformer *transformer, Tokenizer *tokenizer, Sampler *sampler, char *prompt, int steps) {
    char *empty_prompt = "";
    if (prompt == 0) { prompt = empty_prompt; }

    int num_prompt_tokens = 0;
    int* prompt_tokens = (int*)malloc((strlen(prompt)+3) * sizeof(int)); 
    encode(tokenizer, prompt, 1, 0, prompt_tokens, &num_prompt_tokens);
    if (num_prompt_tokens < 1) {
        fprintf(stderr, "something is wrong, expected at least 1 prompt token\n");
        exit(EXIT_FAILURE);
    }

    unsigned long long start = 0;  
    int next;        
    int token = prompt_tokens[0]; 
    int pos = 0;     
    while (pos < steps) {

        float* logits = forward(transformer, token, pos);

        if (pos < num_prompt_tokens - 1) {
            next = prompt_tokens[pos + 1];
        } else {
            next = sample(sampler, logits);
        }
        pos++;

        if (next == 1) { break; }

        char* piece = decode(tokenizer, token, next);
        safe_printf(piece); 
        // fflush not needed/available in basic xv6, prints are usually buffered but handled by kernel console
        
        token = next;

        if (start == 0) { start = get_time(); }
    }
    printf("\n");

    if (pos > 1) {
        unsigned long long end = get_time();
        // Printing raw cycles and an estimated tok/s assuming hypothetical MHz if needed, 
        // or just the cycle count delta as per instructions.
        unsigned long long cycles = end - start;
        printf("Tokens generated: %d\n", pos-1);
        printf("Total Cycles: %ld\n", (long)cycles);
        // Assuming 10MHz for QEMU default (very rough approx), 
        // printf("Approx Tok/s: %f\n", (double)(pos-1) / ((double)cycles / 10000000.0));
    }

    free(prompt_tokens);
}

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    // defaults
    float temperature = 1.0f;   
    float topp = 0.9f;          
    int steps = 256;            
    char *prompt = "Once upon a time"; // Default prompt for testing
    unsigned long long rng_seed = 12345; // Fixed seed for reproducibility/testing

    // Simple arg parsing (Overriding default prompt if provided)
    if (argc > 1) {
        prompt = argv[1];
    }
    // Further args could be parsed here using atoi/atof if needed, 
    // but we keep it simple for xv6 integration.

    // build the Transformer via UDP fetch
    Transformer transformer;
    build_transformer(&transformer);
    
    if (steps == 0 || steps > transformer.config.seq_len) steps = transformer.config.seq_len; 

    // build the Tokenizer via UDP fetch
    Tokenizer tokenizer;
    build_tokenizer(&tokenizer, transformer.config.vocab_size);

    // build the Sampler
    Sampler sampler;
    build_sampler(&sampler, transformer.config.vocab_size, temperature, topp, rng_seed);

    // run!
    generate(&transformer, &tokenizer, &sampler, prompt, steps);

    // cleanup
    free_sampler(&sampler);
    free_tokenizer(&tokenizer);
    free_transformer(&transformer);
    
    exit(0);
}