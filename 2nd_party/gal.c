// Copyright © 2025 kala_telo <kala_telo@proton.me>
// SPDX-License-Identifier: MIT

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *string;
    int length;
} String;

#define S(s) (String){.string = (s), .length = sizeof(s)-1}
#define PS(s) (s).length, (s).string

typedef enum {
    T_DEFAULT,
    // MEMORY REFERENCE INSTRUCTIONS
    T_MEM_REF,
} InstKind;

typedef struct {
    String name;
    int16_t opcode;
    InstKind kind;
} Mnemonic;

Mnemonic mnemonics[] = {
    {S("ION"), 06001},    {S("IOF"), 06002},   {S("RSF"), 06011},   {S("RRB"), 06012},    {S("RFC"), 06014},   {S("PSF"), 06021},
    {S("PCF"), 06022},    {S("PPC"), 06024},   {S("PLS"), 06026},   {S("KSF"), 06031},    {S("KCC"), 06032},   {S("KRS"), 06034},
    {S("KRB"), 06036},    {S("NOP"), 07000},   {S("IAC"), 07001},   {S("RAL"), 07004},    {S("RTL"), 07006},   {S("RAR"), 07010},
    {S("RTR"), 07012},    {S("CML"), 07020},   {S("CMA"), 07040},   {S("CLL"), 07100},    {S("CLA"), 07200},   {S("HLT"), 07402},
    {S("OSR"), 07404},    {S("SKP"), 07410},   {S("SNL"), 07420},   {S("SZL"), 07430},    {S("SZA"), 07440},   {S("SNA"), 07450},
    {S("SMA"), 07500},    {S("SPA"), 07510},   {S("CIA"), 07041},   {S("LAS"), 07604},    {S("STL"), 07120},   {S("GLK"), 07204},
    {S("STA"), 07240},    {S("MMMM"), 06757},  {S("MMSF"), 06761},  {S("MMMF"), 06756},   {S("MMCF"), 06772},  {S("MMML"), 06766},
    {S("MMSC"), 06771},   {S("MMLS"), 06751},  {S("MMRS"), 06774},  {S("MMLM"), 06752},   {S("MMCC"), 06762},  {S("MMLF"), 06754},
    {S("MMLC"), 06764},   {S("DTRA"), 06761},  {S("DTSF"), 06771},  {S("DTCA"), 06762},   {S("DTRB"), 06772},  {S("DTXA"), 06764},
    {S("DTLB"), 06774},   {S("SMP"), 06101},   {S("CMP"), 06104},   {S("CAM"), 06101},    {S("SZO"), 06114},   {S("LMQ"), 06102},
    {S("DIV"), 06121},    {S("LAR"), 06104},   {S("RDM"), 06122},   {S("MUL"), 06111},    {S("SAF"), 06124},   {S("RDA"), 06112},
    {S("MUY"), 07405},    {S("ASR"), 07415},   {S("DVI"), 07407},   {S("LSR"), 07417},    {S("NMI"), 07411},   {S("MQL"), 07421},
    {S("SHL"), 07413},    {S("SCA"), 07441},   {S("MQA"), 07501},   {S("CAM"), 07621},    {S("CDF"), 06201},   {S("RIF"), 06224},
    {S("CIF"), 06202},    {S("RMF"), 06244},   {S("RDF"), 06214},   {S("RIB"), 06234},    {S("SPL"), 06102},   {S("ADC"), 06004},
    {S("ADSF"), 06531},   {S("ADCC"), 06541},  {S("ADCV"), 06532},  {S("ADSC"), 06542},   {S("ADRB"), 06534},  {S("ADIC"), 06544},
    {S("DCX"), 06051},    {S("DYL"), 06063},   {S("DXL"), 06053},   {S("DIX"), 06054},    {S("DCY"), 06061},   {S("DIY"), 06064},
    {S("DXS"), 06057},    {S("DYS"), 06067},   {S("DLB"), 06074},   {S("DSF"), 06071},    {S("DCF"), 06072},   {S("PLSF"), 06501},
    {S("PLCF"), 06502},   {S("PLPU"), 06504},  {S("PLPR"), 06511},  {S("PLPU"), 06512},   {S("PLDD"), 06514},  {S("PLPL"), 06521},
    {S("PLUD"), 06522},   {S("PLPD"), 06524},  {S("RCSF"), 06631},  {S("RCSP"), 06671},   {S("RCRA"), 06632},  {S("RCSE"), 06671},
    {S("RCRB"), 06634},   {S("RCRD"), 06674},  {S("CRSF"), 06632},  {S("CERS"), 06634},   {S("CRRB"), 06671},  {S("CRSA"), 06672},
    {S("CRSB"), 06674},   {S("CPSF"), 06631},  {S("CPSE"), 06642},  {S("CPLB"), 06644},   {S("CPCF"), 06641},  {S("LCF"), 06652},
    {S("LPR"), 06655},    {S("LSF"), 06661},   {S("LCB"), 06662},   {S("LLB"), 06664},    {S("DRCR"), 06603},  {S("DRCF"), 06611},
    {S("DRTS"), 06615},   {S("DRSC"), 06622},  {S("DRCW"), 06605},  {S("DREF"), 06612},   {S("DRSE"), 06621},  {S("DRCN"), 06624},
    {S("MSCR"), 06701},   {S("MTS"), 06706},   {S("MNC"), 06712},   {S("MSWF"), 06721},   {S("MCWF"), 06722},  {S("MIWF"), 06722},
    {S("MDEF"), 06732},   {S("MEEF"), 06732},  {S("MTRS"), 06734},  {S("MRWC"), 06742},   {S("MCD"), 06702},   {S("MSUR"), 06711},
    {S("MTC"), 06716},    {S("MDWF"), 06722},  {S("MEWF"), 06722},  {S("MSEF"), 06731},   {S("MCED"), 06732},  {S("MIEF"), 06732},
    {S("MCC"), 06741},    {S("MRCA"), 06744},  {S("MCA"), 06745},   {S("TSRD"), 06715},   {S("TSWR"), 06716},  {S("TSSR"), 06722},
    {S("TWRT"), 06731},   {S("TSRS"), 06734},  {S("TIFM"), 06707},  {S("TSDF"), 06721},   {S("TSST"), 06724},  {S("TCPI"), 06732},
    {S("TTINCR"), 06401}, {S("TTI"), 06402},   {S("TTO"), 06404},   {S("TTCL"), 06411},   {S("TTSL"), 06412},  {S("TTRL"), 06414},
    {S("TTSKP"), 06421},  {S("TTXON"), 06422}, {S("TTXOF"), 06424}, {S("FEXT"), 00000},   {S("FADD"), 01000},  {S("FSUB"), 02000},
    {S("FMPY"), 03000},   {S("FDIV"), 04000},  {S("FGET"), 05000},  {S("FPUT"), 06000},   {S("FNOR"), 07000},  {S("TSF"), 06041},
    {S("TCF"), 06042},    {S("TLS"), 06046},   {S("TPC"), 06044},   {S("BSW"),  07002},

    {S("AND"), 00000, T_MEM_REF},
    {S("TAD"), 01000, T_MEM_REF},
    {S("ISZ"), 02000, T_MEM_REF},
    {S("DCA"), 03000, T_MEM_REF},
    {S("JMS"), 04000, T_MEM_REF},
    {S("JMP"), 05000, T_MEM_REF},
};

