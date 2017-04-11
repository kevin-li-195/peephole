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
 *  Drop dead labels (where ref count is 0)
 *
 *  L:
 *  -----
 *  (nothing)
 */
int drop_dead_labels(CODE **c)
{
    int l;
    if (is_label(*c, &l) && deadlabel(l))
    {
        return replace(c, 1, NULL);
    }
    return 0;
}

/*
 *  Duplication, single storage, then
 *  pop should be reduced to simply
 *  consumption.
 *
 *  dup
 *  istore_k / astore_k
 *  pop
 *  ------
 *  istore_k / astore_k
 */
int remove_useless_dup_consume_pop(CODE **c)
{
    int l;
    if (is_dup(*c) &&
            is_pop(next(next(*c))))
    {
        if (is_istore(next(*c), &l))
        {
            return replace(c, 3,
                    makeCODEistore(l, NULL));
        }
        if (is_astore(next(*c), &l))
        {
            return replace(c, 3,
                    makeCODEastore(l, NULL));
        }
    }
    return 0;
}

/*
 *  Change branching rules for sequential
 *  labels.
 *
 *  uses_label L1
 *  ...
 *  L1:
 *  L2:
 *  -----
 *  uses_label L2
 *  ...
 *  L1: // reduce ref count
 *  L2: // inc ref count
 */
int change_branch_seq_labels(CODE **c)
{
    int l1, l2;
    if (uses_label(*c, &l1) && is_label(next(destination(l1)), &l2))
    {
        if (is_goto(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEgoto(l2, NULL));
        }
        if (is_ifeq(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEifeq(l2, NULL));
        }
        if (is_ifne(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEifne(l2, NULL));
        }
        if (is_if_acmpeq(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEif_acmpeq(l2, NULL));
        }
        if (is_if_acmpne(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEif_acmpne(l2, NULL));
        }
        if (is_ifnull(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEifnull(l2, NULL));
        }
        if (is_ifnonnull(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEifnonnull(l2, NULL));
        }
        if (is_if_icmpeq(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEif_icmpeq(l2, NULL));
        }
        if (is_if_icmpgt(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEif_icmpgt(l2, NULL));
        }
        if (is_if_icmplt(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEif_icmplt(l2, NULL));
        }
        if (is_if_icmple(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEif_icmple(l2, NULL));
        }
        if (is_if_icmpge(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEif_icmpge(l2, NULL));
        }
        if (is_if_icmpne(*c,&l1))
        {
            droplabel(l1);
            copylabel(l2);
            return replace(c, 1, makeCODEif_icmpne(l2, NULL));
        }
    }

    return 0;
}

/*
 *  aconst_null simplification
 *
 *  aconst_null
 *  if_acmpeq L / if_icmpeq L
 *  -----
 *  ifnull L
 *
 *
 *  aconst_null
 *  if_acmpne L / if_icmpne L
 *  -----
 *  ifnonnull L
 */
int aconst_null_cmp_simplify(CODE **c)
{
    int l;
    if (is_aconst_null(*c))
    {
        if (is_if_acmpeq(next(*c), &l) ||
                is_if_icmpeq(next(*c), &l))
        {
            return replace(c, 2, makeCODEifnull(l, NULL));
        }
        if (is_if_acmpne(next(*c), &l) ||
                is_if_icmpne(next(*c), &l))
        {
            return replace(c, 2, makeCODEifnonnull(l, NULL));
        }
    }
    return 0;
}

int aconst_null_reduce(CODE **c)
{
    int l;
    if (is_aconst_null(*c) &&
            is_dup(next(*c)) &&
            (is_if_icmpeq(next(next(*c)), &l) ||
             is_if_acmpeq(next(next(*c)), &l)
             )
       )
    {
        return replace(c, 3, makeCODEgoto(l, NULL));
    }
    if (is_aconst_null(*c) &&
            is_dup(next(*c)) &&
            (is_if_icmpne(next(next(*c)), &l) ||
             is_if_acmpne(next(next(*c)), &l)
             )
       )
    {
        droplabel(l);
        return replace(c, 3, NULL);
    }
    return 0;
}

/*  Check constant value
 *  if we use an 'if' after it.
 *
 *  iconst_k
 *  ifeq / iflt / ifgt / ifge / ifle / ifne L
 *  -----
 *  goto L / (nothing) // depending on k
 */
int const_if_eval(CODE **c)
{
    int k, l;
    if (is_ldc_int(*c, &k) &&
            is_ifeq(next(*c), &l)
       )
    {
        if (k == 0)
        {
            return replace(c, 2, makeCODEgoto(l, NULL));
        }
        else
        {
            droplabel(l);
            return replace(c, 2, NULL);
        }
    }
    if (is_ldc_int(*c, &k) &&
            is_ifne(next(*c), &l)
       )
    {
        if (k == 0)
        {
            droplabel(l);
            return replace(c, 2, NULL);
        }
        else
        {
            return replace(c, 2, makeCODEgoto(l, NULL));
        }
    }
    return 0;
}

/*
 *  Comparing int to zero is equivalent to
 *  just using comparison to zero.
 *
 *  iconst_0
 *  if_cmpeq L
 *  -----
 *  ifeq L
 *
 *  iconst_0
 *  if_cmpne L
 *  -----
 *  ifne L
 */
int zero_comparison_simplify(CODE **c)
{
    int k;
    int l;
    if (is_ldc_int(*c, &k) && k == 0)
    {
        if (is_if_icmpeq(next(*c), &l))
        {
            return replace(c, 2, makeCODEifeq(l, NULL));
        }
        if (is_if_icmpne(next(*c), &l))
        {
            return replace(c, 2, makeCODEifne(l, NULL));
        }
    }
    return 0;
}

