/* Inference for Llama-2 Transformer model in pure C - Adapted for xv6 with Multithreading */

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
#define NUM_THREADS 4

// Math mappings
#define sqrtf xv6_sqrtf
#define expf xv6_expf
#define powf xv6_powf
#define cosf xv6_cosf
#define sinf xv6_sinf
#define fabsf xv6_fabsf

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

struct ProfilingData {
    unsigned long long start_time;
    unsigned long long ttft_end;     // Time to First Token
    unsigned long long end_time;     
    unsigned long long total_matmul;
    unsigned long long total_attention;
    unsigned long long total_ffn;
    unsigned long long total_activation;
    unsigned long long total_sampling;
    int first_token_flag;
    int total_tokens_generated;
    int prompt_tokens;
};

struct ProfilingData prof; // Global instance

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
    int n_kv_heads; // number of key/value heads 
    int vocab_size; // vocabulary size
    int seq_len; // max sequence length
} Config;

typedef struct {
    float* token_embedding_table;
    float* rms_att_weight; 
    float* rms_ffn_weight; 
    float* wq; 
    float* wk; 
    float* wv; 
    float* wo; 
    float* w1; 
    float* w2; 
    float* w3; 
    float* rms_final_weight; 
    float* wcls;
} TransformerWeights;

typedef struct {
    float *x; 
    float *xb; 
    float *xb2; 
    float *hb; 
    float *hb2; 
    float *q; 
    float *k; 
    float *v; 
    float *att; 
    float *logits; 
    float* key_cache;   
    float* value_cache; 
} RunState;