#define ARRLEN(xs) (sizeof(xs)/sizeof(*(xs)))

#define UNREACHABLE()                                                          \
    do {                                                                       \
        fprintf(stderr, "%s:%d: UNREACHABLE -- It shouldn've not happen...\n", \
                __FILE__, __LINE__);                                           \
        abort();                                                               \
    } while (0)
#define TODO()                                                                 \
    do {                                                                       \
        fprintf(stderr, "%s:%d: TODO\n", __FILE__, __LINE__);                  \
        abort();                                                               \
    } while (0)

typedef enum {
    B_OCT,
    B_BIN,
    B_DEC,
    B_HEX,
} Base;

static inline String string_strip(String s) {
    while (*s.string == ' ' && s.length > 0) {
        s.length--;
        s.string++;
    }
    while (s.string[s.length-1] == ' ' && s.length > 0) {
        s.length--;
    }
    return s;
}

static inline int s_atoi(String s, Base base) {
    int b;
    switch (base) {
    case B_BIN:
        b = 2;
        break;
    case B_DEC:
        b = 10;
        break;
    case B_OCT:
        b = 8;
        break;
    case B_HEX:
        TODO();
        b = 16;
        break;
    }
    int64_t result = 0;
    for (int len = 0; len < s.length; len++) {
        result *= b;
        result += s.string[len] - '0';
    }
    return result;
}

