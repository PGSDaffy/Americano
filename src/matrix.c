#include "matrix.h"

// allocation helpers

#define ALLOC(type, num)  ((type *) malloc(sizeof(type) * (num)))
#define NIL(type)         ((type) 0)

static sm_element *elem_alloc(void)
{
    sm_element *e = ALLOC(sm_element, 1);
    e->row_num = e->col_num = 0;
    e->next_row = e->prev_row = NULL;
    e->next_col = e->prev_col = NULL;
    return e;
}

static void elem_free(sm_element *e) { free(e); }

static sm_row *row_alloc(void)
{
    sm_row *r = ALLOC(sm_row, 1);
    r->row_num  = 0;
    r->length   = 0;
    r->first_col = r->last_col = NULL;
    r->next_row = r->prev_row = NULL;
    return r;
}

static void row_free(sm_row *r) { free(r); }

static sm_col *col_alloc(void)
{
    sm_col *c = ALLOC(sm_col, 1);
    c->col_num  = 0;
    c->length   = 0;
    c->first_row = c->last_row = NULL;
    c->next_col = c->prev_col = NULL;
    return c;
}

static void col_free(sm_col *c) { free(c); }


// doubly-linked-list insertion and unlink macros

#define sorted_insert(type, first, last, count,                          \
                      next, prev, value, newval, newobj)                 \
    do {                                                                 \
        if ((last) == NULL) {                                            \
            (first) = (last) = (newobj);                                 \
            (newobj)->next = (newobj)->prev = NULL;                      \
        } else if ((last)->value <= (newval)) {                          \
            (newobj)->next = NULL;                                       \
            (newobj)->prev = (last);                                     \
            (last)->next = (newobj);                                     \
            (last) = (newobj);                                           \
        } else if ((first)->value >= (newval)) {                         \
            (newobj)->next = (first);                                    \
            (newobj)->prev = NULL;                                       \
            (first)->prev = (newobj);                                    \
            (first) = (newobj);                                          \
        } else {                                                         \
            type *pp;                                                    \
            for (pp = (first); pp->value < (newval); pp = pp->next);    \
            (newobj)->next = pp;                                         \
            (newobj)->prev = pp->prev;                                   \
            pp->prev->next = (newobj);                                   \
            pp->prev = (newobj);                                         \
        }                                                                \
        (count)++;                                                       \
    } while (0)

#define dll_unlink(p, first, last, next, prev, count)                    \
    do {                                                                 \
        if ((p)->prev == NULL) {                                         \
            (first) = (p)->next;                                         \
        } else {                                                         \
            (p)->prev->next = (p)->next;                                 \
        }                                                                \
        if ((p)->next == NULL) {                                         \
            (last) = (p)->prev;                                          \
        } else {                                                         \
            (p)->next->prev = (p)->prev;                                 \
        }                                                                \
        (count)--;                                                       \
    } while (0)


// public API

sm_matrix *matrix_alloc(void)
{
    sm_matrix *A = ALLOC(sm_matrix, 1);
    A->rows      = NULL;
    A->cols      = NULL;
    A->nrows     = A->ncols = 0;
    A->rows_size = A->cols_size = 0;
    A->first_row = A->last_row = NULL;
    A->first_col = A->last_col = NULL;
    return A;
}


void matrix_free(sm_matrix *A)
{
    sm_row *prow, *pnext_row;
    sm_col *pcol, *pnext_col;

    /* free all elements row-by-row, then the row headers */
    for (prow = A->first_row; prow != NULL; prow = pnext_row) {
        sm_element *p, *pnext;
        pnext_row = prow->next_row;
        for (p = prow->first_col; p != NULL; p = pnext) {
            pnext = p->next_col;
            elem_free(p);
        }
        row_free(prow);
    }

    /* free column headers (elements already freed above) */
    for (pcol = A->first_col; pcol != NULL; pcol = pnext_col) {
        pnext_col = pcol->next_col;
        pcol->first_row = pcol->last_row = NULL;
        col_free(pcol);
    }

    free(A->rows);
    free(A->cols);
    free(A);
}