typedef struct {
    Config config; 
    TransformerWeights weights; 
    RunState state; 
    float* data; 
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

void read_checkpoint(Config* config, TransformerWeights* weights, float** data_ptr) {
    int total_size = 0;
    printf("Fetching model weights via UDP...\n");
    char* raw_buffer = fetch_model_weights(&total_size);
    if (raw_buffer == 0) {
        fprintf(stderr, "Failed to fetch model weights\n");
        exit(EXIT_FAILURE);
    }
    printf("Model downloaded: %d bytes\n", total_size);

    Config* fetched_config = (Config*)raw_buffer;
    *config = *fetched_config; 
    int shared_weights = config->vocab_size > 0 ? 1 : 0;
    config->vocab_size = abs(config->vocab_size);
    *data_ptr = (float*)(raw_buffer + sizeof(Config));
    memory_map_weights(weights, config, *data_ptr, shared_weights);
}

void build_transformer(Transformer *t) {
    read_checkpoint(&t->config, &t->weights, &t->data);
    malloc_run_state(&t->state, &t->config);
}

void free_transformer(Transformer* t) {
    free_run_state(&t->state);
}

// ----------------------------------------------------------------------------
// Thread Workers and Parallel Functions
// ----------------------------------------------------------------------------

// --- Parallel Matmul ---

typedef struct {
    float* output;
    float* input;
    float* weight;
    int n;
    int d;
    int row_start;
    int row_end;
} MatmulArgs;

void matmul_worker(void *arg) {
    MatmulArgs* args = (MatmulArgs*)arg;
    for (int i = args->row_start; i < args->row_end; i++) {
        float val = 0.0f;
        for (int j = 0; j < args->n; j++) {
            val += args->weight[i * args->n + j] * args->input[j];
        }
        args->output[i] = val;
    }
    thread_exit();
}

void matmul_parallel(float* output, float* input, float* weight, int n, int d) {
    // For small matrices or odd sizes, run serial to avoid overhead
    if (d < NUM_THREADS * 4) {
        for (int i = 0; i < d; i++) {
            float val = 0.0f;
            for (int j = 0; j < n; j++) {
                val += weight[i * n + j] * input[j];
            }
            output[i] = val;
        }
        return;
    }

    int thread_ids[NUM_THREADS];
    MatmulArgs args[NUM_THREADS];

    int rows_per_thread = d / NUM_THREADS;
    int extra_rows = d % NUM_THREADS;
    int current_row = 0;

    for (int t = 0; t < NUM_THREADS; t++) {
        int rows_for_this_thread = rows_per_thread + (t < extra_rows ? 1 : 0);
        
        args[t].output = output;
        args[t].input = input;
        args[t].weight = weight;
        args[t].n = n;
        args[t].d = d;
        args[t].row_start = current_row;
        args[t].row_end = current_row + rows_for_this_thread;

        thread_ids[t] = thread_create(matmul_worker, (void*)&args[t]);
        if (thread_ids[t] < 0) {
            // Fallback if thread creation fails
            fprintf(stderr, "Thread creation failed, running part serial\n");
            for (int i = current_row; i < d; i++) {
                 float val = 0.0f;
                 for (int j = 0; j < n; j++) val += weight[i * n + j] * input[j];
                 output[i] = val;
            }
            return;
        }
        current_row += rows_for_this_thread;
    }

    for (int t = 0; t < NUM_THREADS; t++) {
        if (thread_ids[t] >= 0) {
            thread_join(thread_ids[t]);
        }
    }
}

// Wrapper to replace original serial matmul
void matmul(float* xout, float* x, float* w, int n, int d) {
    unsigned long long start = rdcycle();
    matmul_parallel(xout, x, w, n, d);
    unsigned long long end = rdcycle();
    prof.total_matmul += (end - start);
}

// --- Parallel Attention ---

typedef struct {
    float* att;
    float* q;
    float* key_cache;
    float* xb;
    float* value_cache;
    int seq_len;
    int head_size;
    int pos;
    int loff;
    int dim;
    int kv_dim;
    int kv_mul;
    int head_start;
    int head_end;
} AttentionArgs;

// Helper function that contains the core logic (doesn't exit)
void run_attention_range(AttentionArgs* args) {
    int head_size = args->head_size;
    
    for (int h = args->head_start; h < args->head_end; h++) {
        float* q = args->q + h * head_size;
        float* att = args->att + h * args->seq_len;
        
        // Calculate attention scores
        for (int t = 0; t <= args->pos; t++) {
            // Handle GQA: (h / kv_mul) maps query head to kv head
            int kv_head = h / args->kv_mul;
            float* k = args->key_cache + args->loff + t * args->kv_dim + kv_head * head_size;
            float score = 0.0f;
            for (int i = 0; i < head_size; i++) {
                score += q[i] * k[i];
            }
            score /= sqrtf(head_size);
            att[t] = score;
        }
        
        // Softmax
        float max_val = att[0];
        for (int t = 1; t <= args->pos; t++) {
            if (att[t] > max_val) max_val = att[t];
        }
        
        float sum = 0.0f;
        for (int t = 0; t <= args->pos; t++) {
            att[t] = expf(att[t] - max_val);
            sum += att[t];
        }
        
        for (int t = 0; t <= args->pos; t++) {
            att[t] /= sum;
        }
        
        // Weighted sum of values
        float* xb = args->xb + h * head_size;
        memset(xb, 0, head_size * sizeof(float));
        for (int t = 0; t <= args->pos; t++) {
            int kv_head = h / args->kv_mul;
            float* v = args->value_cache + args->loff + t * args->kv_dim + kv_head * head_size;
            float a = att[t];
            for (int i = 0; i < head_size; i++) {
                xb[i] += a * v[i];
            }
        }
    }
}

void attention_worker(void *arg) {
    run_attention_range((AttentionArgs*)arg);
    thread_exit();
}

void attention_parallel_run(RunState* s, int n_heads, int head_size, 
                        int seq_len, int pos, int loff, int dim, int kv_dim, int kv_mul) {
    if (n_heads < NUM_THREADS) {
        // Serial fallback: Run directly in this thread
        AttentionArgs args;
        args.att = s->att; args.q = s->q; args.key_cache = s->key_cache;
        args.xb = s->xb; args.value_cache = s->value_cache;
        args.seq_len = seq_len; args.head_size = head_size; args.pos = pos;
        args.loff = loff; args.dim = dim; args.kv_dim = kv_dim; args.kv_mul = kv_mul;
        args.head_start = 0; args.head_end = n_heads;
        
        run_attention_range(&args);
        return;
    }
    
    int thread_ids[NUM_THREADS];
    AttentionArgs args[NUM_THREADS];
    
    int heads_per_thread = n_heads / NUM_THREADS;
    int extra_heads = n_heads % NUM_THREADS;
    int current_head = 0;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        int heads_for_this_thread = heads_per_thread + (t < extra_heads ? 1 : 0);
        
        args[t].att = s->att;
        args[t].q = s->q;
        args[t].key_cache = s->key_cache;
        args[t].xb = s->xb;
        args[t].value_cache = s->value_cache;
        args[t].seq_len = seq_len;
        args[t].head_size = head_size;
        args[t].pos = pos;
        args[t].loff = loff;
        args[t].dim = dim;
        args[t].kv_dim = kv_dim;
        args[t].kv_mul = kv_mul;
        args[t].head_start = current_head;
        args[t].head_end = current_head + heads_for_this_thread;
        
        if (heads_for_this_thread > 0) {
            thread_ids[t] = thread_create(attention_worker, (void*)&args[t]);
        } else {
            thread_ids[t] = -1;
        }
        current_head += heads_for_this_thread;
    }
    
    for (int t = 0; t < NUM_THREADS; t++) {
        if (thread_ids[t] >= 0) {
            thread_join(thread_ids[t]);
        }
    }
}