/*
 *  Useless load/store noop.
 *
 *  aload_k / iload_k
 *  astore_k / istore_k
 *  -----
 *  nothing (for the same k only)
 */
int useless_load_store(CODE **c)
{
    int j, k;
    if (is_aload(*c, &j) &&
            is_astore(next(*c), &k))
    {
        if (j == k)
        {
            return replace(c, 2, NULL);
        }
    }
    if (is_iload(*c, &j) &&
            is_istore(next(*c), &k))
    {
        if (j == k)
        {
            return replace(c, 2, NULL);
        }
    }
    return 0;
}

/*
 *  Comparison after loading and duplication.
 *
 *  iload_k / aload_k
 *  dup
 *  if_icmpeq / if_acmpeq L
 *  -----
 *  goto L
 *
 *  iload_k / aload_k
 *  dup
 *  if_icmpne / if_acmpne L
 *  -----
 *  (nothing)
 */
int comp_load_dup_reduce(CODE **c)
{
    int k, l;
    if (is_iload(*c, &k) &&
            is_dup(next(*c)) &&
            (is_if_icmpeq(next(next(*c)), &l) ||
             is_if_icmpge(next(next(*c)), &l) ||
             is_if_icmple(next(next(*c)), &l)
            )
       )
    {
        return replace(c, 3, makeCODEgoto(l, NULL));
    }
    if (is_aload(*c, &k) &&
            is_dup(next(*c)) &&
            is_if_acmpeq(next(next(*c)), &l)
       )
    {
        return replace(c, 3, makeCODEgoto(l, NULL));
    }
    if (is_iload(*c, &k) &&
            is_dup(next(*c)) &&
            (is_if_icmpne(next(next(*c)), &l) ||
             is_if_icmpgt(next(next(*c)), &l) ||
             is_if_icmplt(next(next(*c)), &l)
            )
       )
    {
        droplabel(l);
        return replace(c, 3, NULL);
    }
    if (is_aload(*c, &k) &&
            is_dup(next(*c)) &&
            is_if_acmpne(next(next(*c)), &l)
       )
    {
        droplabel(l);
        return replace(c, 3, NULL);
    }
    return 0;
}

/*
 *  Unnecessary goto.
 *
 *  goto L
 *  L:
 *  -----
 *  L: // reduce ref count for L
 */
int remove_crazy_goto(CODE **c)
{
    int l1, l2;
    if (is_goto(*c, &l1) &&
            is_label(next(*c), &l2)
       )
    {
        if (l1 == l2)
        {
            droplabel(l1);
            return replace(c, 1, NULL);
        }
    }
    return 0;
}

/*
* iconst_0
* iload_k
* isub
* -------
* iload_k
* ineg
*/
int remove_sub_from_zero(CODE ** c) {
    int l1, l2;
    if(is_ldc_int(*c,&l1) &&
       is_iload(next(*c), &l2) &&
       is_isub(next(next(*c)))) {
        if (l1 == 0) { /* we can replace */
           return replace(c, 3, makeCODEiload(l2,makeCODEineg(NULL))); 
        } else { /*we must leave it as is*/
            return 0; 
        }

    }
    return 0; 
}

/*
* iload_k
* ldc 0
* isub
* -----------
* iload k
*/

int remove_sub_zero(CODE ** c) {
    int l1,l2; 
    if (
        is_iload(*c,&l1) &&
        is_ldc_int(next(*c),&l2) &&
        is_isub(next(next(*c)))
        )
        {
            if(l2 == 0) {
                return replace(c,3, makeCODEiload(l1,NULL)); 
            } else {
            /*cannot reduce*/
                return 0; 
            }
        }
    /*pattern does not match*/
    return 0; 
}

/*
* aconst_null
* checkcast
* ---------
* aconst_null
*/

int remove_null_checkcast(CODE ** c) {
    char * arg; 
    if(
        is_aconst_null(*c) &&
        is_checkcast(next(*c),&arg) 
        ) 
    {
        return replace(c, 2, makeCODEaconst_null(NULL)); 
    }
    /*pattern does not match*/
    return 0; 
}

/*
*
*ldc_1
*idiv
*----------
* 
*/

int remove_div_by_one(CODE ** c) {
   int l1; 
   if(
        is_ldc_int(*c, &l1) &&
        is_idiv(next(*c))
    ) {
        if(l1 == 1) {
            return replace_modified(c,2, NULL); 
        }
   }
   /*
    Pattern does not match
   */
   return 0; 
}


/*
*
*ldc_-1
*idiv
*----------
* ineg
*/

int remove_div_by_mone(CODE ** c) {
   int l1; 
   if(
        is_ldc_int(*c, &l1) &&
        is_idiv(next(*c))
    ) {
        if(l1 == -1) {
            return replace(c,2,makeCODEineg(NULL)); 
        }
   }
   /*
    Pattern does not match
   */
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
    ADD_PATTERN(drop_dead_labels);
    ADD_PATTERN(remove_useless_dup_consume_pop);
    ADD_PATTERN(change_branch_seq_labels);
    ADD_PATTERN(aconst_null_cmp_simplify);
    ADD_PATTERN(aconst_null_reduce);
    ADD_PATTERN(const_if_eval);
    ADD_PATTERN(zero_comparison_simplify);
    ADD_PATTERN(useless_load_store);
    ADD_PATTERN(comp_load_dup_reduce);
    ADD_PATTERN(remove_crazy_goto);
    ADD_PATTERN(remove_sub_from_zero); 
    ADD_PATTERN(remove_sub_zero); 
    ADD_PATTERN(remove_null_checkcast); 
    /*
     *  Make sure the following pattern is
     *  always last.
     */
    ADD_PATTERN(make_backtrack);
}