static inline bool string_eq(String s1, String s2) {
    if (s1.length != s2.length) {
        return false;
    }
    return memcmp(s1.string, s2.string, s1.length) == 0;
}

#define da_append(xs, x)                                                       \
    do {                                                                       \
        if ((xs).len + 1 > (xs).cap) {                                         \
            if ((xs).cap != 0) {                                               \
                (xs).cap *= 2;                                                 \
            } else {                                                           \
                (xs).cap = 4;                                                  \
            }                                                                  \
            (xs).data = realloc((xs).data, sizeof(*(xs).data) * (xs).cap);     \
            assert((xs).data != NULL);                                         \
        }                                                                      \
        (xs).data[(xs).len++] = (x);                                           \
    } while (0)

static inline bool find_mnem(Mnemonic *out, String name) {
    for (size_t i = 0; i < ARRLEN(mnemonics); i++) {
        if (string_eq(mnemonics[i].name, name)) {
            if (out) *out = mnemonics[i];
            return true;
        }
    }
    return false;
}

#define PLOC(x) (x).file, (x).line+1, (x).col+1
typedef struct {
    uint16_t line;
    uint8_t col;
    char *file;
} Loc;

typedef enum {
    LEX_END,
    LEX_STAR,
    LEX_NAME,
    LEX_INST,
    LEX_INT,
    LEX_PLUS,
    LEX_EQ,
    LEX_COMMA,
    LEX_DOT,
    LEX_MINUS,
    LEX_SEMICOLON,
    LEX_CHARACTER,
    LEX_NEWLINE,
} TokenKind;

const char *const lex_names[] = {
    [LEX_END] = "<EOF>",
    [LEX_STAR] = "`*`",
    [LEX_NAME] = "<name>",
    [LEX_INST] = "<INST>",
    [LEX_INT] = "<int>",
    [LEX_PLUS] = "`+`",
    [LEX_EQ] = "`=`",
    [LEX_COMMA] = "`,`",
    [LEX_DOT] = "`.`",
    [LEX_MINUS] = "`-`",
    [LEX_SEMICOLON] = "`,`",
    [LEX_CHARACTER] = "\"<character>",
    [LEX_NEWLINE] = "<newline>",
};

typedef struct {
    TokenKind  kind;
    String str;
    Loc loc;
} Token;

typedef struct {
    int16_t addr, opcode, arg;
} Inst;

typedef struct {
    String name; int16_t value;
} NameEntry;

typedef struct {
    char *code;
    size_t len;
    Loc loc;
} Lexer;

typedef struct {
    Token cause;
    int16_t addr;
    Lexer lexer;
    Base base;
} BackpatchEntry;

// global variables
struct {
    NameEntry *data;
    size_t len, cap;
} names = { 0 };

struct {
    BackpatchEntry *data;
    size_t len, cap;
} backpatch = { 0 };

uint16_t ram[4096] = { 0 };
// ----

static inline bool find_name(int16_t *out, String name) {
    if (names.len < 1) return false;
    size_t i = names.len;
    while (i-->0) {
        if (string_eq(names.data[i].name, name)) {
            *out = names.data[i].value;
            return true;
        }
    }
    return false;
}

bool eat_char(Lexer *lex) {
    if (lex->len < 1) {
        return false;
    }
    if (*lex->code++ == '\n') {
        lex->loc.line++;
        lex->loc.col = 0;
    } else {
        lex->loc.col++;
    }
    lex->len--;
    return true;
}

bool is_kind_binop(TokenKind k) {
    switch (k) {
    case LEX_PLUS:
        return true;
    case LEX_MINUS:
        return true;
    default:
        return false;
    }
}

