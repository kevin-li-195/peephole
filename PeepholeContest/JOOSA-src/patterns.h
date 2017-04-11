/*
 * JOOS is Copyright (C) 1997 Laurie Hendren & Michael I. Schwartzbach
 *
 * Reproduction of all or part of this software is permitted for
 * educational or research use on condition that this copyright notice is
 * included in any copy. This software comes with no warranty of any
 * kind. In no event will the authors be liable for any damages resulting from
 * use of this software.
 *
 * email: hendren@cs.mcgill.ca, mis@brics.dk
 */

/* iload x        iload x        iload x
 * ldc 0          ldc 1          ldc 2
 * imul           imul           imul
 * ------>        ------>        ------>
 * ldc 0          iload x        iload x
 *                               dup
 *                               iadd
 */

#include <stdlib.h>

typedef struct BACKTRACK
{
    CODE **code;
    struct BACKTRACK *prev;
} BACKTRACK;

BACKTRACK *previousCode = NULL;

int simplify_multiplication_right(CODE **c)
{
    int x,k;
    if (is_iload(*c,&x) &&
        is_ldc_int(next(*c),&k) &&
        is_imul(next(next(*c))))
    {
        if (k==0) return replace(c,3,makeCODEldc_int(0,NULL));
        else if (k==1) return replace(c,3,makeCODEiload(x,NULL));
        else if (k==2) return replace(c,3,makeCODEiload(x,
                                         makeCODEdup(
                                         makeCODEiadd(NULL))));
        return 0;
    }
    return 0;
}

/* dup
 * astore x
 * pop
 * -------->
 * astore x
 */
int simplify_astore(CODE **c)
{
    int x;
    if (is_dup(*c) &&
        is_astore(next(*c),&x) &&
        is_pop(next(next(*c))))
    {
        return replace(c,3,makeCODEastore(x,NULL));
    }
  return 0;
}

int remove_dup_pop(CODE **c)
{
    if (is_dup(*c) &&
            is_pop(next(*c)))
    {
        return replace(c, 2, NULL);
    }
    return 0;
}

/* iload x
 * ldc k   (0<=k<=127)
 * iadd
 * istore x
 * --------->
 * iinc x k
 */
int positive_increment(CODE **c)
{
    int x,y,k;
    if (is_iload(*c,&x) &&
        is_ldc_int(next(*c),&k) &&
        is_iadd(next(next(*c))) &&
        is_istore(next(next(next(*c))),&y) &&
        x==y && 0<=k && k<=127)
    {
        return replace(c,4,makeCODEiinc(x,k,NULL));
    }
    return 0;
}

/* goto L1
 * ...
 * L1:
 * goto L2
 * ...
 * L2:
 * --------->
 * goto L2
 * ...
 * L1:    (reference count reduced by 1)
 * goto L2
 * ...
 * L2:    (reference count increased by 1)
 */
int simplify_goto_goto(CODE **c)
{
    int l1,l2;
    if (is_goto(*c,&l1) && is_goto(next(destination(l1)),&l2) && l1>l2)
    {
        droplabel(l1);
        copylabel(l2);
        return replace(c,1,makeCODEgoto(l2,NULL));
    }
    return 0;
}

int prevAddr = 0;

int make_backtrack(CODE **c)
{
    BACKTRACK *b = malloc(sizeof(BACKTRACK));
    b->prev = previousCode;
    b->code = c;
    previousCode = b;

    return 0;
}

/*
void printBacktrack()
{
    BACKTRACK *b = previousCode;
    while (b != NULL)
    {
        fprintf(stderr, "Previous addr: %d\n",
                (int) b->code);
        b = b->prev;
    }
    fprintf(stderr, "Done\n");
}
*/

int remove_nop(CODE **c)
{
    if (is_nop(*c))
    {
        kill_line(c);
        return 1;
    }
    return 0;
}

int test_invokevirtual(CODE **c)
{
    char *cs;
    if (is_invokevirtual(*c, &cs))
    {
        fprintf(stderr, "function name: '%s'\n",
                cs);
    }
    return 0;
}

/*
 *     invokevirtual (java/lang/String/concat (Ljava/lang/String;)Ljava/lang/String;)
 *     dup
 *     ifnull L
 *     ------
 *     invokevirtual (java/lang/String/concat (Ljava/lang/String;)Ljava/lang/String;)
 */
int remove_null_check_string_concat(CODE **c)
{
    int l;
    char *cs;
    if (is_invokevirtual(*c, &cs) &&
            is_dup(next(*c)) &&
            is_ifnull(next(next(*c)), &l))
    {
        /*
        Only when the string concatenation function is run. Null is never returned
        from the string concat, because any null string passed to string concat
        results in a runtime error.
        */
        if (strcmp(cs, "java/lang/String/concat(Ljava/lang/String;)Ljava/lang/String;") == 0)
        {
            droplabel(l);
            return replace(c, 3, makeCODEinvokevirtual(cs, NULL));
        }
    }
    return 0;
}

/*
 *  ldc x
 *  dup
 *  ifnull L // reduce ref count
 *  -----
 *  ldc x
 */
int remove_null_check_after_ldc(CODE **c)
{
    int i, l;
    char *s;
    if (is_ldc_int(*c, &i))
    {
        if (is_dup(next(*c)) &&
                is_ifnull(next(next(*c)), &l))
        {
            droplabel(l);
            return replace(c, 3, makeCODEldc_int(i, NULL));
        }
    }
    else if (is_ldc_string(*c, &s))
    {
        if (is_dup(next(*c)) &&
                is_ifnull(next(next(*c)), &l))
        {
            droplabel(l);
            return replace(c, 3, makeCODEldc_string(s, NULL));
        }
    }
    return 0;
}

/*
    iconst_0
    ifeq l
    ----------------
    goto l

    iconst_0
    ifge l
    -------------
    goto l

    iconst_0
    ifgt l
    -------
    (nothing)

    iconst_0
    ifne l
    -------------
    (Nothing)
*/

void init_patterns(void)
{
    ADD_PATTERN(simplify_multiplication_right);
    ADD_PATTERN(simplify_astore);
    ADD_PATTERN(positive_increment);
    ADD_PATTERN(simplify_goto_goto);
    ADD_PATTERN(remove_dup_pop);
    ADD_PATTERN(remove_nop);
    ADD_PATTERN(remove_null_check_string_concat);
    ADD_PATTERN(remove_null_check_after_ldc);

    /*
     *  Make sure the following pattern is
     *  always last.
     */
    ADD_PATTERN(make_backtrack);
}