void matrix_resize(sm_matrix *A, int row, int col)
{
    if (row >= A->rows_size) {
        int new_size = (A->rows_size > 0) ? A->rows_size * 2 : 16;
        if (new_size <= row) new_size = row + 1;
        A->rows = (sm_row **) realloc(A->rows, new_size * sizeof(sm_row *));
        for (int i = A->rows_size; i < new_size; i++)
            A->rows[i] = NIL(sm_row *);
        A->rows_size = new_size;
    }

    if (col >= A->cols_size) {
        int new_size = (A->cols_size > 0) ? A->cols_size * 2 : 16;
        if (new_size <= col) new_size = col + 1;
        A->cols = (sm_col **) realloc(A->cols, new_size * sizeof(sm_col *));
        for (int i = A->cols_size; i < new_size; i++)
            A->cols[i] = NIL(sm_col *);
        A->cols_size = new_size;
    }
}


sm_element *matrix_insert(sm_matrix *A, int row, int col)
{
    sm_row *prow;
    sm_col *pcol;
    sm_element *element, *saved;

    matrix_resize(A, row, col);

    /* ensure row header exists */
    prow = A->rows[row];
    if (prow == NIL(sm_row *)) {
        prow = A->rows[row] = row_alloc();
        prow->row_num = row;
        sorted_insert(sm_row, A->first_row, A->last_row, A->nrows,
                      next_row, prev_row, row_num, row, prow);
    }

    /* ensure column header exists */
    pcol = A->cols[col];
    if (pcol == NIL(sm_col *)) {
        pcol = A->cols[col] = col_alloc();
        pcol->col_num = col;
        sorted_insert(sm_col, A->first_col, A->last_col, A->ncols,
                      next_col, prev_col, col_num, col, pcol);
    }

    /* allocate a new element; insert into row list */
    element = elem_alloc();
    element->row_num = row;
    element->col_num = col;
    saved   = element;

    sorted_insert(sm_element, prow->first_col, prow->last_col,
                  prow->length, next_col, prev_col, col_num, col, element);

    /* if the element was really inserted (not a duplicate), also
     * insert into the column list */
    if (element == saved) {
        sorted_insert(sm_element, pcol->first_row, pcol->last_row,
                      pcol->length, next_row, prev_row, row_num, row, element);
    } else {
        /* duplicate – free the element we just allocated */
        elem_free(saved);
    }
    return element;
}


sm_element *matrix_find(sm_matrix *A, int rownum, int colnum)
{
    sm_row *prow = matrix_get_row(A, rownum);
    if (prow == NULL) return NULL;

    sm_col *pcol = matrix_get_col(A, colnum);
    if (pcol == NULL) return NULL;

    /* search the shorter list */
    if (prow->length < pcol->length) {
        sm_element *p;
        sm_foreach_row_element(prow, p)
            if (p->col_num == colnum) return p;
    } else {
        sm_element *p;
        sm_foreach_col_element(pcol, p)
            if (p->row_num == rownum) return p;
    }
    return NULL;
}


// remove a specific element (must exist)
static void matrix_remove_element(sm_matrix *A, sm_element *p)
{
    sm_row *prow;
    sm_col *pcol;

    if (p == NULL) return;

    /* unlink from row */
    prow = matrix_get_row(A, p->row_num);
    dll_unlink(p, prow->first_col, prow->last_col,
               next_col, prev_col, prow->length);
    if (prow->first_col == NULL) {
        /* row is now empty – delete the row header */
        A->rows[p->row_num] = NIL(sm_row *);
        dll_unlink(prow, A->first_row, A->last_row,
                   next_row, prev_row, A->nrows);
        prow->first_col = prow->last_col = NULL;
        row_free(prow);
    }

    /* unlink from column */
    pcol = matrix_get_col(A, p->col_num);
    dll_unlink(p, pcol->first_row, pcol->last_row,
               next_row, prev_row, pcol->length);
    if (pcol->first_row == NULL) {
        A->cols[p->col_num] = NIL(sm_col *);
        dll_unlink(pcol, A->first_col, A->last_col,
                   next_col, prev_col, A->ncols);
        pcol->first_row = pcol->last_row = NULL;
        col_free(pcol);
    }

    elem_free(p);
}


void matrix_remove(sm_matrix *A, int row, int col)
{
    matrix_remove_element(A, matrix_find(A, row, col));
}