#define expect_any(token, ...) _expect_any(token, ARRLEN(((TokenKind[]){__VA_ARGS__})), ((TokenKind[]){__VA_ARGS__}))
Token _expect_any(Token t, size_t count, TokenKind *ks) {
    for (size_t i = 0; i < count; i++) {
        if (t.kind == ks[i]) {
            return t;
        }
    }
    fprintf(stderr, "%s:%d:%d Expected any of: ", PLOC(t.loc));
    for (size_t i = 0; i < count; i++) {
        fprintf(stderr, "%s, ", lex_names[ks[i]]);
    }
    fprintf(stderr, "but got %s\n", lex_names[t.kind]);
    exit(1);
    return (Token){0};
}

Token expect(Token t, TokenKind k) {
    if (t.kind != k) {
        fprintf(stderr, "%s:%d:%d Expected %s but got %s (%.*s)\n", PLOC(t.loc),
                lex_names[k], lex_names[t.kind], PS(t.str));
        exit(1);
    }
    return t;
}

Token next_token(Lexer *lex) {
    while ((*lex->code == ' ' || *lex->code == '\t' || *lex->code == '\r') && lex->len > 0) {
        eat_char(lex);
    }
    if (lex->len == 0) {
        return (Token){
            .kind = LEX_END, .str = (String){lex->code, 0}, .loc = lex->loc};
    }
    switch (*lex->code) {
    case '/':
        while (*lex->code != '\n')
            eat_char(lex);
        return next_token(lex);
    case '*':
        eat_char(lex);
        return (Token){.kind = LEX_STAR,
                       .str = (String){lex->code-1, 1},
                       .loc = lex->loc};
    case '=':
        eat_char(lex);
        return (Token){.kind = LEX_EQ,
                       .str = (String){lex->code-1, 1},
                       .loc = lex->loc};
    case ',':
        eat_char(lex);
        return (Token){.kind = LEX_COMMA,
                       .str = (String){lex->code-1, 1},
                       .loc = lex->loc};
    case '.':
        eat_char(lex);
        return (Token){.kind = LEX_DOT,
                       .str = (String){lex->code-1, 1},
                       .loc = lex->loc};
    case '-':
        eat_char(lex);
        return (Token){.kind = LEX_MINUS,
                       .str = (String){lex->code-1, 1},
                       .loc = lex->loc};
    case '+':
        eat_char(lex);
        return (Token){.kind = LEX_PLUS,
                       .str = (String){lex->code-1, 1},
                       .loc = lex->loc};
    case ';':
        eat_char(lex);
        return (Token){.kind = LEX_SEMICOLON,
                       .str = (String){lex->code-1, 1},
                       .loc = lex->loc};
    case '"':
        eat_char(lex); eat_char(lex);
        return (Token){.kind = LEX_CHARACTER,
                       .str = (String){lex->code-1, 1},
                       .loc = lex->loc};
    case '\n':
        eat_char(lex);
        return (Token){.kind = LEX_NEWLINE,
                       .str = (String){lex->code, 1},
                       .loc = lex->loc};
    case '$':
        return (Token){.kind = LEX_END,
                       .str = (String){lex->code, 1},
                       .loc = lex->loc};
    default:
        if (isdigit(*lex->code)) {
            String word = {
                .string = lex->code,
                .length = 1,
            };
            if (!eat_char(lex))
                goto fail;
            while (isalnum(*lex->code)) {
                word.length++;
                if (!eat_char(lex))
                    goto fail;
            }
            return (Token){.kind = LEX_INT, .str = word, .loc = lex->loc};
        } else if (isalnum(*lex->code)) {
            String word = {
                .string = lex->code,
                .length = 1,
            };
            if (!eat_char(lex))
                goto fail;
            while (isalnum(*lex->code)) {
                word.length++;
                if (!eat_char(lex))
                    goto fail;
            }
            if (find_mnem(NULL, word))
                return (Token){.kind = LEX_INST, .str = word, .loc = lex->loc};
            else
                return (Token){.kind = LEX_NAME, .str = word, .loc = lex->loc};
        }
    }
fail:
    fprintf(stderr, "%s:%d:%d Unexpected value '%c' (%d)\n", lex->loc.file,
            lex->loc.line + 1, lex->loc.col, *lex->code, *lex->code);
    TODO();
    return (Token) { 0 };
}

