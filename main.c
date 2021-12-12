#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define EXPECT(cond, ...)             \
    if (!cond)                        \
    {                                 \
        fprintf(stderr, __VA_ARGS__); \
        exit(1);                      \
    }

#define UNREACHABLE(...)                              \
    do                                                \
    {                                                 \
        fprintf(stderr, "unreachable: " __VA_ARGS__); \
        exit(1);                                      \
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

struct hm_entry
{
    bool in_use;
    char *key;
    double val;
};

struct hm
{
    size_t cap;
    size_t size;
    struct hm_entry *entries;
};

void hm_init(struct hm *hm)
{
    hm->cap = 2;
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
        hm->entries[i] = (const struct hm_entry) {0};
    }
}

void hm_add(struct hm *hm, char *key, double val)
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

bool hm_get(struct hm *hm, char *key, double *result)
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
    double dummy;
    return hm_get(hm, key, &dummy);
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

bool scope_get_value(struct scope *s, char *name, double *val)
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

double evaluate_expr(struct scope *s);

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
        double v = evaluate_expr(s);
        switch (op)
        {
        case op_mult:
            res = res * v;
            break;
        case op_add:
            res = res + v;
            break;
        default:
            UNREACHABLE("tried to apply unknown n-ary op '%d'\n", op);
        }
    }

    EXPECT((*(expr) == ')'), "expected ')': %s\n", expr);
    expr++;

    return res;
}

double is_at_end()
{
  return (*expr == '\0') ? 1 : 0;
}

double evaluate_binary_op(struct scope *s, enum op op)
{
  EXPECT((!is_at_end() && *expr != ')'), "not enough arguments for binary op\n");
  double a = evaluate_expr(s);

  EXPECT((!is_at_end() && *expr != ')'), "not enough arguments for binary op\n");
  double b = evaluate_expr(s);

  EXPECT((*expr == ')'), "too many arguments for binary op\n");

  switch (op)
  {
    case op_div:
      return a / b;
    default:
      UNREACHABLE("not a binary op: '%d'\n", op);
  }
}

double evaluate_op(struct scope *s, enum op op)
{
  switch (op)
  {
  case op_add:
  case op_mult:
    return evaluate_n_ary_op(s, op);
  case op_div:
    return evaluate_binary_op(s, op);
  default:
      UNREACHABLE("tried to apply unknown op '%d'\n", op);
  }
}

double evaluate_let(struct scope *parent)
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

        double val = evaluate_expr(&s);

        hm_add(&s.bindings, name, val);
    }

    double ret = evaluate_expr(&s);

    EXPECT((*(expr) == ')'), "expected ')': %s\n", expr);
    expr++;

    return ret;
}

#define STRNCMP(s1, s2) \
  strncmp(s1, s2, sizeof(s2) - 1)

double evaluate_expr(struct scope *s)
{
    skip_whitespace();

    if (*expr == '\0')
    {
      UNREACHABLE("tried to evaluate an empty expression\n");
    }

    if (*expr == '(')
    {
        expr++;
        if (STRNCMP(expr, "add") == 0)
        {
            expr += 3;
            return evaluate_op(s, op_add);
        }
        else if (STRNCMP(expr, "mult") == 0)
        {
            expr += 4;
            return evaluate_op(s, op_mult);
        }
        else if (STRNCMP(expr, "div") == 0)
        {
          expr += 3;
          return evaluate_op(s, op_div);
        }
        else if (STRNCMP(expr, "let") == 0)
        {
            expr += 3;
            return evaluate_let(s);
        }
    }

    if (*expr == '-' || is_num(*expr))
    {
        char *end;
        double value = (double)strtol(expr, &end, 10);
        expr += (end - expr);
        return value;
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

        double val;
        EXPECT(scope_get_value(s, name, &val), "unbound reference: %s\n", name);

        return val;
    }

    UNREACHABLE("unexpected char '%c'\n", *expr);
}

double evaluate(char *source)
{
    struct scope s;
    scope_init(&s, NULL);
    expr = source;
    return evaluate_expr(&s);
}

int main()
{
  char *source = "(div (add 1 1) (mult 1 1))";
  double res = evaluate(source);
  fprintf(stderr, "%s => %f\n", source, res);
  return 0;
}