// ----------------------------------------------------------------------------
// Neural net blocks
// ----------------------------------------------------------------------------

void rmsnorm(float* o, float* x, float* weight, int size) {
    unsigned long long start = rdcycle();
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
    unsigned long long end = rdcycle();
    prof.total_activation += (end - start);
}

void softmax(float* x, int size) {
    unsigned long long start = rdcycle();
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
    unsigned long long end = rdcycle();
    prof.total_activation += (end - start);
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
        // Attention Section
        unsigned long long att_start = rdcycle();
        
        rmsnorm(s->xb, x, w->rms_att_weight + l*dim, dim);

        int loff = l * p->seq_len * kv_dim; 
        s->k = s->key_cache + loff + pos * kv_dim;
        s->v = s->value_cache + loff + pos * kv_dim;

        // QKV Matmuls - these are now PARALLEL via the matmul() wrapper
        matmul(s->q, s->xb, w->wq + l*dim*dim, dim, dim);
        matmul(s->k, s->xb, w->wk + l*dim*kv_dim, dim, kv_dim);
        matmul(s->v, s->xb, w->wv + l*dim*kv_dim, dim, kv_dim);

        // RoPE (Sequential - lightweight)
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

        // Multi-Head Attention - PARALLELIZED
        // Replaces the sequential for loop over heads
        attention_parallel_run(s, p->n_heads, head_size, p->seq_len, pos, loff, dim, kv_dim, kv_mul);

        // Output projection - PARALLEL via matmul()
        matmul(s->xb2, s->xb, w->wo + l*dim*dim, dim, dim);

        for (int i = 0; i < dim; i++) {
            x[i] += s->xb2[i];
        }
        
        unsigned long long att_end = rdcycle();
        prof.total_attention += (att_end - att_start);

        // FFN Section
        unsigned long long ffn_start = rdcycle();
        
        rmsnorm(s->xb, x, w->rms_ffn_weight + l*dim, dim);

        // FFN Matmuls - PARALLEL via matmul()
        matmul(s->hb, s->xb, w->w1 + l*dim*hidden_dim, dim, hidden_dim);
        matmul(s->hb2, s->xb, w->w3 + l*dim*hidden_dim, dim, hidden_dim);

        unsigned long long swiglu_start = rdcycle();
        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];
            val *= (1.0f / (1.0f + expf(-val)));
            val *= s->hb2[i];
            s->hb[i] = val;
        }
        unsigned long long swiglu_end = rdcycle();
        prof.total_activation += (swiglu_end - swiglu_start);

        // FFN Output Matmul - PARALLEL via matmul()
        matmul(s->xb, s->hb, w->w2 + l*dim*hidden_dim, hidden_dim, dim);

        for (int i = 0; i < dim; i++) {
            x[i] += s->xb[i];
        }
        
        unsigned long long ffn_end = rdcycle();
        prof.total_ffn += (ffn_end - ffn_start);
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
    
    char* ptr = buf;
    memcpy(&t->max_token_length, ptr, sizeof(int));
    ptr += sizeof(int);

    int len;
    for (int i = 0; i < vocab_size; i++) {
        memcpy(t->vocab_scores + i, ptr, sizeof(float));
        ptr += sizeof(float);
        memcpy(&len, ptr, sizeof(int));
        ptr += sizeof(int);
        t->vocab[i] = (char *)malloc(len + 1);
        memcpy(t->vocab[i], ptr, len);
        t->vocab[i][len] = '\0';
        ptr += len;
    }
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
    if (piece[0] == '<' && piece[1] == '0' && piece[2] == 'x' && 
        piece[5] == '>' && piece[6] == '\0') {
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
    unsigned long long start = rdcycle();
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
    unsigned long long end = rdcycle();
    prof.total_sampling += (end - start);
    return next;
}

unsigned long long get_time() {
    return rdcycle();
}

// ----------------------------------------------------------------------------
// Generation Loop
// ----------------------------------------------------------------------------

