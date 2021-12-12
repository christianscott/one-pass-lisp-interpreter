#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(cond, ...)                                                                                              \
    if (!cond)                                                                                                         \
    {                                                                                                                  \
        fprintf(stderr, __VA_ARGS__);                                                                                  \
        exit(1);                                                                                                       \
    }

#define UNREACHABLE(...)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        fprintf(stderr, "unreachable: " __VA_ARGS__);                                                                  \
        exit(1);                                                                                                       \
    } while (0)

bool is_num(char c)
{
    return '0' <= c && c <= '9';
}

bool is_alpha(char c)
{
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

bool is_alphanumeric(char c)
{
    return is_alpha(c) || is_num(c);
}

enum rt_value_kind
{
    rt_boolean,
    rt_number,
    rt_nil,
};

// runtime value
struct rt_value
{
    enum rt_value_kind kind;
    union {
        double num;
        bool boolean;
    };
};

void print_rt_value(struct rt_value val)
{
    switch (val.kind)
    {
    case rt_number:
        fprintf(stderr, "%f", val.num);
        break;
    case rt_boolean:
        fprintf(stderr, "%s", val.boolean ? "true" : "false");
        break;
    case rt_nil:
        fprintf(stderr, "nil");
        break;
    }
}

struct hm_entry
{
    bool in_use;
    char *key;
    struct rt_value val;
};

struct hm
{
    size_t cap;
    size_t size;
    struct hm_entry *entries;
};

void hm_init(struct hm *hm)
{
    hm->cap = 16;
    hm->size = 0;
    hm->entries = calloc(hm->cap, sizeof(struct hm_entry));
}

void hm_ensure(struct hm *hm, size_t min_cap)
{
    if (hm->cap >= min_cap)
    {
        return;
    }

    size_t prev_cap = hm->cap;
    hm->cap = prev_cap * 2;
    hm->entries = realloc(hm->entries, sizeof(struct hm_entry) * hm->cap);
    for (size_t i = prev_cap; i < hm->cap; i++)
    {
        hm->entries[i] = (const struct hm_entry){0};
    }
}

void hm_add(struct hm *hm, char *key, struct rt_value val)
{
    hm_ensure(hm, ++hm->size);

    for (size_t i = 0; i < hm->cap; i++)
    {
        if (!hm->entries[i].in_use)
        {
            struct hm_entry entry = {.in_use = true, .key = key, .val = val};
            hm->entries[i] = entry;
            return;
        }

        if (strcmp(hm->entries[i].key, key) == 0)
        {
            hm->entries[i].val = val;
            return;
        }
    }

    UNREACHABLE("could not insert key '%s' into hashmap\n", key);
}

bool hm_get(struct hm *hm, char *key, struct rt_value *result)
{
    for (size_t i = 0; i < hm->cap; i++)
    {
        struct hm_entry entry = hm->entries[i];
        if (!entry.in_use)
        {
            continue;
        }

        if (strcmp(entry.key, key) == 0)
        {
            *result = entry.val;
            return true;
        }
    }

    return false;
}

bool hm_has(struct hm *hm, char *key)
{
    struct rt_value unused;
    return hm_get(hm, key, &unused);
}

struct scope
{
    struct scope *parent;
    struct hm bindings;
};

void scope_init(struct scope *scope, struct scope *parent)
{
    scope->parent = parent;
    hm_init(&scope->bindings);
}

bool scope_get_value(struct scope *s, char *name, struct rt_value *val)
{
    if (hm_get(&s->bindings, name, val))
    {
        return true;
    }

    if (s->parent != NULL)
    {
        return scope_get_value(s->parent, name, val);
    }

    return false;
}

enum op
{
    op_add,
    op_mult,
    op_div,
};

char *expr;

void skip_whitespace()
{
    while (*expr == ' ')
    {
        expr++;
    }
}

struct rt_value evaluate_expr(struct scope *s);

double evaluate_n_ary_op(struct scope *s, enum op op)
{
    double res;
    switch (op)
    {
    case op_add:
        res = 0;
        break;
    case op_mult:
        res = 1;
        break;
    default:
        UNREACHABLE("tried to init res with unknown n-ary op '%d'\n", op);
    }

    while (*expr != ')')
    {
        skip_whitespace();
        struct rt_value v = evaluate_expr(s);
        EXPECT((v.kind == rt_number), "expected a number\n");
        double num = v.num;
        switch (op)
        {
        case op_mult:
            res = res * num;
            break;
        case op_add:
            res = res + num;
            break;
        default:
            UNREACHABLE("tried to apply unknown n-ary op '%d'\n", op);
        }
    }

    EXPECT((*(expr) == ')'), "expected ')': %s\n", expr);
    expr++;

    return res;
}

bool is_at_end()
{
    return (*expr == '\0') ? 1 : 0;
}

double evaluate_binary_op(struct scope *s, enum op op)
{
    EXPECT((!is_at_end() && *expr != ')'), "not enough arguments for binary op\n");
    struct rt_value a = evaluate_expr(s);
    EXPECT((a.kind == rt_number), "expected a number\n");
    double n = a.num;

    EXPECT((!is_at_end() && *expr != ')'), "not enough arguments for binary op\n");
    struct rt_value b = evaluate_expr(s);
    EXPECT((a.kind == rt_number), "expected a number\n");
    double m = b.num;

    EXPECT((*expr == ')'), "too many arguments for binary op\n");

    switch (op)
    {
    case op_div:
        return n / m;
    default:
        UNREACHABLE("not a binary op: '%d'\n", op);
    }
}

struct rt_value evaluate_op(struct scope *s, enum op op)
{
    double num;
    switch (op)
    {
    case op_add:
    case op_mult:
        num = evaluate_n_ary_op(s, op);
        break;
    case op_div:
        num = evaluate_binary_op(s, op);
        break;
    default:
        UNREACHABLE("tried to apply unknown op '%d'\n", op);
    }
    return (struct rt_value){.kind = rt_number, .num = num};
}

struct rt_value evaluate_let(struct scope *parent)
{
    struct scope s;
    scope_init(&s, parent);

    while (true)
    {
        skip_whitespace();

        if (!is_alpha(*expr))
        {
            break;
        }

        char *lookahead = expr + 1;
        while (is_alphanumeric(*lookahead))
        {
            lookahead++;
        }

        if (*lookahead == ')')
        {
            // this must be the terminal expr. throw away the work
            break;
        }

        // otherwise, this is a new identifier
        size_t len = lookahead - expr;
        char *name = malloc(sizeof(char) * (len + 1));
        strncpy(name, expr, len);
        name[len] = '\0';
        expr = lookahead;

        struct rt_value val = evaluate_expr(&s);
        hm_add(&s.bindings, name, val);
    }

    struct rt_value ret = evaluate_expr(&s);

    EXPECT((*(expr) == ')'), "expected ')': %s\n", expr);
    expr++;

    return ret;
}

struct rt_value evaluate_eq(struct scope *parent)
{
    struct scope s;
    scope_init(&s, parent);

    struct rt_value a = evaluate_expr(&s);
    struct rt_value b = evaluate_expr(&s);

    EXPECT((a.kind == b.kind), "expected a and b to have the same kind (got '%d' and '%d')\n", a.kind, b.kind);

    bool boolean;
    switch (a.kind)
    {
    case rt_number:
        boolean = a.num == b.num;
        break;
    case rt_boolean:
        boolean = a.boolean == b.boolean;
        break;
    default:
        UNREACHABLE("unknown runtime value of kind '%d'\n", a.kind);
    }

    EXPECT((*(expr) == ')'), "expected ')': %s\n", expr);
    expr++;

    return (struct rt_value){.kind = rt_boolean, .boolean = boolean};
}

#define STRNCMP(s1, s2) strncmp(s1, s2, sizeof(s2) - 1) == 0

struct rt_value evaluate_expr(struct scope *s)
{
    skip_whitespace();

    if (*expr == '\0')
    {
        UNREACHABLE("tried to evaluate an empty expression\n");
    }

    if (*expr == '(')
    {
        expr++;
        if (STRNCMP(expr, "add"))
        {
            expr += 3;
            return evaluate_op(s, op_add);
        }
        else if (STRNCMP(expr, "mult"))
        {
            expr += 4;
            return evaluate_op(s, op_mult);
        }
        else if (STRNCMP(expr, "div"))
        {
            expr += 3;
            return evaluate_op(s, op_div);
        }
        else if (STRNCMP(expr, "eq"))
        {
            expr += 2;
            return evaluate_eq(s);
        }
        else if (STRNCMP(expr, "let"))
        {
            expr += 3;
            return evaluate_let(s);
        }
        else if (STRNCMP(expr, "print"))
        {
            expr += 5;
            struct rt_value val = evaluate_expr(s);

            print_rt_value(val);
            fprintf(stderr, "\n");

            return (struct rt_value){.kind = rt_nil};
        }

        UNREACHABLE("expected the name of a callable\n");
    }

    if (*expr == '-' || is_num(*expr))
    {
        char *end;
        double num = (double)strtol(expr, &end, 10);
        expr += (end - expr);
        return (struct rt_value){.kind = rt_number, .num = num};
    }

    if (is_alpha(*expr))
    {
        char *lookahead = expr + 1;
        while (is_alphanumeric(*lookahead))
        {
            lookahead++;
        }

        // otherwise, this is a new identifier
        size_t len = lookahead - expr;
        char *name = malloc(sizeof(char) * (len + 1));
        strncpy(name, expr, len);
        name[len] = '\0';
        expr = lookahead;

        struct rt_value val;
        EXPECT(scope_get_value(s, name, &val), "unbound reference: %s\n", name);

        return val;
    }

    UNREACHABLE("unexpected char '%c'\n", *expr);
}

struct rt_value evaluate(char *source)
{
    struct scope s;
    scope_init(&s, NULL);
    expr = source;
    return evaluate_expr(&s);
}

int main()
{
    char *source = "(let a 1 b 1 (print (eq a b)))";
    struct rt_value res = evaluate(source);

    fprintf(stderr, "%s => ", source);
    print_rt_value(res);
    fprintf(stderr, "\n");

    return 0;
}