Token peek_token(Lexer *lex) {
    Lexer copy = *lex;
    return next_token(&copy);
}

Token peek_token_n(Lexer *lex, size_t n) {
    Lexer copy = *lex;
    assert(n > 0);
    for (size_t i = 0; i < n-1; i++)
        next_token(&copy);
    return next_token(&copy);
}

int16_t parse_var_or_int(Lexer *lex, Base base, int16_t addr) {
    Token t = expect_any(next_token(lex), LEX_NAME, LEX_INT, LEX_DOT);
    switch (t.kind) {
    case LEX_NAME: {
        int16_t v;
        if (find_name(&v, t.str)) {
            return v;
        } else {
            return -1;
        }
    } break;
    case LEX_INT:
        return s_atoi(t.str, base);
    case LEX_DOT:
        return addr;
    default:
        UNREACHABLE();
        break;
    }
    UNREACHABLE();
    return -1;
}

int16_t parse_expr(Lexer *lex, Base base, int16_t addr, Token *bp_cause) {
    Token potential_bp_cause = peek_token(lex);
    int16_t v = parse_var_or_int(lex, base, addr);
    if (v < 0)
        *bp_cause = potential_bp_cause;
    while (is_kind_binop(peek_token(lex).kind)) {
        TokenKind op = next_token(lex).kind;
        int16_t dv = parse_var_or_int(lex, base, addr);
        switch (op) {
        case LEX_PLUS:
            if (v >= 0) v += dv;
            break;
        case LEX_MINUS:
            if (v >= 0) v -= dv;
            break;
        default:
            TODO();
        }
    }
    return v;
}

int16_t assemble_mnemonic(Lexer *lex, Base base, int16_t addr, Token *bp_cause) {
    Mnemonic mnem;
    Token t = expect(next_token(lex), LEX_INST);
    find_mnem(&mnem, t.str);
    switch (mnem.kind) {
    case T_MEM_REF: {
        int16_t I = 0, Z = 0;
        if (string_eq(peek_token(lex).str, S("I"))) {
            next_token(lex);
            I = 1<<8;
        }
        char *expr_start = lex->code;
        int16_t v = parse_expr(lex, base, addr, bp_cause);
        if (v >= 0200) {
            Z = 1<<7;
        }
        if (v >= 0) {
            if (v/128 != addr/128 && Z != 0 && I == 0) {
                String name = string_strip((String){expr_start, (int)(lex->code - expr_start)});
                fprintf(stderr,
                        "%s:%d:%d: `%.*s` (%o) is not on the same page as "
                        "current address (%o)\n",
                        PLOC(t.loc), PS(name),
                        v, addr);
                exit(1);
            }
            return mnem.opcode | I | Z | (v & 0x7F);
        } else {
            return -1;
        }
    } break;
    case T_DEFAULT:
        return mnem.opcode;
    }
    return -1;
}