int generate(Transformer *transformer, Tokenizer *tokenizer, Sampler *sampler, char *prompt, int steps) {
    char *empty_prompt = "";
    if (prompt == 0) { prompt = empty_prompt; }

    prof.start_time = rdcycle();
    prof.first_token_flag = 0;
    prof.total_tokens_generated = 0;
    prof.prompt_tokens = 0;
    prof.total_matmul = 0;
    prof.total_attention = 0;
    prof.total_ffn = 0;
    prof.total_activation = 0;
    prof.total_sampling = 0;

    int num_prompt_tokens = 0;
    int* prompt_tokens = (int*)malloc((strlen(prompt)+3) * sizeof(int)); 
    encode(tokenizer, prompt, 1, 0, prompt_tokens, &num_prompt_tokens);
    if (num_prompt_tokens < 1) {
        fprintf(stderr, "something is wrong, expected at least 1 prompt token\n");
        exit(EXIT_FAILURE);
    }
    
    prof.prompt_tokens = num_prompt_tokens;

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
            if (prof.first_token_flag == 0) {
                prof.ttft_end = rdcycle();
                prof.first_token_flag = 1;
            }
        }
        pos++;

        if (next == 1) { break; }

        char* piece = decode(tokenizer, token, next);
        safe_printf(piece); 
        
        token = next;

        if (start == 0) { start = get_time(); }
    }
    printf("\n");

    prof.end_time = rdcycle();
    
    int tokens_generated = pos - num_prompt_tokens;
    prof.total_tokens_generated = tokens_generated;

    if (pos > 1) {
        unsigned long long end = get_time();
        unsigned long long cycles = end - start;
        printf("Tokens generated: %d\n", tokens_generated);
        printf("Total Cycles: %ld\n", (long)cycles);
    }

    free(prompt_tokens);
    return tokens_generated;
}

void print_benchmark_results(void) {
    unsigned long long total_cycles = prof.end_time - prof.start_time;
    unsigned long long ttft_cycles = prof.ttft_end - prof.start_time;
    unsigned long long gen_cycles = prof.end_time - prof.ttft_end;
    unsigned long long inference_sum = prof.total_matmul + prof.total_attention + prof.total_ffn + prof.total_sampling;
    if(inference_sum == 0) inference_sum = 1;

    printf("\n=== BENCHMARK RESULTS ===\n");
    printf("Prompt Tokens: %d\n", prof.prompt_tokens);
    printf("Output Tokens: %d\n", prof.total_tokens_generated);
    
    printf("\nPRIMARY METRICS:\n");
    printf("TTFT: %ld cycles\n", (long)ttft_cycles);
    printf("End-to-End: %ld cycles\n", (long)total_cycles);
    
    if (prof.total_tokens_generated > 0 && gen_cycles > 0) {
        unsigned long long cycles_per_token = gen_cycles / prof.total_tokens_generated;
        printf("TPS (cycles/tok): %ld\n", (long)cycles_per_token);
    } else {
        printf("TPS (cycles/tok): N/A\n");
    }

    printf("\nHOTSPOT BREAKDOWN:\n");
    printf("matmul: %ld%%\n", (long)((prof.total_matmul * 100) / inference_sum));
    printf("Activations: %ld%%\n", (long)((prof.total_activation * 100) / inference_sum));
    printf("Attention: %ld%%\n", (long)((prof.total_attention * 100) / inference_sum));
    printf("FFN: %ld%%\n", (long)((prof.total_ffn * 100) / inference_sum));
    printf("Sampling: %ld%%\n", (long)((prof.total_sampling * 100) / inference_sum));
}

int main(int argc, char *argv[]) {
    float temperature = 0.0f;   
    float topp = 0.9f;          
    int output_tokens = 100;     
    char *prompt = "The old lighthouse stood on the rocky cliff overlooking the vast ocean. For decades, it had guided ships safely through treacherous waters during stormy nights. The keeper, an elderly man named Thomas, climbed the spiral staircase every evening to light the beacon. He knew every crack in the walls and every creak of the wooden steps.";
    unsigned long long rng_seed = 12345;

    if (argc > 1) {
        prompt = argv[1];
    }
    
    Transformer transformer;
    build_transformer(&transformer);

    Tokenizer tokenizer;
    build_tokenizer(&tokenizer, transformer.config.vocab_size);

    int num_prompt_tokens = 0;
    int* prompt_tokens = (int*)malloc((strlen(prompt)+3) * sizeof(int));
    encode(&tokenizer, prompt, 1, 0, prompt_tokens, &num_prompt_tokens);
    
    int steps = num_prompt_tokens + output_tokens;
    
    if (steps > transformer.config.seq_len) {
        steps = transformer.config.seq_len;
    }
    
    printf("Prompt tokens: %d, Output tokens: %d, Total steps: %d\n", 
           num_prompt_tokens, output_tokens, steps);
    
    free(prompt_tokens);

    Sampler sampler;
    build_sampler(&sampler, transformer.config.vocab_size, temperature, topp, rng_seed);

    generate(&transformer, &tokenizer, &sampler, prompt, steps);

    free_sampler(&sampler);
    free_tokenizer(&tokenizer);
    free_transformer(&transformer);
    print_benchmark_results();
    
    exit(0);
}