void assemble_once(Lexer *lex, Base *base, int16_t *addr) {
    switch (peek_token(lex).kind) {
    case LEX_STAR: {
        BackpatchEntry potential_bp = (BackpatchEntry){
            .addr = *addr,
            .base = *base,
            .lexer = *lex,
        };
        next_token(lex);
        int16_t next_addr = parse_expr(lex, *base, *addr, &potential_bp.cause);
        if (next_addr < 0) TODO();
        if (next_addr >= ARRLEN(ram)) TODO();
        *addr = next_addr;
    } break;
    case LEX_INST: {
        BackpatchEntry potential_bp = (BackpatchEntry){
            .cause = peek_token(lex),
            .addr = *addr,
            .base = *base,
            .lexer = *lex,
        };
        if (peek_token_n(lex, 2).kind == LEX_EQ) {
            Token t = next_token(lex);
            next_token(lex);
            Mnemonic mnem;
            if (!find_mnem(&mnem, t.str)) UNREACHABLE();
            int16_t n = parse_expr(lex, *base, *addr, &potential_bp.cause);
            if (n < 0) {
                da_append(backpatch, potential_bp);
                break;
            }
            if (n != mnem.opcode) {
                fprintf(stderr,
                        "%s:%d:%d Redefining mnemonics is not supported! "
                        "(%.*s)\n",
                        PLOC(t.loc), PS(t.str));
                exit(1);
            }
            break;
        }
        int16_t r = 0;
        while (peek_token(lex).kind != LEX_NEWLINE && peek_token(lex).kind != LEX_END) {
            Token t = expect(peek_token(lex), LEX_INST);
            potential_bp.cause = t;
            int16_t o = assemble_mnemonic(lex, *base, *addr, &potential_bp.cause);
            if (o >= 0) r |= o;
            else da_append(backpatch, potential_bp);
        }
        ram[(*addr)++] = r;
    } break;
    case LEX_NAME: {
        if (string_eq(peek_token(lex).str, S("DECIMAL"))) {
            next_token(lex);
            *base = B_DEC;
            break;
        }
        if (string_eq(peek_token(lex).str, S("OCTAL"))) {
            next_token(lex);
            *base = B_OCT;
            break;
        }
        if (string_eq(peek_token(lex).str, S("PAGE"))) {
            next_token(lex);
            int16_t old_addr = *addr;
            if (peek_token(lex).kind == LEX_INT) {
                int16_t n = s_atoi(next_token(lex).str, *base);
                *addr = (128*n)%(36*128);
            } else {
                int16_t round_addr = (*addr)/128*128;
                *addr = (round_addr+128)%(36*128);
            }
            break;
        }
        BackpatchEntry potential_bp = (BackpatchEntry){
            .addr = *addr,
            .base = *base,
            .lexer = *lex,
        };
        Token t = next_token(lex);
        switch (peek_token(lex).kind) {
        case LEX_EQ: {
            int16_t v;
            next_token(lex);
            Token ve = expect_any(peek_token(lex), LEX_NAME, LEX_INT, LEX_INST);
            switch (ve.kind) {
            case LEX_INST: 
                v = assemble_mnemonic(lex, *base, *addr, &potential_bp.cause);
                break;
            case LEX_INT:
            case LEX_NAME:
                v = parse_expr(lex, *base, *addr, &potential_bp.cause);
                break;
            default: UNREACHABLE();
            }
            if (v < 0) da_append(backpatch, potential_bp);
            else da_append(names, ((NameEntry){t.str, v}));
        } break;
        case LEX_COMMA:
            da_append(names, ((NameEntry){t.str, *addr}));
            next_token(lex);
            break;
        default: {
            *lex = potential_bp.lexer;
            *addr = potential_bp.addr;
            *base = potential_bp.base;
            potential_bp.cause = t;
            int16_t v = parse_expr(lex, *base, *addr, &potential_bp.cause);
            if (v < 0) {
                da_append(backpatch, potential_bp);
                (*addr)++;
            } else {
                ram[(*addr)++] = v;
            }
        } break;
        }
    } break;
    case LEX_INT:
        ram[(*addr)++] = s_atoi(next_token(lex).str, *base);
        break;
    case LEX_EQ:
        fprintf(stderr, "%s:%d:%d\n", PLOC(peek_token(lex).loc));
        TODO();
        break;
    case LEX_COMMA:
        fprintf(stderr, "%s:%d:%d\n", PLOC(peek_token(lex).loc));
        TODO();
        break;
    case LEX_DOT: {
        BackpatchEntry potential_bp = (BackpatchEntry){
            .addr = *addr,
            .base = *base,
            .lexer = *lex,
        };
        int16_t v = parse_expr(lex, *base, *addr, &potential_bp.cause);
        if (v < 0) {
            da_append(backpatch, potential_bp);
            (*addr)++;
        } else {
            ram[(*addr)++] = v;
        }
    } break;
    case LEX_MINUS: {
        BackpatchEntry potential_bp = (BackpatchEntry){
            .addr = *addr,
            .base = *base,
            .lexer = *lex,
        };
        next_token(lex);
        int16_t v = s_atoi(expect(next_token(lex), LEX_INT).str, *base);
        int16_t dv = 0;
        TokenKind kind;
        if (is_kind_binop(kind = peek_token(lex).kind))
            dv = parse_expr(lex, *base, *addr, &potential_bp.cause);
        if (dv < 0) da_append(backpatch, potential_bp);
        else ram[(*addr)++] = ((1<<12)-v + dv * (kind == LEX_MINUS ? -1 : 1))%(1<<12);
    } break;
    case LEX_PLUS:
        TODO();
        break;
    case LEX_NEWLINE:
        next_token(lex);
        break;
    case LEX_SEMICOLON:
        TODO();
        break;
    case LEX_CHARACTER:
        ram[(*addr)++] = next_token(lex).str.string[0];
        break;
    case LEX_END:
        break;
    }
}

void assemble(Lexer *lex) {
    Base base = B_OCT;
    int16_t addr = 0200;

    while (peek_token(lex).kind != LEX_END) {
        assemble_once(lex, &base, &addr);
    }
    size_t bp_count = backpatch.len;
    for (size_t i = 0; i < bp_count; i++) {
        BackpatchEntry bp = backpatch.data[i];
        assemble_once(&bp.lexer, &bp.base, &bp.addr);
    }
    // if new requests for backpatching were introduced during backpatching,
    // then those are undefined variables
    for (size_t i = bp_count; i < backpatch.len; i++) { 
        BackpatchEntry bp = backpatch.data[i];
        printf("%s:%d:%d: Error: Undefined name `%.*s`\n", PLOC(bp.cause.loc), PS(bp.cause.str));
    }
    if (backpatch.len > bp_count) {
        exit(1);
    }
    backpatch.len = 0;
}

void export_dec_obj(FILE *out) {
#define O(x) fputc((checksum += (x), (x)), out)
    for (int i = 1; i < 240; i++) {
        fputc(0200, out);
    }
    uint16_t checksum = 0;
    O(0x10);
    O(0x00);
    for (size_t i = 1; i < ARRLEN(ram); i++) {
        O((ram[i] >> 6) & 0x3F);
        O(ram[i] & 0x3F);
    }
    fputc((checksum >> 6) & 0x3F, out);
    fputc(checksum & 0x3F, out);
    fputc(0200, out);
#undef O
}

char *next_arg(int* argc, char ***argv, char* error) {
    if (*argc == 0) {
        if (error != NULL) {
            fprintf(stderr, "%s\n", error);
        }
        exit(1);
    }
    char *result = **argv;
    (*argc)--;
    (*argv)++;
    return result;
}

int main(int argc, char *argv[]) {
    char *program_name = next_arg(&argc, &argv, NULL),
         *input_file   = NULL,
         *output_file  = NULL;
    while (argc) {
        char *arg = next_arg(&argc, &argv, NULL);
        if (strcmp(arg, "-o") == 0) {
            output_file = next_arg(&argc, &argv, "Argument `-o` expects output filename next");
        } else if (strcmp(arg, "-static") == 0) { // just compatibility with GAS
        } else {
            if (!input_file) {
                input_file = arg;
            } else {
                fprintf(stderr, "GAL doesn't accept more than 1 input file: %s, %s provided.\n", input_file, arg);
                return 1;
            }
        }
    }
    if (!input_file) {
        fprintf(stderr, "No input file was provided.\n");
        return 1;
    }
    if (!output_file) {
        fprintf(stderr, "No output file was provided.\n");
        return 1;
    }
    FILE *f = fopen(input_file, "r");
    if (f == NULL) {
        fprintf(stderr, "Couldn't open %s\n", input_file);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    fseek(f, 0, SEEK_SET);
    String str = {0};
    str.string = malloc(size * sizeof(char));
    str.length = size;

    size = fread(str.string, 1, size, f);
    fclose(f);

    // -1 for \0, though it still must be in memory for
    // lexer to not read into unallocated memory
    Lexer lex = (Lexer){
        .len = size - 1,
        .code = str.string,
        .loc = (Loc){0, 0, input_file},
    };
    assemble(&lex);

    f = fopen(output_file, "wb");
    if (f == NULL) {
        fprintf(stderr, "Couldn't open `%s`\n", output_file);
        return 1;
    }
    export_dec_obj(f);
    fclose(f);
}
